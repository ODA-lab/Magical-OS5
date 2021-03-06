#ifndef __MIKOOS_PCI_H
#define __MIKOOS_PCI_H 1

#include <sys/types.h>

// To store  pci device information for each device drivers.
struct pci_device_info {
	uint16_t vender;
	uint16_t devid;
	uint8_t func;
};

// Store PCI device information.
struct pci_device {
	uint8_t bus;             // bus number.
	uint8_t devfn;           // device number.
	uint8_t func;            // function number.
	// 0x0
	uint16_t vender;         // vender id.
	uint16_t devid;          // device id.

	// 0x08
	uint8_t revid;           // revision id.
	uint8_t pg_if;           // program interface.
	uint32_t sub_class;	  // sub class.
	uint32_t base_class;     // base class.

	// 0x0c
	uint8_t header_type;     // header type.
	uint8_t multi;           // multi device.

	// 0x2c
	uint16_t sub_vender;     // sub system vender id.
	uint16_t sub_devid;      // sub system device id.

	// 0x10-0x24
	uint32_t base_address;   // base address.

	//
	uint16_t interrupt_line; // TODO: implement
};

// TODO: implement
typedef struct pci_bar {
	bool locate_below_1meg;
	uint32_t address;
	int type;
	bool prefetchable;
	bool is64bit;
} pci_bar;

#define PCI_BAR_MMIO 1 // TODO: Set correct

void find_pci_device(void);
void show_all_pci_device(void);
struct pci_device *get_pci_device(uint16_t vender, uint16_t device, uint8_t function);
struct pci_bar *pci_resolve_bar(struct pci_device *pci, int val);

void pci_data_write(struct pci_device *pci, uint8_t reg_num, uint32_t data);
uint32_t pci_data_read(struct pci_device *pci, uint8_t reg_num);


#endif // __MIKOOS_PCI_H
