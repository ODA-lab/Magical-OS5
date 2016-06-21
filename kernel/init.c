#include <stddef.h>
#include <cpu.h>
#include <intr.h>
#include <excp.h>
#include <memory.h>
#include <console_io.h>
#include <timer.h>
#include <kernel.h>
#include <syscall.h>
#include <task.h>
#include <fs.h>
#include <sched.h>
#include <kern_task.h>
#include <shell_init.h>
#include <uptime_init.h>
#include <list.h>
#include <queue.h>
#include <common.h>

#include <io_port.h>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int  uint32_t;


void ata_reset(unsigned short dcr)
{
    /* do a singletasking PIO ata 'software reset' with DCR in dx */
    __asm__(
	"srst_ata_st:\n"
	"	push %%eax\n"
	"	movb $4, %%al\n"
	"	outb %%al, %%dx\n"			/* do a 'software reset' on the bus */
	"	xorl %%eax, %%eax\n"
	"	outb %%al, %%dx\n"			/* reset the bus to normal operation */
	"	inb %%dx, %%al\n"			/* it might take 4 tries for status bits to reset */
	"	inb %%dx, %%al\n"			/* ie. do a 400ns delay */
	"	inb %%dx, %%al\n"
	"	inb %%dx, %%al\n"
	".rdylp:\n"
	"	inb %%dx, %%al\n"
	"	andb $0xc0, %%al\n"			/* check BSY and RDY */
	"	cmpb $0x40, %%al\n"			/* want BSY clear and RDY set */
	"	jne .rdylp\n"
	"	pop %%eax\n"
        : : "d" (dcr) : "memory"
    );
}


