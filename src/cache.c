//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include "cache.h"

//
// TODO:Student Information
//
const char *studentName = "Yue Zhuo";
const char *studentID   = "A16110292";
const char *email       = "y1zhuo@ucsd.edu";

//------------------------------------//
//        Cache Configuration         //
//------------------------------------//

uint32_t icacheSets;     // Number of sets in the I$
uint32_t icacheAssoc;    // Associativity of the I$
uint32_t icacheHitTime;  // Hit Time of the I$

uint32_t dcacheSets;     // Number of sets in the D$
uint32_t dcacheAssoc;    // Associativity of the D$
uint32_t dcacheHitTime;  // Hit Time of the D$

uint32_t l2cacheSets;    // Number of sets in the L2$
uint32_t l2cacheAssoc;   // Associativity of the L2$
uint32_t l2cacheHitTime; // Hit Time of the L2$
uint32_t inclusive;      // Indicates if the L2 is inclusive

uint32_t blocksize;      // Block/Line size
uint32_t memspeed;       // Latency of Main Memory

//------------------------------------//
//          Cache Statistics          //
//------------------------------------//

uint64_t icacheRefs;       // I$ references
uint64_t icacheMisses;     // I$ misses
uint64_t icachePenalties;  // I$ penalties

uint64_t dcacheRefs;       // D$ references
uint64_t dcacheMisses;     // D$ misses
uint64_t dcachePenalties;  // D$ penalties

uint64_t l2cacheRefs;      // L2$ references
uint64_t l2cacheMisses;    // L2$ misses
uint64_t l2cachePenalties; // L2$ penalties

//------------------------------------//
//        Cache Data Structures       //
//------------------------------------//
typedef struct Block{
  struct Block *prev, *next;
  uint32_t val;
}Block;

typedef struct Set{
  Block *head, *tail;
  uint32_t size;
}Set;

bool isEmptySet(Set *s){
  return (s->size);
}

Block* initialBlock(uint32_t val){
  Block *b = (Block*)malloc(sizeof(Block));
  b->val = val;
  b->prev = NULL;
  b->next = NULL;
  return b;
}

// append a block into a set, assume s->size >= 0
void appendBlock(Set *s, Block *b){
  if(isEmptySet(s)){
    b->prev = s->tail;
    s->tail->next = b;
    s->tail = b;
  }
  else{
    s->head = b;
    s->tail = b;
  }
  s->size++;
}

// pop the block in front of the set
void popFront(Set *s){
  if(!isEmptySet(s))
    return;
  
  Block *b = s->head;
  s->head = b->next;

  if(s->head)
    s->head->prev = NULL;
  
  s->size--;
  // free(b);
}

// delete the block with index
// and return a pointer to the deleted block
Block* deleteBlock(Set *s, int index){
  if(index >= s->size || index <0)
    return NULL;
  
  Block *b = s->head;
  
  if(s->size == 1){ // delete one block
    s->head = NULL;
    s->tail = NULL;
  }
  else if (index == 0){ // delete the first block
    s->head = b->next;
    s->head->prev = NULL;
  }
  else if (index == s->size -1){ // delete the last block
    b = s->tail;
    s->tail = s->tail->prev;
    s->tail->next = NULL;
  }
  else{ // delete the block in the middle of the set
    for(int i = 0; i < index; i++)
      b = b->next;
    b->prev->next = b->next;
    b->next->prev = b->prev;
  }
  b->next = NULL;
  b->prev = NULL;
  s->size--;
  return b;
}

Set *icache;
Set *dcache;
Set *l2cache;

// variables need to be initialize in init_cache()
uint32_t offsetSize;
uint32_t offsetMask;

uint32_t icacheIndexSize;
uint32_t dcacheIndexSize;
uint32_t l2cacheIndexSize;

uint32_t icacheIndexMask;
uint32_t dcacheIndexMask;
uint32_t l2cacheIndexMask;


//------------------------------------//
//          Cache Functions           //
//------------------------------------//

// Initialize the Cache Hierarchy
//
void
init_cache()
{
  // Initialize cache stats
  icacheRefs        = 0;
  icacheMisses      = 0;
  icachePenalties   = 0;
  dcacheRefs        = 0;
  dcacheMisses      = 0;
  dcachePenalties   = 0;
  l2cacheRefs       = 0;
  l2cacheMisses     = 0;
  l2cachePenalties  = 0;
  
  icache = (Set*)malloc(sizeof(Set) * icacheSets);
  dcache = (Set*)malloc(sizeof(Set) * dcacheSets);
  l2cache = (Set*)malloc(sizeof(Set) * l2cacheSets);

  for(int i = 0; i<icacheSets; i++){
    icache[i].size = 0;
    icache[i].head = NULL;
    icache[i].tail = NULL;
  }

  for(int i=0; i<dcacheSets; i++)
  {
    dcache[i].size = 0;
    dcache[i].head = NULL;
    dcache[i].tail = NULL;
  }

  for(int i=0; i<l2cacheSets; i++)
  {
    l2cache[i].size = 0;
    l2cache[i].head = NULL;
    l2cache[i].tail = NULL;
  }

  offsetSize = (uint32_t)ceil(log2(blocksize));
  // offset_size += ((1<<offset_size)==blocksize)? 0 : 1;
  offsetMask = (1<<offsetSize)-1;
  icacheIndexSize = (uint32_t)ceil(log2(icacheSets));
  dcacheIndexSize = (uint32_t)ceil(log2(dcacheSets));
  l2cacheIndexSize = (uint32_t)ceil(log2(l2cacheSets));

  icacheIndexMask = ((1 << icacheIndexSize) - 1) << offsetSize;
  dcacheIndexMask = ((1 << dcacheIndexSize) - 1) << offsetSize;
  l2cacheIndexMask = ((1 << l2cacheIndexSize) - 1) << offsetSize;

}



