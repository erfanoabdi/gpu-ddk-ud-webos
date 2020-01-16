/*************************************************************************/ /*!
@File
@Title          System Description Header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    This header provides system-specific declarations and macros
@License        Strictly Confidential.
*/ /**************************************************************************/

#include "pvrsrv_device.h"
#include "rgxdevice.h"

#if !defined(__SYSCCONFIG_H__)
#define __SYSCCONFIG_H__


#define RGX_NOHW_CORE_CLOCK_SPEED 297000000
#define RGX_NOHW_SYSTEM_NAME "RGX H13"

/* default BIF tiling heap x-stride configurations. */
static IMG_UINT32 gauiBIFTilingHeapXStrides[RGXFWIF_NUM_BIF_TILING_CONFIGS] =
{
	0, /* BIF tiling heap 1 x-stride */
	1, /* BIF tiling heap 2 x-stride */
	2, /* BIF tiling heap 3 x-stride */
	3  /* BIF tiling heap 4 x-stride */
};

/*****************************************************************************
 * system specific data structures
 *****************************************************************************/
 
#endif	/* __SYSCCONFIG_H__ */
