/*
*	Shell Service.
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

/*
*
*	Shell Change dir command
*
*/

#include "shell_iternals.h"

int change_dir(int term, char* dir){
  char* current_dir;
  int r, index;

  // get current path
  current_dir = get_env("CURRENT_PATH", term);

  r = build_dir(dir, current_dir, term);

  // resolve path
  if( r == 2 || r == 1 )
  {
	if(streq(dir, current_dir)) return 1;

	if(filetype(dir) != IOLIB_FILETYPE_DIRECTORY)
	{
	    term_color_print(term, "cd -> ", 7);
	    term_color_print(term, dir, 7);
	    term_color_print(term, ": No such directory.\n", 7);
	    return 0;
	}	   
	
	// change directory
	free(current_dir);
	current_dir = (char*)malloc(len(dir) + 1);
	memcpy(current_dir, dir, len(dir) + 1);

	set_env("CURRENT_PATH", current_dir, term);

	if(streq(current_dir, "/"))
	{
		csl_dir[term][0] = '/';
		csl_dir[term][1] = '\0';
	}
	else
	{	
		index = last_index_of(current_dir, '/');
		estrcopy(current_dir, csl_dir[term], ((index == -1)? 0 : index + 1), len(current_dir) - index);
	}
  } 
  else if(r == 0)
  {
      term_color_print(term, "Invalid Path.\n", 7);
  }

  return 1;
}

// uses build dir, but assumes dir is not a fixed size buffer
char *build_path(char *dir, int term)
{
	char *tmp = (char*)malloc(256);
	char *d = get_env("CURRENT_PATH", term);
	int r;

	memcpy(tmp, dir, len(dir) + 1);

	r = build_dir(tmp, d, term);

	if(r == 0) return NULL;

	d = (char*)malloc(len(tmp) + 1);
	
	memcpy(d, tmp, len(tmp) + 1);
	free(tmp);

	return d;
}
// return values are:
// - Invalid Path 0 
// - resolved 1
// - absolute 2
int build_dir(char *dir, char *current_dir, int term)
{
	char temp[256] = {'\0',};
	int i = 0, j = 0, k = 0, dlen, index;

	dlen = len(dir);

	if(dir[0] == '/') return 2; // path is absolute

	if(dir[dlen - 1] == '/')
	{
		// trim last /
		dir[dlen - 1] = '\0';
		dlen--;
	}

	strcat(temp, current_dir);		

	j = len(current_dir); // current dir won't have a / unless it's the root

	// resolve ../
	while(dir[i] != '\0' && i < dlen)
	{
		if(dlen && dir[i] == '.' && dir[i + 1] == '\0')
		{
			// ignore ./
			i++;
		}
		else if  (i + 1 < dlen && dir[i] == '.' && dir[i + 1] == '/')
		{
			i += 2;
		}
		else if(i + 1 < dlen && dir[i] == '.' && dir[i + 1] == '.' && ((i+2 < dlen && dir[i + 2] == '/') || dir[i + 2] == '\0'))
		{
			if(j == 1) return 0; // cannot go back beyond /

			// go back one dir on the path structure
			index = last_index_of(temp, '/');
			index = len(temp) - index;
			j -= index;
			if(j == 0)
			{
				temp[0] = '/';
				temp[1] = '\0';
				j = 1;
			}
			else
			{
				temp[j] = '\0';
			}

			// increment i
			if(dir[i + 2] == '\0')
				i += 2;
			else
				i += 3;	
		}
		else
		{
			// append / if temp is not the root
			if(j != 1)
			{
				temp[j] = '/';
				j++;
			}

			// copy dir name to temp
			index = first_index_of(dir, '/', i);
			index = ((index == -1)? dlen - i : index - i);

			estrcopy(dir, temp + j, i, index);

			j += index;
			temp[j] = '\0';

			i = first_index_of(dir, '/', i);
			if(i == -1) i = dlen; // finished searching
			i++; // remove trailing / if any
		}
	}

	dlen = len(temp);

	// remove trailing / if it's not the root
	if(temp[dlen - 1] == '/' && dlen != 1)
	{
		// trim last /
		temp[dlen] = '\0';
		dlen--;
	}

	// copy dir resolution to dir as an absolute path
	if(dlen + 1 > MAX_COMMAND_LEN)
	{
		term_color_print(term, "Path size not supported by shell service.\n", 7);
		return 0;
	}

	memcpy(dir, temp, dlen + 1);

        return 1;
}
