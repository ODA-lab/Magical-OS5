/*
 * Copyright (c) 2011 Joshua Cornutt <jcornutt@randomaccessit.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <io_port.h>
#include <pci.h>
#include <i825xx.h>
#include <console_io.h>
#include <memory.h>
#include <timer.h>
#include <debug.h>

typedef struct CPUContext {
 uint32_t vector;
} CPUContext;

typedef struct net_device_t {
	uint32_t tx_count;
	uint32_t rx_count;
	uint32_t packets_dropped;
	void* lldev;
	uint32_t hwid;
	uint8_t mac_address[6];
	uint16_t interrupt_line;
	struct pci_device *pci;
	void (*rx_poll)(struct net_device_t *netdev);
	void (*tx_poll)(struct net_device_t *netdev, void *pkt, uint16_t length );
	int (*rx_init)(struct net_device_t *netdev);
	int (*tx_init)(struct net_device_t *netdev);
	void (*rx_enable)(struct net_device_t *netdev);
} net_device_t;


net_device_t* find_net_device_by_irq(uint32_t int_vector)
{
	// TODO: implement
	(void)int_vector;
	KERN_ABORT("not implemented");
	return NULL;
		// net_device_t *netdev = find_net_device_by_irq( ctx->vector );
}
void AddInterruptServiceRoutine(int interrupt_line,  void (*interrupt_handler)(CPUContext *))
{
	//TODO: implement
	(void)interrupt_line;
	(void)interrupt_handler;
	KERN_ABORT("not implemented");
}
void register_global_network_device(net_device_t* netdev )
{
	//TODO: implement
	(void)netdev;
	KERN_ABORT("not implemented");
}

// MMIO operations
static uint8_t mmio_read8 (void* p_address);
static uint16_t mmio_read16 (void* p_address);
static uint32_t mmio_read32 (void* p_address);
static uint64_t mmio_read64 (void* p_address);
static void mmio_write8 (void* p_address,uint8_t p_value);
static void mmio_write16 (void* p_address,uint16_t p_value);
static void mmio_write32 (void* p_address,uint32_t p_value);
static void mmio_write64 (void* p_address,uint64_t p_value);

uint8_t mmio_read8 (void* p_address)
{
    return *((volatile uint8_t*)(p_address));
}
uint16_t mmio_read16 (void* p_address)
{
    return *((volatile uint16_t*)(p_address));
}
uint32_t mmio_read32 (void* p_address)
{
    return *((volatile uint32_t*)(p_address));
}
uint64_t mmio_read64 (void* p_address)
{
    return *((volatile uint64_t*)(p_address));
}
void mmio_write8 (void* p_address,uint8_t p_value)
{
    (*((volatile uint8_t*)(p_address)))=(p_value);
}
void mmio_write16 (void* p_address,uint16_t p_value)
{
    (*((volatile uint16_t*)(p_address)))=(p_value);
}
void mmio_write32 (void* p_address,uint32_t p_value)
{
    (*((volatile uint32_t*)(p_address)))=(p_value);
}
void mmio_write64 (void* p_address,uint64_t p_value)
{
    (*((volatile uint64_t*)(p_address)))=(p_value);
}


#define NUM_RX_DESCRIPTORS	768
#define NUM_TX_DESCRIPTORS	768

#define CTRL_FD				(1 << 0)
#define CTRL_ASDE			(1 << 5)
#define CTRL_SLU			(1 << 6)

// RX and TX descriptor structures
typedef struct __attribute__((packed)) i825xx_rx_desc_s
{
	volatile uint64_t	address;

	volatile uint16_t	length;
	volatile uint16_t	checksum;
	volatile uint8_t	status;
	volatile uint8_t	errors;
	volatile uint16_t	special;
} i825xx_rx_desc_t;

typedef struct __attribute__((packed)) i825xx_tx_desc_s
{
	volatile uint64_t	address;

	volatile uint16_t	length;
	volatile uint8_t	cso;
	volatile uint8_t	cmd;
	volatile uint8_t	sta;
	volatile uint8_t	css;
	volatile uint16_t	special;
} i825xx_tx_desc_t;

// Device-specific structure
typedef struct i825xx_device_s
{
	uintptr_t	mmio_address;
	uint32_t	io_address;

	volatile uint8_t		*rx_desc_base;
	volatile i825xx_rx_desc_t	*rx_desc[NUM_RX_DESCRIPTORS];	// receive descriptor buffer
	volatile uint16_t	rx_tail;

	volatile uint8_t		*tx_desc_base;
	volatile i825xx_tx_desc_t	*tx_desc[NUM_TX_DESCRIPTORS];	// transmit descriptor buffer
	volatile uint16_t	tx_tail;

	uint16_t	(* eeprom_read)( struct i825xx_device_s *, uint8_t );
} i825xx_device_t;

static uint16_t net_i825xx_eeprom_read( i825xx_device_t *dev, uint8_t ADDR )
{
	uint16_t DATA = 0;
	uint32_t tmp = 0;
	mmio_write32( i825xx_REG_EERD, (1) | ((uint32_t)(ADDR) << 8) );

	while( !((tmp = mmio_read32(i825xx_REG_EERD)) & (1 << 4)) )
		timer_wait_loop_usec(1);

	DATA = (uint16_t)((tmp >> 16) & 0xFFFF);
	return DATA;
}

/****************************************************
 ** Management Data Input/Output (MDI/O) Interface **
 ** "This interface allows upper-layer devices to  **
 **  monitor and control the state of the PHY."    **
 ****************************************************/
