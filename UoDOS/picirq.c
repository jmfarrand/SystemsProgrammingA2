#include "types.h"
#include "defs.h"
#include "x86.h"
#include "traps.h"

// I/O Addresses of the two programmable interrupt controllers
#define IO_PIC1         0x20    // Master (IRQs 0-7)
#define IO_PIC2         0xA0    // Slave (IRQs 8-15)

//	Controller Registers

// PIC 1 register port addresses
#define I86_PIC1_REG_COMMAND	0x20
#define I86_PIC1_REG_STATUS		0x20
#define I86_PIC1_REG_DATA		0x21
#define I86_PIC1_REG_IMR		0x21

// PIC 2 register port addresses
#define I86_PIC2_REG_COMMAND	0xA0
#define I86_PIC2_REG_STATUS		0xA0
#define I86_PIC2_REG_DATA		0xA1
#define I86_PIC2_REG_IMR		0xA1

//	Initialisation Command Bit Masks

// Initialixation Control Word 1 bit masks
#define I86_PIC_ICW1_MASK_IC4			0x1			//00000001
#define I86_PIC_ICW1_MASK_SNGL			0x2			//00000010
#define I86_PIC_ICW1_MASK_ADI			0x4			//00000100
#define I86_PIC_ICW1_MASK_LTIM			0x8			//00001000
#define I86_PIC_ICW1_MASK_INIT			0x10		//00010000

// Initialisation Control Words 2 and 3 do not require bit masks

// Initialisation Control Word 4 bit masks
#define I86_PIC_ICW4_MASK_UPM			0x1			//00000001
#define I86_PIC_ICW4_MASK_AEOI			0x2			//00000010
#define I86_PIC_ICW4_MASK_MS			0x4			//00000100
#define I86_PIC_ICW4_MASK_BUF			0x8			//00001000
#define I86_PIC_ICW4_MASK_SFNM			0x10		//00010000

//	Initialisation Command 1 control bits

#define I86_PIC_ICW1_IC4_EXPECT				1			//1
#define I86_PIC_ICW1_IC4_NO					0			//0
#define I86_PIC_ICW1_SNGL_YES				2			//10
#define I86_PIC_ICW1_SNGL_NO				0			//00
#define I86_PIC_ICW1_ADI_CALLINTERVAL4		4			//100
#define I86_PIC_ICW1_ADI_CALLINTERVAL8		0			//000
#define I86_PIC_ICW1_LTIM_LEVELTRIGGERED	8			//1000
#define I86_PIC_ICW1_LTIM_EDGETRIGGERED		0			//0000
#define I86_PIC_ICW1_INIT_YES				0x10		//10000
#define I86_PIC_ICW1_INIT_NO				0			//00000

//	Initialisation Command 4 control bits

#define I86_PIC_ICW4_UPM_86MODE			1			//1
#define I86_PIC_ICW4_UPM_MCSMODE		0			//0
#define I86_PIC_ICW4_AEOI_AUTOEOI		2			//10
#define I86_PIC_ICW4_AEOI_NOAUTOEOI		0			//0
#define I86_PIC_ICW4_MS_BUFFERMASTER	4			//100
#define I86_PIC_ICW4_MS_BUFFERSLAVE		0			//0
#define I86_PIC_ICW4_BUF_MODEYES		8			//1000
#define I86_PIC_ICW4_BUF_MODENO			0			//0
#define I86_PIC_ICW4_SFNM_NESTEDMODE	0x10		//10000
#define I86_PIC_ICW4_SFNM_NOTNESTED		0			//a binary 2 (futurama joke hehe ;)

// Send command to PIC
void picSendCommand(uint8_t cmd, uint8_t picNum)
{
	if (picNum > 1)
	{
		return;
	}
	uint8_t	reg = (picNum == 1) ? I86_PIC2_REG_COMMAND : I86_PIC1_REG_COMMAND;
	outputByteToPort(reg, cmd);
}

// Send data to PIC
void picSendData(uint8_t data, uint8_t picNum)
{
	if (picNum > 1)
	{
		return;
	}
	uint8_t	reg = (picNum == 1) ? I86_PIC2_REG_DATA : I86_PIC1_REG_DATA;
	outputByteToPort(reg, data);
}

// Read data from PICs
uint8_t I86_PIC_ReadData(uint8_t picNum)
{
	if (picNum > 1)
	{
		return 0;
	}
	uint8_t	reg = (picNum == 1) ? I86_PIC2_REG_DATA : I86_PIC1_REG_DATA;
	return inputByteFromPort(reg);
}

// Initialise pic
void I86_PIC_Initialise(uint8_t base0, uint8_t base1)
{
	uint8_t	icw = 0;

	// Begin initialisation of PIC
	icw = (icw & ~I86_PIC_ICW1_MASK_INIT) | I86_PIC_ICW1_INIT_YES;
	icw = (icw & ~I86_PIC_ICW1_MASK_IC4) | I86_PIC_ICW1_IC4_EXPECT;
	picSendCommand(icw, 0);
	picSendCommand(icw, 1);

	// Send initialisation control word 2. This is the base addresses of the irq's
	picSendData(base0, 0);
	picSendData(base1, 1);

	// Send initialisation control word 3. This is the connection between master and slave.
	// ICW3 for master PIC is the IR that connects to secondary pic in binary format
	// ICW3 for secondary PIC is the IR that connects to master pic in decimal format
	picSendData(0x04, 0);
	picSendData(0x02, 1);

	//! Send Initialisation control word 4. 

	icw = (icw & ~I86_PIC_ICW4_MASK_UPM) | I86_PIC_ICW4_UPM_86MODE;
	picSendData(icw, 0);
	picSendData(icw, 1);
}


// Don't use the 8259A interrupt controllers.  Xv6 assumes SMP hardware.
void picInitialise(void)
{
	// Remap the interrupts to avoid spurious traps.  This is needed even though
	// we mask all interrupts
	I86_PIC_Initialise(0x60, 0x68);
	// mask all interrupts
	outputByteToPort(IO_PIC1 + 1, 0xFF);
	outputByteToPort(IO_PIC2 + 1, 0xFF);
}

