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

#include <services/pci/pci.h>
#include <services/stds/stdservice.h>
#include <lib/structures/list.h>
#include <lib/malloc.h>
#include "pci.h"

void get_devices(struct pci_getdevices *pcmd, struct pci_res *res)
{
	unsigned int size = mem_size(pcmd->buffer_smo);
	unsigned int rsize = 0;
	int cont = pcmd->continuation;
    struct pci_dev *pcidev;
    struct pci_bridge *bridge;
    struct pci_dev_info devinf;
    struct pci_bus *bus;
    	
    bridge = bridges;
    
    if(cont == -1)
    {
        res->ret = PCIERR_ENUM_FINISHED;
        return;
    }

	if(cont != 0)
	{
        cont = pcmd->continuation;
        int eid = PCI_DEVID_EID(cont);
        int busid = PCI_DEVID_BUS(cont);

        if(eid > 255)
        {
            // it's a bridge
            while(bridge && bridge->eid != cont)
            {
                bridge = bridge->gnext;
            }
            bridge = bridge->gnext;
        }

        if(eid < 256 || bridge == NULL)
        {
            pcidev = devices;
            while(pcidev && pcidev->eid != cont)
            {
                pcidev = pcidev->gnext;
            }
            if(pcidev == NULL)
            {
                res->ret = PCIERR_INVALID_CONTINUATION;
                return;
            }
        }
	}
	    
    if(pcidev == NULL)
    {
        if(rsize == 0 && size < sizeof(struct pci_dev_info))
        {
            res->ret = PCIERR_SMO_TOO_SMALL;
            return;
        }

        // continue returning bridges
        while(size-rsize >= sizeof(struct pci_dev_info) && bridge)
	    {
		    devinf.class = bridge->class;
		    devinf.sclass = bridge->sclass;
		    devinf.vendorid = bridge->vendorid;
		    devinf.function = 0;
		    devinf.progif = bridge->progif;
            devinf.dev_id = bridge->eid;
            devinf.type = PCI_TYPE_BRIDGE;

		    if(write_mem(pcmd->buffer_smo, rsize, sizeof(struct pci_dev_info), (char*)&devinf) == FAILURE)
            {
                res->ret = PCIERR_SMO_WRITE_ERR;
                return;
            }

		    cont = bridge->eid;
		    rsize += sizeof(struct pci_dev_info);
		    size -= sizeof(struct pci_dev_info);
		    bridge = bridge->gnext;
	    }

        // if we still have size on the SMO, continue with PCI devices
        if(size-rsize >= sizeof(struct pci_dev_info))
            pcidev = devices;
    }
    
    if(pcidev != NULL)
    {
        if(rsize == 0 && size < sizeof(struct pci_dev_info))
        {
            res->ret = PCIERR_SMO_TOO_SMALL;
            return;
        }

        // continue returning pci devices
        while(size-rsize >= sizeof(struct pci_dev_info) && pcidev)
        {
            devinf.class = pcidev->class;
		    devinf.sclass = pcidev->sclass;
		    devinf.vendorid = pcidev->vendorid;
		    devinf.function = pcidev->function;
		    devinf.progif = pcidev->progif;
            devinf.dev_id = pcidev->eid;
            devinf.type = PCI_TYPE_DEVICE;

		    if(write_mem(pcmd->buffer_smo, rsize, sizeof(struct pci_dev_info), (char*)&devinf) == FAILURE)
            {
                res->ret = PCIERR_SMO_WRITE_ERR;
                return;
            }

		    cont = pcidev->eid;
		    rsize += sizeof(struct pci_dev_info);
		    size -= sizeof(struct pci_dev_info);
		    pcidev = pcidev->gnext;
	    }

        if(!pcidev)
            cont = -1;
    }
        		
	res->ret = PCIERR_OK;
	res->specific0 = cont;
	res->specific1 = rsize;
}

