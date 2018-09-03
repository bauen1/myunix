#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <mmio.h>
#include <console.h>
#include <cpu.h>
#include <heap.h>
#include <irq.h>
#include <list.h>
#include <module.h>
#include <net/e1000.h>
#include <net/net.h>
#include <pci.h>
#include <pit.h>
#include <process.h>
#include <string.h>
#include <vmm.h>

// FIXME: way too much magic numbers

enum e1000_reg {
	E1000_REG_CTRL = 0x0,
	E1000_REG_STATUS = 0x8,

	E1000_REG_INTERRUPT_CAUSE_READ = 0xc0,

	E1000_REG_TX_CTRL        = 0x0400,
	E1000_REG_TX_DESC_LOW    = 0x3800,
	E1000_REG_TX_DESC_HIGH   = 0x3804,
	E1000_REG_TX_DESC_LENGTH = 0x3808,
	E1000_REG_TX_DESC_HEAD   = 0x3810,
	E1000_REG_TX_DESC_TAIL   = 0x3818,

	E1000_REG_RX_CTRL        = 0x0100,
	E1000_REG_RX_DESC_LOW    = 0x2800,
	E1000_REG_RX_DESC_HIGH   = 0x2804,
	E1000_REG_RX_DESC_LENGTH = 0x2808,
	E1000_REG_RX_DESC_HEAD   = 0x2810,
	E1000_REG_RX_DESC_TAIL   = 0x2818,

	E1000_REG_EEPROM = 0x0014,
};

#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 8

typedef struct e1000_rx_desc {
	volatile uint64_t addr __attribute__((packed));
	volatile uint16_t length __attribute__((packed));
	volatile uint16_t checksum __attribute__((packed));
	volatile uint8_t status __attribute__((packed));
	volatile uint8_t error __attribute__((packed));
	volatile uint16_t special __attribute__((packed));
} __attribute__((packed)) e1000_rx_desc_t;
static_assert(sizeof(e1000_rx_desc_t) % 16 == 0);

typedef struct e1000_tx_desc {
	volatile uint64_t addr __attribute__((packed));
	volatile uint16_t length __attribute__((packed));
	volatile uint8_t cso __attribute__((packed));
	volatile uint8_t cmd __attribute__((packed));
	volatile uint8_t status __attribute__((packed));
	volatile uint8_t css __attribute__((packed));
	volatile uint16_t special __attribute__((packed));
} __attribute__((packed)) e1000_tx_desc_t;
static_assert(sizeof(e1000_tx_desc_t) % 16 == 0);

typedef struct e1000 {
	uint32_t pcidevice;
	uintptr_t iobase;
	uintptr_t iobase_size;
	bool eeprom_exists;
	uint8_t mac[6];
	e1000_rx_desc_t *rx;
	e1000_tx_desc_t *tx;
	list_t *rx_queue;
	list_t *tx_queue;
} e1000_t;

/* helpers */
static void e1000_cmd_writel(e1000_t *e1000, volatile enum e1000_reg address, uint32_t value) {
	mmio_write32(e1000->iobase + address, value);
}

static uint32_t e1000_cmd_readl(e1000_t *e1000, volatile enum e1000_reg address) {
	return  mmio_read32(e1000->iobase + address);
}

static bool e1000_has_eeprom(e1000_t *e1000) {
	e1000_cmd_writel(e1000, E1000_REG_EEPROM, 0x01);
	bool has_eeprom = false;
	for (unsigned int i = 0; i < 10000 && !has_eeprom; i++) {
		uint32_t v = e1000_cmd_readl(e1000, E1000_REG_EEPROM);
		if (v & 0x10) {
			has_eeprom = true;
		} else {
			has_eeprom = false;
		}
	}
	return has_eeprom;
}

static uint16_t e1000_eeprom_readw(e1000_t *e1000, uint16_t addr) {
	assert(e1000->eeprom_exists == true);
	uint32_t tmp = 0;
	e1000_cmd_writel(e1000, E1000_REG_EEPROM, 1 | ((addr) << 8));
	do {
		tmp = e1000_cmd_readl(e1000, E1000_REG_EEPROM);
	} while (!(tmp & (1 << 4)));
	return (uint16_t)((tmp >> 16) & 0xFFFF);

}

