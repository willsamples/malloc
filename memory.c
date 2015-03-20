#include "HardwareProfile.h"
#include "USB/usb.h"
#include "USB/usb_function_cdc.h"

#include "memory.h"
#include "IO.h"
#include "list.h"

// Heap significant memory values:
// 0xfe - Initialized
// 0xaa - Allocated
// 0xee - Deallocated
unsigned char heap[HEAP_SIZE];
ListNode * mblk_head; // Linked list of memory blocks

void heap_init()
{
    int i;
    mblk_head = 0;

    for ( i = 0; i < HEAP_SIZE; ++i )
    {
	// Set initial value for demo purposes
        heap[i] = 0xFE;
    }
}

unsigned char * alloc( int data_size )
{
    // my strategy is to store my memory block data in the heap
    // directly after the memory that is allocated for the user.
    // therefore, alloc(8) will require 8 + 16 bytes, making the
    // heap look like:
    // 0000  aa aa aa aa aa aa aa aa  M1 M1 M1 M1 M2 M2 M2 M2
    // 0010  L1 L1 L1 L1 L2 L2 L2 L2
    // where M is the MBLK struct:
    //  M1 = the address of the first byte allocated,
    //  M2 = the total number of bytes allocated, INCLUDING M1-L2
    //       (that is important because it means the memory block
    //        keeps track of and protects its own location on the
    //        heap)
    // and L is the ListNode struct:
    //  L1 = the address of the next node in the list
    //  L2 = the address of M1 (void * data)

    // keep in mind when you see something like:
    //    mblk.address + mblk.size
    // that the sum is actually one byte PAST mblk's data in the heap
    // since we want to start counting from zero
    // this is why you might see (address+size-1) used to get the
    // last byte in a block

    ListNode * travel, * node;
    unsigned char * addr = heap;
    MBLK * mblk;
    int address_collides;
    int i;

    // pad the requested number of bytes to make it divisible by 4, to
    // satisfy word boundary requirements
    if (data_size % 4 != 0)
        data_size += 4 - (data_size % 4);

    // block_size: how much of the heap we will actually be consuming
    int block_size = data_size + sizeof(MBLK) + sizeof(ListNode);

	
    if (data_size <= 0 || block_size > HEAP_SIZE)
    {
        Display("alloc() error: invalid size request\n\r");
        return 0;
    }

    // Find a suitable starting address in the heap for our new block
    do
    {
        travel = mblk_head;
        address_collides = FALSE;

        // Starting with addr = &heap[0], iterate through the memory blocks
        // and test if allocating at addr would collide with the memory already
        // allocated to the existing block. If so, set addr to the next byte
        // in the heap after the existing memory block and start over. When
        // there is no collision, we have a workable address space for the user
        while (travel != 0 && address_collides == FALSE)
        {
            mblk = (MBLK*) travel->data;

            // if either the first or last byte in our proposed block falls between
            // the existing block's first and last byte, we consider this a collision
            if ((addr >= mblk->addr && addr < mblk->addr+mblk->len)
                || (addr+block_size-1 >= mblk->addr && addr+block_size-1 < mblk->addr+mblk->len))
            {
                address_collides = TRUE;
                addr = mblk->addr + mblk->len;

                // check upper bound of heap to keep from looping forever if there's no space
                if (addr + block_size > heap + HEAP_SIZE)
                {
                    Display("alloc() error: Could not find contiguous space in heap.\n\r");
                    return 0;
                }
            }

            travel = travel->next;
        }
    }
    // need to start the iteration over with a new addr value if we failed our test
    while (address_collides == TRUE);


    // Now we have space set aside, fill it

    for (i = 0; i < data_size; ++i)
    {
        addr[i] = 0xAA; // initial value for demo purposes
    }

    // append our MBLK and ListNode data so we can keep track of the allocation
    mblk = (MBLK*) (addr+data_size);
    mblk->addr = addr;
    mblk->len  = block_size;

    node = (ListNode*) (addr+data_size+sizeof(MBLK));
    node->data = mblk;
    node->next = 0;

    // We just made a new ListNode, need to add it to the global list
    if (mblk_head == 0)
        mblk_head = node;
    else
    {
        for (travel = mblk_head; travel->next != 0; travel = travel->next);
        travel->next = node;
    }

    return addr;
}

void mfree( unsigned char * ptr )
{
    ListNode * travel = mblk_head, * trail = 0;
    MBLK * mblk;
    unsigned char * addr;
    int i, len;

    // iterate through the global list of memory blocks, find
    // the given pointer and remove that block from the list
    while (travel != 0)
    {
        mblk = (MBLK*) travel->data;

        if (ptr == mblk->addr)
        {
            if (travel == mblk_head)
                mblk_head = travel->next;
            else
                trail->next = travel->next;

            addr = mblk->addr;
            len  = mblk->len;
            for (i = 0; i < len; ++i)
                addr[i] = 0xEE; // signify memory was freed, for demo purposes

            return;
        }
        
        trail  = travel;
        travel = travel->next;
    }

    Display("mfree() error: Supplied pointer was never allocated.\n\r");
}
