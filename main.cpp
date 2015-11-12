/*
 *Author: Brian Bosak 
 * Last updated: 09/01/2015
 * TEST PROGRAM
*/




//Include files
#include "GC.h"
#include <iostream>
#include <string.h>

using namespace GC;

class SomeClass {
public:
  SomeClass(const GCHandle<SomeClass>& thisptr) {
    
    printf("This is %p\n",this);
    GCHandle<SomeClass> myptr(thisptr);
    printf("The pointer points to %p and this points to %p\n",myptr.ptr,this);
  }
  ~SomeClass() {
  }
};

//Main entry point
int main(int argc, char** argv) {
  GCHeap heap(2);
  GCHandle<SomeClass> m;
  heap.Construct<SomeClass>(m);
return 0;
}
