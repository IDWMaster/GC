#include "GC.h"
#include <iostream>

int main(int argc, char** argv) {
  void* pool = GC_Init(1);
  for(size_t i = 0;i<500000;i++) {
  
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
