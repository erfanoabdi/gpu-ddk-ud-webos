#
# RGX DDK integrated build system (SIC Lab. LG Electronics,Inc)
#
#
#------------------------------------------------------------------------------
# macro function
#------------------------------------------------------------------------------
_cnd_define  = $(if $(subst $(1)A,,$(strip $($2))A),,-D$2)
_cnd_assign  = $(if $(subst $(1)A,,$(strip $($2))A),$4,$3)
cond_define  = $(call _cnd_define,$(strip $1),$(strip $2))
cond_assign  = $(call _cnd_assign,$(strip $1),$(strip $2),$(strip $3),$(strip $4))

V			?= 0
Q			:= $(call cond_assign, 0, V, @,)
ECHO		:= $(Q)echo
MKDIR		:= $(Q)mkdir
CP			:= $(Q)cp
RM			:= $(Q)rm
MV		:= $(Q)mv

ROGUE_KM_DIR	:= $(CURDIR)
RGX_DDK_CFG := h13b0-WebOS.cfg

#------------------------------------------------------------------------------
# load cfg
#------------------------------------------------------------------------------
LINUX					:= linux
export PLATFORM_CHIP 	:= lg1154
export SUPPORT_LG_BOARD := 1
export INCLUDE_LG_PATCH := 1
export INCLUDE_LG_NC4	:= 0
export INCLUDE_LG_WEBOS := 0

export PDUMP			:= 0
export EXCLUDED_APIS	:= scripts opencl

export ROGUE_BUILD		:= release
export ROGUE_KM_BUILD	:= release

export RGX_BVNC			:= 1.33.2.5
export KERNELDIR		?= $(PWD)/$(LINUX)
export METAG_INST_ROOT	:= $(PWD)/../toolchain-img

export CROSS_COMPILE	?= arm-lg115x-linux-gnueabi-
export INSTALL_DIR		?= $(ROGUE_KM_DIR)/result
export PVR_LINUX_MISR_USING_PRIVATE_WORKQUEUE := 1

export SUPPORT_RGXFW_LOG := 0
export SUPPORT_RGXFW_LOG_PM := 0
export SUPPORT_RGXFW_LOG_RTD := 0
export SUPPORT_RGXFW_LOG_CLEANUP := 0

export SUPPORT_PLM := 1
export LDM_PLATFORM := 1

export SUPPORT_OPENGLES1_V1_ONLY := 1

LG_CMN_CFLAGS := 
LG_CMN_CFLAGS += -DSUPPORT_LG_BOARD=$(SUPPORT_LG_BOARD)
LG_CMN_CFLAGS += -DINCLUDE_LG_PATCH=$(INCLUDE_LG_PATCH)
LG_CMN_CFLAGS += -DINCLUDE_LG_NC4=$(INCLUDE_LG_NC4)
LG_CMN_CFLAGS += -DPLATFORM_CHIP=$(PLATFORM_CHIP) -Dlg1154=0x1154 
LG_CMN_CFLAGS += -DTHIS________IS_______TEST__________

ifeq ($(SUPPORT_LG_BOARD), 1)
LG_SYS_CFLAGS := $(LG_CMN_CFLAGS)
LG_SYS_CFLAGS += -D__ARM__ -D__LINUX_ARM_ARCH=7 
LG_SYS_CFLAGS += -fsigned-char -pipe -std=gnu99 -D_REENTRANT -D_GPU_SOURCE -ggdb -funwind-tables
LG_SYS_CFLAGS += -march=armv7-a -mtune=cortex-a9 -mfloat-abi=softfp -mvectorize-with-neon-quad
#LG_SYS_CFLAGS += -march=armv7-a -mtune=cortex-a9 -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=softfp -mvectorize-with-neon-quad -ftree-vectorize
LG_SYS_CFLAGS +=

LG_KM_CFLAGS  := $(LG_CMN_CFLAGS)
#LG_KM_CFLAGS  += -DDISABLE_RGX_FW_LOG_PRINT
ifeq ($(RGX_BVNC), 1.33.2.5)
LG_KM_CFLAGS  += -DCHIP_REV_B0
endif
LG_KM_CFLAGS  +=

export LG_SYS_CFLAGS
export LG_KM_CFLAGS
endif

# Need to add the --sysroot from OE's TOOLCHAIN_OPTIONS to the CFLAGS used by the
# Makefiles for the user-mode code.
LG_SYS_CFLAGS += $(OE_TOOLCHAIN_OPTIONS)

# some fixed cfg
export TARGETING_ARM := 1

ROGUE_BUILD_OPTS	:= BUILD=$(ROGUE_BUILD)
ROGUE_KM_BUILD_OPTS	:= BUILD=$(ROGUE_KM_BUILD)
ROGUE_KM_BUILD_OPTS	+= ARCH=arm

#------------------------------------------------------------------------------
# 
#------------------------------------------------------------------------------
ROGUE_DIR			:= $(ROGUE_KM_DIR)/rogue
ROGUE_BUILD_DIR		:= binary_h13_android_$(ROGUE_BUILD)
ROGUE_KM_BUILD_DIR	:= binary_h13_android_$(ROGUE_KM_BUILD)

