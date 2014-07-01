SRCDIR  = grub

VPATH   = $(SRCDIR)

LOCAL_CPPFLAGS  = -I$(SRCDIR)/include

OBJS            = $(SRCDIR)/grub-core/kern/err.o $(SRCDIR)/grub-core/kern/list.o $(SRCDIR)/grub-core/kern/misc.o
TARGET          = src/libgrub.a

all: $(TARGET)

include Make.common

clean:
	rm -f $(OBJS) $(TARGET)
