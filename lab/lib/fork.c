// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
    //cprintf("envid: %x, eip: %x, fault_va: %x\n", thisenv->env_id, utf->utf_eip, addr);

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
    if (!(err & FEC_WR))
    {
        panic("pgfault: error is not a write. it is %e with falting addr 0x%x\n", err, addr);
    }

    if (!(uvpt[PGNUM(addr)] & PTE_COW))
    {
        panic("pgfault: page not COW\n");
    }

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
    if ((r = sys_page_alloc(0, PFTEMP, PTE_U|PTE_W|PTE_P)) < 0)
    {
        panic("pgfault: sys_page_alloc fail %e", r);
    }
    memmove(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);
    if ((r = sys_page_map(0, PFTEMP, 0, ROUNDDOWN(addr, PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)
    {
        panic("pgfault: sys_page_map: %e", r);
    }

    if ((r = sys_page_unmap(0, PFTEMP)) < 0)
    {
        panic("pgfault: sys_page_unmap: %e", r);
    }
}

//
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
    void* va = (void *)(pn << PGSHIFT);
    //cprintf("duplicating page number %x\n", va);
    // LAB 5
    if (((uvpd[PDX(va)]) & (PTE_P)) && ((uvpt[pn]) & (PTE_SHARE)))
    {
        if ((r = sys_page_map(0, va, envid, va, uvpt[pn] & PTE_SYSCALL)) < 0)
            panic("duppage: PTE_SHARE mapping error %e", r);
        return 0;
    }
    
    if ((uvpd[PDX(va)] & PTE_P) && (uvpt[pn] & PTE_P) && !(uvpt[pn] & (PTE_W|PTE_COW))) //read only
    {
        
        if ((r = sys_page_map(0, va, envid, va, (uvpt[pn] & PTE_SYSCALL))) < 0)
        {
            panic("what?");
            return r;
        }
        return 0;
    }

	// LAB 4: Your code here.
    if (!(uvpd[PDX(va)] & PTE_P) || !(uvpt[pn] & (PTE_W | PTE_COW)))
    {
        panic("page directory entry not present or pte not writable/COW\n");
    }
    if ((r = sys_page_map(0, va, envid, va, PTE_COW | PTE_P | PTE_U)) < 0)
    {
        panic("duppage: sys_map in child: %e", r);
    } 
    if ((r = sys_page_map(0, va, 0, va, PTE_P | PTE_COW | PTE_U)) < 0)
    {
        panic("duppage: sys_map : %e", r);
    } 
	return 0;
}

//
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
    envid_t envid;
    uint8_t *addr;
    uintptr_t va;
    int r;
	// LAB 4: Your code here.
	set_pgfault_handler(pgfault);
    envid = sys_exofork();
    if (envid < 0)
    {
        panic("sys_exofork: %d", envid);
    }
    if (envid == 0)
    {
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }

    // We are parent
    for(va = UTEXT; va < UTOP - PGSIZE; va += PGSIZE)
    {
        if ((uvpd[PDX(va)] & PTE_P) && (uvpt[PGNUM(va)] & PTE_P) && (uvpt[PGNUM(va)] & PTE_U))
        {
            if ((r = duppage(envid, PGNUM(va))) < 0)
                return r;
        }
    }
    // The user exception stack page
    if ((r = sys_page_alloc(envid, (void*)(UXSTACKTOP-PGSIZE), PTE_P|PTE_U|PTE_W) < 0))
    {
        panic("fork: sys_page_alloc: %e", r);
    }

    // Done mapping pages
    sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall);
    sys_env_set_status(envid, ENV_RUNNABLE);
    return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
