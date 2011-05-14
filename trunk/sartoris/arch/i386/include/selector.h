#ifndef __SELECTORS_H
#define __SELECTORS_H

/* segment selector macros */

#define KRN_CODE     0x0008    /* 1 desc of gdt, rpl=0 */
#define KRN_DATA_SS  0x0010    /* 2 desc of gdt, rpl=0 */

#define PRIV1_CODE_SS  0x0005  /* 1 desc of ldt, rpl=1 */
#define PRIV1_DATA_SS  0x000d  /* 2 desc of ldt, rpl=1 */
#define PRIV1_HIMEM_SS 0x0019  /* 3 desc of gdt, rpl=1 */

#define PRIV2_CODE_SS  0x0006  /* 1 desc of ldt, rpl=2 */
#define PRIV2_DATA_SS  0x000e  /* 2 desc of ldt, rpl=2 */
#define PRIV2_HIMEM_SS 0x001a  /* 3 desc of gdt, rpl=2 */

#define PRIV3_CODE_SS  0x0007  /* 1 desc of ldt, rpl=3 */
#define PRIV3_DATA_SS  0x000f  /* 2 desc of ldt, rpl=3 */

#endif
