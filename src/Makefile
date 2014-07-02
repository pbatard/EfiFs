# meta-Makefile for the filesystem drivers
#

INSTALL_DIR = /boot/efi/EFI/refind/drivers_x64

FILESYSTEMS = affs bfs exfat hfs hfsplus jfs ntfs xfs

all:	$(FILESYSTEMS)

affs:
	@rm -f fs_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

bfs:
	@rm -f fs_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

btrfs:
	@rm -f fs_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

exfat:
	@rm -f fs_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

hfs:
	@rm -f fs_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

# TODO: add compression
hfsplus:
	@rm -f fs_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

jfs:
	@rm -f fs_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

ntfs:
	@rm -f fs_driver.o
	+make DRIVERNAME=$@ EXTRAMODULE=$@comp -f Make.gnuefi

xfs:
	@rm -f fs_driver.o
	+make DRIVERNAME=$@ -f Make.gnuefi

install: all
	@mkdir -p $(INSTALL_DIR)
	@cp -v *.efi $(INSTALL_DIR)

clean:
	+make -f Make.gnuefi clean
