########################################################################### ###
#@File
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Strictly Confidential.
### ###########################################################################

pvrsrvkm-y += services/system/$(PVR_SYSTEM)/sysconfig.o

ifeq ($(SUPPORT_ION),1)
pvrsrvkm-y += services/system/common/env/linux/ion_support_generic.o
endif
