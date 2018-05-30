#ifndef USB_H
#define USB_H 1

#include <stdint.h>
#include <stdbool.h>

#include <list.h>

typedef struct usb_dev_req {
	uint8_t type __attribute__((packed));
	uint8_t req __attribute__((packed));
	uint16_t value __attribute__((packed));
	uint16_t index __attribute__((packed));
	uint16_t length __attribute__((packed));
} __attribute__((packed)) usb_dev_req_t;

typedef struct usb_transfer {
	usb_dev_req_t *req;
	void *data;
	uint32_t len;
	unsigned int complete;
	unsigned int success;
} usb_transfer_t;

typedef struct usb_device {
	uint8_t speed;
	uint32_t addr;
	uint32_t max_packet_size;
	void *hc; // host-controller

	void (*hc_control)(void *hc, struct usb_device *dev, usb_transfer_t *transfer);
} usb_device_t;

typedef struct usb_descriptor {
	uint8_t len __attribute__((packed));
	uint8_t type __attribute__((packed));
	uint16_t bcdUSB __attribute__((packed));
	uint8_t device_class __attribute__((packed));
	uint8_t device_subclass __attribute__((packed));
	uint8_t device_protocol __attribute__((packed));
	uint8_t max_packet_size __attribute__((packed));
	uint16_t idVendor __attribute__((packed));
	uint16_t idProduct __attribute__((packed));
	uint16_t bcdDevice __attribute__((packed));
	uint8_t i_manufactor __attribute__((packed));
	uint8_t i_product __attribute__((packed));
	uint8_t i_serial_number __attribute__((packed));
	uint8_t i_configurations __attribute__((packed));
} __attribute__((packed)) usb_descriptor_t;

extern list_t *usb_devices;

bool usb_dev_init(usb_device_t *dev);
void usb_add_dev(usb_device_t *dev);

void usb_init();

#endif