static void e1000_read_mac(e1000_t *e1000) {
	assert(e1000->eeprom_exists == true);
	uint32_t tmp = e1000_eeprom_readw(e1000, 0);
	e1000->mac[0] = tmp & 0xFF;
	e1000->mac[1] = (tmp >> 8) & 0xFF;
	tmp = e1000_eeprom_readw(e1000, 1);
	e1000->mac[2] = tmp & 0xFF;
	e1000->mac[3] = (tmp >> 8) & 0xFF;
	tmp = e1000_eeprom_readw(e1000, 2);
	e1000->mac[4] = tmp & 0xFF;
	e1000->mac[5] = (tmp >> 8) & 0xFF;
}

static void e1000_init_tx(e1000_t *e1000) {
	e1000->tx_queue = list_init();
	assert(e1000->tx_queue != NULL);

	size_t size = E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t);
	e1000->tx = dma_malloc(size);
	printf("init tx: 0x%x size: 0x%x\n", (uintptr_t)e1000->tx, (uintptr_t)size);
	assert((void *)e1000->tx != NULL);
	assert((((uintptr_t)e1000->tx) & 0xF) == 0);

	for (unsigned int i = 0; i < E1000_NUM_RX_DESC; i++) {
		uintptr_t buf = (uintptr_t)dma_malloc(4096);
		assert(buf != 0);
		e1000->tx[i].addr = buf;
	}

	e1000_cmd_writel(e1000, E1000_REG_TX_DESC_LOW,  (uint32_t)e1000->tx);
	e1000_cmd_writel(e1000, E1000_REG_TX_DESC_HIGH, 0);

	e1000_cmd_writel(e1000, E1000_REG_TX_DESC_LENGTH, size);

	e1000_cmd_writel(e1000, E1000_REG_TX_DESC_HEAD, 0);
	e1000_cmd_writel(e1000, E1000_REG_TX_DESC_TAIL, 0);
	e1000_cmd_writel(e1000, E1000_REG_TX_CTRL,
		(1<<1) |
		(1<<3) |
		(1<<8) |
		(1<<18) | e1000_cmd_readl(e1000, E1000_REG_TX_CTRL));

}

static void e1000_init_rx(e1000_t *e1000) {
	e1000->rx_queue = list_init();
	assert(e1000->rx_queue != NULL);

	size_t size = E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t);
	assert((size % 128) == 0);
	e1000->rx = dma_malloc(size);
	printf("init rx: 0x%x size: 0x%x\n", (uintptr_t)e1000->rx, (uintptr_t)size);
	assert(e1000->rx != NULL);
	assert((((uintptr_t)e1000->rx) & 0xF) == 0);

	for (unsigned int i = 0; i < E1000_NUM_RX_DESC; i++) {
		uintptr_t buf = (uintptr_t)dma_malloc(4096);
		assert(buf != 0);
		e1000->rx[i].addr = buf;
		e1000->rx[i].status = 0;
	}

	e1000_cmd_writel(e1000, E1000_REG_RX_DESC_LOW, (uint32_t)e1000->rx);
	e1000_cmd_writel(e1000, E1000_REG_RX_DESC_HIGH, 0);
	e1000_cmd_writel(e1000, E1000_REG_RX_DESC_LENGTH, size);
	e1000_cmd_writel(e1000, E1000_REG_RX_DESC_HEAD, 0);
	e1000_cmd_writel(e1000, E1000_REG_RX_DESC_TAIL, E1000_NUM_RX_DESC - 1);
	e1000_cmd_writel(e1000, E1000_REG_RX_CTRL, (
			(1<<1) | // enable
			(1<<5) | // long packet
			(1<<15) | // broadcast accept

			(1<<25) | // size (4096)
			(1<<16) |
			(1<<17) |
			e1000_cmd_readl(e1000, E1000_REG_RX_CTRL)
		) & ~((1<<7)|(1<<6)) // no loopback
		);
}

static void e1000_send_packet(void *extra, uint8_t *data, size_t length) {
	e1000_t *e1000 = (e1000_t *)extra;
	assert(e1000 != NULL);
	assert(data != NULL);
	assert(length != 0);
	assert(length <= 4096);
	assert(e1000->tx != NULL);

	uint32_t tx_index = e1000_cmd_readl(e1000, E1000_REG_TX_DESC_TAIL);

	assert(&e1000->tx[tx_index] != NULL);
	uintptr_t tx_addr = e1000->tx[tx_index].addr;
	memcpy((void *)(tx_addr), data, length);
	e1000->tx[tx_index].length = length;
	e1000->tx[tx_index].cmd = (1<<0) | (1<<1) | (1<<3) \
		| (1<<2);
	e1000->tx[tx_index].status = 0;

	tx_index = (tx_index + 1) % E1000_NUM_TX_DESC;
	e1000_cmd_writel(e1000, E1000_REG_TX_DESC_TAIL, tx_index);
}

