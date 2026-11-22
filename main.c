/**
 *
 * Memory allocator with splitting and coalescing.
 * The requests are serviced according to the next fit policy.
 * Heavily insipired by OSTEP book.
 *
 * */

#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#define BLOCK_HEAP_SIZE 0x1000
#define MAGIC_NUMBER 0x12345678


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

// head of the free list
static struct node_t *global_ptr = NULL;

// min amount of bytes we need to store either node_t or header_t
static const uint32_t hdr_size = (
        sizeof(struct header_t) < sizeof(struct node_t) 
        ? sizeof(struct node_t) 
        : sizeof(struct header_t));


void *ffmalloc(uint32_t size);
void ff_free(void *usr_ptr);

int main(void) {
    char *ptr;
    int i;

    ptr = (char *) ffmalloc(10);
    
    for (i = 0; i < 10; i++) {
        *(ptr + i) = i + 1;
    }

    ff_free(ptr);

    return 0;
}

void *ffmalloc(uint32_t size) {
    struct node_t *curr_ptr, *prev_ptr;

    // first-fit strategy
    prev_ptr = NULL; 
    curr_ptr = global_ptr; 
    while (curr_ptr != NULL) {
        if (curr_ptr->size >= size) {
            break;
        }
        prev_ptr = curr_ptr;
        curr_ptr = curr_ptr->next;
    }
    
    // Failed to find one, so ask OS for more
    // Although, there is no difference between 1st call to ffmalloc
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
            // 1st ever ffmalloc
            // allocate 1st BLOCK_HEAP_SIZE with sbrk
            global_ptr = (struct node_t *) ptr;
            global_ptr->size = BLOCK_HEAP_SIZE - hdr_size;
            global_ptr->next = NULL;
            // update current pointer
            curr_ptr = global_ptr;
        } else {
            // not enough space in current heap
            // so grow heap on addition BLOCK_HEAP_SIZE with sbrk
            struct node_t *heap_chunk = (struct node_t *) ptr;
            heap_chunk->size = BLOCK_HEAP_SIZE - hdr_size;
            heap_chunk->next = NULL;
            // update current and previous ptr-s
            prev_ptr->next = heap_chunk;
            curr_ptr = heap_chunk;
        }

    }

    void *user_ptr;
    // split logic
    if (curr_ptr->size > size) {
        struct header_t *hdr;
        struct node_t *ptr_next;
        uint32_t ptr_size;
        char *tmp;

        // pointer to header user by free
        hdr = (struct header_t *) curr_ptr;
        // pointer to the memory user asked for
        user_ptr = (void *) (hdr + 1);
        
        // advance temp pointer to where next free chunk pointed by ptr
        // should be (sizeof header + sizeof user request)
        tmp = (char *) curr_ptr;
        tmp += hdr_size + size;
        
        // save current chunk info
        ptr_next = curr_ptr->next;
        ptr_size = curr_ptr->size;
        // advance ptr
        curr_ptr = (struct node_t *) tmp;
        // compute leftover free space parameters
        curr_ptr->next = ptr_next;
        curr_ptr->size = ptr_size - size;

        // update previous node in the free list, if any
        if (prev_ptr != NULL) {
            prev_ptr->next = curr_ptr;  
        } else {
            // curr_pointer was global_ptr
            global_ptr = curr_ptr;
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

void ff_free(void *usr_ptr) {
    struct node_t *free_chunk;
    struct header_t *usr_hdr;
    uint32_t freed_size;

    if (usr_ptr == NULL) {
        return;
    }
    
    usr_hdr = ((struct header_t *) usr_ptr - 1);
    
    // check integrity
    assert(usr_hdr->magic == MAGIC_NUMBER);

    freed_size = usr_hdr->size;
    free_chunk = (struct node_t *) usr_hdr; 

    free_chunk->size = freed_size;
    free_chunk->next = NULL;

    // easiest case:
    // we allocated everything,
    // now we are getting back 1st chunk back
    if (global_ptr == NULL) {
        global_ptr = free_chunk;
        return;
    }

    // otherwise,
    // we do
    // 1) insert somewhere in the list
    // 2) coalesce the free list
    
    // We insert free chunk back into list by following next strategy:
    // - we iterator on the list
    // - once we meet the node whose next node 
    //   has the address bigger that ours, we stop
    // - and we insert after the current node
    // It should produce the list with accessending addresses
    // and it should be easy to coalesce:
    // we check the nodes around for continuity of the address space
    // we will have 3 cases to consider:
    // - both nodes + currently inserted node is indeed one continuous space
    // - the node before + inserted node form continuous space
    // - the node after + inserted node form continuous space
    // - none of the form with any other node the continuous space

    struct node_t *prev_ptr, *curr_ptr;
    prev_ptr = NULL;
    curr_ptr = global_ptr;
    while (curr_ptr != NULL) {
        if (free_chunk < curr_ptr) {
            break;
        }
        prev_ptr = curr_ptr;
        curr_ptr = curr_ptr->next;
    }
    
    // at this point 
    // (prev_ptr == NULL) && (next_ptr == NULL) is always false
    //
    // Does the coalescing logic is different in all 3 cases?
    //
    if (prev_ptr == NULL) {
        // insert into head
        char *coalesce_ptr;
        uint32_t coalesce_size;
       
        free_chunk->next = curr_ptr;
        
        // coalescing logic
        coalesce_ptr = (char *) free_chunk;
        coalesce_size = free_chunk->size + hdr_size;
        
        // Do inserted chunk + prev. head form a continuous region in memory?
        // If so, then coalesce
        if ((coalesce_ptr + coalesce_size) == (char *) curr_ptr) {
            free_chunk->size += curr_ptr->size + hdr_size;
            free_chunk->next = curr_ptr->next;        
        } 

        global_ptr = free_chunk;

    } else if (curr_ptr == NULL) {
        // insert into tail 
        char *coalesce_ptr;
        uint32_t coalesce_size;
 
        prev_ptr->next = free_chunk;

        // coalescing logic
   
        coalesce_ptr = (char *) prev_ptr;
        coalesce_size = prev_ptr->size + hdr_size;

        if ((coalesce_ptr + coalesce_size) == (char *) free_chunk) {
            prev_ptr->size += free_chunk->size + hdr_size;
            prev_ptr->next = NULL;
        }

    } else {
        // insert between prev and next
        char *coalesce_ptr;
        uint32_t coalesce_size;

        prev_ptr->next = free_chunk;
        free_chunk->next = curr_ptr;

        // coalescing logic
        
        // prev + free chunk
        coalesce_ptr = (char *) prev_ptr;
        coalesce_size = prev_ptr->size + hdr_size;

        if ((coalesce_ptr + coalesce_size) == (char *) free_chunk) {
            prev_ptr->size += free_chunk->size + hdr_size;
            free_chunk = prev_ptr;
        }
        
        // free chunk + current
        coalesce_ptr = (char *) free_chunk;
        coalesce_size = free_chunk->size + hdr_size;
        
        if ((coalesce_ptr + coalesce_size) == (char *) curr_ptr) {
            free_chunk->size += curr_ptr->size + hdr_size;
            free_chunk->next = curr_ptr->next;        
        } 
    }

}





