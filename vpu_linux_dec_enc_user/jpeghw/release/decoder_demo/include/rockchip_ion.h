/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef _LINUX_ROCKCHIP_ION_H
#define _LINUX_ROCKCHIP_ION_H
//#include <linux/ion.h>
#include "linux_ion.h"
enum ion_heap_ids {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 INVALID_HEAP_ID = -1,
 ION_CMA_HEAP_ID = 1,
 ION_IOMMU_HEAP_ID,
 ION_VMALLOC_HEAP_ID,
 ION_DRM_HEAP_ID,
 ION_CARVEOUT_HEAP_ID,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 ION_HEAP_ID_RESERVED = 31
};
#define ION_HEAP(bit) (1 << (bit))
#define ION_CMA_HEAP_NAME "cma"
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ION_IOMMU_HEAP_NAME "iommu"
#define ION_VMALLOC_HEAP_NAME "vmalloc"
#define ION_DRM_HEAP_NAME		"drm"
#define ION_CARVEOUT_HEAP_NAME	"carveout"
#define ION_SET_CACHED(__cache) (__cache | ION_FLAG_CACHED)
#define ION_SET_UNCACHED(__cache) (__cache & ~ION_FLAG_CACHED)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ION_IS_CACHED(__flags) ((__flags) & ION_FLAG_CACHED)
struct ion_flush_data {
 struct ion_handle *handle;
 int fd;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 void *vaddr;
 unsigned int offset;
 unsigned int length;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct ion_phys_data {
 struct ion_handle *handle;
 unsigned long phys;
 unsigned long size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};

struct ion_share_id_data {
	int fd;
	unsigned int id;
};

#define ION_IOC_ROCKCHIP_MAGIC 'R'
#define ION_IOC_CLEAN_CACHES _IOWR(ION_IOC_ROCKCHIP_MAGIC, 0,   struct ion_flush_data)
#define ION_IOC_INV_CACHES _IOWR(ION_IOC_ROCKCHIP_MAGIC, 1,   struct ion_flush_data)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ION_IOC_CLEAN_INV_CACHES _IOWR(ION_IOC_ROCKCHIP_MAGIC, 2,   struct ion_flush_data)
#define ION_IOC_GET_PHYS _IOWR(ION_IOC_ROCKCHIP_MAGIC, 3,   struct ion_phys_data)
#define ION_IOC_GET_SHARE_ID _IOWR(ION_IOC_ROCKCHIP_MAGIC, 4, struct ion_share_id_data)
#define ION_IOC_SHARE_BY_ID _IOWR(ION_IOC_ROCKCHIP_MAGIC, 5, struct ion_share_id_data)

#endif
