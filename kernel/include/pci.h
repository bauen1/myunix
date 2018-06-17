#ifndef PCI_H
#define PCI_H 1

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define PCI_VENDOR_ID 0x00
#define PCI_DEVICE_ID 0x02

#define PCI_PROG_IF   0x09
#define PCI_SUBCLASS  0x0a
#define PCI_CLASS     0x0b

#define PCI_BAR0      0x10
#define PCI_BAR1      0x14
#define PCI_BAR2      0x18
#define PCI_BAR3      0x1C
#define PCI_BAR4      0x20
#define PCI_BAR5      0x24

#define PCI_IRQ       0x3C


#define pci_device(bus, slot, func) ((uint32_t)(((bus)<<16)|((slot)<<11)|((func)<<8)))
#define pci_address(device, offset) ((uint32_t)(0x80000000 | (device) | ((offset) & 0xFC)))

typedef void (pci_callback_t)(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra);

void pci_scan(pci_callback_t f, void *extra);

void pci_config_writel(uint32_t device, uint8_t offset, uint32_t value);
uint32_t pci_config_readl(uint32_t device, uint8_t offset);
uint16_t pci_config_readw(uint32_t device, uint8_t offset);
uint8_t pci_config_readb(uint32_t device, uint8_t offset);
uint32_t pci_config_read(uint32_t device, uint8_t offset, int size);

void pci_print_all();

#endif
