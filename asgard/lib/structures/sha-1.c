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

#include "../../include/lib/structures/sha-1.h"

static void longReverse(unsigned int *buffer, int byteCount)
{
    unsigned int value;

    byteCount >>= 2;
    while( byteCount-- )
        {
        value = *buffer;
        value = ( ( value & 0xFF00FF00L ) >> 8  ) | \
                ( ( value & 0x00FF00FFL ) << 8 );
        *buffer++ = ( value << 16 ) | ( value >> 16 );
        }
}


unsigned int padding(unsigned char *str,unsigned int length, unsigned int **pstr)
{
	/* 

		a.  "1" is appended.  Example: if the original message is "01010000", this
			 is padded to "010100001".

		b.  "0"s are appended.  The number of "0"s will depend on the original length
			 of the message.  The last 64 bits of the last 512-bit block are reserved 
			 for the length l of the original message.

			 Example:  Suppose the original message is the bit string

				  01100001 01100010 01100011 01100100 01100101.
          
			 After step (a) this gives

				  01100001 01100010 01100011 01100100 01100101 1.

			 Since l = 40, the number of bits in the above is 41 and 407 "0"s are 
			 appended, making the total now 448. This gives (in hex)

				  61626364 65800000 00000000 00000000
				  00000000 00000000 00000000 00000000
				  00000000 00000000 00000000 00000000
				  00000000 00000000.

		c.  Obtain the 2-word representation of l, the number of bits in the original
			 message.  If l < 2^32 then the first word is all zeroes.  Append these
			 two words to the padded message. 
         
			 Example: Suppose the original message is as in (b). Then l = 40 (note 
			 that l is computed before any padding). The two-word representation of
			 40 is hex 00000000 00000028. Hence the final padded message is hex

				  61626364 65800000 00000000 00000000
				  00000000 00000000 00000000 00000000
				  00000000 00000000 00000000 00000000
				  00000000 00000000 00000000 00000028.

		The padded message will contain 16 * n words for some n > 0.  The padded 
		message is regarded as a sequence of n blocks M(1) , M(2), ... , M(n), where 
		each M(i) contains 16 words and M(1) contains the first characters (or bits)
		of the message.
*/
	
	unsigned int size = 64 + (length << 3) + 1;
	int rem = size & 0x00000DFF;
	unsigned char *ret;
	unsigned int i = 0, j;

	// Calculate the size of the padded string of bits //


	size = ((size & 0xFFFFFE00) + ((rem == 0)? 0 : 512)) >> 3 ;

	ret = (unsigned char*)malloc(size); 

	// copy original string //

	for(i = 0; i < length; i++)
	{
		ret[i] = str[i];
	}

	j = length;

	// fill with zeros //
	for(i; i < size; i++)
	{
		ret[i] = 0;
	}

	// set the 1... //

	ret[j] = (ret[j] & 0x7F) | 0x80; // set the highest bit on 1

	*pstr = (unsigned int*)ret;

#ifdef LITTLE_ENDIAN
	longReverse(*pstr, size); // consider the added bit
	
#endif

	(*pstr)[(i >> 2)-2] = 0;
	(*pstr)[(i >> 2)-1] = length << 3;

	// this function will work with lengths below or equal to 2^32-1

	return size;
}

#define S(n,X)  ( ( ( X ) << n ) | ( ( X ) >> ( 32 - n ) ) )
/*
	 A sequence of logical functions f(0), f(1),..., f(79) is used in the SHA-1.
	 Each f(t), 0 <= t <= 79, operates on three 32-bit words B, C, D and produces
	 a 32-bit word as output.  f(t;B,C,D) is defined as follows:
	 for words B, C, D,

     f(t;B,C,D) = (B AND C)  OR  ((NOT B) AND D)              ( 0 <= t <= 19)

     f(t;B,C,D) = B  XOR   C   XOR   D                        (20 <= t <= 39)

     f(t;B,C,D) = (B AND C)  OR  (B AND D)  OR  (C AND D)     (40 <= t <= 59)

     f(t;B,C,D) = B   XOR   C   XOR  D                        (60 <= t <= 79).
*/

#define f(t,b,c,d) ((t <= 19)? (b & c) | ((~b) & d) : 0 | \
				   (t > 19 && t <= 39)? (b ^ c ^ d): 0 | \
				   (t > 39 && t <= 59)? (b & c) | (b & d) | (c & d) : 0 | \
				   (t > 59)? b ^ c ^ d : 0 )

/*
    A sequence of constant words K(0), K(1), ... , K(79) is used in the SHA-1.
	In hex these are given by

      K(t) = 5A827999         ( 0 <= t <= 19)

      K(t) = 6ED9EBA1         (20 <= t <= 39)

      K(t) = 8F1BBCDC         (40 <= t <= 59)

      K(t) = CA62C1D6         (60 <= t <= 79).
*/
#define K(t) ((t <= 19)? 0x5A827999 : 0 | \
	         (t > 19 && t <= 39)? 0x6ED9EBA1 : 0 | \
			 (t > 39 && t <= 59)? 0x8F1BBCDC : 0 | \
			 (t > 59)? 0xCA62C1D6 : 0)

#define MASK 0x0000000F


void compute_hash(unsigned int *padded_str, int length, unsigned int H[5])
{
	unsigned int A,B,C,D,E;
	int n = length >> 6; // n = length / ... n = Count(M)
	int i = 0, t = 0, s;
	unsigned int W[16], temp; // circular queue buffer

	H[0] = 0x67452301;
	H[1] = 0xEFCDAB89;
	H[2] = 0x98BADCFE;
	H[3] = 0x10325476;
	H[4] = 0xC3D2E1F0;

	// divide M(i) on 16 words //
	for(i = 0; i < n; i++)
	{
		// process the Block //
		A = H[0]; 
		B = H[1]; 
		C = H[2]; 
		D = H[3]; 
		E = H[4];

		for(s = 0; s < 16; s++)
		{
			W[s] = padded_str[s + (i << 4)];
		}

		for(t = 0; t <= 79; t++)
		{
			s = t & MASK;

			if (t >= 16) W[s] = S(1,W[(s + 13) & MASK] ^ W[(s + 8) & MASK] ^ W[(s + 2) & MASK] ^ W[s]);

			temp = S(5,A) + f(t,B,C,D) + E + W[s] + K(t);
  
			E = D; 
			D = C; 
			C = S(30,B); 
			B = A; 
			A = temp;
		}

		H[0] += A; 
		H[1] += B;
		H[2] += C;
		H[3] += D;
		H[4] += E;
	}
			
}


