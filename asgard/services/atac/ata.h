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

#ifndef ATAC_ATAH
#define ATAC_ATAH

//**************************************************************
//
// Most mandtory and optional ATA commands
//
//**************************************************************

#define ATACMD_CFA_ERASE_SECTORS            0xC0
#define ATACMD_CFA_REQUEST_EXT_ERR_CODE     0x03
#define ATACMD_CFA_TRANSLATE_SECTOR         0x87
#define ATACMD_CFA_WRITE_MULTIPLE_WO_ERASE  0xCD
#define ATACMD_CFA_WRITE_SECTORS_WO_ERASE   0x38
#define ATACMD_CHECK_POWER_MODE1            0xE5
#define ATACMD_CHECK_POWER_MODE2            0x98
#define ATACMD_DEVICE_RESET                 0x08
#define ATACMD_EXECUTE_DEVICE_DIAGNOSTIC    0x90
#define ATACMD_FLUSH_CACHE                  0xE7
#define ATACMD_FLUSH_CACHE_EXT              0xEA
#define ATACMD_FORMAT_TRACK                 0x50
#define ATACMD_IDENTIFY_DEVICE              0xEC
#define ATACMD_IDENTIFY_DEVICE_PACKET       0xA1
#define ATACMD_IDENTIFY_PACKET_DEVICE       0xA1
#define ATACMD_IDLE1                        0xE3
#define ATACMD_IDLE2                        0x97
#define ATACMD_IDLE_IMMEDIATE1              0xE1
#define ATACMD_IDLE_IMMEDIATE2              0x95
#define ATACMD_INITIALIZE_DRIVE_PARAMETERS  0x91
#define ATACMD_INITIALIZE_DEVICE_PARAMETERS 0x91
#define ATACMD_NOP                          0x00
#define ATACMD_PACKET                       0xA0
#define ATACMD_READ_BUFFER                  0xE4
#define ATACMD_READ_DMA                     0xC8
#define ATACMD_READ_DMA_EXT                 0x25
#define ATACMD_READ_DMA_QUEUED              0xC7
#define ATACMD_READ_DMA_QUEUED_EXT          0x26
#define ATACMD_READ_MULTIPLE                0xC4
#define ATACMD_READ_MULTIPLE_EXT            0x29
#define ATACMD_READ_SECTORS                 0x20
#define ATACMD_READ_SECTORS_EXT             0x24
#define ATACMD_READ_VERIFY_SECTORS          0x40
#define ATACMD_READ_VERIFY_SECTORS_EXT      0x42
#define ATACMD_RECALIBRATE                  0x10
#define ATACMD_SEEK                         0x70
#define ATACMD_SET_FEATURES                 0xEF
#define ATACMD_SET_MULTIPLE_MODE            0xC6
#define ATACMD_SLEEP1                       0xE6
#define ATACMD_SLEEP2                       0x99
#define ATACMD_SMART                        0xB0
#define ATACMD_STANDBY1                     0xE2
#define ATACMD_STANDBY2                     0x96
#define ATACMD_STANDBY_IMMEDIATE1           0xE0
#define ATACMD_STANDBY_IMMEDIATE2           0x94
#define ATACMD_WRITE_BUFFER                 0xE8
#define ATACMD_WRITE_DMA                    0xCA
#define ATACMD_WRITE_DMA_EXT                0x35
#define ATACMD_WRITE_DMA_QUEUED             0xCC
#define ATACMD_WRITE_DMA_QUEUED_EXT         0x36
#define ATACMD_WRITE_MULTIPLE               0xC5
#define ATACMD_WRITE_MULTIPLE_EXT           0x39
#define ATACMD_WRITE_SECTORS                0x30
#define ATACMD_WRITE_SECTORS_EXT            0x34
#define ATACMD_WRITE_VERIFY                 0x3C

#define ATACMD_SRST                          256   // fake command code for Soft Reset

//**************************************************************
//
// Global defines -- ATA register and register bits.
// command block & control block regs
//
//**************************************************************

