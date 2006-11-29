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

char buffer[PIPES_BUFFER_SIZE];

struct pipes_res * pwritegen(struct pipes_write *cmd, int delimited, struct pipe *p)
{
	struct pipes_res *res = NULL;
	int c = 0;
	char ch;
	char nullbuff = '\0';

	if(p->type == PIPES_FILEPIPE)
	{
		// fs will do the job for us :)
		switch(cmd->command)
		{
			case PIPES_WRITE:
				c = fwrite(cmd->smo, cmd->count, 1, p->pf);
				if(c != cmd->count)
				{
					res = build_response_msg(PIPESERR_EOF);
					res->param1 = c;
					res->param2 = 1; // eof
				}
				else
				{
					res = build_response_msg(PIPESERR_OK);
					res->param1 = c;
					res->param2 = p->pf->eof; // eof
				}
				break;
			case PIPES_PUTS:
				ch = fputs(cmd->smo, p->pf);
				if(ch != 1)
				{
					res = build_response_msg(PIPESERR_EOF);
					res->param1 = 0;
					res->param2 = p->pf->eof; // eof
				}
				else
				{
					res = build_response_msg(PIPESERR_OK);
					res->param1 = 0;
					res->param2 = p->pf->eof; // eof
				}
				break;
			case PIPES_PUTC:
				ch = fputc(((struct pipes_putc*)cmd)->c, p->pf);
				if(ch == EOF)
				{
					res = build_response_msg(PIPESERR_EOF);
					res->param1 = 0;
					res->param2 = p->pf->eof; // eof
				}
				else
				{
					res = build_response_msg(PIPESERR_OK);
					res->param1 = 0;
					res->param2 = p->pf->eof; // eof
				}
				break;
		}
	}
	else
	{
		unsigned long offset = 0, left = 0, written = 0, curd;
		struct pipe_buffer_block *b = NULL;
		int i;
//print("writing",0);
		// as this function is not as simple as read, if writing is delimited, we will modify command
		// count now, by reading the user buffer
		if(delimited)
		{
			left = cmd->count; 	// remaining
			offset = 0;		// offset on sender buffer
			i = 0;			// count
			
			while(left > 0)
			{
				// read data from buffer, onto dir buffer	
				read_mem(cmd->smo, offset, MIN( PIPES_BUFFER_SIZE, cmd->count), &buffer);

				left -= MIN( PIPES_BUFFER_SIZE, cmd->count);
				
				while(buffer[i - offset] != '\0' && i < cmd->count){i++;}

				if(buffer[i - offset] == '\0')
					break;

				offset += MIN( PIPES_BUFFER_SIZE, cmd->count);
			}

			cmd->count = i;
		}

		// if writing nothing return ok
		if(cmd->count == 0)
		{
			res = build_response_msg(PIPESERR_OK);				
			res->param1 = 0;
			res->param2 = (p->buffer->wcursor == p->buffer->size); // eof
			return res;
		}

		// Move to the current block (this could be improved by keeping it on the pipe, FIXME)
		CPOSITION it = get_head_position(&p->buffer->blocks);

		i = 0;
		unsigned int bn = (int)(p->buffer->wcursor / PIPES_BUFFER_SIZE);

		while(i < bn)
		{
			b = (struct pipe_buffer_block *)get_next(&it);	
			i++;
		}

		left = cmd->count;
		curd = p->buffer->wcursor % PIPES_BUFFER_SIZE;

		while(left > 0)
		{
			bn = (int)((p->buffer->wcursor + cmd->count - written) / PIPES_BUFFER_SIZE);

			// get a new block if needed
			if(bn > length(&p->buffer->blocks))
			{
				add_tail(&p->buffer->blocks, alloc_block());	
			}

			// write data from smo onto the current block
			if(cmd->command == STDFSS_PUTC)
			{
				*((char*)(b->bytes + curd)) = ((struct stdfss_putc *)cmd)->c;
			}
			else
			{
				read_mem(cmd->smo, offset, MIN( PIPES_BUFFER_SIZE - curd, cmd->count), (char*)(b->bytes + curd));
			}

			p->buffer->wcursor += MIN( PIPES_BUFFER_SIZE-curd, left );
			offset += MIN( PIPES_BUFFER_SIZE-curd, left );
			written += MIN( PIPES_BUFFER_SIZE-curd, left );
			// decrement left on bytes read
			left -=  MIN( PIPES_BUFFER_SIZE-curd, left );
			curd = 0;
		}
		if(p->buffer->size < p->buffer->wcursor) p->buffer->size = p->buffer->wcursor;
		res = build_response_msg(PIPESERR_OK);
		res->param1 = 0;
		res->param2 = (p->buffer->wcursor == p->buffer->size); // eof
	}
	return res;
}
