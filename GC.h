#ifndef GC_HEADER
#define GC_HEADER

#include <stdio.h>
#include <new>
extern "C" {
  void* GC_Init(size_t generations);
  void GC_Allocate(void* gc,size_t sz, void** output);
  void GC_Unmark(void* gc,void** ptr);
  void GC_Mark(void* gc,void** ptr);
  
}


#ifdef __cplusplus



namespace GC {
template<typename T>
class GCHandle {
public:
  T* ptr;
  void* heap;
  GCHandle() {
    ptr = 0;
  }
  GCHandle<T> operator=(const GCHandle<T>& other) {
    if(ptr) {
      GC_Unmark(heap,(void**)&ptr);
    }
    heap = other.heap;
    ptr = other.ptr;
    GC_Mark(heap,(void**)&ptr);
  }
  GCHandle(void* heap, const T* objaddr) {
    this->ptr = (T*)objaddr;
    this->heap = heap;
    GC_Mark(heap,(void**)&ptr);
    
  }
  //Copy constructor
  GCHandle(const GCHandle<T>& other) {
    ptr = other.ptr;
   heap = other.heap;
   GC_Mark(heap,(void**)&ptr);
  }
  T* operator->() {
    return ptr;
  }
  T* operator*() {
    return ptr;
  }
  ~GCHandle() {
    if(ptr) {
      GC_Unmark(heap,(void**)&ptr);
    }
  }
};
class GCHeap {
public:
  void* ptr;
  GCHeap(int generations) {
    ptr = GC_Init(generations);
  }
  template<typename T, typename... args>
  void Construct(GCHandle<T>& handle, args... uments) {
    handle.heap = ptr;
    GC_Allocate(ptr,sizeof(T),(void**)&handle.ptr);
    //TODO: Placement NEW
    new(handle.ptr)T(uments...);
  }
  template<typename T,typename... args>
  GCHandle<T> Construct(args... uments) {
    GCHandle<T> retval;
    Construct(retval,uments...);
    return retval;
  }
  ~GCHeap() {
    
  }
};
}
#endif


#endif
