/*  
 *   Sartoris bitmap operations
 *   
 *   Copyright (C) 2002, 2003, Santiago Bazerque and Nicolas de Galarreta
 *
 *   sbazerqu@dc.uba.ar                   
 *   nicodega@gmail.com      
 */

#include "lib/containers.h"
#include "lib/indexing.h"

#define PG_SIZE 0x1000
/* 
This will never make it onto kernel image, it's just to 
check the dinamic memory size is ok.
*/
#ifndef PAD
#define PAD(a,b) (a + (b - (a % b)))
#endif

#define CONT_DYN_THR      (PAD((MAX_THR - STATIC_THR),CONT_THR_PER_CONT) / CONT_THR_PER_CONT)
#define CONT_DYN_TSK      (PAD((MAX_TSK - STATIC_TSK),CONT_TSK_PER_CONT) / CONT_TSK_PER_CONT)
#define CONT_DYN_SMO      (PAD((MAX_SMO - STATIC_SMO),CONT_SMO_PER_CONT) / CONT_SMO_PER_CONT)
#define CONT_DYN_MSG      (PAD((MAX_SMO - STATIC_MSG),CONT_MSG_PER_CONT) / CONT_MSG_PER_CONT)
#define CONT_DYN_PRT      (PAD((MAX_TSK_OPEN_PORTS*CONT_DYN_TSK), CONT_PRT_PER_CONT) / CONT_PRT_PER_CONT)

#define CONT_DINAMIC_CONTAINERS  (CONT_DYN_THR + CONT_DYN_TSK + CONT_DYN_SMO + CONT_DYN_MSG + CONT_DYN_PRT)

unsigned char size_thr[sizeof(struct c_thread_unit)];
unsigned char size_tsk[sizeof(struct c_task_unit)];
unsigned char size_smo[sizeof(struct smo)];
unsigned char size_msg[sizeof(struct message)];
unsigned char size_port[sizeof(struct port)];

unsigned char static_containers[CONT_STATIC_CONTAINERS];

unsigned char static_size_calculation[(IDX_LEN(STATIC_THR) + IDX_LEN(STATIC_TSK) + IDX_LEN(STATIC_SMO)) * PG_SIZE + CONT_STATIC_CONTAINERS * PG_SIZE];
unsigned char dinamic_size_calculation[(IDX_LEN(MAX_THR-STATIC_THR) + IDX_LEN(MAX_TSK-STATIC_TSK) + IDX_LEN(MAX_SMO-STATIC_SMO)) * PG_SIZE + CONT_DINAMIC_CONTAINERS * PG_SIZE];
unsigned char thr_per_cont[CONT_THR_PER_CONT];

unsigned char indexes[(IDX_LEN(MAX_THR-STATIC_THR) + IDX_LEN(MAX_TSK-STATIC_TSK) + IDX_LEN(MAX_SMO-STATIC_SMO)) * PG_SIZE];

unsigned char mapping_zone[MAX_THR * PG_SIZE];

unsigned char uper_mem[(IDX_LEN(MAX_THR-STATIC_THR) + IDX_LEN(MAX_TSK-STATIC_TSK) + IDX_LEN(MAX_SMO-STATIC_SMO)) * PG_SIZE + CONT_DINAMIC_CONTAINERS * PG_SIZE + MAX_THR * PG_SIZE];

int main()
{
    return 0;    
}
