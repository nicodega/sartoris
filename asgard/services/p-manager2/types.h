
#ifndef PMANTYPESH
#define PMANTYPESH

#ifndef NULL
#	define NULL 0
#endif

#define TRUE 1
#define FALSE 0

typedef unsigned char      BOOL;
typedef unsigned char      BYTE;
typedef unsigned int       UINT32;
typedef int	               INT32;
typedef unsigned long long UINT64;
typedef long long          INT64;
typedef unsigned short     UINT16;
typedef short              INT16;
typedef unsigned char      UINT8;
typedef void*              ADDR;

#ifndef PACKED_ATT
#define PACKED_ATT __attribute__ ((__packed__));
#endif

#endif

