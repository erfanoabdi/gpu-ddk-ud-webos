/*************************************************************************/ /*!
@Copyright      Copyright (c) LG Electronics All Rights Reserved
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


/* [LG] LGE target : WEBOS/NC5/GM */


#include <linux/version.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/fb.h>

#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

#include "kerneldisplay.h"
#include "imgpixfmts_km.h"
#include "pvrmodule.h" /* for MODULE_LICENSE() */

#define DRVNAME					"dc_gm"
#define DC_PHYS_HEAP_ID			0
#define MAX_COMMANDS_IN_FLIGHT	2
#define NUM_PREFERRED_BUFFERS	2

#define FALLBACK_REFRESH_RATE		60
#define FALLBACK_PHYSICAL_WIDTH		1920
#define FALLBACK_PHYSICAL_HEIGHT	1080

#define KM_CHECK_UPDATE_CUSTOM_ID		0xdcfb0000
#define KM_CHECK_RETIRE_CUSTOM_ID		0x00fbdc00


#define DBG_D4_EXT		(0x1<<1)
#define DBG_D4_INFO		(0x1<<2)
#define DBG_D4_DEBUG	(0x1<<3)
#define DBG_D4_ERROR	(0x1<<4)
//#define DBG_D4_LV 		(DBG_D4_DEBUG | DBG_D4_INFO | DBG_D4_EXT | DBG_D4_ERROR)
#define DBG_D4_LV 		(DBG_D4_INFO | DBG_D4_ERROR)

#define DBG_D4_PRINT(lv, fmt, args...) do { if(lv & DBG_D4_LV) printk("[0x%x]"fmt, lv, ##args); } while(0)

#define DC_WORK_COUNT 16

DEFINE_SPINLOCK(dc_retired_ext_lock);


typedef struct
{
	IMG_HANDLE			hSrvHandle;
	IMG_UINT32			ePixFormat;
	bool				bCanFlip;
}
DC_GM_DEVICE;

typedef struct
{
	DC_GM_DEVICE		*psDeviceData;
	IMG_HANDLE			hLastConfigData;
	IMG_UINT32			ui32AllocCount;
	IMG_INT32			window_idx;
	PVRSRV_SURFACE_GM_WIN_INFO saWinInfo[2];
	int buffer_idx;
	IMG_HANDLE			hReadyConfigData;
	IMG_UINT32			ui32UpdateCount;
}
DC_GM_CONTEXT;

typedef struct
{
	DC_GM_CONTEXT	*psDeviceContext;
	IMG_UINT32			ui32Width;
	IMG_UINT32			ui32Height;
	IMG_UINT32			ui32ByteStride;
	IMG_UINT32			ui32BufferID;
}
DC_GM_BUFFER;

typedef struct _work_retired_s
{
	IMG_HANDLE hconfig;
	struct work_struct swork;
} work_retired_t;

typedef struct _workqueue_retired_s
{
	int current_work_index;
	struct workqueue_struct * p_retire_wq;
	work_retired_t work_array[DC_WORK_COUNT];
} work_retird_workqueue_t;


static work_retird_workqueue_t s_work_controls;

static void DC_RetireHandler(struct work_struct *psWork)
{
	work_retired_t * configs = container_of(psWork, work_retired_t, swork);

	if (configs->hconfig)
		DCDisplayConfigurationRetired(configs->hconfig);

	configs->hconfig = IMG_NULL;

	return;
}

static void DCDisplayConfigurationRetired_ext(IMG_HANDLE handle)
{
	int idx;

	if(s_work_controls.p_retire_wq == NULL)
	{
		s_work_controls.p_retire_wq = create_singlethread_workqueue("dc_retired_queue");
		if (s_work_controls.p_retire_wq == NULL)
		{
			DBG_D4_PRINT(DBG_D4_ERROR, "DC_GM: Oppsss not create workqueue !!\n");
			return;
		}
	}

	spin_lock(&dc_retired_ext_lock);

	idx = s_work_controls.current_work_index;

	if (s_work_controls.work_array[idx].hconfig == IMG_NULL)
	{
		s_work_controls.work_array[idx].hconfig = handle;
	}
	else
	{
		DBG_D4_PRINT(DBG_D4_ERROR, "DC_GM: Oppsss not use work items !!\n");
		spin_unlock(&dc_retired_ext_lock);
		return;
	}

	INIT_WORK(&s_work_controls.work_array[idx].swork, DC_RetireHandler);
	if (0 != queue_work(s_work_controls.p_retire_wq, &s_work_controls.work_array[idx].swork))
	{
		s_work_controls.current_work_index = (idx+1)%DC_WORK_COUNT;
	}
	else
	{
		DBG_D4_PRINT(DBG_D4_ERROR, "DC_GM: Oppsss not use queue_work !!\n");
	}

	spin_unlock(&dc_retired_ext_lock);

	return;
}

