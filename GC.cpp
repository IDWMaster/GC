#include <iostream>
#include <vector>
#include <string.h>

static inline void MEM_Init(void* region, size_t sz) {
//Initialize memory region
  size_t* ptr = (size_t*)region;
    //Allocation header:
    //Total size of memory region (including header)
    //Pointer to data segment (fast lookup)
    //Number of pointers in list
    //List of pointers (write barriers)
  ptr[0] = sz;
  ptr[1] = ptr+3;
  ptr[2] = 0;
  ptr[3] = 0;
}
static inline size_t MEM_ListCapacity(void* region) {
  size_t* ptr = (size_t*)region;
  return (ptr[1]-(size_t)ptr)/sizeof(size_t);
}
static inline size_t MEM_ListLength(void* region) {
  size_t* ptr = ((size_t*)region);
  return ptr[2];
}

static inline void MEM_AddPtr(void* region, void* ptr) {
  size_t* rptr = (size_t*)region;
  rptr[3+rptr[2]] = ptr;
}
static inline void MEM_RemovePtr(void* region, void* ptr) {
  size_t* meta_start = (size_t*)region;
  
  size_t* rptr = ((size_t*)region)+3;
  size_t i;
  for(i = 0;rptr[i] != ptr;i++) {};
  
  memmove(rptr+i,rptr+meta_start[2],meta_start[2]-(i*sizeof(size_t)));
  meta_start[2]--;
}

class GCGeneration {
public:
  unsigned char* memory_unaligned;
  unsigned char* memory;
  size_t marker;
  size_t genSz;
  
  size_t* freeList;
  size_t freeList_size;
  size_t freeList_capacity;
  GCGeneration() {
    memory_unaligned = new unsigned char[1024*1024*5];
    size_t start_addr = (size_t)memory_unaligned;
    size_t orig_addr = start_addr;
    start_addr+=sizeof(size_t)-(start_addr % sizeof(size_t));
    memory = (unsigned char*)start_addr;
    genSz = 1024*1024*5-(start_addr-orig_addr);
    marker = 0;
    freeList_size = 0;
    freeList_capacity = 0;
    freeList = 0;
  }
  void Free(void* ptr) {
    //Add object to free list
    if(freeList == 0) {
      freeList = new size_t[1];
      freeList[0] = ptr;
      return;
    }
    if(freeList_capacity-freeList_size == 0) {
      size_t newCapacity = freeList_capacity*2;
      size_t* newList = new size_t[newCapacity];
      memcpy(newList,freeList,freeList_size*sizeof(size_t));
      delete[] freeList;
      freeList = newList;
      freeList_capacity = newCapacity;
    }
    freeList[freeList_size] = ptr;
  }
  size_t Available() {
    return genSz-marker;
  }
  //Write barrier, mark
  void WB_Mark(void*& ptr) {
    if(MEM_ListCapacity(ptr)-MEM_ListLength(ptr) == 0) {
      //Expand list
      size_t* realPtr = (size_t*)ptr;
      size_t prevCapacity = MEM_ListCapacity(ptr);
      size_t newCapacity = prevCapacity*2;
      //Allocate new block
      Unsafe_Allocate();
    }
    
  }
  void* Unsafe_Allocate(size_t sz) {
    if(sz>Available()) {
      return 0;
    }
    void* retval = marker+sz;
    marker+=sz;
    return retval;
  }
  //Return pointer to memory space
  void* Allocate(size_t sz) {
   //Ensure enough memory for header
      sz+=sizeof(size_t)*4;
    //Align memory allocation request 
      sz += sizeof(size_t)-(sz % sizeof(size_t));
      void* retval = Unsafe_Allocate(sz);
      if(retval) {
	MEM_Init(retval);
	return retval;
      }
      return 0;
  }
  ~GCGeneration() {
    delete[] memory_unaligned;
    if(freeList) {
      delete[] freeList;
    }
  }
};
class GCPool {
public:
  GC_Ptr* ptrTable;
  size_t ptrTable_sz;
  GCPool() {
    ptrTable_sz = 0;
    ptrTable = new GC_Ptr[80];
    ptrTable_sz = 80;
  }
  GC_Ptr* allocptr() {
    
  }
  ~GCPool() {
    delete[] ptrTable;
  }
};

static GCPool pool;
extern "C" {
  
}