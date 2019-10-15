bits 32

extern _trap

global _alltraps
_alltraps:
	;  Build trap frame.
	push	ds
	push	es
	push	fs
	push	gs
	pushad
  
	; Set up data segments.
	mov		ax, 10h		; SEG_KDATA << 3
	mov		ds, ax
	mov		es, ax

	; Call trap(tf), where tf=%esp
	push	esp
	call	_trap
	add		esp, 4

	; Return falls through to trapret...
global _trapret
_trapret:
	popad
	pop		gs
	pop		fs
	pop		es
	pop		ds
	add		esp, 8		; Move past trapno and errcode
	iret
