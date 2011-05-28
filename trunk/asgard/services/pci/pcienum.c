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

/* PCI enumeration */
void enum_pci()
{
    WORD vendor;
    unsigned int dev, func, bcount, fcount, id = 0, bid = 0;
    struct pci_bus *bus, *nbus;
    struct pci_dev *pcidev, *lastdev;
    BYTE header, class, sclass;
    list buses;
    CPOSITION it;

    init(&buses);

    /* Begin enumeration at first bus */
    bus0.id = 0;
    bus0.first_dev = NULL;
    bus0.bridge_up = NULL;
    bus0.first_child = NULL;
    bus0.parent = NULL;
    bus0.next = NULL;

    busesc = 1;
    devices = NULL;
    bridges = NULL;
    
    bus = &bus0;

    while(bus)
    {   
        lastdev = NULL;
        id = 0;
        bid = 256;
        busesc++;
    
        // scan up to 32 devices //
		for(dev=0; dev<32; dev++) 
		{ 
			fcount = 1;	// initially we assume there is only one function

			while(func < fcount)
			{
				// read the vendor field //
				vendor = read_pci_cnfw(bus->id, dev, func, PCI_VENDORID);

				if(vendor != 0xFFFF && vendor != 0) 
				{
                    // See if it's a pci-pci bridge //
                    header = read_pci_cnfb(bus->id, dev, func, PCI_HEADER);
                    class = read_pci_cnfb(bus->id, dev, func, PCI_CLASS);
                    sclass = read_pci_cnfb(bus->id, dev, func, PCI_CLASS);

                    if(PCI_HEADER_TYPE(header) == PCI_TYPE_BRIDGE && 
                        class == 0x06 && (sclass == 0x04 || sclass == 0x09))
                    {
                        // it's a bridge
                        struct pci_bridge *bridge = (struct pci_bridge*)malloc(sizeof(struct pci_bridge));

                        bridge->id = bid++;
                        bridge->vendorid = vendor;
                        bridge->deviceid = read_pci_cnfw(bus->id, dev, func, PCI_DEVICEID);
	                    bridge->class = class;
                        bridge->sclass = sclass;
                        bridge->revision = read_pci_cnfb(bus->id, dev, func, PCI_REVISION);
	                    bridge->header = header;
                        bridge->dev = dev;
	                    bridge->eid = ((bus->id << 16) | (bridge->id));
                        bridge->function = func;
                        
                        // read the header //
                        read_bridge(bridge);

                        // allocate the new bus and put it on the buses list
                        // to enumerate it
                        nbus = (struct pci_bus*)malloc(sizeof(struct pci_bus));
                        busesc++;

                        nbus->id = bridge->secondary_bus;
                        nbus->first_dev = NULL;
                        nbus->bridge_up = bridge;
                        nbus->first_child = NULL;
                        nbus->parent = bus;
                        nbus->next = bus->first_child;
                        
                        bus->first_child = nbus;

                        bridge->gnext = bridges;
                        bridges = bridge;

                        add_tail(&buses, nbus);

                        if(header & PCI_HEADER_MF)
					        fcount = 8;
                    }
                    else
                    {
					    pcidev = (struct pci_dev *)malloc(sizeof(struct pci_dev));

					    pcidev->vendorid = vendor;
					    pcidev->id = id++;
                        pcidev->dev = dev;
					    pcidev->function = func;
                        pcidev->deviceid = read_pci_cnfw(bus->id, dev, func, PCI_DEVICEID);                        
	                    pcidev->class = class;
                        pcidev->sclass = sclass;
                        pcidev->revision = read_pci_cnfb(bus->id, dev, func, PCI_REVISION);
	                    pcidev->header = header;
	                    pcidev->eid = ((bus->id << 16) | (id));
                        
					    // read all information //
					    read_device(pcidev);
                        
                        pcidev->next = NULL;
                        pcidev->gnext = devices;
                        devices = pcidev;

					    // check multifunction //
					    if(header & PCI_HEADER_MF)
					        fcount = 8;

                        if(lastdev)
                            lastdev->next = pcidev;
                        else
                            bus->first_dev = pcidev;
                        lastdev = pcidev;
                    }
				}

				func++;
			}
		}

        // get a bus and remove from buses
        it = get_head_position(&buses);
        bus = (struct pci_bus*)get_at(it);
        remove_at(&buses, it);
    }
}

