
#include "sartoris/kernel.h"
#include <acpi.h>

#define EBDA_PTR_ADDR 0x0000040E   

RSDPDescriptor *find_RSDP_Internal(RSDPDescriptor *start, unsigned int len);

RSDPDescriptor *find_RSDP()
{
	unsigned int addr = 0;
	addr = *((unsigned short*)EBDA_PTR_ADDR);
	addr = addr << 4;

	if(addr % 16 != 0)
		addr += addr % 16;

	RSDPDescriptor *ptr = find_RSDP_Internal((RSDPDescriptor*)addr, 0x400);

	if(ptr == NULL)
		ptr = find_RSDP_Internal((RSDPDescriptor*)0x000E0000, 0x0001FFF0);
    
	return ptr;
}

RSDPDescriptor *find_RSDP_Internal(RSDPDescriptor *start, unsigned int len)
{
	do
	{
		if(*((unsigned int*)start) == 0x20445352
			&& *(((unsigned int*)start)+1) == 0x20525450)
		{
			// checksum
			unsigned char *c = (unsigned char*)start;
			unsigned int sum = 0;
			int i = 0;

			while(i < 20)
			{
				sum += (*(c++));
                i++;
			}
			if((sum & 0x000000FF) == 0)
            	return start;            
		}

		start = (RSDPDescriptor *)((unsigned int)start+16);
		len -= 16;
	} while(len > 0);

    return NULL;
}

int strneq(char *str1, char *str2, int len)
{
	int i = 0;
	while(i < len && str1[i] == str2[i]){ i++; }

	return (i == len);
}

int acpi_checksum(ACPISDTHeader *tableHeader)
{
    unsigned char sum = 0;
 
    for (int i = 0; i < tableHeader->Length; i++)
        sum += ((char*)tableHeader)[i];
    
    return sum == 0;
}

void *findOnRSDP(RSDPDescriptor *RSDP, char *tbl, int len)
{
    RSDT *rsdt = (RSDT*)RSDP->RsdtAddress;
    int entries = (rsdt->h.Length - sizeof(rsdt->h)) / 4;
 
    RSDT_ENTRY *addr = (RSDT_ENTRY*)((unsigned int)rsdt + sizeof(rsdt->h));

    for (int i = 0; i < entries; i++)
    {
        ACPISDTHeader *h = addr[i].prtSDT;
        if (strneq(h->Signature, tbl, len) && acpi_checksum(h))
            return (void*)h;
    }
 
    return NULL;
}