#define MDIC_PHYADD			(1 << 21)
#define MDIC_OP_WRITE		(1 << 26)
#define MDIC_OP_READ		(2 << 26)
#define MDIC_R				(1 << 28)
#define MDIC_I				(1 << 29)
#define MDIC_E				(1 << 30)

static uint16_t net_i825xx_phy_read( i825xx_device_t *dev, int MDIC_REGADD )
{
	uint16_t MDIC_DATA = 0;

	mmio_write32( i825xx_REG_MDIC, (((MDIC_REGADD & 0x1F) << 16) |
									MDIC_PHYADD | MDIC_I | MDIC_OP_READ) );

	while( !(mmio_read32(i825xx_REG_MDIC) & (MDIC_R | MDIC_E)) )
	{
		timer_wait_loop_usec(1);
	}

	if( mmio_read32(i825xx_REG_MDIC) & MDIC_E )
	{
		printk("i825xx: MDI READ ERROR\n");
		return -1;
	}

	MDIC_DATA = (uint16_t)(mmio_read32(i825xx_REG_MDIC) & 0xFFFF);
	return MDIC_DATA;
}

static void net_i825xx_phy_write( i825xx_device_t *dev, int MDIC_REGADD, uint16_t MDIC_DATA )
{
	mmio_write32( i825xx_REG_MDIC, ((MDIC_DATA & 0xFFFF) | ((MDIC_REGADD & 0x1F) << 16) |
									MDIC_PHYADD | MDIC_I | MDIC_OP_WRITE) );

	while( !(mmio_read32(i825xx_REG_MDIC) & (MDIC_R | MDIC_E)) )
	{
		timer_wait_loop_usec(1);
	}

	if( mmio_read32(i825xx_REG_MDIC) & MDIC_E )
	{
		printk("i825xx: MDI WRITE ERROR\n");
		return;
	}
}

#define RCTL_EN				(1 << 1)
#define RCTL_SBP			(1 << 2)
#define RCTL_UPE			(1 << 3)
#define RCTL_MPE			(1 << 4)
#define RCTL_LPE			(1 << 5)
#define RDMTS_HALF			(0 << 8)
#define RDMTS_QUARTER		(1 << 8)
#define RDMTS_EIGHTH		(2 << 8)
#define RCTL_BAM			(1 << 15)
#define RCTL_BSIZE_256		(3 << 16)
#define RCTL_BSIZE_512		(2 << 16)
#define RCTL_BSIZE_1024		(1 << 16)
#define RCTL_BSIZE_2048		(0 << 16)
#define RCTL_BSIZE_4096		((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192		((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384	((1 << 16) | (1 << 25))
#define RCTL_SECRC			(1 << 26)

static void net_i825xx_rx_enable( net_device_t *netdev )
{
	i825xx_device_t *dev = (i825xx_device_t *)netdev->lldev;
	mmio_write32( i825xx_REG_RCTL, mmio_read32(i825xx_REG_RCTL) | (RCTL_EN) );
}

