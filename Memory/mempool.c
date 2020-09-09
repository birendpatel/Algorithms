/*
* Author: Biren Patel
* Description: Memory pool implementation with 8 byte alignment
*/

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

#include "mempool.h"

/*******************************************************************************
* struct: block
* purpose: doubly linked list node hidden before requested memory blocks
* @ size : the byte-size of the memory block handed back to the user
* @ available : flag where 1 indiciates that the block is free
* note: in total, if a user requests an x-byte block, x + 32 bytes are reserved
*******************************************************************************/

struct block
{
    struct block *prev;
    struct block *next;
    size_t size;
    char available;
    char reserved[7];
};

/*******************************************************************************
* struct: manager
* purpose: doubly linked list and memory pool manager
* @ top : points to first open and aligned byte after the last occupied block
* @ available : total bytes available outside of blocks (from *top onward)
*******************************************************************************/

static struct
{
    struct block *head;
    struct block *tail;
    size_t available;
    void *pool;
    void *top;
}
manager =
{
    .head = NULL,
    .tail = NULL,
    .available = 0,
    .pool = NULL,
    .top = NULL
};

/*******************************************************************************
* prototypes and general macros
* @ ALIGNMENT : this macro cannot be modified
* @ ALIGN_MASK : used for alignment modulus masking
* @ ROUND_TO_ALIGN : round value upward to the next multiple of the alignment
* @ SIZEOF_BLOCK : sizeof(struct block) assuming 64-bit
* @ MEMPOOL_FORWARD_MERGE : join a free block with the next node if also free
* @ MEMPOOL_BACKWARD_MERGE : join a free block with the prev node if also free
* @ MIN_SPLIT: minimum byte threshold required to call split_block()
* @ CONTAINER_OF : access block node via pointer to first byte of user memory
*******************************************************************************/

#define ALIGNMENT 0x8
#define ALIGN_MASK (ALIGNMENT - 0x1)
#define ROUND_TO_ALIGN(value) (value += ((ALIGNMENT - (value & ALIGN_MASK)) & ALIGN_MASK))
        
#define SIZEOF_BLOCK 32
#define MIN_SPLIT 40

#define CONTAINER_OF(ptr) ((struct block*) ((char*) ptr - SIZEOF_BLOCK))

static void insert_node_at_tail(struct block *new);
static void split_block(struct block *block, size_t size);

/*******************************************************************************
* function: mempool_init
* purpose: heap-allocated initialization of memory pool
* @ size : total byte size of pool
* returns: true on successful initialization
*******************************************************************************/

bool mempool_init(size_t size)
{
    if (size == 0) return false;
    if (manager.pool != NULL) return false;
    
    manager.pool = malloc(size);
    if (manager.pool == NULL) return false;
    
    assert((uintptr_t) manager.top % ALIGNMENT == 0 && "top unaligned");

    manager.top = manager.pool;
    manager.available = size;
    return true;
}


/*******************************************************************************
* function: mempool_free
* purpose: release the memory pool and reset the pool manager
*******************************************************************************/

void mempool_free(void)
{
    if (manager.pool == NULL) return;

    free(manager.pool);

    manager.head = NULL;
    manager.tail = NULL;
    manager.available = 0;
    manager.pool = NULL;
    manager.top = NULL;
}

/*******************************************************************************
* function: pmalloc
* purpose: return an available memory block from the pool
* @ size : total bytes requested
* returns: void pointer, null if pmalloc cannot find a suitable memory block
*******************************************************************************/

void *pmalloc(size_t size)
{
    if (size == 0) return NULL;
    
    ROUND_TO_ALIGN(size);

    //try to obtain memory from pool top, else repurpose an available block
    if (size + SIZEOF_BLOCK <= manager.available)
    {
        //place a new block node at top and add to tail of linked list
        struct block *new = manager.top;

        new->size = size;
        new->available = false;
        insert_node_at_tail(new);

        //update manager, move top a la sbrk() and note total bytes used
        manager.available -= SIZEOF_BLOCK + size;
        manager.top = (char*) manager.top + (SIZEOF_BLOCK + size);

        //hide the metadata and return 32 bytes above for user to use
        assert((uintptr_t) new % ALIGNMENT == 0 && "block not aligned");
        assert((uintptr_t) (new + 1) % ALIGNMENT == 0 && "user not aligned");

        return new + 1;
    }
    else
    {
        struct block *block = manager.head;

        while (block != NULL)
        {
            if (block->available && block->size >= size)
            {
                //split block into two if it is large enough
                if (block->size - size >= MIN_SPLIT) split_block(block, size);

                block->available = false;

                return block + 1;
            }
            else block = block->next;
        }

        assert(block == NULL && "incomplete list walk");
    }

    //not enough free memory in pool to fulfill the user request
    return NULL;
}

