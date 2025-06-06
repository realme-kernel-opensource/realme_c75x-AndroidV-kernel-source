// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 * Copyright 2018 The Hafnium Authors.
 */

#include "mm.h"

#include "arch_mmu.h"
#include "arch_std.h"

#define UINT16_C(c) (c)
#define UINT32_C(c) (c##U)
#define UINT64_C(c) (c##ULL)
bool has_vhe_support(void)
{
	return false;
}
/* Keep macro alignment */
/* clang-format off */
#define NON_SHAREABLE   UINT64_C(0)
#define OUTER_SHAREABLE UINT64_C(2)
#define INNER_SHAREABLE UINT64_C(3)

#define PTE_VALID        (UINT64_C(1) << 0)
#define PTE_LEVEL0_BLOCK (UINT64_C(1) << 1)
#define PTE_TABLE        (UINT64_C(1) << 1)

#define STAGE1_XN          (UINT64_C(1) << 54)
#define STAGE1_UXN         (UINT64_C(1) << 54)
#define STAGE1_PXN         (UINT64_C(1) << 53)
#define STAGE1_CONTIGUOUS  (UINT64_C(1) << 52)
#define STAGE1_DBM         (UINT64_C(1) << 51)
#define STAGE1_GP          (UINT64_C(1) << 50)
#define STAGE1_NG          (UINT64_C(1) << 11)
#define STAGE1_AF          (UINT64_C(1) << 10)
#define STAGE1_SH(x)       ((x) << 8)
#define STAGE1_AP2         (UINT64_C(1) << 7)
#define STAGE1_AP1         (UINT64_C(1) << 6)
#define STAGE1_AP(x)       ((x) << 6)
#define STAGE1_NS          (UINT64_C(1) << 5)
#define STAGE1_ATTRINDX(x) ((x) << 2)

#define STAGE1_READONLY  UINT64_C(2)
#define STAGE1_READWRITE UINT64_C(0)
#define STAGE1_AP_USER_RW UINT64_C(1)

#define STAGE1_DEVICEINDX UINT64_C(0)
#define STAGE1_NORMALINDX UINT64_C(1)
#define STAGE1_STACKINDX UINT64_C(2)

#define STAGE2_XN(x)      ((x) << 53)
#define STAGE2_CONTIGUOUS (UINT64_C(1) << 52)
#define STAGE2_DBM        (UINT64_C(1) << 51)
#define STAGE2_AF         (UINT64_C(1) << 10)
#define STAGE2_SH(x)      ((x) << 8)
#define STAGE2_S2AP(x)    ((x) << 6)

#define STAGE2_EXECUTE_ALL  UINT64_C(0)
#define STAGE2_EXECUTE_EL0  UINT64_C(1)
#define STAGE2_EXECUTE_NONE UINT64_C(2)
#define STAGE2_EXECUTE_EL1  UINT64_C(3)
#define STAGE2_EXECUTE_MASK UINT64_C(3)

/* Table attributes only apply to stage 1 translations. */
#define TABLE_NSTABLE  (UINT64_C(1) << 63)
#define TABLE_APTABLE1 (UINT64_C(1) << 62)
#define TABLE_APTABLE0 (UINT64_C(1) << 61)
#define TABLE_XNTABLE  (UINT64_C(1) << 60)
#define TABLE_PXNTABLE (UINT64_C(1) << 59)

/* The following are stage-1 software defined attributes. */
#define STAGE1_SW_OWNED     (UINT64_C(1) << 55)
#define STAGE1_SW_EXCLUSIVE (UINT64_C(1) << 56)

/* The following are stage-2 software defined attributes. */
#define STAGE2_SW_OWNED     (UINT64_C(1) << 55)
#define STAGE2_SW_EXCLUSIVE (UINT64_C(1) << 56)