static int net_i825xx_rx_init( net_device_t *netdev )
{
	int i;
	i825xx_device_t *dev = (i825xx_device_t *)netdev->lldev;

	// unaligned base address
	void* tmpbase = kmalloc((sizeof(i825xx_rx_desc_t) * NUM_RX_DESCRIPTORS) + 16);
	// aligned base address
	dev->rx_desc_base = ((int)tmpbase % 16) ? (uint8_t *)(((int)tmpbase) + 16 - ((int)tmpbase % 16)) : (uint8_t *)tmpbase;

	for( i = 0; i < NUM_RX_DESCRIPTORS; i++ )
	{
		dev->rx_desc[i] = (i825xx_rx_desc_t *)(dev->rx_desc_base + (i * 16));
		dev->rx_desc[i]->address = (uint64_t)(int)kmalloc(8192+16); // packet buffer size (8K)
		dev->rx_desc[i]->status = 0;
	}

	// setup the receive descriptor ring buffer (TODO: >32-bits may be broken in this code)
	mmio_write32( i825xx_REG_RDBAH, (uint32_t)( ((uint64_t)(uint32_t)dev->rx_desc_base) >> 32) );
	mmio_write32( i825xx_REG_RDBAL, (uint32_t)( ((uint64_t)(uint32_t)dev->rx_desc_base) & 0xFFFFFFFF) );
	printk("i825xx[%u]: RDBAH/RDBAL = %x:%x\n", (int)netdev->hwid, (int)mmio_read32(i825xx_REG_RDBAH), (int)mmio_read32(i825xx_REG_RDBAL));

	// receive buffer length; NUM_RX_DESCRIPTORS 16-byte descriptors
	mmio_write32( i825xx_REG_RDLEN, (uint32_t)(NUM_RX_DESCRIPTORS * 16) );

	// setup head and tail pointers
	mmio_write32( i825xx_REG_RDH, 0 );
	mmio_write32( i825xx_REG_RDT, NUM_RX_DESCRIPTORS );
	dev->rx_tail = 0;

	// set the receieve control register (promisc ON, 8K pkt size)
	mmio_write32( i825xx_REG_RCTL, (RCTL_SBP | RCTL_UPE | RCTL_MPE | RDMTS_HALF | RCTL_SECRC |
									RCTL_LPE | RCTL_BAM | RCTL_BSIZE_8192) );
	return 0;
}

#define TCTL_EN				(1 << 1)
#define TCTL_PSP			(1 << 3)

static int net_i825xx_tx_init( net_device_t *netdev )
{
	int i;
	i825xx_device_t *dev = (i825xx_device_t *)netdev->lldev;

	void* tmpbase = kmalloc((sizeof(i825xx_tx_desc_t) * NUM_TX_DESCRIPTORS) + 16);
	dev->tx_desc_base = (int)tmpbase % 16 ? ((uint8_t *)tmpbase + 16 - (int)tmpbase % 16) : (uint8_t *)tmpbase;

	for( i = 0; i < NUM_TX_DESCRIPTORS; i++ )
	{
		dev->tx_desc[i] = (i825xx_tx_desc_t *)(dev->tx_desc_base + (i * 16));
		dev->tx_desc[i]->address = 0;
		dev->tx_desc[i]->cmd = 0;
	}

	// setup the transmit descriptor ring buffer
	mmio_write32( i825xx_REG_TDBAH, (uint32_t)((uint64_t)(uint32_t)dev->tx_desc_base >> 32) );
	mmio_write32( i825xx_REG_TDBAL, (uint32_t)((uint64_t)(uint32_t)dev->tx_desc_base & 0xFFFFFFFF) );
	printk("i825xx[%u]: TDBAH/TDBAL = %x:%x\n", (int)netdev->hwid, (int)mmio_read32(i825xx_REG_TDBAH), (int)mmio_read32(i825xx_REG_TDBAL));

	// transmit buffer length; NUM_TX_DESCRIPTORS 16-byte descriptors
	mmio_write32( i825xx_REG_TDLEN, (uint32_t)(NUM_TX_DESCRIPTORS * 16) );

	// setup head and tail pointers
	mmio_write32( i825xx_REG_TDH, 0 );
	mmio_write32( i825xx_REG_TDT, NUM_TX_DESCRIPTORS );
	dev->tx_tail = 0;

	// set the transmit control register (padshortpackets)
	mmio_write32( i825xx_REG_TCTL, (TCTL_EN | TCTL_PSP) );
	return 0;
}

static void net_i825xx_tx_poll( net_device_t *netdev, void *pkt, uint16_t length )
{
	i825xx_device_t *dev = (i825xx_device_t *)netdev->lldev;
	//printk("i825xx[%u]: transmitting packet (%u bytes) [h=%u, t=%u]\n", netdev->hwid, length, mmio_read32(i825xx_REG_TDH), dev->tx_tail);

	dev->tx_desc[dev->tx_tail]->address = (uint64_t)(int)pkt;
	dev->tx_desc[dev->tx_tail]->length = length;
	dev->tx_desc[dev->tx_tail]->cmd = ((1 << 3) | (3));

	// update the tail so the hardware knows it's ready
	int oldtail = dev->tx_tail;
	dev->tx_tail = (dev->tx_tail + 1) % NUM_TX_DESCRIPTORS;
	mmio_write32(i825xx_REG_TDT, dev->tx_tail);

	while( !(dev->tx_desc[oldtail]->sta & 0xF) )
	{
		timer_wait_loop_usec(1);
	}

	netdev->tx_count++;
	//printk("i825xx[%u]: transmit status = 0x%x\n", (int)netdev->hwid, (int)(dev->tx_desc[oldtail]->sta & 0xF));

}

