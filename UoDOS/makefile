# Create the boot loader binaries

.DEFAULT_GOAL:=all

.SUFFIXES: .img .bin .asm .sys .o .lib
.PRECIOUS: %.o

IMAGE=uodos

CC = gcc
CFLAGS= -ffreestanding -m32 -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -Werror -fno-omit-frame-pointer -fno-stack-protector
OBJS= kernel_main.o proc.o spinlock.o sleeplock.o string.o console.o mp.o kalloc.o bio.o vm.o lapic.o uart.o file.o ide.o pipe.o ioapic.o trap.o kbd.o syscall.o sysproc.o sysfile.o exec.o picirq.o fs.o
ULIBOBJS = ulib.o usys.o printf.o umalloc.o
USERPROGS = init.exe sh.exe echo.exe
HEADERS = bpb.h buf.h date.h defs.h fcntl.h file.h fs.h kbd.h memlayout.h mp.h param.h pe.h proc.h sleeplock.h spinlock.h stat.h traps.h types.h user.h x86.h 

syscall.h: syscalls.pl
	perl syscalls.pl -h > syscall.h

syscalltable.h: syscalls.pl
	perl syscalls.pl -c > syscalltable.h
	
syscall.asm: syscalls.pl	
	perl syscalls.pl -i > syscall.asm

%.o: %.c syscall.h syscalltable.h $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

%.bin: %.asm
	nasm -w+all -f bin -o $@ $<

boot.bin: boot.asm functions_16.asm bpb.asm

boot2.bin: boot2.asm functions_16.asm a20.asm messages.asm fat12.asm floppy16.asm bpb.asm gdt.asm paging.asm

kernelentry.o: kernelentry.asm
	nasm -w+all -f elf -o kernelentry.o kernelentry.asm

initcode.o: initcode.asm syscall.asm
	nasm -w+all -f elf -o initcode.o initcode.asm

swtch.o: swtch.asm
	nasm -w+all -f elf -o swtch.o swtch.asm

trapasm.o: trapasm.asm
	nasm -w+all -f elf -o trapasm.o trapasm.asm

vectors.o: vectors.pl
	perl vectors.pl > vectors.asm
	nasm -w+all -f elf -o vectors.o vectors.asm

usys.o: syscalls.pl syscall.asm
	perl syscalls.pl -a > usys.asm
	nasm -w+all -f elf -o usys.o usys.asm

%.exe: %.o $(ULIBOBJS)
	$(LD) -e _main -mi386pe -Ttext 0 --image-base 0 -o $@ $^ 

kernel.sys: kernelentry.o $(OBJS) initcode.o swtch.o trapasm.o vectors.o kernel.ld 
	ld -o kernel.bin -T kernel.ld --verbose -mi386pe kernelentry.o $(OBJS) initcode.o swtch.o trapasm.o vectors.o
	objcopy -O binary kernel.bin kernel.sys

$(IMAGE).img: boot.bin boot2.bin kernel.sys $(USERPROGS)
#	Get the blank disk image
	cp diskimage/uodos.img $(IMAGE).img
#	Copy the boot sector over to the disk image
	dd status=noxfer conv=notrunc if=boot.bin of=$(IMAGE).img
#   Copy our second stage of the boot to the disk image
	dd status=noxfer conv=notrunc seek=1 if=boot2.bin of=$(IMAGE).img
#	Mount hard image file as Z:
	imdisk -a -t file -f $(IMAGE).img -o rem -m z:
#	Now copy files to z: (we do it this way to avoid problems with cygwin and drive specifiers)
	cmd /c "copy kernel.sys z:KERNEL.SYS"
	cmd /c "mkdir z:\usrbin"
#	Copy all of the user programs to z:\usrbin
	$(foreach var,$(USERPROGS), cmd /c "copy $(var) z:\usrbin\$(var)";)	
	ls /cygdrive/z
#	Unmount the hard disk image
	imdisk -D -m z:
	rm -f $(IMAGE).img.lock
	
all: $(IMAGE).img

clean:
	rm -f boot.bin
	rm -f boot2.bin
	rm -f *.exe
	rm -f *.o
	rm -f *.d
	rm -f vectors.asm
	rm -f usys.asm
	rm -f syscall.asm
	rm -f syscall.h
	rm -f syscalltable.h
	rm -f kernel.bin
	rm -f kernel.sys
	rm -f $(IMAGE).img
	rm -f $(IMAGE).img.lock
	
	