// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

extern Device devices[NDEV];

static void consputc(int);

static int panicked = 0;

static struct 
{
	Spinlock	Lock;
	int			Locking;
} cons;

static void printInt(int xx, int base, int sign)
{
	static char digits[] = "0123456789abcdef";
	char buf[16];
	int i;
	uint32_t x;

	if (sign && (sign = xx < 0))
	{
		x = -xx;
	}
	else
	{
		x = xx;
	}
	i = 0;
	do 
	{
		buf[i++] = digits[x % base];
	} while ((x /= base) != 0);
	if (sign)
	{
		buf[i++] = '-';
	}
	while (--i >= 0)
	{
		consputc(buf[i]);
	}
}

// Print to the console. only understands %d, %x, %p, %s.

void cprintf(char *fmt, ...)
{
	int i, c, locking;
	uint32_t *argp;
	char *s;

	locking = cons.Locking;
	if (locking)
	{
		spinlockAcquire(&cons.Lock);
	}
	if (fmt == 0)
	{
		panic("null fmt");
	}
	argp = (uint32_t*)(void*)(&fmt + 1);
	for (i = 0; (c = fmt[i] & 0xff) != 0; i++) 
	{
		if (c != '%') 
		{
			consputc(c);
			continue;
		}
		c = fmt[++i] & 0xff;
		if (c == 0)
		{
			break;
		}
		switch (c) 
		{
			case 'd':
				printInt(*argp++, 10, 1);
				break;
			case 'x':
			case 'p':
				printInt(*argp++, 16, 0);
				break;
			case 's':
				if ((s = (char*)*argp++) == 0)
				{
					s = "(null)";
				}
				for (; *s; s++)
				{
					consputc(*s);
				}
				break;
			case '%':
				consputc('%');
				break;
			default:
				// Print unknown % sequence to draw attention.
				consputc('%');
				consputc(c);
				break;
		}
	}

	if (locking)
	{
		spinlockRelease(&cons.Lock);
	}
}

void panic(char *s)
{
	int i;
	uint32_t pcs[10];

	disableInterrupts();
	cons.Locking = 0;
	// use lapiccpunum so that we can call panic from myCpu()
	cprintf("localApicId %d: panic: ", localApicId());
	cprintf(s);
	cprintf("\n");
	getProcessCallStack(&s, pcs);
	for (i = 0; i < 10; i++)
	{
		cprintf(" %p", pcs[i]);
	}
	panicked = 1; // freeze other CPU
	for (;;)
		;
}

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static uint16_t *crt = (uint16_t*)P2V(0xb8000);  // CGA memory

static void cgaputc(int c)
{
	int pos;

	// Cursor position: col + 80*row.
	outputByteToPort(CRTPORT, 14);
	pos = inputByteFromPort(CRTPORT + 1) << 8;
	outputByteToPort(CRTPORT, 15);
	pos |= inputByteFromPort(CRTPORT + 1);

	if (c == '\n')
	{
		pos += 80 - pos % 80;
	}
	else if (c == BACKSPACE) 
	{
		if (pos > 0) --pos;
	}
	else
	{
		crt[pos++] = (c & 0xff) | 0x0700;  // black on white
	}
	if (pos < 0 || pos > 25 * 80)
	{
		panic("pos under/overflow");
	}

	if ((pos / 80) >= 24) 
	{  // Scroll up.
		memmove(crt, crt + 80, sizeof(crt[0]) * 23 * 80);
		pos -= 80;
		memset(crt + pos, 0, sizeof(crt[0])*(24 * 80 - pos));
	}

	outputByteToPort(CRTPORT, 14);
	outputByteToPort(CRTPORT + 1, pos >> 8);
	outputByteToPort(CRTPORT, 15);
	outputByteToPort(CRTPORT + 1, pos);
	crt[pos] = ' ' | 0x0700;
}

void consputc(int c)
{
	if (panicked) 
	{
		disableInterrupts();
		for (;;)
			;
	}

	if (c == BACKSPACE) 
	{
		uartputc('\b'); 
		uartputc(' '); 
		uartputc('\b');
	}
	else
	{
		uartputc(c);
	}
	cgaputc(c);
}

#define INPUT_BUF 128
struct 
{
	char		buf[INPUT_BUF];
	uint32_t	r;  // Read index
	uint32_t	w;  // Write index
	uint32_t	e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

void consoleInterrupt(int(*getc)(void))
{
	int c, doprocdump = 0;

	spinlockAcquire(&cons.Lock);
	while ((c = getc()) >= 0) 
	{
		switch (c) 
		{
			case C('P'):  // Process listing.
				// processDump() locks cons.Lock indirectly; invoke later
				doprocdump = 1;
				break;
			case C('U'):  // Kill line.
				while (input.e != input.w && input.buf[(input.e - 1) % INPUT_BUF] != '\n') 
				{
					input.e--;
					consputc(BACKSPACE);
				}
				break;
			case C('H'): 
			case '\x7f':  // Backspace
				if (input.e != input.w) 
				{
					input.e--;
					consputc(BACKSPACE);
				}
				break;
			default:
				if (c != 0 && input.e - input.r < INPUT_BUF) 
				{
					c = (c == '\r') ? '\n' : c;
					input.buf[input.e++ % INPUT_BUF] = c;
					consputc(c);
					if (c == '\n' || c == C('D') || input.e == input.r + INPUT_BUF) 
					{
						input.w = input.e;
						wakeup(&input.r);
					}
				}
				break;
		}
	}
	spinlockRelease(&cons.Lock);
	if (doprocdump) 
	{
		processDump();  // now call processDump() wo. cons.Lock held
	}
}

int consoleRead(File * f, char *dst, int n)
{
	uint32_t target;
	int c;

	target = n;
	spinlockAcquire(&cons.Lock);
	while (n > 0) 
	{
		while (input.r == input.w) 
		{
			if (myProcess()->IsKilled) 
			{
				spinlockRelease(&cons.Lock);
				//ilock(ip);
				return -1;
			}
			sleep(&input.r, &cons.Lock);
		}
		c = input.buf[input.r++ % INPUT_BUF];
		if (c == C('D')) 
		{   // EOF
			if (n < target) 
			{
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
			}
			break;
		}
		*dst++ = c;
		--n;
		if (c == '\n')
		{
			break;
		}
	}
	spinlockRelease(&cons.Lock);

	return target - n;
}

int consoleWrite(File *f, char *buf, int n)
{
	int i;

	spinlockAcquire(&cons.Lock);
	for (i = 0; i < n; i++)
	{
		consputc(buf[i] & 0xff);
	}
	spinlockRelease(&cons.Lock);
	return n;
}

void consoleInitialise(void)
{
	spinlockInitialise(&cons.Lock, "console");

	devices[CONSOLE].write = consoleWrite;
	devices[CONSOLE].read = consoleRead;
	cons.Locking = 1;

	ioApicEnable(IRQ_KBD, 0);
}

