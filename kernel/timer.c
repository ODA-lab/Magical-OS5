#include <timer.h>
#include <io_port.h>
#include <intr.h>
#include <sched.h>
#include <sys/types.h>

volatile unsigned long int global_counter = 0;

void do_ir_timer(void)
{
	global_counter += TIMER_TICK_MS;
	sched_update_wakeupq();
	schedule(SCHED_CAUSE_TIMER);
}

void timer_init(void)
{
	/* Setup PIT */
	outb_p(IOADR_PIT_CONTROL_WORD_BIT_COUNTER0
	       | IOADR_PIT_CONTROL_WORD_BIT_16BIT_READ_LOAD
	       | IOADR_PIT_CONTROL_WORD_BIT_MODE2, IOADR_PIT_CONTROL_WORD);
	/* 割り込み周期11932(0x2e9c)サイクル(=100Hz、10ms毎)に設定 */
	outb_p(0x9c, IOADR_PIT_COUNTER0);
	outb_p(0x2e, IOADR_PIT_COUNTER0);
}

unsigned long int timer_get_global_counter(void)
{
	return global_counter;
}


/**
 * Wait seconds.
 * @param how many seconds do you wait?
 */
void timer_wait_loop_sec(unsigned int sec)
{
	unsigned long int old = global_counter;
	unsigned long int end = old + (sec * 1000) + 1;

	while (global_counter < end)
		; // nothing.
}

/**
 * Wait usec.
 * @param How many seconds do you wait?
 */
void timer_wait_loop_usec(unsigned int usec)
{
	unsigned long int old = global_counter;
	unsigned long int end = old + (usec / 1000) + 1;

	while (global_counter < end)
		; // nothing.
}

