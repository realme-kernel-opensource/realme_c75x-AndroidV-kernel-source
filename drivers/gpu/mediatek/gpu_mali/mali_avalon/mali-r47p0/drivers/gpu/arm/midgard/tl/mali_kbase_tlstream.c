// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2015-2023 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */

#include "mali_kbase_tlstream.h"
#include "mali_kbase_tl_serialize.h"
#include "mali_kbase_mipe_proto.h"

/**
 * kbasep_packet_header_setup - setup the packet header
 * @buffer:     pointer to the buffer
 * @pkt_family: packet's family
 * @pkt_type:   packet's type
 * @pkt_class:  packet's class
 * @stream_id:  stream id
 * @numbered:   non-zero if this stream is numbered
 *
 * Function sets up immutable part of packet header in the given buffer.
 */
static void kbasep_packet_header_setup(char *buffer, enum tl_packet_family pkt_family,
				       enum tl_packet_class pkt_class, enum tl_packet_type pkt_type,
				       unsigned int stream_id, int numbered)
{
	u32 words[2] = {
		MIPE_PACKET_HEADER_W0(pkt_family, pkt_class, pkt_type, stream_id),
		MIPE_PACKET_HEADER_W1(0, !!numbered),
	};
	memcpy(buffer, words, sizeof(words));
}

/**
 * kbasep_packet_header_update - update the packet header
 * @buffer:    pointer to the buffer
 * @data_size: amount of data carried in this packet
 * @numbered:   non-zero if the stream is numbered
 *
 * Function updates mutable part of packet header in the given buffer.
 * Note that value of data_size must not include size of the header.
 */
static void kbasep_packet_header_update(char *buffer, size_t data_size, int numbered)
{
	u32 word1 = MIPE_PACKET_HEADER_W1((u32)data_size, !!numbered);

	KBASE_DEBUG_ASSERT(buffer);

	/* we copy the contents of word1 to its respective position in the buffer */
	memcpy(&buffer[sizeof(u32)], &word1, sizeof(word1));
}

/**
 * kbasep_packet_number_update - update the packet number
 * @buffer:  pointer to the buffer
 * @counter: value of packet counter for this packet's stream
 *
 * Function updates packet number embedded within the packet placed in the
 * given buffer.
 */
static void kbasep_packet_number_update(char *buffer, u32 counter)
{
	KBASE_DEBUG_ASSERT(buffer);

	memcpy(&buffer[PACKET_HEADER_SIZE], &counter, sizeof(counter));
}

void kbase_tlstream_reset(struct kbase_tlstream *stream)
{
	unsigned int i;

	for (i = 0; i < PACKET_COUNT; i++) {
		if (stream->numbered)
			atomic_set(&stream->buffer[i].size,
				   PACKET_HEADER_SIZE + PACKET_NUMBER_SIZE);
		else
			atomic_set(&stream->buffer[i].size, PACKET_HEADER_SIZE);
	}

	atomic_set(&stream->wbi, 0);
	atomic_set(&stream->rbi, 0);
}

/* Configuration of timeline streams generated by kernel. */
static const struct {
	enum tl_packet_family pkt_family;
	enum tl_packet_class pkt_class;
	enum tl_packet_type pkt_type;
	enum tl_stream_id stream_id;
} tl_stream_cfg[TL_STREAM_TYPE_COUNT] = {
	{
		TL_PACKET_FAMILY_TL,
		TL_PACKET_CLASS_OBJ,
		TL_PACKET_TYPE_SUMMARY,
		TL_STREAM_ID_KERNEL,
	},
	{
		TL_PACKET_FAMILY_TL,
		TL_PACKET_CLASS_OBJ,
		TL_PACKET_TYPE_BODY,
		TL_STREAM_ID_KERNEL,
	},
	{
		TL_PACKET_FAMILY_TL,
		TL_PACKET_CLASS_AUX,
		TL_PACKET_TYPE_BODY,
		TL_STREAM_ID_KERNEL,
	},
	{
		TL_PACKET_FAMILY_TL,
		TL_PACKET_CLASS_OBJ,
		TL_PACKET_TYPE_BODY,
		TL_STREAM_ID_CSFFW,
	},
};

void kbase_tlstream_init(struct kbase_tlstream *stream, enum tl_stream_type stream_type,
			 wait_queue_head_t *ready_read)
{
	unsigned int i;

	KBASE_DEBUG_ASSERT(stream);
	KBASE_DEBUG_ASSERT(stream_type < TL_STREAM_TYPE_COUNT);

	spin_lock_init(&stream->lock);

	/* All packets carrying tracepoints shall be numbered. */
	if (tl_stream_cfg[stream_type].pkt_type == TL_PACKET_TYPE_BODY)
		stream->numbered = 1;
	else
		stream->numbered = 0;

	for (i = 0; i < PACKET_COUNT; i++)
		kbasep_packet_header_setup(stream->buffer[i].data,
					   tl_stream_cfg[stream_type].pkt_family,
					   tl_stream_cfg[stream_type].pkt_class,
					   tl_stream_cfg[stream_type].pkt_type,
					   tl_stream_cfg[stream_type].stream_id, stream->numbered);

#if MALI_UNIT_TEST
	atomic_set(&stream->bytes_generated, 0);
#endif
	stream->ready_read = ready_read;

	kbase_tlstream_reset(stream);
}

