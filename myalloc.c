#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "myalloc.h"
#include "list.h"

sem_t mutex;

struct Myalloc {
    enum allocation_algorithm aalgorithm;
    int size;
    void* memory;
    // Some other data members you want, 
    // such as lists to record allocated/free memory

    struct nodeStruct* allocatedmemory;
    struct nodeStruct* freememory; 
};



struct Myalloc myalloc;

void initialize_allocator(int _size, enum allocation_algorithm _aalgorithm) {
    assert(_size > 0);
    sem_init(&mutex, 0, 1);
    myalloc.aalgorithm = _aalgorithm;

    int rounded_memory_size = ((_size + 63) / 64) * 64;
    printf("Rounded up size is %d \n", rounded_memory_size);
    myalloc.size = rounded_memory_size;

    void *ptr = malloc((size_t)myalloc.size);
    if (ptr == NULL) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    memset(ptr, 0, (size_t)myalloc.size);

    myalloc.memory = ptr;  
    myalloc.allocatedmemory = NULL;
    myalloc.freememory = List_createNode(ptr, rounded_memory_size);
    if (myalloc.freememory == NULL) {
        perror("Failed to create free memory node");
        free(ptr);
        exit(EXIT_FAILURE);
    }
    sem_post(&mutex);
}


void destroy_allocator() {
    sem_wait(&mutex);
    free(myalloc.memory);

    // free other dynamic allocated memory to avoid memory leak
    List_distoryer(myalloc.allocatedmemory);
    List_distoryer(myalloc.freememory);
    sem_post(&mutex);

}

void* allocate(int _size) {
    sem_wait(&mutex);
    int sizewithheader = _size + 8;
    void* ptr = NULL;
    struct nodeStruct *emptyblock = NULL;
    struct nodeStruct *currentblock = myalloc.freememory;

    int available_mem = available_memory();
    printf("Available memory at this point is %d \n", available_mem);

    if ((_size <= 0) || (sizewithheader > available_mem) || (sizewithheader > myalloc.size)) {
        sem_post(&mutex);
        return ptr;
    }

    int leftover;
    if (myalloc.aalgorithm == FIRST_FIT) {
        while (currentblock != NULL && currentblock->item < sizewithheader) {
            currentblock = currentblock->next;
        }
        if (currentblock != NULL && currentblock->item >= sizewithheader) {
            emptyblock = currentblock;
        }
    } else if (myalloc.aalgorithm == BEST_FIT) {
        int temp = myalloc.size;
        int best = myalloc.size;

        while (currentblock != NULL) {
            leftover = currentblock->item - sizewithheader;
            if (leftover >= 0) {
                temp = leftover;
                if (temp < best) {
                    emptyblock = currentblock;
                    best = temp;
                }
            }
            currentblock = currentblock->next;
        }
    } else {  // WORST_FIT
        int temp = 0;
        int worst = -1;

        while (currentblock != NULL) {
            leftover = currentblock->item - sizewithheader;
            if (leftover >= 0) {
                temp = leftover;
                if (temp > worst) {
                    emptyblock = currentblock;
                    worst = temp;
                }
            }
            currentblock = currentblock->next;
        }
    }

    if (emptyblock == NULL) {
        sem_post(&mutex);
        return ptr;
    }

    int new_emptyblock_item = emptyblock->item - sizewithheader;
    if (new_emptyblock_item == 0) {
        List_insertTail(&myalloc.allocatedmemory, emptyblock);
        emptyblock->item = sizewithheader; // Include the header size
        ptr = emptyblock->nodeptr;
        List_deleteNode(&myalloc.freememory, emptyblock);
    } else {
        struct nodeStruct *newnode = List_createNode((char*)emptyblock->nodeptr + sizewithheader, new_emptyblock_item);
        ptr = emptyblock->nodeptr;

        struct nodeStruct *blockcopy = List_createNode(emptyblock->nodeptr, sizewithheader);

        List_insertTail(&myalloc.allocatedmemory, blockcopy);
        List_deleteNode(&myalloc.freememory, emptyblock);
        List_insertTail(&myalloc.freememory, newnode); 
    }
    sem_post(&mutex);
    return ptr;
}


