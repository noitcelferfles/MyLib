/*
 * tx_automemory.cpp
 *
 *  Created on: Jan 18, 2024
 *      Author: tian_
 */


#include "tx_automemory.hpp"

#include <stddef.h>
#include <atomic>
#include "tx_assert.h"
#include <stdint.h>



//============================== START OF IMPLEMENTATION ==================================


struct AutoLinAlloc::MemBlock
{
	size_t 									size;								// Size of the block including info segment
	std::atomic<size_t>			ref_count;					// Number of ptr to this block; this number being zero means this block is free
	char										content;						// Start of user content
};


class AutoLinAllocImpl : public AutoLinAlloc
{
public:

	static constexpr size_t const BLOCK_INFO_SIZE = sizeof(MemBlock) - sizeof(char);

	static constexpr size_t const MIN_ALLOC_SIZE_LOG2 = 2;
	static constexpr size_t const BYTE_PER_WORD_LOG2 = 2; static_assert((1u << BYTE_PER_WORD_LOG2) == sizeof(size_t));

	static constexpr size_t const BLOCK_REF_COUNT_TRAVERSAL = (size_t)(-1); // This special ref_count means that the block is free and is currently considered for allocation by a thread


public:

	static inline size_t blockptr_to_address(MemBlock const * block_ptr) {return (size_t) block_ptr;}
	static inline MemBlock * address_to_blockptr(size_t address) {return (MemBlock *) address;}

	MemBlock * find_next_block(MemBlock const * block_ptr);
	size_t get_contiguous_free_size(MemBlock const * block_ptr);
	void split_block_if_possible(MemBlock * block_ptr, size_t first_block_size);

public:

	size_t allocate(void ** content_ptr, size_t content_size);
};

AutoLinAlloc::MemBlock * AutoLinAllocImpl::find_next_block(MemBlock const * block_ptr)
{
	size_t next_address = blockptr_to_address(block_ptr) + block_ptr->size;
	if (next_address == this->address_end)
	{
		next_address = this->address_start;
	}
	return address_to_blockptr(next_address);
}

size_t AutoLinAllocImpl::get_contiguous_free_size(MemBlock const * block_ptr)
{
	size_t end_address = blockptr_to_address(block_ptr);
	while (end_address != this->address_end)
	{
		MemBlock * current_block_ptr = address_to_blockptr(end_address);
		if (current_block_ptr->ref_count.load(std::memory_order_relaxed) == 0)
		{
			end_address += current_block_ptr->size;
		}
		else
		{
			break;
		}
	}
	return end_address - blockptr_to_address(block_ptr);
}


void AutoLinAllocImpl::split_block_if_possible(MemBlock * block_ptr, size_t first_block_size)
{
	if (block_ptr->size >= first_block_size + BLOCK_INFO_SIZE + (1u << MIN_ALLOC_SIZE_LOG2))
	{
		MemBlock * new_block_ptr = address_to_blockptr(blockptr_to_address(block_ptr) + first_block_size);
		new_block_ptr->size = block_ptr->size - first_block_size;
		new_block_ptr->ref_count.store(0, std::memory_order_relaxed);
		block_ptr->size = first_block_size;
	}
}


size_t AutoLinAllocImpl::allocate(void ** content_ptr, size_t content_size)
{
	TX_ASSERT(this->is_initialized());

	// Adjust the allocation size to the nearest valid number
	if (content_size < (1u << MIN_ALLOC_SIZE_LOG2))
	{
		content_size = 1u << MIN_ALLOC_SIZE_LOG2;
	}
	content_size = (((content_size - 1) >> BYTE_PER_WORD_LOG2) + 1) << BYTE_PER_WORD_LOG2;

	size_t block_size = content_size + BLOCK_INFO_SIZE;
	MemBlock * search_block = this->next_search_block;

	while (1)
	{
		if (search_block->ref_count.load(std::memory_order_relaxed) == 0)
		{
			search_block->size = this->get_contiguous_free_size(search_block);
			if (search_block->size >= block_size)
			{
				this->split_block_if_possible(search_block, block_size);
				break;
			}
		}

		search_block = this->find_next_block(search_block);

		if (blockptr_to_address(search_block) <= blockptr_to_address(this->next_search_block)
				&& blockptr_to_address(search_block) + search_block->size > blockptr_to_address(this->next_search_block))
		{
			return 1;
		}
	}

	// At this point, $search_block is a pointer to a suitable block for allocation
	search_block->ref_count.store(1, std::memory_order_relaxed);
	*content_ptr = (void**) &search_block->content;
	this->next_search_block = search_block;

	return 0;
}


