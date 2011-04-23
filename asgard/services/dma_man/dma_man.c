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

#include "dma_internals.h"
#include <lib/structures/list.h>
#include <lib/const.h>
#include <lib/scheduler.h>
#include <services/pmanager/services.h>

/* REMOVEME */
#include <lib/debug.h>
#include <drivers/screen/screen.h>

struct stdservice_res dieres;
int dieid;
int dieretport;

struct channel channels[8];

unsigned char buffer[BUFF_SIZE];
unsigned char pagesbuffer[PAGE_BUFF_SIZE] __attribute__((aligned(0x1000))); // buffers must be 64kb aligned
unsigned int phys;

list assigned;

void dma_srv() 
{
  int die = 0, i, id, result;
  struct dma_command msg;
  struct dma_response res;
  struct channel *c;
  struct mem_desc *mdes;
  struct stdservice_cmd service_cmd;
  struct stdservice_res servres;
  int service_count;
  char *service_name = "system/dmac";
  struct directory_register reg_cmd;
  struct directory_response dir_res;
  struct pm_msg_phymem pmem;
  struct pm_msg_phymem_response pres;

  // open port with permisions for services only (lv 0, 1, 2) //
  open_port(STDSERVICE_PORT, 2, PRIV_LEVEL_ONLY);
  open_port(DMA_COMMAND_PORT, 2, PRIV_LEVEL_ONLY);
  open_port(2, 2, PRIV_LEVEL_ONLY);

  __asm__ ("sti": :);

  /* open_port(DMA_COMMAND_PORT, ALLOW_ALL); */
  init_dma(DMA_ENABLE);
  init_mem(buffer, BUFF_SIZE);
  init_blocks();

  for(i = 0; i < 8; i++) channels[i].task = -1;

  /* we need to know our physical address */
  pmem.pm_type = PM_PHYMEM;
  pmem.req_id = 0;
  pmem.response_port = 2;
  pmem.linear = 0;
  
  send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &pmem);

  while(get_msg_count(2) == 0)
  { 
      reschedule(); 
  }

  get_msg(2, &pres, &id);
  
  phys = pres.physical; // this will contain the physical address

  /* Register service with directory */
  reg_cmd.command = DIRECTORY_REGISTER_SERVICE;
  reg_cmd.ret_port = 2;
  reg_cmd.service_name_smo = share_mem(DIRECTORY_TASK, service_name, 12, READ_PERM);
  
  while(send_msg(DIRECTORY_TASK, DIRECTORY_PORT, &reg_cmd) < 0)
  { 
      reschedule(); 
  }

  while (get_msg_count(2) == 0) { reschedule(); }

  get_msg(2, &dir_res, &id);

  close_port(2);
  claim_mem(reg_cmd.service_name_smo);

  while (!die) 
  {
    
    /* wait for a message to come */
    while(get_msg_count(DMA_COMMAND_PORT) == 0 && get_msg_count(STDSERVICE_PORT) == 0) {
        string_print("DMA ALIVE",2*160-18,i++);
      reschedule();
    }

    service_count = get_msg_count(STDSERVICE_PORT);
		
    while(service_count != 0)
    {
		get_msg(STDSERVICE_PORT, &service_cmd, &id);

		servres.ret = STDSERVICE_RESPONSE_OK;
		servres.command = service_cmd.command;

		if(service_cmd.command == STDSERVICE_DIE)
		{
			die = 1;
			dieres.ret = STDSERVICE_RESPONSE_OK;
			dieres.command = service_cmd.command;
			dieid = id;
			dieretport = service_cmd.ret_port;
			break;
		}
		else if(service_cmd.command == STDSERVICE_QUERYINTERFACE)
		{
			process_query_interface((struct stdservice_query_interface *)&service_cmd, id);
			service_count--;
			continue;
		}
		send_msg(id, service_cmd.ret_port, &servres);
		service_count--;
    }

    while(!die && get_msg_count(DMA_COMMAND_PORT) > 0)
    {
	    get_msg(DMA_COMMAND_PORT, &msg, &id);

	    /* process message */
		if(msg.channel > 7 || msg.channel < 0 || msg.channel == 0 || msg.channel == 4)
		{
			result = DMA_FAIL ;
		}
		else
		{
			switch(msg.op) {
		      
			case GET_CHANNEL:
				c = &channels[msg.channel];

				if(c->task == -1) 
				{
					c->assigned_it = get_buffer(msg.buff_size, msg.channel >= 5);

					if(c->assigned_it != NULL) 
					{
						struct assigned_memory *asig = (struct assigned_memory *)get_at(c->assigned_it);

						c->task = id;
						c->smo = res.res1 = share_mem(id, (void *)asig->start, msg.buff_size, WRITE_PERM | READ_PERM);
						set_dma_channel(msg.channel, (int)((unsigned int)asig->start + phys), msg.buff_size, msg.channel_mode);
						result = DMA_OK;
					} 
					else 
					{
						result = ALLOC_FAIL;
					}
				} 
				else 
				{
					result = CHANNEL_TAKEN;
				}

				break;

			case SET_CHANNEL:
				c = &channels[msg.channel];

				if(c->task == id || c->task == -1) 
				{
					struct assigned_memory *asig = NULL;

					if(c->task != -1)
						asig = (struct assigned_memory *)get_at(c->assigned_it);

					if(c->task == -1 || msg.buff_size != asig->size)
					{
						if(c->task != -1)
						{
							claim_mem(c->smo);
							free_buffer(c->assigned_it);
						}

						c->assigned_it = get_buffer(msg.buff_size, msg.channel >= 5);
						if(c->assigned_it != NULL) 
						{
							asig = (struct assigned_memory *)get_at(c->assigned_it);
							
							c->smo = res.res1 = share_mem(id, (void *)asig->start, msg.buff_size, WRITE_PERM | READ_PERM);
						} 
						else 
						{
							result = ALLOC_FAIL;
							break;
						}
						c->smo = share_mem(id, (void *)asig->start, asig->size, WRITE_PERM | READ_PERM);
					}
					set_dma_channel(msg.channel, (int)((unsigned int)asig->start + phys), msg.buff_size, msg.channel_mode);
					res.res1 = c->smo;
					result = DMA_OK;
				} 
				else 
				{
					result = DMA_FAIL;
				}
				break;
			case FREE_CHANNEL: 
				if(channels[msg.channel].task == id) 
				{
					channels[msg.channel].task = -1;
					free_buffer(channels[msg.channel].assigned_it);
					result = DMA_OK;
				} 
				else 
				{
					result = DMA_FAIL;
				}
				break;
			}
		}

	    /* prepare response */
	    res.op = msg.op;
	    res.result = result;
		res.thr_id = msg.thr_id;
	    send_msg(id, msg.ret_port, &res);
	}
  }

  send_msg(dieid, dieretport, &dieres);  

  // NOTE: on this loop we should send failure messages 
  for(;;); /* wait to be killed */
  
}

