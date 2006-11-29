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
* INITFS2 creation tool
*/

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include "../services/p-manager2/formats/initfs2.h"

#define MAX_FILES 20

int israw(const char filename[], char *dest);

struct header_flags
{
	char imgname[MAX_NAME_SIZE];
	unsigned short flags;
	unsigned short pman_type;
	unsigned short taskid;
	unsigned short mainthreadid;
};

struct header_flags hflags[MAX_FILES];
int hflags_size;
char rbuff[256];

int set_flags(char *imgname, struct ifs2srv_header *header);
void build_flags_tbl();

int main(int argc, char** args) 
{
	int i;
	int filecount=0;
	DIR *dirp;
	struct dirent *curdir;
	struct stat fileattr;
	struct ifs2srv_header headers[MAX_FILES];
	char files[MAX_FILES][MAX_NAME_SIZE];
	struct ifs2_header header;
	int offset;
	char command[MAX_FILES];
	FILE* filep;
	

	/* check the command line arguments */
	if ( argc < 2) 
	{
		printf ("Too few arguments\nUsage: %s <destdir>\n", args[0]);
		return 1;
	}
  
	/* Get image properties from initfs2conf file  */
	build_flags_tbl();
	
	/* 
		Get the .raw file list   
	*/

	/* open the current directory */
	if (!(dirp = opendir("."))) 
	{
		printf("Error opening directory\n");
		return 1;
	}
  
	/* get the list of the .raw files */
	while (curdir = readdir(dirp)) 
	{
		/* if the entry is a .raw file */
		if ( israw(curdir->d_name, args[1])) 
		{
			if ( strlen(curdir->d_name) >= MAX_NAME_SIZE) 
			{
				printf ("%s exceeded %d chars\n", curdir->d_name, MAX_NAME_SIZE);
				return 1;
			} 
			else 
			{
				strcpy(files[filecount], curdir->d_name);
								
				filecount++;
			}
		}
	}
  
	closedir(dirp);

	header.entries = filecount;
	header.flags = IFS2_FLAG_NONE;
	header.ifs2magic = IFS2MAGIC;
	header.headers = sizeof(struct ifs2_header);

	printf("\nCreating InitFS2 Services Image....\n\n");

	/* generate the initfs2 table  */
	
	/* for each file, calculate the header */
	offset = filecount * sizeof(struct ifs2srv_header) + sizeof(struct ifs2_header);
	
	for ( i = 0; i < filecount; i++ ) 
	{
		stat(files[i], &fileattr);

		strncpy(headers[i].img_name, files[i], strlen(files[i]) - 4);
		headers[i].img_name[ strlen(files[i]) - 4] = '\0'; // add \0

		/* If image has flags set them */
		set_flags(headers[i].img_name, &headers[i]);

		headers[i].image_size = fileattr.st_size;
		headers[i].image_pos = offset;

		printf("Service image file %s (%s) : (%d bytes), pos: %d %s%s%s%s\n", 
			files[i],
			headers[i].img_name, 
			headers[i].image_size,
			headers[i].image_pos,
			((headers[i].flags & IFS2SRV_FLAG_LOWMEM)? "|lowmem" : ""),
			((headers[i].flags & IFS2SRV_FLAG_PHYSTART)? "|phymsg" : ""),
			((headers[i].pman_type & IFS2SRV_PMTYPE_MAINFS)? "|mainfs" : ""),
			((headers[i].pman_type & IFS2SRV_PMTYPE_HDD)? "|hddc" : ""),
			((headers[i].flags & IFS2SRV_FLAG_IGNORE)? "|DISABLED" : "")
			);

		offset += headers[i].image_size;
	}

	header.size = offset;

	/* write the headers in the output file */
  
	/* open the output file */
	if (!(filep = fopen(args[1], "wb"))) 
	{
		printf("Error opening the output file %s\n",args[1]);
		return 1;
	}
  
	/* write Init Header */
	if (fwrite (&header, sizeof(struct ifs2_header), 1, filep) != 1) 
	{
		printf("Error writting Init Header in the output file %s\n",args[1]);
		return 1;
	}
  
	/* write headers */
	if (fwrite(headers, sizeof(struct ifs2srv_header), filecount, filep) != filecount) 
	{
		printf("Error writting Service Headers in the output file %s\n",args[1]);
		return 1;
	}

	fclose(filep);

	/* append the files info to the initfs2 image */
	for ( i=0; i < filecount; i++) 
	{
		/* prepare the command string */
		strcpy(command, "cat ");
		strcat(command, files[i]);
		strcat(command, " >> ");
		strcat(command, args[1]);
		system(command);
	}

	printf("\nInit services image created.\n\n");
	return 0;
}

