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

typedef struct usb_endpoint {
	uint8_t endpt_addr;
	uint8_t attributes;
	uint16_t max_packet_size; // FIXME: assert that reserved bits are set to 0, etc...
	uint8_t interval;
} usb_endpoint_t;

typedef struct usb_interface {
	uint8_t interface_index; // index in the configuration array (FIXME: assert that this is true everywhere)
	uint8_t alternative;
	uint8_t num_endpoints;
	uint8_t interface_class;
	uint8_t interface_subclass;
	uint8_t interface_protocol;

	usb_endpoint_t **endpoints;
} usb_interface_t;

typedef struct usb_conf {
	uint8_t num_interfaces;
	uint8_t conf_value;
	uint8_t i_conf;
	uint8_t attributes;
	uint8_t max_power;

	usb_interface_t **interfaces;
} usb_conf_t;

typedef struct usb_device {
	uint8_t speed;
	uint32_t addr;
	uint8_t max_packet_size;
	uint16_t bcdUSB;
	uint8_t device_class;
	uint8_t device_subclass;
	uint8_t device_protocol;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t num_of_confs;
	usb_conf_t **confs;

	void *hc; // host-controller

	void (*hc_control)(void *hc, struct usb_device *dev, usb_transfer_t *transfer);
} usb_device_t;

typedef struct usb_desc_device {
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
} __attribute__((packed)) usb_desc_device_t;

typedef struct usb_desc_conf {
	uint8_t length __attribute__((packed));
	uint8_t type __attribute__((packed));
	uint16_t total_length __attribute__((packed));
	uint8_t num_interfaces __attribute__((packed));
	uint8_t conf_value __attribute__((packed));
	uint8_t i_conf __attribute__((packed));
	uint8_t attributes __attribute__((packed));
	uint8_t max_power __attribute__((packed));
} __attribute__((packed)) usb_desc_conf_t;

typedef struct usb_desc_interface {
	uint8_t length __attribute__((packed));
	uint8_t type __attribute__((packed));
	uint8_t interface_num __attribute__((packed));
	uint8_t alternate_setting __attribute__((packed));
	uint8_t num_endpoints __attribute__((packed));
	uint8_t interface_class __attribute__((packed));
	uint8_t interface_subclass __attribute__((packed));
	uint8_t interface_protocol __attribute__((packed));
	uint8_t i_interface __attribute__((packed));
} __attribute__((packed)) usb_desc_interface_t;

typedef struct usb_desc_endpoint {
	uint8_t length __attribute__((packed));
	uint8_t type __attribute__((packed));
	uint8_t endpoint_address __attribute__((packed));
	uint8_t attributes __attribute__((packed));
	uint16_t max_packet_size __attribute__((packed));
	uint8_t bInterval __attribute__((packed));
} __attribute__((packed)) usb_desc_endpoint_t;

extern list_t *usb_devices;

bool usb_dev_init(usb_device_t *dev);
void usb_add_dev(usb_device_t *dev);

void usb_init();

#endif
