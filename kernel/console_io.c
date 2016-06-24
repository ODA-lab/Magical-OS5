#include <cpu.h>
#include <intr.h>
#include <io_port.h>
#include <console_io.h>
#include <sched.h>
#include <lock.h>
#include <stdarg.h>
#include <stdlib.h>

#define PRINTK_BUF_SIZE (256 + 1)

#define QUEUE_BUF_SIZE	256

const char keymap[] = {
	0x00, ASCII_ESC, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '^', ASCII_BS, ASCII_HT,
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '@', '[', '\n', 0x00, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
	':', 0x00, 0x00, ']', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/', 0x00, '*',
	0x00, ' ', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, '7',
	'8', '9', '-', '4', '5', '6', '+', '1',
	'2', '3', '0', '.', 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, '_', 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, '\\', 0x00, 0x00
};

struct queue {
	unsigned char buf[QUEUE_BUF_SIZE];
	unsigned char start, end;
	unsigned char is_full;
} keycode_queue;

struct cursor_position cursor_pos;

static unsigned char error_status;

static void enqueue(struct queue *q, unsigned char data)
{
	unsigned char if_bit;

	if (q->is_full) {
		error_status = 1;
	} else {
		error_status = 0;
		kern_lock(&if_bit);
		q->buf[q->end] = data;
		q->end++;
		if (q->start == q->end) q->is_full = 1;
		kern_unlock(&if_bit);
	}
}

static unsigned char dequeue(struct queue *q)
{
	unsigned char data = 0;
	unsigned char if_bit;

	kern_lock(&if_bit);
	if (!q->is_full && (q->start == q->end)) {
		error_status = 1;
	} else {
		error_status = 0;
		data = q->buf[q->start];
		q->start++;
		q->is_full = 0;
	}
	kern_unlock(&if_bit);

	return data;
}

void do_ir_keyboard(void)
{
	unsigned char status, data;

	status = inb_p(IOADR_KBC_STATUS);
	if (status & IOADR_KBC_STATUS_BIT_OBF) {
		data = inb_p(IOADR_KBC_DATA);
		enqueue(&keycode_queue, data);
	}
	sched_update_wakeupevq(EVENT_TYPE_KBD);
	outb_p(IOADR_MPIC_OCW2_BIT_MANUAL_EOI | INTR_IR_KB,
		IOADR_MPIC_OCW2);
}

