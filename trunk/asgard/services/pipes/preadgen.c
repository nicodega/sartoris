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

struct pipes_res * preadgen(struct pipes_read *cmd, int delimited, struct pipe *p)
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
			case PIPES_READ:
				c = fread(cmd->smo, cmd->count, 1, p->pf);
				if(c != cmd->count)
				{
					// enqueue pending read
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
			case PIPES_GETS:
				c = fgets(cmd->smo, cmd->count, p->pf);
				if(c != cmd->smo)
				{
					res = build_response_msg(PIPESERR_EOF);
					res->param1 = c;
					res->param2 = p->pf->eof; // eof
				}
				else
				{
					res = build_response_msg(PIPESERR_OK);
					res->param1 = c;
					res->param2 = p->pf->eof; // eof
				}
				break;
			case PIPES_GETC:
				ch = fgetc(p->pf);
				if(ch == EOF)
				{
					res = build_response_msg(PIPESERR_EOF);
					((struct pipes_getc_res*)res)->c = EOF;
					res->param1 = 0;
					res->param2 = p->pf->eof; // eof
				}
				else
				{
					res = build_response_msg(PIPESERR_OK);
					((struct pipes_getc_res*)res)->c = ch;
					res->param1 = 1;
					res->param2 = p->pf->eof; // eof
				}
				break;
		}
	}
	else
	{

		// if reading nothing return ok
		if(cmd->count == 0)
		{
			res = build_response_msg(PIPESERR_OK);				
			res->param1 = 0;
			res->param2 = (p->buffer->rcursor == p->buffer->size);
			return res;
		}

		if(delimited) cmd->count--;

		// if there are not enough bytes to read request is left pending
		// UPDATE: if the pipe has been closed on the writing side, read is completed
		if(cmd->count + p->buffer->rcursor > p->buffer->size && !p->task1_closed)
		{
			p->pending = 1;
			p->pending_cmd = *((struct pipes_cmd*)cmd);
			return NULL;
		}

		// well.... let's do it!
		unsigned int bn = (int)(p->buffer->rcursor / PIPES_BUFFER_SIZE)+1;
		struct pipe_buffer_block *b = NULL;
		unsigned long offset = 0, left = MIN ( cmd->count,  p->buffer->size - p->buffer->rcursor);
		int left_to_read = 0, read = 0, ret_read = 0;
		char *bcheck, character;

		read = (int)(p->buffer->rcursor % (unsigned long)PIPES_BUFFER_SIZE);
		left_to_read = PIPES_BUFFER_SIZE - read;

		if(left > 0)
		{
			// Move to the current block (this could be improved by keeping it on the pipe, FIXME)
			CPOSITION it = get_head_position(&p->buffer->blocks);

			int i = 0;
			while(i < bn)
			{
				b = (struct pipe_buffer_block *)get_next(&it);	
				i++;
			}

			while(left > 0)
			{
				bcheck = (char*)(b->bytes + read);

				// if delimited, attempt to cap left_to_read.
				if(delimited)
				{
					// look on the file buffer for \n
					i = 0;
					while(bcheck[i] != '\n' && i < (int)MIN( (unsigned int)PIPES_BUFFER_SIZE-read, (unsigned int)left)){i++;}

					if(i != (int)MIN( (unsigned int)PIPES_BUFFER_SIZE, (unsigned int)left ))
					{
						left = i+1;
					}
				}

				if(cmd->command == PIPES_GETC)
				{
					character = *bcheck;
				}
				else
				{
					// writes on the task buffer as many bytes as possible
					write_mem(cmd->smo, (int)offset, 
						(int)MIN( (unsigned int)PIPES_BUFFER_SIZE - read, (unsigned int)left ), /* size */
						bcheck		/* buffer */
					);
				}

				// increment cursor possition, offset and bytes read so far by bytes read
				p->buffer->rcursor += MIN( PIPES_BUFFER_SIZE-read, left );
				offset += MIN( PIPES_BUFFER_SIZE-read, left );
				ret_read += MIN( PIPES_BUFFER_SIZE-read, (int)left );
				// decrement left on bytes read
				left -= MIN( PIPES_BUFFER_SIZE-read, left );
				read = 0;
				left_to_read = 0;

				// get next buffer
				if(left > 0) b = (struct pipe_buffer_block *)get_next(&it);	
			}
		}

/*		if(p->task1_closed && p->buffer->rcursor == p->buffer->size)
		{
			res = build_response_msg(PIPESERR_OK);
		}
		else*/
		{
			res = build_response_msg(PIPESERR_OK);
		}
		res->param1 = ret_read;
		res->param2 = (p->buffer->rcursor == p->buffer->size);

		// if reading was delimited, append \0 to the end of the buffer
		if(delimited) write_mem(cmd->smo, ret_read, 1, &nullbuff); 

	}
	return res;
}

