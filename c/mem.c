/* mem.c : memory manager
 */

#include <xeroskernel.h>
#include <i386.h>
#include <stdarg.h>

extern  long  freemem;

/* Your code goes here */
memHeader *freeList = NULL;

void kmeminit(void) {
  // Get the first aligned address after freemem
  long alignedStart = (freemem/16 + (freemem%16?1:0)) * 16;

  // Initialize first node to be everything before the hole
  freeList = (memHeader*) alignedStart;
  freeList->size = HOLESTART - sizeof(memHeader) - (int) alignedStart;
  freeList->prev = NULL;
  freeList->next = (memHeader*) HOLEEND;
  freeList->sanityCheck = NULL;

  // The second node is everything after the hole
  memHeader *afterHole = (memHeader*) HOLEEND;
  afterHole->size = FREEMEM_END - HOLEEND - sizeof(memHeader);
  afterHole->prev = freeList;
  afterHole->next = NULL;
  afterHole->sanityCheck = NULL;  
}


void *kmalloc(int size) {
  memHeader *node;
  memHeader *nextNode= NULL;
  int rmdSize;

  // return immediately for some case
  if (size <= 0) {
    return NULL;
  }

  int amount = ((size/16) + (size%16?1:0)) * 16;
  // kprintf("aligned amount 0x%x\n", amount);

  node = freeList;
  while (node != NULL && node->size < amount) {
    node = node->next;
  }

  // Return if all free nodes are smaller than requested size
  if (node == NULL) {
    return NULL;
  }

  // Check if the node size is greater than requested size + memHeader size
  rmdSize = node->size - amount - sizeof(memHeader);
  if (rmdSize > 0) {
    // Split the free node and initialize a new free node
    // dprintf("nextNode start at 0x%x\n", node->dataStart + amount);
    nextNode = (memHeader*) (node->dataStart + amount);
    nextNode->size = rmdSize;
    nextNode->next = node->next;
    if (node->next) {
      node->next->prev = nextNode;
    }
  } else {
    // Node not big enough to be splitted
    nextNode = node->next;
  }

  // Adjust free memory list
  if (nextNode) {
    nextNode->prev = node->prev;
  }
  if (node->prev) {
    node->prev->next = nextNode;
  } else {
    freeList = nextNode;
  }
 
  // Initialize memory header 
  node->size = amount;
  node->next = NULL;
  node->prev = NULL;
  node->sanityCheck = (char*) node->dataStart;
  return node->dataStart;
}

void kfree(void *ptr) {
  memHeader *hdr, *freeNode;
  hdr = (memHeader*) (ptr - sizeof(memHeader));

  // Validate sanity check
  if (hdr->sanityCheck == (char*) hdr->dataStart) {
    hdr->next = NULL;
    hdr->prev = NULL; 
    hdr->sanityCheck = NULL;
    if (freeList == NULL) {
      // Free list is empty, make free list -> node
      freeList = hdr;
    } else if (hdr < freeList) {
      // First node in free list has higher address then ptr,
      // prepend node to free list
      freeList->prev = hdr;
      hdr->next = freeList;
      freeList = hdr;
    } else {
      // Try to insert node back while keeping list in order of address
      freeNode = freeList;
      while (freeNode->next != NULL && hdr > freeNode->next) {
        freeNode = freeNode->next;
      }
      if (freeNode->next != NULL) {
        hdr->next = freeNode->next;
        freeNode->next->prev = hdr;
      }
      freeNode->next = hdr;
      hdr->prev = freeNode;
    }
    // Try to coalesce with a free node at lower address
    if (hdr->prev && (hdr->prev->dataStart + hdr->prev->size) == (unsigned char*) hdr ) {
      hdr->prev->size += hdr->size + sizeof(memHeader);
      hdr->prev->next = hdr->next;
      if (hdr->next) {
        hdr->next->prev = hdr->prev;
      }
      hdr = hdr->prev;
    }
    // Try to coalesce with a free node at higher address
    if (hdr->next && (hdr->dataStart + hdr->size) == (unsigned char*) (hdr->next)) {
      hdr->size += hdr->next->size + sizeof(memHeader);
      hdr->next = hdr->next->next;
      if (hdr->next && hdr->next->next) {
        hdr->next->next->prev = hdr;
      }
    }
  }
}
