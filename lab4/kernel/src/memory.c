#include "memory.h"
#include "u_list.h"
#include "uart1.h"
#include "exception.h"
#include "dtb.h"

extern char  _heap_top;
static char* htop_ptr = &_heap_top;

extern char  _start;
extern char  _end;
extern char  _stack_top;
extern char* CPIO_DEFAULT_START;
extern char* CPIO_DEFAULT_END;
extern char* dtb_ptr;

/* from lab2: rename malloc to s_allocator */
void* s_allocator(unsigned int size) {

    // keep 0x10 for heap_block header
    char* r = htop_ptr + 0x10;
    // size paddling to multiple of 0x10
    size = size - size % 0x10 + 0x10; //calculate number of heap blocks (無條件進位)
    
    *(unsigned int*)(r - 0x8) = size; //put size in header.
    htop_ptr += size;
    return r; //base address to use. 
}


void s_free(void* ptr) {
    // TBD
}


/* lab4 */
static frame_t*           frame_array;                    // store memory's statement(freelist, order)and page's corresponding index
static list_head_t        frame_freelist[FRAME_MAX_IDX];  // store available block for each page  (7)
static list_head_t        cache_list[CACHE_MAX_IDX];      // store available block for each cache (7)


void init_allocator()
{
    frame_array = s_allocator(BUDDY_MEMORY_PAGE_COUNT * sizeof(frame_t)); 

    // 1. init frame_array (each to be max order: 256KB)
    for (int i = 0; i < BUDDY_MEMORY_PAGE_COUNT; i++)
    {
        if (i % (1 << FRAME_IDX_FINAL) == 0) 
        {
            frame_array[i].val = FRAME_IDX_FINAL; //store order
            frame_array[i].used = FRAME_FREE; // marked as unused
        }
    }

    // 2. init frame freelist for each order. (7 frame freelists.)
    for (int i = FRAME_IDX_0; i <= FRAME_IDX_FINAL; i++)
    {
        INIT_LIST_HEAD(&frame_freelist[i]);
    }

    // 3. init cache list for each cache size pool. (7 freelists.)
    for (int i = CACHE_IDX_0; i<= CACHE_IDX_FINAL; i++)
    {
        INIT_LIST_HEAD(&cache_list[i]);
    }
    // 4. init listhead for each frame_array[i] & add [init frame] to freelist.
    for (int i = 0; i < BUDDY_MEMORY_PAGE_COUNT; i++)
    {
        INIT_LIST_HEAD(&frame_array[i].listhead); 
        frame_array[i].idx = i;
        frame_array[i].cache_order = CACHE_NONE; //marked cache as unused.

        // add [init frame] into freelist
        if (i % (1 << FRAME_IDX_FINAL) == 0) 
        {
            list_add(&frame_array[i].listhead, &frame_freelist[FRAME_IDX_FINAL]);
        }
    }

    /* advance 3: startup allocation.
    Startup reserving the following region:
    1. Spin tables for multicore boot (0x0000 - 0x1000)
    2. Devicetree (Optional, if you have implement it)
    3. Kernel image in the physical memory
    4. Your simple allocator (startup allocator) (Stack + Heap)
    5. Initramfs
    */
    uart_sendline("\r\n* Advance 3: Startup Allocation *\r\n Before reserve memory: \n");
    dump_page_info();
    uart_sendline("========start to reserve memory==========\n");
    dtb_find_and_store_reserved_memory(); // 1&2 find reserve memory in dtb
    //uart_sendline("buddy system: usable memory region: 0x%x ~ 0x%x\n", BUDDY_MEMORY_BASE, BUDDY_MEMORY_BASE + BUDDY_MEMORY_PAGE_COUNT * PAGESIZE);
    uart_sendline("[Memory reserved] for kernel.\n");
    memory_reserve((unsigned long long)&_start, (unsigned long long)&_end); // kernel
    uart_sendline("[Memory reserved] for simple allocator.\n");
    memory_reserve((unsigned long long)&_heap_top, (unsigned long long)&_stack_top);  // heap & stack -> simple allocator
    uart_sendline("[Memory reserved] for initramfs. \n");
    memory_reserve((unsigned long long)CPIO_DEFAULT_START, (unsigned long long)CPIO_DEFAULT_END); //initramfs
}

/* buddy system:
page_malloc
page_free
 */
