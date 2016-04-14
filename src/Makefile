TARGET = x64

ifeq ($(TARGET),x64)
	ARCH          = x86_64
	CROSS_COMPILE = x86_64-w64-mingw32-
	LIB_ARCH      = x86_64
	CFLAGS        = -m64 -mno-red-zone
else ifeq ($(TARGET),ia32)
	ARCH          = ia32
	CROSS_COMPILE = i686-w64-mingw32-
	LIB_ARCH      = i386
	CFLAGS        = -m32 -mno-red-zone
else ifeq ($(TARGET),arm)
	ARCH          = arm
	CROSS_COMPILE = arm-linux-gnueabihf-
	LIB_ARCH      = arm
	CFLAGS        = -marm -fpic -fshort-wchar -nostdlib
endif

# MinGW on Windows doesn't need a prefix
ifneq ($(SYSTEMROOT),)
  CROSS_COMPILE =
endif

#SRC_PATH        = $(CURDIR)
#LIB_TARGET      = $(SRC_PATH)/libgrub.a

INSTALL_DIR     = /boot/efi/EFI/refind/drivers_$(ARCH)
FILESYSTEMS     = affs bfs btrfs exfat ext2 hfs hfsplus iso9660 jfs nilfs2 ntfs reiserfs sfs udf ufs2 xfs zfs
HOSTARCH        = $(shell uname -m | sed s,i[3456789]86,ia32,)

all: $(FILESYSTEMS)

affs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs TARGET=$(TARGET) -f Make.driver

bfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs TARGET=$(TARGET) -f Make.driver

btrfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs EXTRAMODULE=gzio EXTRAMODULEDIR=io TARGET=$(TARGET) -f Make.driver

ext2:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs TARGET=$(TARGET) -f Make.driver

exfat:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs TARGET=$(TARGET) -f Make.driver

hfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs TARGET=$(TARGET) -f Make.driver

hfsplus:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs EXTRAMODULE=$@comp EXTRAMODULEDIR=fs EXTRAMODULE2=gzio EXTRAMODULE2DIR=io TARGET=$(TARGET) -f Make.driver

iso9660:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs TARGET=$(TARGET) -f Make.driver

jfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs TARGET=$(TARGET) -f Make.driver

nilfs2:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs TARGET=$(TARGET) -f Make.driver

ntfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs EXTRAMODULE=$@comp EXTRAMODULEDIR=fs TARGET=$(TARGET) -f Make.driver

reiserfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs TARGET=$(TARGET) -f Make.driver

sfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs TARGET=$(TARGET) -f Make.driver

udf:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs TARGET=$(TARGET) -f Make.driver

ufs2:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs TARGET=$(TARGET) -f Make.driver

xfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs TARGET=$(TARGET) -f Make.driver

zfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs/zfs EXTRAMODULE=gzio EXTRAMODULEDIR=io EXTRAOBJS="zfs_fletcher.o zfs_lz4.o zfs_lzjb.o zfs_sha256.o" TARGET=$(TARGET) -f Make.driver

install: all
	@mkdir -p $(INSTALL_DIR)
	@cp -v $(FILESYSTEMS:%=%_$(TARGET).efi) $(INSTALL_DIR)

clean:
	+$(MAKE) -f Make.driver clean
