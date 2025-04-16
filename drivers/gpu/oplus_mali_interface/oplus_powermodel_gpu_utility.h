/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __OPLUS_POWERMODEL_GPU_UTILITY_H__
#define __OPLUS_POWERMODEL_GPU_UTILITY_H__

#include <linux/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* returning false indicated no implement */

void *oplus_get_gpu_inode(void);
bool oplus_mali_notify_gpu_inode(void * inode_ptr);
void *oplus_get_gpu_kbasefile(void);
bool oplus_mali_notify_gpu_kbasefile_create(void * kfile_ptr);
void *oplus_get_gpu_kbasectx(void);
bool oplus_mali_notify_gpu_kbasectx_create(void * kctx_ptr);

int oplus_kbase_open(void * inode,void* flip);
int oplus_kbase_release(void * inode,void* flip);
int oplus_kbase_api_handshake(void *kfile,void *version);
int oplus_kbase_api_set_flags(void *kfile,void *flags);
int oplus_kbase_api_get_gpuprops(void *kfile,void *get_props);
int oplus_kbase_api_ioctl_cs_get_glb_iface(void *kctx,void *param);
int oplus_kbase_api_kinstr_prfcnt_enum_info(void *kfile,void *prfcnt_enum_info);
int oplus_kbase_api_kinstr_prfcnt_setup(void *kfile,void *prfcnt_setup);
int oplus_kbase_api_kinstr_prfcnt_cmd(void *cli,void *control_cmd);
int oplus_kbase_api_kinstr_prfcnt_get_sample(void *cli,void *sample_access);
int oplus_kbase_api_kinstr_prfcnt_put_sample(void *cli,void *sample_access);
bool oplus_mali_notify_prfcnt_client_dump(u32 write_index);


#ifdef __cplusplus
}
#endif

#endif  /*__OPLUS_POWERMODEL_GPU_UTILITY_H__*/