/* only call with interrupts disabled */
static packet_t *e1000_receive_packet(void *extra) {
	e1000_t *e1000 = (e1000_t *)extra;
	assert(e1000 != NULL);
	assert(e1000->rx_queue != NULL);
	while (e1000->rx_queue->length == 0) {
		switch_task();
	}

	return (packet_t *)list_dequeue(e1000->rx_queue);
}

static unsigned int e1000_irq(unsigned int irq, void *extra) {
	assert(extra != NULL);
	e1000_t *e1000 = (e1000_t *)extra;

	uint32_t status = e1000_cmd_readl(e1000, 0xc0);

	if (!status) {
		return IRQ_IGNORED;
	}

	irq_ack(irq);

	bool should_return;
	if (status & 0x04) { // link status change
		should_return = true;
	}

	if (status & 0x10) { // rx threshold reached
		// TODO: implement
		should_return = true;
	}
	if (status & 0x01) { // tx desc written back
		should_return = true;
	}

	if (status & 0x02) { // tx queue empty
		should_return = true;
	}

	if (status & (1<<7)) { // packet received
		while (1) {
			uint32_t rx_index = e1000_cmd_readl(e1000, E1000_REG_RX_DESC_TAIL);
			assert(rx_index != e1000_cmd_readl(e1000, E1000_REG_RX_DESC_HEAD));
			rx_index = (rx_index + 1) % E1000_NUM_RX_DESC;
			if (e1000->rx[rx_index].status & 0x01) {
				// insert received packet into receive queue
				// TODO: instead of this let the network stack give us a function to call
				uintptr_t rx_addr = e1000->rx[rx_index].addr;
				uint8_t *data = (uint8_t *)(rx_addr);
				uint16_t length = e1000->rx[rx_index].length;
				assert(length <= 4096);
				assert(length != 0);
				packet_t *packet = kcalloc(1, sizeof(packet_t));
				assert(packet != NULL);
				packet->length = length;
				packet->data = kmalloc(length);
				assert(packet->data != NULL);
				memcpy(packet->data, data, length);
				e1000->rx[rx_index].status = 0;
				// FIXME: race condition
				list_insert(e1000->rx_queue, packet);
				e1000_cmd_writel(e1000, E1000_REG_RX_DESC_TAIL, rx_index);
			} else {
				break;
			}
		}
		should_return = true;
	}

	if (status & (1<<6)) { // receiver fifo overrun
		should_return = false;
	}
	if (should_return) {
		return IRQ_HANDLED;
	}
	printf("status: 0x%x\n", status);
	assert(0);
}

