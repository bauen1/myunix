#include <assert.h>
#include <stdbool.h>

#include <console.h>
#include <list.h>
#include <module.h>
#include <string.h>
#include <heap.h>

#include <usb/ehci.h>
#include <usb/ohci.h>
#include <usb/uhci.h>
#include <usb/usb.h>

/* direction */
#define REQ_TYPE_DEV_TO_HOST 0x80
#define REQ_TYPE_HOST_TO_DEV 0x00

/* type */
#define REQ_TYPE_STANDARD 0x00
#define REQ_TYPE_CLASS 0x20
#define REQ_TYPE_VENDOR 0x40

/* receiver type */
#define REQ_TYPE_DEVICE 0x00
#define REQ_TYPE_INTERFACE 0x01
#define REQ_TYPE_ENDPOINT 0x02
#define REQ_TYPE_OTHER 0x03
// reserved


#define REQ_REQUEST_GET_STATUS 0x00
#define REQ_REQUEST_CLEAR_FEATURE 0x01
// reserved: 0x02
#define REQ_REQUEST_SET_FEATURE 0x03
// reserved: 0x04
#define REQ_REQUEST_SET_ADDRESS 0x05
#define REQ_REQUEST_GET_DESCRIPTOR 0x06
#define REQ_REQUEST_SET_DESCRIPTOR 0x07
#define REQ_REQUEST_GET_CONFIGURATION 0x08
#define REQ_REQUEST_SET_CONFIGURATION 0x09
#define REQ_REQUEST_GET_INTERFACE 0x0A
#define REQ_REQUEST_SET_INTERFACE 0x0B
#define REQ_REQUEST_SYNC_FRAME 0x0C

/* descriptors */
#define USB_DESC_DEVICE 0x01
#define USB_DESC_CONFIGURATION 0x02
#define USB_DESC_STRING 0x03
#define USB_DESC_INTERFACE 0x04
#define USB_DESC_ENDPOINT 0x05
#define USB_DESC_DEVICE_QUALIFIER 0x06
#define USB_DESC_OTHER_SPEED_CONFIGURATION 0x07
#define USB_DESC_INTERFACE_POWER 0x08

list_t *usb_devices;

void usb_add_dev(usb_device_t *dev) {
	assert(dev != NULL);
	printf("found usb device vendorid: 0x%4x productid: 0x%4x\n", dev->idVendor, dev->idProduct);
	list_insert(usb_devices, dev);
}

static bool usb_dev_req(usb_device_t *dev, uint8_t type, uint8_t request, uint16_t value, uint16_t index, uint16_t len, void *data) {
//	printf("usb_dev_req(dev: 0x%x, type: 0x%x, request: 0x%x, value: 0x%x, index: 0x%x, len: 0x%x, data: 0x%x)\n", (uintptr_t)dev, type, request, value, index, len, (uintptr_t)data);
	assert(dev != NULL);
	assert(data != NULL);

	// FIXME: assert's everywhere
	usb_dev_req_t req;
	memset(&req, 0, sizeof(usb_dev_req_t));
	usb_transfer_t trans;
	memset(&trans, 0, sizeof(usb_transfer_t));

	req.type = type;
	req.req = request;
	req.value = value;
	req.index = index;
	req.length = len;

	trans.req = &req;
	trans.data = data;
	trans.len = len;
	trans.complete = false;
	trans.success = false;

	dev->hc_control(dev->hc, dev, &trans);

	return trans.success;
}

