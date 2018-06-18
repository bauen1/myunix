// TODO: use a kernel task for async init
// TODO: use suspend on the ports to get interrupts when device connect
// FIXME: less magic numbers
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include <cpu.h>
#include <console.h>
#include <heap.h>
#include <irq.h>
#include <pci.h>
#include <pic.h>
#include <pit.h>
#include <pmm.h>
#include <string.h>
#include <vmm.h>

#include <usb/usb.h>
#include <usb/uhci.h>

#define MAX_QH 16
#define MAX_TD 128

#define REG_CMD       0x00
#define REG_STS       0x02
#define REG_INTR      0x04
#define REG_FRNUM     0x06
#define REG_FRBASEADD 0x08
#define REG_SOF       0x0C
#define REG_PORT1     0x10
#define REG_PORT2     0x12

#define CMD_START     0x0001
#define CMD_HCRESET   0x0002
#define CMD_GRESET    0x0004
#define CMD_GLOBAL_RESUME 0x0008
#define CMD_FORCE_GLOBAL_RESUME 0x0010
#define CMD_SWDBG     0x0020
#define CMD_CONFIG    0x0040
#define CND_MAXP      0x0080

#define PORT_CONNECT        0x0001
#define PORT_CONNECT_CHANGE 0x0002
#define PORT_ENABLE         0x0004
#define PORT_ENABLE_CHANGE  0x0008
#define PORT_LINE_STATUS    0x0030
#define PORT_RESUME_DETECT  0x0040
// reserved                 0x0080
#define PORT_LOWSPEED       0x0100
#define PORT_RESET          0x0200
// reserved

#define TD_PTR_TERMINATE 0x01
#define TD_PTR_QH        0x02
#define TD_PTR_DEPTH     0x04

#define TD_CTRL_BITSTUFF    (1<<17)
#define TD_CTRL_CRC_TIMEOUT (1<<18)
#define TD_CTRL_NAK         (1<<19)
#define TD_CTRL_BABBLE      (1<<20)
#define TD_CTRL_DATABUFFER  (1<<21)
#define TD_CTRL_STALLED     (1<<22)
#define TD_CTRL_ACTIVE      (1<<23)
#define TD_CTRL_LOWSPEED    (1<<26)

/* minimum alignment: 16 */
typedef struct uhci_td {
	volatile uint32_t link __attribute__((packed));
	volatile uint32_t control __attribute__((packed)); // this might be the only one needing volatile
	volatile uint32_t token __attribute__((packed));
	volatile uint32_t buffer __attribute__((packed));

	uint8_t active __attribute__((packed));
	void *td_next __attribute__((packed));
	uint8_t free_for_use[3] __attribute__((packed));
	volatile uint32_t free_for_use3 __attribute__((packed));
	volatile uint32_t free_for_use4 __attribute__((packed));
} __attribute__((packed)) __attribute__((aligned(16))) uhci_td_t;
static_assert(sizeof(uhci_td_t) % 16 == 0);

/* minimum alignment: 16 */
typedef struct uhci_qh {
	volatile uint32_t head __attribute__((packed));
	volatile uint32_t element __attribute__((packed)); // this might be the only one needing volatile

	usb_transfer_t *transfer __attribute__((packed));

	uint8_t active __attribute__((packed));
	uint8_t free[3] __attribute__((packed));

	void *prev __attribute__((packed));
	void *next __attribute__((packed));
	uint32_t free2[1] __attribute__((packed));
	uhci_td_t *td_head;
} __attribute__((packed)) __attribute__((aligned(16))) uhci_qh_t;
static_assert(sizeof(uhci_qh_t) % 16 == 0);

typedef struct uhci_controller {
	uint32_t pci_device;
	uint16_t iobase;
	uint16_t iobase_size;
	unsigned int port_count;
	uint32_t *framelist;
	uhci_qh_t *queue_heads;
	uhci_td_t *transfer_descriptors;
	uhci_qh_t *async_qhs;
} uhci_controller_t;

