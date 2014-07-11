SRCDIR  = grub

VPATH   = $(SRCDIR)

LOCAL_CPPFLAGS  = -I$(SRCDIR)/include -I$(SRCDIR)/grub-core/lib/minilzo

OBJS            = src/grub.o $(SRCDIR)/grub-core/kern/err.o $(SRCDIR)/grub-core/kern/list.o $(SRCDIR)/grub-core/kern/misc.o \
                  $(SRCDIR)/grub-core/lib/crc.o $(SRCDIR)/grub-core/lib/minilzo/minilzo.o
TARGET          = src/libgrub.a

all: $(TARGET)

include Make.common

clean:
	rm -f $(OBJS) $(TARGET)
