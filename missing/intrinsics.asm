; The following maps memset and memcpy to grub_memset and grub_memcpy respectively
; Needed as the x86 compiler inserts intrinsic memset/memcpy's

IFNDEF AMD64
  .model flat,stdcall
ENDIF

extern grub_memset:near, grub_memmove:near
public memset, memcpy

.CODE
memset:
	jmp grub_memset

memcpy:
	jmp grub_memmove
END
