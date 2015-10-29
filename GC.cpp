#include <iostream>
#include <vector>
#include <string.h>


//A header containing additional object metadata, which are necessary for proper function of the garbage collection algorithm
typedef struct {
  void(*Destructor)(void*,void*); //Destructor with heap pointer and object pointer
} ObjectHeader;




//Converts an object address to a segment pointer
static inline void* FindSegmentPointer(void* objAddress) {
  objAddress = ((unsigned char*)objAddress);
  return (void*)*(((size_t*)objAddress)-1);
}


static inline void MEM_Init(void* region, size_t sz) {
//Initialize memory region
  size_t* ptr = (size_t*)region;
    //Allocation header:
    //Total size of memory region (including header)
    //Pointer to data segment (fast lookup)
    //Number of pointers in list
    //List of pointers (write barriers)
    //Pointer to beginning of memory region
    //Data segment
  ptr[0] = sz; //Total size (including header)
  ptr[1] = (size_t)(ptr+5); //Pointer to data segment
  ptr[2] = 0; //Number of pointers in list (default 0)
  ptr[3] = 0; //Empty pointer
  ptr[4] = (size_t)ptr; //Pointer to allocation header
}

//Gets the number of pointer entries that this memory region can store
static inline size_t MEM_ListCapacity(void* region) {
  size_t* ptr = (size_t*)region;
  size_t listStart = (size_t)(ptr+3); //Starting address of list
  size_t dataAddress = (size_t)ptr[1]; //Start of data in segment
  return ((dataAddress-listStart)/sizeof(size_t))-1; //Capacity = ((Data address)-(starting address of list))-sizeof(Pointer to allocation header)
  
}
//Get the number of pointers currently in the list. 
static inline size_t MEM_ListLength(void* region) {
  size_t* ptr = ((size_t*)region);
  return ptr[2];
}
//Inserts a pointer into the list, increasing its length by 1 (assumes that the list has enough capacity to store the element)
static inline void MEM_AddPtr(void* region, void* ptr) {
  size_t* rptr = (size_t*)region;
  rptr[3+rptr[2]] = (size_t)ptr;
  rptr[2]++;
}
//Removes a pointer from the list, decrementing its length by one.
static inline void MEM_RemovePtr(void* region, void* ptr) {
  size_t* meta_start = (size_t*)region;
  
  size_t* rptr = ((size_t*)region)+3;
  size_t i;
  for(i = 0;rptr[i] != (size_t)ptr;i++) {};
  //meta_Start[2] is number of pointers in list
  memmove(rptr+i,rptr+meta_start[2],(meta_start[2]*sizeof(size_t))-(i*sizeof(size_t)));
  meta_start[2]--;
}
//Computes the length of the data segment of this memory region.
static inline size_t MEM_DataSize(void* region) {
  size_t* ptr = (size_t*)region;
  return ptr[1]-(size_t)ptr;
}
//Updates all pointers to an object, from the src memory chunk to the dest memory chunk
//dest -- Destination address to copy to
//src -- Source address
//capacityChange -- Number of new elements to add to the list (amount of capacity increase)
static inline size_t MEM_MovePtr(void* dest, void* src, size_t capacityChange = 0) {
  printf("Move ptr in progress from %p to %p\n",src,dest);
  size_t* ptr = (size_t*)src;
  size_t* destchunk = (size_t*)dest;
  //Copy all data from ptr to destchunk (we assume that it is big enough -- if it isn't, too bad for you; you're gonna have a really rough time debugging this)
  memcpy(destchunk,ptr,*ptr);
  destchunk[1] = (size_t)((ptr[1]-(size_t)ptr)+((size_t)destchunk)+capacityChange); //Updated position = segment offset of data segment+destination address+(number of new elements in list)
  printf("Updated position chunk to %p\n",(void*)destchunk[1]);
  
  
  for(size_t i = 0;i<destchunk[2];i++) {
    void** d = (void**)destchunk[i+3];
    *d = (void*)destchunk[1]; //Update all references to point to this new memory address.
  }
  //Update segment pointer in dest
  size_t* dataptr = (size_t*)destchunk[1];
  *(dataptr-1) = (size_t)destchunk; //TODO: Does this work?
  printf("Move ptr complete\n"); 
}


class GCGeneration {
public:
  unsigned char* memory_unaligned; //Unaligned memory chunk.
  unsigned char* memory; //Memory chunk aligned to machine-specific word size
  size_t marker; //Current marker (in bytes) into free-space
  size_t genSz; //The size of this generation
  size_t liveObjectCount; //The current count of live objects
  GCGeneration() {
    memory_unaligned = new unsigned char[1024*512];
    size_t start_addr = (size_t)memory_unaligned;
    size_t orig_addr = start_addr;
    start_addr+=sizeof(size_t)-(start_addr % sizeof(size_t));
    memory = (unsigned char*)start_addr;
    genSz = 1024*512-(start_addr-orig_addr);
    marker = 0;
    next = 0;
    liveObjectCount = 0;
  }
  
