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
  ptr[1] = (size_t)(ptr+3);
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
  rptr[3+rptr[2]] = (size_t)ptr;
  rptr[2]++;
}
static inline void MEM_RemovePtr(void* region, void* ptr) {
  size_t* meta_start = (size_t*)region;
  
  size_t* rptr = ((size_t*)region)+3;
  size_t i;
  for(i = 0;rptr[i] != (size_t)ptr;i++) {};
  
  memmove(rptr+i,rptr+meta_start[2],meta_start[2]-(i*sizeof(size_t)));
  meta_start[2]--;
}

static inline size_t MEM_DataSize(void* region) {
  size_t* ptr = (size_t*)region;
  return ptr[1]-(size_t)ptr;
}

static inline size_t MEM_MovePtr(void* dest, void* src) {
  size_t* ptr = (size_t*)src;
  for(size_t i = 0;i<ptr[2];i++) {
    void** d = (void**)ptr[i+3];
    *d = dest;
  }
}


class GCGeneration {
public:
  unsigned char* memory_unaligned;
  unsigned char* memory;
  size_t marker;
  size_t genSz;
  GCGeneration() {
    memory_unaligned = new unsigned char[1024*512];
    size_t start_addr = (size_t)memory_unaligned;
    size_t orig_addr = start_addr;
    start_addr+=sizeof(size_t)-(start_addr % sizeof(size_t));
    memory = (unsigned char*)start_addr;
    genSz = 1024*512-(start_addr-orig_addr);
    marker = 0;
    next = 0;
  }
  void Compact() {
   //Go through all memory chunks, find free ones, and move them to the left one by one
    size_t allocBreak = 0;
    size_t offset = 0;
    while(true) {
      if(offset>marker) {
      throw "oob"; //out of bounds
	
      }
      if(offset == marker) {
	//Compaction cycle complete.
	marker = allocBreak;
	
	//  std::cerr<<"Marker updated to "<<marker<<std::endl;;
	break;
      }
      size_t fragsz;
      memcpy(&fragsz,memory+offset,sizeof(fragsz));
      if(fragsz == 0) {
	throw "memory corrupt";
      }
      if(MEM_ListLength(memory+offset) == 0) {
	//std::cerr<<"Free move\n";
	//Free segment found. Move memory from right of this region into current one
	if((size_t)(memory+offset+fragsz) == marker) {
	  //End of list encountered. Compaction complete. Update marker
	  marker = (size_t)(memory+offset);
	  std::cerr<<"Marker updated to "<<marker<<std::endl;;
	  break;
	}
	//Update all pointers to this memory segment
	MEM_MovePtr(memory+offset,memory+offset+fragsz);
	//Copy contents to this segment
	memcpy(memory+offset,memory+offset+fragsz,*((size_t*)(memory+offset+fragsz)));
	//Fragment size may have changed. Process before moving to next segment.
	memcpy(&fragsz,memory+offset,sizeof(fragsz));
	memset(memory+offset+fragsz+(2*sizeof(size_t)),0,sizeof(size_t)); //TODO: Mark next segment as free
	
      }else {
	//Found in use block, make sure the break is after this block
	allocBreak = offset+fragsz;
      }
      //Move to next segment
      offset+=fragsz;
      
      
    }
  }
  size_t Available() {
    return genSz-marker;
  }
  //Write barrier, mark
  void WB_Mark(void*& ptr) {
    size_t* realPtr = (size_t*)ptr;
    if(MEM_ListCapacity(ptr)-MEM_ListLength(ptr) == 0) {
      //Expand list
      size_t prevCapacity = MEM_ListCapacity(ptr);
      size_t newCapacity = prevCapacity*2;
      //Allocate new block
      void* newmem = Unsafe_Allocate((3*sizeof(size_t))+newCapacity+MEM_DataSize(ptr));
      MEM_MovePtr(newmem,ptr);
      memcpy(newmem,ptr,realPtr[0]);
      ptr = newmem;
    }
    MEM_AddPtr(realPtr,&ptr);
    
  }
  
  void WB_Unmark(void*& ptr) {
    MEM_RemovePtr(ptr,&ptr);
  }
  void* Unsafe_Allocate(size_t sz) {
    if(sz>Available()) {
      return 0;
    }
    void* retval = memory+marker;
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
	MEM_Init(retval,sz);
	return retval;
      }
      return 0;
  }
  GCGeneration* next;
  bool Contains(void* ptr) {
    size_t c = (size_t)ptr;
    size_t a = (size_t)memory;
    size_t b = (size_t)memory+marker;
    return c>=a && c<=b;
  }
  ~GCGeneration() {
    delete[] memory_unaligned;
    if(next) {
      delete next;
    }
  }
};
class GCPool {
public:
  //The VERY FIRST generation of Star Trek!
  GCGeneration* firstGeneration;
  GCPool(size_t generations) {
    firstGeneration = new GCGeneration();
  }
  ~GCPool() {
    delete firstGeneration;
  }
};
extern "C" {
  void* GC_Init(size_t generations) {
    return new GCPool(generations);
  }
  void GC_Allocate(void* gc,size_t sz, void** output) {
    GCGeneration* gen = ((GCPool*)gc)->firstGeneration;
    void* ptr = gen->Allocate(sz);
    if(ptr == 0) {
      //Compact
      gen->Compact();
      ptr = gen->Allocate(sz);
      //TODO: Increase total size of pool
    }
    *output = ptr;
    if(ptr != 0) {
    gen->WB_Mark(*output);
    }
  }
  void GC_Unmark(void* gc,void** ptr) {
    GCGeneration* gen = ((GCPool*)gc)->firstGeneration;
    while(!gen->Contains(*ptr)) {
      gen = gen->next;
    }
    gen->WB_Unmark(*ptr);
  }
  void GC_Mark(void* gc,void** ptr) {
    GCGeneration* gen = ((GCPool*)gc)->firstGeneration;
    while(!gen->Contains(*ptr)) {
      gen = gen->next;
    }
    gen->WB_Mark(*ptr);
  }
}