/* The following are stage-2 memory attributes for normal memory. */
#define STAGE2_DEVICE_MEMORY UINT64_C(0)
#define STAGE2_NONCACHEABLE  UINT64_C(1)
#define STAGE2_WRITETHROUGH  UINT64_C(2)
#define STAGE2_WRITEBACK     UINT64_C(3)

/* The following are stage-2 memory attributes for device memory. */
#define STAGE2_MEMATTR_DEVICE_nGnRnE UINT64_C(0)
#define STAGE2_MEMATTR_DEVICE_nGnRE  UINT64_C(1)
#define STAGE2_MEMATTR_DEVICE_nGRE   UINT64_C(2)
#define STAGE2_MEMATTR_DEVICE_GRE    UINT64_C(3)

/* The following construct and destruct stage-2 memory attributes. */
#define STAGE2_MEMATTR(outer, inner) ((((outer) << 2) | (inner)) << 2)
#define STAGE2_MEMATTR_TYPE_MASK (UINT64_C(3) << 4)

#define STAGE2_ACCESS_READ  UINT64_C(1)
#define STAGE2_ACCESS_WRITE UINT64_C(2)

#define CACHE_WORD_SIZE 4

/**
 * Threshold number of pages in TLB to invalidate after which we invalidate all
 * TLB entries on a given level.
 * Constant is the number of pointers per page table entry, also used by Linux.
 */
#define MAX_TLBI_OPS  MM_PTE_PER_PAGE

/* clang-format on */

