#include <assert.h>
#include <stdint.h>

#include <console.h>
#include <cpu.h>
#include <pci.h>

void pci_config_writel(uint32_t device, uint8_t offset, uint32_t value) {
	assert((offset & 3) == 0);
	outl(PCI_CONFIG_ADDRESS, pci_address(device, offset));
	outl(PCI_CONFIG_DATA, value);
}

uint32_t pci_config_readl(uint32_t device, uint8_t offset) {
	assert((offset & 3) == 0);
	outl(PCI_CONFIG_ADDRESS, pci_address(device, offset));
	return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_readw(uint32_t device, uint8_t offset) {
	assert((offset & 1) == 0);
	outl(PCI_CONFIG_ADDRESS, pci_address(device, offset));
	return inw(PCI_CONFIG_DATA + (offset & 2));
}

uint8_t pci_config_readb(uint32_t device, uint8_t offset) {
	outl(PCI_CONFIG_ADDRESS, pci_address(device, offset));
	return inb(PCI_CONFIG_DATA + (offset & 3));
}

uint32_t pci_config_read(uint32_t device, uint8_t offset, int size) {
	switch(size) {
		case 4:
			return pci_config_readl(device, offset);
		case 2:
			return pci_config_readw(device, offset);
		case 1:
			return pci_config_readb(device, offset);
		default:
			assert(0); // This should never happen
	}
}

static void pci_scan_bus(pci_callback_t f, void *extra, uint8_t bus);
static void pci_scan_function(pci_callback_t f, void *extra, uint8_t bus, uint8_t slot, uint8_t func) {
	uint8_t device_class = pci_config_readb(pci_device(bus, slot, func), 0x0B);
	uint8_t device_subclass = pci_config_readb(pci_device(bus, slot, func), 0x0A);
	if ((device_class == 0x06) && (device_subclass == 0x04)) {
		// found a pci-pci bridge
		uint8_t secondary_bus = pci_config_readb(pci_device(bus, slot, func), 0x19);
		pci_scan_bus(f, extra, secondary_bus);
	}

	uint32_t device = pci_device(bus, slot, func);
	uint16_t vendorid = pci_config_readw(device, 0);
	uint16_t deviceid = pci_config_readw(device, 2);
	f(device, vendorid, deviceid, extra);
}

static void pci_scan_slot(pci_callback_t f, void *extra, uint8_t bus, uint8_t slot) {
	uint16_t vendorid = pci_config_readw(pci_device(bus, slot, 0), 0);
	if (vendorid == 0xFFFF) {
		return;
	}

	uint8_t header_type = pci_config_readb(pci_device(bus, slot, 0), 0xE);
	if ((header_type & 0x80) != 0) {
		// multi function device
		for (uint8_t func = 0; func < 8; func++) {
			if (pci_config_readw(pci_device(bus, slot, func), 0) != 0xFFFF) {
				pci_scan_function(f, extra, bus, slot, func);
			}
		}
	} else {
		pci_scan_function(f, extra, bus, slot, 0);
	}
}

static void pci_scan_bus(pci_callback_t f, void *extra, uint8_t bus) {
	for (uint8_t slot = 0; slot < 32; slot++) {
		pci_scan_slot(f, extra, bus, slot);
	}
}

void pci_scan(pci_callback_t f, void *extra) {
	uint8_t header_type = pci_config_readb(pci_device(0, 0, 0), 0xE);
	if ((header_type & 0x80) == 0) {
		pci_scan_bus(f, extra, 0);
	} else {
		for (uint8_t function = 0; function < 8; function++) {
			uint16_t vendorid = pci_config_readw(pci_device(0, 0, function), 0);
			if (vendorid == 0xFFFF) {
				break;
			}
			pci_scan_bus(f, extra, function);
		}
	}
}

static void pci_print(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra) {
	(void)extra;
	printf("[pci] device: 0x%6x vendorid: 0x%4x deviceid: 0x%4x\n", device, vendorid, deviceid);
}

void pci_print_all() {
	pci_scan(pci_print, NULL);
}