__attribute__((unused))
static uint32_t uhci_reg_readl(uhci_controller_t *hc, uint16_t reg) {
	return inl(hc->iobase + reg);
}

static void uhci_reg_writel(uhci_controller_t *hc, uint16_t reg, uint32_t data) {
	outl(hc->iobase + reg, data);
	iowait();
}

static uint16_t uhci_reg_readw(uhci_controller_t *hc, uint16_t reg) {
	return inw(hc->iobase + reg);
}

static void uhci_reg_writew(uhci_controller_t *hc, uint16_t reg, uint16_t data) {
	outw(hc->iobase + reg, data);
	iowait();
}

__attribute__((unused))
static uint8_t uhci_reg_readb(uhci_controller_t *hc, uint16_t reg) {
	return inb(hc->iobase + reg);
}

static void uhci_reg_writeb(uhci_controller_t *hc, uint16_t reg, uint8_t data) {
	outl(hc->iobase + reg, data);
	iowait();
}

static void uhci_port_set(uhci_controller_t *hc, uint16_t reg, uint16_t data) {
	uint16_t v = uhci_reg_readw(hc, reg);
	v |= data;
	v &= ~(PORT_CONNECT_CHANGE | PORT_ENABLE_CHANGE); // don't clear {connect status, port enable}-change registers
	uhci_reg_writew(hc, reg, v);
}

static void uhci_port_unset(uhci_controller_t *hc, uint16_t reg, uint16_t data) {
	uint16_t v = uhci_reg_readw(hc, reg);
	v &= ~(PORT_CONNECT_CHANGE | PORT_ENABLE_CHANGE);
	v &= ~data;
	v |= ((PORT_CONNECT_CHANGE | PORT_ENABLE_CHANGE) & data);
	uhci_reg_writew(hc, reg, v);
}

static uhci_td_t *uhci_alloc_td(struct uhci_controller *hc) {
	for (unsigned int i = 0; i < MAX_TD; i++) {
		if (!hc->transfer_descriptors[i].active) {
			uhci_td_t *td = &hc->transfer_descriptors[i];
			td->active = 1;
			return td;
		}
	}
	return NULL;
}

static uhci_qh_t *uhci_alloc_qh(struct uhci_controller *hc) {
	for (unsigned int i = 0; i < MAX_QH; i++) {
		if (!hc->queue_heads[i].active) {
			uhci_qh_t *qh = &hc->queue_heads[i];
			qh->active = 1;
			return qh;
		}
	}
	return NULL;
}

static void uhci_free_td(struct uhci_controller *hc, uhci_td_t *td) {
	(void)hc;
	td->active = 0;
}

static void uhci_free_qh(struct uhci_controller *hc, uhci_qh_t *qh) {
	(void)hc;
	qh->active = 0;
}

static void uhci_init_td(uhci_td_t *td, uhci_td_t *prev, unsigned int speed, uint8_t addr, uint8_t endpt, uint8_t data_toggle, uint8_t packet_type, uint16_t len, void *data) {
	assert((addr & ~0x7F) == 0);
	assert((endpt & ~0xF) == 0);
	assert((data_toggle & ~0x1) == 0);
	assert((packet_type == 0x69) || (packet_type == 0xE1) || (packet_type == 0x2D));

//	printf("uhci_init_td(td: 0x%8x, prev: 0x%8x, speed: 0x%x, addr: 0x%x, endpt: 0x%x, data_toggle: 0x%x, packet_type: 0x%x, len: 0x%x, data: 0x%x)\n", (uintptr_t)td, (uintptr_t)prev, speed, addr, endpt, data_toggle, packet_type, len, (uintptr_t)data);

	if (prev != NULL) {
		prev->link = (uint32_t)((uintptr_t)td | TD_PTR_DEPTH);
		prev->td_next = td;
	}

	td->link = TD_PTR_TERMINATE;
	td->td_next = NULL;

	td->control = (3 << 27) | TD_CTRL_ACTIVE;
	if (speed == 1) { // 1 = low speed
		td->control |= TD_CTRL_LOWSPEED;
	}

	td->token = (
		(len << 21) |
		(data_toggle << 19) |
		(endpt << 15) |
		(addr << 8)
	) | packet_type;

	td->buffer = (uint32_t)data;
}

