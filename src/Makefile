# Our directory
THUNK_ROOT ?= $(abspath ../)

# Include directories
INCLUDE_DIR := ../include
INCLUDES += $(INCLUDE_DIR)
CFLAGS += $(foreach DIR,$(INCLUDES),-I$(DIR))

LIB_NAME = libhsakmt.so
LIB_MAJOR_VER = 1

PACKAGE_MAJOR_VER = 2
PACKAGE_MINOR_VER = 0
PACKAGE_PATCH_VER ?= 0
PACKAGE_VER = $(PACKAGE_MAJOR_VER).$(PACKAGE_MINOR_VER).$(PACKAGE_PATCH_VER)
export PACKAGE_VER

# Compiler options
CFLAGS += -fPIC # Position-independent code required to build shared library
CFLAGS += -W -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS += -Wformat-security -Wswitch-default -Wundef \
	  -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual \
	  -Wlogical-op -Wstrict-prototypes -Wmissing-prototypes    \
	  -Wmissing-declarations -Wredundant-decls                 \
	  -Wunreachable-code
CFLAGS += -std=gnu99 -pthread -fvisibility=hidden -O2

# If this is not a release build, add debug-specific flags
ifneq ($(REL),1)
  CFLAGS += -ggdb
endif

LDFLAGS += -lrt -pthread -Wl,--version-script=libhsakmt.ver -Wl,-soname=$(LIB_NAME).$(LIB_MAJOR_VER) -Wl,-z,nodelete

OBJS = debug.o globals.o memory.o perfctr.o time.o version.o \
    events.o openclose.o queues.o topology.o fmm.o pmc_table.o \
    libhsakmt.o

.PHONY: all lnx lnx64a clean

# Default target
all: lnx lnx64a

# To relocate build output, set "O=target/dir/"
# otherwise it defaults to a local directory

ifdef O
  BUILD_ROOT := $(O)
else
  BUILD_ROOT := ../build
endif
BUILD_ROOT := $(abspath $(BUILD_ROOT))

BUILDDIR = $(BUILD_ROOT)/$(MAKECMDGOALS)
TARGET = $(addprefix $(BUILDDIR)/,$(OBJS))

$(BUILDDIR)/$(LIB_NAME).$(LIB_MAJOR_VER): $(TARGET)
	gcc -shared $(LDFLAGS) -o $@ $^

$(BUILDDIR)/$(LIB_NAME): $(BUILDDIR)/$(LIB_NAME).$(LIB_MAJOR_VER)
	@ln -sf $(LIB_NAME).$(LIB_MAJOR_VER) $(BUILDDIR)/$(LIB_NAME)

lnx: CFLAGS += -m32
lnx: LDFLAGS += -m32
lnx: $(BUILDDIR)/$(LIB_NAME)

PACKAGE_DIR = $(BUILDDIR)/libhsakmt
DEBIAN_DIR = ../DEBIAN
LDCONF_FILE = $(DEBIAN_DIR)/x86_64-libhsakmt.conf
LDCONF_INSTALL_DIR = $(PACKAGE_DIR)/etc/ld.so.conf.d
ROCM_ROOT = $(PACKAGE_DIR)/opt/rocm
ROCM_LIB = $(ROCM_ROOT)/lib
ROCM_INCLUDE = $(ROCM_ROOT)/include
INSTALL_LIB_DIR = libhsakmt/lib
INSTALL_INCLUDE_DIR = libhsakmt/include/libhsakmt
export PACKAGE_DIR

package-common: lnx64a
	@mkdir -p $(PACKAGE_DIR)

	@mkdir -p $(LDCONF_INSTALL_DIR)
	@cp -r $(LDCONF_FILE) $(LDCONF_INSTALL_DIR)

	@mkdir -p $(ROCM_ROOT)/$(INSTALL_LIB_DIR)
	@cp $(BUILDDIR)/$(LIB_NAME).$(LIB_MAJOR_VER) $(ROCM_ROOT)/$(INSTALL_LIB_DIR)

	@mkdir -p $(ROCM_ROOT)/$(INSTALL_INCLUDE_DIR)
	@cp -r $(INCLUDE_DIR)/* $(ROCM_ROOT)/$(INSTALL_INCLUDE_DIR)

	@mkdir -p $(ROCM_LIB)
	@ln -fs ../$(INSTALL_LIB_DIR)/$(LIB_NAME).$(LIB_MAJOR_VER) $(ROCM_LIB)/$(LIB_NAME)

	@mkdir -p $(ROCM_INCLUDE)
	ln -fs ../$(INSTALL_INCLUDE_DIR) $(ROCM_INCLUDE)/

deb: package-common
	@mkdir -p $(PACKAGE_DIR)/DEBIAN
	@sed 's/\$$version/$(PACKAGE_VER)/g' $(DEBIAN_DIR)/control > $(PACKAGE_DIR)/DEBIAN/control
	@fakeroot dpkg-deb --build $(PACKAGE_DIR) \
		$(BUILDDIR)/hsakmt-dev-$(PACKAGE_VER)-amd64.deb

rpm: package-common
	@rpmbuild --define '_topdir $(BUILD_ROOT)/rpm' -ba $(THUNK_ROOT)/RPM/libhsakmt.spec

lnx64a: $(BUILDDIR)/$(LIB_NAME)

clean:
	rm -rf $(BUILD_ROOT)

#Rule
$(BUILDDIR)/%.o: %.c ../include/hsakmt.h ../include/hsakmttypes.h ../include/linux/kfd_ioctl.h
	@echo Compiling $^
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -o $@
