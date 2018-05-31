#include <assert.h>
#include <stdint.h>

#include <console.h>
#include <pci.h>

#include <usb/usb.h>

static void ohci_controller_init(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra) {
	(void)device;
	(void)vendorid;
	(void)deviceid;
	(void)extra;
	// TODO: implement
}

static void ohci_scan_callback(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra) {
	if ((pci_config_readb(device, PCI_CLASS) == 0x0C) &&
		(pci_config_readb(device, PCI_SUBCLASS) == 0x03) &&
		(pci_config_readb(device, PCI_PROG_IF) == 0x10)) {
		printf("Found OHCI controller (device: 0x%c vendorid: 0x%x deviceid: 0x%x)\n", device, vendorid, deviceid);
		ohci_controller_init(device, vendorid, deviceid, extra);
	}
}

void ohci_init() {
	pci_scan(ohci_scan_callback, NULL);
}
