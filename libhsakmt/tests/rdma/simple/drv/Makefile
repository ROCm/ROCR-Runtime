
ifeq ($(src),)
	src := $$PWD
endif



ifneq ($(KERNELRELEASE),)
# Specify default location of RDMA header
RDMA_HEADER_DIR := $(ROOT_OF_ROOTS)/kernel/drivers/gpu/drm/amd/include
ifeq ("$(wildcard $(RDMA_HEADER_DIR)/amd_rdma.h)","")
RDMA_HEADER_DIR := /usr/src/linux-headers-$(shell uname -r)/drivers/gpu/drm/amd/include
ifeq ("$(wildcard $(RDMA_HEADER_DIR)/amd_rdma.h)","")
$(error amd_rdma.h header is not found)
endif
endif

$(info INFO: RDMA Header directory is $(RDMA_HEADER_DIR))

kver_major:=$(shell echo $(KERNELRELEASE) | awk -F '.' '// { print $$2;}' )

obj-m += amdp2ptest.o

ccflags-y += -I $(RDMA_HEADER_DIR)

else

KDIR ?= /lib/modules/$(shell uname -r)/build
REL := $(subst ., , $(subst -, , $(shell uname -r)))
REL_MAJOR  := $(word 1,$(REL))
REL_MEDIUM := $(word 2,$(REL))
REL_MINOR  := $(word 3,$(REL))

all: default


default:
	@ $(MAKE) -C $(KDIR) M=$$PWD  modules

install:
	$(MAKE) -C $(KDIR) M=$$PWD  modules_install

help:
	$(MAKE) -C $(KDIR) M=$$PWD help

clean:
	rm -rf *.o *.ko* *.mod.* .*.cmd Module.symvers modules.order .tmp_versions/ *~ core .depend TAGS

TAGS:
	find $(KERNELDIR) -follow -name \*.h -o -name \*.c  |xargs etags

.PHONY: clean all help install default linksyms

endif