void deallocate(void* _ptr) {
    sem_wait(&mutex);
    if (_ptr == NULL) {
        sem_post(&mutex);
        return;
    }

    struct nodeStruct *currentblock = myalloc.allocatedmemory;
    while (currentblock != NULL && currentblock->nodeptr != _ptr) {
        currentblock = currentblock->next;
    }
    if (currentblock == NULL) {
        sem_post(&mutex);
        return;
    }

    struct nodeStruct *copy = List_createNode(currentblock->nodeptr, currentblock->item); // Include the header size
    List_deleteNode(&myalloc.allocatedmemory, currentblock);

    struct nodeStruct *prev = NULL;
    struct nodeStruct *mergecheck = myalloc.freememory;
    bool merged = false;

    // Check if we can merge with the previous block
    while (mergecheck != NULL) {
        if (((char*)mergecheck->nodeptr + mergecheck->item) == copy->nodeptr) {
            mergecheck->item += copy->item;
            merged = true;
            printf("Merged with previous block.\n");
            free(copy); // Free the copy node as it is now merged
            break;
        }
        prev = mergecheck;
        mergecheck = mergecheck->next;
    }

    // Check if we can merge with the next block
    if (!merged) {
        mergecheck = myalloc.freememory;
        while (mergecheck != NULL) {
            if (((char*)copy->nodeptr + copy->item) == mergecheck->nodeptr) {
                copy->item += mergecheck->item;
                if (prev == NULL) {
                    myalloc.freememory = copy;
                } else {
                    prev->next = copy;
                }
                List_deleteNode(&myalloc.freememory, mergecheck); // Remove the merged node
                merged = true;
                printf("Merged with next block.\n");
                break;
            }
            prev = mergecheck;
            mergecheck = mergecheck->next;
        }
    }

    // If we didn't merge, just add the block to the free list
    if (!merged) {
        List_insertTail(&myalloc.freememory, copy);
        printf("Added to free list without merging.\n");
    }

    sem_post(&mutex);
}





int compact_allocation(void** _before, void** _after) {
    sem_wait(&mutex);
    int compacted = 0;

    // compact allocated memory
    // update _before, _after and compacted_size
    if(available_memory() == 0) return myalloc.size;
    if(List_countNodes(myalloc.freememory) == 0) return myalloc.size;

    struct nodeStruct *currentblock = myalloc.allocatedmemory;

    while(currentblock != NULL){
        if(currentblock->next == NULL ){                    // only one allocated block
            break;
        }

        if(currentblock->nodeptr + currentblock->item + 8 < currentblock->next->nodeptr){
            _before[compacted] =  currentblock->next->nodeptr;
            memmove(currentblock->nodeptr + currentblock->item + 8, currentblock->next->nodeptr, currentblock->next->item + 8); // shift memory forward
            currentblock->next->nodeptr = currentblock->nodeptr + currentblock->item + 8; // 
            _after[compacted] =  currentblock->next->nodeptr; 
            compacted++;
        }
        currentblock = currentblock->next;
    }
    
    struct nodeStruct *newnode = malloc(sizeof(struct nodeStruct));
    newnode->item = available_memory();
    newnode->nodeptr = currentblock->nodeptr + currentblock->item;

    struct nodeStruct *temp; 
    while(myalloc.freememory != NULL){                         // tranverse to delate the previous freelist
        temp = myalloc.freememory->next;                       
        free(myalloc.freememory); 
        myalloc.freememory = temp;
    }

    List_insertTail(&myalloc.freememory, newnode);             // create new freelist
    sem_post(&mutex);
    return compacted;
}

int available_memory() {
    sem_wait(&mutex);
    int available_memory_size = List_sum_memsize(myalloc.freememory);      // use list function to tranverse and sum
    // Calculate available memory size
    sem_post(&mutex);
    return available_memory_size;
}

void print_statistics() {
    sem_wait(&mutex);
    int allocated_size = List_sum_memsize(myalloc.allocatedmemory); // Sum of allocated memory
    int allocated_chunks = List_countNodes(myalloc.allocatedmemory); // Count of allocated chunks
    int free_size = List_sum_memsize(myalloc.freememory); // Sum of free memory
    int free_chunks = List_countNodes(myalloc.freememory); // Count of free chunks
    int temp_min = myalloc.size;
    int temp_max = 0;
    int smallest_free_chunk_size;
    int largest_free_chunk_size;

    for (struct nodeStruct *current = myalloc.freememory; current != NULL; current = current->next) {
        if (current->item > temp_max) {
            temp_max = current->item;
        }
        if (current->item < temp_min) {
            temp_min = current->item;
        }
    }

    if (free_chunks > 0) {
        smallest_free_chunk_size = temp_min;
        largest_free_chunk_size = temp_max;
    } else {
        smallest_free_chunk_size = 0;
        largest_free_chunk_size = 0;
    }


    printf("Allocated size = %d\n", allocated_size);
    printf("Allocated chunks = %d\n", allocated_chunks);
    printf("Free size = %d\n", free_size);
    printf("Free chunks = %d\n", free_chunks);
    printf("Largest free chunk size = %d\n", largest_free_chunk_size);
    printf("Smallest free chunk size = %d\n", smallest_free_chunk_size);
    sem_post(&mutex);
}



