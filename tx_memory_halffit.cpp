/*
 * tx_memory_halffit.cpp
 *
 *  Created on: Jan 29, 2024
 *      Author: tian_
 */


#include "tx_memory_halffit.hpp"

#include "tx_assert.h"


//============================== START OF IMPLEMENTATION =========================

struct AllocatorHalfFit::MemBlock
{
	// Block header
	size_t					size;
	size_t					ref_count;
	MemBlock *			prev_free_block;	// Ptr to the next block in the linked list of free blocks in the same size range
	MemBlock *			next_free_block;


	// A terminal segment of the block (called footer) is reserved for book-keeping
	// The segment also stores the size of this block; it is used for reverse lookup from the next block
	inline size_t & get_block_footer(void)
	{
		size_t * footer_ptr = (size_t *)((size_t)this + size - sizeof(size_t));
		return *footer_ptr;
	}
	inline size_t get_prev_block_size(void) const
	{
		size_t * footer_ptr = (size_t *)((size_t)this - sizeof(size_t));
		return *footer_ptr;
	}
	inline MemBlock * get_prev_block(void) const
	{
		size_t * footer_ptr = (size_t *)((size_t)this - sizeof(size_t));
		return (MemBlock *)((size_t)this - *footer_ptr);
	}
};

static_assert(sizeof(void *) == sizeof(size_t));



class AllocatorHalfFitImpl : public AllocatorHalfFit
{
public:

	static constexpr size_t const BLOCKUSED_INFO_SIZE = 3 * sizeof(size_t);
	static constexpr size_t const BLOCKFREE_INFO_SIZE = 5 * sizeof(size_t);

	static constexpr size_t const MIN_ALLOC_SIZE_LOG2 = 5;
	static constexpr size_t const MIN_ALLOC_SIZE = 1u << MIN_ALLOC_SIZE_LOG2; // Including block header and footer
	static_assert(MIN_ALLOC_SIZE >= BLOCKFREE_INFO_SIZE && MIN_ALLOC_SIZE >= BLOCKUSED_INFO_SIZE); // Ensure every block has enough space for the segment data
	static constexpr size_t const BLOCK_ALIGNMENT_LOG2 = 3;
	static constexpr size_t const BLOCK_ALIGNMENT = 1u << BLOCK_ALIGNMENT_LOG2;


public:

	inline static size_t get_order_from_size(size_t size);
	inline static size_t get_block_order(MemBlock const * block_ptr);

	inline static MemBlock * address_to_blockptr(size_t size) {return (MemBlock *)size;}
	inline static size_t blockptr_to_address(MemBlock const * block_ptr) {return (size_t)block_ptr;}

	inline static size_t next_aligned_address(size_t size)
	{
		TX_ASSERT(size > 0);
		return (((size - 1) >> BLOCK_ALIGNMENT_LOG2) + 1) << BLOCK_ALIGNMENT_LOG2;
	}

public:

	size_t get_used_size_ver2(void) const;

	void initialize_management_data(void);

	void register_free_block(MemBlock * block_ptr);
	void unregister_free_block(MemBlock * block_ptr);

	void * allocate(size_t size);
	void free(void * content_ptr);
};

size_t AllocatorHalfFitImpl::get_order_from_size(size_t size)
// Roughly, size in the interval [2^k, 2^(k+1)) has order k.
{
	return 8u * sizeof(size_t) - 1 - __builtin_clz(size) - MIN_ALLOC_SIZE_LOG2;
}

size_t AllocatorHalfFitImpl::get_block_order(MemBlock const * block_ptr)
{
	return get_order_from_size(block_ptr->size);
}

void AllocatorHalfFitImpl::initialize_management_data(void)
{
	for (uint8_t i = 0; i < free_block_list_size; i++)
	{
		free_block_list[i] = nullptr;
	}

	MemBlock * block_ptr = address_to_blockptr(this->address_start);
	block_ptr->size = this->address_end - this->address_start;
	block_ptr->get_block_footer() = block_ptr->size;
	block_ptr->ref_count = 0;
	register_free_block(block_ptr);
}

