/*
* Author: Biren Patel
* Description: Implementation for dynamic array abstract data type
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "dynamic_array.h"

/*******************************************************************************
* macro: darray_trace
* purpose: debugging to stdout if macro DYNAMIC_ARRAY_DEBUG set to 1
*******************************************************************************/

#define darray_trace(fmt, ...)                                                 \
        do                                                                     \
        {                                                                      \
            if (DYNAMIC_ARRAY_DEBUG)                                           \
            {                                                                  \
                printf("\n>> %s (%d): " fmt, __func__, __LINE__, __VA_ARGS__); \
            }                                                                  \
        }                                                                      \
        while(0)


/*******************************************************************************
* macro: hsize
* purpose: size of struct darray_header, typically 16 bytes
*******************************************************************************/

#define HSIZE sizeof(struct darray_header)

/*******************************************************************************
* macro: darray_header_var
* purpose: get a pointer to darray_header given a pointer to the data member
* note: offset is typically 16 bytes
*******************************************************************************/

#define DARRAY_HEADER_VAR(d)                                                   \
        ((struct darray_header *)                                              \
        (((char*) (d)) - offsetof(struct darray_header, data)))

/*******************************************************************************
* public functions
*******************************************************************************/

darray darray_create(size_t init_capacity, void (*destroy)(void *ptr))
{
    struct darray_header *dh;
    
    //check conditions
    assert(init_capacity > 0 && "init_capacity is not positive int");

    //allocate memory for header + flexible array member
    dh = malloc(HSIZE + sizeof(array_item) * init_capacity);
    if (dh == NULL) return NULL;
    
    //define remaining header metadata
    dh->destroy = destroy;
    dh->capacity = init_capacity;
    dh->count = 0;

    //client receives the array but everything else remains hidden
    return dh->data;
}

/******************************************************************************/

void darray_destroy(darray d)
{
    //define pointer to array header
    struct darray_header *dh = DARRAY_HEADER_VAR(d);

    assert(dh->data[0] == *d && "input pointer is not constructor pointer");

    (*dh->destroy)(dh);
}

/******************************************************************************/

int darray_count(darray d)
{
    //define pointer to array header
    struct darray_header *dh = DARRAY_HEADER_VAR(d);

    assert(dh->data[0] == *d && "input pointer is not constructor pointer");

    return dh->count;
}


/******************************************************************************/

int darray_append(darray *d, array_item element)
{
    //define pointer to array header
    struct darray_header *dh = DARRAY_HEADER_VAR(*d);

    assert(dh->data[0] == **d && "input pointer is not constructor pointer");

    //no space left in the allocated block to push an element.
    if (dh->count == dh->capacity)
    {
        if (dh->count == UINT32_MAX)
        {
            darray_trace("capacity cannot increase, push impossible%c\n", ' ');
            return 1;
        }
        else
        {
            //determine new array capacity as a function of current capacity
            uint32_t new_capacity = INCREASE_CAPACITY(dh->capacity);
            
            //override growth with a ceiling if new capacity exceeds uint32
            if (new_capacity > UINT32_MAX) new_capacity = UINT32_MAX;

            assert(new_capacity > dh->count && "capacity fx not monotonic");
            darray_trace("increased capacity to %d\n", new_capacity);

            //determine total bytes needed
            size_t new_size = HSIZE + sizeof(array_item) * new_capacity;
            darray_trace("new memory block of %d bytes\n", (int) new_size);

            //reallocate memory and redirect dh pointer
            struct darray_header *tmp = realloc(dh, new_size);
            
            if (tmp == NULL) return 2;
            else dh = tmp;
            
            //realloc success so update dh->capacity
            dh->capacity = new_capacity;
            
            //realloc may have moved dh to new section of memory.
            //if so, update dereferenced input so client has correct address
            *d = dh->data;
        }
    }

    //space available in allocated block, go ahead and push element
    darray_trace("pushing element to dynamic array%c\n", ' ');
    
    dh->data[dh->count++] = element;
    
    assert(dh->count <= dh->capacity && "count exceeds maximum capacity");
    
    return 0;
}

/******************************************************************************/

bool darray_pop(darray d, array_item *popped_item)
{
    darray_trace("pop requested%c\n", ' ');

    //define pointer to array header
    struct darray_header *dh = DARRAY_HEADER_VAR(d);

    assert(dh->data[0] == *d && "input pointer is not constructor pointer");

    if (dh->count == 0)
    {
        darray_trace("nothing to pop%c\n", ' ');
        
        return false;
    }
    else
    {        
        //decrement count for synthetic pop but only return item if requested
        --dh->count;
        
        if (popped_item != NULL) *popped_item = dh->data[dh->count];
        
        assert(dh->count >= 0 && "array count is negative");
        darray_trace("pop successful%c\n", ' ');
        
        return true;
    }
}

/******************************************************************************/

bool darray_popleft(darray d, array_item *popped_item)
{
    darray_trace("popleft requested%c\n", ' ');

    //define pointer to array header
    struct darray_header *dh = DARRAY_HEADER_VAR(d);

    assert(dh->data[0] == *d && "input pointer is not constructor pointer");

    if (dh->count == 0)
    {
        darray_trace("nothing to popleft%c\n", ' ');
        
        return false;
    }
    else
    {
        //copy first item to client storage if requested
        if (popped_item != NULL) *popped_item = dh->data[0];

        //move all the elements in the data array back by one index
        if (--dh->count != 0)
        {
            memmove(dh->data, dh->data + 1, dh->count * sizeof(array_item));
        }

        darray_trace("popleft successful%c\n", ' ');
        
        return true;
    }
}

/******************************************************************************/

bool darray_peek(darray d, array_item *peeked_item)
{
    darray_trace("peek requested%c\n", ' ');

    //define pointer to array header
    struct darray_header *dh = DARRAY_HEADER_VAR(d);

    assert(dh->data[0] == *d && "input pointer is not constructor pointer");

    if (dh->count == 0 || peeked_item == NULL)
    {
        darray_trace("nothing to peek at or peek used improperly%c\n", ' ');

        return false;
    }
    else
    {   
        *peeked_item = dh->data[dh->count - 1];
        
        assert(dh->count >= 0 && "array count is negative");
        darray_trace("peek successful%c\n", ' ');
        
        return true;
    }
}
