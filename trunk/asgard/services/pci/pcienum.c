/*
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
*
*	
*	Redistribution and use in source and binary forms, with or without 
* 	modification, are permitted provided that conditions specified on 
*	the License file, located at the root project directory are met.
*
*	You should have received a copy of the License along with the code,
*	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*	or you can contact us directly at the email addresses provided above.
*
*
*/

#include <lib/structures/list.h>
#include "pci.h"

extern list pci_devices;


/* PCI enumeration */
void enum_pci()
{
	unsigned int bus = 0, dev, func, bcount, fcount, id = 0;
	struct pci_dev *pcidev;
	WORD vendor;

	/* Begin enumeration at first bus */
	while(bus < bcount)
	{
		/* scan up to 32 devices */
		for(dev=0; dev<32; dev++) 
		{ 
			fcount = 1;	// initially we assume there is only one function

			while(func < fcount)
			{
				/* read the vendor field */
				vendor = read_pci_cnfw(bus, dev, func, PCI_VENDORID);

				if(vendor != 0xFFFF && vendor!=0) 
				{
					pcidev = (struct pci_dev *)malloc(sizeof(struct pci_dev));

					pcidev->vendorid = vendor;
					pcidev->id = id++;
					pcidev->bus = bus;
					pcidev->dev = dev;
					pcidev->func = func;

					/* read all information */
					pcidev->deviceid = read_pci_cnfw(bus, dev, func, PCI_DEVICEID);
					pcidev->status = read_pci_cnfw(bus, dev, func, PCI_STATUS);
					pcidev->command = read_pci_cnfw(bus, dev, func, PCI_COMMAND);
					DWORD cl = read_pci_cnfw(bus, dev, func, PCI_CLASS);
					pcidev->class_sclass = (cl >> 8);
					pcidev->progif = (cl & 0xff);
					pcidev->self_test = read_pci_cnfb(bus, dev, func, PCI_SELFTEST);
					pcidev->header = read_pci_cnfb(bus, dev, func, PCI_HEADER);
					pcidev->latency = read_pci_cnfb(bus, dev, func, PCI_LATENCY);
					pcidev->cache_line_size = read_pci_cnfb(bus, dev, func, PCI_CACHELINE);
					pcidev->baseaddrr0 = read_pci_cnfd(bus, dev, func, PCI_BASEREG0);
					pcidev->baseaddrr1 = read_pci_cnfd(bus, dev, func, PCI_BASEREG1);
					pcidev->baseaddrr2 = read_pci_cnfd(bus, dev, func, PCI_BASEREG2);
					pcidev->baseaddrr3 = read_pci_cnfd(bus, dev, func, PCI_BASEREG3);
					pcidev->baseaddrr4 = read_pci_cnfd(bus, dev, func, PCI_BASEREG4);
					pcidev->baseaddrr5 = read_pci_cnfd(bus, dev, func, PCI_BASEREG5);
					pcidev->cardbus_CIS = read_pci_cnfd(bus, dev, func, PCI_CARDBUSCIS);
					pcidev->subsystemid = read_pci_cnfw(bus, dev, func, PCI_SUBSYSID);
					pcidev->subsystemvendorid = read_pci_cnfw(bus, dev, func, PCI_SUBSYSVENDOR);
					pcidev->expromaddr = read_pci_cnfd(bus, dev, func, PCI_EXPASIONROM);
					pcidev->max_latency = read_pci_cnfb(bus, dev, func, PCI_MAXLATENCY);
					pcidev->min_grant = read_pci_cnfb(bus, dev, func, PCI_MINGRANT);
					pcidev->interupt_Pin = read_pci_cnfb(bus, dev, func, PCI_INTERRUPTPIN);
					pcidev->irq_Line = read_pci_cnfb(bus, dev, func, PCI_IRQLINE);

					/* check multifunction */
					if(pcidev->header & 0x80)
					{
						fcount = 8;
					}

					/* Found a bridge! */
					if( (pcidev->header & 0x7f) == 0x01 || pcidev->class_sclass == 0x0604)
					{
						bcount++;
					}

					add_tail(&pci_devices, pcidev);
				}

				func++;
			}
		}

		bus++;
	}
	
}



