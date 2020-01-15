/*************************************************************************/ /*!
@File
@Title          System Description Header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    This header provides system-specific declarations and macros
@License        Strictly Confidential.
*/ /**************************************************************************/

#if !defined(__SYSINFO_H__)
#define __SYSINFO_H__

/*!< System specific poll/timeout details */
#define MAX_HW_TIME_US				(500000)
#define DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT  (10000)
#define DEVICES_WATCHDOG_POWER_OFF_SLEEP_TIMEOUT (3600000)
#define WAIT_TRY_COUNT				(10000)

#define SYS_DEVICE_COUNT 3 /* RGX, DISPLAY (external), BUFFER (external) */

#define SYS_PHYS_HEAP_COUNT		1

#if defined(__linux__)
#define SYS_RGX_DEV_NAME    "rgx_h13"
#if defined(SUPPORT_DRM)
/*
 * Use the static bus ID for the platform DRM device.
 */
#if defined(PVR_DRM_DEV_BUS_ID)
#define	SYS_RGX_DEV_DRM_BUS_ID	PVR_DRM_DEV_BUS_ID
#else
#define SYS_RGX_DEV_DRM_BUS_ID	"platform:rgx_h13"
#endif	/* defined(PVR_DRM_DEV_BUS_ID) */
#endif	/* defined(SUPPORT_DRM) */
#endif

#endif	/* !defined(__SYSINFO_H__) */