static void uhci_init_qh(uhci_qh_t *qh, usb_transfer_t *transfer, uhci_td_t *td) {
	qh->td_head = td;
	qh->element = (uint32_t)td;
	qh->transfer = transfer;
}

// FIXME: needs to disable interrupts ?
static void uhci_insert_qh(struct uhci_controller *hc, uhci_qh_t *qh) {
	assert(hc != NULL);
	assert(hc->async_qhs != NULL);
	assert(hc->async_qhs->next == NULL);

	qh->head = TD_PTR_TERMINATE;

	uhci_qh_t *end = hc->async_qhs;
	end->next = qh;
	qh->prev = end;
	qh->next = NULL;

	hc->async_qhs = qh;
	assert(((uintptr_t)qh & 0xF) == 0);
	end->head = (uint32_t)qh | TD_PTR_QH;
}

// FIXME: needs to disable interrupts ?
static void uhci_remove_qh(struct uhci_controller *hc, uhci_qh_t *qh) {
	assert(hc != NULL);
	assert(qh != NULL);
	uhci_qh_t *prev = qh->prev;
	assert((prev != NULL) && "TRYING TO REMOVED HEAD");
	uhci_qh_t *next = qh->next;
	prev->head = qh->head;
	prev->next = next;
	if (next != NULL) {
		next->prev = prev;
	}
	if (hc->async_qhs == qh) {
		if (next != NULL) {
			hc->async_qhs = next;
		} else if (prev != NULL) {
			hc->async_qhs = prev;
		}
	}
}

static uint16_t uhci_reset_port(uhci_controller_t *hc, uint16_t port) {
	// FIXME: ack old status flags
	// FIXME: try disabling the port before the reset
	// FIXME: this works ... somehow
//	printf("resetting port: ");
	{
		uint16_t port_status = uhci_reg_readw(hc, port);
		port_status &= ~PORT_ENABLE;
		uhci_reg_writew(hc, port, port_status);
		do {
			port_status = uhci_reg_readw(hc, port);
			if ((port_status & PORT_ENABLE) == 0) {
				break;
			} else {
				_sleep(1);
			}
		} while (1);
	}

	uhci_port_set(hc, port, PORT_RESET);
	_sleep(50);
	uhci_port_unset(hc, port, PORT_RESET);
	while (1) {
		uint16_t port_status = uhci_reg_readw(hc, port);
		if (port_status & PORT_RESET) {
			_sleep(1);
//			printf(".");
		} else {
//			printf("reset finished\n");
			break;
		}
	}

	{
//		printf("trying to enable: \n");
		uint16_t port_status = uhci_reg_readw(hc, port);
		port_status |= PORT_ENABLE;
		uhci_reg_writew(hc, port, port_status);
		do {
			port_status = uhci_reg_readw(hc, port);
//			printf("status: 0x%x\n", port_status);
			if (port_status & PORT_CONNECT) {
//				printf("connected\n");
			} else {
				return port_status;
			}

			if (port_status & PORT_ENABLE) {
				return port_status;
			} else {
//				printf(".");
				_sleep(1);
			}
		} while (1);
	}

	return 0;
}

