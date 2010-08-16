/*
*
*    Copyright (C) 2002, 2003, 2004, 2005
*       
*    Santiago Bazerque     sbazerque@gmail.com            
*    Nicolas de Galarreta    nicodega@gmail.com
*
*    
*    Redistribution and use in source and binary forms, with or without 
*     modification, are permitted provided that conditions specified on 
*    the License file, located at the root project directory are met.
*
*    You should have received a copy of the License along with the code,
*    if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*    or you can contact us directly at the email addresses provided above.
*
*
*/

#ifndef DMA
#define DMA

void init_dma(int control);
void set_dma_channel(int channel, int physaddr, int length, int mode);


/* transfer types */

#define DMA_ENABLE       0
#define DMA_DISABLE      4      
#define ROTATING_PRI     16
#define MEM_TO_MEM_ALLOW 1

#endif
