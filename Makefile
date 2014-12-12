GRUBDIR   = grub
GNUEFIDIR = gnu-efi

LOCAL_CPPFLAGS  = -I$(GRUBDIR) -I$(GRUBDIR)/include -I$(GRUBDIR)/grub-core/lib/minilzo

OBJS            = src/grub.o $(GRUBDIR)/grub-core/kern/err.o $(GRUBDIR)/grub-core/kern/list.o $(GRUBDIR)/grub-core/kern/misc.o \
                  $(GRUBDIR)/grub-core/lib/crc.o $(GRUBDIR)/grub-core/lib/minilzo/minilzo.o
TARGET          = src/libgrub.a

all: $(GNUEFIDIR)/lib/libefi.a $(GRUBDIR)/config.h $(GRUBDIR)/include/grub/cpu $(TARGET)

$(GNUEFIDIR)/lib/libefi.a:
	cd $(GNUEFIDIR) && make

$(GRUBDIR)/include/grub/cpu:
	cd $(GRUBDIR)/include/grub && ln -s x86_64 cpu

$(GRUBDIR)/config.h:
	@echo $@
	@echo "/*" > $(GRUBDIR)/config.h
	@echo " * minimal GRUB config.h for gcc compilation" >> $(GRUBDIR)/config.h
	@echo " */" >> $(GRUBDIR)/config.h
	@echo "#define _LARGEFILE_SOURCE" >> $(GRUBDIR)/config.h
	@echo "#define _FILE_OFFSET_BITS 64" >> $(GRUBDIR)/config.h
	@echo "#define PACKAGE_STRING \"GRUB 2.02~beta2\"" >> $(GRUBDIR)/config.h
	@echo "#define GRUB_TARGET_CPU \"x86_64\"" >> $(GRUBDIR)/config.h
	@echo "#define GRUB_PLATFORM \"efi\"" >> $(GRUBDIR)/config.h
	@echo "#define GRUB_MACHINE_EFI" >> $(GRUBDIR)/config.h
	@echo "#define GRUB_KERNEL" >> $(GRUBDIR)/config.h
	@echo "" >> $(GRUBDIR)/config.h
	@echo "// GNU extensions" >> $(GRUBDIR)/config.h
	@echo "#define _GNU_SOURCE 1" >> $(GRUBDIR)/config.h

include Make.common

clean:
	rm -f $(OBJS) $(TARGET)
