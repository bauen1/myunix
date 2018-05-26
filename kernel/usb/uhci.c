// TODO: use a kernel task for async init
// TODO: use suspend on the ports to get interrupts when device connect
#include <assert.h>

#include <console.h>
#include <pci.h>

#include <usb/usb.h>
#include <usb/uhci.h>

static void uhci_controller_init(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra) {
	(void)device;
	(void)vendorid;
	(void)deviceid;
	(void)extra;
}

void uhci_scan_callback(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra) {
	if ((pci_config_readb(device, PCI_CLASS) == 0x0C) &&
		(pci_config_readb(device, PCI_SUBCLASS) == 0x03) &&
		(pci_config_readb(device, PCI_PROG_IF) == 0x00)) {

		printf("Found UHCI controller (device: 0x%x vendorid: 0x%x deviceid: 0x%x)\n", device, vendorid, deviceid);
		uhci_controller_init(device, vendorid, deviceid, extra);
	}
}



void uhci_init() {
	pci_scan(uhci_scan_callback, NULL);
}
