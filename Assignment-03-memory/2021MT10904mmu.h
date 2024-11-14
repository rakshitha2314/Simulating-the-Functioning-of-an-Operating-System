#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#define HEADER_SIZE sizeof(struct header)

typedef struct header *header_ptr;
size_t page_size = 0;  // Store system page size for alignment
// Base pointer for the memory pool
void *base = NULL;

// Memory block header structure
struct header {
    bool is_free;       // True if memory block is free, false otherwise
    size_t size;        // Size of memory block (excluding the header)
    header_ptr next;    // Points to the next memory block
    header_ptr prev;    // Points to the previous memory block
};

// HELPER FUNCTIONS

// Internal function to initialize page_size
static void initialize_page_size() {
    if (page_size == 0) {  // Only initialize if not already set
        page_size = (size_t)sysconf(_SC_PAGESIZE);
    }
}

// Function to round size to the nearest multiple of the system's page size
size_t round_to_page_size(size_t size) {
    return (size + page_size - 1) & ~(page_size - 1);
}

// Find a suitable free block using the first-fit policy
header_ptr find_suitable_block(header_ptr *last, size_t size) {
    header_ptr search_ptr = (header_ptr)base;
    while (search_ptr && !(search_ptr->is_free && search_ptr->size >= size)) {
        *last = search_ptr;
        search_ptr = search_ptr->next;
    }
    return search_ptr;
}

// Split the block if it’s larger than needed and ensures alignment
void split_block(header_ptr block, size_t size) {
    size = round_to_page_size(size + HEADER_SIZE);
    if (block->size - size >= HEADER_SIZE) {  // Ensure there's space for another block
        header_ptr new_block = (header_ptr)((char *)block + size); // New block starts after user data
        new_block->size = block->size - size - HEADER_SIZE;
        block->size = size - HEADER_SIZE;

        // Update pointers
        new_block->next = block->next;
        block->next = new_block;
        new_block->prev = block;

        if (new_block->next) {
            new_block->next->prev = new_block;
        }
        
        new_block->is_free = true;  // Mark the new block as free
    }
}

// Extend the heap using mmap to request new memory
header_ptr extend_heap(header_ptr last, size_t size) {
    size_t total_size = round_to_page_size(HEADER_SIZE + size); // Ensure allocation is page-aligned
    header_ptr new_ptr = (header_ptr)mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    
    if (new_ptr == MAP_FAILED) {  // Check if mmap succeeded
        return NULL;
    }

    new_ptr->size = total_size - HEADER_SIZE;  // Store size of the user-accessible block
    new_ptr->is_free = false;  // Mark as used
    new_ptr->next = NULL;      // No next block yet
    new_ptr->prev = last;      // Set previous block

    if (last) {
        last->next = new_ptr;  // Link with previous block
    }

    return new_ptr;
}

// Merge current block with the next free block if possible
header_ptr merge_free_blocks(header_ptr current_block) {
    if (current_block->next && current_block->next->is_free) {
        current_block->size += HEADER_SIZE + current_block->next->size;  // Increase size
        current_block->next = current_block->next->next;  // Update next pointer

        if (current_block->next) {
            current_block->next->prev = current_block;  // Update previous pointer
        }
    }
    return current_block;
}

// Get the header of the block containing the pointer
header_ptr get_block_start(void *ptr) {
    return (header_ptr)((char *)ptr - HEADER_SIZE);
}

// Calculate the pointer to user data from the header pointer
static inline void* get_user_data(header_ptr head) {
    return (void*)((char *)head + HEADER_SIZE);  // Move past the header
}

// PART-02 of ASSIGNMENT : my_malloc, my_free, my_calloc

// Function to allocate memory
void* my_malloc(size_t size) {
    initialize_page_size(); // Initialize page size if required
    header_ptr last = NULL;
    header_ptr first_fit;

    // If base is not NULL, find a suitable block
    if (base) {
        last = (header_ptr)base;
        first_fit = find_suitable_block(&last, size);  // Only use the requested size
        if (first_fit) {
            // If block is larger than needed, split it
            split_block(first_fit, size);  // Use requested size
            first_fit->is_free = false;  // Mark block as used
        } else {
            // Extend heap if no suitable block was found
            first_fit = extend_heap(last, size);
            if (!first_fit) {
                perror("malloc");
                return NULL;
            }
        }
    } else {
        // Initialize the linked list
        first_fit = extend_heap(NULL, size);  // First allocation
        if (!first_fit) {
            perror("malloc");
            return NULL;
        }
        base = first_fit;  // Set base pointer
    }
    return get_user_data(first_fit);  // Return pointer to user data
}

// Function to allocate and initialize memory to zero
void* my_calloc(size_t nelem, size_t size) {
    size_t s = nelem * size;  // Calculate total size
    void *new_block = my_malloc(s);  // Allocate memory
    if (new_block) {
        memset(new_block, 0, s);  // Initialize allocated memory to zero
    } else {
        perror("calloc");
    }
    return new_block;  // Return pointer to allocated memory
}

// Function to release allocated memory
void my_free(void* ptr) {
    initialize_page_size();     // Initialize page size if not done already
    if (ptr == NULL) return;  // Safety check

    // We assume only the pointers allocated by malloc/calloc are freed
    header_ptr head = get_block_start(ptr);  // Get block header
    head->is_free = true;  // Mark as free

    // Attempt to merge with previous and next blocks
    if (head->prev && head->prev->is_free) {
        head = merge_free_blocks(head->prev);
    }

    if (head->next) {
        merge_free_blocks(head);  // Merge with the next block if possible
    } else {
        // Release memory back to the system if it's the last block
        if (head->prev) {
            head->prev->next = NULL;  // Update previous block's next pointer
        } else {
            base = NULL;  // Reset base if it's the last block
        }

        // Release memory back to the system
        munmap(head, (HEADER_SIZE + head->size));
    }
}