int ata_read_pio48(unsigned int lba_relative, unsigned int lba_base, unsigned int sec_read, unsigned char* ptr_dest, unsigned short ioport_base, unsigned char flags_masterslave)
{
	unsigned int result = 0;
/*
;ATA PI0 33bit singletasking disk read function (up to 64K sectors, using 48bit mode)
; inputs: bx = sectors to read (0 means 64K sectors), edi -> destination buffer
; esi -> driverdata info, dx = base bus I/O port (0x1F0, 0x170, ...), ebp = 32bit "relative" LBA
; BSY and DRQ ATA status bits must already be known to be clear on both slave and master
; outputs: data stored in edi; edi and ebp advanced, ebx decremented
; flags: on success Zero flag set, Carry clear
*/
    __asm__(
	".intel_syntax noprefix\n"
	"	xor eax, eax\n"
	".att_syntax\n"
	//	"	add ebp, [esi + dd_stLBA]\n"	// convert relative LBA to absolute LBA 
	"	addl %2, %%esi\n"	// convert relative LBA to absolute LBA 
	".intel_syntax noprefix\n"
	//  special case: did the addition overflow 32 bits (carry set)? 
	"	adc ah, 0\n"			// if so, ah = LBA byte #5 = 1 
	"	mov ecx, esi\n"		// save a working copy of 32 bit absolute LBA 
/* 
; for speed purposes, never OUT to the same port twice in a row -- avoiding it is messy but best
;outb (0x1F2, sectorcount high)
;outb (0x1F3, LBA4)
;outb (0x1F4, LBA5)			-- value = 0 or 1 only
;outb (0x1F5, LBA6)			-- value = 0 always
;outb (0x1F2, sectorcount low)
;outb (0x1F3, LBA1)
;outb (0x1F4, LBA2)
;outb (0x1F5, LBA3)
*/
	"bswap ecx\n"		// make LBA4 and LBA3 easy to access (cl, ch)
	"or dl, 2\n"		// dx = sectorcount port -- usually port 1f2
	"mov al, bh\n"		// sectorcount -- high byte
	"out dx, al\n"
	"mov al, cl\n"
	"inc edx\n"
	"out dx, al\n"		// LBA4 = LBAlow, high byte (1f3)
	"inc edx\n"
	"mov al, ah\n"		// LBA5 was calculated above
	"out dx, al\n"		// LBA5 = LBAmid, high byte (1f4)
	"inc edx\n"
	"mov al, 0\n"		// LBA6 is always 0 in 32 bit mode
	"out dx, al\n"		// LBA6 = LBAhigh, high byte (1f5)
//
	"sub dl, 3\n"
	"mov al, bl\n"		// sectorcount -- low byte (1f2)
	"out dx, al\n"
	"mov ax, bp\n"		// get LBA1 and LBA2 into ax
	"inc edx\n"
	"out dx, al\n"		// LBA1 = LBAlow, low byte (1f3)
	"mov al, ah\n"		// LBA2
	"inc edx\n"
	"out dx, al\n"		// LBA2 = LBAmid, low byte (1f4)
	"mov al, ch\n"		// LBA3
	"inc edx\n"
	"out dx, al\n"		// LBA3 = LBAhigh, low byte (1f5)
 
	//	"mov al, byte [esi + dd_sbits]\n"	// master/slave flag | 0xe0
	".att_syntax\n"
	"movb %6, %%al\n"	// master/slave flag | 0xe0
	".intel_syntax noprefix\n"

	"inc edx\n"
	"and al, 0x50\n"	// get rid of extraneous LBA28 bits in drive selector
	"out dx, al\n"		// drive select (1f6)
 
	"inc edx\n"
	"mov al, 0x24\n"	// send "read ext" command to drive
	"out dx, al\n"		// command (1f7)
// ignore the error bit for the first 4 status reads -- ie. implement 400ns delay on ERR only
// wait for BSY clear and DRQ set
	"mov ecx, 4\n"
".lp1:\n"
	"in al, dx\n"		// grab a status byte
	"test al, 0x80\n"	// BSY flag set?
	"jne short .retry\n"
	"test al, 8\n"		// DRQ set?
	"jne short .data_rdy\n"
".retry:\n"
	"dec ecx\n"
	"jg  .lp1\n"
// need to wait some more -- loop until BSY clears or ERR sets (error exit if ERR sets)
".pior_l:\n"
	"in al, dx\n"		// grab a status byte
	"test al, 0x80\n"	// BSY flag set?
	"jne  .pior_l\n"	// (all other flags are meaningless if BSY is set)
	"test al, 0x21\n"		// ERR or DF set?
	"jne  .fail\n"
".data_rdy:\n"
// if BSY and ERR are clear then DRQ must be set -- go and read the data
	"sub dl, 7\n"		// read from data port (ie. 0x1f0)
	"mov cx, 256\n"
	"rep insw\n"		// gulp one 512b sector into edi
	"or dl, 7\n"		// "point" dx back at the status register
	"in al, dx\n"		// delay 400ns to allow drive to set new values of BSY and DRQ
	"in al, dx\n"
	"in al, dx\n"
	"in al, dx\n"
// After each DRQ data block it is mandatory to either:
// receive and ack the IRQ -- or poll the status port all over again
	"inc esi\n"			// increment the current absolute LBA (overflowing is OK!)
	"dec ebx\n"			// decrement the "sectors to read" count
	"test bx, bx\n"			// check if "sectorcount" just decremented to 0
	"jne short .pior_l\n"
 
	"sub dx, 7\n"			// "point" dx back at the base IO port, so it's unchanged

	//	"sub esi, [esi + dd_stLBA]\n"	// convert absolute lba back to relative
	".att_syntax\n"
	"subl %2, %%esi\n"	// convert absolute lba back to relative
	".intel_syntax noprefix\n"
	
// this sub handles the >32bit overflow cases correcty, too
// "test" sets the zero flag for a "success" return -- also clears the carry flag 
	"mov bx, 0\n"	// clear error flag
	"test al, 0x21\n"		// test the last status ERR bits
	"je short .done\n"
".fail:\n"
	"stc\n"
	"mov bx, 1\n"	// set error flag
".done:\n"
	".att_syntax\n"
	//	%0		%1		%2		%3		%4		    %5		 %6	
	: "=b" (result) : "S" (lba_relative), "m" (lba_base), "b" (sec_read), "D" (ptr_dest), "d" (ioport_base), "m" (flags_masterslave) : "memory"
	);

/*
unsigned int lba_base, unsigned int sec_read, unsigned int* ptr_dest, unsigned short ioport_base)
;ATA PI0 33bit singletasking disk read function (up to 64K sectors, using 48bit mode)
; inputs: bx = sectors to read (0 means 64K sectors), edi -> destination buffer
; esi->ebp -> driverdata info, dx = base bus I/O port (0x1F0, 0x170, ...), ebp -> esi = 32bit "relative" LBA
; BSY and DRQ ATA status bits must already be known to be clear on both slave and master
; outputs: data stored in edi; edi and ebp advanced, ebx decremented
*/
	return result;
}

void ata_init()
{
	put_str("ATA: start init\n\r");
	unsigned int ioport_ata_status = 0x1f0 + 7; // primary bus status port
	ata_reset(ioport_ata_status);
	put_str("ATA: reset done \n\r");

	unsigned int lba_relative = 0;
	unsigned int lba_base = 0;
	unsigned int sec_read = 1;
	unsigned short ioport_base = 0x1f0;
	unsigned char ptr_dest[512];
	unsigned char flags_masterslave = 0x40; // 0x40 for master, 0x50 for slave in LBA48bit PIO
	if (!ata_read_pio48(lba_relative, lba_base, sec_read, ptr_dest, ioport_base, flags_masterslave)) {
		put_str("ATA: Error on read\n\r");
	} else {
		put_str("ATA: may succeed\n\r");
	}

}



void sys_out_long(uint16_t port, uint32_t  data)
{
	__asm__ __volatile__("outl %%eax, %%dx" : : "d" (port), "a" (data));
}