#define tlbi(op)                                                               \
	do {                                                                   \
		__asm__ volatile("tlbi " #op);                                 \
	} while (0)
#define tlbi_reg(op, reg)                                                      \
	do {                                                                   \
		__asm__ __volatile__("tlbi " #op ", %0" : : "r"(reg));         \
	} while (0)

/** Mask for the address bits of the pte. */
#define PTE_ADDR_MASK                                                          \
	(((UINT64_C(1) << 48) - 1) & ~((UINT64_C(1) << PAGE_BITS) - 1))

/** Mask for the attribute bits of the pte. */
#define PTE_ATTR_MASK (~(PTE_ADDR_MASK | (UINT64_C(1) << 1)))

/**
 * Configuration information for memory management. Order is important as this
 * is read from assembly.
 *
 * It must only be written to from `arch_mm_init()` to avoid cache and
 * synchronization problems.
 */
struct arch_mm_config {
	uintreg_t ttbr0_el2;
	uintreg_t mair_el2;
	uintreg_t tcr_el2;
	uintreg_t sctlr_el2;
	uintreg_t hcr_el2;
	uintreg_t vtcr_el2;
	uintreg_t vstcr_el2;
} arch_mm_config;

static uint8_t mm_s1_max_level;

// static uint8_t mm_s2_max_level;
// static uint8_t mm_s2_root_table_count;

/* According to memory size and page table granule, smmu travse page table
 * from level 2.
 */
static uint8_t mm_s2_max_level = 2;
static uint8_t mm_s2_root_table_count = 1;

/**
 * Returns the encoding of a page table entry that isn't present.
 */
pte_t arch_mm_absent_pte(uint8_t level)
{
	(void)level;
	return 0;
}

/**
 * Converts a physical address to a table PTE.
 *
 * The spec says that 'Table descriptors for stage 2 translations do not
 * include any attribute field', so we don't take any attributes as arguments.
 */
pte_t arch_mm_table_pte(uint8_t level, paddr_t pa)
{
	/* This is the same for all levels on aarch64. */
	(void)level;
	return pa_addr(pa) | PTE_TABLE | PTE_VALID;
}

/**
 * Converts a physical address to a block PTE.
 *
 * The level must allow block entries.
 */
pte_t arch_mm_block_pte(uint8_t level, paddr_t pa, uint64_t attrs)
{
	pte_t pte = pa_addr(pa) | attrs;

	if (level == 0) {
		/* A level 0 'block' is actually a page entry. */
		pte |= PTE_LEVEL0_BLOCK;
	}
	return pte;
}

/**
 * Specifies whether block mappings are acceptable at the given level.
 *
 * Level 0 must allow block entries.
 */
bool arch_mm_is_block_allowed(uint8_t level)
{
	return level <= 2;
}

/**
 * Determines if the given pte is present, i.e., if it is valid or it is invalid
 * but still holds state about the memory so needs to be present in the table.
 */
bool arch_mm_pte_is_present(pte_t pte, uint8_t level)
{
	return arch_mm_pte_is_valid(pte, level) || (pte & STAGE2_SW_OWNED) != 0;
}

/**
 * Determines if the given pte is valid, i.e., if it points to another table,
 * to a page, or a block of pages that can be accessed.
 */
bool arch_mm_pte_is_valid(pte_t pte, uint8_t level)
{
	(void)level;
	return (pte & PTE_VALID) != 0;
}

/**
 * Determines if the given pte references a block of pages.
 */
bool arch_mm_pte_is_block(pte_t pte, uint8_t level)
{
	/* We count pages at level 0 as blocks. */
	return arch_mm_is_block_allowed(level) &&
	       (level == 0 ? (pte & PTE_LEVEL0_BLOCK) != 0 :
			     arch_mm_pte_is_present(pte, level) &&
				     !arch_mm_pte_is_table(pte, level));
}

/**
 * Determines if the given pte references another table.
 */
bool arch_mm_pte_is_table(pte_t pte, uint8_t level)
{
	return level != 0 && arch_mm_pte_is_valid(pte, level) &&
	       (pte & PTE_TABLE) != 0;
}

static uint64_t pte_addr(pte_t pte)
{
	return pte & PTE_ADDR_MASK;
}

/**
 * Clears the given physical address, i.e., clears the bits of the address that
 * are not used in the pte.
 */
paddr_t arch_mm_clear_pa(paddr_t pa)
{
	return pa_init(pte_addr(pa_addr(pa)));
}

/**
 * Extracts the physical address of the block referred to by the given page
 * table entry.
 */
paddr_t arch_mm_block_from_pte(pte_t pte, uint8_t level)
{
	(void)level;
	return pa_init(pte_addr(pte));
}

/**
 * Extracts the physical address of the page table referred to by the given page
 * table entry.
 */
paddr_t arch_mm_table_from_pte(pte_t pte, uint8_t level)
{
	(void)level;
	return pa_init(pte_addr(pte));
}

/**
 * Extracts the architecture-specific attributes applies to the given page table
 * entry.
 */
uint64_t arch_mm_pte_attrs(pte_t pte, uint8_t level)
{
	(void)level;
	return pte & PTE_ATTR_MASK;
}

/**
 * Execute any barriers or synchronization that is required
 * by a given architecture, after page table writes.
 */
void arch_mm_sync_table_writes(void)
{
	/*
	 * Ensure visibility of table updates to translation table walks.
	 */
	dsb(ish);
}

/**
 * Invalidates stage-2 TLB entries referring to the given intermediate physical
 * address range.
 */
void arch_mm_invalidate_stage2_range(uint16_t vmid, ipaddr_t va_begin,
				     ipaddr_t va_end)
{
	uintpaddr_t begin = ipa_addr(va_begin);
	uintpaddr_t end = ipa_addr(va_end);
	uintpaddr_t it;

	(void)vmid;

	/* TODO: This only applies to the current VMID. */

	/* Sync with page table updates. */
	arch_mm_sync_table_writes();

	/*
	 * Revisions prior to Armv8.4 do not support invalidating a range of
	 * addresses, which means we have to loop over individual pages. If
	 * there are too many, it is quicker to invalidate all TLB entries.
	 */
	if ((end - begin) > (MAX_TLBI_OPS * PAGE_SIZE)) {
		/*
		 * Invalidate all stage-1 and stage-2 entries of the TLB for
		 * the current VMID.
		 */
		tlbi(vmalls12e1is);
	} else {
		begin >>= 12;
		end >>= 12;

		/*
		 * Invalidate stage-2 TLB, one page from the range at a time.
		 * Note that this has no effect if the CPU has a TLB with
		 * combined stage-1/stage-2 translation.
		 */
		for (it = begin; it < end;
		     it += (UINT64_C(1) << (PAGE_BITS - 12))) {
			tlbi_reg(ipas2e1is, it);
		}

		/*
		 * Ensure completion of stage-2 invalidation in case a page
		 * table walk on another CPU refilled the TLB with a complete
		 * stage-1 + stage-2 walk based on the old stage-2 mapping.
		 */
		dsb(ish);

		/*
		 * Invalidate all stage-1 TLB entries. If the CPU has a combined
		 * TLB for stage-1 and stage-2, this will invalidate stage-2 as
		 * well.
		 */
		tlbi(vmalle1is);
	}

	/* Sync data accesses with TLB invalidation completion. */
	dsb(ish);

	/* Sync instruction fetches with TLB invalidation completion. */
	isb();
}

uint64_t arch_mm_mode_to_stage1_attrs(uint32_t mode)
{
	uint64_t attrs = 0;

	attrs |= STAGE1_AF | STAGE1_SH(INNER_SHAREABLE);

	/*
	 * STAGE1_XN can be XN or UXN depending on if the EL2
	 * translation regime uses one VA range or two VA ranges(VHE).
	 * PXN is res0 when the translation regime does not support two
	 * VA ranges.
	 */
	if (mode & MM_MODE_X) {
		if (has_vhe_support()) {
			attrs |=
				(mode & MM_MODE_USER) ? STAGE1_PXN : STAGE1_UXN;
		}

#if BRANCH_PROTECTION
		/* Mark code pages as Guarded Pages if BTI is supported. */
		if (is_arch_feat_bti_supported())
			attrs |= STAGE1_GP;
#endif
	} else {
		if (has_vhe_support())
			attrs |= (STAGE1_UXN | STAGE1_PXN);
		else
			attrs |= STAGE1_XN;
	}

	/* Define the read/write bits. */
	if (mode & MM_MODE_W)
		attrs |= STAGE1_AP(STAGE1_READWRITE);
	else
		attrs |= STAGE1_AP(STAGE1_READONLY);

	if (has_vhe_support()) {
		attrs |= (mode & MM_MODE_USER) ? STAGE1_AP(STAGE1_AP_USER_RW) :
						 0;
		if (mode & MM_MODE_NG)
			attrs |= STAGE1_NG;
	}

	/* Define the memory attribute bits. */
	if (mode & MM_MODE_D)
		attrs |= STAGE1_ATTRINDX(STAGE1_DEVICEINDX);
	else if (mode & MM_MODE_T)
		attrs |= STAGE1_ATTRINDX(STAGE1_STACKINDX);
	else
		attrs |= STAGE1_ATTRINDX(STAGE1_NORMALINDX);

	/* Define the ownership bit. */
	if (!(mode & MM_MODE_UNOWNED))
		attrs |= STAGE1_SW_OWNED;

	/* Define the exclusivity bit. */
	if (!(mode & MM_MODE_SHARED))
		attrs |= STAGE1_SW_EXCLUSIVE;

	/* Define the valid bit. */
	if (!(mode & MM_MODE_INVALID))
		attrs |= PTE_VALID;

	return attrs;
}

uint32_t arch_mm_stage1_attrs_to_mode(uint64_t attrs)
{
	uint32_t mode = 0;

	if ((attrs & STAGE1_AP(STAGE1_READONLY)) ==
	    STAGE1_AP(STAGE1_READONLY)) {
		mode |= MM_MODE_R;
	} else {
		mode |= MM_MODE_W | MM_MODE_R;
	}

	if (has_vhe_support() && (attrs & STAGE1_AP(STAGE1_AP_USER_RW)))
		mode |= MM_MODE_USER;

	if (!(attrs & STAGE1_XN) || !(attrs & STAGE1_PXN))
		mode |= MM_MODE_X;

	if (has_vhe_support() && (attrs & STAGE1_NG))
		mode |= MM_MODE_NG;

	if (!((attrs & STAGE1_ATTRINDX(STAGE1_NORMALINDX)) ==
	      STAGE1_ATTRINDX(STAGE1_NORMALINDX))) {
		mode |= MM_MODE_D;
	}

	if (!(attrs & STAGE1_SW_OWNED))
		mode |= MM_MODE_UNOWNED;

	if (!(attrs & STAGE1_SW_EXCLUSIVE))
		mode |= MM_MODE_SHARED;

	if (!(attrs & PTE_VALID))
		mode |= MM_MODE_INVALID;

	return mode;
}

uint64_t arch_mm_mode_to_stage2_attrs(uint32_t mode)
{
	uint64_t attrs = 0;
	uint64_t access = 0;

	/*
	 * Default shareability is inner shareable in stage 2 tables. Per
	 * table D5-45 of ARM ARM DDI0487G, Inner shareable attribute will
	 * pass through the stage 1 attribute of outer shareable and inner
	 * shareable, but NOT non-shareable. A stage 1 non-shareable attribute
	 * combined with stage 2 inner shareable, results in an inner shareable
	 * access. This is intentional, since a VCPU that marks a memory region
	 * as non-shareable in its stage 1 translation tables, can be migrated
	 * to a different PHYSICAL PE unless the VCPU is pinned to the PE.
	 * If stage 2 was marked as non-shareable below, the resulting accesses
	 * for a VCPU on a physical PE would be marked as non-shareable, and
	 * hence potentially not visible on another physical PE, which could
	 * cause coherency issues when the VCPU is migrated and expects its
	 * non-shareable accesses to be visible, but would read stale or invalid
	 * data. Note that for a access that results in device memory type, the
	 * shareability does not matter and is always treated as outer
	 * shareable.
	 */
	// attrs |= STAGE2_AF | STAGE2_SH(INNER_SHAREABLE);
	/*
	 * For MTK DMA, must use non_shareable by default
	 * Our intention is not to overwrite thte stage-1 shareability
	 * It's for SMMU usage, not relevant to VCPU
	 */
	attrs |= STAGE2_AF | STAGE2_SH(NON_SHAREABLE);

	/* Define the read/write bits. */
	if (mode & MM_MODE_R)
		access |= STAGE2_ACCESS_READ;

	if (mode & MM_MODE_W)
		access |= STAGE2_ACCESS_WRITE;

	attrs |= STAGE2_S2AP(access);

	/* Define the execute bits. */
	if (mode & MM_MODE_X)
		attrs |= STAGE2_XN(STAGE2_EXECUTE_ALL);
	else
		attrs |= STAGE2_XN(STAGE2_EXECUTE_NONE);

	/*
	 * Define the memory attribute bits, using the "neutral" values which
	 * give the stage-1 attributes full control of the attributes.
	 */
	if (mode & MM_MODE_D) {
		attrs |= STAGE2_MEMATTR(STAGE2_DEVICE_MEMORY,
					STAGE2_MEMATTR_DEVICE_GRE);
	} else {
		attrs |= STAGE2_MEMATTR(STAGE2_WRITEBACK, STAGE2_WRITEBACK);
	}

	/* Define the ownership bit. */
	if (!(mode & MM_MODE_UNOWNED))
		attrs |= STAGE2_SW_OWNED;

	/* Define the exclusivity bit. */
	if (!(mode & MM_MODE_SHARED))
		attrs |= STAGE2_SW_EXCLUSIVE;

	/* Define the valid bit. */
	if (!(mode & MM_MODE_INVALID))
		attrs |= PTE_VALID;

	return attrs;
}

uint32_t arch_mm_stage2_attrs_to_mode(uint64_t attrs)
{
	uint32_t mode = 0;

	if (attrs & STAGE2_S2AP(STAGE2_ACCESS_READ))
		mode |= MM_MODE_R;

	if (attrs & STAGE2_S2AP(STAGE2_ACCESS_WRITE))
		mode |= MM_MODE_W;

	if ((attrs & STAGE2_XN(STAGE2_EXECUTE_MASK)) ==
	    STAGE2_XN(STAGE2_EXECUTE_ALL)) {
		mode |= MM_MODE_X;
	}

	if ((attrs & STAGE2_MEMATTR_TYPE_MASK) == STAGE2_DEVICE_MEMORY)
		mode |= MM_MODE_D;

	// if (!(attrs & STAGE2_SW_OWNED))
	//mode |= MM_MODE_UNOWNED;

	// if (!(attrs & STAGE2_SW_EXCLUSIVE))
	//mode |= MM_MODE_SHARED;

	/* It's for MTK SMMU HW, so not to set any SWBITS[58:55]*/

	if (!(attrs & PTE_VALID))
		mode |= MM_MODE_INVALID;

	return mode;
}

void arch_mm_stage1_max_level_set(uint32_t pa_bits)
{
	/* Maximum supported PA range in bits is 48 */

	if (pa_bits >= 40) {
		mm_s1_max_level = 3;
	} else {
		/* Setting to 2 covers physical memory up to 512GB */
		mm_s1_max_level = 2;
	}
}

uint8_t arch_mm_stage1_max_level(void)
{
	return mm_s1_max_level;
}

uint8_t arch_mm_stage2_max_level(void)
{
	return mm_s2_max_level;
}

uint8_t arch_mm_stage1_root_table_count(void)
{
	/* Stage 1 doesn't concatenate tables. */
	return 1;
}

uint8_t arch_mm_stage2_root_table_count(void)
{
	return mm_s2_root_table_count;
}

/**
 * Given the attrs from a table at some level and the attrs from all the blocks
 * in that table, returns equivalent attrs to use for a block which will replace
 * the entire table.
 */
uint64_t arch_mm_combine_table_entry_attrs(uint64_t table_attrs,
					   uint64_t block_attrs)
{
	/*
	 * Only stage 1 table descriptors have attributes, but the bits are res0
	 * for stage 2 table descriptors so this code is safe for both.
	 */
	if (table_attrs & TABLE_NSTABLE)
		block_attrs |= STAGE1_NS;
	if (table_attrs & TABLE_APTABLE1)
		block_attrs |= STAGE1_AP2;
	if (table_attrs & TABLE_APTABLE0) {
		/* When two VA ranges are supported, AP1 is valid */
		if (has_vhe_support())
			block_attrs |= STAGE1_AP1;
		else
			block_attrs &= ~STAGE1_AP1;
	}
	if (table_attrs & TABLE_XNTABLE)
		block_attrs |= STAGE1_XN;

	if (table_attrs & TABLE_PXNTABLE)
		block_attrs |= STAGE1_PXN;
	return block_attrs;
}

/**
 * Returns the maximum supported PA Range in bits.
 */
uint32_t arch_mm_get_pa_bits(uint64_t pa_range)
{
	static const uint32_t pa_bits_table[16] = { 32, 36, 40, 42, 44, 48, 52 };

	assert(pa_range < ARRAY_SIZE(pa_bits_table));

	return pa_bits_table[pa_range];
}

uintptr_t arch_mm_get_vtcr_el2(void)
{
	return arch_mm_config.vtcr_el2;
}

uintptr_t arch_mm_get_vstcr_el2(void)
{
	return arch_mm_config.vstcr_el2;
}
