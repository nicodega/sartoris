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
*	NOTE: This code is based on ARADRVR by Hale Landis (hlandis@ata-atapi.com)
*	Hale Landis library source code is distributed under this conditions: 
*
*	"There is no copyright and there are no restrictions on the use
*	of this ATA Low Level I/O Driver code.  It is distributed to
*	help other programmers understand how the ATA device interface
*	works and it is distributed without any warranty.  Use this
*	code at your own risk."
*
*	I thought it would be worth mentioning I'm pretty grateful for 
*	his library, for it has helped a lot on ata interface undertanding.
*
*/


#include "atac.h"


//******************************************************************
//
// tmr_wait_timeout() - wait for timeout, or irq signal on a channel
//
// returns 0 if timed out, 1 otherwise. This call will block the 
// thread until the signal arrives.
//******************************************************************

int tmr_wait_timeout(struct ata_channel *channel)
{
	/* Wait for our interrupt service signal, or timeout */
	return wait_signal(IRQ_EVENT, get_current_task(), COMMAND_TIMEOUT, channel->id, SIGNAL_PARAM_IGNORE, NULL, NULL);
}

//******************************************************************
//
// tmr_set_timeout_int() - set a timeout signal for an int
//
// returns 0 if timed out, 1 otherwise.
//******************************************************************
SIGNALHANDLER tmr_set_timeout_int(struct ata_channel *channel)
{
	/* Wait for our interrupt service signal, or timeout */
	return wait_signal_async(IRQ_EVENT, get_current_task(), COMMAND_TIMEOUT, channel->id, SIGNAL_PARAM_IGNORE);
}

//******************************************************************
//
// tmr_set_timeout() - set a timeout signal
//
// returns 0 if timed out, 1 otherwise.
//******************************************************************
SIGNALHANDLER tmr_set_timeout(struct ata_channel *channel)
{
	/* Wait for our interrupt service signal, or timeout */
	return wait_signal_async(PMAN_SLEEP, PMAN_TASK, COMMAND_TIMEOUT, SIGNAL_PARAM_IGNORE, SIGNAL_PARAM_IGNORE);
}

//******************************************************************
//
// tmr_check_timeout() - see if timed out.
//
// returns 0 if no signal, -1 if timed out, 1 otherwise.
//******************************************************************
int tmr_check_timeout(SIGNALHANDLER sigh)
{
	return check_signal(sigh, NULL, NULL);
}

//******************************************************************
//
// tmr_discard_timeout() - discard a timeout signal
//
// This function will turn the sigh handler obsolete.
//******************************************************************

void tmr_discard_timeout(SIGNALHANDLER sigh)
{
	discard_signal(sigh);
}