void* page_malloc(unsigned int size)
{
    uart_sendline("    [+] Allocate page - size : %d(0x%x)\r\n", size, size);
    uart_sendline("        Before\r\n");
    dump_page_info();

    int val;
    /* 1. find the nearlist order of frame */
    for (int i = FRAME_IDX_0; i <= FRAME_IDX_FINAL; i++)
    {
        if (size <= (PAGESIZE << i)) //(pagesize << i): 4kb * 2^val 
        {
            val = i; //store order 
            uart_sendline("        block size = 0x%x\n", PAGESIZE << i);
            break;
        }

        if ( i == FRAME_IDX_FINAL)
        {
            uart_puts("[!] request size exceeded for page_malloc!!!!\r\n");
            return (void*)0;
        }
    }

    // 2. find the smallest larger frame in freelist
    int target_val;
    for (target_val = val; target_val <= FRAME_IDX_FINAL; target_val++)
    {
        // if freelist does not have target order frame, going for next order
        if (!list_empty(&frame_freelist[target_val])) //if not empty: means "have" free frame.
            break;
    }
    if (target_val > FRAME_IDX_FINAL)
    {
        uart_puts("[!] No available frame in freelist, page_malloc ERROR!!!!\r\n");
        return (void*)0;
    }

    // 3. get the first available frame from freelist
    frame_t *target_frame_ptr = (frame_t*)frame_freelist[target_val].next;
    list_del_entry((struct list_head *)target_frame_ptr);

    // 4. release redundant memory block to separate into pieces (ex:target:3 use:4 -> split 4'th to 2 pices of 3'th)
    for (int j = target_val; j > val; j--)
    {
        release_redundant(target_frame_ptr);
    }

    // 5. Allocate it
    target_frame_ptr->used = FRAME_ALLOCATED;
    uart_sendline("        physical address : 0x%x\n", BUDDY_MEMORY_BASE + (PAGESIZE*(target_frame_ptr->idx)));
    uart_sendline("        After\r\n");
    dump_page_info();

    return (void *) BUDDY_MEMORY_BASE + (PAGESIZE * (target_frame_ptr->idx)); //return page address
}


void page_free(void* ptr)
{
    frame_t *target_frame_ptr = &frame_array[((unsigned long long)ptr - BUDDY_MEMORY_BASE) >> 12]; // (ptr-buddy_memory_base)/(pagesize 4kb) = offset / pagesize = frame idx.
    uart_sendline("    [+] Free page: 0x%x, val = %d\r\n",ptr, target_frame_ptr->val);
    uart_sendline("        Before\r\n");
    dump_page_info();
    target_frame_ptr->used = FRAME_FREE; // marked as unused.
    while(coalesce(target_frame_ptr)==0); // merge buddy iteratively.
    list_add(&target_frame_ptr->listhead, &frame_freelist[target_frame_ptr->val]);
    uart_sendline("        After\r\n");
    dump_page_info();
}


/* for buddy system implement function */
frame_t* release_redundant(frame_t *frame)
{
    // (order -1) -> add its buddy to free list (frame itself will be used in master function)
    frame->val -= 1;
    frame_t *buddyptr = get_buddy(frame);
    buddyptr->val = frame->val;
    list_add(&buddyptr->listhead, &frame_freelist[buddyptr->val]); //add buddy to freelist in the new order.
    return frame;
}


frame_t* get_buddy(frame_t *frame)
{
    // XOR(idx, order) to find the buddy's idx. ex:idx=1, order=3 => buddy idx = xor(0001,1000) =9
    return &frame_array[frame->idx ^ (1 << frame->val)];
}


int coalesce(frame_t *frame_ptr)
{
    frame_t *buddy = get_buddy(frame_ptr);
    // situation1: frame order is max
    if (frame_ptr->val == FRAME_IDX_FINAL)
        return -1;

    // situation2: buddy's order must be the same: 2**i + 2**i = 2**(i+1)
    if (frame_ptr->val != buddy->val)
        return -1;

    // situation3: buddy is in used
    if (buddy->used == FRAME_ALLOCATED)
        return -1;

    //coalesce to a bigger order frame.
    //delete buddy from freelist
    list_del_entry((struct list_head *)buddy);
    frame_ptr->val += 1; 
    uart_sendline("    coalesce detected, merging 0x%x, 0x%x, -> val = %d\r\n", frame_ptr->idx, buddy->idx, frame_ptr->val);
    return 0;
}

void dump_page_info(){
    unsigned int exp2 = 1;
    uart_sendline("        ----------------- [  Number of Available Page Blocks  ] -----------------\r\n        | ");
    for (int i = FRAME_IDX_0; i <= FRAME_IDX_FINAL; i++)
    {
        uart_sendline("%4dKB(%1d) ", 4*exp2, i);
        exp2 *= 2;
    }
    uart_sendline("|\r\n        | ");
    for (int i = FRAME_IDX_0; i <= FRAME_IDX_FINAL; i++)
        uart_sendline("     %4d ", list_size(&frame_freelist[i]));
    uart_sendline("|\r\n");
}

void dump_cache_info()
{
    unsigned int exp2 = 1;
    uart_sendline("    -- [  Number of Available Cache Blocks ] --\r\n    | ");
    for (int i = CACHE_IDX_0; i <= CACHE_IDX_FINAL; i++)
    {
        uart_sendline("%4dB(%1d) ", 32*exp2, i);
        exp2 *= 2;
    }
    uart_sendline("|\r\n    | ");
    for (int i = CACHE_IDX_0; i <= CACHE_IDX_FINAL; i++)
        uart_sendline("   %5d ", list_size(&cache_list[i]));
    uart_sendline("|\r\n");
}