  void Compact() {
   //Go through all memory chunks, find free ones, and move them to the left one by one
    size_t allocBreak = 0; //Allocation break (objects to left of this are free space, objects to right are in-use)
    size_t offset = 0; //Current offset
    size_t foundObjects = 0; //Number of objects still in-use
    while(foundObjects<liveObjectCount) {
    
      if(offset == marker) {
	//Compaction cycle complete.
	marker = allocBreak;
	break;
      }
      size_t fragsz; //Size of memory fragment (including headers)
      memcpy(&fragsz,memory+offset,sizeof(fragsz));
      if(fragsz == 0) { //Hmm. We should at least have a header here.
	throw "memory corrupt";
      }
      if(MEM_ListLength(memory+offset) == 0) { //If there's no active pointers, we have a free object.
	//Free segment found. Move memory from right of this region into current one
	if((size_t)(memory+offset+fragsz) == marker) {
	  //End-of-list
	  break;
	}
	
	unsigned char* dest_position = memory+offset; //Position of current fragment
	unsigned char* src_position = dest_position+fragsz; //Source block = addressof(current block)+sizeof(current block)
	while(src_position<memory+genSz) {
	  //Move object from the right into this position (IMPORTANT NOTE: This segment COULD have gotten smaller, or larger)
	  MEM_MovePtr(dest_position,src_position);
	  dest_position += *((size_t*)dest_position);
	  src_position += *((size_t*)src_position);
	}
	
	
      }else {
	//Found in use block, make sure the break is after this block
	allocBreak = offset+fragsz;
	foundObjects++;
	//TODO: Promote this block to the next generation of Star Trek.
	if(next) {
	  
	}
      }
      //Move to next segment
      offset+=fragsz;
      
      
    }
    marker = allocBreak;
  }
  size_t Available() {
    return genSz-marker;
  }
  //Write barrier, mark
  void WB_Mark(void*& ptr) {
    size_t* realPtr = (size_t*)FindSegmentPointer(ptr);
    printf("Mark at address %p\n",realPtr);
    printf("WB mark -- Available space in list == %i out of %i used\n",(int)MEM_ListLength(realPtr),(int)MEM_ListCapacity(realPtr));
    if(MEM_ListCapacity(realPtr)-MEM_ListLength(realPtr) == 0) {
      //Expand list
      size_t prevCapacity = MEM_ListCapacity(realPtr);
      size_t newCapacity = prevCapacity*2;
      //Allocate new block
      size_t* newmem = (size_t*)Unsafe_Allocate((4*sizeof(size_t))+(newCapacity*sizeof(size_t))+MEM_DataSize(realPtr));
      MEM_MovePtr(newmem,realPtr,(newCapacity-prevCapacity)*sizeof(size_t));
      realPtr = newmem;
      ptr = (void*)newmem[1];
    }
    MEM_AddPtr(realPtr,&ptr);
    
  }
  
  void WB_Unmark(void*& ptr) {
    void* segptr = FindSegmentPointer(ptr);
    MEM_RemovePtr(segptr,&ptr);
    if(MEM_ListLength(segptr) == 0) {
      //Object can be reclaimed (no longer alive)
      liveObjectCount--;
      if(liveObjectCount == 0) {
	//Set marker to initial position (reduces number of memory fragments and prevents unncessary growth of pool)
	marker = 0;
      }
    }
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
      sz+=sizeof(size_t)*5;
    //Align memory allocation request 
      sz += sizeof(size_t)-(sz % sizeof(size_t));
      void* retval = Unsafe_Allocate(sz);
      if(retval) {
	MEM_Init(retval,sz);
	liveObjectCount++;
	return retval;
      }
      return 0;
  }
  GCGeneration* next;
  bool Contains(void*& ptr) {
    //Don't need segment pointer here. Should still be within bounds.
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
    printf("Allocate block at address %p\n",ptr);
    *output = ((unsigned char*)((void**)ptr)[1]); //Output fast pointer to data segment
    if(ptr != 0) {
      gen->WB_Mark(*output); //MARK pointer
    }
  }
  void GC_Unmark(void* gc,void** ptr) {
    GCGeneration* gen = ((GCPool*)gc)->firstGeneration;
    
    while(!gen->Contains(*ptr)) {
      printf("Object %p not found. Moving to newer generation of Star Trek....\n",*ptr);
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