/*******************************************************************************
* function: pcalloc
* purpose: return new memory block with contents initialized to zero
* @ n : number of array elements requested
* @ size : byte-size of each element
* returns: void pointer, null if pcalloc cannot find a suitable memory block
*******************************************************************************/

void *pcalloc(size_t n, size_t size)
{
    //todo: check for overflow before passing bytes var and return error
    size_t bytes = size * n;

    void *address = pmalloc(bytes);

    if (address == NULL)
    {
        return NULL;
    }
    else
    {
        //ignore extra bytes that pmalloc may allocate to maintain alignment
        memset(address, 0, bytes);
        return address;
    }
}

/*******************************************************************************
* function: prealloc
* purpose: change the size of the memory block pointed to by ptr to size bytes
* @ ptr : first byte of a memory block previously passed to user via pmalloc
*******************************************************************************/

void *prealloc(void *ptr, size_t size)
{
    //prealloc reduces to pmalloc or pfree on degenerate arguments
    if (ptr == NULL) return pmalloc(size);
    
    if (ptr != NULL && size == 0) 
    {
        pfree(ptr);
        return ptr;
    }
    
    ROUND_TO_ALIGN(size);
    
    struct block *block = CONTAINER_OF(ptr);
    
    if (block->size == size) 
    {
        //new request still fits the alignment padding so do nothing
        return ptr;
    }
    else if (block->size > size)
    {
        //user requests less memory, split the block if possible else do nothing
        if (block->size - size >= MIN_SPLIT) split_block(block, size);
        
        return block + 1;
    }
    else
    {
        //request for more memory, first allocate a new block
        void *new = pmalloc(size);
        if (new == NULL) return NULL;
        
        //then copy contents to new block
        memcpy(new, ptr, block->size);
        
        //finally free old block
        pfree(ptr);
        
        return new;
    }
}

/*******************************************************************************
* function: pfree
* purpose: return memory block to pool for reuse
* @ ptr : first byte of a memory block previously passed to user via pmalloc
*******************************************************************************/

void pfree(void *ptr)
{
    if (ptr == NULL) return;

    struct block *block = CONTAINER_OF(ptr);

    //mark the block for reuse
    block->available = true;

    //merge next block if it is available
    if (block->next != NULL && block->next->available == true)
    {
        //goto from backward merge
        forward_merge: ;

        struct block *next_block = block->next;

        //swallow bytes occupied by next block and its user data
        block->size += next_block->size + SIZEOF_BLOCK;

        //connect our block and one over, or make it as the new tail
        if (next_block->next != NULL)
        {
            block->next = next_block->next;
            next_block->next->prev = block;
        }
        else
        {
            block->next = NULL;
            manager.tail = block;
        }
    }
    
    //merge with previous block if it is available, this block will disappear
    if (block->prev != NULL && block->prev->available == true)
    {
        //backward merge reduces to a forward merge on the previous block
        block = block->prev;
        goto forward_merge;
    }
}


/*******************************************************************************
* function: insert_node_at_tail
* purpose: newly-created nodes via pmalloc are inserted at tail of block manager
*******************************************************************************/

static void insert_node_at_tail(struct block *new)
{
    if (manager.head == NULL)
    {
        //insert into empty list
        manager.head = new;
        manager.tail = new;
        new->prev = NULL;
        new->next = NULL;
    }
    else
    {
        //insert at tail
        new->prev = manager.tail;
        new->next = NULL;
        manager.tail->next = new;
        manager.tail = new;
    }
}

/*******************************************************************************
* function: split_block
* purpose: split an existing block into two new neighbor blocks
*******************************************************************************/