static unsigned int g_context_count;

MODULE_SUPPORTED_DEVICE(DEVNAME);

static DC_GM_DEVICE *gpsDeviceData;

static
IMG_VOID DC_GM_GetInfo(IMG_HANDLE hDeviceData,
						  DC_DISPLAY_INFO *psDisplayInfo)
{
	PVR_UNREFERENCED_PARAMETER(hDeviceData);

	strncpy(psDisplayInfo->szDisplayName, DRVNAME, DC_NAME_SIZE);

	psDisplayInfo->ui32MinDisplayPeriod = 0;
	psDisplayInfo->ui32MaxDisplayPeriod = 1;
	psDisplayInfo->ui32MaxPipes = 1;
}

static
PVRSRV_ERROR DC_GM_PanelQueryCount(IMG_HANDLE hDeviceData,
									  IMG_UINT32 *pui32NumPanels)
{
	PVR_UNREFERENCED_PARAMETER(hDeviceData);

	*pui32NumPanels = 1;

	return PVRSRV_OK;
}

static
PVRSRV_ERROR DC_GM_PanelQuery(IMG_HANDLE hDeviceData,
								 IMG_UINT32 ui32PanelsArraySize,
								 IMG_UINT32 *pui32NumPanels,
								 PVRSRV_PANEL_INFO *psPanelInfo)
{
	PVR_UNREFERENCED_PARAMETER(hDeviceData);

	*pui32NumPanels = 1;

	psPanelInfo[0].sSurfaceInfo.sFormat.ePixFormat = gpsDeviceData->ePixFormat;
	psPanelInfo[0].sSurfaceInfo.sDims.ui32Width    = FALLBACK_PHYSICAL_WIDTH;
	psPanelInfo[0].sSurfaceInfo.sDims.ui32Height   = FALLBACK_PHYSICAL_HEIGHT;

	psPanelInfo[0].ui32RefreshRate = FALLBACK_REFRESH_RATE;

	psPanelInfo[0].ui32XDpi = FALLBACK_PHYSICAL_WIDTH;

	psPanelInfo[0].ui32YDpi = FALLBACK_PHYSICAL_HEIGHT;

	return PVRSRV_OK;
}

static
PVRSRV_ERROR DC_GM_FormatQuery(IMG_HANDLE hDeviceData,
								  IMG_UINT32 ui32NumFormats,
								  PVRSRV_SURFACE_FORMAT *pasFormat,
								  IMG_UINT32 *pui32Supported)
{
	DC_GM_DEVICE *psDeviceData = hDeviceData;
	int i;

	for(i = 0; i < ui32NumFormats; i++)
	{
		pui32Supported[i] = 0;

		if(pasFormat[i].ePixFormat == psDeviceData->ePixFormat)
			pui32Supported[i]++;
	}

	return PVRSRV_OK;
}

static
PVRSRV_ERROR DC_GM_DimQuery(IMG_HANDLE hDeviceData,
							   IMG_UINT32 ui32NumDims,
							   PVRSRV_SURFACE_DIMS *psDim,
							   IMG_UINT32 *pui32Supported)
{
	int i;
	PVR_UNREFERENCED_PARAMETER(hDeviceData);


	for(i = 0; i < ui32NumDims; i++)
	{
		pui32Supported[i] = 0;

		if(psDim[i].ui32Width  == FALLBACK_PHYSICAL_WIDTH &&
		   psDim[i].ui32Height == FALLBACK_PHYSICAL_HEIGHT)
			pui32Supported[i]++;
	}

	return PVRSRV_OK;
}

