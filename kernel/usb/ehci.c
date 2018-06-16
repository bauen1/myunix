// TODO: implement, this currently just disables the ehci
#include <assert.h>
#include <stdint.h>

#include <heap.h>
#include <console.h>
#include <pci.h>
#include <vmm.h>

#include <usb/usb.h>

#define REG_CAP_LENGTH 0x00
#define REG_CAP_HCSPARAMS 0x04
#define REG_OP_PORTSC 0x44

typedef struct ehci_controller {
	uint32_t pcidevice;
	uintptr_t iobase;
	size_t iobase_size;
	uint8_t op_reg_offset;
} ehci_controller_t;

static uint8_t ehci_reg_readb(ehci_controller_t *hc, uintptr_t reg) {
	return *((volatile uint8_t *)(hc->iobase + reg));
}
static uint32_t ehci_reg_readl(ehci_controller_t *hc, uintptr_t reg) {
	uintptr_t v_addr = hc->iobase + reg;
	return *((volatile uint32_t *)v_addr);
}

static void ehci_reg_writel(ehci_controller_t *hc, uintptr_t reg, uint32_t v) {
	uintptr_t v_addr = hc->iobase + reg;
	*((volatile uint32_t *)(v_addr)) = v;
}

static void ehci_controller_init(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra) {
	(void)vendorid;
	(void)deviceid;
	(void)extra;

	ehci_controller_t *hc = (ehci_controller_t *)kcalloc(1, sizeof(ehci_controller_t));
	assert(hc != NULL);

	uintptr_t iobase = pci_config_readl(device, PCI_BAR0);
	printf("real iobase: 0x%x\n", iobase);
	iobase = iobase & ~0xFFC;
	pci_config_writel(device, PCI_BAR0, 0xFFFFFFFF);
	uint32_t iobase_size = pci_config_readl(device, PCI_BAR0);
	pci_config_writel(device, PCI_BAR0, iobase);
	iobase_size = ~(iobase_size & ~0x3) + 1;

	printf("iobase_size: 0x%x\n", iobase_size);
	printf("iobase: 0x%x (", iobase);
	hc->iobase = iobase & ~3;
	hc->iobase_size = iobase_size;

	if ((iobase & 1) == 1) {
		printf("IO mapped)\n");
		printf("not supported, disabling device\n");
		return;
	} else {
		printf("memory mapped)\n");
		for (uintptr_t i = 0; i < iobase_size; i+=PAGE_SIZE) {
			uintptr_t v_addr = hc->iobase + i;
			page_t old_page = get_page(get_table(v_addr, kernel_directory), v_addr);
			assert((old_page & PAGE_TABLE_PRESENT) == 0);
			map_direct_kernel(v_addr);
		}
	}

	hc->op_reg_offset = ehci_reg_readb(hc, REG_CAP_LENGTH);
	printf("op reg offset: 0x%x\n", hc->op_reg_offset);

	uint32_t hcsparams = ehci_reg_readl(hc, REG_CAP_HCSPARAMS);
	printf("hcsparams: 0x%x\n", hcsparams);
	unsigned int ports_count = hcsparams & 0xF;
	printf("ports: %u\n", ports_count);
	for (unsigned int port = 0; port < ports_count; port++) {
		uint32_t v = ehci_reg_readl(hc, hc->op_reg_offset + REG_OP_PORTSC + port*4);
		v |= 1<<13; // release the port
		ehci_reg_writel(hc, hc->op_reg_offset + REG_OP_PORTSC + port*4, v);
	}
}

static void ehci_scan_callback(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra) {
	if ((pci_config_readb(device, PCI_CLASS) == 0x0C) &&
		(pci_config_readb(device, PCI_SUBCLASS) == 0x03) &&
		(pci_config_readb(device, PCI_PROG_IF) == 0x20)) {

		printf("Found ehci controller (device: 0x%x vendorid: 0x%x deviceid: 0x%x)\n", device, vendorid, deviceid);
		ehci_controller_init(device, vendorid, deviceid, extra);
	}
}

void ehci_init() {
	pci_scan(ehci_scan_callback, NULL);
}
