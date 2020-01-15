/*************************************************************************/ /*!
@File
@Title          System Configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    System Configuration functions
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include "pvrsrv_device.h"
#include "syscommon.h"
#include "sysconfig.h"
#include "physheap.h"
#if defined(SUPPORT_ION)
#include "ion_support.h"
#endif

static RGX_TIMING_INFORMATION	gsRGXTimingInfo;
static RGX_DATA					gsRGXData;
static PVRSRV_DEVICE_CONFIG 	gsDevices[1];
static PVRSRV_SYSTEM_CONFIG 	gsSysConfig;

static PHYS_HEAP_FUNCTIONS	gsPhysHeapFuncs;
static PHYS_HEAP_CONFIG		gsPhysHeapConfig;

/*
	CPU to Device physcial address translation
*/
static
IMG_VOID UMAPhysHeapCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
									   IMG_DEV_PHYADDR *psDevPAddr,
									   IMG_CPU_PHYADDR *psCpuPAddr)
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);
	
	psDevPAddr->uiAddr = psCpuPAddr->uiAddr;
}

/*
	Device to CPU physcial address translation
*/
static
IMG_VOID UMAPhysHeapDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
									   IMG_CPU_PHYADDR *psCpuPAddr,
									   IMG_DEV_PHYADDR *psDevPAddr)				  
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);
	
	psCpuPAddr->uiAddr = psDevPAddr->uiAddr;
}

/* [LG] LGE */
#if defined(INCLUDE_LG_PATCH) && defined(CHIP_REV_B0)

#include <asm/io.h>                     /**< For ioremap_nocache */
#include <linux/delay.h>
#include <linux/kernel.h>

#define        CTOP_CTRL_BASE  0xC001B000
static void LGE_PATCH_InitHardware(void)
{
	volatile unsigned long addr;
	volatile unsigned long org_data;

	addr = (volatile unsigned long)ioremap(CTOP_CTRL_BASE, 0x10);

	org_data = __raw_readl(addr + 0x8);

	//printk("Step 1: Get GPU TOP Reg value (0x%x)\n", org_data);

	__raw_writel((org_data|0x40000000), addr + 0x8);

	//printk("Step 2: write LOW (0x%x)\n", (unsigned long)__raw_readl(addr + 0x8));

	udelay(100);

	__raw_writel(org_data , addr + 0x8);

	//printk("Step 3: write HIGH (0x%x)\n", (unsigned long)__raw_readl(addr + 0x8));

	udelay(100);

/*
       __raw_writel(0x40000000 , addr + 0x8);
       udelay(5);
       __raw_writel(0x00000000 , addr + 0x8);
*/       

       iounmap((void *)addr);
}

#endif

static PVRSRV_ERROR h13_system_pre_power_state(PVRSRV_SYS_POWER_STATE eNewPowerState)
{
#if defined(INCLUDE_LG_PATCH) && defined(CHIP_REV_B0)
	if (eNewPowerState == PVRSRV_SYS_POWER_STATE_ON)
	{
		//printk("__%s__%d__\n", __func__, __LINE__);
		LGE_PATCH_InitHardware();
	}
#endif
	return PVRSRV_OK;
}


