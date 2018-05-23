#ifndef PCI_H
#define PCI_H 1

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define pci_device(bus, slot, func) ((uint32_t)(0x80000000 | ((bus)<<16)|((slot)<<11)|((func)<<8)))
#define pci_address(device, offset) ((uint32_t)(device) | ((offset) & 0xFC))

typedef void (*pci_callback_t)(uint32_t device, uint16_t vendorid, uint16_t deviceid, void *extra);

void pci_scan(pci_callback_t f, void *extra);
void pci_init();

#endif
