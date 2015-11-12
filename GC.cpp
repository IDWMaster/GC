#include <iostream>
#include <string.h>



//TODO: Important NOTE -- Memory alignment is INCREDIBLY important here. GCC/G++ will compile the below struct to 24 bytes.
//Other compilers should be tested to ensure that struct padding is sufficient to prevent alignment faults in case an odd size is selected for this struct.
//A header containing additional object metadata, which are necessary for proper function of the garbage collection algorithm
typedef struct {
  void(*Destructor)(void*,void*); //Destructor with heap pointer and object pointer
  bool allocmark;
  size_t pointerCount; //Number of objects in this object's pointer table  (the pointer table is located before the start of this metadata header)
} ObjectMetadata;


/**
 * A list that can store plain-old-data elements. This list type will not work for complex types.
 * */
template<typename T>
class PODList {
public:
  T* values;
  //The number of elements in the array
  size_t count;
  //The size (capacity) of the array
  size_t size;
  PODList() {
    values = 0;
    count = 0;
    size = 0;
  }
  void EnsureCapacity() {
    if(values == 0) {
      values = new T[1];
      size = 1;
      return;
    }
    if(count == size) {
    T* nlist = new T[size*2];
    memcpy(nlist,values,size*sizeof(T));
    values = nlist;
    size*=2;
    }
    
  }
  /**
   * Adds an element to the list
   * @param value The element to add
   * */
  void Add(const T& value) {
    EnsureCapacity();
    values[count] = value;
    count++;
  }
  /**
   * Inserts a specified value at a given index, moving all elements to the right
   * */
  void InsertAt(size_t index, const T& value) {
    EnsureCapacity();
    memmove(values+index+1,values+index,sizeof(T)*(count-index));
    values[index] = value;
    count++;
  }
  
  /**
   * Removes an element at the specified index
   * @param The index of the element to remove
   * */
  void RemoveAt(size_t index) {
    count--;
    memmove(values+index,values+index+1,sizeof(T)*(count-index));
  }
  T& operator[](size_t index) {
    return values[index];
  }
  ~PODList() {
    if(values) {
      delete[] values;
    }
  }
};
template<typename T>
class PODSet {
public:
  PODList<T> array;
  PODSet() {
    
  }
  void Insert(const T& value) {
    if(array.count == 0) {
      array.Add(value);
      return;
    }
    size_t index;
    bool found = BSearch(value,index);
    if(found) {
      array[index] = value;
      return;
    }
    if(value<array[index]) {
      //Insert to left of current value
      array.InsertAt(index,value);
    }else {
      //Insert after current value
      array.InsertAt(index+1,value);
    }
    
  }
  bool Find(T& value) {
    size_t index;
    bool retval = BSearch(value,index);
    value = array[index];
    return retval;
  }
  bool BSearch(const T& value,size_t& index) {
    index = 0;
    size_t start = 0;
    size_t end = array.count;
    
    while(end-start>0) {
      index = array.count/2;
      if(array[index] == value) {
	return true;
      }
      if(value<array[index]) {
	//Go down
	end = index-1;
      }else {
	//Go up
	start = index+1;
      }
    }
    return false;
  }
  
};



/*
 * Full collection algorithm:
 * Scan through all objects in the heap.
 * Determine if the object is a root. An object is a root if anything referencing it exists from outside the managed heap (such as executable code; for example)
 * If the object is a root, recursively follow all pointers going out from that object. Toggle a bit in the ObjectMetadata for that object to indicate that it is allocated (in use).
 * NEXT:
 * Scan all objects in the heap. If an object does not have the allocmark bit set, it can be freed. Free the object.
 * 
 * Problems with this algorithm:
 * This algorithm will run through all elements twice. It may be more efficient to track roots as they are allocated, in some sort of external table.
 * However; this will add a dependency to an OS-specific dynamic memory allocator, which may not be desirable, and could cause the program to take up additional space in memory.
 * 
 * 
 * 
 * */




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
static inline void* MEM_GetDataPointer(void* region) {
  return (void*)((size_t*)region)[1];
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
  size_t* ptr = (size_t*)src;
  size_t* destchunk = (size_t*)dest;
  //Copy all data from ptr to destchunk (we assume that it is big enough -- if it isn't, too bad for you; you're gonna have a really rough time debugging this)
  memcpy(destchunk,ptr,*ptr);
  destchunk[1] = (size_t)((ptr[1]-(size_t)ptr)+((size_t)destchunk)+capacityChange); //Updated position = segment offset of data segment+destination address+(number of new elements in list)
  
  
  for(size_t i = 0;i<destchunk[2];i++) {
    void** d = (void**)destchunk[i+3];
    *d = (void*)destchunk[1]; //Update all references to point to this new memory address.
  }
  //Update segment pointer in dest
  size_t* dataptr = (size_t*)destchunk[1];
  *(dataptr-1) = (size_t)destchunk; //TODO: Does this work?
  
}


class GCGeneration {
public:
  PODSet<size_t> roots;
  
  unsigned char* memory_unaligned; //Unaligned memory chunk.
  unsigned char* memory; //Memory chunk aligned to machine-specific word size
  size_t marker; //Current marker (in bytes) into free-space
  size_t genSz; //The size of this generation
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
  void Collect() {
    auto Mark = [](size_t* ptr) {
      size_t mtaptr = ((size_t)(MEM_GetDataPointer(ptr)+MEM_DataSize(ptr)))-sizeof(ObjectMetadata); //Memory address of metadata pointer
      //Assume ObjectMetadata is aligned on a word-sized boundary. If we are wrong; we may get an alignment fault.
      ObjectMetadata* metadata = (ObjectMetadata*)mtaptr;
      metadata->allocmark = true;
      size_t* ptrlist = (size_t*)(mtaptr-(metadata->pointerCount*sizeof(size_t)));
      for(size_t i = 0;i<metadata->pointerCount;i++) {
	Mark(ptr); //Mark all descendents
      }
      
    };
   for(size_t i = 0;i<roots.array.count;i++) {
     Mark((size_t*)roots.array[i]);
   }
   
   //Sweep phase
   //Scan for free space extents, and de-allocate all at once
   size_t current = memory;
   bool inExtent = false;
   size_t extentStart;
   while(current<memory+marker) {
     //Scan heap
     size_t* ptr = (size_t*)current;
     ObjectMetadata* metadata = ((size_t)(MEM_GetDataPointer(ptr)+MEM_DataSize(ptr)))-sizeof(ObjectMetadata);
     if(metadata->allocmark) {
       if(inExtent) { //We've reached the end of an extent. Move everything in the extent over to the left.
	 //TODO: Free this extent
	 inExtent = false;
	 //Shift everything over
	 for(size_t i = 0;(i+current)<(memory+marker);i+=(size_t*)(i+current)[0]) {
	   MEM_MovePtr((void*)(extentStart+i),i+current);  
	 }
	 //Freed bytes == current address - start of extent
	 marker-=(current-extentStart);
	 current = extentStart; //Go back and check the blocks we just moved.
	 
       }else {
	 metadata->allocmark = false;
	 current+=ptr[0];
       }
     }else {
       if(!inExtent) {
	 inExtent = true;
	 extentStart = current;
       } 
       current+=ptr[0];
     }
   }
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
      //Ensure enough memory for footer after object
      sz+=sizeof(ObjectMetadata);
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
    
    //Initialize object metadata
	ObjectMetadata meta;
	memset(&meta,0,sizeof(meta));
	memcpy(((unsigned char*)*output)+sz,&meta,sizeof(meta));
	
    
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