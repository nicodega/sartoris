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

#include "pipes_srv_internals.h"

struct pipes_res * pread(struct pipes_read *pcmd, struct pipe *p)
{
	return preadgen(pcmd, FALSE, p);
}

struct pipes_res * pwrite(struct pipes_write *pcmd, struct pipe *p)
{
	return pwritegen(pcmd, FALSE, p);
}

struct pipes_res * pputs(struct pipes_puts *pcmd, struct pipe *p)
{
	return pwritegen((struct pipes_write *)pcmd, TRUE, p);
}

struct pipes_res * pputc(struct pipes_putc *pcmd, struct pipe *p)
{
	((struct pipes_write *)pcmd)->count = 1;
	return pwritegen((struct pipes_write *)pcmd, FALSE, p);
}

struct pipes_res * pgets(struct pipes_gets *pcmd, struct pipe *p)
{
	return preadgen((struct pipes_read *)pcmd, TRUE, p);
}

struct pipes_res * pgetc(struct pipes_getc *pcmd, struct pipe *p)
{
	((struct pipes_read *)pcmd)->count = 1;
	return preadgen((struct pipes_read *)pcmd, FALSE, p);
}

struct pipes_res *pseek(struct pipes_seek *pcmd, struct pipe *p, int task)
{
	if(p->type == PIPES_FILEPIPE)
	{
		if(fseek(p->pf, pcmd->possition, pcmd->origin))
		{
			return build_response_msg(PIPESERR_EOF);
		}
		else
		{
			return build_response_msg(PIPESERR_OK);
		}
	}
	else
	{
		unsigned long long newpos = 0;

		// calculate new possition
		switch(pcmd->origin)
		{
			case PIPES_SEEK_SET:
				newpos = pcmd->possition;
				break;
			case PIPES_SEEK_CUR:
				newpos = ((task == p->taskid)? p->buffer->wcursor : p->buffer->rcursor) + pcmd->possition;
				break;
			case PIPES_SEEK_END:
				if(p->buffer->size < pcmd->possition) 
					return build_response_msg(PIPESERR_ERR);
				newpos = p->buffer->size - pcmd->possition;
				break;
			default:
				return build_response_msg(PIPESERR_ERR);
		}

		// calculate blocks needed
		unsigned int bc = (int)(newpos / PIPES_BUFFER_SIZE) + ((newpos % PIPES_BUFFER_SIZE == 0)? 0 : 1);

		// if trying to seek beyond with the reading cursor
		if(bc > length(&p->buffer->blocks) && task == p->taskid2)
		{
			// if writing task has not closed, put the message on pending status
			if(p->task1_closed)
			{
				return build_response_msg(PIPESERR_EOF);
			}
			p->pending = 1;
			p->pending_cmd = *((struct pipes_cmd*)pcmd);
			return NULL;
		}

		if(task == p->taskid2)
		{
			// get as many new blocks as needed
			while(bc > length(&p->buffer->blocks))
			{
				add_tail(&p->buffer->blocks, alloc_block());	
			}
			p->buffer->rcursor = newpos;
		}
		else
		{
			p->buffer->wcursor = newpos;
		}
		return build_response_msg(PIPESERR_OK);
	}
}

struct pipes_res * ptell(struct pipes_tell *pcmd, struct pipe *p, int task)
{
	struct pipes_res *res = build_response_msg(PIPESERR_OK);

	if(p->type == PIPES_FILEPIPE)
	{
		((struct pipes_tell_res*)res)->cursor = ftell(p->pf);
	}
	else
	{
		((struct pipes_tell_res*)res)->cursor = ((task == p->taskid)? p->buffer->wcursor : p->buffer->rcursor);
	}
	
	return res;
}

struct pipes_res * pclose(struct pipes_close *pcmd, struct pipe *p, int task)
{
	CPOSITION it;

	// check pipe type
	if(p->type == PIPES_FILEPIPE)
	{
		fclose(p->pf);		
		return build_response_msg(PIPESERR_OK);
	}

	if(task == p->taskid)
	{
		p->task1_closed = 1;
	}
	else if(task == p->taskid2)
	{
		p->task2_closed = 1;
	}

	// if there was a pending read, send what's left on the buffer
	if(p->pending)
	{
		process_pending(p);
	}

	if((p->task1_closed && p->task2_closed) || task == p->creating_task)
	{
		it = get_head_position(&p->buffer->blocks);
		while(it != NULL)
		{
			struct pipe_buffer_block *b = (struct pipe_buffer_block*)get_next(&it);
			free_block(b);
		}
		lstclear(&p->buffer->blocks);	

		avl_remove(&pipes, p->id);

		free(p);
	}

	return build_response_msg(PIPESERR_OK);
}

void process_pending(struct pipe *p)
{
	struct pipes_res *res = NULL;

	switch(p->pending_cmd.command)
	{
		case PIPES_READ:
		res = pread((struct pipes_read *)&p->pending_cmd, p);
			break;
		case PIPES_SEEK:
		res = pseek((struct pipes_seek *)&p->pending_cmd, p, p->taskid2);
			break;
		case PIPES_GETS:
		res = pgets((struct pipes_gets *)&p->pending_cmd, p);
			break;
		case PIPES_GETC:
		res = pgetc((struct pipes_getc *)&p->pending_cmd, p);
			break;
	}

	if(res != NULL)
	{
		p->pending = 0;
		res->thr_id = p->pending_cmd.thr_id;
		res->command = p->pending_cmd.command;
		send_msg(p->taskid2, p->pending_cmd.ret_port, res);
	}
}