static void uhci_process_qh(struct uhci_controller *hc, uhci_qh_t *qh) {
	usb_transfer_t *transfer = qh->transfer;
	uhci_td_t *td = (uhci_td_t *)(qh->element & ~0xF);

	if (td == NULL) {
		transfer->success = true;
		transfer->complete = true;
	} else if (~td->control & TD_CTRL_ACTIVE) {
		// ERROR
		printf("td error ! (control: 0x%x)\n", td->control);
		if (td->control & TD_CTRL_NAK) {
			printf("NAK!\n");
		}

		if (td->control & TD_CTRL_STALLED) {
			printf("STALLED\n");
			transfer->success = false;
			transfer->complete = true;
		}

		if (td->control & TD_CTRL_DATABUFFER) {
			printf("databuffer!\n");
		}

		if (td->control & TD_CTRL_BABBLE) {
			printf("babble!\n");
		}

		if (td->control & TD_CTRL_CRC_TIMEOUT) {
			printf("crc timeout\n");
		}

		if (td->control & TD_CTRL_BITSTUFF) {
			printf("bitstuff!\n");
		}
		transfer->complete = true;
//		transfer->success = false;
	}

	if (transfer->complete) {
		qh->transfer = NULL;
		// FIXME: toggle endpoint state
		uhci_remove_qh(hc, qh);

		td = qh->td_head;
		while (td != NULL) {
			uhci_td_t *next = td->td_next;
			uhci_free_td(hc, td);
			td = next;
		}

		uhci_free_qh(hc, qh);
	}
}

static void uhci_wait_for_qh(struct uhci_controller *hc, uhci_qh_t *qh) {
	assert(qh != NULL);
	assert(qh->transfer != NULL);
	usb_transfer_t *transfer = qh->transfer;
	while (!transfer->complete) {
		uhci_process_qh(hc, qh);
		_sleep(1);
	}
}

void uhci_dev_control(void *_hc, usb_device_t *dev, usb_transfer_t *trans) {
	struct uhci_controller *hc = (struct uhci_controller *)_hc;
	usb_dev_req_t *req = trans->req;

	uint8_t speed = dev->speed;
	uint32_t addr = dev->addr;
	uint8_t endp = 0;
	uint32_t max_len = dev->max_packet_size;
	uint8_t type = req->type;
	uint32_t len = req->length;

	uhci_td_t *td = uhci_alloc_td(hc);
	assert(td != NULL);

	uhci_td_t *head = td;
	uhci_td_t *prev = NULL;

	uint8_t toggle = 0;
	uint8_t packet_type = 0x2D; // SETUP
	uint32_t packet_size = sizeof(usb_dev_req_t);
	uhci_init_td(td, prev, speed, addr, endp, toggle, packet_type, sizeof(usb_dev_req_t) - 1, req);
	prev = td;

	if (type & 0x80) { // dev to host
		packet_type = 0x69; // IN
	} else {
		packet_type = 0xE1; // OUT
	}

	uint8_t *it = (uint8_t *)trans->data;
	uint8_t *end = it + len;
	while (it < end) {
		td = uhci_alloc_td(hc);
		assert(td != NULL);

		toggle ^= 1;
		packet_size = end - it;
		if (packet_size > max_len) {
			packet_size = max_len;
		}
		uhci_init_td(td, prev, speed, addr, endp, toggle, packet_type, packet_size - 1, it);

		it += packet_size;
		prev = td;
	}

	// status packet (???)
/*	td = uhci_alloc_td(hc);
	assert(td != NULL);
	toggle ^= 1;
//	toggle = 1;
	if (type & 0x80) { // dev to host
		packet_type = 0xE1; // OUT
	} else {
		packet_type = 0x69; // IN
	}

	uhci_init_td(td, prev, speed, addr, endp, toggle, packet_type, 0x7FF, 0);
*/
	uhci_qh_t *qh = uhci_alloc_qh(hc);
	assert(qh != NULL);
	uhci_init_qh(qh, trans, head);

	uhci_insert_qh(hc, qh);
	uhci_wait_for_qh(hc, qh);
}