// These are the offsets into pio_reg_addrs[] of a channel

#define CB_DATA  0   // data reg         in/out pio_base_addr1+0
#define CB_ERR   1   // error            in     pio_base_addr1+1
#define CB_FR    1   // feature reg         out pio_base_addr1+1
#define CB_SC    2   // sector count     in/out pio_base_addr1+2
#define CB_SN    3   // sector number    in/out pio_base_addr1+3
#define CB_CL    4   // cylinder low     in/out pio_base_addr1+4
#define CB_CH    5   // cylinder high    in/out pio_base_addr1+5
#define CB_DH    6   // device head      in/out pio_base_addr1+6
#define CB_STAT  7   // primary status   in     pio_base_addr1+7
#define CB_CMD   7   // command             out pio_base_addr1+7
#define CB_ASTAT 8   // alternate status in     pio_base_addr2+6
#define CB_DC    8   // device control      out pio_base_addr2+6
#define CB_DA    9   // device address   in     pio_base_addr2+7

// error reg (CB_ERR) bits

#define CB_ER_ICRC 0x80    // ATA Ultra DMA bad CRC
#define CB_ER_BBK  0x80    // ATA bad block
#define CB_ER_UNC  0x40    // ATA uncorrected error
#define CB_ER_MC   0x20    // ATA media change
#define CB_ER_IDNF 0x10    // ATA id not found
#define CB_ER_MCR  0x08    // ATA media change request
#define CB_ER_ABRT 0x04    // ATA command aborted
#define CB_ER_NTK0 0x02    // ATA track 0 not found
#define CB_ER_NDAM 0x01    // ATA address mark not found

#define CB_ER_P_SNSKEY 0xf0   // ATAPI sense key (mask)
#define CB_ER_P_MCR    0x08   // ATAPI Media Change Request
#define CB_ER_P_ABRT   0x04   // ATAPI command abort
#define CB_ER_P_EOM    0x02   // ATAPI End of Media
#define CB_ER_P_ILI    0x01   // ATAPI Illegal Length Indication

// ATAPI Interrupt Reason bits in the Sector Count reg (CB_SC)

#define CB_SC_P_TAG    0xf8   // ATAPI tag (mask)
#define CB_SC_P_REL    0x04   // ATAPI release
#define CB_SC_P_IO     0x02   // ATAPI I/O
#define CB_SC_P_CD     0x01   // ATAPI C/D

// bits 7-4 of the device/head (CB_DH) reg

#define CB_DH_LBA  0x40    // LBA bit
#define CB_DH_DEV0 0x00    // select device 0
#define CB_DH_DEV1 0x10    // select device 1
// #define CB_DH_DEV0 0xa0    // select device 0 (old definition)
// #define CB_DH_DEV1 0xb0    // select device 1 (old definition)

// status reg (CB_STAT and CB_ASTAT) bits

#define CB_STAT_BSY  0x80  // busy
#define CB_STAT_RDY  0x40  // ready
#define CB_STAT_DF   0x20  // device fault
#define CB_STAT_WFT  0x20  // write fault (old name)
#define CB_STAT_SKC  0x10  // seek complete (only SEEK command)
#define CB_STAT_SERV 0x10  // service (overlap/queued commands)
#define CB_STAT_DRQ  0x08  // data request
#define CB_STAT_CORR 0x04  // corrected (obsolete)
#define CB_STAT_IDX  0x02  // index (obsolete)
#define CB_STAT_ERR  0x01  // error (ATA)
#define CB_STAT_CHK  0x01  // check (ATAPI)

// device control reg (CB_DC) bits

#define CB_DC_HOB    0x80  // High Order Byte (48-bit LBA)
#define CB_DC_HD15   0x00  // bit 3 is reserved
// #define CB_DC_HD15   0x08  // (old definition of bit 3)
#define CB_DC_SRST   0x04  // soft reset
#define CB_DC_NIEN   0x02  // disable interrupts

#endif

