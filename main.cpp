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
  
  void* pool = GC_Init(1); //Initialize the garbage collector
  for(size_t i = 0;i<900000;i++) { //Allocate, de-allocate a whole bunch of times
  
    void* ptr;
    
    GC_Allocate(pool,50,&ptr); //Allocate memory (50 bytes)
    if(ptr == 0) { //Check if allocation was successful, print error upon failure.
      std::cerr<<"Allocation error at "<<i<<std::endl;
      return -1;
    }
    GC_Unmark(pool,&ptr); //Unmark this section of memory, such that it can be released by the garbage collector at some point in time.
  }
return 0;
}
