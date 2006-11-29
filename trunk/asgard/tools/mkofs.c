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


#include "mkofs.h"

struct ofsinfo ofsinf;

int skip = OFS_BLOCKDEV_BLOCKSIZE;
int skip_blocks = 1;

// usage:
// makeofs -option option-params
// where options are:
//	no option Creates an empty ofs, named ofsfloppy.img. 
//	c	Creates an empty ofs. params are image name, image size, metadata size
//		image name and image size must be suplied.
//	a	Adds a file to the specified image at the specified path. (eg mkofs a imgname ofs-path filename)
// if no options are specified a floppy image named ofsfloppy.img will be created.
//	d	creates a dir. params are: imgname dir-path
//	dev	Creates a device file. parameters are: imgname dev-path service-name logic-devid
int main(int argc, char** args) 
{
	if(argc == 1)
	{
		return format("ofsfloppy.img" ,1474048, 0);
	}
	else if(argc > 1)
	{
		if(strcmp(args[1], "c") == 0)
		{
			if(argc != 4 && argc != 5)
			{
				invalid_usage();
				return 0;
			}
			if(argc == 4)
			{
				return format(args[2] , atol(args[3]), 0);
			}
			else
			{
				return format(args[2] , atol(args[3]), atol(args[4]));
			}
		}
		else if(strcmp(args[1], "a") == 0)
		{
			if(argc != 5)
			{
				invalid_usage();
				return 0;
			}
			return add(args[2] , args[3], args[4]);
		}
		else if(strcmp(args[1], "dev") == 0)
		{
			if(argc != 6)
			{
				invalid_usage();
				return 0;
			}
			return mkde(args[2] , args[3], args[4], atoi(args[5]));
		}
		if(strcmp(args[1], "d") == 0)
		{
			if(argc != 4)
			{
				invalid_usage();
				return 0;
			}
			return mkd(args[2] , args[3]);
		}
		else
		{
			invalid_usage();
			return 0;
		}
	}

}

void invalid_usage()
{
	printf("usage:\nmakeofs -option option-params\nwhere options are:\n\tc\tCreates an empty ofs, named ofsfloppy.img.\n\t\timage name, image size and metadata size must be suplied.\n\ta\tAdds a file to the specified image at the specified path. (eg mkofs a imgname ofs-path filename)\n\t\tif no options are specified a floppy image named ofsfloppy.img will be created.\n\td\tcreates a dir. params are: imgname dir-path\n\tdev\tCreates a device file. parameters are: imgname dev-path service-name logic-devid\n");
}

int bitmaps_size(int node_count, int block_count)
{
	int nodes = ((int)(node_count / BITS_PER_DEVBLOCK) + ((node_count % BITS_PER_DEVBLOCK == 0)? 0 : 1));
	int blocks = ((int)(block_count / BITS_PER_DEVBLOCK) + ((block_count % BITS_PER_DEVBLOCK == 0)? 0 : 1));
	return nodes * OFS_BLOCKDEV_BLOCKSIZE + blocks * OFS_BLOCKDEV_BLOCKSIZE;
}

