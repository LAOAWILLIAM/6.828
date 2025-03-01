// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
    { "backtrace", "Backtrace", mon_backtrace },
    { "continue", "Continue instructions", mon_continue },
    { "stepi", "Single-step one instruction", mon_stepi }
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	cprintf("Stack backtrace:\n");

    uint32_t ebp = read_ebp();
    uint32_t *cur_ebp_address = (uint32_t *)(ebp);

//    cprintf("%08x, %08x, %08x, %08x, %08x, %08x\n",
//            tf->tf_ss, tf->tf_cs, tf->tf_ds, tf->tf_es, tf->tf_esp, tf->tf_eip);

    while (true) {
        struct Eipdebuginfo info;
        uint32_t eip = *(cur_ebp_address + 1);
        cprintf("ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n",
                cur_ebp_address,
                eip,
                *(cur_ebp_address + 2),
                *(cur_ebp_address + 3),
                *(cur_ebp_address + 4),
                *(cur_ebp_address + 5),
                *(cur_ebp_address + 6));
        if (debuginfo_eip(eip, &info) == 0) {
            cprintf("\t%s:%d: %.*s+%d\n",
                    info.eip_file,
                    info.eip_line,
                    info.eip_fn_namelen,
                    info.eip_fn_name,
                    eip - info.eip_fn_addr);
        } else {
            cprintf("\t\n");
        }
        // kernel page fault here fixed by not allowing cur_ebp_address beyond tf_esp
        if (cur_ebp_address == (void *) tf->tf_esp) break;
        cur_ebp_address = (uint32_t *)(*cur_ebp_address);
    }

	return 0;
}

int
mon_continue(int argc, char **argv, struct Trapframe *tf) {
    if (!tf) {
        cprintf("mon_continue: tf is null\n");
        return 0;
    }
    tf->tf_eflags &= ~(FL_TF);
    return -1;
}

int
mon_stepi(int argc, char **argv, struct Trapframe *tf) {
    if (!tf) {
        cprintf("mon_stepi: tf is null\n");
        return 0;
    }

    struct Eipdebuginfo info;
    uint32_t eip = tf->tf_eip;

    if (debuginfo_eip(eip, &info) == 0) {
        cprintf("%s:%d: %.*s+%d\n",
                info.eip_file,
                info.eip_line,
                info.eip_fn_namelen,
                info.eip_fn_name,
                eip - info.eip_fn_addr);
    } else {
        cprintf("\n");
    }

    tf->tf_eflags |= (FL_TF);
    return -1;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

    int x = 1, y = 3, z = 4;
    cprintf("x %d, y %x, z %d\n", x, y, z);

    unsigned int i = 0x00646c72;
    cprintf("H%x Wo%s\n", 57616, &i);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
