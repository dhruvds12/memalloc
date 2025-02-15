#include <cstdlib>
#include <cstring>
#include <unistd.h>   // for sbrk
#include <pthread.h>  // for pthread_mutex_t
#include <cstddef>    // for size_t

typedef char ALIGN[16];

// Unions is a data structure where all members share the same memory location.
union header {
    struct {
        size_t size;
        unsigned is_free;
        union header *next;
    } s;
    ALIGN stub; // This is used to align the header to 16 bytes.
};

typedef union header header_t;

// Require a pointer to the head and tail of this linked list
static header_t *head = nullptr;
static header_t *tail = nullptr;

// Mutex to protect the global linked list
static pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;

// Function to get the free block
header_t *get_free_block(size_t size){
    header_t *curr = head;
    // first fit approach to find the free block (could be optimised)
    while(curr){
        if (curr->s.is_free && curr->s.size >= size){
            return curr;
        }
        curr = curr->s.next;
    }
    return nullptr;
}

extern "C" {

    // Allocates size bytes of memory and returns a pointer to the allocated memory.
    void* malloc(size_t size){
        // if the requested size is 0, return NULL
        if (!size){
            return nullptr;
        }

        // Lock the mutex
        pthread_mutex_lock(&global_malloc_lock);
        // Try find an existing free block
        // Get pointer to the free block
        header_t* header_ptr = get_free_block(size);
        if (header_ptr){
            header_ptr->s.is_free = 0;
            // Unlock the mutex
            pthread_mutex_unlock(&global_malloc_lock);
            // Want to hide the header from the user
            // Incrementing the pointer by 1 will give the user the memory block
            // Adding one moves the pointer by the size of one header_t as header_ptr is a pointer to header_t
            return reinterpret_cast<void*>(header_ptr + 1);
        }

        // No existing free block found
        // Allocate memory using sbrk

        size_t total_size = sizeof(header_t) + size;
        void *block = sbrk(total_size);
        // If sbrk fails, return NULL
        if (block == (void*) -1){
            // Unlock the mutex
            pthread_mutex_unlock(&global_malloc_lock);
            return NULL;
        }

        // Create a header for the new block
        header_ptr = reinterpret_cast<header_t*>(block);
        header_ptr->s.size = size; // size of the block requested by the user (excluding the header)
        header_ptr->s.is_free = 0; // block is not free
        header_ptr->s.next = nullptr; // Added to the end of the linked list therefore next is null

        // If head is null, this is the first block
        if (!head){
            head = header_ptr;
        }

        // If tail is not null, set the next block of the tail to the new block
        if (tail){
            tail->s.next = header_ptr;
        }

        tail = header_ptr;

        // Unlock the mutex
        pthread_mutex_unlock(&global_malloc_lock);

        // Return the memory block to the user (shift header_ptr by one to hide the header)
        return reinterpret_cast<void*>(header_ptr + 1);
    }

    // Frees memory block
    // First check if the block is at the end of the heap and can be released
    // If not, mark the block as free
    void free(void *block){
        if (!block){
            return;
        }

        // lock the mutex
        pthread_mutex_lock(&global_malloc_lock);

        // Get the pointer to the header of the block
        header_t* header_ptr = reinterpret_cast<header_t*>(block) - 1;

        void *programbreak = sbrk(0); // Current value of program break

        // if the block is at the end of the heap, release it
        if ((char*)block + header_ptr->s.size == programbreak){
            if (head == tail){
                head = tail = nullptr;
            } else {
                header_t *temp = head;
                while(temp){
                    // If next block is the tail, set the next block to null and set the tail to the current block
                    if (temp->s.next == tail){
                        temp->s.next = nullptr;
                        tail = temp;
                        break;
                    }
                    // Move to the next block
                    temp = temp->s.next;
                }
            }
            // Decrease the program break by the size of the block
            sbrk(0 - sizeof(header_t) - header_ptr->s.size);
            // Unlock the mutex
            pthread_mutex_unlock(&global_malloc_lock);
            return;
        }

        // The block is not at the end of the heap
        // Mark the block as free
        header_ptr->s.is_free = 1;

        // unlock the mutex
        pthread_mutex_unlock(&global_malloc_lock);
    }

    // Allocates memory for an array of num elements of nsize bytes each and returns a pointer to the allocated memory
    void* calloc(size_t num, size_t nsize){
        if (!num || !nsize){
            return nullptr;
        }

        size_t size = num * nsize;

        // Check for overflow
        if (nsize != size / num){
            return NULL;
        }

        void *block = malloc(size);
        if (!block){
            return nullptr;
        }

        // set the memory block to zero
        memset(block, 0, size);
        return block;

    }

    // Change the size of the given memory block to the size given
    void* realloc(void *block, size_t size){
        if (!block || !size){
            // if block is null, call malloc
            // if size is 0, malloc will handle it
            return malloc(size);
        }
        
        // Get the header of the block
        header_t *header_ptr = reinterpret_cast<header_t*>(block) - 1;

        // If the size of the block is greater than the requested size, return the block
        if (header_ptr->s.size >= size){
            return block;
        }

        // Allocate a new block of correct size
        void *new_block = malloc(size);
        if (new_block){
            // copy the contents of the old block to the new block
            memcpy(new_block, block, header_ptr->s.size);
            // free the old block
            free(block);
        }
        return new_block;
    }
}