void con_init(void)
{
	keycode_queue.start = 0;
	keycode_queue.end = 0;
	keycode_queue.is_full = 0;
	error_status = 0;

/*
	// unlock CRTC registers 
	outb_p(0x3, 0x3d4);
        outb_p(inb_p(0x3d5) | 0x80, 0x3d5);
        outb_p(0x11, 0x3d4);
        outb_p(inb_p(0x3d5) & ~0x80, 0x3d5);

	outb_p(0x10, 0x3c0); outb_p(0x01, 0x3c1);  // mode control
	outb_p(0x11, 0x3c0); outb_p(0x00, 0x3c1);  // overscan register
	outb_p(0x12, 0x3c0); outb_p(0x0f, 0x3c1);  // color plane enable
	outb_p(0x13, 0x3c0); outb_p(0x00, 0x3c1);  // horizontal plane enable
	outb_p(0x14, 0x3c0); outb_p(0x00, 0x3c1);  // color select
	outb_p(0xe3, 0x3c2); // misc out register
	outb_p(0x01, 0x3c4); outb_p(0x01, 0x3c5);  // clock mode register
	outb_p(0x03, 0x3c4); outb_p(0x00, 0x3c5);  // character select
	outb_p(0x04, 0x3c4); outb_p(0x02, 0x3c5);  // memory mode register
	outb_p(0x05, 0x3ce); outb_p(0x00, 0x3cf);  // mode register
	outb_p(0x06, 0x3ce); outb_p(0x05, 0x3cf);  // misc register
	outb_p(0x00, 0x3d4); outb_p(0x5f, 0x3d5);  // horizontal total
	outb_p(0x01, 0x3d4); outb_p(0x4f, 0x3d5);  // horizontal display enable end
	outb_p(0x02, 0x3d4); outb_p(0x50, 0x3d5);  // horizontal blank start
	outb_p(0x03, 0x3d4); outb_p(0x82, 0x3d5);  // horizontal blank end
	outb_p(0x04, 0x3d4); outb_p(0x54, 0x3d5);  // horizontal retrace start
	outb_p(0x05, 0x3d4); outb_p(0x80, 0x3d5);  // horizontal retrace end
	outb_p(0x06, 0x3d4); outb_p(0x0b, 0x3d5);  // vertical total
	outb_p(0x07, 0x3d4); outb_p(0x3e, 0x3d5);  // overflow register
	outb_p(0x08, 0x3d4); outb_p(0x00, 0x3d5);  // preset row scan
	outb_p(0x09, 0x3d4); outb_p(0x40, 0x3d5);  // max scan line
	outb_p(0x10, 0x3d4); outb_p(0xea, 0x3d5);  // vertical retrace start
	outb_p(0x11, 0x3d4); outb_p(0x8c, 0x3d5);  // vertical retrace end
	outb_p(0x12, 0x3d4); outb_p(0xdf, 0x3d5);  // vertical display enable end
	outb_p(0x13, 0x3d4); outb_p(0x28, 0x3d5);  // logical width
	outb_p(0x14, 0x3d4); outb_p(0x00, 0x3d5);  // underline location
	outb_p(0x15, 0x3d4); outb_p(0xe7, 0x3d5);  // vertical blank start
	outb_p(0x16, 0x3d4); outb_p(0x04, 0x3d5);  // vertical blank end
	outb_p(0x17, 0x3d4); outb_p(0xe3, 0x3d5);  // mode control
	
	// color palette
	{
	  int idx, red, green, blue;
	  for(idx=0;idx<16;idx++) {
	    red=idx * 4; green = idx * 4; blue = idx * 4;
	    outb_p(idx, 0x3c8); outb_p(red, 0x3c9); outb_p(green, 0x3c9); outb_p(blue, 0x3c9);
	  }
	}

	// lock 16-color palette and unblank display 
	(void)inb_p(0x3da);
	outb_p(0x20, 0x3c0);


	{
		// unsigned char *vram = 0xa0000;
		unsigned int x;
		for(x= 0xa0000;x<0xbffff; x += 4) {
		  *((unsigned char*) x ) = 0xff ; 
		}
	}
*/
}

void update_cursor(void)
{
	unsigned int cursor_address = (cursor_pos.y * 80) + cursor_pos.x;
	unsigned char cursor_address_msb = (unsigned char)(cursor_address >> 8);
	unsigned char cursor_address_lsb = (unsigned char)cursor_address;
	unsigned char if_bit;

	kern_lock(&if_bit);
	outb_p(0x0e, 0x3d4);
	outb_p(cursor_address_msb, 0x3d5);
	outb_p(0x0f, 0x3d4);
	outb_p(cursor_address_lsb, 0x3d5);
	kern_unlock(&if_bit);

	if (cursor_pos.y >= ROWS) {
		unsigned int start_address = (cursor_pos.y - ROWS + 1) * 80;
		unsigned char start_address_msb = (unsigned char)(start_address >> 8);
		unsigned char start_address_lsb = (unsigned char)start_address;

		kern_lock(&if_bit);
		outb_p(0x0c, 0x3d4);
		outb_p(start_address_msb, 0x3d5);
		outb_p(0x0d, 0x3d4);
		outb_p(start_address_lsb, 0x3d5);
		kern_unlock(&if_bit);
	}
}

void put_char_pos(char c, unsigned char x, unsigned char y)
{
	unsigned char *pos;

	pos = (unsigned char *)(SCREEN_START + (((y * COLUMNS) + x) * 2));
	*(unsigned short *)pos = (unsigned short)((ATTR << 8) | c);
}

void put_char(char c)
{
	switch (c) {
	case '\r':
		cursor_pos.x = 0;
		break;

	case '\n':
		cursor_pos.y++;
		cursor_pos.x=0;
		break;

	default:
		put_char_pos(c, cursor_pos.x, cursor_pos.y);
		if (cursor_pos.x < COLUMNS - 1) {
			cursor_pos.x++;
		} else {
			cursor_pos.x = 0;
			cursor_pos.y++;
		}
		break;
	}

	update_cursor();
}

void put_str(char *str)
{
	while (*str != '\0') {
		put_char(*str);
		str++;
	}
}

void put_str_pos(char *str, unsigned char x, unsigned char y)
{
	while (*str != '\0') {
		switch (*str) {
		case '\r':
			x = 0;
			break;

		case '\n':
			y++;
			break;

		default:
			put_char_pos(*str, x, y);
			if (x < COLUMNS - 1) {
				x++;
			} else {
				x = 0;
				y++;
			}
			break;
		}
		str++;
	}
}

