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

    Node* allocatedMemory;
    Node* freeMemory; 
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
    myalloc.allocatedMemory = NULL;
    myalloc.freeMemory = createNode(ptr, rounded_memory_size);
    if (myalloc.freeMemory == NULL) {
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
    destructor(myalloc.allocatedMemory);
    destructor(myalloc.freeMemory);
    sem_post(&mutex);

}

void* allocate(int _size) {
    sem_wait(&mutex);
    int sizewithheader = _size + 8;
    void* ptr = NULL;
    Node *emptyblock = NULL;
    Node *currentblock = myalloc.freeMemory;

    int available_mem = available_memory();
    printf("Available memory at this point is %d \n", available_mem);

    if ((_size <= 0) || (sizewithheader > available_mem) || (sizewithheader > myalloc.size)) {
        sem_post(&mutex);
        return ptr;
    }

    int leftover;
    if (myalloc.aalgorithm == FIRST_FIT) {
        while (currentblock != NULL && currentblock->data < sizewithheader) {
            currentblock = currentblock->next;
        }
        if (currentblock != NULL && currentblock->data >= sizewithheader) {
            emptyblock = currentblock;
        }
    } else if (myalloc.aalgorithm == BEST_FIT) {
        int temp = myalloc.size;
        int best = myalloc.size;

        while (currentblock != NULL) {
            leftover = currentblock->data - sizewithheader;
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
            leftover = currentblock->data - sizewithheader;
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

    int new_emptyblock_data = emptyblock->data - sizewithheader;
    if (new_emptyblock_data == 0) {
        insertNodeAtTail(&myalloc.allocatedMemory, emptyblock);
        emptyblock->data = sizewithheader; // Include the header size
        ptr = emptyblock->nodeptr;
        deleteNode(&myalloc.freeMemory, emptyblock);
    } else {
        Node *newnode = createNode((char*)emptyblock->nodeptr + sizewithheader, new_emptyblock_data);
        ptr = emptyblock->nodeptr;

        Node *blockcopy = createNode(emptyblock->nodeptr, sizewithheader);

        insertNodeAtTail(&myalloc.allocatedMemory, blockcopy);
        deleteNode(&myalloc.freeMemory, emptyblock);
        insertNodeAtTail(&myalloc.freeMemory, newnode); 
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

    Node *currentblock = myalloc.allocatedMemory;
    while (currentblock != NULL && currentblock->nodeptr != _ptr) {
        currentblock = currentblock->next;
    }
    if (currentblock == NULL) {
        sem_post(&mutex);
        return;
    }

    Node *copy = createNode(currentblock->nodeptr, currentblock->data); // Include the header size
    deleteNode(&myalloc.allocatedMemory, currentblock);

    Node *prev = NULL;
    Node *mergecheck = myalloc.freeMemory;
    bool merged = false;

    // Check if we can merge with the previous block
    while (mergecheck != NULL) {
        if (((char*)mergecheck->nodeptr + mergecheck->data) == copy->nodeptr) {
            mergecheck->data += copy->data;
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
        mergecheck = myalloc.freeMemory;
        while (mergecheck != NULL) {
            if (((char*)copy->nodeptr + copy->data) == mergecheck->nodeptr) {
                copy->data += mergecheck->data;
                if (prev == NULL) {
                    myalloc.freeMemory = copy;
                } else {
                    prev->next = copy;
                }
                deleteNode(&myalloc.freeMemory, mergecheck); // Remove the merged node
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
        insertNodeAtTail(&myalloc.freeMemory, copy);
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
    if(countNodes(myalloc.freeMemory) == 0) return myalloc.size;

    Node *currentblock = myalloc.allocatedMemory;

    while(currentblock != NULL){
        if(currentblock->next == NULL ){                    // only one allocated block
            break;
        }

        if(currentblock->nodeptr + currentblock->data + 8 < currentblock->next->nodeptr){
            _before[compacted] =  currentblock->next->nodeptr;
            memmove(currentblock->nodeptr + currentblock->data + 8, currentblock->next->nodeptr, currentblock->next->data + 8); // shift memory forward
            currentblock->next->nodeptr = currentblock->nodeptr + currentblock->data + 8; // 
            _after[compacted] =  currentblock->next->nodeptr; 
            compacted++;
        }
        currentblock = currentblock->next;
    }
    
    Node *newnode = malloc(sizeof(Node));
    newnode->data = available_memory();
    newnode->nodeptr = currentblock->nodeptr + currentblock->data;

    Node *temp; 
    while(myalloc.freeMemory != NULL){                         // tranverse to delate the previous freelist
        temp = myalloc.freeMemory->next;                       
        free(myalloc.freeMemory); 
        myalloc.freeMemory = temp;
    }

    insertNodeAtTail(&myalloc.freeMemory, newnode);             // create new freelist
    sem_post(&mutex);
    return compacted;
}

int available_memory() {
    sem_wait(&mutex);
    int available_memory_size = sumNodesData(myalloc.freeMemory);      // use list function to tranverse and sum
    // Calculate available memory size
    sem_post(&mutex);
    return available_memory_size;
}

void print_statistics() {
    sem_wait(&mutex);
    int allocated_size = sumNodesData(myalloc.allocatedMemory); // Sum of allocated memory
    int allocated_chunks = countNodes(myalloc.allocatedMemory); // Count of allocated chunks
    int free_size = sumNodesData(myalloc.freeMemory); // Sum of free memory
    int free_chunks = countNodes(myalloc.freeMemory); // Count of free chunks
    int temp_min = myalloc.size;
    int temp_max = 0;
    int smallest_free_chunk_size;
    int largest_free_chunk_size;

    for (Node *current = myalloc.freeMemory; current != NULL; current = current->next) {
        if (current->data > temp_max) {
            temp_max = current->data;
        }
        if (current->data < temp_min) {
            temp_min = current->data;
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