void process_query_interface(struct stdservice_query_interface *query_cmd, int task)
{
	struct stdservice_query_res qres;

	qres.command = STDSERVICE_QUERYINTERFACE;
	qres.ret = STDSERVICE_RESPONSE_FAILED;
	qres.msg_id = query_cmd->msg_id;

	switch(query_cmd->uiid)
	{
		case DMAMAN_UIID:
			// check version
			if(query_cmd->ver != DMAMAN_VER) break;
			qres.port = DMA_COMMAND_PORT;
			qres.ret = STDSERVICE_RESPONSE_OK;
			break;
		default:
			break;
	}

	// send response
	send_msg(task, query_cmd->ret_port, &qres);
}


/* Initialize dma-man bocks buffer */
void init_blocks()
{
	/* initialize the list */
	init(&assigned);

	/* insert a phony first assigned item, so get_buffer is easier to implement */
	struct assigned_memory *asign = (struct assigned_memory *)malloc(sizeof(struct assigned_memory));
	asign->start = (unsigned int)pagesbuffer;
	asign->size = 0;
	asign->end = asign->start + 0;

	add_head(&assigned, asign); // this won't ever be freed
}

/* Get a page (or a just a piece of one) */
CPOSITION get_buffer(unsigned int size, int align128)
{
	CPOSITION it, nit = get_head_position(&assigned);
	it = nit;
	if(nit != NULL) get_next(&nit);	// move next it

	struct assigned_memory *asign_next = NULL;
	unsigned int pagesbufferphys = phys + (unsigned int)pagesbuffer;
	struct assigned_memory *newasign = NULL;

	while(it != NULL)
	{
		struct assigned_memory *asign = (struct assigned_memory *)get_next(&it);

		if(nit != NULL)
			asign_next = (struct assigned_memory *)get_next(&nit);
		else 
			asign_next = NULL;

		unsigned int physpos = asign->start + asign->size + phys;
		unsigned int avsize = ((asign_next == NULL)?  ((pagesbufferphys + PAGE_BUFF_SIZE) - (physpos + asign->size)) : (asign_next->start - (asign->start + asign->size) ) );

		unsigned int align = 0x10000;

		if(align128)
			align = 0x20000;

		// check if a 128kb/64kb aligned chunk is available between this
		// assigned block and the next one
		if(size < avsize)
		{
			unsigned int next_boundary = physpos + (align - (physpos % align)); // pos aligned up

			// it might fit at the begining, or the end of available block
			if(next_boundary - physpos >= size)
			{
				newasign = (struct assigned_memory *)malloc(sizeof(struct assigned_memory));
				newasign->start = asign->start + asign->size;
				newasign->size = size;
			}
			else if(avsize > (next_boundary - physpos) && avsize - (next_boundary - physpos) >= size )
			{
				newasign = (struct assigned_memory *)malloc(sizeof(struct assigned_memory));
				newasign->start = asign->start + asign->size + (align - (physpos % align));
				newasign->size = size;
			}
			newasign->end = newasign->start + size;
		}

		if(newasign != NULL)
		{
			if(it == NULL)
			{
				add_tail(&assigned, newasign);
				return get_tail_position(&assigned);
			}
			else
			{
				insert_before(&assigned, it, newasign);
				get_prev(&it);
				return it;
			}
		}
	}

	// we will never get here, thanks to the phony first assigned item :)

	return NULL;
}

void free_buffer(CPOSITION assigned_it)
{
	struct assigned_memory *asign = (struct assigned_memory *)get_at(assigned_it);
	remove_at(&assigned, assigned_it);
	free(asign);
}