void kbase_tlstream_term(struct kbase_tlstream *stream)
{
	KBASE_DEBUG_ASSERT(stream);
}

/**
 * kbasep_tlstream_msgbuf_submit - submit packet to user space
 * @stream:     Pointer to the stream structure
 * @wb_idx_raw: Write buffer index
 * @wb_size:    Length of data stored in the current buffer
 *
 * Updates currently written buffer with the packet header.
 * Then write index is incremented and the buffer is handed to user space.
 * Parameters of the new buffer are returned using provided arguments.
 *
 * Return: length of data in the new buffer
 *
 * Warning: the user must update the stream structure with returned value.
 */
static size_t kbasep_tlstream_msgbuf_submit(struct kbase_tlstream *stream, unsigned int wb_idx_raw,
					    unsigned int wb_size)
{
	unsigned int wb_idx = wb_idx_raw % PACKET_COUNT;

	/* Set stream as flushed. */
	atomic_set(&stream->autoflush_counter, -1);

	kbasep_packet_header_update(stream->buffer[wb_idx].data, wb_size - PACKET_HEADER_SIZE,
				    stream->numbered);

	if (stream->numbered)
		kbasep_packet_number_update(stream->buffer[wb_idx].data, wb_idx_raw);

	/* Increasing write buffer index will expose this packet to the reader.
	 * As stream->lock is not taken on reader side we must make sure memory
	 * is updated correctly before this will happen.
	 */
	smp_wmb();
	atomic_inc(&stream->wbi);

	/* Inform user that packets are ready for reading. */
	wake_up_interruptible(stream->ready_read);

	wb_size = PACKET_HEADER_SIZE;
	if (stream->numbered)
		wb_size += PACKET_NUMBER_SIZE;

	return wb_size;
}

char *kbase_tlstream_msgbuf_acquire(struct kbase_tlstream *stream, size_t msg_size,
				    unsigned long *flags) __acquires(&stream->lock)
{
	unsigned int wb_idx_raw;
	unsigned int wb_idx;
	size_t wb_size;

	KBASE_DEBUG_ASSERT(PACKET_SIZE - PACKET_HEADER_SIZE - PACKET_NUMBER_SIZE >= msg_size);

	spin_lock_irqsave(&stream->lock, *flags);

	wb_idx_raw = (unsigned int)atomic_read(&stream->wbi);
	wb_idx = wb_idx_raw % PACKET_COUNT;
	wb_size = (size_t)atomic_read(&stream->buffer[wb_idx].size);

	/* Select next buffer if data will not fit into current one. */
	if (wb_size + msg_size > PACKET_SIZE) {
		wb_size = kbasep_tlstream_msgbuf_submit(stream, wb_idx_raw, wb_size);
		wb_idx = (wb_idx_raw + 1) % PACKET_COUNT;
	}

	/* Reserve space in selected buffer. */
	atomic_set(&stream->buffer[wb_idx].size, wb_size + msg_size);

#if MALI_UNIT_TEST
	atomic_add(msg_size, &stream->bytes_generated);
#endif /* MALI_UNIT_TEST */

	return &stream->buffer[wb_idx].data[wb_size];
}

void kbase_tlstream_msgbuf_release(struct kbase_tlstream *stream, unsigned long flags)
	__releases(&stream->lock)
{
	/* Mark stream as containing unflushed data. */
	atomic_set(&stream->autoflush_counter, 0);

	spin_unlock_irqrestore(&stream->lock, flags);
}

size_t kbase_tlstream_flush_stream(struct kbase_tlstream *stream)
{
	unsigned long flags;
	unsigned int wb_idx_raw;
	unsigned int wb_idx;
	size_t wb_size;
	size_t min_size = PACKET_HEADER_SIZE;

	if (stream->numbered)
		min_size += PACKET_NUMBER_SIZE;

	spin_lock_irqsave(&stream->lock, flags);

	wb_idx_raw = (unsigned int)atomic_read(&stream->wbi);
	wb_idx = wb_idx_raw % PACKET_COUNT;
	wb_size = (size_t)atomic_read(&stream->buffer[wb_idx].size);

	if (wb_size > min_size) {
		wb_size = kbasep_tlstream_msgbuf_submit(stream, wb_idx_raw, wb_size);
		wb_idx = (wb_idx_raw + 1) % PACKET_COUNT;
		atomic_set(&stream->buffer[wb_idx].size, wb_size);
	} else {
		/* we return that there is no bytes to be read.*/
		/* Timeline io fsync will use this info the decide whether
		 * fsync should return an error
		 */
		wb_size = 0;
	}

	spin_unlock_irqrestore(&stream->lock, flags);
	return wb_size;
}