uint32_t sys_in_long(uint16_t port)
{
	uint32_t result;
	__asm__ __volatile__("inl %%dx, %%eax" : "=a" (result) : "dN" (port));
	return result;
}

uint16_t pci_config_read_word (uint8_t bus, uint8_t slot,
                             uint8_t func, uint8_t offset)
 {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;
 
    /* create configuration address as per Figure 1 */
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
 
    /* write out the address */
    sys_out_long (0xCF8, address);
    /* read in the data */
    /* (offset & 2) * 8) = 0 will choose the first word of the 32 bits register */
    tmp = (uint16_t)((sys_in_long (0xCFC) >> ((offset & 2) * 8)) & 0xffff);
    return (tmp);
}

 uint16_t pci_check_vendor(uint8_t bus, uint8_t slot)
 {
    uint16_t vendor, device;
    /* try and read the first configuration register. Since there are no */
    /* vendors that == 0xFFFF, it must be a non-existent device. */
    if ((vendor = pci_config_read_word(bus,slot,0,0)) != 0xFFFF) {
       device = pci_config_read_word(bus,slot,0,2);
       put_str("Found PCI devce bus: 0x"); dump_hex(bus, 2); put_str(" slot: 0x"); dump_hex(slot, 2);
       put_str(" vendor: 0x"); dump_hex(vendor,4); put_str(" device: 0x"); dump_hex(device,4); put_str("\n\r");

       
    } return (vendor);
 }

void pci_init()
{
   int bus, slot;
   for(bus=0;bus<256;bus++) {
     for(slot=0;slot<256;slot++) {
       pci_check_vendor(bus, slot);
     }
   }
}



int kern_init(void)
{
	extern unsigned char syscall_handler;

	unsigned char mask;
	unsigned char i;

	/* Setup console */
	cursor_pos.y += 2;
	update_cursor();

	/* Setup exception handler */
	for (i = 0; i < EXCEPTION_MAX; i++)
		intr_set_handler(i, (unsigned int)&exception_handler);
	intr_set_handler(EXCP_NUM_DE, (unsigned int)&divide_error_handler);
	intr_set_handler(EXCP_NUM_DB, (unsigned int)&debug_handler);
	intr_set_handler(EXCP_NUM_NMI, (unsigned int)&nmi_handler);
	intr_set_handler(EXCP_NUM_BP, (unsigned int)&breakpoint_handler);
	intr_set_handler(EXCP_NUM_OF, (unsigned int)&overflow_handler);
	intr_set_handler(EXCP_NUM_BR, (unsigned int)&bound_range_exceeded_handler);
	intr_set_handler(EXCP_NUM_UD, (unsigned int)&invalid_opcode_handler);
	intr_set_handler(EXCP_NUM_NM, (unsigned int)&device_not_available_handler);
	intr_set_handler(EXCP_NUM_DF, (unsigned int)&double_fault_handler);
	intr_set_handler(EXCP_NUM_CSO, (unsigned int)&coprocessor_segment_overrun_handler);
	intr_set_handler(EXCP_NUM_TS, (unsigned int)&invalid_tss_handler);
	intr_set_handler(EXCP_NUM_NP, (unsigned int)&segment_not_present_handler);
	intr_set_handler(EXCP_NUM_SS, (unsigned int)&stack_fault_handler);
	intr_set_handler(EXCP_NUM_GP, (unsigned int)&general_protection_handler);
	intr_set_handler(EXCP_NUM_PF, (unsigned int)&page_fault_handler);
	intr_set_handler(EXCP_NUM_MF, (unsigned int)&x87_fpu_floating_point_error_handler);
	intr_set_handler(EXCP_NUM_AC, (unsigned int)&alignment_check_handler);
	intr_set_handler(EXCP_NUM_MC, (unsigned int)&machine_check_handler);
	intr_set_handler(EXCP_NUM_XM, (unsigned int)&simd_floating_point_handler);
	intr_set_handler(EXCP_NUM_VE, (unsigned int)&virtualization_handler);

	/* Setup devices */
	con_init();
	timer_init();
	mem_init();

	pci_init();
	// ata_init();

	/* Setup File System */
	fs_init((void *)0x00011000);

	/* Setup tasks */
	kern_task_init();
	task_init(&fshell);

	/* Start paging */
	mem_page_start();

	/* Setup interrupt handler and mask register */
	intr_set_handler(INTR_NUM_TIMER, (unsigned int)&timer_handler);
	intr_set_handler(INTR_NUM_KB, (unsigned int)&keyboard_handler);
	intr_set_handler(INTR_NUM_USER128, (unsigned int)&syscall_handler);
	intr_init();
	mask = intr_get_mask_master();
	mask &= ~(INTR_MASK_BIT_TIMER | INTR_MASK_BIT_KB);
	intr_set_mask_master(mask);
	sti();

	/* End of kernel initialization process */
	while (1) {
		x86_halt();
	}

	return 0;
}
