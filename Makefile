TARGET = x64

ifeq ($(TARGET),x64)
	ARCH          = x86_64
	GCC_ARCH      = x86_64
	CROSS_COMPILE = x86_64-w64-mingw32-
	LIB_ARCH      = x86_64
	CFLAGS        = -m64 -mno-red-zone
else ifeq ($(TARGET),ia32)
	ARCH          = ia32
	GCC_ARCH      = i686
	CROSS_COMPILE = i686-w64-mingw32-
	LIB_ARCH      = i386
	CFLAGS        = -m32 -mno-red-zone
else ifeq ($(TARGET),arm)
	ARCH          = arm
	CROSS_COMPILE = arm-linux-gnueabihf-
	LIB_ARCH      = arm
	CFLAGS        = -marm -fpic -fshort-wchar -nostdlib
endif

# MinGW doesn't need a prefix
ifneq ($(SYSTEMROOT),)
  CROSS_COMPILE =
endif

GNUEFI_PATH     = $(CURDIR)/gnu-efi
GRUB_PATH       = $(CURDIR)/grub
SRC_PATH        = $(CURDIR)/src
LIB_TARGET      = $(SRC_PATH)/libgrub.a

CC             := $(CROSS_COMPILE)gcc
CFLAGS         += -fno-stack-protector -Wshadow -Wall -Wunused -Werror-implicit-function-declaration
CFLAGS         += -I$(GNUEFI_PATH)/inc -I$(GNUEFI_PATH)/inc/$(ARCH) -I$(GNUEFI_PATH)/inc/protocol
CFLAGS         += -DCONFIG_$(ARCH) -D__MAKEWITH_GNUEFI -DGNU_EFI_USE_MS_ABI -DGRUB_FILE=\"$(subst $(srcdir)/,,$<)\"
CFLAGS         += -DLZO_CFG_FREESTANDING 
CFLAGS         += -I$(GRUB_PATH) -I$(GRUB_PATH)/include -I$(GRUB_PATH)/grub-core/lib/minilzo
OBJS            = $(SRC_PATH)/grub.o $(GRUB_PATH)/grub-core/kern/err.o $(GRUB_PATH)/grub-core/kern/list.o \
                  $(GRUB_PATH)/grub-core/kern/misc.o $(GRUB_PATH)/grub-core/lib/crc.o \
                  $(GRUB_PATH)/grub-core/lib/minilzo/minilzo.o

ifeq (, $(shell which $(CC)))
  $(error The selected compiler ($(CC)) was not found)
endif

GCCVERSION := $(shell $(CC) -dumpversion | cut -f1 -d.)
GCCMINOR   := $(shell $(CC) -dumpversion | cut -f2 -d.)
GCCMACHINE := $(shell $(CC) -dumpmachine)
GCCNEWENOUGH := $(shell ( [ $(GCCVERSION) -gt "4" ]           \
                          || ( [ $(GCCVERSION) -eq "4" ]      \
                               && [ $(GCCMINOR) -ge "7" ] ) ) \
                        && echo 1)
ifneq ($(GCCNEWENOUGH),1)
  $(error You need GCC 4.7 or later)
endif

ifneq ($(GCC_ARCH),$(findstring $(GCC_ARCH), $(GCCMACHINE)))
  $(error The selected compiler ($(CC)) is not set for $(TARGET))
endif

all: $(GNUEFI_PATH)/$(ARCH)/lib/libefi.a $(GRUB_PATH)/config.h $(GRUB_PATH)/include/grub/cpu_$(LIB_ARCH) $(LIB_TARGET)

$(GNUEFI_PATH)/$(ARCH)/lib/libefi.a:
	$(MAKE) -C$(GNUEFI_PATH) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) lib

$(GRUB_PATH)/include/grub/cpu_$(LIB_ARCH):
	@rm -rf $(GRUB_PATH)/include/grub/cpu*
	@echo  [LN]  $(GRUB_PATH)/include/grub/$(LIB_ARCH) -\> $(GRUB_PATH)/include/grub/cpu
	@cd $(GRUB_PATH)/include/grub && ln -s $(LIB_ARCH) cpu && touch cpu_$(ARCH)

$(GRUB_PATH)/config.h:
	@echo  [CP]  $(notdir $@)
	@cp $(CURDIR)/.msvc/config.h $(GRUB_PATH)

# http://scottmcpeak.com/autodepend/autodepend.html
-include $(OBJS:.o=.d)

%.o: %.c
	@echo  [CC]  $(notdir $@)
	@$(CC) $(CFLAGS) -ffreestanding -c $< -o $@
	@$(CC) -MM $(CFLAGS) $*.c > $*.d

ifneq (,$(filter %.a,$(LIB_TARGET)))
$(LIB_TARGET): $(OBJS)
	@echo  [AR]  $(notdir $@)
	@$(AR) sq $@ $(OBJS)
endif

clean:
	$(MAKE) -C$(GNUEFI_PATH) ARCH=$(ARCH) clean
	rm -rf $(GNUEFI_PATH)/$(ARCH)
	$(shell find . | grep "\.d$$" | xargs rm -f 2>/dev/null)
	rm -rf $(GRUB_PATH)/include/grub/cpu*
	rm -f $(OBJS) $(LIB_TARGET) $(GRUB_PATH)/config.h
