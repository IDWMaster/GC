#include "GC.h"
#include <iostream>
#include <string.h>

void* deopmalloc(size_t sz) {
  unsigned char* rval = (unsigned char*)malloc(sz);
  auto bot = rval[2];
  rval[4] = 9;
  return rval;
}

int main(int argc, char** argv) {
  void* pool = GC_Init(1);
  for(size_t i = 0;i<900000;i++) {
  
   // free(deopmalloc(50));
    
    void* ptr;
    
    GC_Allocate(pool,50,&ptr);
    if(ptr == 0) {
      std::cerr<<"Allocation error at "<<i<<std::endl;
      return -1;
    }
    GC_Unmark(pool,&ptr);
  }
return 0;
}