static
PVRSRV_ERROR DC_GM_ContextCreate(IMG_HANDLE hDeviceData,
									IMG_HANDLE *hDisplayContext)
{
	DC_GM_CONTEXT *psDeviceContext;
	PVRSRV_ERROR eError = PVRSRV_OK;

	psDeviceContext = kzalloc(sizeof(DC_GM_CONTEXT), GFP_KERNEL);
	if(!psDeviceContext)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_out;
	}

	g_context_count++;

	DBG_D4_PRINT(DBG_D4_INFO, "DC_GM: context create-> count = (%d)\n", g_context_count);

	psDeviceContext->psDeviceData = hDeviceData;
	*hDisplayContext = psDeviceContext;

err_out:
	return eError;
}

static PVRSRV_ERROR
DC_GM_ContextConfigureCheck(IMG_HANDLE hDisplayContext,
							   IMG_UINT32 ui32PipeCount,
							   PVRSRV_SURFACE_CONFIG_INFO *pasSurfAttrib,
							   IMG_HANDLE *ahBuffers)
{
	DC_GM_CONTEXT *psDeviceContext = hDisplayContext;

	DBG_D4_PRINT(DBG_D4_DEBUG,"\n\nDC_GM __%s__(W:%d): -- pipecount = %d (custom:%x)\n", __func__, psDeviceContext->window_idx, ui32PipeCount, pasSurfAttrib->ui32Custom);

	if(ui32PipeCount != 1)
		return PVRSRV_ERROR_DC_INVALID_CONFIG;


	if (KM_CHECK_UPDATE_CUSTOM_ID == pasSurfAttrib->ui32Custom)
	{
		DBG_D4_PRINT(DBG_D4_DEBUG,"\n\nDC_GM __%s__(W:%d): -- UpdateCount = %d\n", __func__, psDeviceContext->window_idx, psDeviceContext->ui32UpdateCount);

		if ( psDeviceContext->ui32UpdateCount == 0 )
			return PVRSRV_ERROR_RETRY;
		else
			psDeviceContext->ui32UpdateCount--;
	}
	else if(KM_CHECK_RETIRE_CUSTOM_ID == pasSurfAttrib->ui32Custom)
	{
		if(psDeviceContext->hReadyConfigData != IMG_NULL)
		{
			DCDisplayConfigurationRetired_ext(psDeviceContext->hReadyConfigData);
			psDeviceContext->hReadyConfigData = IMG_NULL;
		}
	}

	return PVRSRV_OK;
}

static
IMG_VOID DC_GM_ContextConfigure(IMG_HANDLE hDisplayContext,
								   IMG_UINT32 ui32PipeCount,
								   PVRSRV_SURFACE_CONFIG_INFO *pasSurfAttrib,
								   IMG_HANDLE *ahBuffers,
								   IMG_UINT32 ui32DisplayPeriod,
								   IMG_HANDLE hConfigData)
{
	DC_GM_CONTEXT *psDeviceContext = hDisplayContext;

	PVR_UNREFERENCED_PARAMETER(pasSurfAttrib);
	PVR_UNREFERENCED_PARAMETER(ui32DisplayPeriod);


	DBG_D4_PRINT(DBG_D4_DEBUG,"\n\nDC_GM(W:%d): -- pipecount = %d, hConfigData = %x\n", psDeviceContext->window_idx, ui32PipeCount, (unsigned int)hConfigData);
	
	if(ui32PipeCount != 0)
	{
		DC_GM_BUFFER * psBuffer;

		BUG_ON(ahBuffers == IMG_NULL);

		psBuffer = ahBuffers[0];

		psDeviceContext->hReadyConfigData = psDeviceContext->hLastConfigData;

		psDeviceContext->hLastConfigData = hConfigData;

		psDeviceContext->ui32UpdateCount++;
	}
	else
	{
		DBG_D4_PRINT(DBG_D4_DEBUG, "DC_GM(W:%d): -- pipecount = %d, hConfigData = %x\n", psDeviceContext->window_idx, ui32PipeCount, (unsigned int)hConfigData);

		if(psDeviceContext->hReadyConfigData != IMG_NULL)
		{
			DCDisplayConfigurationRetired_ext(psDeviceContext->hReadyConfigData);
		}

		if(psDeviceContext->hLastConfigData != IMG_NULL)
		{
			DCDisplayConfigurationRetired_ext(psDeviceContext->hLastConfigData);
		}

		psDeviceContext->hLastConfigData = NULL;

		if (hConfigData != IMG_NULL)
		{
			DCDisplayConfigurationRetired_ext(hConfigData);
		}
	}

	return;
}