static unsigned int uhci_irq(unsigned int irq, void *extra) {
	assert(extra != NULL);
	irq_ack(irq);

	printf("uhci IRQ!\n");
	printf("uhci status: 0x%x\n", uhci_reg_readw((uhci_controller_t *)extra, REG_STS));
	assert(0);
}

static void uhci_probe_port(uhci_controller_t *hc, uint16_t port) {
	uint16_t status = uhci_reset_port(hc, port);
	printf("reset_port(%u): 0x%x\n", port, status);

	if (status & PORT_ENABLE) {
		usb_device_t *dev = kcalloc(1, sizeof(usb_device_t));
		dev->hc = hc;
		dev->hc_control = uhci_dev_control;
		// FIXME: usb_dev_t needs a way of knowing which port a usb device is attached to
		if (status & 0x80) { // low speed
			dev->speed = 1;
		} else {
			dev->speed = 0;
		}

		dev->max_packet_size = 8;

		if (!usb_dev_init(dev)) {
			printf("usb_dev_init failed\n");
			kfree(dev);
		} else {
			usb_add_dev(dev);
		}

	}
}

static unsigned int uhci_controller_count_ports(struct uhci_controller *hc) {
//	printf("uhci_controller_probe(hc->iobase: 0x%x; hc->iobase_size: 0x%x)\n", hc->iobase, hc->iobase_size);
//	printf("(usbbase_size - REG_PORT1) / 2: %u\n", (hc->iobase_size - REG_PORT1) / 2);
	for (unsigned int port = 0; port < (((unsigned)hc->iobase_size - REG_PORT1) / 2); port++) {
//		printf("probing port %u: ", port);
		uint16_t v = inw(hc->iobase + REG_PORT1 + port*2);
		if (!(v & 0x0080) || (v == 0xFFFF)) {
//			printf("not a port\n");
			if (port > 7) {
				printf("counted more than 7 ports, forcing to 2!!!\n");
				return 2;
			} else {
				return port;
			}
		} else {
//			printf("valid port\n");
		}
	}
	return 2;
}

// FIXME: fixed ?: FIXME: doesn't work on the old macbook
static void uhci_controller_probe(struct uhci_controller *hc) {
	hc->port_count = uhci_controller_count_ports(hc);
	printf("counted %u ports\n", hc->port_count);

	uhci_probe_port(hc, REG_PORT1);
	uhci_probe_port(hc, REG_PORT2);
}