void pci_find(struct pci_finddev *pcmd, struct pci_res *res)
{
	struct pci_finddev_res *fres = (struct pci_finddev_res *)res;
	struct pci_dev *pcidev = devices;
    struct pci_bridge *bridge = bridges;

    while(pcidev)
    {
        if(pcidev->class == pcmd->class 
            && pcidev->sclass ==pcmd->sclass
            && pcidev->vendorid == pcmd->vendorid)
        {
            if(((pcmd->flags & FIND_FLAG_PROGIF) && pcidev->progif != pcmd->progif)
                || ((pcmd->flags & FIND_FLAG_FUNCTION) && pcidev->function != pcmd->function))
            {   
                pcidev = pcidev->gnext;
                continue;
            }
            fres->ret = PCIERR_OK;
	        fres->dev_id = pcidev->eid;
	        fres->function = pcidev->function;
            fres->progif = pcidev->progif;
        }
        pcidev = pcidev->gnext;
    }

    if(pcmd->flags & FIND_FLAG_FUNCTION)
    {
        fres->ret = PCIERR_NOT_FOUND;
    }
    else
    {
        while(bridge)
        {
            if(bridge->class == pcmd->class 
                && bridge->sclass ==pcmd->sclass
                && bridge->vendorid == pcmd->vendorid)
            {
                if((pcmd->flags & FIND_FLAG_PROGIF) && bridge->progif != pcmd->progif)
                {   
                    bridge = bridge->gnext;
                    continue;
                }
                fres->ret = PCIERR_OK;
	            fres->dev_id = bridge->eid;
	            fres->function = 0;
                fres->progif = bridge->progif;
            }
            bridge = bridge->gnext;
        }
    }

	fres->ret = PCIERR_NOT_FOUND;
}

void get_config(struct pci_getcfg *pcmd, struct pci_res *res)
{
	res->ret = PCIERR_OK;
    
    if(PCI_DEVID_IS_BRIDGE(pcmd->dev_id))
    {
        struct pci_bridge *bridge = get_bridge(PCI_DEVID_BUS(pcmd->dev_id), PCI_DEVID_EID(pcmd->dev_id));

        if(bridge)
            if(write_mem(pcmd->cfg_smo, 0, sizeof(struct pci_cfg), ((char*)bridge) + 19) == FAILURE)
                res->ret = PCIERR_SMO_WRITE_ERR;
        else
            res->ret = PCIERR_INVALID_DEVID;
    }
    else
    {
	    struct pci_dev *pcidev = get_device(PCI_DEVID_BUS(pcmd->dev_id), PCI_DEVID_EID(pcmd->dev_id));
        if(pcidev)
            if(write_mem(pcmd->cfg_smo, 0, sizeof(struct pci_cfg), ((char*)pcidev) + 19) == FAILURE)
                res->ret = PCIERR_SMO_WRITE_ERR;
        else
            res->ret = PCIERR_INVALID_DEVID;
    }
}