static
IMG_VOID DC_GM_ContextDestroy(IMG_HANDLE hDisplayContext)
{
	DC_GM_CONTEXT *psDeviceContext = hDisplayContext;

	g_context_count--;
	
	DBG_D4_PRINT(DBG_D4_INFO, "DC_GM: context dest-> count = (%d)\n", g_context_count);

	BUG_ON(psDeviceContext->hLastConfigData != IMG_NULL);

	kfree(psDeviceContext);
}

#define BYTE_TO_PAGES(range) (((range) + (PAGE_SIZE - 1)) >> PAGE_SHIFT)

static
PVRSRV_ERROR DC_GM_BufferAlloc(IMG_HANDLE hDisplayContext,
								  DC_BUFFER_CREATE_INFO *psCreateInfo,
								  IMG_DEVMEM_LOG2ALIGN_T *puiLog2PageSize,
								  IMG_UINT32 *pui32PageCount,
								  IMG_UINT32 *pui32PhysHeapID,
								  IMG_UINT32 *pui32ByteStride,
								  IMG_HANDLE *phBuffer)
{
	DC_GM_CONTEXT *psDeviceContext = hDisplayContext;
	DC_GM_DEVICE *psDeviceData = psDeviceContext->psDeviceData;
	PVRSRV_SURFACE_INFO *psSurfInfo = &psCreateInfo->sSurface;
	PVRSRV_ERROR eError = PVRSRV_OK;
	DC_GM_BUFFER *psBuffer;
	IMG_UINT32 ui32ByteSize;

	if(psDeviceContext->ui32AllocCount == NUM_PREFERRED_BUFFERS)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_out;
	}

	if(psSurfInfo->sFormat.ePixFormat != psDeviceData->ePixFormat)
	{
		eError = PVRSRV_ERROR_UNSUPPORTED_PIXEL_FORMAT;
		goto err_out;
	}

	psBuffer = kmalloc(sizeof(DC_GM_BUFFER), GFP_KERNEL);
	if(!psBuffer)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_out;
	}

	psBuffer->psDeviceContext = psDeviceContext;
	psBuffer->ui32ByteStride =
		psSurfInfo->sDims.ui32Width * psCreateInfo->ui32BPP;

	psBuffer->ui32Width = psSurfInfo->sDims.ui32Width;
	psBuffer->ui32Height = psSurfInfo->sDims.ui32Height;
	psBuffer->ui32BufferID = psDeviceContext->ui32AllocCount;

	ui32ByteSize = psBuffer->ui32ByteStride * psBuffer->ui32Height;

	*puiLog2PageSize = PAGE_SHIFT;
	*pui32PageCount	 = BYTE_TO_PAGES(ui32ByteSize);
	*pui32PhysHeapID = DC_PHYS_HEAP_ID;
	*pui32ByteStride = psBuffer->ui32ByteStride;
	*phBuffer		 = psBuffer;

	/* If we don't support flipping, allow this code to give every
	 * allocated buffer the same ID. This means that the display
	 * won't be panned, and the same page list will be used for
	 * every allocation.
	 */

	psDeviceContext->ui32AllocCount++;

	psDeviceContext->saWinInfo[psBuffer->ui32BufferID] = psSurfInfo->sWinInfo;
	psDeviceContext->window_idx = psSurfInfo->sWinInfo.ui32WindowId;
	
err_out:
	return eError;
}

