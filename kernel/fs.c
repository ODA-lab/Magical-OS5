#include <fs.h>
#include <sys/types.h>
#include <memory.h>
#include <list.h>
#include <queue.h>
#include <common.h>
#include <debug.h>

struct file_head fhead;
struct file fshell, fuptime, fnew;

void fs_init(void *fs_base_addr)
{
	queue_init((struct list *)&fhead);
	fhead.num_files = *(unsigned char *)fs_base_addr;

	fshell.name = (char *)fs_base_addr + PAGE_SIZE;
	fshell.data_base_addr = (char *)fs_base_addr + PAGE_SIZE + 32;
	queue_enq((struct list *)&fshell, (struct list *)&fhead);

	fuptime.name = (char *)fs_base_addr + (PAGE_SIZE * 2);
	fuptime.data_base_addr = (char *)fs_base_addr + (PAGE_SIZE * 2) + 32;
	queue_enq((struct list *)&fuptime, (struct list *)&fhead);

	fnew.name = (char *)fs_base_addr + (PAGE_SIZE * 3);
	fnew.data_base_addr = (char *)fs_base_addr + (PAGE_SIZE * 3) + 32;
	queue_enq((struct list *)&fnew, (struct list *)&fhead);
}

struct file *fs_open(const char *name)
{
	struct file *f;

	/* 将来的には、struct fileのtask_idメンバにopenしたタスクの
	 * TASK_IDを入れるようにする。そして、openしようとしているファ
	 * イルのtask_idが既に設定されていれば、fs_openはエラーを返す
	 * ようにする */
	for (f = (struct file *)fhead.lst.next; f != (struct file *)&fhead; f = (struct file *)f->lst.next) {
		if (!str_compare(name, f->name))


			return f;
	}

	return NULL;
}

int fs_close(struct file *f)
{
	/* 将来的には、fidに対応するstruct fileのtask_idメンバーを設定
	 * なし(0)にする。 */
	(void)f;
	return 0;
}
