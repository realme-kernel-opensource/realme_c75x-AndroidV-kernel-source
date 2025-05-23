/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_INTERCONNECT_PROVIDER_H
#define __MTK_INTERCONNECT_PROVIDER_H

//#include <linux/interconnect.h>
#include "mtk-interconnect.h"

#define icc_units_to_bps(bw)  ((bw) * 1000ULL)

struct icc_node;
struct of_phandle_args;

/**
 * struct icc_onecell_data - driver data for onecell interconnect providers
 *
 * @num_nodes: number of nodes in this device
 * @nodes: array of pointers to the nodes in this device
 */
struct icc_onecell_data {
	unsigned int num_nodes;
	struct icc_node *nodes[];
};

struct icc_node *of_mtk_icc_xlate_onecell(struct of_phandle_args *spec,
				      void *data);

/**
 * struct icc_provider - interconnect provider (controller) entity that might
 * provide multiple interconnect controls
 *
 * @provider_list: list of the registered interconnect providers
 * @nodes: internal list of the interconnect provider nodes
 * @set: pointer to device specific set operation function
 * @aggregate: pointer to device specific aggregate operation function
 * @pre_aggregate: pointer to device specific function that is called
 *		   before the aggregation begins (optional)
 * @xlate: provider-specific callback for mapping nodes from phandle arguments
 * @dev: the device this interconnect provider belongs to
 * @users: count of active users
 * @data: pointer to private data
 */
struct icc_provider {
	struct list_head	provider_list;
	struct list_head	nodes;
	int (*set)(struct icc_node *src, struct icc_node *dst);
	int (*aggregate)(struct icc_node *node, u32 tag, u32 avg_bw,
			 u32 peak_bw, u32 *agg_avg, u32 *agg_peak);
	bool (*path_is_write)(struct icc_node *node);
	int (*comm_chn_info)(struct icc_node *src, struct icc_node *dst);
	void (*pre_aggregate)(struct icc_node *node);
	struct icc_node* (*xlate)(struct of_phandle_args *spec, void *data);
	struct device		*dev;
	int			users;
	void			*data;
};

/**
 * struct icc_node - entity that is part of the interconnect topology
 *
 * @id: platform specific node id
 * @name: node name used in debugfs
 * @links: a list of targets pointing to where we can go next when traversing
 * @num_links: number of links to other interconnect nodes
 * @provider: points to the interconnect provider of this node
 * @node_list: the list entry in the parent provider's "nodes" list
 * @search_list: list used when walking the nodes graph
 * @reverse: pointer to previous node when walking the nodes graph
 * @is_traversed: flag that is used when walking the nodes graph
 * @req_list: a list of QoS constraint requests associated with this node
 * @avg_bw: aggregated value of average bandwidth requests from all consumers
 * @peak_bw: aggregated value of peak bandwidth requests from all consumers
 * @data: pointer to private data
 */
struct icc_node {
	int			id;
	const char              *name;
	struct icc_node		**links;
	size_t			num_links;

	struct icc_provider	*provider;
	struct list_head	node_list;
	struct list_head	search_list;
	struct icc_node		*reverse;
	u8			is_traversed:1;
	struct icc_node		**reverse_links;
	size_t			num_reverse_links;
	struct hlist_head	req_list;
	struct hlist_head	direct_req_list;
	u32			avg_bw;
	u32			peak_bw;
	u32			v2_mix_bw;
	u32			v2_avg_bw;
	u32			v2_peak_bw;
	u32			v2_avg_r_bw;
	u32			v2_peak_r_bw;
	u32			v2_avg_w_bw;
	u32			v2_peak_w_bw;
	bool			v2_max_ostd;
	void			*data;
};

#if IS_ENABLED(CONFIG_INTERCONNECT_MTK_EXTENSION)

struct icc_node *mtk_icc_node_create(int id);
void mtk_icc_node_destroy(int id);
int mtk_icc_link_create(struct icc_node *node, const int dst_id);
int mtk_icc_link_destroy(struct icc_node *src, struct icc_node *dst);
void mtk_icc_node_add(struct icc_node *node, struct icc_provider *provider);
void mtk_icc_node_del(struct icc_node *node);
int mtk_icc_provider_add(struct icc_provider *provider);
int mtk_icc_provider_del(struct icc_provider *provider);

#else

static inline struct icc_node *mtk_icc_node_create(int id)
{
	return ERR_PTR(-ENOTSUPP);
}

static inline void mtk_icc_node_destroy(int id)
{
}

static inline int mtk_icc_link_create(struct icc_node *node, const int dst_id)
{
	return -ENOTSUPP;
}

static inline int mtk_icc_link_destroy(struct icc_node *src, struct icc_node *dst)
{
	return -ENOTSUPP;
}

static inline void mtk_icc_node_add(struct icc_node *node, struct icc_provider *provider)
{
}

static inline void mtk_icc_node_del(struct icc_node *node)
{
}

static inline int mtk_icc_provider_add(struct icc_provider *provider)
{
	return -ENOTSUPP;
}

static inline int mtk_icc_provider_del(struct icc_provider *provider)
{
	return -ENOTSUPP;
}

#endif /* CONFIG_INTERCONNECT_MTK_EXTENSION */

#endif /* __MTK_INTERCONNECT_PROVIDER_H */
