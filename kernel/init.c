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
#include <pci.h>
#include <block_driver.h>
#include <ata.h>
#include <io_port.h>
#include <string.h>

char buf0[512];
char buf1[512];

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

	// find pci devices
	find_pci_device();
	show_all_pci_device();

	// Init ATA device.
	init_ata_disk_driver();
	show_all_registered_driver();
	struct blk_device_drivers* drivers = get_blk_driver("ATA disk");
	drivers->op->open();
	strcpy(buf1, "--------------- Please add virtual hard drive at primary master ------------\n");
	strcpy(buf0, "--------------- Test data written in disk! Disk I/O Success ----------------\n");
	drivers->op->write(0, 100, (sector_t *)buf0, SECTOR_SIZE);
	drivers->op->read(0, 100, (sector_t *)buf1, SECTOR_SIZE);
	put_str(buf1);

	/* End of kernel initialization process */
	while (1) {
		x86_halt();
	}

	return 0;
}