void dump_hex(unsigned int val, unsigned int num_digits)
{
	unsigned int new_x = cursor_pos.x + num_digits;
	unsigned int dump_digit = new_x - 1;

	while (num_digits) {
		unsigned char tmp_val = val & 0x0000000f;
		if (tmp_val < 10) {
			put_char_pos('0' + tmp_val, dump_digit, cursor_pos.y);
		} else {
			put_char_pos('A' + tmp_val - 10, dump_digit, cursor_pos.y);
		}
		val >>= 4;
		dump_digit--;
		num_digits--;
	}

	cursor_pos.x = new_x;

	update_cursor();
}

void dump_hex_pos(unsigned int val, unsigned int num_digits, unsigned char x, unsigned char y)
{
	unsigned int new_x = x + num_digits;
	unsigned int dump_digit = new_x - 1;

	while (num_digits) {
		unsigned char tmp_val = val & 0x0000000f;
		if (tmp_val < 10) {
			put_char_pos('0' + tmp_val, dump_digit, y);
		} else {
			put_char_pos('A' + tmp_val - 10, dump_digit, y);
		}
		val >>= 4;
		dump_digit--;
		num_digits--;
	}
}

unsigned char get_keydata_noir(void)
{
	while (!(inb_p(IOADR_KBC_STATUS) & IOADR_KBC_STATUS_BIT_OBF));
	return inb_p(IOADR_KBC_DATA);
}

unsigned char get_keydata(void)
{
	unsigned char data;
	unsigned char dequeuing = 1;
	unsigned char if_bit;

	while (dequeuing) {
		kern_lock(&if_bit);
		data = dequeue(&keycode_queue);
		if (!error_status)
			dequeuing = 0;
		kern_unlock(&if_bit);
		if (dequeuing)
			wakeup_after_event(EVENT_TYPE_KBD);
	}

	return data;
}

unsigned char get_keycode(void)
{
	return get_keydata() & ~IOADR_KBC_DATA_BIT_BRAKE;
}

unsigned char get_keycode_pressed(void)
{
	unsigned char keycode;
	while ((keycode = get_keydata()) & IOADR_KBC_DATA_BIT_BRAKE);
	return keycode & ~IOADR_KBC_DATA_BIT_BRAKE;
}

unsigned char get_keycode_released(void)
{
	unsigned char keycode;
	while (!((keycode = get_keydata()) & IOADR_KBC_DATA_BIT_BRAKE));
	return keycode & ~IOADR_KBC_DATA_BIT_BRAKE;
}

char get_char(void)
{
	return keymap[get_keycode_pressed()];
}

unsigned int get_line(char *buf, unsigned int buf_size)
{
	unsigned int i;

	for (i = 0; i < buf_size - 1;) {
		buf[i] = get_char();
		if (buf[i] == ASCII_BS) {
			if (i == 0) continue;
			cursor_pos.x--;
			update_cursor();
			put_char_pos(' ', cursor_pos.x, cursor_pos.y);
			i--;
		} else {
			put_char(buf[i]);
			if (buf[i] == '\n') {
				put_char('\r');
				break;
			}
			i++;
		}
	}
	buf[i] = '\0';

	return i;
}


/**
 * It's a simple printf(3) for kernel.
 * @param fmt as format strings.
 * @param arguments for format strings.
 */
void printk(char *fmt, ...) 
{
        va_list ap;
        int ival;
        char *sval;
	int longflag = 0;

        va_start(ap, fmt);
        while (*fmt) {
                char buf[PRINTK_BUF_SIZE] = { 0 };
		if (*fmt == '%' || longflag ) {
			longflag = 0;
                        switch (*++fmt) {
                        case 'd':
                        case 'c':
			case 'u':
                                ival = va_arg(ap, int);
                                itoa(ival, buf);
                                put_str(buf);
                                break;
                        case 'x':
                                ival = va_arg(ap, int);
                                itox(ival, buf);
                                put_str(buf);
                                break;
                        case 's':
                                sval = va_arg(ap, char *);
				put_str(sval);
                                break;
			case 'l':
				longflag = 1;
				fmt--;
				break;
                        default:
				break;
			}
                        fmt++;
                } else
                        put_char(*fmt++);
        }

        va_end(ap);
}

