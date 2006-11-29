/*
*
*	This is an utility for manipulating OFS disk images
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

#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include "elf.h"

char buffer[1024];
char *types[8] = { "NULL", "LOAD", "DYNAMIC", "INTERP", "NOTE", "SHLIB", "PHDR", "UNKNOWN" };

/*
	What this program does, is stripping padding data from an elf executable.
	This is useful for services, because we need them to be small, and cannot
	sacrifice runtime memory nor file size.

	Usage: sld filename outputfile
*/
int main(int argc, char** args) 
{
	FILE *elf_file, *out_file;
	struct Elf32_Ehdr elf_header;
	char *raw_header;
	char *raw_newheader;
	struct Elf32_Phdr *prog_header;
	int i;
	
	
	if (argc != 3) {
		printf("usage %s elffile outfile, or %s --dump elffile\n", args[0], args[0]);
		return 1;
	}

	int src_index = 1;
	int write = 1;
	if(strcmp("--dump", args[1]) == 0)
	{
		src_index = 2;
		write = 0;
	}

	elf_file = fopen(args[src_index], "rb");

	if ( elf_file == NULL) 
	{
		printf("could not open file %s.\n", args[1]);
		return 1;
	}

	if(fread(&elf_header, sizeof(struct Elf32_Ehdr), 1, elf_file) != 1) 
	{
		printf("error while reading elf header from file %s.\n", args[1]);
		return 1;
	} 

	printf("entry point is: %x\n", elf_header.e_entry);

	printf("found %i entries in program header table:\n", elf_header.e_phnum);

	raw_header = (char*)malloc(elf_header.e_phentsize * elf_header.e_phnum);

	fseek(elf_file, elf_header.e_phoff, SEEK_SET);
	fread(raw_header, elf_header.e_phentsize, elf_header.e_phnum, elf_file);

	for (i=0; i<elf_header.e_phnum; i++) 
	{
		prog_header = (struct Elf32_Phdr*) & raw_header[i*elf_header.e_phentsize];
		printf("segment %i, type %s, flags %x, fileoffset %x, memoffset %x, filesize %x, memsize %x\n",
			i, types[prog_header->p_type < 7 ? prog_header->p_type : 7], prog_header->p_flags,
			prog_header->p_offset, prog_header->p_vaddr, 
			prog_header->p_filesz, prog_header->p_memsz);
	}

	if(!write) return 0;	// no conversion is done, just watching 

	printf("Creating output file.\n");

	out_file = fopen(args[2], "wb");

	if ( elf_file == NULL) 
	{
		printf("could not create file %s.\n", args[2]);
		fclose(elf_file);
		free(raw_header);
		return 1;
	}
	int read = 0;

	printf("Writing header.\n");

	if(fwrite(&elf_header, sizeof(struct Elf32_Ehdr), 1, out_file) != 1)
	{
		printf("could not write on file %s.\n", args[2]);
		fclose(elf_file);
		fclose(out_file);
		free(raw_header);
		return 1;
	}

	// write whatever is between the header and pheaders
	printf("Writing intermediate data...\n");

	fseek(elf_file, sizeof(struct Elf32_Ehdr), SEEK_SET);

	int left = elf_header.e_phoff - sizeof(struct Elf32_Ehdr);
	while(left > 0)
	{
		int count = ((left > 1024)? 1024 : left);

		read = fread(buffer, 1, count, elf_file);
		fwrite(buffer, 1, count, out_file);

		left -= count;
	}

	printf("Processing program headers...\n");

	// order headers by fileoffset
	// (a pretty uneficient algorithm used, because I really dont care if it takes long :D)
	raw_newheader = (char*)malloc(elf_header.e_phentsize * elf_header.e_phnum);

	int j;
	struct Elf32_Phdr new_header;
	unsigned int curr_off = (unsigned int)-1;
	int index;

	for(j=0; j<elf_header.e_phnum; j++) 
	{
		index = j;

		// find the lowest offset on file
		for(i=j; i<elf_header.e_phnum; i++) 
		{
			prog_header = (struct Elf32_Phdr*) &raw_header[i*elf_header.e_phentsize];

			if(prog_header->p_offset < curr_off)
			{
				curr_off = prog_header->p_offset;
				index = i;
			}
		}	
	
		// swap if not the same element
		if(j != index)
		{
			memcpy(buffer, &raw_header[j*elf_header.e_phentsize], elf_header.e_phentsize); // j to buffer
			memcpy(&raw_header[j*elf_header.e_phentsize], &raw_header[index*elf_header.e_phentsize], elf_header.e_phentsize); // index to j
			memcpy(&raw_header[index*elf_header.e_phentsize],buffer, elf_header.e_phentsize); // old j to index
		}

		// copy to destination array
		memcpy(&raw_newheader[j*elf_header.e_phentsize],&raw_header[j*elf_header.e_phentsize], elf_header.e_phentsize);
		
	}

	// Write program data
	printf("Writing data...\n");

	int new_offset = ((struct Elf32_Phdr*)raw_newheader)->p_offset; // initial offset is that of the first header

	fseek(out_file, new_offset, SEEK_SET);

	for (i=0; i<elf_header.e_phnum; i++) 
	{
		prog_header = (struct Elf32_Phdr*) &raw_newheader[i*elf_header.e_phentsize];

		printf("seg %i, type %s, fgs %x, foffset %x fsize %x: ",
			i, types[prog_header->p_type < 7 ? prog_header->p_type : 7], prog_header->p_flags, 
			prog_header->p_offset, prog_header->p_filesz);

		if(prog_header->p_filesz != 0)
		{
			fseek(elf_file, prog_header->p_offset, SEEK_SET);
			
			left = prog_header->p_filesz;
			while(left > 0)
			{
				int count = ((left > 1024)? 1024 : left);

				read = fread(buffer, 1, count, elf_file);
				fwrite(buffer, 1, count, out_file);

				left -= count;

				printf(".");
			}

			prog_header->p_offset = new_offset;

			printf("-> foffset %x\n", new_offset);

			new_offset += prog_header->p_filesz; 
		}
		else
		{
			printf("File size is 0, skipping entry.\n");
		}
	}

	printf("Writing re ordered headers...\n");

	// write reordered pheaders
	fseek(out_file, elf_header.e_phoff, SEEK_SET);
	fwrite(raw_newheader, elf_header.e_phentsize, elf_header.e_phnum, out_file);

	printf("Finished!\n");

	fclose(elf_file);
	fclose(out_file);
	free(raw_header);
	free(raw_newheader);

	return 0;
}