void AllocatorHalfFitImpl::register_free_block(MemBlock * block_ptr)
{
	size_t order = get_block_order(block_ptr);

	MemBlock * next_free_block = free_block_list[order];
	if (next_free_block != nullptr)
	{
		TX_ASSERT(next_free_block->prev_free_block == nullptr);
		next_free_block->prev_free_block = block_ptr;
	}

	block_ptr->prev_free_block = nullptr;
	block_ptr->next_free_block = next_free_block;
	free_block_list[order] = block_ptr;
}

void AllocatorHalfFitImpl::unregister_free_block(MemBlock * block_ptr)
{
	MemBlock * prev_free_block = block_ptr->prev_free_block;
	MemBlock * next_free_block = block_ptr->next_free_block;

	if (prev_free_block != nullptr)
	{
		prev_free_block->next_free_block = next_free_block;
	}
	else
	{
		size_t order = get_block_order(block_ptr);
		free_block_list[order] = next_free_block;
	}

	if (next_free_block != nullptr)
	{
		next_free_block->prev_free_block = prev_free_block;
	}
}

void * AllocatorHalfFitImpl::allocate(size_t size)
{
	// Adjust the allocation size to the nearest valid number
	size += BLOCKUSED_INFO_SIZE;
	if (size < MIN_ALLOC_SIZE) {size = MIN_ALLOC_SIZE;}
	size = next_aligned_address(size);

	// Find a suitable free block for the allocation
	size_t index = get_order_from_size(size) + 1;
	TX_ASSERT(index < free_block_list_size);
	while (free_block_list[index] == nullptr)
	{
		index ++;
		if (index >= free_block_list_size)
		{
			TX_ASSERT(0); // Failing means out of memory; TODO: Replace by exception
		}
	}
	MemBlock * block_ptr = free_block_list[index];

	unregister_free_block(block_ptr);

	// Split the current free block into two if the size allows
	if (block_ptr->size >= size + MIN_ALLOC_SIZE)
	{
		MemBlock * new_block_ptr = address_to_blockptr(blockptr_to_address(block_ptr) + size);
		new_block_ptr->size = block_ptr->size - size;
		new_block_ptr->get_block_footer() = new_block_ptr->size;
		new_block_ptr->ref_count = 0;
		register_free_block(new_block_ptr);

		block_ptr->size = size;
		block_ptr->get_block_footer() = size;
	}

	block_ptr->ref_count = 1;
	return &block_ptr->prev_free_block;
}

void AllocatorHalfFitImpl::free(void * content_ptr)
{
	MemBlock * block_ptr = address_to_blockptr((size_t)content_ptr - __builtin_offsetof(MemBlock, prev_free_block));

	TX_ASSERT(block_ptr->size == block_ptr->get_block_footer()); // Check (without guarantee) that this is a memory block
	TX_ASSERT(block_ptr->ref_count > 0); // Ensure that the block is used

	// Merge with the next block if it is free
	size_t block_size = block_ptr->size;
	MemBlock * next_block_ptr = address_to_blockptr(blockptr_to_address(block_ptr) + block_size);
	if (blockptr_to_address(next_block_ptr) != this->address_end)
	{
		if (next_block_ptr->ref_count == 0)
		{
			unregister_free_block(next_block_ptr);
			block_size += next_block_ptr->size;
		}
	}

	// Merge with the previous block if it is free
	if (blockptr_to_address(block_ptr) != this->address_start)
	{
		MemBlock * prev_block_ptr = block_ptr->get_prev_block();
		if (prev_block_ptr->ref_count == 0)
		{
			unregister_free_block(prev_block_ptr);
			block_size += prev_block_ptr->size;
			block_ptr = prev_block_ptr;
		}
	}

	block_ptr->size = block_size;
	block_ptr->get_block_footer() = block_size;
	block_ptr->ref_count = 0;
	register_free_block(block_ptr);
}

