#include <assert.h>
#include <stdbool.h>

#include <console.h>

#include <usb/ohci.h>
#include <usb/uhci.h>
#include <usb/usb.h>

void usb_init() {
	uhci_init();
	ohci_init();
}
