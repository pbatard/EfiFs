; The following maps memset and memcpy to grub_memset and grub_memcpy respectively
; Needed as the ARM compiler inserts intrinsic memset/memcpy's
; Oh, and this stupid assembler uses a tab to differentiate a label from an instruction...

	IMPORT grub_memset
	IMPORT grub_memmove
	EXPORT memset
	EXPORT memcpy

memset
	b grub_memset

memcpy
	b grub_memmove

	END
