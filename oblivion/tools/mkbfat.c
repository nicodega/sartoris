#include <lib/bfilesys.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_FILES 256
#define MAX_FILE_SIZE 50

int israw(const char filename[]);

int main(int argc, char** args) {
  int i;
  int filecount=0;
  DIR *dirp;
  struct dirent *curdir;
  struct stat fileattr;
  char files[MAX_FILES][MAX_FILE_SIZE];
  struct fatline fatable[MAX_FILES];
  int	offset;
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
    if ( israw(curdir->d_name)) {
      if ( strlen(curdir->d_name) >= 50) {
	printf ("%s exceeded 50 chars\n", curdir->d_name);
	return 1;
      } else {
	strcpy(files[filecount], curdir->d_name);
	filecount++;
      }
    }
  }
  
  closedir(dirp);

  /* generate the bFAT table  */
	
  /* for each file, calculate the fat entry */
  offset = sizeof(int) + sizeof(struct fatline) * filecount;
  for ( i = 0; i < filecount; i++ ) {
    stat(files[i], &fileattr);
    strncpy(fatable[i].filename, files[i], strlen(files[i]) - 4);
    fatable[i].filesize = fileattr.st_size;
    fatable[i].filepos = (void*)offset;
    printf("File %s : %d -> %d (%d bytes)\n", 
	   files[i], 
	   fatable[i].filepos, 
	   fatable[i].filepos + fatable[i].filesize, 
	   fatable[i].filesize);
    offset += fatable[i].filesize;
  }

  /* write the bFAT table in the output file */
  
  /* open the output file */
  if (!(filep = fopen(args[1], "wb"))) {
    printf("Error opening the output file %s\n",args[1]);
    return 1;
  }
  
  /* write the filenum field of the bFAT */
  if (fwrite (&filecount, sizeof(int), 1, filep) != 1) {
    printf("Error writting in the output file %s\n",args[1]);
    return 1;
  }
  
  /* write the file fields of the bFAT */
  if (fwrite(fatable, sizeof(struct fatline), filecount, filep) != filecount) {
    printf("Error writting in the output file %s\n",args[1]);
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
  
  return 0;
}

int israw(const char filename[]) {
  int i;
  if (strlen(filename) < 5) {
    return 0;
  } else {
    i = strlen(filename)-4;
    if ( strcmp(&filename[i], ".raw") == 0) {
      return 1;
    } else {
      return 0;
    }
  }
}
