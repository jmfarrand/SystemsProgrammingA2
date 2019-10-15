; Initial process execs /init.
; This code runs in user space.

BITS 32

global _initcode_start
global _initcode_end

_initcode_start:

%include "syscall.asm"

; exec(init, argv)

start:
	push	dword argv - _initcode_start
	push	dword init - _initcode_start
	push	dword 0
	mov		eax, SYS_exec
	int		64

; for(;;) exit();
exit:
	mov		eax, SYS_exit
	int		64
	jmp		exit

init	db '/usrbin/init.exe', 0

ALIGN 2
argv	dd init - _initcode_start
		dd 0
_initcode_end: