/* See COPYRIGHT for copyright information. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/dwarf.h>

#include <kern/monitor.h>
#include <kern/console.h>
#include <kern/env.h>
#include <kern/trap.h>
#include <kern/sched.h>
#include <kern/cpu.h>
#include <kern/picirq.h>
#include <kern/kclock.h>

void load_debug_info(void);
void readsect(void*, uint32_t);
void readseg(uint32_t, uint32_t, uint32_t);

void
i386_init(void)
{
	extern char edata[], end[];

	// Before doing anything else, complete the ELF loading process.
	// Clear the uninitialized global data (BSS) section of our program.
	// This ensures that all static/global variables start out zero.
	memset(edata, 0, end - edata);

	// Initialize the console.
	// Can't call cprintf until after we do this!
	cons_init();

	cprintf("6828 decimal is %o octal!\n", 6828);
	cprintf("END: %p\n", end);

	// user environment initialization functions
	env_init();

	clock_idt_init();

	pic_init();
	rtc_init();
	// После инициализации часов RTC и программируемого контроллера прерываний PIC 
	// необходимо размаскировать на контроллере линию IRQ_CLOCK
	irq_setmask_8259A(irq_mask_8259A & ~(1 << IRQ_CLOCK));

	load_debug_info();

#ifdef CONFIG_KSPACE
	// Touch all you want.
	ENV_CREATE_KERNEL_TYPE(prog_test1);
	ENV_CREATE_KERNEL_TYPE(prog_test2);
	ENV_CREATE_KERNEL_TYPE(prog_test3);
	ENV_CREATE_KERNEL_TYPE(prog_test4);
#endif

	// Schedule and run the first user environment!
	sched_yield();
}


/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void
_panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	if (panicstr)
		goto dead;
	panicstr = fmt;

	// Be extra sure that the machine is in as reasonable state
	__asm __volatile("cli; cld");

	va_start(ap, fmt);
	cprintf("kernel panic at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);

dead:
	/* break into the kernel monitor */
	while (1)
		monitor(NULL);
}

/* like panic, but don't */
void
_warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}

#include <inc/elf.h>
#include <inc/x86.h>

#define SECTSIZE	512
#define ELFHDR		((struct Elf *) 0x10000) // scratch space
#define TEMPBUF         ((char *) 0x30000) // place for temporary buffers

const unsigned char *__DEBUG_ARANGES_BEGIN__;
const unsigned char *__DEBUG_ARANGES_END__;

const unsigned char *__DEBUG_ABBREV_BEGIN__;
const unsigned char *__DEBUG_ABBREV_END__;

const unsigned char *__DEBUG_INFO_BEGIN__;
const unsigned char *__DEBUG_INFO_END__;

const unsigned char *__DEBUG_LINE_BEGIN__;
const unsigned char *__DEBUG_LINE_END__;

const unsigned char *__DEBUG_STR_BEGIN__;
const unsigned char *__DEBUG_STR_END__;

const unsigned char *__DEBUG_PUBNAMES_BEGIN__;
const unsigned char *__DEBUG_PUBNAMES_END__;

const unsigned char *__DEBUG_PUBTYPES_BEGIN__;
const unsigned char *__DEBUG_PUBTYPES_END__;