int format(char *device_filename, unsigned int storage_size, unsigned int metadatasize)
{
	FILE *devf = NULL;
	int count = 0;
	struct OFST ofst;
	struct GH gh;
	struct node rootn;
	int block_count, node_count, remaining, proposed;
	int lbabitmaps_end, i ,j;
	char bitmap_buff[OFS_BLOCKDEV_BLOCKSIZE];
	long filesize;

	unsigned int metadata_sec = (metadatasize / OFS_BLOCKDEV_BLOCKSIZE) + ((metadatasize % OFS_BLOCKDEV_BLOCKSIZE == 0)? 0 : 1);

	printf("Metadata size=%x, sect=%x\n", metadatasize, metadata_sec);
	
	// create OFS Table
	ofst.first_group = 1 + skip_blocks + metadata_sec;
	ofst.group_count = 1;
	ofst.mount_count = 0;
	ofst.block_size = OFS_BLOCK_SIZE;
	ofst.Magic_number = OFS_MAGIC_NUMBER;
	ofst.MetaData = (metadata_sec != 0)? (1 + skip_blocks) : 0;
	ofst.ptrs_on_node = PTRS_ON_NODE;
	ofst.node_size = 128;

	// calculate block and node count based on space required for headers, bitmaps, etc
	block_count = 0;
	node_count = 0;
	remaining = storage_size - OFS_BLOCKDEV_BLOCKSIZE * (2 + skip_blocks + metadata_sec);

	// maximize block and node count 
	proposed = OFS_BLOCKDEV_BLOCKSIZE + OFS_BLOCK_SIZE * OFS_NODESPER_BLOCKDEV_BLOCK;
	proposed += bitmaps_size(node_count + OFS_NODESPER_BLOCKDEV_BLOCK, block_count + OFS_NODESPER_BLOCKDEV_BLOCK);

	// atempt to assign an equal ammount of nodes and blocks, if not possible assign more blocks
	while(remaining - proposed >= 0)
	{
		block_count += OFS_NODESPER_BLOCKDEV_BLOCK;
		node_count += OFS_NODESPER_BLOCKDEV_BLOCK;

		remaining -= OFS_BLOCKDEV_BLOCKSIZE + OFS_BLOCK_SIZE * OFS_NODESPER_BLOCKDEV_BLOCK;
		proposed = OFS_BLOCKDEV_BLOCKSIZE + OFS_BLOCK_SIZE * OFS_NODESPER_BLOCKDEV_BLOCK;
		proposed += bitmaps_size(node_count + OFS_NODESPER_BLOCKDEV_BLOCK, block_count + OFS_NODESPER_BLOCKDEV_BLOCK);
	}

	// fill remaining space with blocks //
	proposed = OFS_BLOCK_SIZE + bitmaps_size(node_count, block_count + 1);

	while(remaining - proposed > 0)
	{
		block_count++;
		proposed = OFS_BLOCK_SIZE + bitmaps_size(node_count, block_count + 1);
		remaining -= OFS_BLOCK_SIZE;
	}

	ofst.nodes_per_group = node_count;
	ofst.blocks_per_group = block_count;

	printf("Will create %d Nodes, %d Blocks\n", node_count, block_count);

	// write tables and nodes
	devf = fopen(device_filename, "wb");

	// create OFS disk with one group //
	if(devf == NULL)
	{
		printf("Could not open file\n");
		return 1; // FAIL
	}

	// skip initial sectors
	if(fseek(devf, skip, SEEK_SET))
	{
		printf("Seek failed\n");		
		return 1; // seek
	}

	// write ofst to media
	if(fwrite(&ofst, 1, sizeof(struct OFST), devf) != sizeof(struct OFST))
	{
		printf("Could not write OFST\n");	
		return 1;
	}

	// create group header
	gh.blocks_bitmap_offset = 2 + skip_blocks + metadata_sec;
	gh.nodes_bitmap_offset = 2 + skip_blocks + metadata_sec + bitmaps_size(0, block_count) / OFS_BLOCKDEV_BLOCKSIZE;
	gh.blocks_per_group = block_count;
	gh.nodes_per_group = node_count;
	gh.group_id = 0;
	gh.group_size = OFS_BLOCKDEV_BLOCKSIZE + bitmaps_size(node_count, block_count) + OFS_BLOCKDEV_BLOCKSIZE * (node_count / OFS_NODESPER_BLOCKDEV_BLOCK) + OFS_BLOCK_SIZE * block_count;
	gh.meta_data = OFS_NULL_VALUE;
	gh.meta_data_size = 0;
	gh.nodes_table_offset = gh.nodes_bitmap_offset + bitmaps_size(node_count, 0) / OFS_BLOCKDEV_BLOCKSIZE;
	gh.blocks_offset = gh.nodes_bitmap_offset + bitmaps_size(node_count, 0)  / OFS_BLOCKDEV_BLOCKSIZE + (int)(node_count / OFS_NODESPER_BLOCKDEV_BLOCK) + ((node_count % OFS_NODESPER_BLOCKDEV_BLOCK == 0)? 0 : 1);

	printf("Offsets will be:\nBlock BMP: %d\nNodes BMP: %d\nBlocks TBL: %d\nNodes TBL: %d\n",
		gh.blocks_bitmap_offset, gh.nodes_bitmap_offset, gh.blocks_offset, gh.nodes_table_offset);	

	// write Group Header to media
	if(fseek(devf, OFS_BLOCKDEV_BLOCKSIZE + skip + metadatasize, SEEK_SET))
	{
		printf("Seek failed\n");		
		return 1; // seek
	}

	if(fwrite(&gh, 1, sizeof(struct GH), devf) != sizeof(struct GH))
	{
		printf("Could not write GH\n");	
		return 1;
	}

	// initialize bitmaps
	if(fseek(devf, gh.blocks_bitmap_offset * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
	{
		printf("Could not seek for bitmaps\n");	
		return 1; // seek until bitmap possition
	}
	
	i = gh.blocks_bitmap_offset;
	j = 0;
	lbabitmaps_end = bitmaps_size(node_count, block_count) / OFS_BLOCKDEV_BLOCKSIZE + gh.blocks_bitmap_offset;
	
	// init zeroing buffer
	while(j < OFS_BLOCKDEV_BLOCKSIZE)
	{
		bitmap_buff[j] = 0;
		j++;
	}

	i++;

	while(i < lbabitmaps_end)
	{
		if(fwrite(bitmap_buff, 1, OFS_BLOCKDEV_BLOCKSIZE, devf) != OFS_BLOCKDEV_BLOCKSIZE)
		{
			printf("Could not write Bitmaps\n");	
			return 1;
		}
		i++;
	}

	if(fseek(devf, gh.nodes_bitmap_offset * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
	{
		printf("Could not seek for nodes bitmaps\n");	
		return 1; 
	}

	bitmap_buff[0] = 0x80; // set first node as used for it's the root dir
	
	if(fwrite(bitmap_buff, 1, OFS_BLOCKDEV_BLOCKSIZE, devf) != OFS_BLOCKDEV_BLOCKSIZE)
	{
		printf("Could not write nodes Bitmap, first block\n");	
		return 1;
	}

	// create root node
	i = PTRS_ON_NODE - 1;
	while(i >= 0)
	{
		rootn.blocks[i] = OFS_NULL_VALUE;
		i--;
	}

	rootn.file_size = 0;
	rootn.link_count = 0;
	rootn.next_node = OFS_NULL_VALUE;
	rootn.type = OFS_DIRECTORY_FILE;
	rootn.creation_date = 0;
	rootn.modification_date = 0;
	rootn.owner_group_id = 0;
	rootn.owner_id = 0;
	rootn.protection_mode = OFS_PROTECTIONMODE_OUR | OFS_PROTECTIONMODE_OGR | OFS_PROTECTIONMODE_OTR;

	// write the node
	if(fseek(devf, gh.nodes_table_offset * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
	{
		printf("Could not seek ROOT node\n");	
		return 1;
	}

	if(fwrite(&rootn, 1, sizeof(struct node), devf) != sizeof(struct node)) 
	{
		printf("Could not write ROOT node\n");	
		return 1;
	}

	fclose(devf);

	devf = fopen(device_filename, "ab");
	fseek(devf, 0, SEEK_END);
	filesize = ftell(devf);
	
	if (filesize > storage_size) {
		printf("padding failed, file is too big! (%l bytes)", filesize);
	} else {
		while (filesize++ < storage_size)  {
			fputc(0, devf);

		}
	}
	
	fclose(devf);
	
	

	return 0; // ok
	
}

int mkde(char *img, char *path, char *service, unsigned int logicdevice)
{
	FILE *f;
	char *devtmp = "device.temp";

	// create a temporary file and then use add (awful but simple)
	f = fopen(devtmp, "wb");

	if(f == NULL)
	{
		printf("Could not open temp device file\n");	
		return 1;
	}

	/*
		int LOGIC DEVICE ID: internal ID on the service (4 BYTES)
		int service name size; (4 BYTES)
		char SERVICE NAME[]: name of the device driver (zero terminated string) 
	*/
	putw(logicdevice, f);
	putw(len(service), f);
	fwrite(service, 1, len(service)+1, f);

	fclose(f);

	int r = addinternal(img, path, devtmp, OFS_DEVICE_FILE);

	// remove the file
	remove(devtmp);

	return r; //FAIL
}

int add(char *img, char *path, char *filename)
{
	return addinternal(img, path, filename, OFS_FILE);
}

int addinternal(char *img, char *path, char *filename, int type)
{
	struct node parsed_node, newnode, dir_node, currnode;
	int dir_exists, nodeid, dnodeid, i;
	unsigned int *nodes = NULL, *blocks = NULL, ncount, bcount, bsize;
	long filesize;
	FILE *f;

	// create a file
	if(load_info(&ofsinf, img)) return 1;

	// check file does not exist
	if(!parse_dir(&ofsinf, path, &dir_node, &parsed_node, &dir_exists, &nodeid, &dnodeid) || !dir_exists)
	{
		if(!dir_exists)
		{
			printf("Path does not exists");
		}
		else
		{
			printf("Directory already exists");
		}
		free_info(&ofsinf, 1);
		return 1;
	}

	
	f = fopen(filename, "rb");

	if(f == NULL)
	{
		printf("Could not open imported file");
		free_info(&ofsinf, 1);
		return 1;
	}

	fseek(f, 0, SEEK_END);
	filesize = ftell(f);

	bcount = (filesize / OFS_BLOCK_SIZE) + ((filesize % OFS_BLOCK_SIZE == 0)? 0 : 1);
	ncount = (bcount / PTRS_ON_NODE) + ((bcount % PTRS_ON_NODE == 0)? 0 : 1);
	ncount--;

	// get free blocks and nodes for the new file
	if(bcount > 0)
	{
		blocks = get_free_blocks(&ofsinf, bcount);
		if(blocks == NULL)
		{
			printf("Could not get free blocks");
			free_info(&ofsinf, 1);
			return 1;
		}
	}
	if(ncount > 0)
	{
		nodes = get_free_nodes(&ofsinf, ncount);
		if(nodes == NULL)
		{
			printf("Could not get free blocks");
			free_info(&ofsinf, 1);
			free(blocks);
			return 1;
		}
	}

	// use create file 
	if(create_dentry(&ofsinf, &dir_node, dnodeid, &newnode, &nodeid, path, last_index_of(path, '/') + 1, type, OFS_NOFLAGS))
	{
		printf("File creation failed");
		free_info(&ofsinf, 1);
		fclose(f);
		free(blocks);
		free(nodes);
		return 1;
	}

	if(filesize > 0)
	{
		// copy contents of the file, to the ofs
		fseek(f, 0, SEEK_SET);
		int bl = 0, cn = 0, bindex = 0;
		long tot = 0;
		currnode = newnode;

		while(filesize - tot > 0)
		{
			int j;
			for(j = 0; j < OFS_BLOCK_SIZE * 2; j++) ofsinf.pbuffer[j] = 0;
			// read from source
			bsize = fread(ofsinf.pbuffer, 1, MIN(OFS_BLOCK_SIZE, filesize - tot), f);

			// write to dest
			if(bsize > 0)
			{
				currnode.blocks[bl] = blocks[bindex];
				if(cn == 0) newnode.blocks[bl] = blocks[bindex];
				
				write_block(&ofsinf, ofsinf.pbuffer, currnode.blocks[bl]);

				bl++;
				bindex++;

				if(bl == PTRS_ON_NODE && filesize - tot > 0)
				{
					// switch node
					if(cn == 0) newnode.next_node = nodes[cn];
					currnode.next_node = nodes[cn];

					write_node(&ofsinf, &currnode, (cn == 0)? nodeid : nodes[cn-1]);

					cn++;
					bl = 0;

					// reset node
					currnode.file_size = 0;
					currnode.creation_date = 666; // TODO SET FOR TODAY
					currnode.modification_date = 0;
					currnode.link_count = 0;
					currnode.next_node = OFS_NULL_VALUE;
					currnode.type = OFS_CHILD;
					currnode.owner_group_id = 0;
					currnode.owner_id = 0;
					currnode.protection_mode = OFS_PROTECTIONMODE_OUR | OFS_PROTECTIONMODE_OGR | OFS_PROTECTIONMODE_OTR;

					i = 0;
					while(i < PTRS_ON_NODE){currnode.blocks[i] = OFS_NULL_VALUE; i++;}
				}
			}

			tot += bsize;
		}

		newnode.file_size = filesize;

		// preserve base node
		write_node(&ofsinf, &newnode, nodeid);
	}
	
	fclose(f);

	free_info(&ofsinf, 0);

	return 0;
}

int mkd(char *img, char *path)
{
	struct node parsed_node, newnode, dir_node;
	int dir_exists, nodeid, dnodeid;

	// create a directory
	load_info(&ofsinf, img);

	// check file does not exist
	if(!parse_dir(&ofsinf, path, &dir_node, &parsed_node, &dir_exists, &nodeid, &dnodeid) || !dir_exists)
	{
		if(!dir_exists)
		{
			printf("Path does not exists");
		}
		else
		{
			printf("Directory already exists");
		}
		free_info(&ofsinf, 1);
		return 1;
	}

	// use create file 
	if(create_dentry(&ofsinf, &dir_node, dnodeid, &newnode, &nodeid, path, last_index_of(path, '/') + 1, OFS_DIRECTORY_FILE, OFS_NOFLAGS))
	{
		printf("File creation failed");
		free_info(&ofsinf, 1);
		return 1;
	}

	free_info(&ofsinf, 0);

	return 0;
}

int load_info(struct ofsinfo *inf, char *img_name)
{
	// open file 
	inf->f = fopen(img_name, "rb+");

	// create OFS disk with one group //
	if(inf->f == NULL)
	{
		printf("Could not open image file\n");
		return 1; // FAIL
	}

	// load ofst

	if(fseek(inf->f, skip, SEEK_SET))
	{
		printf("GH seek failed\n");		
		return 1; // seek
	}

	if(fread(&inf->ofst, 1, sizeof(struct OFST), inf->f) != sizeof(struct OFST))
	{
		printf("Could not read OFST\n");	
		return 1;
	}

	// load GH
	if(fseek(inf->f, inf->ofst.first_group * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
	{
		printf("GH seek failed\n");		
		return 1; // seek
	}

	if(fread(&inf->gh, 1, sizeof(struct GH), inf->f) != sizeof(struct GH))
	{
		printf("Could not read GH\n");	
		return 1;
	}

	// load bitmaps
	inf->nodes_bitmap_size = ((inf->ofst.nodes_per_group / 8) + ((inf->ofst.nodes_per_group % 8 == 0)? 0 : 1));
	inf->nodes_bitmap = (char *)malloc(inf->nodes_bitmap_size);

	if(fseek(inf->f, inf->gh.nodes_bitmap_offset * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
	{
		printf("Could not seek for bitmaps\n");	
		return 1; // seek until bitmap possition
	}

	if(fread(inf->nodes_bitmap, 1, inf->nodes_bitmap_size, inf->f) != inf->nodes_bitmap_size)
	{
		printf("Could not read nodes bitmap\n");	
		return 1;
	}

	inf->blocks_bitmap_size = (inf->ofst.blocks_per_group / 8) + ((inf->ofst.blocks_per_group % 8 == 0)? 0 : 1);
	inf->blocks_bitmap = (char *)malloc(inf->blocks_bitmap_size);

	if(fseek(inf->f, inf->gh.blocks_bitmap_offset * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
	{
		printf("Could not seek for bitmaps\n");	
		return 1; // seek until bitmap possition
	}

	if(fread(inf->blocks_bitmap, 1, inf->blocks_bitmap_size, inf->f) != inf->blocks_bitmap_size)
	{
		printf("Could not read blocks bitmap\n");	
		return 1;
	}

	// load rootn
	read_node(inf, &inf->rootn, 0);

	return 0;
}

int free_info(struct ofsinfo *inf, int cancel)
{
	// preserve bitmap changes
	if(!cancel)
	{
		if(fseek(inf->f, inf->gh.nodes_bitmap_offset * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
		{
			printf("Could not seek for bitmaps\n");	
			return 1; // seek until bitmap possition
		}

		if(fwrite(inf->nodes_bitmap, 1, inf->nodes_bitmap_size, inf->f) != inf->nodes_bitmap_size)
		{
			printf("Could not write nodes bitmap\n");	
			return 1;
		}

		if(fseek(inf->f, inf->gh.blocks_bitmap_offset * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
		{
			printf("Could not seek for bitmaps\n");	
			return 1;
		}

		if(fwrite(inf->blocks_bitmap, 1, inf->blocks_bitmap_size, inf->f) != inf->blocks_bitmap_size)
		{
			printf("Could not write nodes bitmap\n");	
			return 1;
		}
	}

	// free bitmaps
	free(inf->blocks_bitmap);
	free(inf->nodes_bitmap);

	fclose(inf->f);

	return 0;
}

int write_block(struct ofsinfo *inf, char *buffer, int lba)
{
	if(fseek(inf->f, lba * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
	{
		printf("Could not seek block lba: %d\n", lba);	
		return 1;
	}

	if(fwrite(buffer, 1, OFS_BLOCK_SIZE, inf->f) != OFS_BLOCK_SIZE)
	{
		printf("Could not write block lba: %d\n", lba);		
		return 1;
	}

	return 0;
}

int read_block(struct ofsinfo *inf, char *buffer, int lba)
{
	if(fseek(inf->f, lba * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
	{
		printf("Could not seek block lba: %d\n", lba);	
		return 1;
	}

	if(fread(buffer, 1, OFS_BLOCK_SIZE, inf->f) != OFS_BLOCK_SIZE)
	{
		printf("Could not read block lba: %d\n", lba);		
		return 1;
	}

	return 0;
}

int write_node(struct ofsinfo *inf, struct node *n, unsigned int nodeid)
{
	unsigned int node_pos = inf->gh.nodes_table_offset * OFS_BLOCKDEV_BLOCKSIZE + nodeid * sizeof(struct node);

	if(fseek(inf->f, node_pos, SEEK_SET))
	{
		printf("Could not seek node: %d\n", nodeid);	
		return 1;
	}

	if(fwrite((char*)n, 1, sizeof(struct node), inf->f) !=  sizeof(struct node))
	{
		printf("Could not write node: %d\n", nodeid);		
		return 1;
	}

	return 0;
}

int read_node(struct ofsinfo *inf, struct node *n, unsigned int nodeid)
{
	unsigned int node_pos = inf->gh.nodes_table_offset * OFS_BLOCKDEV_BLOCKSIZE + nodeid * sizeof(struct node);

	if(fseek(inf->f, node_pos, SEEK_SET))
	{
		printf("Could not seek node: %d\n", nodeid);	
		return 1;
	}

	if(fread((char*)n, 1, sizeof(struct node), inf->f) !=  sizeof(struct node))
	{
		printf("Could not read node: %d\n", nodeid);		
		return 1;
	}

	return 0;
}

unsigned int *get_free_nodes(struct ofsinfo *inf, int count)
{
	return get_free(inf->nodes_bitmap, inf->nodes_bitmap_size, count, 0, 1);
}

unsigned int *get_free_blocks(struct ofsinfo *inf, int count)
{
	return get_free(inf->blocks_bitmap, inf->blocks_bitmap_size, count, inf->gh.blocks_offset, 8);
}

unsigned int *get_free(char *bitmap, unsigned int bsize, int count, int offset, int mul)
{
	int *ret = (int*)malloc(count * sizeof(unsigned int));
	unsigned int i = 0, k = 0, j = 0, curr = 0;
	
	while(i < bsize && k < count)
	{
		// check each char
		j = 0;
		while(j < 8 && k < count)
		{
			if((bitmap[i] & 0xFF) == 0xFF)
			{
				curr += 8;
				break;
			}
			if((bitmap[i] & (0x1 << (7 - j))) == 0)
			{
				ret[k] = curr;
				k++;
			}
			j++;
			curr++;
		}
		i++;
	}

	if(k != count) 
	{
		free(ret);
		return NULL;
	}

	// set selected ones as used
	i = 0;
	while(i < count)
	{
		k = (ret[i] / 8);
		j = ret[i] % 8;
		bitmap[k] = bitmap[k] | (0x1 << (7 - j));
		ret[i] = ret[i] * mul + offset;
		i++;
	}

	return ret;
}


int last_index_of(char *str, char c)
{
	int i = 0, k = -1;

	if(str == NULL) return -1;

	while(str[i] != '\0'){
		if(str[i] == c) k = i;
		i++;	
	}
	return k;
}

int first_index_of(char *str, char c, int start)
{
	int i = start;

	if(str == NULL) return -1;

	while(str[i] != '\0'){
		if(str[i] == c) break;
		i++;	
	}

	if(str[i] == '\0') return -1;	

	return i;
}

// this will create a node and a dir entry for it
int create_dentry(struct ofsinfo *inf, struct node *directory_node, unsigned int dirnodeid, struct node *out_newnode, unsigned int *out_nodeid, char *file_name, int filename_start, int node_type, int flags)
{
	// NOTE: This function is based on the fact that no dirname 
	// can be greater than something like 500 bytes... this is
	// a service restriction for an easier implementation, my
	// apologies for that.
	struct node dir_node, dir_basenode, new_dirnode;
	struct sdir_entry *dentry = NULL;
	int i = 0, dir_base_nodeid;
	unsigned int k = 0;
	unsigned int j = 0;
	unsigned int *free_nodeid = NULL, *newdir_nodeid = NULL, *newdir_blockid = NULL, *newdir_base_blockid = NULL;
	unsigned int dir_block, second_dir_block, dir_block_blockdev, block_offset, blockdev_offset, dirbuffer_size;
	unsigned long long dir_used_size;
	char *dir_buffer = NULL, *buffer_name = NULL;

	dir_base_nodeid = dirnodeid;

	i = 0;
	// initialize new node
	while(i < PTRS_ON_NODE)
	{
		out_newnode->blocks[i] = OFS_NULL_VALUE;
		i++;
	}

	out_newnode->file_size = 0;
	out_newnode->creation_date = 666; // TODO SET FOR TODAY
	out_newnode->modification_date = 0;
	out_newnode->link_count = 1;
	out_newnode->next_node = OFS_NULL_VALUE;
	out_newnode->type = node_type;
	out_newnode->owner_group_id = 0;
	out_newnode->owner_id = 0;
	out_newnode->protection_mode = OFS_PROTECTIONMODE_OUR | OFS_PROTECTIONMODE_OGR | OFS_PROTECTIONMODE_OTR;

	// get a free node
	free_nodeid = get_free_nodes(inf, 1);

	if(free_nodeid == NULL){ return 1; }

	// write node to disk
	if(write_node(inf, out_newnode, *free_nodeid))
	{
		return 1;
	}


	// create the node on the dir

	// get dir last node and calculate
	// write possition on the last node
	dir_basenode = dir_node = *directory_node;
	dir_used_size = dir_basenode.file_size;

	while(dir_node.next_node != OFS_NULL_VALUE)
	{
		dirnodeid = dir_node.next_node;

		if(!read_node(inf, &dir_node, dir_node.next_node))
		{
			return 1;
		}

		dir_used_size -= PTRS_ON_NODE * OFS_BLOCK_SIZE;
	}

	// now dir_node has the dir last node, and dir_used_size 
	// has the size of the file on that node

	// set dir_block to the first block being written on the last node
	dir_block = (unsigned int)dir_used_size / OFS_BLOCK_SIZE + ((dir_used_size % OFS_BLOCK_SIZE == 0 && dir_used_size != 0)? 1 : 0);
	// set block_offset to the offset on the last block
	block_offset = (unsigned int)dir_used_size % OFS_BLOCK_SIZE;
	// set blockdev_offset to the offset on the last devblock
	blockdev_offset = (unsigned int)dir_used_size % OFS_BLOCKDEV_BLOCKSIZE;
	// set dir_block_blockdev to the index of the blockdev on the block 
	dir_block_blockdev = (unsigned int)(blockdev_offset / OFS_BLOCKDEV_BLOCKSIZE);

	// new dir entry might cross two blocks boundaries //
	// second_dir_block:
	//			if -1 then there is no second block
	//			if 0 it means a new node is needed for the second block
	//			if non zero then it holds dir_block + 1 (meaning the second blockdev goes on the next block)
	// dirbuffer_size is set to the size of the dir buffer
	if((OFS_BLOCK_SIZE - block_offset < (sizeof(struct sdir_entry) + len(file_name) - filename_start + 1)))
	{
		// new dir entry crosses two blocks boundaries //
		if(dir_block == PTRS_ON_NODE - 1 && dir_block_blockdev == OFS_BLOCKDEVBLOCK_SIZE - 1 )
		{
			// dentry needs two blockdevs and the second blockdev does NOT fit on this node
			dirbuffer_size = OFS_BLOCKDEV_BLOCKSIZE << 1;
			second_dir_block = 0;
		}
		else if(dir_block_blockdev == OFS_BLOCKDEVBLOCK_SIZE - 1)
		{
			// dentry needs two blockdevs and the second blockdev does fit on this node
			dirbuffer_size = OFS_BLOCKDEV_BLOCKSIZE << 1;
			second_dir_block = dir_block + 1;
		}
		else
		{
			// the two devblocks fit on the current block
			dirbuffer_size = OFS_BLOCKDEV_BLOCKSIZE << 1;
			second_dir_block = -1;
		}
	}
	else
	{
		// only one block is needed
		dirbuffer_size = OFS_BLOCKDEV_BLOCKSIZE;
		second_dir_block = -1;
	}

	int jj;
	for(jj = 0; jj < OFS_BLOCK_SIZE * 2; jj++) ofsinf.pbuffer[jj] = 0;

	dir_buffer = (char*)inf->pbuffer; // use parsing buffer...

	newdir_base_blockid = NULL;

	// get bytes for first block
	if(dir_node.blocks[dir_block] != OFS_NULL_VALUE && dir_node.file_size > 0)
	{
		if(fseek(inf->f, (dir_node.blocks[dir_block] + dir_block_blockdev) * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
		{
			printf("Create dentry failed\n");	
			return 1;
		}

		if(fread(dir_buffer, 1, OFS_BLOCKDEV_BLOCKSIZE, inf->f) !=  OFS_BLOCKDEV_BLOCKSIZE)
		{
			printf("Create dentry failed\n");	
			return 1;
		}
	}
	else if(dir_node.blocks[dir_block] == OFS_NULL_VALUE)
	{
		// get a new block for the directoy, for it's empty and no block has been
		// assigned yet

		newdir_base_blockid = get_free_blocks(inf, 1);

		if(newdir_base_blockid == NULL)
		{
			return 1;
		}

		dir_node.blocks[dir_block] = *newdir_base_blockid;
	}

	
	// if a new node is needed get one
	if(second_dir_block == 0)
	{
		// get a free node
		newdir_nodeid = get_free_nodes(inf, 1);

		if(newdir_nodeid == NULL)
		{
			free(free_nodeid);
			if(newdir_base_blockid != NULL)
			{
				free(newdir_base_blockid);
			}
			return 1;
		}

		
		i = 0;
		while(i < PTRS_ON_NODE)
		{
			new_dirnode.blocks[i] = OFS_NULL_VALUE;
			i++;
		}
		
		dir_node.next_node = *newdir_nodeid;

		new_dirnode.file_size = 0; 
		new_dirnode.creation_date = 0;
		new_dirnode.modification_date = 0;
		new_dirnode.link_count = 1;
		new_dirnode.type = OFS_CHILD;
		new_dirnode.protection_mode = 0;
		new_dirnode.owner_group_id = dir_basenode.owner_group_id;
		new_dirnode.owner_id = dir_basenode.owner_id;
		new_dirnode.next_node = OFS_NULL_VALUE;
	}

	
	// if a new block is needed get one
	if(second_dir_block != -1)
	{
		// get a free block
		newdir_blockid = get_free_blocks(inf, 1);

		if(newdir_blockid == NULL)
		{
			if(newdir_base_blockid != NULL)
			{
				free(newdir_base_blockid);
			}
			free(free_nodeid);
			if(newdir_nodeid != NULL) free(newdir_nodeid);
			return 1;
		}

		// set the new block pointer on the proper node
		if(second_dir_block == 0) 
		{
			new_dirnode.blocks[0] = *newdir_blockid;
		}
		else
		{
			dir_node.blocks[second_dir_block] = *newdir_blockid;
		}
	}

	// write dentry to buffer
	dentry = (struct sdir_entry *)(dir_buffer + blockdev_offset);
	dentry->flags = flags;
	dentry->name_length = len(file_name) - filename_start;
	dentry->nodeid = *free_nodeid;
	dentry->record_length = sizeof(struct sdir_entry) + dentry->name_length + 1;
	
	buffer_name = dir_buffer + (blockdev_offset + sizeof(struct sdir_entry));
	
	// copy file name
	j = filename_start;
	k = 0;
	
	while(k <= dentry->name_length)
	{
		buffer_name[k] = file_name[j];
		j++;
		k++;
	}
	
	if(dirnodeid == dir_base_nodeid)
	{
		// dir_node is the base node
		dir_node.file_size = dir_basenode.file_size + dentry->record_length;
	}
	else
	{
		dir_basenode.file_size = dir_basenode.file_size + dentry->record_length;
	}

	// preserve blocks on buffer
	if(second_dir_block == -1)
	{		
		// there is only one block to preserve
		if(fseek(inf->f, (dir_node.blocks[dir_block] + dir_block_blockdev) * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
		{
			printf("Create dentry failed\n");	
			free(free_nodeid);
			if(newdir_nodeid != NULL) free(newdir_nodeid);
			if(newdir_blockid != NULL) free(newdir_blockid);
			if(newdir_base_blockid != NULL)
			{
				free(newdir_base_blockid);
			}
			return 1;
		}

		if(fwrite(dir_buffer, 1, dirbuffer_size, inf->f) != dirbuffer_size)
		{
			printf("Create dentry failed\n");	
			free(free_nodeid);
			if(newdir_nodeid != NULL) free(newdir_nodeid);
			if(newdir_blockid != NULL) free(newdir_blockid);
			if(newdir_base_blockid != NULL)
			{
				free(newdir_base_blockid);
			}
			return 1;
		}
	}
	else
	{
		// there are two blocks

		// write first block
		if(fseek(inf->f, (dir_node.blocks[dir_block] + dir_block_blockdev) * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
		{
			printf("Create dentry failed\n");		
			free(free_nodeid);
			if(newdir_nodeid != NULL) free(newdir_nodeid);
			if(newdir_blockid != NULL) free(newdir_blockid);
			if(newdir_base_blockid != NULL)
			{
				free(newdir_base_blockid);
			}
			return 1;
		}

		if(fwrite(dir_buffer, 1, OFS_BLOCKDEV_BLOCKSIZE, inf->f) != OFS_BLOCKDEV_BLOCKSIZE)
		{
			printf("Create dentry failed\n");	
			free(free_nodeid);
			if(newdir_nodeid != NULL) free(newdir_nodeid);
			if(newdir_blockid != NULL) free(newdir_blockid);
			if(newdir_base_blockid != NULL)
			{
				free(newdir_base_blockid);
			}
			return 1;
		}
		
		if(second_dir_block == 0)
		{
			if(fseek(inf->f, new_dirnode.blocks[0] * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
			{
				printf("Create dentry failed\n");	
				free(free_nodeid);
				if(newdir_nodeid != NULL) free(newdir_nodeid);
				if(newdir_blockid != NULL) free(newdir_blockid);
				if(newdir_base_blockid != NULL)
				{
					free(newdir_base_blockid);
				}
				return 1;
			}

			if(fwrite(dir_buffer + OFS_BLOCKDEV_BLOCKSIZE, 1, OFS_BLOCKDEV_BLOCKSIZE, inf->f) != OFS_BLOCKDEV_BLOCKSIZE)
			{
				printf("Create dentry failed\n");		
				free(free_nodeid);
				if(newdir_nodeid != NULL) free(newdir_nodeid);
				if(newdir_blockid != NULL) free(newdir_blockid);
				if(newdir_base_blockid != NULL)
				{
					free(newdir_base_blockid);
				}
				return 1;
			}
			
		}
		else
		{
			if(fseek(inf->f, new_dirnode.blocks[second_dir_block] * OFS_BLOCKDEV_BLOCKSIZE, SEEK_SET))
			{
				printf("Create dentry failed\n");	
				free(free_nodeid);
				if(newdir_nodeid != NULL) free(newdir_nodeid);
				if(newdir_blockid != NULL) free(newdir_blockid);
				if(newdir_base_blockid != NULL)
				{
					free(newdir_base_blockid);
				}
				return 1;
			}

			if(fwrite(dir_buffer + OFS_BLOCKDEV_BLOCKSIZE, 1, OFS_BLOCKDEV_BLOCKSIZE, inf->f) != OFS_BLOCKDEV_BLOCKSIZE)
			{
				printf("Create dentry failed\n");		
				free(free_nodeid);
				if(newdir_nodeid != NULL) free(newdir_nodeid);
				if(newdir_blockid != NULL) free(newdir_blockid);
				if(newdir_base_blockid != NULL)
				{
					free(newdir_base_blockid);
				}
				return 1;
			}
		}
	}

	// preserve new dir node if any
	if(newdir_nodeid != NULL && write_node(inf, &new_dirnode, *newdir_nodeid))
	{
		free(free_nodeid);
		if(newdir_nodeid != NULL) free(newdir_nodeid);
		if(newdir_blockid != NULL) free(newdir_blockid);
		if(newdir_base_blockid != NULL)
		{
			free(newdir_base_blockid);
		}
		return 1;
	}
	
	// preserve dir last node if it has a new block, or it's the base node
	if((second_dir_block != -1 || dirnodeid == dir_base_nodeid) && write_node(inf, &dir_node, dirnodeid))
	{
		free(free_nodeid);
		if(newdir_nodeid != NULL) free(newdir_nodeid);
		if(newdir_blockid != NULL) free(newdir_blockid);
		if(newdir_base_blockid != NULL)
		{
			free(newdir_base_blockid);
		}
		return 1;
	}
	
	// preserve dir_basenode
	if(dirnodeid != dir_base_nodeid && write_node(inf, &dir_basenode, dir_base_nodeid))
	{
		// if this failed, the last dir node migh have been left 
		// with the new block... fix it if posible
		dir_node.blocks[second_dir_block] = OFS_NULL_VALUE;

		if(second_dir_block != -1) write_node(inf, &dir_node, dirnodeid);

		free(free_nodeid);
		if(newdir_nodeid != NULL) free(newdir_nodeid);
		if(newdir_blockid != NULL) free(newdir_blockid);
		if(newdir_base_blockid != NULL)
		{
			free(newdir_base_blockid);
		}
		return 1;
	}

	if(out_nodeid != NULL) *out_nodeid = *free_nodeid;

	free(free_nodeid);
	if(newdir_nodeid != NULL) free(newdir_nodeid);
	if(newdir_blockid != NULL) free(newdir_blockid);
	if(newdir_base_blockid != NULL) free(newdir_base_blockid);
	
	// preserve changes on the node sent as a parameter
	if(dirnodeid == dir_base_nodeid)
	{
		// dir_node is the base node
		*directory_node = dir_node;
	}
	else
	{
		*directory_node = dir_basenode;
	}

	return 0;
}

// this version of mkofs is limited to a maximum fdir file size of OFS_BLOCK_SIZE * PTRS_ON_NODE
int parse_dir(struct ofsinfo *inf, char *path, struct node *dir_node, struct node *parsed_node, int *dir_exists, int *nodeid, int *dnodeid)
{
	struct sdir_entry *dentry;
	struct node currnode;
	int block, bcount;
	char *str;
	unsigned int bsize, read, total, pathstart, pathend, cnodeid;

	*dir_exists = 0;

	if(strcmp(path, "/") == 0)
	{
		// looking for root...
		*dir_exists = 1;
		*nodeid = 0;
		*dnodeid = -1;

		*parsed_node = inf->rootn;
		
		return 0; // ok	
	}

	if(inf->rootn.blocks[0] == OFS_NULL_VALUE)
	{
		*dir_exists = (last_index_of(path, '/') == 0);
		if(*dir_exists)
		{
			*dir_node = inf->rootn;
		}
		*dnodeid = 0;
		return 1;
	}

	// not looking for /, start parsing
	*dir_node = inf->rootn;
	currnode = inf->rootn;
	block = 0;
	bcount = 1;
	bsize = (unsigned int)MIN(OFS_BLOCK_SIZE, currnode.file_size);
	read = 0;
	total = 0;
	pathstart = 1;
	pathend = first_index_of(path, '/', pathstart);
	cnodeid = 0;
	*dnodeid = 0;

	if(pathend == -1)
	{
		pathend = len(path);
	}

	pathend--; // remove trailing '/'

	// read block
	read_block(inf, inf->pbuffer, currnode.blocks[block]);

	while(1)
	{
		// seek for entry
		while(read < bsize)
		{
			if(OFS_BLOCK_SIZE - read > sizeof(struct sdir_entry))
			{
				dentry = (struct sdir_entry *)(inf->pbuffer + read);

				// check there's enough size for the record on the buffer
				if(bsize - read >= dentry->record_length )
				{
					if(dentry->name_length == (pathend - pathstart + 1))
					{
						str = (char *)(inf->pbuffer + read + sizeof(struct sdir_entry));

						if(istrprefix(pathstart, str, path))
						{
							// ok found the part of the path we were looking for
							*dir_node = currnode;
							*dnodeid = cnodeid;

							// read next dir/file node
							read_node(inf, &currnode, dentry->nodeid);

							pathstart = first_index_of(path, '/', pathstart);

							// check if we found our file/dir
							if(pathstart == -1 || pathstart == len(path) - 1)
							{
								// found ^^
								*parsed_node = currnode;
								*nodeid = dentry->nodeid;
								*dir_exists = 1;
								return 0;
							}
						
							// check its a directory node
							if(!(currnode.type & OFS_DIRECTORY_FILE) || currnode.blocks[0] == OFS_NULL_VALUE)
							{
								*dir_exists = (last_index_of(path, '/') == (pathstart));
	
								*dir_node = currnode;
								*dnodeid = dentry->nodeid;

								return 1;	
							}

							pathstart++;

							cnodeid = dentry->nodeid;

							// begin reading next dir
							block = 0;
							bcount = 1;
							bsize = (unsigned int)MIN(OFS_BLOCK_SIZE, currnode.file_size);
							read = 0;
							total = 0;
							
							*dnodeid = cnodeid;
							*dir_node = currnode;

							pathend = first_index_of(path, '/', pathstart);

							if(pathend == -1)
							{
								pathend = len(path);
							}

							pathend--; // remove trailing '/'

							// read block
							read_block(inf, inf->pbuffer, currnode.blocks[block]);

							continue;
						}
					}
				}
				else
				{
					break;
				}				
			}
			else
			{
				break;
			}

			read += dentry->record_length;
		}

		int oldtot = total;
		total += read;

		// entry not found find out why

		// check eof
		if(total == (unsigned int)currnode.file_size)
		{
			*dir_exists = (last_index_of(path, '/') == (pathstart - 1));
			
			return 1;
		}

		// refill buffer
		if(read == bsize)
		{
			// all buffer was read
			if(bcount == 2)
			{
				block += 2;				
			}
			else
			{
				block++;
			}
			
			bcount = 1;
			bsize = (unsigned int)MIN(OFS_BLOCK_SIZE, (unsigned int)currnode.file_size - total);
			read = 0;

			// read block
			read_block(inf, inf->pbuffer, currnode.blocks[block]);
		}
		else
		{
			// there are bytes remaining on the buffer!
			if(read < OFS_BLOCK_SIZE)
			{
				// bytes remaining are from the first block... this means 
				// bcount = 1
				bcount++;
				total = oldtot;
				bsize = (unsigned int)MIN(OFS_BLOCK_SIZE * 2, (unsigned int)currnode.file_size - total);
				// read block
				read_block(inf, inf->pbuffer + OFS_BLOCK_SIZE, currnode.blocks[block + 1]);
			}
			else
			{
				// bytes remaining are from the second buffer
				memcpy(inf->pbuffer, inf->pbuffer + OFS_BLOCK_SIZE, OFS_BLOCK_SIZE);
				block++;
				total = oldtot + OFS_BLOCK_SIZE;
				read -= OFS_BLOCK_SIZE;
				// read block
				read_block(inf, inf->pbuffer + OFS_BLOCK_SIZE, currnode.blocks[block + 1]);
			}
		}
	}	

	return 0;
}

int istrprefix(int start,char* str1, char* str2)
{
	int i = start;
	int j = 0;

	if(start >= len(str2)) return 0;


	if(str1 == NULL && str2 == NULL) return -1;
	if(str1 == NULL || str2 == NULL) return -1;

	while(str1[j] != '\0' && str2[i] != '\0' && str1[j] == str2[i])
	{
		i++;
		j++;
	}

	return str1[j] == '\0';
}

int len(const char* str)
{
	int i = 0;

	if(str == NULL) return 0;

	while(str[i] != '\0')
	{
		i++;
	}

	return i;
}
