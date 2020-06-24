#include <inc/types.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/stdio.h>
#include <inc/x86.h>
#include <kern/timer.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/trap.h>

#define kilo (1000ULL)
#define Mega (kilo * kilo)
#define Giga (kilo * Mega)
#define Tera (kilo * Giga)
#define Peta (kilo * Tera)
#define ULONG_MAX ~0UL

#ifndef KADDR
// Hint: You will have to rework this in future labs!
#define KADDR(pa) ((void *)(uintptr_t)(pa))
static void *mmio_map_region(physaddr_t pa, size_t size)
{
	(void) size;
	return (void *) pa;
}
#endif

struct Timer timertab[MAX_TIMERS];
struct Timer *timer_for_schedule;

struct Timer timer_hpet0 = {
	.timer_name = "hpet0",
	.timer_init = hpet_init,
	.get_cpu_freq =	hpet_cpu_frequency,
	.enable_interrupts = hpet_enable_interrupts_tim0,
	.handle_interrupts = hpet_handle_interrupts_tim0,
};

struct Timer timer_hpet1 = {
	.timer_name = "hpet1",
	.timer_init = hpet_init,
	.get_cpu_freq =	hpet_cpu_frequency,
	.enable_interrupts = hpet_enable_interrupts_tim1,
	.handle_interrupts = hpet_handle_interrupts_tim1,
};

struct Timer timer_acpipm = {
	.timer_name = "pm",
	.timer_init = acpi_enable,
	.get_cpu_freq = pmtimer_cpu_frequency,
};

static uint32_t kfadt;
static uint32_t krsdp;
static uint32_t khpet;

bool check_sum(void *Table, int type) {
	int sum = 0;
	uint32_t len = 0;
	switch (type) {
		case 0:
			len = ((struct RSDPDescriptor *)Table)->Length;
			break;
		case 1:
			len = ((struct ACPISDTHeader *)Table)->Length;
			break;
		default:
			break;
	}
	for (int i = 0; i < len; i++)
		sum += ((char *)Table)[i];
	if (sum % 0x100 == 0)
		return 1;
	return 0;
}

void acpi_enable(void) {
	struct FADT *fadt = (struct FADT *)get_fadt();
	outb(fadt->SMI_CommandPort,fadt->AcpiEnable);
	while ((inw(fadt->PM1aControlBlock) & 1) == 0) {}
}

// LAB5: Your code here.
// Find the RSDP ACPI table address. Consult to ACPI specification
// on where to find it. Use KADDR macro to access physical memory,
// as you will have to do physical address mapping in the future.
uint32_t get_rsdp(void) {
	if (krsdp != 0)
		return krsdp;

	return krsdp;
}

// Finding FADT table.
uint32_t get_fadt(void) {
	if (kfadt != 0)
		return kfadt;

	struct RSDPDescriptor *Rsdp_desc = (struct RSDPDescriptor *)get_rsdp();
	struct RSDT *rsdt = Rsdp_desc->Revision == 0 ? KADDR(Rsdp_desc->RsdtAddress) : KADDR(Rsdp_desc->XsdtAddress);

	int entries = (rsdt->h.Length - sizeof(rsdt->h)) / 4;
	if (!check_sum(&(rsdt->h), 1))
		panic("Bad RSDT header\n");

	for (int i = 0; i < entries; i++) {
		struct ACPISDTHeader *head = KADDR(rsdt->PointerToOtherSDT[i]);
		if (!strncmp(head->Signature, "FACP", 4) && check_sum(head, 1)) {
			kfadt = (uint32_t) head;
			break;
		}
	}

	if (!kfadt)
		panic("No FADT\n");

	return kfadt;
}

// LAB5: Your code here.
// Find the RSDP ACPI table address.
uint32_t get_hpet(void) {
	if (khpet != 0)
		return khpet;

	return khpet;
}

// Getting physical HPET timer address from its table.
uint32_t hpet_address(void) {
	struct HPET *hpet_timer = (struct HPET *)get_hpet();
	return (hpet_timer->address).address;
}

// Debug HPET timer state.
void hpet_print_struct (void) {
	struct HPET *hpet = (struct HPET *)get_hpet();
	cprintf("signature = %s\n", (hpet->h).Signature);
	cprintf("length = %08x\n", (hpet->h).Length);
	cprintf("revision = %08x\n", (hpet->h).Revision);
	cprintf("checksum = %08x\n", (hpet->h).Checksum);

	cprintf("oem_revision = %08x\n", (hpet->h).OEMRevision);
	cprintf("creator_id = %08x\n", (hpet->h).CreatorID);
	cprintf("creator_revision = %08x\n", (hpet->h).CreatorRevision);

	cprintf("hardware_rev_id = %08x\n", hpet->hardware_rev_id);
	cprintf("comparator_count = %08x\n", hpet->comparator_count);
	cprintf("counter_size = %08x\n", hpet->counter_size);
	cprintf("reserved = %08x\n", hpet->reserved);
	cprintf("legacy_replacement = %08x\n", hpet->legacy_replacement);
	cprintf("pci_vendor_id = %08x\n", hpet->pci_vendor_id);
	cprintf("hpet_number = %08x\n", hpet->hpet_number);
	cprintf("minimum_tick = %08x\n", hpet->minimum_tick);

	cprintf("address_structure:\n");
	cprintf("address_space_id = %08x\n", (hpet->address).address_space_id);
	cprintf("register_bit_width = %08x\n", (hpet->address).register_bit_width);
	cprintf("register_bit_offset = %08x\n", (hpet->address).register_bit_offset);
	cprintf("address = %08llx\n", (hpet->address).address);
}

