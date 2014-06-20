SRCDIR = grub

VPATH = $(SRCDIR)

LOCAL_CPPFLAGS  = -DGRUB_MACHINE_EFI -DGRUB_KERNEL -DGRUB_FILE=\"$(subst $(srcdir)/,,$<)\" -I$(SRCDIR) -I$(SRCDIR)/include -I$(SRCDIR)

OBJS            = $(SRCDIR)/grub-core/kern/err.o $(SRCDIR)/grub-core/kern/list.o $(SRCDIR)/grub-core/kern/misc.o $(SRCDIR)/grub-core/fs/fshelp.o $(SRCDIR)/grub-core/fs/ntfs.o src/grubberize.o
TARGET          = src/libgrub_efi.a

all: $(TARGET)

include Make.common

clean:
	rm -f $(OBJS) $(TARGET)