void set_config(struct pci_setcfg *pcmd, struct pci_res *res)
{
    int bus = PCI_DEVID_BUS(pcmd->dev_id);
	res->ret = PCIERR_OK;
    
    if(PCI_DEVID_IS_BRIDGE(pcmd->dev_id))
    {
        struct pci_bridge *bridge = get_bridge(bus, PCI_DEVID_EID(pcmd->dev_id));

        if(bridge)
        {
            struct pci_bridge_cfg bcfg;
            if(read_mem(pcmd->cfg_smo, 0, sizeof(struct pci_bridge_cfg), &bcfg))
            {
                res->ret = PCIERR_SMO_READ_ERR;
            }
            else
            {
                if(pcmd->flags & PCI_SETCFG_STATUS) write_pci_cnfw(bus, bridge->dev, 0, PCI_STATUS, bcfg.status);
				if(pcmd->flags & PCI_SETCFG_COMMAND) write_pci_cnfw(bus, bridge->dev, 0, PCI_COMMAND, bcfg.command);
				if(pcmd->flags & PCI_SETCFG_BIST) write_pci_cnfb(bus, bridge->dev, 0, PCI_SELFTEST, bcfg.self_test);
				if(pcmd->flags & PCI_SETCFG_LATENCY) write_pci_cnfb(bus, bridge->dev, 0, PCI_LATENCY, bcfg.latency);
				if(pcmd->flags & PCI_SETCFG_CACHELSIZE) write_pci_cnfb(bus, bridge->dev, 0, PCI_CACHELINE, bcfg.cache_line_size);
                if(pcmd->flags & PCI_SETCFG_BASEADDR0) write_pci_cnfd(bus, bridge->dev, 0, PCI_BASEREG0, bcfg.baseaddrr0);
				if(pcmd->flags & PCI_SETCFG_BASEADDR1) write_pci_cnfd(bus, bridge->dev, 0, PCI_BASEREG1, bcfg.baseaddrr1);
                if(pcmd->flags & PCI_SETCFG_SEC_LATENCY_TMR) write_pci_cnfb(bus, bridge->dev, 0, PCI_SLT, bcfg.sec_latency_timer);
                if(pcmd->flags & PCI_SETCFG_SUBORD_BUS) write_pci_cnfb(bus, bridge->dev, 0, PCI_SUBBN, bcfg.subordinate_bus);
                if(pcmd->flags & PCI_SETCFG_SECONDARY_BUS) write_pci_cnfb(bus, bridge->dev, 0, PCI_SBN, bcfg.secondary_bus);
                if(pcmd->flags & PCI_SETCFG_PRIMARY_BUS) write_pci_cnfb(bus, bridge->dev, 0, PCI_PBN, bcfg.primary_bus);
                if(pcmd->flags & PCI_SETCFG_SECONDARY_STATUS) write_pci_cnfw(bus, bridge->dev, 0, PCI_SECSTATUS, bcfg.secondary_status);
                if(pcmd->flags & PCI_SETCFG_IO_LIMIT) write_pci_cnfb(bus, bridge->dev, 0, PCI_IOLIMIT, bcfg.io_imit);
                if(pcmd->flags & PCI_SETCFG_IO_BASE) write_pci_cnfb(bus, bridge->dev, 0, PCI_IOBASE, bcfg.io_base );
                if(pcmd->flags & PCI_SETCFG_MEM_LIMIT) write_pci_cnfw(bus, bridge->dev, 0, PCI_MEMLIMIT, bcfg.memory_limit);
                if(pcmd->flags & PCI_SETCFG_MEM_BASE) write_pci_cnfw(bus, bridge->dev, 0, PCI_MEMBASE, bcfg.memory_base);
                if(pcmd->flags & PCI_SETCFG_PREF_MEM_LIMIT) write_pci_cnfw(bus, bridge->dev, 0, PCI_PREFMEMLIM, bcfg.prefetch_mem_limit);
                if(pcmd->flags & PCI_SETCFG_PREF_MEM_BASE) write_pci_cnfw(bus, bridge->dev, 0, PCI_PREFMEMBASE, bcfg.prefetch_mem_base);
                if(pcmd->flags & PCI_SETCFG_PREFETCHB_UP32) write_pci_cnfd(bus, bridge->dev, 0, PCI_PREFBASEU32, bcfg.prefetch_base_upper32);
                if(pcmd->flags & PCI_SETCFG_PREFETCHL_UP32) write_pci_cnfd(bus, bridge->dev, 0, PCI_PREFLIMU32, bcfg.prefetch_limit_upper32);
                if(pcmd->flags & PCI_SETCFG_IO_LIMIT_UP16) write_pci_cnfw(bus, bridge->dev, 0, PCI_IOLIMU32, bcfg.io_limit_upper16);
                if(pcmd->flags & PCI_SETCFG_IO_BASE_UP16) write_pci_cnfw(bus, bridge->dev, 0, PCI_IOBASEU32, bcfg.io_base_upper16);
                if(pcmd->flags & PCI_SETCFG_EXP_ROM_ADDR) write_pci_cnfd(bus, bridge->dev, 0, PCI_EXPROMBASE, bcfg.expansion_rom_base);
                if(pcmd->flags & PCI_SETCFG_BRIDGE_CTRL) write_pci_cnfw(bus, bridge->dev, 0, PCI_BRIDGECTRL, bcfg.bridge_control);
                if(pcmd->flags & PCI_SETCFG_INT_PIN) write_pci_cnfb(bus, bridge->dev, 0, PCI_INTPIN, bcfg.interupt_pin);
                if(pcmd->flags & PCI_SETCFG_INT_LINE) write_pci_cnfb(bus, bridge->dev, 0, PCI_INTLINE, bcfg.irq_line);

                read_bridge(bridge);

                // write it back
                if(write_mem(pcmd->cfg_smo, 0, sizeof(struct pci_cfg), ((char*)bridge) + 19) == FAILURE)
                    res->ret = PCIERR_SMO_WRITE_ERR;
            }
        }
        else
            res->ret = PCIERR_INVALID_DEVID;
    }
    else
    {
	    struct pci_dev *pcidev = get_device(bus, PCI_DEVID_EID(pcmd->dev_id));
        if(pcidev)
        {
            struct pci_cfg cfg;
            if(read_mem(pcmd->cfg_smo, 0, sizeof(struct pci_cfg), &cfg))
            {
                res->ret = PCIERR_SMO_READ_ERR;
            }
            else
            {
                if(pcmd->flags & PCI_SETCFG_STATUS) write_pci_cnfw(bus, pcidev->dev, pcidev->function, PCI_STATUS, cfg.status);
				if(pcmd->flags & PCI_SETCFG_COMMAND) write_pci_cnfw(bus, pcidev->dev, pcidev->function, PCI_COMMAND, cfg.command);
				if(pcmd->flags & PCI_SETCFG_BIST) write_pci_cnfb(bus, pcidev->dev, pcidev->function, PCI_SELFTEST, cfg.self_test);
				if(pcmd->flags & PCI_SETCFG_LATENCY) write_pci_cnfb(bus, pcidev->dev, pcidev->function, PCI_LATENCY, cfg.latency);
				if(pcmd->flags & PCI_SETCFG_CACHELSIZE) write_pci_cnfb(bus, pcidev->dev, pcidev->function, PCI_CACHELINE, cfg.cache_line_size);
				if(pcmd->flags & PCI_SETCFG_BASEADDR0) write_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_BASEREG0, cfg.baseaddrr0);
				if(pcmd->flags & PCI_SETCFG_BASEADDR1) write_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_BASEREG1, cfg.baseaddrr1);
				if(pcmd->flags & PCI_SETCFG_BASEADDR2) write_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_BASEREG2, cfg.baseaddrr2);
				if(pcmd->flags & PCI_SETCFG_BASEADDR3) write_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_BASEREG3, cfg.baseaddrr3);
				if(pcmd->flags & PCI_SETCFG_BASEADDR4) write_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_BASEREG4, cfg.baseaddrr4);
				if(pcmd->flags & PCI_SETCFG_BASEADDR5) write_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_BASEREG5, cfg.baseaddrr5);
				if(pcmd->flags & PCI_SETCFG_CIS_PTR) write_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_CARDBUSCIS, cfg.cardbus_CIS);
				if(pcmd->flags & PCI_SETCFG_EXP_ROM_ADDR) write_pci_cnfd(bus, pcidev->dev, pcidev->function, PCI_EXPASIONROM, cfg.expromaddr );
				if(pcmd->flags & PCI_SETCFG_INT_PIN) write_pci_cnfb(bus, pcidev->dev, pcidev->function, PCI_INTERRUPTPIN, cfg.interupt_pin);
				if(pcmd->flags & PCI_SETCFG_INT_LINE) write_pci_cnfb(bus, pcidev->dev, pcidev->function, PCI_IRQLINE, cfg.irq_line);

                read_device(pcidev);

                if(write_mem(pcmd->cfg_smo, 0, sizeof(struct pci_cfg), ((char*)pcidev) + 19) == FAILURE)
                    res->ret = PCIERR_SMO_WRITE_ERR;
            }
        }
        else
            res->ret = PCIERR_INVALID_DEVID;
    }
}


