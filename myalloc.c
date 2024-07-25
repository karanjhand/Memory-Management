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

    Node* allocated_memory;
    Node* free_memory; 
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
    myalloc.allocated_memory = NULL;
    myalloc.free_memory = createNode(ptr, rounded_memory_size);
    if (myalloc.free_memory == NULL) {
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
    destructor(myalloc.allocated_memory);
    destructor(myalloc.free_memory);
    sem_post(&mutex);
}


void* allocate(int _size) {
    sem_wait(&mutex);
    int sizewithheader = _size + 8;
    void* allocatedPtr = NULL;
    Node *emptyblock = NULL;
    Node *currentblock = myalloc.free_memory;
    int availableMemory = available_memory();
   
    printf("Available memory at this point is %d \n", availableMemory);

    if ((_size <= 0) || (sizewithheader > availableMemory) || (sizewithheader > myalloc.size)) {
        sem_post(&mutex);
        return allocatedPtr;
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
        return allocatedPtr;
    }

    int new_emptyblock_data = emptyblock->data - sizewithheader;
    if (new_emptyblock_data == 0) {
        insertNodeAtTail(&myalloc.allocated_memory, emptyblock);
        emptyblock->data = sizewithheader; // Include the header size
        allocatedPtr = emptyblock->nodeptr;
        deleteNode(&myalloc.free_memory, emptyblock);
    } else {
        Node *newnode = createNode((char*)emptyblock->nodeptr + sizewithheader, new_emptyblock_data);
        allocatedPtr = emptyblock->nodeptr;

        Node *blockcopy = createNode(emptyblock->nodeptr, sizewithheader);

        insertNodeAtTail(&myalloc.allocated_memory, blockcopy);
        deleteNode(&myalloc.free_memory, emptyblock);
        insertNodeAtTail(&myalloc.free_memory, newnode); 
    }
    sem_post(&mutex);
    return allocatedPtr;
}





void deallocate(void* _ptr) {
    sem_wait(&mutex);
    if (_ptr == NULL) {
        sem_post(&mutex);
        return;
    }

    Node *currentblock = myalloc.allocated_memory;
    while (currentblock != NULL && currentblock->nodeptr != _ptr) {
        currentblock = currentblock->next;
    }
    if (currentblock == NULL) {
        sem_post(&mutex);
        return;
    }

    Node *copy = createNode(currentblock->nodeptr, currentblock->data); // Include the header size
    deleteNode(&myalloc.allocated_memory, currentblock);

    Node *prev = NULL;
    Node *mergecheck = myalloc.free_memory;
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
        mergecheck = myalloc.free_memory;
        while (mergecheck != NULL) {
            if (((char*)copy->nodeptr + copy->data) == mergecheck->nodeptr) {
                copy->data += mergecheck->data;
                if (prev == NULL) {
                    myalloc.free_memory = copy;
                } else {
                    prev->next = copy;
                }
                deleteNode(&myalloc.free_memory, mergecheck); // Remove the merged node
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
        insertNodeAtTail(&myalloc.free_memory, copy);
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
    if(countNodes(myalloc.free_memory) == 0) return myalloc.size;

    Node *currentblock = myalloc.allocated_memory;

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
    while(myalloc.free_memory != NULL){                         // tranverse to delate the previous freelist
        temp = myalloc.free_memory->next;                       
        free(myalloc.free_memory); 
        myalloc.free_memory = temp;
    }

    insertNodeAtTail(&myalloc.free_memory, newnode);             // create new freelist
    sem_post(&mutex);
    return compacted;
}

int available_memory() {
    sem_wait(&mutex);
    int available_memory_size = sumNodesData(myalloc.free_memory);      // use list function to tranverse and sum
    // Calculate available memory size
    sem_post(&mutex);
    return available_memory_size;
}

void print_statistics() {
    sem_wait(&mutex);
    int allocated_size = sumNodesData(myalloc.allocated_memory); // Sum of allocated memory
    int allocated_chunks = countNodes(myalloc.allocated_memory); // Count of allocated chunks
    int free_size = sumNodesData(myalloc.free_memory); // Sum of free memory
    int free_chunks = countNodes(myalloc.free_memory); // Count of free chunks
    int temp_min = myalloc.size;
    int temp_max = 0;
    int smallest_free_chunk_size;
    int largest_free_chunk_size;

    for (Node *current = myalloc.free_memory; current != NULL; current = current->next) {
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



