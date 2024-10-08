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

    void *memory_ptr = malloc((size_t)myalloc.size);
    if (memory_ptr == NULL) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    memset(memory_ptr, 0, (size_t)myalloc.size);

    myalloc.memory = memory_ptr;  
    myalloc.allocated_memory = NULL;
    myalloc.free_memory = createNode(memory_ptr, rounded_memory_size);
    if (myalloc.free_memory == NULL) {
        perror("Failed to create free memory node");
        free(memory_ptr);
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

    int headerSize = 8;
    int blockSizeWithHeader = _size + headerSize;
    int remainingSpace;
    void* allocatedPtr = NULL;
    Node *fitBlock = NULL;
    Node *currentBlock = myalloc.free_memory;
    int freeMemorySize = available_memory();
    
    printf("Available memory at this point is %d \n", freeMemorySize);

    if (blockSizeWithHeader > myalloc.size || blockSizeWithHeader > freeMemorySize || _size <= 0) {
        sem_post(&mutex);
        return allocatedPtr;
    }
    
    int currentFit, bestFit, worstFit;
    switch (myalloc.aalgorithm){
        case FIRST_FIT: 
            while (currentBlock->data < blockSizeWithHeader && currentBlock != NULL )
                currentBlock = currentBlock->next;
            if (currentBlock->data >= blockSizeWithHeader && currentBlock != NULL )
                fitBlock = currentBlock;
            break;
        case BEST_FIT:
            currentFit = myalloc.size;
            bestFit = myalloc.size;
                for (; currentBlock != NULL; currentBlock = currentBlock->next) {
                remainingSpace = currentBlock->data - blockSizeWithHeader;
                if (remainingSpace >= 0) {
                    currentFit = remainingSpace;
                    if (bestFit >= currentFit) {
                        fitBlock = currentBlock;
                        bestFit = currentFit;
                    }
                }
            }
            break;
        case WORST_FIT:
            currentFit = 0;
            worstFit = -1;
            for (; currentBlock != NULL; currentBlock = currentBlock->next){
                remainingSpace = currentBlock->data - blockSizeWithHeader;
                if (remainingSpace >= 0) {
                    currentFit = remainingSpace;
                    if (worstFit <= currentFit) {
                        fitBlock = currentBlock;
                        worstFit = currentFit;
                    }
                }
            }
            break;
        default:
            printf("Invalid operation.\n");
            break;
    }


    if (fitBlock == NULL) {
        sem_post(&mutex);
        return allocatedPtr;
    }

    int newFitBlockData = fitBlock->data - blockSizeWithHeader;

    if (newFitBlockData == 0) {
        insertNodeAtTail(&myalloc.allocated_memory, fitBlock);
        fitBlock->data = blockSizeWithHeader; 
        allocatedPtr = fitBlock->nodeptr;
        deleteNode(&myalloc.free_memory, fitBlock);
    } 
    else {
        Node *newNode = createNode((char*)fitBlock->nodeptr + blockSizeWithHeader, newFitBlockData);
        allocatedPtr = fitBlock->nodeptr;

        Node *tempBlock = createNode(fitBlock->nodeptr, blockSizeWithHeader);

        insertNodeAtTail(&myalloc.allocated_memory, tempBlock);
        deleteNode(&myalloc.free_memory, fitBlock);
        insertNodeAtTail(&myalloc.free_memory, newNode); 
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

    Node *currentBlock = myalloc.allocated_memory;
    while (currentBlock != NULL && currentBlock->nodeptr != _ptr)
        currentBlock = currentBlock->next;
    
    if (currentBlock == NULL) {
        sem_post(&mutex);
        return;
    }

    Node *tempBlock = createNode(currentBlock->nodeptr, currentBlock->data); 
    deleteNode(&myalloc.allocated_memory, currentBlock);

    Node *prev = NULL;
    Node *mergeBlock = myalloc.free_memory;
    bool merged = false;

    for (; mergeBlock != NULL; mergeBlock = mergeBlock->next) {
        if (((char*)mergeBlock->nodeptr + mergeBlock->data) == tempBlock->nodeptr) {
            mergeBlock->data = mergeBlock->data + tempBlock->data;
            merged = true;
            printf("Merged with previous block.\n");
            free(tempBlock); // since merged
            break;
        }
        prev = mergeBlock;
    }
    if (!merged) {
        mergeBlock = myalloc.free_memory;
        for (; mergeBlock != NULL; mergeBlock = mergeBlock->next) {
            if (((char*)tempBlock->nodeptr + tempBlock->data) == mergeBlock->nodeptr) {
                tempBlock->data = tempBlock->data + mergeBlock->data;
                if (prev == NULL) {
                    myalloc.free_memory = tempBlock;
                } else {
                    prev->next = tempBlock;
                }
                deleteNode(&myalloc.free_memory, mergeBlock); 
                merged = true;
                printf("Merged with next block.\n");
                break;
            }
            prev = mergeBlock;
        }

    }

    if (!merged) {
        insertNodeAtTail(&myalloc.free_memory, tempBlock);
        printf("Added to free list without merging.\n");
    }

    sem_post(&mutex);
}

int compact_allocation(void** _before, void** _after) {
    sem_wait(&mutex);
    int compactedIndex = 0;
    int currentMemorySize = available_memory();
    int nodeCounter = countNodes(myalloc.free_memory);

    if(currentMemorySize == 0 || nodeCounter == 0 ) 
        return myalloc.size;

    Node *currentBlock = myalloc.allocated_memory;

    for (; currentBlock != NULL; currentBlock = currentBlock->next){
        if(currentBlock->next == NULL ){                  
            break;
        }

    char* endOfCurrentBlock = (char*)currentBlock->nodeptr + currentBlock->data + 8;

    if (endOfCurrentBlock < (char*)currentBlock->next->nodeptr) {
        _before[compactedIndex] = currentBlock->next->nodeptr;
        memmove(endOfCurrentBlock, currentBlock->next->nodeptr, currentBlock->next->data + 8);
        currentBlock->next->nodeptr = endOfCurrentBlock;
        _after[compactedIndex] = currentBlock->next->nodeptr;
        compactedIndex++;
    }

    }
    
    Node *newNode = malloc(sizeof(Node));
    newNode->data = available_memory();
    newNode->nodeptr = currentBlock->nodeptr + currentBlock->data;

    for (Node* traverse; myalloc.free_memory != NULL; myalloc.free_memory = traverse) {                         
        traverse = myalloc.free_memory->next;                       
        free(myalloc.free_memory); 
    }

    insertNodeAtTail(&myalloc.free_memory, newNode);             
    sem_post(&mutex);
    return compactedIndex;
}


int available_memory() {
    sem_wait(&mutex);
    int available_memory_size = sumNodesData(myalloc.free_memory); 
    sem_post(&mutex);

    return available_memory_size;
}

void print_statistics() {
    sem_wait(&mutex);
    int allocated_size = sumNodesData(myalloc.allocated_memory); // Sum of allocated memory
    int allocated_chunks = countNodes(myalloc.allocated_memory); // Count of allocated chunks
    int free_size = sumNodesData(myalloc.free_memory); // Sum of free memory
    int free_chunks = countNodes(myalloc.free_memory); // Count of free chunks
    int smallest_free_chunk_size;
    int minSize = myalloc.size;
    int largest_free_chunk_size;
    int maxSize = 0;

    for (Node *current = myalloc.free_memory; current != NULL; current = current->next) {
        if (current->data > maxSize) 
            maxSize = current->data;
        if (current->data < minSize) 
            minSize = current->data;
    }

    if (free_chunks > 0) {
        smallest_free_chunk_size = minSize;
        largest_free_chunk_size = maxSize;
    } else {
        smallest_free_chunk_size = 0;
        largest_free_chunk_size = 0;
    }

     // Calculate the statistics

    printf("Allocated size = %d\n", allocated_size);
    printf("Allocated chunks = %d\n", allocated_chunks);
    printf("Free size = %d\n", free_size);
    printf("Free chunks = %d\n", free_chunks);
    printf("Largest free chunk size = %d\n", largest_free_chunk_size);
    printf("Smallest free chunk size = %d\n", smallest_free_chunk_size);
    sem_post(&mutex);
}



