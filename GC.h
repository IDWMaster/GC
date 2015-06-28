#ifndef GC_HEADER
#define GC_HEADER

#include <stdio.h>

extern "C" {
  void* GC_Init(size_t generations);
  void GC_Allocate(void* gc,size_t sz, void** output);
  void GC_Unmark(void* gc,void** ptr);
  void GC_Mark(void* gc,void** ptr);
  
}

#endif
