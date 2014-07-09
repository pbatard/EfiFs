# meta-Makefile for the filesystem drivers
#

INSTALL_DIR     = /boot/efi/EFI/refind/drivers_x64
FILESYSTEMS     = affs bfs exfat hfs hfsplus jfs ntfs xfs
HOSTARCH        = $(shell uname -m | sed s,i[3456789]86,ia32,)

ifeq ($(HOSTARCH),ia64)
  DRIVERARCH    = ia64
endif
ifeq ($(HOSTARCH),ia32)
  DRIVERARCH    = ia32
endif
ifeq ($(HOSTARCH),x86_64)
  DRIVERARCH    = x64
endif

all:	$(FILESYSTEMS)

affs:
	@rm -f grub_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

bfs:
	@rm -f grub_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

btrfs:
	@rm -f grub_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

exfat:
	@rm -f grub_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

hfs:
	@rm -f grub_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

# TODO: add compression
hfsplus:
	@rm -f grub_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

jfs:
	@rm -f grub_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

ntfs:
	@rm -f grub_driver.o
	+make DRIVERNAME=$@ EXTRAMODULE=$@comp -f Make.gnuefi

xfs:
	@rm -f grub_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

install: all
	@mkdir -p $(INSTALL_DIR)
	@cp -v $(FILESYSTEMS:%=%_$(DRIVERARCH).efi) $(INSTALL_DIR)

clean:
	+make -f Make.gnuefi clean
