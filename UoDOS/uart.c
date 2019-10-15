// Intel 8250 serial port (UART).

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

#define COM1    0x3f8

static int uart;    // is there a uart?

void
uartinit(void)
{
	char *p;

	// Turn off the FIFO
	outputByteToPort(COM1 + 2, 0);

	// 9600 baud, 8 data bits, 1 stop bit, parity off.
	outputByteToPort(COM1 + 3, 0x80);    // Unlock divisor
	outputByteToPort(COM1 + 0, 115200 / 9600);
	outputByteToPort(COM1 + 1, 0);
	outputByteToPort(COM1 + 3, 0x03);    // Lock divisor, 8 data bits.
	outputByteToPort(COM1 + 4, 0);
	outputByteToPort(COM1 + 1, 0x01);    // Enable receive interrupts.

	// If status is 0xFF, no serial port.
	if (inputByteFromPort(COM1 + 5) == 0xFF)
		return;
	uart = 1;

	// Acknowledge pre-existing interrupt conditions;
	// enable interrupts.
	inputByteFromPort(COM1 + 2);
	inputByteFromPort(COM1 + 0);
	ioApicEnable(IRQ_COM1, 0);

	// Announce that we're here.
	for (p = "xv6...\n"; *p; p++)
		uartputc(*p);
}

void
uartputc(int c)
{
	int i;

	if (!uart)
		return;
	for (i = 0; i < 128 && !(inputByteFromPort(COM1 + 5) & 0x20); i++)
		microDelay(10);
	outputByteToPort(COM1 + 0, c);
}

static int
uartgetc(void)
{
	if (!uart)
		return -1;
	if (!(inputByteFromPort(COM1 + 5) & 0x01))
		return -1;
	return inputByteFromPort(COM1 + 0);
}

void
uartintr(void)
{
	consoleInterrupt(uartgetc);
}