/*
	SysCreateConfigData
*/
PVRSRV_ERROR SysCreateConfigData(PVRSRV_SYSTEM_CONFIG **ppsSysConfig)
{
	//printk("__RGX:Build Date/Time (%s/%s)\n" ,__DATE__, __TIME__);

	/*
	 * Setup information about physaical memory heap(s) we have
	 */
	gsPhysHeapFuncs.pfnCpuPAddrToDevPAddr = UMAPhysHeapCpuPAddrToDevPAddr;
	gsPhysHeapFuncs.pfnDevPAddrToCpuPAddr = UMAPhysHeapDevPAddrToCpuPAddr;

	gsPhysHeapConfig.ui32PhysHeapID = 0;
	gsPhysHeapConfig.pszPDumpMemspaceName = "SYSMEM";
	gsPhysHeapConfig.eType = PHYS_HEAP_TYPE_UMA;
	gsPhysHeapConfig.psMemFuncs = &gsPhysHeapFuncs;
	gsPhysHeapConfig.hPrivData = IMG_NULL;

	gsSysConfig.pasPhysHeaps = &gsPhysHeapConfig;
	gsSysConfig.ui32PhysHeapCount = sizeof(gsPhysHeapConfig) / sizeof(PHYS_HEAP_CONFIG);

	gsSysConfig.pui32BIFTilingHeapConfigs = gauiBIFTilingHeapXStrides;
	gsSysConfig.ui32BIFTilingHeapCount = IMG_ARR_NUM_ELEMS(gauiBIFTilingHeapXStrides);

/* [LG] LGE */
#if defined(INCLUDE_LG_PATCH) && defined(CHIP_REV_B0)
	       LGE_PATCH_InitHardware();
	#endif 	

	/*
	 * Setup RGX specific timing data
	 */
	gsRGXTimingInfo.ui32CoreClockSpeed        = RGX_NOHW_CORE_CLOCK_SPEED;
	gsRGXTimingInfo.bEnableActivePM           = IMG_FALSE;
	gsRGXTimingInfo.bEnableRDPowIsland        = IMG_FALSE;
	

	/*
	 *Setup RGX specific data
	 */
	gsRGXData.psRGXTimingInfo = &gsRGXTimingInfo;

	/*
	 * Setup RGX device
	 */
	gsDevices[0].eDeviceType            = PVRSRV_DEVICE_TYPE_RGX;
	gsDevices[0].pszName                = "RGX";

	/* Device setup information */
	gsDevices[0].sRegsCpuPBase.uiAddr   = 0xc0060000;
	gsDevices[0].ui32RegsSize           = 0x10000;
	gsDevices[0].ui32IRQ                = 85;
	gsDevices[0].bIRQIsShared           = IMG_FALSE;

	/* No power management on no HW system */
	gsDevices[0].pfnPrePowerState       = IMG_NULL;
	gsDevices[0].pfnPostPowerState      = IMG_NULL;

	/* No clock frequency either */
	gsDevices[0].pfnClockFreqGet        = IMG_NULL;

	/* No interrupt handled either */
	gsDevices[0].pfnInterruptHandled    = IMG_NULL;

	gsDevices[0].hDevData               = &gsRGXData;

	/*
	 * Setup system config
	 */
	gsSysConfig.pszSystemName = RGX_NOHW_SYSTEM_NAME;
	gsSysConfig.uiDeviceCount = sizeof(gsDevices)/sizeof(gsDevices[0]);
	gsSysConfig.pasDevices = &gsDevices[0];

	/* No power management on no HW system */
	gsSysConfig.pfnSysPrePowerState = h13_system_pre_power_state;
	gsSysConfig.pfnSysPostPowerState = IMG_NULL;

	/* no cache snooping */
	gsSysConfig.eCacheSnoopingMode = PVRSRV_SYSTEM_SNOOP_NONE;

	/* Setup other system specific stuff */
#if defined(SUPPORT_ION)
	IonInit(NULL);
#endif

	*ppsSysConfig = &gsSysConfig;
	return PVRSRV_OK;
}


/*
	SysDestroyConfigData
*/
IMG_VOID SysDestroyConfigData(PVRSRV_SYSTEM_CONFIG *psSysConfig)
{
	PVR_UNREFERENCED_PARAMETER(psSysConfig);

#if defined(SUPPORT_ION)
	IonDeinit();
#endif
}

PVRSRV_ERROR SysAcquireSystemData(IMG_HANDLE hSysData)
{
    PVR_UNREFERENCED_PARAMETER(hSysData);
    
    return PVRSRV_ERROR_NOT_SUPPORTED;
}

PVRSRV_ERROR SysReleaseSystemData(IMG_HANDLE hSysData)
{
    PVR_UNREFERENCED_PARAMETER(hSysData);
    
    return PVRSRV_ERROR_NOT_SUPPORTED;
}

PVRSRV_ERROR SysDebugInfo(PVRSRV_SYSTEM_CONFIG *psSysConfig, DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf)
{
    PVR_UNREFERENCED_PARAMETER(psSysConfig);
    PVR_UNREFERENCED_PARAMETER(pfnDumpDebugPrintf);
    return PVRSRV_OK;
}

// Do nothing
PVRSRV_ERROR RgxResume(IMG_VOID)
{
    return PVRSRV_OK;
}

// Do nothing
PVRSRV_ERROR RgxSuspend(IMG_VOID)
{
    return PVRSRV_OK;
}
/******************************************************************************
 End of file (sysconfig.c)
******************************************************************************/