void page2caches(int order)
{
    // make caches from a smallest-size page
    char *page = page_malloc(PAGESIZE);
    frame_t *pageframe_ptr = &frame_array[((unsigned long long)page - BUDDY_MEMORY_BASE) >> 12]; //from page to frame
    pageframe_ptr->cache_order = order; //marked frame is using cache

    // split page into a lot of caches and push them into cache_list (with same order.)
    int cachesize = (32 << order);
    for (int i = 0; i < PAGESIZE; i += cachesize)
    {
        list_head_t *c = (list_head_t *)(page + i);
        list_add(c, &cache_list[order]);
    }
}

/* cache */
void* cache_malloc(unsigned int size)
{
    uart_sendline("[+] Allocate cache - size : %d(0x%x)\r\n", size, size);
    uart_sendline("    Before\r\n");
    dump_cache_info();

    // turn size into cache order: 32B * 2**order
    int order;
    for (int i = CACHE_IDX_0; i <= CACHE_IDX_FINAL; i++)
    {
        if (size <= (32 << i)) { order = i; break; }
    }

    // if no available cache in list, assign one samllest page(4kb) for it
    if (list_empty(&cache_list[order]))
    {
        page2caches(order);
    }

    list_head_t* r = cache_list[order].next; //get first free cache.
    list_del_entry(r);
    uart_sendline("    physical address : 0x%x\n", r);
    uart_sendline("    After\r\n");
    dump_cache_info();
    return r;
}

void cache_free(void *ptr)
{
    list_head_t *c = (list_head_t *)ptr;
    frame_t *pageframe_ptr = &frame_array[((unsigned long long)ptr - BUDDY_MEMORY_BASE) >> 12];
    uart_sendline("[+] Free cache: 0x%x, val = %d\r\n",ptr, pageframe_ptr->cache_order);
    uart_sendline("    Before\r\n");
    dump_cache_info();
    list_add(c, &cache_list[pageframe_ptr->cache_order]); //add to free cache list.
    uart_sendline("    After\r\n");
    dump_cache_info();
}

/* dynamic allocator (basic 2)
kmalloc()
    page_malloc()
    cache_malloc()
kfree()
    page_free()
    cache_free()
*/
void *kmalloc(unsigned int size)
{
    uart_sendline("\n\n");
    uart_sendline("================================\r\n");
    uart_sendline("[+] Request kmalloc size: %d\r\n", size);
    uart_sendline("================================\r\n");
    // if size is larger than cache size, go for page
    if (size > (32 << CACHE_IDX_FINAL)) //(32<<cache_idx_final): 32* 2^ 7 =2048 bytes.
    {
        void *r = page_malloc(size);
        return r;
    }
    // go for cache
    void *r = cache_malloc(size);
    return r;
}

void kfree(void *ptr)
{
    uart_sendline("\n\n");
    uart_sendline("==========================\r\n");
    uart_sendline("[-] Request kfree 0x%x\r\n", ptr);
    uart_sendline("==========================\r\n");
    // If no cache assigned, go for page
    if ((unsigned long long)ptr % PAGESIZE == 0 && frame_array[((unsigned long long)ptr - BUDDY_MEMORY_BASE) >> 12].cache_order == CACHE_NONE)
    {
        page_free(ptr);
        return;
    }
    // go for cache
    cache_free(ptr);
}


/*advance 2 */
void memory_reserve(unsigned long long start, unsigned long long end)
{
    /* start and end alignment */
    start -= start % PAGESIZE; // floor (align 0x1000)
    end = end % PAGESIZE ? end + PAGESIZE - (end % PAGESIZE) : end; // ceiling (align 0x1000)

    uart_sendline("Reserved Memory: ");
    uart_sendline("start 0x%x ~ ", start);
    uart_sendline("end 0x%x\r\n",end);

    // delete page from free list
    //traverse all frame_freelist
    for (int order = FRAME_IDX_FINAL; order >= 0; order--)
    {
        list_head_t *pos;
        list_for_each(pos, &frame_freelist[order])
        {
            unsigned long long pagestart = ((frame_t *)pos)->idx * PAGESIZE + BUDDY_MEMORY_BASE;
            unsigned long long pageend = pagestart + (PAGESIZE << order);

            //situation1: if page all in reserved memory -> delete it from freelist
            if (start <= pagestart && end >= pageend)
            {
                ((frame_t *)pos)->used = FRAME_ALLOCATED;
                uart_sendline("    [!] Reserved page in 0x%x - 0x%x\n", pagestart, pageend);
                uart_sendline("        [Before]\n");
                dump_page_info();
                list_del_entry(pos);
                uart_sendline("        Remove usable block for reserved memory: order %d\r\n", order);
                uart_sendline("        [After]\n");
                dump_page_info();
            }
            //situation2: no intersection
            else if (start >= pageend || end <= pagestart)
            {
                continue;
            }
            //situation3: partial intersection, separate the page into smaller size.
            else
            {
                list_del_entry(pos);
                list_head_t *temppos = pos -> prev;
                //split it to smaller order and add it to the next small order.
                list_add(&release_redundant((frame_t *)pos)->listhead, &frame_freelist[order - 1]);
                pos = temppos;
            }
        }
    }
}
