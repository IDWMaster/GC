/*
 *Author: Brian Bosak 
 * Last updated: 09/01/2015
 * TEST PROGRAM
*/




//Include files
#include "GC.h"
#include <iostream>
#include <string.h>



//Main entry point
int main(int argc, char** argv) {
  void* heap = GC_Init(2);
  void* mclass;
  void** ptrlist;
  GC_Allocate(0,1,(void**)&mclass,&ptrlist);
  //Mark root from C(++) ABI
  printf("Object is at address %p\n",mclass);
  GC_Mark((void**)&mclass,true);
  ptrlist[0] = mclass; //This object references itself
 
 GC_Mark(ptrlist+0,false); //Track value in garbage collector
 
 printf("Object is at address %p\n",mclass);
  GC_Unmark((void**)&mclass,true); //We no longer need to reference this object from C(++). Free it.
  GC_Collect(true); //Force garbage collection of all generations
return 0;
}
