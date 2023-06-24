#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "u_list.h"

/* Lab2  ( Lab4: simple allocator ) */ 
void* s_allocator(unsigned int size);
void  s_free(void* ptr);

/* Lab4 */
#define BUDDY_MEMORY_BASE       0x0     // (basic 1: 0x10000000 - 0x20000000) -> 0x0 for Advanced #3 for all memory region
#define BUDDY_MEMORY_PAGE_COUNT 0x3C000 // let BUDDY_MEMORY use 0x0 ~ 0x3C000000 (SPEC) (count: all_region_size/pagesize)
#define PAGESIZE    0x1000     // 4KB
#define MAX_PAGES   0x10000    // 65536 (Entries), (MAX_PAGES = 0x10000000 / PAGESIZE)

typedef enum { //Set the buddy system frame page size from 4KB ~ 256KB (7 order)
    FRAME_FREE = -2,
    FRAME_ALLOCATED,
    FRAME_IDX_0 = 0,      //  0x1000(4KB * 2^0)
    FRAME_IDX_1,          //  0x2000(4KB * 2^1)
    FRAME_IDX_2,          //  0x4000(4KB * 2^2)
    FRAME_IDX_3,          //  0x8000(4KB * 2^3)
    FRAME_IDX_4,          // 0x10000(4KB * 2^4)
    FRAME_IDX_5,          // 0x20000(4KB * 2^5)
    FRAME_IDX_FINAL = 6,  // 0x40000*4KB * 2^6) 
    FRAME_MAX_IDX = 7
} frame_value_type;

typedef enum { //create several memory pools for some common size.(from 32 ~ 2048 bytes) 
    CACHE_NONE = -1,     // Cache not used
    CACHE_IDX_0 = 0,     //  0x20
    CACHE_IDX_1,         //  0x40
    CACHE_IDX_2,         //  0x80
    CACHE_IDX_3,         // 0x100
    CACHE_IDX_4,         // 0x200
    CACHE_IDX_5,         // 0x400
    CACHE_IDX_FINAL = 6, // 0x800
    CACHE_MAX_IDX = 7
} cache_value_type;

typedef struct frame
{
    struct list_head listhead; // store freelist
    int val;                   // store order
    int used;
    int cache_order;
    unsigned int idx;
} frame_t;

void     init_allocator();
frame_t *release_redundant(frame_t *frame);
frame_t *get_buddy(frame_t *frame);
int      coalesce(frame_t *frame_ptr);

void dump_page_info();
void dump_cache_info();

//buddy system
void* page_malloc(unsigned int size);
void  page_free(void *ptr);
void  page2caches(int order);
void* cache_malloc(unsigned int size);
void  cache_free(void* ptr);

void* kmalloc(unsigned int size);
void  kfree(void *ptr);
void  memory_reserve(unsigned long long start, unsigned long long end);

#endif /* _MEMORY_H_ */
