// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

// Assembly language pgfault entrypoint defined in lib/pfentry.S.
extern void _pgfault_upcall(void);

// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
//    cprintf("enter pgfault\n");
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	pte_t pte = 0;
    if((uvpd[PDX((uintptr_t)addr)] & PTE_P) != 0)
        pte = uvpt[PGNUM((uintptr_t)addr)];

//    cprintf("enter 0\n");

	if (!(FEC_WR & err) && !(pte & PTE_COW))
	    panic("faulting access not a write and not a copy-on-write page\n");

//    cprintf("enter 1\n");

    // Allocate a new page, map it at a temporary location (PFTEMP),
    // copy the data from the old page to the new page, then move the new
    // page to the old page's address.
    // Hint:
    //   You should make three system calls.

    // LAB 4: Your code here.

    // sys_getenvid here is a user-level system call so we can use it
    envid_t envid = sys_getenvid();
//    cprintf("envid: %08x\n", envid);
    // round down addr to be page aligned to pass validation check
    uintptr_t start_addr = ROUNDDOWN((uintptr_t)addr, PGSIZE);

//    cprintf("enter 2\n");

    // allocate a new page in child space and map it at PFTEMP
    if ((r = sys_page_alloc(envid, (void *) PFTEMP, PTE_P | PTE_U | PTE_W)) < 0)
        panic("sys_page_alloc: %e", r);

//    cprintf("enter 3\n");

    // copy the data from the old page to the new page
    memmove((void *) PFTEMP, (void *) start_addr, PGSIZE);

//    cprintf("enter 4\n");

    // map the old page address to the new page address to solve page fault
    if ((r = sys_page_map(envid, (void *) PFTEMP, envid, (void *) start_addr, PTE_P | PTE_U | PTE_W)) < 0)
        panic("sys_page_map: %e", r);

//    cprintf("quit pgfault\n");

//	panic("pgfault not implemented");
}

// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
//	panic("duppage not implemented");

    // calculate virtual memory based on page number
    void *addr = (void *) (pn * PGSIZE);

    if (uvpt[pn] & PTE_SHARE) {
        if ((r = sys_page_map(thisenv->env_id, addr, envid, addr, uvpt[pn] & PTE_SYSCALL)) < 0) {
            panic("sys_page_map: %e", r);
            return r;
        }
    } else if (uvpt[pn] & PTE_W || uvpt[pn] & PTE_COW) {
        // here we cannot use curenv, as it is a kernel-level variable rather than a user-level variable

        // map the new page to child and mark it COW in child
        if ((r = sys_page_map(thisenv->env_id, addr, envid, addr, PTE_P | PTE_U | PTE_COW)) < 0) {
            panic("sys_page_map: %e", r);
            return r;
        }
        // mark the page in parent COW as well if it is not COW right now
//        if (!(uvpt[pn] & PTE_COW)) {
            if ((r = sys_page_map(thisenv->env_id, addr, thisenv->env_id, addr, PTE_P | PTE_COW | PTE_U)) < 0) {
                panic("sys_page_map: %e", r);
                return r;
            }
//        }
    } else {
        // map the new page to child but not mark it COW in child
        if ((r = sys_page_map(thisenv->env_id, addr, envid, addr, PTE_P | PTE_U)) < 0) {
            panic("sys_page_map: %e", r);
            return r;
        }
    }

	return 0;
}

// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
//	panic("fork not implemented");

    envid_t envid;
    uint8_t *addr;
    int r;

    set_pgfault_handler(pgfault);
    // sys_exofork is a user-level system call
    envid = sys_exofork();

    if (envid < 0)
        panic("sys_exofork: %e", envid);
    if (envid == 0) {
        // We're the child.
        // The copied value of the global variable 'thisenv'
        // is no longer valid (it refers to the parent!).
        // Fix it and return 0.
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }

    // We're the parent.

    // Copy parent address space to child
    for (int i = 0; i < PGNUM(USTACKTOP); i++) {
        if ((uvpd[PDX(i * PGSIZE)] & PTE_P) && (uvpt[i] & PTE_P)) {
            if ((r = duppage(envid, i)) < 0)
                panic("fork: duppage: %e", r);
        }
    }

    // page fault handler setup in child, alloc user exception stack space first
    if ((r = sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), PTE_SYSCALL)) < 0)
        panic("fork: sys_page_alloc: %e", r);

    // page fault handler setup in child, set page fault upcall second
    if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
        panic("fork: sys_env_set_pgfault_upcall: %e", r);

    // Start the child environment running
    if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
        panic("fork: sys_env_set_status: %e", r);

    return envid;
}

// Challenge!
int
sfork(void)
{
//	panic("sfork not implemented");

    envid_t envid;
    uint8_t *addr;
    int r;

    set_pgfault_handler(pgfault);
    // sys_exofork is a user-level system call
    envid = sys_exofork();

    if (envid < 0)
        panic("sys_exofork: %e", envid);
    if (envid == 0) {
        // We're the child.
        // The copied value of the global variable 'thisenv'
        // is no longer valid (it refers to the parent!).
        // Fix it and return 0.
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }

    // We're the parent.

    // Copy parent address space to child
    int i;
    for (i = 0; i < PGNUM(USTACKTOP) - 1; i++) {
        if ((uvpd[PDX(i * PGSIZE)] & PTE_P) && (uvpt[i] & PTE_P) && (uvpt[i] & PTE_U)) {
            void *addr = (void *) (i * PGSIZE);
            if ((r = sys_page_map(thisenv->env_id, addr, envid, addr, PTE_P | PTE_U | PTE_W)) < 0) {
                panic("sys_page_map: %e", r);
            }
            if ((r = sys_page_map(thisenv->env_id, addr, thisenv->env_id, addr, PTE_P | PTE_U | PTE_W)) < 0) {
                panic("sys_page_map: %e", r);
            }
        }
    }

    if ((r = duppage(envid, i)) < 0)
        panic("fork: duppage: %e", r);

    // page fault handler setup in child, alloc user exception stack space first
    if ((r = sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), PTE_SYSCALL)) < 0)
        panic("fork: sys_page_alloc: %e", r);

    // page fault handler setup in child, set page fault upcall second
    if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
        panic("fork: sys_env_set_pgfault_upcall: %e", r);

    // Start the child environment running
    if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
        panic("fork: sys_env_set_status: %e", r);

    return envid;
}
