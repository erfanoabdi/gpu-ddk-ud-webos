########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Strictly Confidential.
### ###########################################################################

include ../config/preconfig.mk
include ../config/window_system.mk

PVR_SYSTEM := rgx_h13
KERNEL_COMPONENTS := srvkm
DRM_DC_MODULE ?= 0

ifeq ($(DRM_DC_MODULE),1)
DISPLAY_CONTROLLER := dc_drmexample
DISPLAY_CONTROLLER_DIR := dc_drmexample

SUPPORT_DRM := 1
SUPPORT_DRM_DC_MODULE := 1
else
 PVR_LINUX_MISR_USING_PRIVATE_WORKQUEUE ?= 1
DISPLAY_CONTROLLER := dc_fbdev
DISPLAY_CONTROLLER_DIR := dc_fbdev

ifeq ($(INCLUDE_LG_WEBOS), 1)
## [LG] start for WebOS GM
 DISPLAY_CONTROLLER += dc_gm
 DISPLAY_CONTROLLER_DIR += dc_gm
## [LG] end 
endif

endif

ifeq ($(SUPPORT_DRM),1)
LDM_PLATFORM := 1
PVR_DRM_PLATFORM_DEV := 1
PVR_DRM_STATIC_BUS_ID := 1
ifeq ($(call kernel-version-at-least,3,1),true)
ifeq ($(call kernel-version-at-least,3,4),true)
PVR_DRM_DEV_BUS_ID := "\"platform:rgxnohw:00\""
else
PVR_DRM_DEV_BUS_ID := "\"platform:rgxnohw:-1\""
endif
endif
ifeq ($(SUPPORT_DRM_DC_MODULE),1)
KERNEL_COMPONENTS += $(DISPLAY_CONTROLLER)
else
EXTRA_PVRSRVKM_COMPONENTS += $(DISPLAY_CONTROLLER_DIR)
endif
else
KERNEL_COMPONENTS += $(DISPLAY_CONTROLLER)
endif

NO_HARDWARE := 0

GLES2_EXTENSION_EGL_IMAGE_EXTERNAL := 1


ifeq ($(TARGETING_ARM),1)
SYS_CFLAGS := -march=armv7-a
CACHEFLUSH_TYPE ?= CACHEFLUSH_GENERIC
else
CACHEFLUSH_TYPE ?= CACHEFLUSH_X86
endif

# Ubuntu ARM compilers need an explicit -marm option to build native ARM code,
# rather than ARM thumb code.
EXPLICIT_NOT_THUMB :=
ifeq ($(CROSS_COMPILE),)
EXPLICIT_NOT_THUMB = 1
endif
ifeq ($(CROSS_COMPILE),arm-linux-gnueabi-)
EXPLICIT_NOT_THUMB = 1
endif
ifeq ($(CROSS_COMPILE),arm-linux-gnueabihf-)
EXPLICIT_NOT_THUMB = 1
endif

ifneq ($(EXPLICIT_NOT_THUMB),)
SYS_CFLAGS += -marm
endif

RGX_BVNC ?= 1.6.2.0
# Using range based invalidation seems to have issues
PVR_LINUX_DONT_USE_RANGE_BASED_INVALIDATE := 1

# Should be last
include ../config/core.mk
-include ../common/lws.mk
include ../common/drm.mk
-include ../common/opencl.mk
include ../common/pvrgdb.mk

$(eval $(call TunableUserConfigC,SGLTRACE,))
