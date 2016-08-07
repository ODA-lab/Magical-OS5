#include <fs.h>
#include <sys/types.h>
#include <memory.h>
#include <list.h>
#include <queue.h>
#include <common.h>
#include <debug.h>
#include <console_io.h>
#include <string.h>


struct file_head fhead;
struct file fshell, fuptime;//, fnew;

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
	/*
	fnew.name = (char *)fs_base_addr + (PAGE_SIZE * 3);
	fnew.data_base_addr = (char *)fs_base_addr + (PAGE_SIZE * 3) + 32;
	queue_enq((struct list *)&fnew, (struct list *)&fhead);
	*/


}

struct file *fs_open(const char *name)
{
	struct file *f;

	/* 将来的には、struct fileのtask_idメンバにopenしたタスクの
	 * TASK_IDを入れるようにする。そして、openしようとしているファ
	 * イルのtask_idが既に設定されていれば、fs_openはエラーを返す
	 * ようにする */
	for (f = (struct file *)fhead.lst.next; f != (struct file *)&fhead; f = (struct file *)f->lst.next) {

		if (!str_compare(name, f->name)){
			return f;
			//return f;
		}
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

void fs_make(char *filename,char *filedata){

	struct file *check;
	for(check = (struct file *)fhead.lst.next;check != (struct file *)&fhead;check = (struct file *)check->lst.next){
		if (!str_compare(filename, check->name)){
			put_str("The named file exists.");
			put_str("\r\n");
			return;
		}
	}
	struct file *f;
	f = (struct file *)kmalloc(sizeof(struct file)); //sizeof(struct file)
	//f->name = (char *)kmalloc((unsigned int) 32);
	f->name = (char *)mem_alloc();
	f->data_base_addr = (char *)f->name + 32;
	queue_enq((struct list *)f,(struct list *)&fhead);
	//queue_enq(&f->lst,(struct list *)&fhead);

	strcpy(f->name,filename);
	strcpy(f->data_base_addr,filedata);


	/*
	put_str("f       :");
	dump_hex(f,8);
	put_str("\r\n");

	put_str("f->name :");
	dump_hex(f->name,8);
	put_str("\r\n");

	put_str("f->name :");
	put_str(f->name);
	put_str("\r\n");

	put_str("f->data_base_addr :");
	dump_hex(f->data_base_addr,8);
	put_str("\r\n");


	put_str("f->data_base_addr :");
	put_str(filedata);
	put_str("\r\n");
	*/



}

void ls(){
	struct file *f;
	//put_str("--- ls command ---");
	//put_str("\r\n");
	for(f = (struct file *)fhead.lst.next;f != (struct file *)&fhead;f = (struct file *)f->lst.next){//

		put_str(f->name);
		put_str(" ");
	}
	put_str("\r\n");
	/*put_str("\r\n");
	put_str("--- ls end ---");
	put_str("\r\n");*/
}

void read(char *filename){
	struct file *f;
	for(f = (struct file *)fhead.lst.next;f != (struct file *)&fhead;f = (struct file *)f->lst.next){
		if (!str_compare(filename, f->name)){
			put_str(f->data_base_addr);
			put_str("\r\n");
			//return f;
		}
	}
}
