// program to cause a breakpoint trap

#include <inc/lib.h>

void
umain(int argc, char **argv)
{
//    cprintf("hello, world\n");
	asm volatile("int $3");
//    cprintf("i am environment %08x\n", thisenv->env_id);
}

