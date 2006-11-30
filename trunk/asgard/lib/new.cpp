
#include <lib/structures/stdlibsim.h>

extern "C" int _exit(int ret);
extern "C" void *calloc(size_t nelem, size_t elsize);
extern "C" void free(void *ptr);

/*
Support for New/Delete and virtual functions.
*/

/* Used when puere virtual functions are not defined */
extern "C" void __cxa_pure_virtual()
{
    _exit(-20); // finish the program with an exception
}

/* NOTE: Remember to destruct your objects in the opposite order you have constructed them. */

//overload the operator "new"
void * operator new (uint_t size)
{
    return calloc(size,1);
}

//overload the operator "new[]"
void * operator new[] (uint_t size)
{
    return calloc(size,1);
}

//overload the operator "delete"
void operator delete (void * p)
{
    free(p);
}

//overload the operator "delete[]"
void operator delete[] (void * p)
{
    free(p);
}

/*These are awful, we should keep track of these in order not to step over, but this will do fine for now. */
inline void* operator new(uint_t, void* p)   { return p; }
inline void* operator new[](uint_t, void* p) { return p; }
inline void  operator delete  (void*, void*) { };
inline void  operator delete[](void*, void*) { };



