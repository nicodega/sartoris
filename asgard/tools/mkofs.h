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
#include <services/ofsservice/ofs.h>

struct ofsinfo
{
	struct OFST ofst;
	struct GH gh;
	char *nodes_bitmap;
	char *blocks_bitmap;
	int nodes_bitmap_size;
	int blocks_bitmap_size;
	struct node rootn;
	FILE *f;
	char pbuffer[OFS_BLOCK_SIZE * 2];
};

int last_index_of(char *str, char c);
int istrprefix(int start,char* str1, char* str2);
int first_index_of(char *str, char c, int start);
int len(const char* str);

int addinternal(char *img, char *path, char *filename, int type);
int bitmaps_size(int node_count, int block_count);
int format(char *device_filename, unsigned int size, unsigned int metadatasize);
int add(char *img, char *path, char *file);
int mkd(char *img, char *path);
int mkde(char *img, char *path, char *service, unsigned int logicdevice);
void invalid_usage();
int load_info(struct ofsinfo *inf, char *img_name);
int free_info(struct ofsinfo *inf, int cancel);
int write_block(struct ofsinfo *inf, char *buffer, int lba);
int read_block(struct ofsinfo *inf, char *buffer, int lba);
int write_node(struct ofsinfo *inf, struct node *n, unsigned int nodeid);
int read_node(struct ofsinfo *inf, struct node *n, unsigned int nodeid);
unsigned int *get_free(char *bitmap, unsigned int bsize, int count, int offset, int mul);
unsigned int *get_free_nodes(struct ofsinfo *inf, int count);
unsigned int *get_free_blocks(struct ofsinfo *inf, int count);
int parse_dir(struct ofsinfo *inf, char *path, struct node *dir_node, struct node *parsed_node, int *dir_exists, int *nodeid, int *dnodeid);
int create_dentry(struct ofsinfo *inf, struct node *directory_node, unsigned int dirnodeid, struct node *out_newnode, unsigned int *out_nodeid, char *file_name, int filename_start, int node_type, int flags);

#define MIN(a,b) ((a > b)? b : a)
#define MAX(a,b) ((a > b)? a : b)
