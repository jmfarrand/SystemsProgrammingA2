// The local APIC manages internal (non-I/O) interrupts.
// See Chapter 8 & Appendix C of Intel processor manual volume 3.

#include "param.h"
#include "types.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "traps.h"
#include "mmu.h"
#include "x86.h"

// Local APIC registers, divided by 4 for use as uint32_t[] indices.
#define ID			(0x0020/4)   // ID
#define VER			(0x0030/4)   // Version
#define TPR			(0x0080/4)   // Task Priority
#define EOI			(0x00B0/4)   // EOI
#define SVR			(0x00F0/4)   // Spurious Interrupt Vector
#define ENABLE		0x00000100   // Unit Enable
#define ESR			(0x0280/4)   // Error Status
#define ICRLO		(0x0300/4)   // Interrupt Command
#define INIT		0x00000500   // INIT/RESET
#define STARTUP		0x00000600   // Startup IPI
#define DELIVS		0x00001000   // Delivery status
#define ASSERT		0x00004000   // Assert interrupt (vs deassert)
#define DEASSERT	0x00000000
#define LEVEL		0x00008000   // Level triggered
#define BCAST		0x00080000   // Send to all APICs, including self.
#define BUSY		0x00001000
#define FIXED		0x00000000
#define ICRHI		(0x0310/4)   // Interrupt Command [63:32]
#define TIMER		(0x0320/4)   // Local Vector Table 0 (TIMER)
#define X1			0x0000000B   // divide counts by 1
#define PERIODIC	0x00020000   // Periodic
#define PCINT		(0x0340/4)   // Performance Counter LVT
#define LINT0		(0x0350/4)   // Local Vector Table 1 (LINT0)
#define LINT1		(0x0360/4)   // Local Vector Table 2 (LINT1)
#define ERROR		(0x0370/4)   // Local Vector Table 3 (ERROR)
#define MASKED		0x00010000   // Interrupt masked
#define TICR		(0x0380/4)   // Timer Initial Count
#define TCCR		(0x0390/4)   // Timer Current Count
#define TDCR		(0x03E0/4)   // Timer Divide Configuration

volatile uint32_t *localApic;  // Initialized in mp.c

static void localApicWrite(int index, int value)
{
	localApic[index] = value;
	localApic[ID];  // wait for write to finish, by reading
}