void AutoLinAlloc::SharedPtr::increase_ref_count(void) const
{
	TX_ASSERT(this->mem_ptr != nullptr);
	MemBlock * block_ptr = AutoLinAllocImpl::address_to_blockptr((size_t) mem_ptr - AutoLinAllocImpl::BLOCK_INFO_SIZE);
	block_ptr->ref_count.fetch_add(1, std::memory_order_relaxed);
}

void AutoLinAlloc::SharedPtr::decrease_ref_count(void) const
{
	TX_ASSERT(this->mem_ptr != nullptr);
	MemBlock * block_ptr = AutoLinAllocImpl::address_to_blockptr((size_t) mem_ptr - AutoLinAllocImpl::BLOCK_INFO_SIZE);
	block_ptr->ref_count.fetch_sub(1, std::memory_order_release);	// Ensure completion of all memory operations to the (potentially freed) block
}

size_t AutoLinAlloc::SharedPtr::get_size(void) const
{
	if (this->mem_ptr == nullptr)
	{
		return 0;
	}
	else
	{
		MemBlock * block_ptr = AutoLinAllocImpl::address_to_blockptr((size_t) mem_ptr - AutoLinAllocImpl::BLOCK_INFO_SIZE);
		return block_ptr->size - AutoLinAllocImpl::BLOCK_INFO_SIZE;
	}
}

size_t AutoLinAlloc::SharedPtr::get_ref_count(void) const
{
	if (this->mem_ptr == nullptr)
	{
		return 0;
	}
	else
	{
		MemBlock * block_ptr = AutoLinAllocImpl::address_to_blockptr((size_t) mem_ptr - AutoLinAllocImpl::BLOCK_INFO_SIZE);
		return block_ptr->ref_count;
	}
}

//============================== END OF IMPLEMENTATION ====================================



//============================== START OF API =============================================

void AutoLinAlloc::initialize(void * mem_ptr, size_t size)
{
	AutoLinAllocImpl * me = (AutoLinAllocImpl *) this;
	size_t address_start = (size_t) mem_ptr;

	TX_ASSERT(!me->is_initialized());
	TX_ASSERT((address_start & (sizeof(size_t) - 1)) == 0);
	TX_ASSERT((size & (sizeof(size_t) - 1)) == 0);
	TX_ASSERT(address_start + size > address_start);
	TX_ASSERT(size >= me->BLOCK_INFO_SIZE + (1u << me->MIN_ALLOC_SIZE_LOG2));

	MemBlock * block_ptr = me->address_to_blockptr(address_start);
	block_ptr->ref_count = 0;
	block_ptr->size = size;

	me->next_search_block = block_ptr;
	me->address_start = address_start;
	me->address_end = address_start + size;
	me->allocation_lock.store(false, std::memory_order_relaxed);
}

AutoLinAlloc::SharedPtr AutoLinAlloc::alloc(size_t content_size)
{
	AutoLinAllocImpl * me = (AutoLinAllocImpl *) this;


//	__disable_irq();
//	__DSB();

	while (!me->allocation_lock.exchange(true, std::memory_order_acquire));

	SharedPtr result;
	me->allocate(&result.mem_ptr, content_size);

	me->allocation_lock.store(false, std::memory_order_release);

//	__enable_irq();

	return result;
}

//============================== END OF API ===============================================
