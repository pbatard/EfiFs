# meta-Makefile for the filesystem drivers
#

INSTALL_DIR = /boot/efi/EFI/refind/drivers_x64

FILESYSTEMS = ntfs xfs

all:	$(FILESYSTEMS)

exfat:
	@rm -f fs_driver.o
	+make DRIVERNAME=exfat -f Make.gnuefi

ntfs:
	@rm -f fs_driver.o
	+make DRIVERNAME=ntfs -f Make.gnuefi

xfs:
	@rm -f fs_driver.o
	+make DRIVERNAME=xfs -f Make.gnuefi

install:
	@mkdir -p $(INSTALL_DIR)
	@cp -v *.efi $(INSTALL_DIR)

clean:
	+make -f Make.gnuefi clean
