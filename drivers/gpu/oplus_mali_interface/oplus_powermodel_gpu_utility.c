// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
	
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include "oplus_powermodel_gpu_utility.h"


void * g_kfile_ptr = NULL;
void * g_kctx_ptr = NULL;
void * g_inode_ptr = NULL;

/* ----------------------gpu prfcnt counter dump finish notify fp-------------------------- */
void (*oplus_mali_notify_prfcnt_client_dump_fp)(u32 write_index) = NULL;
EXPORT_SYMBOL(oplus_mali_notify_prfcnt_client_dump_fp);
bool oplus_mali_notify_prfcnt_client_dump(u32 write_index )
{
	if (oplus_mali_notify_prfcnt_client_dump_fp != NULL) {
		oplus_mali_notify_prfcnt_client_dump_fp(write_index);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(oplus_mali_notify_prfcnt_client_dump);


/* ----------------------Kbase ctx crate notify/get fp-------------------------- */
void (*oplus_notify_gpu_inode_fp)(void * inode_ptr) = NULL;
EXPORT_SYMBOL(oplus_notify_gpu_inode_fp);
bool oplus_mali_notify_gpu_inode(void * inode_ptr)
{
	g_inode_ptr = inode_ptr;
	if (oplus_notify_gpu_inode_fp != NULL) {
		oplus_notify_gpu_inode_fp(g_inode_ptr);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(oplus_mali_notify_gpu_inode);

void *oplus_get_gpu_inode(void)
{
	if (g_inode_ptr)
	{
		return g_inode_ptr;
	}	
	return NULL;
}
EXPORT_SYMBOL(oplus_get_gpu_inode);

/* ----------------------Kbase file crate notify/get fp-------------------------- */
void (*oplus_notify_gpu_kbasefile_create_fp)(void * kfile_ptr) = NULL;
EXPORT_SYMBOL(oplus_notify_gpu_kbasefile_create_fp);
bool oplus_mali_notify_gpu_kbasefile_create(void * kfile_ptr)
{
	g_kfile_ptr = kfile_ptr;
	if (oplus_notify_gpu_kbasefile_create_fp != NULL) {
		oplus_notify_gpu_kbasefile_create_fp(g_kfile_ptr);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(oplus_mali_notify_gpu_kbasefile_create);

void *oplus_get_gpu_kbasefile(void)
{	
	if (g_kfile_ptr)
	{
		return g_kfile_ptr;
	}	
	return NULL;
}
EXPORT_SYMBOL(oplus_get_gpu_kbasefile);


/* ----------------------Kbase ctx crate notify/get fp-------------------------- */
void (*oplus_notify_gpu_kbasectx_create_fp)(void * kctx_ptr) = NULL;
EXPORT_SYMBOL(oplus_notify_gpu_kbasectx_create_fp);
bool oplus_mali_notify_gpu_kbasectx_create(void * kctx_ptr)
{
	g_kctx_ptr = kctx_ptr;
	if (oplus_notify_gpu_kbasectx_create_fp != NULL) {
		oplus_notify_gpu_kbasectx_create_fp(g_kctx_ptr);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(oplus_mali_notify_gpu_kbasectx_create);

void *oplus_get_gpu_kbasectx(void)
{
	if (g_kctx_ptr)
	{
		return g_kctx_ptr;
	}	
	return NULL;
}
EXPORT_SYMBOL(oplus_get_gpu_kbasectx);


/* ----------------------Mali Kbase Open fp-------------------------- */
int (*oplus_kbase_open_fp)(void * inode,void* flip) = NULL;
EXPORT_SYMBOL(oplus_kbase_open_fp);
int oplus_kbase_open(void * inode,void* flip)
{
	if (oplus_kbase_open_fp != NULL) {
		return oplus_kbase_open_fp(inode,flip);
	}
	return -EINVAL;
}
 EXPORT_SYMBOL(oplus_kbase_open);


/* ----------------------Mali Kbase release fp-------------------------- */
int (*oplus_kbase_release_fp)(void * inode,void* flip) = NULL;
EXPORT_SYMBOL(oplus_kbase_release_fp);
int oplus_kbase_release(void * inode,void* flip)
{
	if (oplus_kbase_release_fp != NULL) {
		return oplus_kbase_release_fp(inode,flip);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(oplus_kbase_release);


/* ----------------------Mali Kbase check version fp-------------------------- */
int (*oplus_kbase_api_handshake_fp)(void * kfile,void* version) = NULL;
EXPORT_SYMBOL(oplus_kbase_api_handshake_fp);
int oplus_kbase_api_handshake(void *kfile,void *version)
{
	if (oplus_kbase_api_handshake_fp != NULL) {
		return oplus_kbase_api_handshake_fp(kfile,version);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(oplus_kbase_api_handshake);

/* ----------------------Mali Kbase set flag fp-------------------------- */
int (*oplus_kbase_api_set_flags_fp)(void *kfile,void *flags) = NULL;
EXPORT_SYMBOL(oplus_kbase_api_set_flags_fp);
int oplus_kbase_api_set_flags(void *kfile,void *version)
{
	if (oplus_kbase_api_set_flags_fp != NULL) {
		return oplus_kbase_api_set_flags_fp(kfile,version);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(oplus_kbase_api_set_flags);


/* ----------------------Mali Kbase set flag fp-------------------------- */
int (*oplus_kbase_api_get_gpuprops_fp)(void *kfile,void *get_props) = NULL;
EXPORT_SYMBOL(oplus_kbase_api_get_gpuprops_fp);
int oplus_kbase_api_get_gpuprops(void *kfile,void *get_props)
{
	if (oplus_kbase_api_get_gpuprops_fp != NULL) {
		return oplus_kbase_api_get_gpuprops_fp(kfile,get_props);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(oplus_kbase_api_get_gpuprops);

/* ----------------------Mali Kbase get global interface fp-------------------------- */
int (*oplus_kbase_api_ioctl_cs_get_glb_iface_fp)(void *kfile,void *param) = NULL;
EXPORT_SYMBOL(oplus_kbase_api_ioctl_cs_get_glb_iface_fp);
int oplus_kbase_api_ioctl_cs_get_glb_iface(void *kfile,void *param)
{
	if (oplus_kbase_api_ioctl_cs_get_glb_iface_fp != NULL) {
		return oplus_kbase_api_ioctl_cs_get_glb_iface_fp(kfile,param);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(oplus_kbase_api_ioctl_cs_get_glb_iface);


/* ----------------------Mali Kbase prfcnt enum info fp-------------------------- */
int (*oplus_kbase_api_kinstr_prfcnt_enum_info_fp)(void *kfile,void *prfcnt_enum_info) = NULL;
EXPORT_SYMBOL(oplus_kbase_api_kinstr_prfcnt_enum_info_fp);
int oplus_kbase_api_kinstr_prfcnt_enum_info(void *kfile,void *prfcnt_enum_info)
{
	if (oplus_kbase_api_kinstr_prfcnt_enum_info_fp != NULL) {
		return oplus_kbase_api_kinstr_prfcnt_enum_info_fp(kfile,prfcnt_enum_info);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(oplus_kbase_api_kinstr_prfcnt_enum_info);


/* ----------------------Mali Kbase prfcnt setup fp-------------------------- */
int (*oplus_kbase_api_kinstr_prfcnt_setup_fp)(void *kfile,void *prfcnt_setup) = NULL;
EXPORT_SYMBOL(oplus_kbase_api_kinstr_prfcnt_setup_fp);
int oplus_kbase_api_kinstr_prfcnt_setup(void *kfile,void *prfcnt_setup)
{
	if (oplus_kbase_api_kinstr_prfcnt_setup_fp != NULL) {
		return oplus_kbase_api_kinstr_prfcnt_setup_fp(kfile,prfcnt_setup);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(oplus_kbase_api_kinstr_prfcnt_setup);


/* ----------------------Mali Kbase prfcnt cmd fp-------------------------- */
int (*oplus_kbase_api_kinstr_prfcnt_cmd_fp)(void *cli, void *control_cmd) = NULL;
EXPORT_SYMBOL(oplus_kbase_api_kinstr_prfcnt_cmd_fp);
int oplus_kbase_api_kinstr_prfcnt_cmd(void *cli,void *control_cmd)
{
	if (oplus_kbase_api_kinstr_prfcnt_cmd_fp != NULL) {
		return oplus_kbase_api_kinstr_prfcnt_cmd_fp(cli,control_cmd);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(oplus_kbase_api_kinstr_prfcnt_cmd);


/* ----------------------Mali Kbase prfcnt get sample fp-------------------------- */
int (*oplus_kbase_api_kinstr_prfcnt_get_sample_fp)(void *cli, void *sample_access) = NULL;
EXPORT_SYMBOL(oplus_kbase_api_kinstr_prfcnt_get_sample_fp);
int oplus_kbase_api_kinstr_prfcnt_get_sample(void *cli,void *sample_access)
{
	if (oplus_kbase_api_kinstr_prfcnt_get_sample_fp != NULL) {
		return oplus_kbase_api_kinstr_prfcnt_get_sample_fp(cli,sample_access);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(oplus_kbase_api_kinstr_prfcnt_get_sample);


/* ----------------------Mali Kbase prfcnt put sample fp-------------------------- */
int (*oplus_kbase_api_kinstr_prfcnt_put_sample_fp)(void *cli, void *sample_access) = NULL;
EXPORT_SYMBOL(oplus_kbase_api_kinstr_prfcnt_put_sample_fp);
int oplus_kbase_api_kinstr_prfcnt_put_sample(void *cli,void *sample_access)
{
	if (oplus_kbase_api_kinstr_prfcnt_put_sample_fp != NULL) {
		return oplus_kbase_api_kinstr_prfcnt_put_sample_fp(cli,sample_access);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(oplus_kbase_api_kinstr_prfcnt_put_sample);


static int __init oplus_mali_pm_interface_init(void)
{
	/*Do Nothing*/
	return 0;
}

static void __exit oplus_mali_pm_interface_exit(void)
{
	/*Do Nothing*/
}

module_init(oplus_mali_pm_interface_init);
module_exit(oplus_mali_pm_interface_exit);

MODULE_AUTHOR("OPPO");
MODULE_VERSION("v1.0.0");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("OPLUS MALI GPU PM INTERFACE v1.0.0");
MODULE_ALIAS("Power module");

