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

extern list pci_devices;

struct mem_region *get_mregion(struct pci_dev *pcidev, int addr, int *cont)
{
	struct mem_region *mreg = NULL;
	DWORD baddr;

	switch(addr)
	{
		case 0:
			baddr = pcidev->baseaddrr0;
			break;
		case 1:
			baddr = pcidev->baseaddrr1;
			break;
		case 2:
			baddr = pcidev->baseaddrr2;
			break;
		case 3:
			baddr = pcidev->baseaddrr3;
			break;
		case 4:
			baddr = pcidev->baseaddrr4;
			break;
		case 5:
			baddr = pcidev->baseaddrr5;
			break;
	}

	*cont = 1;

	if(baddr != 0 && !(baddr & 0x1))
	{
		mreg = (struct mem_region *)malloc(sizeof(struct mem_region));
		mreg->size = sizeof(struct mem_region);
		mreg->start = (baddr & ~(0x0000000F));
		/* Now get the end */
		DWORD oldval = *((DWORD*)mreg->start);
		*((DWORD*)mreg->start) = 0xFFFFFFFF;
		mreg->end = ((*((DWORD*)mreg->start)) & 0xf) + mreg->start;
		*((DWORD*)mreg->start) = oldval;
   	}
	else if(baddr == 0)
	{
		*cont = 0;	// no more addresses
	}

	return mreg;
}

struct pci_dev *get_dev(BYTE class, WORD sclass, WORD vendorid, BYTE function)
{
	struct pci_dev *dv = NULL;
	CPOSITION it;

	it = get_head_position(&pci_devices);
	while(it != NULL)
	{ 
		dv = (struct pci_dev *)get_next(&it);
		if(GET_CLASS(dv->class_sclass) == class && GET_SUBCLASS(dv->class_sclass) == sclass && dv->vendorid == vendorid && dv->func == function) return dv;
	}

	return NULL;
}

struct pci_res *get_mapped_regions(struct pci_getmregions *pcmd)
{
	unsigned int size = mem_size(pcmd->buffer_smo);
	unsigned int rsize = 0;
	struct pci_res *res = NULL;
	CPOSITION it;
	int cont = pcmd->continuation;

	it = get_head_position(&pci_devices);
	
	if(cont != 0)
	{
		while(it != NULL && cont > 0){ get_next(&it); cont--;}

		if(it == NULL)
		{
			/* Bad continuation */
			res = build_response_msg(PCIERR_INVALID_CONTINUATION);
			return res;
		}

		cont = pcmd->continuation;
	}
	
	while(size >= sizeof(struct mem_region) && it != NULL)
	{
		struct pci_dev *pcidev = (struct pci_dev *)get_at(it);
		int continues = 0, addr = 0;

		struct mem_region *mreg = get_mregion(pcidev, addr, &continues);
		
		while(size >= sizeof(struct mem_region) && continues)
		{
			if(mreg != NULL)
			{
				write_mem(pcmd->buffer_smo, rsize, sizeof(struct mem_region), (char*)mreg);

				rsize += sizeof(struct mem_region);
				size -= sizeof(struct mem_region);

				free(mreg);
			}
			addr++;
			if(continues) mreg = get_mregion(pcidev, addr, &continues);
		}
		
		cont++;
		get_next(&it);
	}
	
	res = build_response_msg(PCIERR_OK);
	res->specific0 = cont;
	res->specific1 = rsize;
	return res;
}

struct pci_res *get_devices(struct pci_getdevices *pcmd)
{
	unsigned int size = mem_size(pcmd->buffer_smo);
	unsigned int rsize = 0;
	struct pci_res *res = NULL;
	CPOSITION it;
	int cont = pcmd->continuation;

	it = get_head_position(&pci_devices);
	
	if(cont != 0)
	{
		while(it != NULL && cont > 0){ get_next(&it); cont--;}

		if(it == NULL)
		{
			/* Bad continuation */
			res = build_response_msg(PCIERR_INVALID_CONTINUATION);
			return res;
		}

		cont = pcmd->continuation;
	}
	
	while(size >= sizeof(struct pci_dev_info) && it != NULL)
	{
		struct pci_dev *pcidev = (struct pci_dev *)get_at(it);
		
		struct pci_dev_info devinf;

		devinf.class = GET_CLASS(pcidev->class_sclass);
		devinf.sclass = GET_SUBCLASS(pcidev->class_sclass);
		devinf.vendorid = pcidev->vendorid;
		devinf.function = pcidev->func;
		devinf.progif = pcidev->progif;

		write_mem(pcmd->buffer_smo, rsize, sizeof(struct pci_dev_info), (char*)&devinf);

		cont++;
		rsize += sizeof(struct pci_dev_info);
		size -= sizeof(struct pci_dev_info);
		get_next(&it);
	}
	
	res = build_response_msg(PCIERR_OK);
	res->specific0 = cont;
	res->specific1 = rsize;
	return res;
}

struct pci_res *get_config(struct pci_getcfg *pcmd)
{
	if(mem_size(pcmd->cfg_smo) < sizeof(struct pci_cfg))
	{
		return build_response_msg(PCIERR_SMO_TOO_SMALL);
	}

	struct pci_dev *pcidev = get_dev(pcmd->class, pcmd->sclass, pcmd->vendorid, pcmd->function);

	if(pcidev == NULL)
	{
		return build_response_msg(PCIERR_DEVICE_NOT_FOUND);
	}

	write_mem(pcmd->cfg_smo, 0, sizeof(struct pci_cfg), ((char*)pcidev) + 16);

	return build_response_msg(PCIERR_OK);
}

struct pci_res *set_config(struct pci_setcfg *pcmd)
{
	/* FIXME: Implement this function... it's not hard */
	return build_response_msg(PCIERR_NOTIMPLEMENTED);
}


