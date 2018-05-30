#include <assert.h>
#include <stdbool.h>

#include <console.h>
#include <list.h>
#include <string.h>

#include <usb/ohci.h>
#include <usb/uhci.h>
#include <usb/usb.h>

list_t *usb_devices;

void usb_add_dev(usb_device_t *dev) {
	printf("Found USB device!\n");
	list_insert(usb_devices, dev);
}

bool usb_dev_init(usb_device_t *dev) {
	dev->max_packet_size = 8;
	usb_descriptor_t dev_desc;
	memset(&dev_desc, 0, sizeof(usb_descriptor_t));
	usb_dev_req_t req;
	memset(&req, 0, sizeof(usb_dev_req_t));
	usb_transfer_t trans;
	memset(&req, 0, sizeof(usb_transfer_t));

	req.type = 0x80;
	req.req = 0x06;
	req.value = (0x01 << 8) | 0;
	req.index = 0;
	req.length = 8;
	trans.req = &req;
	trans.data = &dev_desc;
	trans.len = 8;
	trans.complete = false;
	trans.success = false;

	dev->hc_control(dev->hc, dev, &trans);

	printf("\n");
	printf("dev.len: 0x%x\n", dev_desc.len);
	printf("dev.type: 0x%x\n", dev_desc.type);
	printf("dev.bcdUSB: 0x%x\n", dev_desc.bcdUSB);
	printf("dev.device_class: 0x%x\n", dev_desc.device_class);
	printf("dev.device_subclass: 0x%x\n", dev_desc.device_subclass);
	printf("dev.device_protocol: 0x%x\n", dev_desc.device_protocol);
	printf("dev.device_max_packet_size: 0x%x\n", dev_desc.max_packet_size);

	dev->max_packet_size = dev_desc.max_packet_size;

	if (dev_desc.len != 0) {
		return true;
	} else {
		return false;
	}
}

void usb_init() {
	usb_devices = list_init();
	assert(usb_devices != NULL);
	uhci_init();
	ohci_init();
}
