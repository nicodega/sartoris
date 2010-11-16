#include "dma_internals.h"

struct channel channels[8];

int buffer[BUFF_SIZE/4];

void dma_srv() {

	int die = 0, i, id, result;
	struct dma_command msg;
	struct dma_response res;
	struct channel *c;
	struct mem_desc *mdes;

	__asm__ ("sti": :);

	open_port(DMA_COMMAND_PORT, 0, UNRESTRICTED);

	init_dma(DMA_ENABLE);
	init_mem(buffer,BUFF_SIZE);

	for(i = 0; i < 8; i++) channels[i].task = -1;

	while (!die) 
	{

		/* wait for a message to come */

		while(get_msg_count(DMA_COMMAND_PORT) == 0) {
			run_thread(SCHED_THR); 
		}

		get_msg(DMA_COMMAND_PORT, &msg, &id);

		/* process message */

		switch(msg.op) {

	case GET_CHANNEL:

		c = &channels[msg.channel];
		if(c->task == -1) {
			c->buffer = (int *) malloc(1, msg.buff_size);
			if(c->buffer != 0) {
				c->task = id;
				c->smo = res.res1 = share_mem(id, c->buffer, 
					msg.buff_size/4, 
					WRITE_PERM | READ_PERM);
				set_dma_channel(msg.channel, 
					SRV_MEM_BASE + SRV_SLOT_SIZE * DMA_MAN_SLOT + (int) c->buffer, 
					msg.buff_size, 
					msg.channel_mode);
				c->buff_size = msg.buff_size; /* was == but made no sense, changed 10/9/02 */
				result = DMA_OK;
			} else {
				result = ALLOC_FAIL;
			}
		} else {
			result = CHANNEL_TAKEN;
		}
		break;

	case SET_CHANNEL:

		c = &channels[msg.channel];
		if(c->task == id) {

			if(msg.buff_size != c->buff_size){
				claim_mem(c->smo);
				free(c->buffer);
				c->buffer = (int*) malloc(sizeof(int), msg.buff_size);
				if(c->buffer == 0) {
					result = ALLOC_FAIL; 
					break;
				}
				c->smo = share_mem(id, c->buffer, msg.buff_size/4, WRITE_PERM | READ_PERM);
				c->buff_size = msg.buff_size;
			}
			set_dma_channel(msg.channel, 
				SRV_MEM_BASE + SRV_SLOT_SIZE * DMA_MAN_SLOT + (int) c->buffer, 
				msg.buff_size, 
				msg.channel_mode);
			res.res1 = c->smo;
			result = DMA_OK;
		} else {
			result = DMA_FAIL;
		}
		break;
	case FREE_CHANNEL: 
		if(channels[msg.channel].task == id) {
			channels[msg.channel].task = -1;
			free(channels[msg.channel].buffer);
			result = DMA_OK;
		} else {
			result = DMA_FAIL;
		}
		break;
	case DMA_DIE:
		die = 1;
		break;
		}

		/* prepare response */

		if(!die) {
			res.op = msg.op;
			res.result = result;
			send_msg(id, msg.ret_port, &res);
		}
	}

	while(1); /* wait to be killed */

}



