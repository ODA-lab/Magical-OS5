#include <debug.h>
#include <memory.h>
#include <sys/types.h>
#include <console_io.h>

#define CR4_BIT_PGE	(1U << 7)
#define MAX_HEAP_PAGES	3584
#define HEAP_START_ADDR	0x00100000

static char heap_alloc_table[MAX_HEAP_PAGES] = {0};

void mem_init(void)
{
	struct page_directory_entry *pde;
	struct page_table_entry *pte;
	unsigned int paging_base_addr;
	unsigned int i;
	unsigned int cr4;

	/* Enable PGE(Page Global Enable) flag of CR4*/
	__asm__("movl	%%cr4, %0":"=r"(cr4):);
	cr4 |= CR4_BIT_PGE;
	__asm__("movl	%0, %%cr4"::"r"(cr4));

	/* Initialize kernel page directory */
	pde = (struct page_directory_entry *)0x0008f000;
	pde->all = 0;
	pde->p = 1;
	pde->r_w = 1;
	pde->pt_base = 0x00090;
	pde++;
	for (i = 1; i < 0x400; i++) {
		pde->all = 0;
		pde++;
	}

	/* Initialize kernel page table */
	pte = (struct page_table_entry *)0x00090000;
	for (i = 0x000; i < 0x007; i++) {
		pte->all = 0;
		pte++;
	}
	paging_base_addr = 0x00007;
	for (; i <= 0x085; i++) {
		pte->all = 0;
		pte->p = 1;
		pte->r_w = 1;
		pte->g = 1;
		pte->page_base = paging_base_addr;
		paging_base_addr += 0x00001;
		pte++;
	}
	for (; i < 0x095; i++) {
		pte->all = 0;
		pte++;
	}
	//heap
	paging_base_addr = 0x00095;
	for (; i <= 0x09f; i++) {
		pte->all = 0;
		pte->p = 1;
		pte->r_w = 1;
		pte->g = 1;
		pte->page_base = paging_base_addr;
		paging_base_addr += 0x00001;
		pte++;
	}
	for (; i < 0x000b8; i++) {
		pte->all = 0;
		pte++;
	}
	//VRAM
	paging_base_addr = 0x000b8;
	for (; i <= 0x0bf; i++) {
		pte->all = 0;
		pte->p = 1;
		pte->r_w = 1;
		pte->pwt = 1;
		pte->pcd = 1;
		pte->g = 1;
		pte->page_base = paging_base_addr;
		paging_base_addr += 0x00001;
		pte++;
	}
	for (; i < 0x00100; i++) {
		pte->all = 0;
		pte++;
	}
	//Extended Heap(14MiB)
	paging_base_addr = 0x00100;
	for (; i < 0x00eff; i++) {
		pte->all = 0;
		pte->p = 1;
		pte->r_w = 1;
    pte->g = 1;
		pte->page_base = paging_base_addr;
		paging_base_addr += 0x00001;
		pte++;
	}
  for (; i < 0x40000; i++) {
		pte->all = 0;
		pte++;
	}
}

void mem_page_start(void)
{
	unsigned int cr0;

	__asm__("movl	%%cr0, %0":"=r"(cr0):);
	cr0 |= 0x80000000;
	__asm__("movl	%0, %%cr0"::"r"(cr0));
}

void *mem_alloc(void)
{
	unsigned int i;

	for (i = 0; heap_alloc_table[i] && (i < MAX_HEAP_PAGES); i++);

	if (i >= MAX_HEAP_PAGES)
		return (void *)NULL;

	heap_alloc_table[i] = 1;
	return (void *)(HEAP_START_ADDR + i * PAGE_SIZE);
}

void mem_free(void *page)
{
	unsigned int i = ((unsigned int)page - HEAP_START_ADDR) / PAGE_SIZE;
	heap_alloc_table[i] = 0;
}

void* kmalloc(unsigned int size)
{
	static unsigned int sTotalSize = 0;
	static unsigned char* sTopAddr = NULL;

	if (sTopAddr == NULL) {
		sTopAddr = mem_alloc();
		if (sTopAddr == NULL) {
	  		KERN_ABORT("Memory allocate failed");
		}
	}

	sTotalSize += size;
	// printk("kmalloc: %d left: %d\n", size, PAGE_SIZE-sTotalSize);
	if (sTotalSize > PAGE_SIZE)  {
		printk("kmalloc: %d left: %d\n", size, PAGE_SIZE-sTotalSize);
		KERN_ABORT("Too much memory");
	}

	return sTopAddr+sTotalSize-size;
}

void  kfree(void *ptr)
{
	// mem_free(ptr);
	(void)ptr;
}
