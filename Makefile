include Make.common

GRUB_LIB        = $(GRUB_DIR)/libgrub.a

CFLAGS         += -DGRUB_FILE=\"$(subst $(srcdir)/,,$<)\" -DLZO_CFG_FREESTANDING 
CFLAGS         += -I$(GRUB_DIR) -I$(GRUB_DIR)/include -I$(GRUB_DIR)/grub-core/lib/minilzo
OBJS            = $(SRC_DIR)/grub.o $(GRUB_DIR)/grub-core/kern/err.o $(GRUB_DIR)/grub-core/kern/list.o \
                  $(GRUB_DIR)/grub-core/kern/misc.o $(GRUB_DIR)/grub-core/lib/crc.o \
                  $(GRUB_DIR)/grub-core/lib/minilzo/minilzo.o

-include $(OBJS:.o=.d)
.PHONY: all clean
.DEFAULT_GOAL := all

all: $(GNUEFI_LIB) $(GRUB_LIB)
	$(MAKE) -C $(SRC_DIR)

$(GRUB_DIR)/include/grub/cpu_$(CPU_ARCH):
	@rm -rf $(GRUB_DIR)/include/grub/cpu*
	@echo  [LN]  $(GRUB_DIR)/include/grub/$(CPU_ARCH) -\> $(GRUB_DIR)/include/grub/cpu
	@cd $(GRUB_DIR)/include/grub && ln -s $(CPU_ARCH) cpu && touch cpu_$(CPU_ARCH)

$(GRUB_DIR)/config.h:
	@echo  [CP]  $(notdir $@)
	@cp $(CURDIR)/config.h $(GRUB_DIR)

$(GRUB_LIB): $(GRUB_DIR)/config.h $(GRUB_DIR)/include/grub/cpu_$(CPU_ARCH) $(OBJS) 
	@echo  [AR]  $(notdir $@)
	@$(AR) sq $@ $(OBJS)

clean: clean.common
	$(MAKE) -C $(SRC_DIR) clean
	rm -rf $(GRUB_DIR)/include/grub/cpu*
	rm -f $(OBJS) $(LIB_TARGET) $(GRUB_DIR)/config.h $(GRUB_DIR)/*.a