static volatile struct HPETRegister *hpetReg;
// HPET timer MMIO region virtual address
static uint32_t hpetAddr = 0;
// HPET timer MMIO region physical address
static uint32_t hpetAddrp = 0;
// HPET timer period (in femtoseconds)
static uint64_t hpetFemto = 0;
// HPET timer frequency
static uint64_t hpetFreq = 0;

// HPET timer initialisation
void hpet_init() {
	if (hpetAddrp == 0) {
		nmi_disable();
		hpetAddrp = (uint32_t)hpet_address();
		hpetAddr = (uint32_t)mmio_map_region(hpetAddrp, sizeof(struct HPETRegister));
		// cprintf("HPET: vaddr = %08x, paddr = %08x\n", hpetAddr , hpetAddrp);
		hpetReg = (struct HPETRegister *)(hpetAddr);

		hpetFemto = (uint32_t)(hpetReg->GCAP_ID >> 32);
		// cprintf("hpetFemto = %llu\n", hpetFemto);
		hpetFreq = (1 * Peta) / hpetFemto;
		// cprintf("HPET: Frequency = %d.%03dMHz\n", (uint32_t)(hpetFreq / Mega), (uint32_t)(hpetFreq % Mega));
		// Enable ENABLE_CNF bit to enable timer.
		hpetReg->GEN_CONF |= 1;
		nmi_enable();
	}
}

// HPET register contents debugging.
void hpet_print_reg(void) {
	cprintf("GCAP_ID = %016llx\n", hpetReg->GCAP_ID);
	cprintf("GEN_CONF = %016llx\n", hpetReg->GEN_CONF);
	cprintf("GINTR_STA = %016llx\n", hpetReg->GINTR_STA);
	cprintf("MAIN_CNT = %016llx\n", hpetReg->MAIN_CNT);
	cprintf("TIM0_CONF = %016llx\n", hpetReg->TIM0_CONF);
	cprintf("TIM0_COMP = %016llx\n", hpetReg->TIM0_COMP);
	cprintf("TIM0_FSB = %016llx\n", hpetReg->TIM0_FSB);
	cprintf("TIM1_CONF = %016llx\n", hpetReg->TIM1_CONF);
	cprintf("TIM1_COMP = %016llx\n", hpetReg->TIM1_COMP);
	cprintf("TIM1_FSB = %016llx\n", hpetReg->TIM1_FSB);
	cprintf("TIM2_CONF = %016llx\n", hpetReg->TIM2_CONF);
	cprintf("TIM2_COMP = %016llx\n", hpetReg->TIM2_COMP);
	cprintf("TIM2_FSB = %016llx\n", hpetReg->TIM2_FSB);
}

// HPET main timer counter value.
uint64_t hpet_get_main_cnt(void) {
	return hpetReg->MAIN_CNT;
}

// LAB5: Your code here.
// - Configure HPET timer 0 to trigger every 0.5 seconds
// on IRQ_TIMER line.
// - Configure HPET timer 0 to trigger every 1.5 seconds
// on IRQ_CLOCK line.
// Hint: to be able to use HPET as PIT replacement consult
// to LegacyReplacement functionality in HPET spec.

void hpet_enable_interrupts_tim0(void) {

}

void hpet_enable_interrupts_tim1(void) {

}

void hpet_handle_interrupts_tim0(void) {

}

void hpet_handle_interrupts_tim1(void) {

}

// LAB5: Your code here.
// Calculate CPU frequency in Hz with the help with HPET timer.
// Hint: use hpet_get_main_cnt function and do not forget about
// about pause instruction.
uint64_t hpet_cpu_frequency(void) {
	static uint64_t hpet_cpu_freq;
	if (hpet_cpu_freq != 0) {
		return hpet_cpu_freq;
	}

	return hpet_cpu_freq;
}

uint32_t pmtimer_get_timeval(void) {
	struct FADT *fadt = (struct FADT *)get_fadt();
	return inl(fadt->PMTimerBlock);
}

#define PM_FREQ 3579545

// LAB5: Your code here.
// Calculate CPU frequency in Hz with the help with ACPI PowerManagement timer.
// Hint: use pmtimer_get_timeval function and do not forget that ACPI PM timer
// can be 24-bit or 32-bit.
uint64_t pmtimer_cpu_frequency(void)
{
	static uint64_t pm_freq;
	if (pm_freq != 0) {
		return pm_freq;
	}

	return pm_freq;
}
