// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/mmu.h>
#include <kern/pmap.h>

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
	{ "backtrace", "Display the call stack", mon_backtrace },
    { "showmappings", "Show the physical page mappings for virtual addresses", mon_showmappings},
    { "setpermission", "Set the permission of the physical page mapped by the given virtual address", mon_setperm},
    { "clearpermission", "Clear the permission of the physical page mapped by the given virtual address", mon_clearperm},
    { "changepermission", "Add the specified permission to the physical page mapped by the given virtual address", mon_changeperm},
    { "memdump", "Dump the contents between the virtual or physicl memory range.\n", mon_memdump},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
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
    uint32_t ebp, eip, args[5];
    uint32_t *crt_ebp = (uint32_t *) read_ebp();
    
    while((uint32_t) crt_ebp != 0x0)
    {
        ebp = *crt_ebp;
        eip = *(uint32_t *) ((uint32_t) (crt_ebp + 1));
        int i;
        for (i = 0; i < 5; i++)
        {
            args[i] = *(uint32_t *)((uint32_t) (crt_ebp + i + 2));
        }
        cprintf("ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n", (uint32_t) crt_ebp, eip, args[0], args[1], args[2], args[3], args[4]);
        struct Eipdebuginfo info;
        debuginfo_eip(eip, &info);
        cprintf("%s:%d: ", info.eip_file, info.eip_line);
        cprintf("%.*s", info.eip_fn_namelen, info.eip_fn_name);
        cprintf("+%d\n", eip - info.eip_fn_addr);
        crt_ebp = (uint32_t *) ebp;
    }
	return 0;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
    if (argc != 3)
    {
        cprintf("showmappings expects 3 arguments, %d given.\n", argc);
        return 0;
    }
    uintptr_t start = (uintptr_t)strtol(argv[1], NULL, 0);
    start = (uintptr_t) ROUNDDOWN((char *) start, PGSIZE);
    uintptr_t end = (uintptr_t)strtol(argv[2], NULL, 0);
    end = (uintptr_t) ROUNDDOWN((char *) end, PGSIZE);

    if (start > end)
    {
        cprintf("Invalid range.\n");
        return 0;
    }

    for (; start <= end; start += PGSIZE)
    {
        pte_t *start_pte = pgdir_walk(kern_pgdir, (void *)start, 0);
        if (!start_pte)
        {
            cprintf("Virtual address %x is not mapped to any physical page.\n", start);
            return 0;
        }
        cprintf("Virtual Address: %x, Physical Page Number: %x, Permissions: ", start, PGNUM(*start_pte));
        cprintf("---");
        if (*start_pte & PTE_D)
            cprintf("D");
        else
            cprintf("-");
        if (*start_pte & PTE_A)
            cprintf("A");
        else
            cprintf("-");
        cprintf("--");
        if (*start_pte & PTE_U)
            cprintf("U");
        else
            cprintf("-");
        if (*start_pte & PTE_W)
            cprintf("W");
        else
            cprintf("-");
        if (*start_pte & PTE_P)
            cprintf("P");
        else
            cprintf("-");
        cprintf("\n"); 
    } 
    return 0;
}

int
mon_setperm(int argc, char **argv, struct Trapframe *tf)
{
    if (argc != 3)
    {
        cprintf("setpermission expects 3 arguments, %d given.\n", argc);
        return 0;
    }
    if (argv[1][0] != '0' || argv[1][1] != 'x' || argv[2][0] != '0' || argv[2][1] != 'x')
    {
        cprintf("The arguments need to be hex numbers starting with 0x.\n");
        return 0;
    }
    uintptr_t va = (uintptr_t)strtol(argv[1], NULL, 0);
    int perm = strtol(argv[2], NULL, 0);
    if (perm < 0x0 || perm > 0xfff)
    {
        cprintf("Permission needs to be in range 0x0 and 0xfff.\n");
        return 0;
    }
    pte_t *pte = pgdir_walk(kern_pgdir, (void *)va, 0);
    if (!pte)
    {
        cprintf("Virtual address %s is not mapped to any physical page.\n", argv[1]);
        return 0;
    }

    *pte = (*pte) >> 12;
    *pte = (*pte) << 12;
    *pte |= perm;

    return 0;
}

int
mon_changeperm(int argc, char **argv, struct Trapframe *tf)
{
    if (argc != 3)
    {
        cprintf("changepermission expects 3 arguments, %d given.\n", argc);
        return 0;
    }
    if (argv[1][0] != '0' || argv[1][1] != 'x' || argv[2][0] != '0' || argv[2][1] != 'x')
    {
        cprintf("The arguments need to be hex numbers starting with 0x.\n");
        return 0;
    }
    uintptr_t va = (uintptr_t)strtol(argv[1], NULL, 0);
    int perm = strtol(argv[2], NULL, 0);
    if (perm < 0x0 || perm > 0xfff)
    {
        cprintf("Permission needs to be in range 0x0 and 0xfff.\n");
        return 0;
    }
    pte_t *pte = pgdir_walk(kern_pgdir, (void *)va, 0);
    if (!pte)
    {
        cprintf("Virtual address %s is not mapped to any physical page.\n", argv[1]);
        return 0;
    }

    *pte |= perm;
    return 0;
}

int
mon_clearperm(int argc, char **argv, struct Trapframe *tf)
{
    if (argc != 2)
    {
        cprintf("clearpermission expects 2 arguments, %d given.\n", argc);
        return 0;
    }
    if (argv[1][0] != '0' || argv[1][1] != 'x')
    {
        cprintf("The argument need to be hex numbers starting with 0x.\n");
        return 0;
    }
    uintptr_t va = (uintptr_t)strtol(argv[1], NULL, 0);
    pte_t *pte = pgdir_walk(kern_pgdir, (void *)va, 0);
    if (!pte)
    {
        cprintf("Virtual address %s is not mapped to any physical page.\n", argv[1]);
        return 0;
    }

    *pte = (*pte) >> 12;
    *pte = (*pte) << 12;
    return 0;
}
int
mon_memdump(int argc, char **argv, struct Trapframe *tf)
{
    if (argc != 4)
    {
        cprintf("memdump expects 4 arguments, %d given.\n", argc);
        cprintf("Usage: memdump low_addr high_addr is_physical. is_physical = 0 or 1\n");
        return 0;
    }
    if (argv[1][0] != '0' || argv[1][1] != 'x' || argv[2][0] != '0' || argv[2][1] != 'x')
    {
        cprintf("The arguments need to be hex numbers starting with 0x.\n");
        return 0;
    }
    char * low_addr = (char *)strtol(argv[1], NULL, 0);
    char * high_addr = (char *)strtol(argv[2], NULL, 0);
    int is_physical = (int) strtol(argv[3], NULL, 10);
    if (is_physical != 0 && is_physical != 1)
    {
        cprintf("is_physical must be 0 or 1.\n");
        return 0;
    }
    int low_number, high_number, low_offset, high_offset;
    if (is_physical)
    {
        low_addr = KADDR((physaddr_t)low_addr);
        high_addr = KADDR((physaddr_t)high_addr);
    }
    pte_t *low_pte = pgdir_walk(kern_pgdir, (void *)low_addr, 0);
    pte_t *high_pte = pgdir_walk(kern_pgdir, (void *)high_addr, 0);
    char *addr;
    
    for (addr = low_addr; addr <= high_addr; addr++)
    {
        pgdir_walk(kern_pgdir, (void *)addr, 1);
        cprintf("0x%x : 0x%x\n", addr, *addr);
    }
    return 0;
    
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
	for (i = 0; i < NCOMMANDS; i++) {
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

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