static void uhci_controller_init(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra) {
	(void)vendorid;
	(void)deviceid;
	(void)extra;
	uint32_t usbbase = pci_config_readl(device, PCI_BAR4);
	uint8_t sbrn = pci_config_readb(device, 0x60);

	// FIXME: what if the controller hasn't been mapped into i/o space ?
	printf("usbbase: 0x%x\n", usbbase);
	if (!(usbbase & 1)) {
		printf("memory mapped I/O not supported, disabling device\n");
		return;
	}

	pci_config_writel(device, PCI_BAR4, 0xFFFFFFFF);
	uint32_t usbbase_size = pci_config_readl(device, PCI_BAR4);
	pci_config_writel(device, PCI_BAR4, usbbase);
	usbbase_size = ~(usbbase_size & ~0x3) + 1;
	printf("usbbase_size: 0x%x\n", usbbase_size);

	assert((usbbase & 0xFFFF0000) == 0);

	uint32_t cmd = pci_config_readl(device, 0x04);
	cmd |= 0x405; // this also disables interrupts ??
	cmd &= ~0x400;
	pci_config_writel(device, 0x04, cmd);

	if ((sbrn != 0x00) && (sbrn != 0x10)) {
		printf("sbrn: 0x%x\n", sbrn);
		printf("protocol not supported, disabling device\n");
		return;
	}

	uhci_controller_t *hc = (uhci_controller_t *)kcalloc(1, sizeof(struct uhci_controller));
	assert(hc != NULL);

	hc->pci_device = device;
	hc->iobase = usbbase & ~0x01;
	hc->iobase_size = usbbase_size;

	uint8_t irq = pci_config_readb(device, PCI_IRQ);
	if ((irq == 0) || (irq == 0xFF)) {
		// FIXME: find a free IRQ line
		irq = 9;
		uint32_t r = pci_config_readw(device, PCI_IRQ);
		r &= ~0xFF;
		r |= irq;
		pci_config_writel(device, PCI_IRQ, r);
	}
	printf("irq: 0x%x\n", irq);

	irq_set_handler(irq, uhci_irq, hc);
	// we currently don't use the IRQ, so disable it

	printf("host controller reset!\n");
	uhci_reg_writew(hc, REG_CMD, CMD_HCRESET);
	_sleep(5);
	while (1) {
		uint16_t v = uhci_reg_readw(hc, REG_CMD);
		if ((v & CMD_HCRESET) == 0) {
			printf("complete\n");
			break;
		}
		_sleep(5);
	}
	uhci_reg_writew(hc, REG_CMD, uhci_reg_readw(hc, REG_CMD) | CMD_FORCE_GLOBAL_RESUME);
	iowait();
	_sleep(20);
	uhci_reg_writew(hc, REG_CMD, uhci_reg_readw(hc, REG_CMD) & ~CMD_FORCE_GLOBAL_RESUME);


	uhci_reg_writew(hc, REG_CMD, uhci_reg_readw(hc, REG_CMD) | CMD_GLOBAL_RESUME);
	iowait();
	_sleep(50);
	uhci_reg_writew(hc, REG_CMD, uhci_reg_readw(hc, REG_CMD) & ~CMD_GLOBAL_RESUME);


	uhci_reg_writew(hc, REG_STS, 0x3F);


	uhci_reg_writew(hc, REG_INTR, 0); // disable interrupts

	// FIXME: handle the this better
	hc->framelist = dma_malloc(sizeof(uint32_t) * 1024); // minimum alignment: 4kb
	hc->queue_heads = dma_malloc(sizeof(uhci_qh_t) * MAX_QH);
	assert(hc->queue_heads != NULL);
	hc->transfer_descriptors = dma_malloc(sizeof(uhci_td_t) * MAX_TD);
	assert(hc->transfer_descriptors != NULL);
	hc->async_qhs = uhci_alloc_qh(hc);
	assert(hc->async_qhs != NULL);
	hc->async_qhs->head = TD_PTR_TERMINATE;
	hc->async_qhs->element = TD_PTR_TERMINATE;
	for (unsigned int i = 0; i < 1024; i++) {
		hc->framelist[i] = TD_PTR_QH | (uint32_t)hc->async_qhs;
	}

	__asm__ __volatile__("lock; addl $0,0(%%esp)" ::: "memory");

	// FIXME: disable legacy
	uhci_reg_writew(hc, 0xc0, 0x8f00);

	uhci_reg_writew(hc, REG_CMD, uhci_reg_readw(hc, REG_CMD) & ~CMD_START);
	uhci_reg_writel(hc, REG_FRBASEADD, (uintptr_t)hc->framelist);
	iowait();
	uhci_reg_writew(hc, REG_FRNUM, 0); // reset index to 0
	iowait();
	uhci_reg_writew(hc, REG_CMD, uhci_reg_readw(hc, REG_CMD) & ~CMD_START);
	

	uhci_reg_writeb(hc, REG_SOF, 0x40); // 12mhz timing
	iowait();
	uhci_reg_writew(hc, REG_STS, 0xFFFF); // reset status register
	iowait();
	uhci_reg_writeb(hc, REG_INTR, 0xF); // enable all available interrupts

	// enable controller here
	uhci_reg_writew(hc, REG_CMD, CMD_START | CMD_CONFIG );

	uhci_controller_probe(hc);
	printf("\n");
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