// This can be used stand-alone or from an interrupt handler
static void net_i825xx_rx_poll( net_device_t *netdev )
{
	i825xx_device_t *dev = (i825xx_device_t *)netdev->lldev;

	while( (dev->rx_desc[dev->rx_tail]->status & (1 << 0)) )
	{
		// raw packet and packet length (excluding CRC)
		uint8_t *pkt = (void *)(int)(dev->rx_desc[dev->rx_tail]->address);
		uint16_t pktlen = dev->rx_desc[dev->rx_tail]->length;
		bool dropflag = false;

		if( pktlen < 60 )
		{
			printk("net[%u]: short packet (%u bytes)\n", (int)netdev->hwid, (int)pktlen);
			dropflag = true;
		}

		// while not technically an error, there is no support in this driver
		if( !(dev->rx_desc[dev->rx_tail]->status & (1 << 1)) )
		{
			printk("net[%u]: no EOP set! (len=%u, 0x%x 0x%x 0x%x)\n",
				(int)netdev->hwid, (int)pktlen, (int)pkt[0], (int)pkt[1], (int)pkt[2]);
			dropflag = true;
		}

		if( dev->rx_desc[dev->rx_tail]->errors )
		{
			printk("net[%u]: rx errors (0x%x)\n", (int)netdev->hwid, (int)dev->rx_desc[dev->rx_tail]->errors);
			dropflag = true;
		}

		if( !dropflag )
		{
			// send the packet to higher layers for parsing
		}

		// update RX counts and the tail pointer
		netdev->rx_count++;
		dev->rx_desc[dev->rx_tail]->status = (uint16_t)(0);

		dev->rx_tail = (dev->rx_tail + 1) % NUM_RX_DESCRIPTORS;

		// write the tail to the device
		mmio_write32(i825xx_REG_RDT, dev->rx_tail);
	}
}

static void i825xx_interrupt_handler( CPUContext *ctx )
{
	net_device_t *netdev = find_net_device_by_irq( ctx->vector );
	i825xx_device_t *dev = (i825xx_device_t *)netdev->lldev;

	if( netdev == NULL )
	{
		printk("i825xx: UNKNOWN IRQ / INVALID DEVICE\n");
		return;
	}

	// read the pending interrupt status
	uint32_t icr = mmio_read32(dev->mmio_address + 0xC0);

	// tx success stuff
	icr &= ~(3);

	// LINK STATUS CHANGE
	if( icr & (1 << 2) )
	{
		icr &= ~(1 << 2);
		mmio_write32(i825xx_REG_CTRL, (mmio_read32(i825xx_REG_CTRL) | CTRL_SLU));

		// debugging info (probably not necessary in most scenarios)
		printk("i825xx: Link Status Change, STATUS = 0x%x\n", (int)mmio_read32(i825xx_REG_STATUS));
		printk("i825xx: PHY CONTROL = 0x%x\n", (int)net_i825xx_phy_read(dev, i825xx_PHYREG_PCTRL));
		printk("i825xx: PHY STATUS = 0x%x\n", (int)net_i825xx_phy_read(dev, i825xx_PHYREG_PSTATUS));
		printk("i825xx: PHY PSSTAT = 0x%x\n", (int)net_i825xx_phy_read(dev, i825xx_PHYREG_PSSTAT));
		printk("i825xx: PHY ANA = 0x%x\n", (int)net_i825xx_phy_read(dev, 4));
		printk("i825xx: PHY ANE = 0x%x\n", (int)net_i825xx_phy_read(dev, 6));
		printk("i825xx: PHY GCON = 0x%x\n", (int)net_i825xx_phy_read(dev, 9));
		printk("i825xx: PHY GSTATUS = 0x%x\n", (int)net_i825xx_phy_read(dev, 10));
		printk("i825xx: PHY EPSTATUS = 0x%x\n", (int)net_i825xx_phy_read(dev, 15));
	}

	// RX underrun / min threshold
	if( icr & (1 << 6) || icr & (1 << 4) )
	{
		icr &= ~((1 << 6) | (1 << 4));
		printk(" underrun (rx_head = %u, rx_tail = %u)\n", (int)mmio_read32(i825xx_REG_RDH), (int)dev->rx_tail);

		volatile int i;
		for(i = 0; i < NUM_RX_DESCRIPTORS; i++)
		{
			if( dev->rx_desc[i]->status )
				printk(" pending descriptor (i=%u, status=0x%x)\n", i, (int)dev->rx_desc[i]->status );
		}

		netdev->rx_poll(netdev);
	}

	// packet is pending
	if( icr & (1 << 7) )
	{
		icr &= ~(1 << 7);
		netdev->rx_poll(netdev);
	}

	if( icr )
		printk("i825xx[%u]: unhandled interrupt #%u received! (0x%x)\n", (int)netdev->hwid, (int)ctx->vector, (int)icr);

	// clearing the pending interrupts
	mmio_read32(dev->mmio_address + 0xC0);
}

