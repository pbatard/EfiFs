#
# Build control file for the EFI drivers, as built with GNU-EFI
#

DRIVERNAME      = fs_driver
SRCDIR          = .
VPATH           = $(SRCDIR)
HOSTARCH        = $(shell uname -m | sed s,i[3456789]86,ia32,)
ARCH           ?= $(HOSTARCH)

# Note: IA64 options are untested; taken from Debian's rEFIt package.
ifeq ($(ARCH),ia64)
  # EFI specs allows only lower floating point partition to be used
  ARCH_C_CFLAGS  = -frename-registers -mfixed-range=f32-f127
  FILENAME_CODE = ia64
endif

ifeq ($(ARCH),ia32)
  LIBEG = build32
  ARCH_C_FLAGS = -m32 -malign-double
  FILENAME_CODE = ia32
  LD_CODE = elf_i386
endif

ifeq ($(ARCH),x86_64)
  LIBEG = build64
  ARCH_C_FLAGS = "-DEFIAPI=__attribute__((ms_abi))" -m64
  FILENAME_CODE = x64
  LD_CODE = elf_x86_64
endif

LOCAL_CPPFLAGS  = $(ARCH_C_FLAGS) -I$(SRCDIR) -I$(SRCDIR)/../include
OBJS            = $(DRIVERNAME).o
TARGET          = $(DRIVERNAME)_$(FILENAME_CODE).efi

all: $(TARGET)

include $(SRCDIR)/../Make.common

$(TARGET): $(SHLIB_TARGET)
	@echo [OBJ] $@
	@$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel \
		   -j .rela -j .reloc --target=$(FORMAT_DRIVER)  $< $@
	@chmod a-x $(TARGET)

clean:
	rm -f $(OBJS) *.o *.so *.efi

install: all
	@cp -v $(TARGET) $(INSTALL_DIR)
