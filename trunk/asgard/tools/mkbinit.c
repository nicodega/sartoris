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

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include "../services/p-manager/initfs.h"

#define MAX_FILES 20

int israw(const char filename[], char *dest);

int main(int argc, char** args) {
  int i;
  int filecount=0;
  DIR *dirp;
  struct dirent *curdir;
  struct stat fileattr;
  struct service_header headers[MAX_FILES];
  char files[MAX_FILES][MAX_NAME_SIZE];
  struct init_header header;
  int offset;
  char command[MAX_FILES];
  FILE* filep;
  
  /* check the command line arguments */
  if ( argc < 2) {
    printf ("Too few arguments\n");
    return 1;
  }
  
  
  /* get the .raw file list   */
  
  /* open the current directory */
  if (!(dirp = opendir("."))) {
    printf("Error opening directory\n");
    return 1;
  }
  
  /* get the list of the .raw files */
  while (curdir = readdir(dirp)) {
    /* if the entry is a .raw file */
    if ( israw(curdir->d_name, args[1])) {
      if ( strlen(curdir->d_name) >= MAX_NAME_SIZE) {
	printf ("%s exceeded %d chars\n", curdir->d_name, MAX_NAME_SIZE);
	return 1;
      } else {
	strcpy(files[filecount], curdir->d_name);
	filecount++;
      }
    }
  }
  
  closedir(dirp);

  header.services_count = filecount;

  printf("\nCreating Init Services Image....\n\n");

  /* generate the initfs table  */
	
  /* for each file, calculate the header */
  offset = filecount * sizeof(struct service_header) + sizeof(struct init_header);

  for ( i = 0; i < filecount; i++ ) 
  {
    stat(files[i], &fileattr);
    strncpy(headers[i].image_name, files[i], strlen(files[i]) - 4);
    headers[i].image_name[ strlen(files[i]) - 4] = '\0'; // remove .img
    headers[i].image_size = fileattr.st_size;
    headers[i].image_pos = offset;
    printf("Service image file %s (%s) : (%d bytes), pos: %d\n", 
	   files[i],
	   headers[i].image_name, 
	   headers[i].image_size,
	   headers[i].image_pos);
    offset += headers[i].image_size;
  }

  /* write the headers in the output file */
  
  /* open the output file */
  if (!(filep = fopen(args[1], "wb"))) {
    printf("Error opening the output file %s\n",args[1]);
    return 1;
  }
  
  /* write Init Header */
  if (fwrite (&header, sizeof(struct init_header), 1, filep) != 1) {
    printf("Error writting Init Header in the output file %s\n",args[1]);
    return 1;
  }
  
  /* write headers */
  if (fwrite(headers, sizeof(struct service_header), filecount, filep) != filecount) 
  {
	printf("Error writting Service Headers in the output file %s\n",args[1]);
	return 1;
  }

  /* close the otuput file */
  fclose(filep);

  /* append the files info to the bFAT image */
  for ( i=0; i < filecount; i++) {
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

int israw(const char filename[], char *dest) {
  int i;
 
    i = strlen(filename)-4;
    if ( strcmp(&filename[i], ".img") == 0 && strcmp(filename, dest) != 0) {
      return 1;
    } else {
      return 0;
    }
  
}
