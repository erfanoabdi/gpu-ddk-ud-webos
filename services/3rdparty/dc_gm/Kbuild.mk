########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Strictly Confidential.
### ###########################################################################

ccflags-y += \
 -I$(TOP)/services/3rdparty/dc_gm

dc_gm-y += \
	services/3rdparty/dc_gm/dc_gm.o

ifneq ($(W),1)
#CFLAGS_dc_gm.o := -Werror
endif