/***************************************************
 ** Intel 825xx-series Chipset Driver Entry Point **
 ***************************************************/
int net_i825xx_init( struct pci_device *pcidev )
{
	net_device_t *netdev = (net_device_t *)kmalloc(sizeof(net_device_t));
	i825xx_device_t *dev = (i825xx_device_t *)kmalloc(sizeof(i825xx_device_t));

	netdev->pci = pcidev;			// pci structure
	netdev->lldev = (void *)dev;	// i825xx-specific structure
	netdev->rx_count = netdev->tx_count = 0;
	netdev->packets_dropped = 0;

	// read the PCI BAR for the device's MMIO space
	pci_bar *bar = pci_resolve_bar(pcidev, 0);

	// is this BAR valid?
	if( bar == NULL )
		return -1;

	// the spec says BAR0 is MMIO
	if( bar->type != PCI_BAR_MMIO )
		return -1;

	// MMIO should not have this limitation for this chipset
	if( bar->locate_below_1meg )
		return -1;

	// TODO (REMOVE) : DIAGNOSTIC INFO
	printk("BAR0:\n");
	printk(" type: %s\n", bar->type ? "I/O" : "Memory Mapped I/O");
	printk(" prefetchable: %s\n", bar->prefetchable ? "true" : "false");
	printk(" is64bit: %s\n", bar->is64bit ? "true" : "false");
	printk(" locate below 1MB: %s\n", bar->locate_below_1meg ? "true" : "false");
	printk(" address: 0x%x\n", (int)bar->address);

	// register the MMIO address
	dev->mmio_address = (uintptr_t)bar->address;

	// setup the function pointers
	netdev->rx_init = net_i825xx_rx_init;
	netdev->tx_init = net_i825xx_tx_init;
	dev->eeprom_read = net_i825xx_eeprom_read;
	netdev->rx_enable = net_i825xx_rx_enable;
	netdev->rx_poll = net_i825xx_rx_poll;
	netdev->tx_poll = net_i825xx_tx_poll;

	// register the interrupt handler
	netdev->interrupt_line = netdev->pci->interrupt_line;
	AddInterruptServiceRoutine(netdev->interrupt_line, i825xx_interrupt_handler);

	// globally register the devices
	register_global_network_device( netdev );
	printk("i825xx[%u]: interrupt line = %u (isr%u)\n", (int)netdev->hwid, (int)netdev->interrupt_line, 32+netdev->interrupt_line);

	// get the MAC address
	uint16_t mac16 = dev->eeprom_read(dev, 0);
	netdev->mac_address[0] = (mac16 & 0xFF);
	netdev->mac_address[1] = (mac16 >> 8) & 0xFF;
	mac16 = dev->eeprom_read(dev, 1);
	netdev->mac_address[2] = (mac16 & 0xFF);
	netdev->mac_address[3] = (mac16 >> 8) & 0xFF;
	mac16 = dev->eeprom_read(dev, 2);
	netdev->mac_address[4] = (mac16 & 0xFF);
	netdev->mac_address[5] = (mac16 >> 8) & 0xFF;

	// set the LINK UP
	mmio_write32(i825xx_REG_CTRL, (mmio_read32(i825xx_REG_CTRL) | CTRL_SLU));

	// Initialize the Multicase Table Array
	printk("i825xx[%u]: Initializing the Multicast Table Array...\n", (int)netdev->hwid);
	int i;
	for( i = 0; i < 128; i++ )
		mmio_write32(i825xx_REG_MTA + (i * 4), 0);

	// enable all interrupts (and clear existing pending ones)
	mmio_write32( i825xx_REG_IMS, 0x1F6DC );
	mmio_read32(dev->mmio_address + 0xC0);

	// start the RX/TX processes
	netdev->rx_init(netdev);
	netdev->tx_init(netdev);

	printk("i825xx[%u]: configuration complete\n", (int)netdev->hwid);

	return 0;
}
