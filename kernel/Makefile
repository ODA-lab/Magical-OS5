CFLAGS	=	-Wall -Wextra -Wno-unused-function
CFLAGS	+=	-nostdinc -nostdlib -fno-builtin -c
CFLAGS	+=	-Iinclude

.S.o:
	gcc $(CFLAGS) -o $@ $<
.c.o:
	gcc $(CFLAGS) -o $@ $<

kernel.bin: sys.o cpu.o intr.o excp.o memory.o sched.o fs.o task.o syscall.o lock.o timer.o console_io.o queue.o common.o debug.o init.o kern_task_init.o \
	 pci.o ata.o stdlib.o string.o block_driver.o i825xx.o
	ld -o $@ $+ -Map System.map -s -T sys.ld -x

sys.o: sys.S
cpu.o: cpu.c
intr.o: intr.c
excp.o: excp.c
memory.o: memory.c
sched.o: sched.c
fs.o: fs.c
task.o: task.c
syscall.o: syscall.c
lock.o: lock.c
timer.o: timer.c
console_io.o: console_io.c
queue.o: queue.c
common.o: common.c
debug.o: debug.c
init.o: init.c
kern_task_init.o: kern_task_init.c
pci.o: pci.c
ata.o: ata.c
stdlib.o: stdlib.c
string.o: string.c
block_driver.o: block_driver.c
i825xx.o: i825xx.c

clean:
	rm -f *~ *.o *.bin *.dat *.img *.map

.PHONY: clean