struct pci_dev *get_device(int bus_id, int device_id)
{
    struct pci_bus *bus = &bus0;
    struct pci_bus *bchild = bus0.first_child;
    struct pci_dev *dev = NULL;

    /* find the bus */
    while(bus && bus->id != bus_id)
    {
        if(bchild->id <= bus_id && bchild->bridge_up->subordinate_bus > bus_id)
        {
            // it's through this bridge
            bus = bchild;
            bchild = bus->first_child;
        }
        else
        {
            bchild = bchild->next;
        }
    }

    if(bus && bus->id == bus_id)
    {
        // find the device
        dev = bus->first_dev;

        while(dev)
        {
            if(dev->id == device_id)
                break;
            dev = dev->next;
        }

    }

    return dev;
}

struct pci_bridge *get_bridge(int bus_id, int bridge_id)
{
    struct pci_bus *bus = &bus0;
    struct pci_bus *bchild = bus0.first_child;
    struct pci_bus *cbus = NULL;

    /* find the bus */
    while(bus && bus->id != bus_id)
    {
        if(bchild->id <= bus_id && bchild->bridge_up->subordinate_bus > bus_id)
        {
            // it's through this bridge
            bus = bchild;
            bchild = bus->first_child;
        }
        else
        {
            bchild = bchild->next;
        }
    }

    if(bus && bus->id == bus_id)
    {
        // find the bridge
        cbus = bus->first_child;

        while(cbus)
        {
            if(cbus->bridge_up->id == bridge_id)
                break;
            cbus = cbus->next;
        }

    }

    if(!cbus) return NULL;

    return cbus->bridge_up;
}


void read_device(struct pci_dev *pcidev)
{
    int bus = PCI_DEVID_BUS(pcidev->eid);
    pcidev->status = read_pci_cnfw(bus, pcidev->dev, pcidev->function, PCI_STATUS);
	pcidev->command = read_pci_cnfw(bus, pcidev->dev, pcidev->function, PCI_COMMAND);
	pcidev->progif = read_pci_cnfb(bus, pcidev->dev, pcidev->function, PCI_PROGIF);
	pcidev->self_test = read_pci_cnfb(bus, pcidev->dev, pcidev->function, PCI_SELFTEST);
	pcidev->latency = read_pci_cnfb(bus, pcidev->dev, pcidev->function, PCI_LATENCY);
	pcidev->cache_line_size = read_pci_cnfb(bus, pcidev->dev, pcidev->function, PCI_CACHELINE);
	pcidev->baseaddrr0 = read_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_BASEREG0);
	pcidev->baseaddrr1 = read_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_BASEREG1);
	pcidev->baseaddrr2 = read_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_BASEREG2);
	pcidev->baseaddrr3 = read_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_BASEREG3);
	pcidev->baseaddrr4 = read_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_BASEREG4);
	pcidev->baseaddrr5 = read_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_BASEREG5);
	pcidev->cardbus_CIS = read_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_CARDBUSCIS);
	pcidev->subsystemid = read_pci_cnfw(bus, pcidev->dev, pcidev->function, PCI_SUBSYSID);
	pcidev->subsystemvendorid = read_pci_cnfw(bus, pcidev->dev, pcidev->function, PCI_SUBSYSVENDOR);
	pcidev->expromaddr = read_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_EXPASIONROM);
	pcidev->max_latency = read_pci_cnfb(bus, pcidev->dev, pcidev->function, PCI_MAXLATENCY);
	pcidev->min_grant = read_pci_cnfb(bus, pcidev->dev, pcidev->function, PCI_MINGRANT);
	pcidev->interupt_pin = read_pci_cnfb(bus, pcidev->dev, pcidev->function, PCI_INTERRUPTPIN);
	pcidev->irq_line = read_pci_cnfb(bus, pcidev->dev, pcidev->function, PCI_IRQLINE);
}