static
PVRSRV_ERROR DC_GM_BufferAcquire(IMG_HANDLE hBuffer,
									IMG_DEV_PHYADDR *pasDevPAddr,
									IMG_PVOID *ppvLinAddr)
{
	DC_GM_BUFFER *psBuffer = hBuffer;
	DC_GM_CONTEXT *psDeviceContext = psBuffer->psDeviceContext;
	IMG_UINT32 ui32ByteSize = psBuffer->ui32ByteStride * psBuffer->ui32Height;
	IMG_UINTPTR_T uiStartAddr;
	IMG_UINT32 i, ui32MaxLen;

	uiStartAddr = psDeviceContext->saWinInfo[psBuffer->ui32BufferID].ui32PhyMemptr;

	ui32MaxLen = ui32ByteSize;

	for (i = 0; i < BYTE_TO_PAGES(ui32ByteSize); i++)
	{
		BUG_ON(i * PAGE_SIZE >= ui32MaxLen);
		pasDevPAddr[i].uiAddr = uiStartAddr + (i * PAGE_SIZE);
	}

	/* We're UMA, so services will do the right thing and make
	 * its own CPU virtual address mapping for the buffer.
	 */
	*ppvLinAddr = IMG_NULL;

	DBG_D4_PRINT(DBG_D4_EXT, "DC_GM: %s OK\n", __func__);
	return PVRSRV_OK;
}

static IMG_VOID DC_GM_BufferRelease(IMG_HANDLE hBuffer)
{
	PVR_UNREFERENCED_PARAMETER(hBuffer);
}

static IMG_VOID DC_GM_BufferFree(IMG_HANDLE hBuffer)
{
	kfree(hBuffer);
}

/* If we can flip, we need to make sure we have the memory to do so.
 *
 * We'll assume that the gm device provides extra space in
 * yres_virtual for panning; xres_virtual is theoretically supported,
 * but it involves more work.
 *
 * If the gm device doesn't have yres_virtual > yres, we'll try
 * requesting it before bailing. Userspace applications commonly do
 * this with an FBIOPUT_VSCREENINFO ioctl().
 *
 * Another problem is with a limitation in the services DC -- it
 * needs framebuffers to be page aligned (this is a SW limitation,
 * the HW can support non-page-aligned buffers). So we have to
 * check that stride * height for a single buffer is page aligned.
 */

static int __init DC_GM_init(void)
{
	static DC_DEVICE_FUNCTIONS sDCFunctions =
	{
		.pfnGetInfo					= DC_GM_GetInfo,
		.pfnPanelQueryCount			= DC_GM_PanelQueryCount,
		.pfnPanelQuery				= DC_GM_PanelQuery,
		.pfnFormatQuery				= DC_GM_FormatQuery,
		.pfnDimQuery				= DC_GM_DimQuery,
		.pfnContextCreate			= DC_GM_ContextCreate,
		.pfnContextDestroy			= DC_GM_ContextDestroy,
		.pfnContextConfigure		= DC_GM_ContextConfigure,
		.pfnContextConfigureCheck	= DC_GM_ContextConfigureCheck,
		.pfnBufferAlloc				= DC_GM_BufferAlloc,
		.pfnBufferAcquire			= DC_GM_BufferAcquire,
		.pfnBufferRelease			= DC_GM_BufferRelease,
		.pfnBufferFree				= DC_GM_BufferFree,
	};

	int err = -ENODEV;

	gpsDeviceData = kmalloc(sizeof(DC_GM_DEVICE), GFP_KERNEL);
	if(!gpsDeviceData)
		goto err_module_put;

	gpsDeviceData->ePixFormat = IMG_PIXFMT_B8G8R8A8_UNORM;

	if(DCRegisterDevice(&sDCFunctions,
						MAX_COMMANDS_IN_FLIGHT,
						gpsDeviceData,
						&gpsDeviceData->hSrvHandle) != PVRSRV_OK)
		goto err_kfree;

	gpsDeviceData->bCanFlip = IMG_TRUE;


	pr_info("Found usable gm device (using externl windows):\n"
			"xres x yres      = %ux%u\n"
			"img pix fmt      = %u\n",
			FALLBACK_PHYSICAL_WIDTH, FALLBACK_PHYSICAL_HEIGHT,
			gpsDeviceData->ePixFormat);

	DBG_D4_PRINT(DBG_D4_EXT, "build date/time  = (%s)/(%s)\n", __DATE__, __TIME__);

	err = 0;

	g_context_count = 0;

	return err;

err_kfree:
	kfree(gpsDeviceData);
err_module_put:
	return err;
}

static void __exit DC_GM_exit(void)
{
	DC_GM_DEVICE *psDeviceData = gpsDeviceData;

	DCUnregisterDevice(psDeviceData->hSrvHandle);

	kfree(psDeviceData);
}

module_init(DC_GM_init);
module_exit(DC_GM_exit);