void
load_debug_info()
{
	extern char edata[], end[];
	
	assert(ELFHDR->e_magic == ELF_MAGIC);

	struct Secthdr *bsh, *sh, *esh;

	char *buf = TEMPBUF;
	uint32_t tablesize = ELFHDR->e_shnum * ELFHDR->e_shentsize;
	readseg((uint32_t)buf, tablesize + SECTSIZE, ELFHDR->e_shoff);
	uint32_t offset = ELFHDR->e_shoff % SECTSIZE;

	bsh = sh = (struct Secthdr *)(buf + offset);
	esh = sh + ELFHDR->e_shnum;
	buf += (tablesize + offset);

	struct Secthdr *shstrtab_header = sh + ELFHDR->e_shstrndx;
	uint32_t shstrtab_offset = shstrtab_header->sh_offset;

	buf = ROUNDUP(buf, SECTSIZE);
	char *curraddr = end;
	for (sh = bsh; sh < esh; ++sh) {
		uint32_t name_offset = shstrtab_offset + sh->sh_name;
		readseg((uint32_t)buf, 14 + SECTSIZE, name_offset);
		offset = name_offset % SECTSIZE;
		if (strncmp(".debug_aranges", &buf[offset], 14) == 0) {
			curraddr = ROUNDUP(curraddr, SECTSIZE);
			__DEBUG_ARANGES_BEGIN__ = (unsigned char *)curraddr;
			readseg((uint32_t)curraddr, sh->sh_size + SECTSIZE, sh->sh_offset);
			offset = sh->sh_offset % SECTSIZE;
			memmove(curraddr, curraddr + offset, sh->sh_size);
			curraddr += sh->sh_size;
			__DEBUG_ARANGES_END__ = (unsigned char *)curraddr;
		}
		else if (strncmp(".debug_abbrev", &buf[offset], 13) == 0) {
			curraddr = ROUNDUP(curraddr, SECTSIZE);
			__DEBUG_ABBREV_BEGIN__ = (unsigned char *)curraddr;
			readseg((uint32_t)curraddr, sh->sh_size + SECTSIZE, sh->sh_offset);
			offset = sh->sh_offset % SECTSIZE;
			memmove(curraddr, curraddr + offset, sh->sh_size);
			curraddr += sh->sh_size;
			__DEBUG_ABBREV_END__ = (unsigned char *)curraddr;
		}
		else if (strncmp(".debug_info", &buf[offset], 12) == 0) {
			curraddr = ROUNDUP(curraddr, SECTSIZE);
			__DEBUG_INFO_BEGIN__ = (unsigned char *)curraddr;
			readseg((uint32_t)curraddr, sh->sh_size + SECTSIZE, sh->sh_offset);
			offset = sh->sh_offset % SECTSIZE;
			memmove(curraddr, curraddr + offset, sh->sh_size);
			curraddr += sh->sh_size;
			__DEBUG_INFO_END__ = (unsigned char *)curraddr;
		}
		else if (strncmp(".debug_line", &buf[offset], 12) == 0) {
			curraddr = ROUNDUP(curraddr, SECTSIZE);
			__DEBUG_LINE_BEGIN__ = (unsigned char *)curraddr;
			readseg((uint32_t)curraddr, sh->sh_size + SECTSIZE, sh->sh_offset);
			offset = sh->sh_offset % SECTSIZE;
			memmove(curraddr, curraddr + offset, sh->sh_size);
			curraddr += sh->sh_size;
			__DEBUG_LINE_END__ = (unsigned char *)curraddr;
		}
		else if (strncmp(".debug_str", &buf[offset], 11) == 0) {
			curraddr = ROUNDUP(curraddr, SECTSIZE);
			__DEBUG_STR_BEGIN__ = (unsigned char *)curraddr;
			readseg((uint32_t)curraddr, sh->sh_size + SECTSIZE, sh->sh_offset);
			offset = sh->sh_offset % SECTSIZE;
			memmove(curraddr, curraddr + offset, sh->sh_size);
			curraddr += sh->sh_size;
			__DEBUG_STR_END__ = (unsigned char *)curraddr;
		}
		else if (strncmp(".debug_pubnames", &buf[offset], 15) == 0) {
			curraddr = ROUNDUP(curraddr, SECTSIZE);
			__DEBUG_PUBNAMES_BEGIN__ = (unsigned char *)curraddr;
			readseg((uint32_t)curraddr, sh->sh_size + SECTSIZE, sh->sh_offset);
			offset = sh->sh_offset % SECTSIZE;
			memmove(curraddr, curraddr + offset, sh->sh_size);
			curraddr += sh->sh_size;
			__DEBUG_PUBNAMES_END__ = (unsigned char *)curraddr;
		}
		else if (strncmp(".debug_pubtypes", &buf[offset], 15) == 0) {
			curraddr = ROUNDUP(curraddr, SECTSIZE);
			__DEBUG_PUBTYPES_BEGIN__ = (unsigned char *)curraddr;
			readseg((uint32_t)curraddr, sh->sh_size + SECTSIZE, sh->sh_offset);
			offset = sh->sh_offset % SECTSIZE;
			memmove(curraddr, curraddr + offset, sh->sh_size);
			curraddr += sh->sh_size;
			__DEBUG_PUBTYPES_END__ = (unsigned char *)curraddr;
		}
	}
}

// Read 'count' bytes at 'offset' from kernel into physical address 'pa'.
// Might copy more than asked
void
readseg(uint32_t pa, uint32_t count, uint32_t offset)
{
	uint32_t end_pa;

	end_pa = pa + count;

	// round down to sector boundary
	pa &= ~(SECTSIZE - 1);

	// translate from bytes to sectors, and kernel starts at sector 1
	offset = (offset / SECTSIZE) + 1;

	// If this is too slow, we could read lots of sectors at a time.
	// We'd write more to memory than asked, but it doesn't matter --
	// we load in increasing order.
	while (pa < end_pa) {
		// Since we haven't enabled paging yet and we're using
		// an identity segment mapping (see boot.S), we can
		// use physical addresses directly.  This won't be the
		// case once JOS enables the MMU.
		readsect((uint8_t*) pa, offset);
		pa += SECTSIZE;
		offset++;
	}
}

void
waitdisk(void)
{
	// wait for disk ready
	while ((inb(0x1F7) & 0xC0) != 0x40)
		/* do nothing */;
}

void
readsect(void *dst, uint32_t offset)
{
	// wait for disk to be ready
	waitdisk();

	outb(0x1F2, 1);		// count = 1
	outb(0x1F3, offset);
	outb(0x1F4, offset >> 8);
	outb(0x1F5, offset >> 16);
	outb(0x1F6, (offset >> 24) | 0xE0);
	outb(0x1F7, 0x20);	// cmd 0x20 - read sectors

	// wait for disk to be ready
	waitdisk();

	// read a sector
	insl(0x1F0, dst, SECTSIZE/4);
}
