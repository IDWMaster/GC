#ifndef GC_HEADER
#define GC_HEADER




#include <stdio.h>
#include <new>
#include <stdint.h>
#include <string.h>

#ifdef GC_FAKE

//Fake GC for testing

void* GC_Init(size_t generations) {
  return (void*)1;
}
void GC_Allocate(size_t sz,size_t numberOfPointers, void** output, void*** ptrList) {
  unsigned char* mander = (unsigned char*)malloc(sz+(sizeof(size_t)*numberOfPointers));
  *output = mander;
  if(ptrList) {
    *ptrList = (void**)mander+sz;
  }
  
}
void GC_Unmark(void** ptr, bool isRoot) {
  
}
void GC_Mark(void** ptr, bool isRoot) {
}
void GC_Collect(bool fullCollection){
}


#else


#ifdef __cplusplus
extern "C" {
#endif
  void* GC_Init(size_t generations);
  /**
   * Allocates an object from managed memory
   * @param gc The heap to allocate from
   * @param sz The amount of memory to allocate
   * @param numberOfPointers The number of pointers that the object contains
   * @param output The memory address to output to
   * @param ptrList The memory address of the first pointer in the pointer list.
   */
  void GC_Allocate(size_t sz,size_t numberOfPointers, void** output, void*** ptrList);
  /**
   * Unmarks a pointer
   * @param gc The heap the object is allocated in
   * @param ptr The memory address to stop tracking
   * @param isRoot Whether or not this value is a root
   * */
  void GC_Unmark(void** ptr, bool isRoot);
  /**
   * Marks a pointer, causing it to be tracked by the garbage collector
   * @param gc The heap the object is allocated in
   * @param ptr The pointer to track
   * @param isRoot Whether or not this value is a root
   */
  void GC_Mark(void** ptr, bool isRoot);
  /**
   * Forces a garbage collection
   * @param gc The heap to collect from
   * @param fullCollection Whether or mot to collect memory from all generations (not just the first one)
   * */
  void GC_Collect(bool fullCollection);
  
  
#ifdef __cplusplus
}
#endif



#ifdef __cplusplus
  template<typename T>
  static inline void GCMark(T** ptr, bool isRoot) {
    GC_Mark((void**)ptr,isRoot);
  }
  
  template<typename T>
  static inline void GCUnmark(T** ptr, bool isRoot) {
    GC_Unmark((void**)ptr,isRoot);
  }
#endif

#endif

#endif