static void e1000_device_init(uint32_t device, uint16_t deviceid, void *extra) {
	(void)device;
	(void)deviceid;
	(void)extra;

	uintptr_t bar0 = pci_config_readl(device, PCI_BAR0);
	pci_config_writel(device, PCI_BAR0, 0xFFFFFFFF);
	uint32_t bar0_size = pci_config_readl(device, PCI_BAR0);
	bar0_size = ~(bar0_size & ~0x3) + 1;
	pci_config_writel(device, PCI_BAR0, bar0);

	assert((bar0 & 1) == 0);
	bar0 = bar0 & ~3;
	printf("memory mapped start: 0x%x length: 0x%x\n", bar0, bar0_size);
	for (uintptr_t i = 0; i < bar0_size; i += PAGE_SIZE) {
		uintptr_t v_addr = bar0 + i;
		page_t old_page = get_page(get_table(v_addr, kernel_directory), v_addr);
		assert(old_page == 0);
		map_direct_kernel(v_addr);
	}

	e1000_t *e1000 = kcalloc(1, sizeof(e1000_t));
	assert(e1000 != NULL);
	e1000->pcidevice = device;
	e1000->iobase = bar0;
	e1000->iobase_size = bar0_size;

	uint32_t pci_cmd = pci_config_readl(device, 0x04);
	pci_cmd |= (1<<0);
	pci_cmd |= (1<<2);
	pci_config_writel(device, 0x04, pci_cmd);

	e1000->eeprom_exists = e1000_has_eeprom(e1000);
	assert(e1000->eeprom_exists == true);

	e1000_read_mac(e1000);
	printf("MAC: %2x:%2x:%2x:%2x:%2x:%2x\n", e1000->mac[0], e1000->mac[1], e1000->mac[2], e1000->mac[3], e1000->mac[4], e1000->mac[5]);

	// set DEVICE RESET bit
	printf("resetting ");
	e1000_cmd_writel(e1000, E1000_REG_CTRL, (1 << 26));
	for (unsigned int i = 0; i < 10000; i++) { // FIXME: check for timeout
		printf(".");
		_sleep(10);
		uint32_t tmp = e1000_cmd_readl(e1000, E1000_REG_CTRL);
		if ((tmp & (1 << 26)) == 0) {
			break;
		}
	}
	printf(" ok!\n");

	uint32_t cmd = e1000_cmd_readl(e1000, E1000_REG_CTRL);
	cmd |= (1<<5);   // auto-negotiate
	cmd |= (1<<6);   // set link up
	cmd &= ~(1<<3);  // disable link reset
	cmd &= ~(1<<7);  // disable invert loss-of-signal
	cmd &= ~(1<<31); // disable phy reset
	cmd &= ~(1<<30); // disable vlan ( TODO: implement )
	e1000_cmd_writel(e1000, E1000_REG_CTRL, cmd);

	// disable flow control
	e1000_cmd_writel(e1000, 0x0028, 0); // flow control address low
	e1000_cmd_writel(e1000, 0x002C, 0); // flow control address high
	e1000_cmd_writel(e1000, 0x0030, 0); // flow control type
	e1000_cmd_writel(e1000, 0x0170, 0); // flow control transmit timer value

	_sleep(100); // FIXME: not needed ?

	unsigned int irq = pci_config_readb(e1000->pcidevice, PCI_IRQ);
	printf("irq: %u\n", irq);
	assert(irq != 0);
	assert(irq != 0xFF);
	irq_set_handler(irq, e1000_irq, e1000);

	// reset multicast table array
	for (int i = 0; i < 128; i++) {
		e1000_cmd_writel(e1000, 0x5200 + i * 4, 0);
	}

	for (int i = 0; i < 64; i++) {
		e1000_cmd_writel(e1000, 0x4000 + i * 4, 0);
	}

	e1000_cmd_writel(e1000, 0x5400, *(uint32_t*)(&(e1000->mac[0])));
	e1000_cmd_writel(e1000, 0x5404, *(uint16_t*)(&(e1000->mac[4])));
	e1000_cmd_writel(e1000, 0x5404, e1000_cmd_readl(e1000, 0x5404) | (1<<31)); // FIXME: research what this actually does

	// link reset
	e1000_cmd_writel(e1000, E1000_REG_RX_CTRL, (1<<4));

	// FIXME: this is a bit late
	e1000_init_tx(e1000);
	e1000_init_rx(e1000);

	e1000_cmd_writel(e1000, 0x00D0, 0xFF);
	e1000_cmd_writel(e1000, 0x00D8, 0xFF);
	e1000_cmd_writel(e1000, 0x00D0,
		(1<<0) | // tx desc written back
		(1<<1) | // tx queue empty
		(1<<2) | // link status change
		(1<<4) | // rx desc min threshold
		(1<<6) | // receiver fifo overrun
		(1<<7) | // receiver timer int
		(1<<15) | // tx desc low threshold
		0);
//		(1<<2) | (1<<6) | (1<<7)|(1<<1)|1);

	_sleep(100);

	net_register_netif(&e1000_send_packet, &e1000_receive_packet, e1000->mac, e1000);
}

static void e1000_scan_callback(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra) {
	if ((pci_config_readb(device, PCI_CLASS) == 0x02) &&
		(pci_config_readb(device, PCI_SUBCLASS) == 0x00) &&
		(vendorid == 0x8086) &&
		(deviceid == 0x100E)) {
		printf("Found e1000 Network controller (device: 0x%x vendorid: 0x%x deviceid: 0x%x)\n", device, vendorid, deviceid);
		e1000_device_init(device, deviceid, extra);
	}
}

void e1000_init(void) {
	pci_scan(e1000_scan_callback, NULL);
}

//MODULE_INFO(e1000, e1000_init);