static void split_block(struct block *block, size_t size)
{
    uintptr_t delta = (uintptr_t) block + SIZEOF_BLOCK + size;
    struct block  *new = (struct block *) delta;
    
    new->prev = block;
    new->next = block->next;
    new->size = block->size - size - SIZEOF_BLOCK;
    new->available = true;
    
    if (block->next != NULL) block->next->prev = new;
    else manager.tail = new;
    
    block->next = new;
    assert(size == block->size - SIZEOF_BLOCK - new->size && "block mismatch");
    block->size = size;
    
    return;
}

/*******************************************************************************
* function: memmap
* purpose: display the memory contents of the pool to stdout
* @ words : number of words (x64) to display from pool head onwards
*******************************************************************************/

#define memmap_manager()                                                       \
        do                                                                     \
        {                                                                      \
            printf("\nHead: 0x%p\n", (void*) manager.head);                    \
            printf("Tail: 0x%p\n", (void*) manager.tail);                      \
            printf("Pool: 0x%p\n", (void*) manager.pool);                      \
            printf("Top:  0x%p\n", (void*) manager.top);                       \
            printf("Available: %llu\n", manager.available);                    \
        }                                                                      \
        while (0)                                                              \


#define memmap_header()                                                        \
        do                                                                     \
        {                                                                      \
            printf("\n");                                                      \
            printf("%5sAddress%13sStorage%17sValue\n", " ", " ", " ");         \
            printf("------------------\t---------\t----------------------\n"); \
        } while(0)                                                             \


//an address in the memory pool can have 6 different interpretations,
//4 of which relate to the struct block members
#define BLOCK_PREV_FMT "0x%p      [B] prev        0x%p       \n"
#define BLOCK_NEXT_FMT "0x%p      [B] next        0x%p       \n"
#define BLOCK_SIZE_FMT "0x%p      [B] size        %llu       \n"
#define BLOCK_FLAG_FMT "0x%p      [B] flag        %d         \n"
#define BLOCK_USER_FMT "0x%p      [U]             "
#define BLOCK_NONE_FMT "0x%p      [N]                        \n"

void memmap(size_t words)
{
    //display memory map header
    memmap_manager();
    memmap_header();

    uintptr_t curr = (uintptr_t) manager.pool;
    uintptr_t end = (uintptr_t) manager.pool + (words - 1) * 8;

    struct block *block = manager.head;

    while (curr <= end)
    {
        if (curr == (uintptr_t) block)
        {
            //curr is at block node so an unrolled loop prints next 4 words
            printf(BLOCK_PREV_FMT, (void*) curr, (void*) block->prev);
            curr += 8;

            printf(BLOCK_NEXT_FMT, (void*) curr, (void*) block->next);
            curr += 8;

            printf(BLOCK_SIZE_FMT, (void*) curr, block->size);
            curr += 8;

            printf(BLOCK_FLAG_FMT, (void*) curr, block->available);
            curr += 8;

            //and then print the remaining user blocks using char values
            size_t user_words = block->size/8;

            for (size_t i = 0; i < user_words; ++i)
            {
                printf(BLOCK_USER_FMT, (void*) curr);

                char *byte = (char*) curr;

                for (size_t j = 0; j < 8; ++j, ++byte)
                {
                    if (*byte != '\0')
                    {
                        if (*byte > 32 && *byte < 127) printf("%-3c", *byte);
                        else printf("?  ");
                    }
                    else printf(".  ");
                }

                printf("\n");

                curr += 8;
            }

            printf("\n");

            block = block->next;
        }
        else
        {
            //this section of memory is not in use
            printf(BLOCK_NONE_FMT, (void*) curr);
            curr += 8;
        }
    }

    return;
}

/******************************************************************************/
//just some basic testing and debugging. todo: write unit tests in Unity

int main(void)
{
    mempool_init(1024);

    char *x = pcalloc(24, 1);
    
    char *y = prealloc(x, 32);
    
    char *z = prealloc(y, 64);
    
    char *w = pcalloc(16, 1);
    
    w[0] = 'Z';
    
    z[63] = 'Z';
    
    memmap(32);

    return 0;
}