void localApicInitialise(void)
{
	if (!localApic)
	{
		return;
	}
	// Enable local APIC; set spurious interrupt vector.
	localApicWrite(SVR, ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

	// The timer repeatedly counts down at bus frequency
	// from localApic[TICR] and then issues an interrupt.
	// If xv6 cared more about precise timekeeping,
	// TICR would be calibrated using an external time source.
	localApicWrite(TDCR, X1);
	localApicWrite(TIMER, PERIODIC | (T_IRQ0 + IRQ_TIMER));
	localApicWrite(TICR, 10000000);

	// Disable logical interrupt lines.
	localApicWrite(LINT0, MASKED);
	localApicWrite(LINT1, MASKED);

	// Disable performance counter overflow interrupts
	// on machines that provide that interrupt entry.
	if (((localApic[VER] >> 16) & 0xFF) >= 4)
	{
		localApicWrite(PCINT, MASKED);
	}
	// Map error interrupt to IRQ_ERROR.
	localApicWrite(ERROR, T_IRQ0 + IRQ_ERROR);

	// Clear error status register (requires back-to-back writes).
	localApicWrite(ESR, 0);
	localApicWrite(ESR, 0);

	// Ack any outstanding interrupts.
	localApicWrite(EOI, 0);

	// Send an Init Level De-Assert to synchronise arbitration ID's.
	localApicWrite(ICRHI, 0);
	localApicWrite(ICRLO, BCAST | INIT | LEVEL);
	while (localApic[ICRLO] & DELIVS)
		;

	// Enable interrupts on the APIC (but not on the processor).
	localApicWrite(TPR, 0);
}

int localApicId(void)
{
	if (!localApic)
	{
		return 0;
	}
	return localApic[ID] >> 24;
}

// Acknowledge interrupt.
void localApicEndOfInterrupt(void)
{
	if (localApic)
	{
		localApicWrite(EOI, 0);
	}
}

// Spin for a given number of microseconds.
// On real hardware would want to tune this dynamically.
void microDelay(int us)
{
}

#define CMOS_PORT    0x70
#define CMOS_RETURN  0x71

// Start additional processor running entry code at addr.
// See Appendix B of MultiProcessor Specification.

void localApicStartup(uint8_t apicid, uint32_t addr)
{
	int i;
	uint16_t *wrv;

	// "The BSP must initialize CMOS shutdown code to 0AH
	// and the warm reset vector (DWORD based at 40:67) to point at
	// the AP startup code prior to the [universal startup algorithm]."
	outputByteToPort(CMOS_PORT, 0xF);  // offset 0xF is shutdown code
	outputByteToPort(CMOS_PORT + 1, 0x0A);
	wrv = (uint16_t*)P2V((0x40 << 4 | 0x67));  // Warm reset vector
	wrv[0] = 0;
	wrv[1] = addr >> 4;

	// "Universal startup algorithm."
	// Send INIT (level-triggered) interrupt to reset other CPU.
	localApicWrite(ICRHI, apicid << 24);
	localApicWrite(ICRLO, INIT | LEVEL | ASSERT);
	microDelay(200);
	localApicWrite(ICRLO, INIT | LEVEL);
	microDelay(100);    // should be 10ms, but too slow in Bochs!

	// Send startup IPI (twice!) to enter code.
	// Regular hardware is supposed to only accept a STARTUP
	// when it is in the halted state due to an INIT.  So the second
	// should be ignored, but it is part of the official Intel algorithm.
	// Bochs complains about the second one.  Too bad for Bochs.
	for (i = 0; i < 2; i++) 
	{
		localApicWrite(ICRHI, apicid << 24);
		localApicWrite(ICRLO, STARTUP | (addr >> 12));
		microDelay(200);
	}
}

#define CMOS_STATA   0x0a
#define CMOS_STATB   0x0b
#define CMOS_UIP    (1 << 7)        // RTC update in progress

#define SECS    0x00
#define MINS    0x02
#define HOURS   0x04
#define DAY     0x07
#define MONTH   0x08
#define YEAR    0x09

static uint32_t cmosRead(uint32_t reg)
{
	outputByteToPort(CMOS_PORT, reg);
	microDelay(200);

	return inputByteFromPort(CMOS_RETURN);
}

static void cmosGetRealTimeClock(RtcDate *r)
{
	r->second = cmosRead(SECS);
	r->minute = cmosRead(MINS);
	r->hour = cmosRead(HOURS);
	r->day = cmosRead(DAY);
	r->month = cmosRead(MONTH);
	r->year = cmosRead(YEAR);
}

// qemu seems to use 24-hour GWT and the values are BCD encoded
void cmosTime(RtcDate *r)
{
	RtcDate t1, t2;
	int sb, bcd;

	sb = cmosRead(CMOS_STATB);

	bcd = (sb & (1 << 2)) == 0;

	// make sure CMOS doesn't modify time while we read it
	for (;;) 
	{
		cmosGetRealTimeClock(&t1);
		if (cmosRead(CMOS_STATA) & CMOS_UIP)
		{
			continue;
		}
		cmosGetRealTimeClock(&t2);
		if (memcmp(&t1, &t2, sizeof(t1)) == 0)
		{
			break;
		}
	}

	// convert
	if (bcd) 
	{
#define    CONV(x)     (t1.x = ((t1.x >> 4) * 10) + (t1.x & 0xf))
		CONV(second);
		CONV(minute);
		CONV(hour);
		CONV(day);
		CONV(month);
		CONV(year);
#undef     CONV
	}

	*r = t1;
	r->year += 2000;
}