size_t AllocatorHalfFitImpl::get_used_size_ver2(void) const
{
	size_t address_current = address_start;
	size_t size_used = 0;

	while (address_current != address_end)
	{
		MemBlock * block_ptr = (MemBlock *) address_current;
		if (block_ptr->ref_count > 0)
		{
			size_used += block_ptr->size;
		}
		address_current += block_ptr->size;
	}

	return size_used;
}

//============================== END OF IMPLEMENTATION ===========================





//============================== START OF UNIT TESTS =============================

void unit_test1(void)
{
	char mem_ptr[0x200];
	AllocatorHalfFit allocator;
	allocator.initialize(mem_ptr, sizeof(mem_ptr));

	size_t alloc_count = 10;
	void * ptr[alloc_count];

	for (size_t i = 0; i < alloc_count; i++)
	{
		ptr[i] = allocator.alloc(0x10);
	}

	for (size_t i = 0; i < alloc_count; i++)
	{
		if ((i & 0b1) == 1)
		allocator.free(ptr[i]);
	}

	for (size_t i = 0; i < alloc_count; i++)
	{
		if ((i & 0b1) == 0)
		allocator.free(ptr[i]);
	}
}

void AllocatorHalfFit::run_unit_tests(void)
{
	unit_test1();
}

//============================== END OF UNIT TESTS ===============================




//============================== START OF API ====================================

void AllocatorHalfFit::initialize(void * mem_ptr, size_t size)
{
	TX_ASSERT(!is_initialized());
	TX_ASSERT((address_start & (AllocatorHalfFitImpl::BLOCK_ALIGNMENT - 1)) == 0); // Ensure alignment
	TX_ASSERT((size & (AllocatorHalfFitImpl::BLOCK_ALIGNMENT - 1)) == 0);

	AllocatorHalfFitImpl * me = (AllocatorHalfFitImpl *) this;

	me->free_block_list_size = 1 + me->get_order_from_size(size - 1);
	me->free_block_list = (MemBlock **) mem_ptr;
	me->address_start = (size_t)mem_ptr + me->free_block_list_size * sizeof(size_t);
	me->address_start = me->next_aligned_address(me->address_start);
	me->address_end = (size_t)mem_ptr + size;

	TX_ASSERT(me->address_end > me->address_start && me->address_start > (size_t)mem_ptr);

	me->initialize_management_data();
}

void AllocatorHalfFit::uninitialize(void)
{
	TX_ASSERT(get_unused_size() == get_total_size()); // Allocated space is not freed (potential memory corruption)
	address_start = 0;
	address_end = 0;
}

void * AllocatorHalfFit::alloc(size_t content_size)
{
	TX_ASSERT(is_initialized());

	AllocatorHalfFitImpl * me = (AllocatorHalfFitImpl *) this;

	me->m_lock.acquire();

	void * result;
	result = me->allocate(content_size);

	me->m_lock.release();

	return result;
}

void AllocatorHalfFit::free(void * content_ptr)
{
	TX_ASSERT(is_initialized());

	AllocatorHalfFitImpl * me = (AllocatorHalfFitImpl *) this;

	me->m_lock.acquire();

	me->free(content_ptr);

	me->m_lock.release();
}

void AllocatorHalfFit::clear(void)
{
	TX_ASSERT(is_initialized());

	AllocatorHalfFitImpl * me = (AllocatorHalfFitImpl *) this;
	me->initialize_management_data();
}

size_t AllocatorHalfFit::get_unused_size(void)
{
	TX_ASSERT(is_initialized());

	size_t size_unused = 0;

	m_lock.acquire();

	for (size_t i = 0; i < free_block_list_size; i++)
	{
		MemBlock * block_ptr = free_block_list[i];
		while (block_ptr != nullptr)
		{
			size_unused += block_ptr->size;
			block_ptr = block_ptr->next_free_block;
		}
	}

	m_lock.release();

	return size_unused;
}

//============================== END OF API ======================================


