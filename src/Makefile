# Include directories
INCLUDES += ../include
CFLAGS += $(foreach DIR,$(INCLUDES),-I$(DIR))

LIB_NAME = libhsakmt.so
LIB_MAJOR_VER = 1

# Compiler options
CFLAGS += -fPIC # Position-independent code required to build shared library
CFLAGS += -W -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS += -Wformat-security -Wswitch-default -Wundef \
	  -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual \
	  -Wlogical-op -Wstrict-prototypes -Wmissing-prototypes    \
	  -Wmissing-declarations -Wredundant-decls                 \
	  -Wunreachable-code
CFLAGS += -std=gnu99 -ggdb -pthread -fvisibility=hidden -O2

LDFLAGS += -lrt -pthread -Wl,--version-script=libhsakmt.ver -Wl,-soname=$(LIB_NAME).$(LIB_MAJOR_VER)

OBJS = debug.o globals.o memory.o perfctr.o time.o version.o \
    events.o openclose.o queues.o topology.o fmm.o

.PHONY: all lnx lnx64a clean

# Default target
all: lnx lnx64a

BUILD_ROOT = ../build
BUILDDIR = $(BUILD_ROOT)/$(MAKECMDGOALS)

TARGET = $(addprefix $(BUILDDIR)/,$(OBJS))

$(BUILDDIR)/$(LIB_NAME).$(LIB_MAJOR_VER): $(TARGET)
	gcc -shared $(LDFLAGS) -o $@ $^

$(BUILDDIR)/$(LIB_NAME): $(BUILDDIR)/$(LIB_NAME).$(LIB_MAJOR_VER)
	@ln -sf $(LIB_NAME).$(LIB_MAJOR_VER) $(BUILDDIR)/$(LIB_NAME)

lnx: CFLAGS += -m32
lnx: LDFLAGS += -m32
lnx: $(BUILDDIR)/$(LIB_NAME)

lnx64a: $(BUILDDIR)/$(LIB_NAME)

clean:
	rm -rf $(BUILD_ROOT)

#Rule
$(BUILDDIR)/%.o: %.c
	@echo Compiling $^
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c $< -o $@
