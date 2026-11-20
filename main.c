/**
 * Simple-malloc implementation
 * Simple memory allocator with splitting and coalising.
 * The requests are serviced according to the next fit policy.
 * Heavily insipired by OSTEP book.
 *
 * */

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#define BLOCK_HEAP_SIZE 0x1000
#define MAGIC_NUMBER 0x1234

// info about allocated block
struct header_t {
    uint32_t size;
    uint32_t magic;
};

// embedded free single linked list
struct node_t {
    uint32_t size;
    struct node_t *next;
};

static struct node_t *global_ptr = NULL;

void *smalloc(uint32_t size);
void sfree(void *ptr);

int main(void) {
    char *ptr;

    ptr = (char *) smalloc(10);

    return 0;
}

void *smalloc(uint32_t size) {
    struct node_t *curr_ptr, *prev_ptr;
    uint32_t node_t_size = sizeof(struct node_t);
    uint32_t header_t_size = sizeof(struct header_t);

    // first-fit strategy
    prev_ptr = NULL; 
    curr_ptr = global_ptr; 
    while (curr_ptr != NULL) {
        if ((curr_ptr->size + node_t_size) >= (size + header_t_size)) {
            break;
        }
        curr_ptr = curr_ptr->next;
        prev_ptr = curr_ptr;
    }
    
    // Failed to find one, so ask OS for more
    // Although, there is no difference between 1st call to smalloc
    // and once all the free space has been consumed
    // Ask OS to grow heap
    if (curr_ptr == NULL) {

        void *ptr;
        
        ptr = sbrk(BLOCK_HEAP_SIZE);
        
        // OS cannot grow heap
        if (ptr == (void *) -1) {
            errno = ENOMEM;
            return NULL;
        }
        
        if (global_ptr == NULL) {
            // 1st ever smalloc
            // allocate 1st BLOCK_HEAP_SIZE with sbrk
            global_ptr = (struct node_t *) ptr;
            global_ptr->size = BLOCK_HEAP_SIZE - node_t_size;
            global_ptr->next = NULL;
            // update current pointer
            curr_ptr = global_ptr;
        } else {
            // not enough space in current heap
            // so grow heap on addition BLOCK_HEAP_SIZE with sbrk
            struct node_t *heap_chunk = (struct node_t *) ptr;
            heap_chunk->size = BLOCK_HEAP_SIZE - node_t_size;
            heap_chunk->next = NULL;
            // update current and previous ptr-s
            curr_ptr->next = heap_chunk;
            prev_ptr = curr_ptr;
            curr_ptr = curr_ptr->next;
        }

    }

    void *user_ptr;
    // split logic
    if ((curr_ptr->size + node_t_size) > (size + header_t_size)) {
        struct header_t *hdr;
        struct node_t *ptr_next;
        uint32_t ptr_size;
        char *tmp;
        
        // pointer to the memory user asked for
        user_ptr = (void *) ((struct header_t *)curr_ptr + 1);
        // pointer to header user by free
        hdr = (struct header_t *) curr_ptr;
        
        // advance temp pointer to where next free chunk pointed by ptr
        // should be (sizeof header + sizeof user request)
        tmp = (char *) curr_ptr;
        tmp += header_t_size + size;
        
        // save current chunk info
        ptr_next = curr_ptr->next;
        ptr_size = curr_ptr->size;
        // advance ptr
        curr_ptr = (struct node_t *) tmp;
        // compute leftover free space parameters
        curr_ptr->next = ptr_next;
        curr_ptr->size = ptr_size - (header_t_size + size) + node_t_size;

        // update previous node in the free list
        if (prev_ptr != NULL) {
            prev_ptr->next = curr_ptr;  
        } 

        // write header to user's chunk
        hdr->size = size;
        hdr->magic = MAGIC_NUMBER;

    } else {
        // only can accomodate exactly what user has asked for
        
        struct header_t *hdr;
        
        // pointer to the memory user asked for
        user_ptr = (void *) (curr_ptr + 1);
        // pointer to header user by free
        hdr = (struct header_t *) curr_ptr;

        // update previous node in the free list
        if (prev_ptr != NULL) {
            prev_ptr->next = curr_ptr->next;
        }
        // adance current node
        curr_ptr = curr_ptr->next;

        // write header to user's chunk
        hdr->size = size;
        hdr->magic = MAGIC_NUMBER;

    }
    
    return user_ptr;
}

void sfree(void *ptr) {



}





