#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int r;
	cprintf("i am parent environment %08x\n", thisenv->env_id);
	if ((r = spawnl("hello", "hello", 0)) < 0)
		panic("spawn(hello) failed: %e", r);

//    if (fork() == 0) {
//        cprintf("i am child environment %08x\n", thisenv->env_id);
//        exec("hello", "hello", 0);
//        exit();
//    }
//
//    cprintf("i am parent environment after fork\n");
}