void read_bridge(struct pci_bridge *bridge)
{
    int bus = PCI_DEVID_BUS(bridge->eid);
    bridge->status = read_pci_cnfw(bus, bridge->dev, bridge->function, PCI_STATUS);
	bridge->command = read_pci_cnfw(bus, bridge->dev, bridge->function, PCI_COMMAND);
	bridge->progif = read_pci_cnfb(bus, bridge->dev, bridge->function, PCI_PROGIF);
	bridge->self_test = read_pci_cnfb(bus, bridge->dev, bridge->function, PCI_SELFTEST);
    bridge->latency = read_pci_cnfb(bus, bridge->dev, bridge->function, PCI_LATENCY);
	bridge->cache_line_size = read_pci_cnfb(bus, bridge->dev, bridge->function, PCI_CACHELINE);
    bridge->baseaddrr0 = read_pci_cnfd(bus, bridge->dev, bridge->function, PCI_BASEREG0);
	bridge->baseaddrr1 = read_pci_cnfd(bus, bridge->dev, bridge->function, PCI_BASEREG1);
    bridge->sec_latency_timer = read_pci_cnfb(bus, bridge->dev, bridge->function, PCI_SLT);
    bridge->subordinate_bus = read_pci_cnfb(bus, bridge->dev, bridge->function, PCI_SUBBN);
    bridge->secondary_bus = read_pci_cnfb(bus, bridge->dev, bridge->function, PCI_SBN);
    bridge->primary_bus = read_pci_cnfb(bus, bridge->dev, bridge->function, PCI_PBN);
    bridge->secondary_status = read_pci_cnfw(bus, bridge->dev, bridge->function, PCI_SECSTATUS);
    bridge->io_imit = read_pci_cnfb(bus, bridge->dev, bridge->function, PCI_IOLIMIT);
    bridge->io_base = read_pci_cnfb(bus, bridge->dev, bridge->function, PCI_IOBASE);
    bridge->memory_limit = read_pci_cnfw(bus, bridge->dev, bridge->function, PCI_MEMLIMIT);
    bridge->memory_base = read_pci_cnfw(bus, bridge->dev, bridge->function, PCI_MEMBASE);
    bridge->prefetch_mem_limit = read_pci_cnfw(bus, bridge->dev, bridge->function, PCI_PREFMEMLIM);
    bridge->prefetch_mem_base = read_pci_cnfw(bus, bridge->dev, bridge->function, PCI_PREFMEMBASE);
    bridge->prefetch_base_upper32 = read_pci_cnfd(bus, bridge->dev, bridge->function, PCI_PREFBASEU32);
    bridge->prefetch_limit_upper32 = read_pci_cnfd(bus, bridge->dev, bridge->function, PCI_PREFLIMU32);
    bridge->io_limit_upper16 = read_pci_cnfw(bus, bridge->dev, bridge->function, PCI_IOLIMU32);
    bridge->io_base_upper16 = read_pci_cnfw(bus, bridge->dev, bridge->function, PCI_IOBASEU32);
    bridge->capability_ptr = read_pci_cnfb(bus, bridge->dev, bridge->function, PCI_CAPPTR);
    bridge->expansion_rom_base = read_pci_cnfd(bus, bridge->dev, bridge->function, PCI_EXPROMBASE);
    bridge->bridge_control = read_pci_cnfw(bus, bridge->dev, bridge->function, PCI_BRIDGECTRL);
    bridge->interupt_pin = read_pci_cnfb(bus, bridge->dev, bridge->function, PCI_INTPIN);
    bridge->irq_line = read_pci_cnfb(bus, bridge->dev, bridge->function, PCI_INTLINE);
}
