DRIVER_MAKEFILE = $(CURDIR)/../Make.common
FILESYSTEMS     = affs bfs btrfs exfat ext2 hfs hfsplus iso9660 jfs nilfs2 ntfs reiserfs sfs udf ufs2 xfs zfs

.PHONY: all clean

all: $(FILESYSTEMS)

affs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs -f $(DRIVER_MAKEFILE) driver

bfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs -f $(DRIVER_MAKEFILE) driver

btrfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs EXTRAMODULE=gzio EXTRAMODULEDIR=io -f $(DRIVER_MAKEFILE) driver

ext2:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs -f $(DRIVER_MAKEFILE) driver

exfat:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs -f $(DRIVER_MAKEFILE) driver

hfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs -f $(DRIVER_MAKEFILE) driver

hfsplus:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs EXTRAMODULE=$@comp EXTRAMODULEDIR=fs EXTRAMODULE2=gzio EXTRAMODULE2DIR=io -f $(DRIVER_MAKEFILE) driver

iso9660:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs -f $(DRIVER_MAKEFILE) driver

jfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs -f $(DRIVER_MAKEFILE) driver

nilfs2:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs -f $(DRIVER_MAKEFILE) driver

ntfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs EXTRAMODULE=$@comp EXTRAMODULEDIR=fs -f $(DRIVER_MAKEFILE) driver

reiserfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs -f $(DRIVER_MAKEFILE) driver

sfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs -f $(DRIVER_MAKEFILE) driver

udf:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs -f $(DRIVER_MAKEFILE) driver

ufs2:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs -f $(DRIVER_MAKEFILE) driver

xfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs -f $(DRIVER_MAKEFILE) driver

zfs:
	@rm -f this.o
	+$(MAKE) DRIVERNAME=$@ FSDIR=fs/zfs EXTRAMODULE=gzio EXTRAMODULEDIR=io EXTRAOBJS="zfs_fletcher.o zfs_lz4.o zfs_lzjb.o zfs_sha256.o" -f $(DRIVER_MAKEFILE) driver

clean:
	+$(MAKE) -f $(DRIVER_MAKEFILE) clean.driver