bool usb_dev_init(usb_device_t *dev) {
	dev->max_packet_size = 8;


	usb_desc_device_t dev_desc;
	memset(&dev_desc, 0, sizeof(usb_desc_device_t));

	bool success = usb_dev_req(dev, REQ_TYPE_DEV_TO_HOST | REQ_TYPE_STANDARD | REQ_TYPE_DEVICE,
		REQ_REQUEST_GET_DESCRIPTOR, (USB_DESC_DEVICE << 8), 0, sizeof(usb_desc_device_t), &dev_desc);
	printf("success: ");
	if (success) {
		printf("true");
	} else {
		printf("false");
	}
	printf("\n");

	if (dev_desc.len != 0) {
		printf("dev.len: 0x%x\n", dev_desc.len);
		if (dev_desc.len != sizeof(usb_desc_device_t)) {
			printf("WARN: dev_desc.len doesn't match sizeof!\n");
			return false;
		}
		printf("supports USB version: 0x%x\n", dev_desc.bcdUSB);
		dev->bcdUSB = dev_desc.bcdUSB;
		/* according to: http://www.usb.org/developers/defined_class */
		printf("device class: ");
		switch (dev_desc.device_class) {
			case 0x03:
				printf("human interface device\n");
				break;
			case 0x08:
				printf("mass storage\n");
				break;
			case 0x09:
				printf("hub\n");
				break;
			default:
				printf("0x%x\n", dev_desc.device_class);
				break;
		}
		dev->device_class = dev_desc.device_class;
		printf("device subclass: 0x%x\n", dev_desc.device_subclass);
		dev->device_subclass = dev_desc.device_subclass;
		printf("dev.device_protocol: 0x%x\n", dev_desc.device_protocol);
		dev->device_protocol = dev_desc.device_protocol;
		printf("Maximum packet size: 0x%x\n", dev_desc.max_packet_size);
		if ((dev_desc.max_packet_size != 8) &&
			(dev_desc.max_packet_size != 16) &&
			(dev_desc.max_packet_size != 32) &&
			(dev_desc.max_packet_size != 64)) {
			printf("WARN: invalid max packet size ?!!\n");
			return false;
		}

		dev->max_packet_size = dev_desc.max_packet_size;
		printf("vendorid: 0x%x\n", dev_desc.idVendor);
		dev->idVendor = dev_desc.idVendor;
		printf("productid: 0x%x\n", dev_desc.idProduct);
		dev->idProduct = dev_desc.idProduct;
		printf("bcdDevice: 0x%x\n", dev_desc.bcdDevice);
		dev->bcdDevice = dev_desc.bcdDevice;
		printf("i_manufactor: 0x%x\n", dev_desc.i_manufactor);
		printf("i_product: 0x%x\n", dev_desc.i_product);
		printf("i_serial_number: 0x%x\n", dev_desc.i_serial_number),
		printf("i_configurations: 0x%x\n", dev_desc.i_configurations);
		dev->num_of_confs = dev_desc.i_configurations;
		dev->confs = kcalloc(1, sizeof(usb_conf_t *) * dev->num_of_confs);
		assert(dev->confs != NULL);

		uint8_t buf[4096];
		for (unsigned int i = 0; i < dev->num_of_confs; i++) {
			success = usb_dev_req(dev, REQ_TYPE_DEV_TO_HOST | REQ_TYPE_STANDARD | REQ_TYPE_DEVICE,
				REQ_REQUEST_GET_DESCRIPTOR, (USB_DESC_CONFIGURATION << 8) | i, 0, 4, &buf);
			if (!success) {
				printf("failed reading conf %u\n", i);
				continue;
			}

			usb_desc_conf_t *conf = (usb_desc_conf_t *)buf;
			printf("conf %u length: 0x%x\n", i, conf->length);
			if (conf->length != sizeof(usb_desc_conf_t)) {
				printf("WARN: sizes don't match!\n");
			}
			printf("conf %u total_length: 0x%x\n", i, conf->total_length);
			if (conf->total_length > sizeof(buf)) {
				// FIXME: this is probably a good reason to disable the device
				printf("length too big\n");
				continue;
			}
			// read all conf
			success = usb_dev_req(dev, REQ_TYPE_DEV_TO_HOST | REQ_TYPE_STANDARD | REQ_TYPE_DEVICE,
				REQ_REQUEST_GET_DESCRIPTOR, (USB_DESC_CONFIGURATION << 8) | i, 0, conf->total_length, &buf);
			if (!success) {
				printf("failed reading conf %u\n", i);
				continue;
			}

			dev->confs[i] = (usb_conf_t *)kcalloc(1, sizeof(usb_conf_t));
			assert(dev->confs[i] != NULL);
			printf("conf %u num_interfaces: 0x%x\n", i, conf->num_interfaces);
			dev->confs[i]->num_interfaces = conf->num_interfaces;
			printf("conf %u conf_value: 0x%x\n", i, conf->conf_value);
			dev->confs[i]->conf_value = conf->conf_value;
			printf("conf %u i_conf: 0x%x\n", i, conf->i_conf);
			dev->confs[i]->i_conf = conf->i_conf;
			printf("conf %u attributes: 0x%x\n", i, conf->attributes);
			dev->confs[i]->attributes = conf->attributes;
			printf("conf %u max_power: 0x%x\n", i, conf->max_power);
			dev->confs[i]->max_power = conf->max_power;

			uint8_t *data = (uint8_t *)(buf + conf->length);
			uint8_t *end = (uint8_t *)(buf + conf->total_length);

			usb_desc_interface_t *interface = NULL;
			usb_desc_endpoint_t *endpoint = NULL;
//			uint8_t interface_i = -1;
//			uint8_t endpoint_i = 0;
			while (data < end) {
				uint8_t len = data[0];
				uint8_t type = data[1];
				if (len < 2) {
					// FIXME: return false & free the lists
					printf("length 0 exit early!\n");
					break;
				}

				switch(type) {
					case USB_DESC_INTERFACE:
						interface = (usb_desc_interface_t *)data;
						printf("interface\n");
						if (len != sizeof(usb_desc_interface_t)) {
							printf("WARN: lengths don't match!\n");
						}
						printf("num: %u\n", interface->interface_num);
						printf("alternate setting: %u\n", interface->alternate_setting);
						printf("endpoints: %u\n", interface->num_endpoints);
						printf("interface class: ");
						switch (interface->interface_class) {
							case 0x03:
								printf("human input device\n");
								break;
							case 0x08:
								printf("mass storage\n");
								break;
							case 0x09:
								printf("hub\n");
								break;
							default:
								printf("0x%x\n", interface->interface_class);
								break;
						}

						printf("interface subclass: 0x%x\n", interface->interface_subclass);
						printf("interface protocol: 0x%x\n", interface->interface_protocol);
						break;
					case USB_DESC_ENDPOINT:
						endpoint = (usb_desc_endpoint_t *)data;
						printf("endpoint\n");
						if (len != sizeof(usb_desc_endpoint_t)) {
							printf("WARN: lengths don't match!\n");
						}
						printf("address: 0x%x\n", endpoint->endpoint_address);
						printf("attributes: 0x%x\n", endpoint->attributes);
						printf("max packet size: 0x%x\n", endpoint->max_packet_size);
						printf("interval: 0x%x\n", endpoint->bInterval);
						break;
					default:
						printf("type: 0x%x\n", type);
						break;
				}
				data += len;
			}
		}

		return true;
	} else {
		return false;
	}
}

void usb_init() {
	usb_devices = list_init();
	assert(usb_devices != NULL);
	ehci_init();
	uhci_init();
	ohci_init();
}

MODULE_INFO(usb, usb_init);
