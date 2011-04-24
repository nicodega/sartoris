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

#ifndef INT64H
#define INT64H

#ifdef __cplusplus
extern "C" {
#endif

long long __ashldi3 (long long a, int b); // return the result of shifting a left by b bits.
long long __ashrdi3 (long long a, int b); // result of arithmetically shifting a right by b bits.
long long __divdi3 (long long a, long long b); // return the quotient of the signed division of a and b.
//long long __lshrdi3 (long long a, int b); // return the result of logically shifting a right by b bits.
long long __moddi3 (long long a, long long b); // return the remainder of the signed division of a and b.
long long __muldi3 (long long a, long long b); // return the product of a and b
long long __negdi2 (long long a); // negation
unsigned long long __udivdi3 (unsigned long long a, unsigned long long b); // return the quotient of the unsigned division of a and b.
//unsigned long long __udivmoddi3 (unsigned long long a, unsigned long long b, unsigned long long *c); // calculate both the quotient and remainder of the unsigned division of a and b. The return value is the quotient, and the remainder is placed in variable pointed to by c.
unsigned long long __umoddi3 (unsigned long long a, unsigned long long b); // return the remainder of the unsigned division of a and b.

#ifdef __cplusplus
}
#endif

#endif