// Perform a memory access through the icache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
icache_access(uint32_t addr)
{
  if(icacheSets == 0)
    return l2cache_access(addr);
  
  icacheRefs++;

  uint32_t offset = addr & offsetMask;
  uint32_t index = (addr & icacheIndexMask) >> offsetSize;
  uint32_t tag = addr >> (icacheIndexSize + offsetSize);

  Block *ptr = icache[index].head;

  for(int i=0; i<icache[index].size; i++){
    if(ptr-> val == tag){ // Meet a hit
      Block *hitBlock = deleteBlock(&icache[index], i);  // pop the hit block
      appendBlock(&icache[index], hitBlock);
      return icacheHitTime;
    }
    ptr = ptr->next;
  }

  // Miss
  icacheMisses++;

  uint32_t penalty = l2cache_access(addr);
  icachePenalties += penalty;

  // use LRU algorithm to process miss replacement
  Block *newBlock = initialBlock(tag);

  if(icache[index].size == icacheAssoc) // if set is filled, then replace LRU 
    popFront(&icache[index]);
  appendBlock(&icache[index],newBlock);

  return penalty + icacheHitTime;
}


// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
dcache_access(uint32_t addr)
{
  if(dcacheSets == 0)
    return l2cache_access(addr);
  
  dcacheRefs++;

  uint32_t offset = addr & offsetMask;
  uint32_t index = (addr & dcacheIndexMask) >> offsetSize;
  uint32_t tag = addr >> (dcacheIndexSize + offsetSize);

  Block *ptr = dcache[index].head;

  for(int i=0; i<dcache[index].size; i++){
    if(ptr-> val == tag){ // Meet a hit
      Block *hitBlock = deleteBlock(&dcache[index], i);  // pop the hit block
      appendBlock(&dcache[index], hitBlock);
      return dcacheHitTime;
    }
    ptr = ptr->next;
  }

  // Miss
  dcacheMisses++;

  uint32_t penalty = l2cache_access(addr);
  dcachePenalties += penalty;

  // use LRU algorithm to process miss replacement
  Block *newBlock = initialBlock(tag);

  if(dcache[index].size == dcacheAssoc) // if set is filled, then replace LRU 
    popFront(&dcache[index]);
  appendBlock(&dcache[index],newBlock);

  return penalty + dcacheHitTime;
}

void icacheInvalidation(uint32_t addr){
  uint32_t offset = addr & offsetMask;
  uint32_t index = (addr & icacheIndexMask) >> offsetSize;
  uint32_t tag = addr >> (icacheIndexSize + offsetSize);

  Block *ptr = icache[index].head;

  for(int i=0; i<icache[index].size; i++){
    if(ptr->val == tag){ // search for the tag
      Block *b = deleteBlock(&icache[index], i); //Invalidate it
      // free(b);
      return;
    }
    ptr = ptr->next;
  }
}

void dcacheInvalidation(uint32_t addr){
  uint32_t offset = addr & offsetMask;
  uint32_t index = (addr & dcacheIndexMask) >> offsetSize;
  uint32_t tag = addr >> (dcacheIndexSize + offsetSize);

  Block *p = dcache[index].head;

  for(int i=0; i<dcache[index].size; i++){
    if(p->val == tag){ // Find it
      Block *b = deleteBlock(&dcache[index], i); // Invalidate it
      // free(b);
      return;
    }
    p = p->next;
  }
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
l2cache_access(uint32_t addr)
{
  if(l2cacheSets == 0)
    return memspeed;
  
  l2cacheRefs++;

  uint32_t offset = addr & offsetMask;
  uint32_t index = (addr & l2cacheIndexMask) >> offsetSize;
  uint32_t tag = addr >> (l2cacheIndexSize + offsetSize);

  Block *ptr = l2cache[index].head;

  for(int i=0; i<l2cache[index].size; i++){
    if(ptr-> val == tag){ // Meet a hit
      Block *hitBlock = deleteBlock(&l2cache[index], i);  // pop the hit block
      appendBlock(&l2cache[index], hitBlock);
      return l2cacheHitTime;
    }
    ptr = ptr->next;
  }

  // Miss
  l2cacheMisses += 1;

  Block *newBlock = initialBlock(tag);
  
  if(l2cache[index].size == l2cacheAssoc){
    if(inclusive){  // L1 Invalidation
      uint32_t swapoutBlockAddr = (((l2cache[index].head->val)<<l2cacheIndexSize)+index)<<offsetSize;
      icacheInvalidation(swapoutBlockAddr);
      dcacheInvalidation(swapoutBlockAddr);
    }
    popFront(&l2cache[index]);
  }
  appendBlock(&l2cache[index], newBlock);

  l2cachePenalties += memspeed;
  return memspeed + l2cacheHitTime;
}
