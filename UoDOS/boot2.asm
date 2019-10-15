; Second stage of the boot loader

BITS 16

ORG 9000h
	jmp 	Second_Stage

%include "bpb.asm"						; A copy of the BIOS Parameter Block (i.e. information about the disk format)
%include "floppy16.asm"					; Routines to access the floppy disk drive
%include "fat12.asm"					; Routines to handle the FAT12 file system
%include "functions_16.asm"
%include "a20.asm"

; 32-bit virtual memory address of where the kernel will be relocated to in protected mode
%define KERNEL_PMODE_BASE 		80100000h

; This is the real mode address where we will initially load the kernel
%define	KERNEL_RMODE_SEG		1000h
%define KERNEL_RMODE_OFFSET		0000h

; This is the above address once we are in 32-bit mode
%define KERNEL_PMODE_LOAD_ADDR 	10000h

; Kernel name (Must be a 8.3 filename and must be 11 bytes exactly)
ImageName     db "KERNEL  SYS"

; This is where we will store the size of the kernel image in sectors (updated just before jump to kernel to be kernel size in bytes)
KernelSize    dd 0

; Used to store the number of the boot device

boot_device	  db  0				

;	Start of the second stage of the boot loader
	
Second_Stage:
    mov		[boot_device], dl		; Boot device number is passed in from first stage in DL. Save it to pass to kernel later.

    mov 	si, second_stage_msg	; Output our greeting message
    call 	Console_WriteLine_16

	call	Enable_A20
	
	push 	dx						; Save the number containing the mechanism used to enable A20
	mov		si, dx					; Display the appropriate message that indicates how the A20 line was enabled
	add		si, dx
	mov		si, [si + a20_message_list]
	call	Console_WriteLine_16
	pop		dx						; Retrieve the number
	cmp		dx, 0					; If we were unable to enable the A20 line, we cannot continue the boot
	je		Cannot_Continue

; 	We are now going to load the kernel into memory
	
;	First, load the root directory table
	call	LoadRootDirectory

;	Load kernel.sys.  We have to load it into conventional memory since the BIOS routines
;   can only access conventional memory.  We will relocate it to high memory later
  
	mov		ebx, KERNEL_RMODE_SEG	; BX:BP points to memory address to load the file to
    mov		bp, KERNEL_RMODE_OFFSET
	mov		si, ImageName			; The file to load
	call	LoadFile				; Load the file

	mov		dword [KernelSize], ecx	; Save size of kernel (in sectors)
	cmp		ax, 0					; Test for successful load
	je		Switch_To_Protected_Mode

	mov		si, msgFailure			; Unable to load kernel.sys - print error
	call	Console_Write_16
	
Cannot_Continue:	
	mov		si, wait_for_key_msg
	call	Console_WriteLine_16
	mov		ah, 0
	int     16h                    	; Wait for key press before continuing
	int     19h                     ; Warm boot computer
	hlt

; 	We are now ready to switch to 32-bit protected mode

Switch_To_Protected_Mode:
	lgdt 	[gdt_descriptor]		; Load the global descriptor table
	mov		eax, cr0				; Set the first bit of control register CR0 to switch 
									; into 32-bit protected mode
	or		eax, 1
	mov		cr0, eax
	
	jmp		CODE_SEG:Initialise_PM	; Do a far jump to the 32-bit code.  Doing a far jump 
									;clears the pipeline of any 16-bit instructions.
	
; 32-bit code
									
BITS 32

Initialise_PM:						; Start of main 32-bit code
	
;   It is now vital that we make sure that interrupts are turned off until
;   our kernel is running and has setup the interrupt tables.
;   Now we are in protected mode, we have no way of handling interrupts
;   until those tables are setup, so any interrupt would cause a crash.

	cli								
	mov		ax, DATA_SEG			; Update the segment registers to point to our new data segment
	mov		ds, ax
	mov 	ss, ax
	mov		es, ax
	mov		fs, ax
	mov		gs, ax
	
	mov		ebp, 90000h				; Update our stack position so that it is at the top of free space
	mov		esp, ebp	
	
;   Enable paging
	
	call 	EnablePaging
	
;	Copy kernel from load address to where it expects to be	
	
  	mov		eax, dword [KernelSize]	; Calculate how many bytes we need to copy
  	movzx	ebx, word [bpbBytesPerSector]
  	mul		ebx
  	mov		ebx, 4					; Now divide by 4 to calculate the number of dwords
  	div		ebx
   	cld								; Make sure direction flag is clear (i.e. ESI and EDI get incremented)
   	mov    	esi, KERNEL_PMODE_LOAD_ADDR	; ESI = Where we are copying from
   	mov		edi, KERNEL_PMODE_BASE	; EDI = Where we are copying to
	mov		ecx, eax				; ECX = Number of dwords to copy
   	rep	movsd                   	; Copy kernel to its protected mode address

	; Now we can execute our kernel

	call	KERNEL_PMODE_BASE        
	
	
%include "messages.asm"
%include "gdt.asm"
%include "paging.asm"

	times 3584-($-$$) db 0	