void build_flags_tbl()
{
	int i = -1;
	int nest = 0;

	hflags_size = 0;

	FILE *fp = fopen("initfs2conf", "r");

	if(fp == NULL)
		return;
	
	/* parse file */
	while(fgets(rbuff, 255, fp))
	{
		if(rbuff[0] == '#') continue;	// ignore comment lines

		if(strcmp(rbuff, "\n") == 0)
		{
			nest = 0;	// found empty line
		}
		else
		{
			switch(nest)
			{
				case 0: /* new image name */
					i++;
					hflags_size++;
					strncpy(hflags[i].imgname, rbuff, strlen(rbuff)-1);
					hflags[i].imgname[strlen(rbuff)] = '\0'; // remove \n
					hflags[i].mainthreadid = 0xFFFF;
					hflags[i].taskid = 0xFFFF;
					break;
				case 1:
					hflags[i].flags = 0;
					/* flags (lowmem, phystartmsg, none) comma separated */
					if(strstr(rbuff,"lowmem"))
					{
						hflags[i].flags = hflags[i].flags | IFS2SRV_FLAG_LOWMEM;
					}
					if(strstr(rbuff,"phystartmsg"))
					{
						hflags[i].flags = hflags[i].flags | IFS2SRV_FLAG_PHYSTART;
					}
					if(strstr(rbuff,"disabled"))
					{
						hflags[i].flags = hflags[i].flags | IFS2SRV_FLAG_IGNORE;
					}
					break;
				case 2:
					/* type (hdd, fs, normal) exclusive */
					if(strstr(rbuff,"fs"))
					{
						hflags[i].pman_type = IFS2SRV_PMTYPE_MAINFS;
					}
					else if(strstr(rbuff,"hdd"))
					{
						hflags[i].pman_type = IFS2SRV_PMTYPE_HDD;
					}
					else
					{
						hflags[i].pman_type = 0;
					}
					break;
				case 3: /* Task id */
					hflags[i].taskid = (0xFFFF & atoi(rbuff));
					break;
				case 4: /* Task id */
					hflags[i].mainthreadid = (0xFFFF & atoi(rbuff));
					break;					
			}
			nest++;
			if(nest == 5) nest = 0;
		}
	}
}

int get_imgflags_pos(char *imgname)
{
	int i = 0;
	while(i < hflags_size)
	{
		if(strcmp(imgname, hflags[i].imgname) == 0)
			return i;
		i++;
	}

	return -1;
}

int set_flags(char *imgname, struct ifs2srv_header *header)
{
	int pos = get_imgflags_pos(imgname);

	if(pos == -1) return 0;

	header->flags = hflags[pos].flags;
	header->pman_type = hflags[pos].pman_type;
	header->task = hflags[pos].taskid;
	header->main_thread = hflags[pos].mainthreadid;

	return 1;
}

int israw(const char filename[], char *dest) 
{
	int i;
 
    i = strlen(filename) - 4;
    if ( strcmp(&filename[i], ".img") == 0 && strcmp(filename, dest) != 0) 
		return 1;
    else 
		return 0;    
}
