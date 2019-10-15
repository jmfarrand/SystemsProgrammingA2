; Context switch
;
;   void swtch(struct context **old, struct context *new);
; 
; Save the current registers on the stack, creating
; a struct context, and save its address in *old.
; Switch stacks to new and pop previously-saved registers.

bits 32
global _swtch
_swtch:
	mov		eax, dword [esp + 4]
	mov		edx, dword [esp + 8]

;   Save old callee-save registers
	push	ebp
	push	ebx
	push	esi
	push	edi

;	Switch stacks
	mov		dword [eax], esp
	mov		esp, edx

;   Load new callee-save registers
	pop		edi
	pop		esi
	pop		ebx
	pop		ebp
	ret