TGT_CC		:= $(shell which $(CROSS_COMPILE)gcc)

RGX_DDK_TREE_ALL_IN_ONE ?= $(shell ([ ! -d $(ROGUE_KM_DIR) ] && echo 1)|| echo 0)

.PHONEY : chk_env param help
.PHONEY : check_evn param rogue_km.b install
.PHONEY : rogue_km.clean  rogue_km.install
#------------------------------------------------------------------------------
# main
#------------------------------------------------------------------------------
all: 	chk_env param rogue_km.b install
	$(ECHO) "<!> build done..."

clean: 	rogue_km.clean rogue.clean cmn.clean
	$(ECHO) "<!> clean done..."

install:rogue_km.install 
	$(ECHO) "<!> install done..."

ifeq ($(RENAMING_LIBRARY),1)
ifeq ($(SUPPORT_OPENGLES1_V1_ONLY),1)
	$(MV)	$(INSTALL_DIR)/libGLESv1_CM.so $(INSTALL_DIR)/libGLESv1_CM.so.1.1
endif	
	$(MV)	$(INSTALL_DIR)/libGLESv2.so $(INSTALL_DIR)/libGLESv2.so.2.0
	$(MV)	$(INSTALL_DIR)/libEGL.so $(INSTALL_DIR)/libEGL.so.1.4
endif

ifeq ($(RGX_DDK_TREE_ALL_IN_ONE),1)
	$(CP) -f $(ROGUE_DIR)/$(ROGUE_BUILD_DIR)/target_armv7-a/*.ko $(INSTALL_DIR)
endif


ifeq ($(PDUMP),1)
	$(CP) -Rf $(ROGUE_DIR)/$(ROGUE_BUILD_DIR)/target/pdump 		$(INSTALL_DIR)
endif

rogue_km.b:
ifneq ($(RGX_DDK_TREE_ALL_IN_ONE),1)
	$(MAKE) -j4 -C $(ROGUE_KM_DIR)/build/linux/h13_android $(ROGUE_KM_BUILD_OPTS)
endif

rogue_km.clean:
ifneq ($(RGX_DDK_TREE_ALL_IN_ONE),1)
	$(MAKE) -C $(ROGUE_KM_DIR)/build/linux/h13_android clean clobber $(ROGUE_KM_BUILD_OPTS)
	$(RM) -rf $(ROGUE_KM_DIR)/$(ROGUE_KM_BUILD_DIR)
endif

rogue_km.install:
ifneq ($(RGX_DDK_TREE_ALL_IN_ONE),1)
	$(CP) -f $(ROGUE_KM_DIR)/$(ROGUE_KM_BUILD_DIR)/target_armv7-a/*.ko $(INSTALL_DIR)
endif
#cmn.install:
#	$(CP) -Rf cfg/go $(INSTALL_DIR)

cmn.clean:
	$(RM) -rf $(INSTALL_DIR)

rgxfw_debug:
	$(MAKE) -C $(ROGUE_KM_DIR)/build/linux/h13_android $(ROGUE_BUILD_OPTS) rgxfw_debug


# check build environment before entering the real build process
chk_env: 

	@if [ ! -e $(KERNELDIR) ]; then \
		echo "[ERR] 'linux' kernel link not found or invalid";  \
		exit -1;\
	fi

	@if [ "x" = "x$(TGT_CC)" ]; then \
		echo "[ERR] cross compiler $(CROSS_TOOL)gcc not found. add it to your PATH"; \
		exit -1;\
	fi

	$(MKDIR) -p $(INSTALL_DIR)	

	$(ECHO) "<!> environment check ok.."

#------------------------------------------------------------------------------
# param reporter
#------------------------------------------------------------------------------
param:
	@echo "-------------------------------------------------------------------------------"
	@echo "Q               = $(Q)"
	@echo "CFG_FILE        = $(CFG_FILE)"
	@echo "-------------------------------------------------------------------------------"
	@echo "KENELDIR        = $(subst $(ROGUE_KM_DIR),<top>,$(KERNELDIR))"
	@echo "INSTALL_DIR     = $(subst $(ROGUE_KM_DIR),<top>,$(INSTALL_DIR))"
	@echo "CROSS_COMPILE   = $(CROSS_COMPILE)"
	@echo "CC              = $(CROSS_COMPILE)gcc"
	@echo "RGX_BVNC        = $(RGX_BVNC)"
	@echo "PDUMP           = $(PDUMP)"
	@echo "ROGUE_BUILD     = $(ROGUE_BUILD)"
	@echo "ROGUE_KM_BUILD  = $(ROGUE_KM_BUILD)"
	@echo "-------------------------------------------------------------------------------"
	@echo "SUPPORT_LG_BOARD= $(SUPPORT_LG_BOARD)"
	@echo "INCLUDE_LG_PATCH= $(INCLUDE_LG_PATCH)"
	@echo "INCLUDE_LG_NC4  = $(INCLUDE_LG_NC4)"
	@echo "-------------------------------------------------------------------------------"

#	$(MAKE) -C $(ROGUE_KM_DIR)/rogue_km/build/linux/h13_android param

#------------------------------------------------------------------------------
# help
#------------------------------------------------------------------------------
help:
