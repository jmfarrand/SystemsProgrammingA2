; Entry point to the C kernel.  This is prepended to the start of 
; the kernel by the linker and forms a 'known' entry point that we
; jump to from the boot assembler.

bits 32

extern _main
	; Setup stack for kernel
	mov		esp, stack + 4096
	call	_main
	
	; We should never get back here
	hlt

section .bss
stack:	resb 4096



