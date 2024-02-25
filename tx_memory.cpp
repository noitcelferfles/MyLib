/*
 * tx_hfmem.cpp
 *
 *  Created on: Dec 24, 2023
 *      Author: tian_
 */



#include "tx_memory.hpp"

#include <stddef.h>
#include "tx_assert.h"
#include <stdint.h>
#include <atomic>

extern "C" {
#include "cmsis_compiler.h"
}



//============================== START OF IMPLEMENTATION ==================================


class __attribute__((packed)) LinAllocator::MemBlock
{
public:
	enum class State : size_t
	{
		Used = 0xF0F0F0F0,
		Free = 0xF0F0F0F1,
	};

public:
	State						state;
	size_t 					size;								// Size of the block including info segment
	char						content;						// Start of user content

};


class LinAllocatorImpl : public LinAllocator
{
public:

	static constexpr size_t const BLOCK_INFO_SIZE = sizeof(MemBlock) - sizeof(char);

	static constexpr size_t const MIN_ALLOC_SIZE_LOG2 = 2;
	static constexpr size_t const BYTE_PER_WORD_LOG2 = 2; static_assert((1u << BYTE_PER_WORD_LOG2) == sizeof(size_t));


public:

	static inline size_t blockptr_to_address(MemBlock const * block_ptr) {return (size_t) block_ptr;}
	static inline MemBlock * address_to_blockptr(size_t address) {return (MemBlock *) address;}

	MemBlock * find_next_block(MemBlock const * block_ptr);
	bool absorb_next_block_if_possible(MemBlock * block_ptr);
	void split_block_if_possible(MemBlock * block_ptr, size_t first_block_size);

public:

	size_t allocate(void ** content_ptr, size_t content_size);
	size_t free(void * content_ptr);
};

LinAllocator::MemBlock * LinAllocatorImpl::find_next_block(MemBlock const * block_ptr)
{
	size_t next_address = blockptr_to_address(block_ptr) + block_ptr->size;
	if (next_address == this->address_end)
	{
		next_address = this->address_start;
	}
	return address_to_blockptr(next_address);
}

bool LinAllocatorImpl::absorb_next_block_if_possible(MemBlock * block_ptr)
{
	//TX_ASSERT(block_ptr->state == MemBlock::State::FREE);
	size_t next_address = blockptr_to_address(block_ptr) + block_ptr->size;
	if (next_address == this->address_end) {return false;}
	MemBlock * next_block_ptr = address_to_blockptr(next_address);
	if (next_block_ptr->state != MemBlock::State::Free) {return false;}

	next_block_ptr->size += next_block_ptr->size;
	return true;
}

void LinAllocatorImpl::split_block_if_possible(MemBlock * block_ptr, size_t first_block_size)
{
	if (block_ptr->size >= first_block_size + BLOCK_INFO_SIZE + (1u << MIN_ALLOC_SIZE_LOG2))
	{
		MemBlock * new_block_ptr = address_to_blockptr(blockptr_to_address(block_ptr) + first_block_size);
		new_block_ptr->size = block_ptr->size - first_block_size;
		new_block_ptr->state = MemBlock::State::Free;
		block_ptr->size = first_block_size;
	}
}


size_t LinAllocatorImpl::allocate(void ** content_ptr, size_t content_size)
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
		if (search_block->state == MemBlock::State::Free)
		{
			while (this->absorb_next_block_if_possible(search_block))
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
	search_block->state = MemBlock::State::Used;
	*content_ptr = (void**) &search_block->content;
	this->next_search_block = search_block;

	return 0;
}

size_t LinAllocatorImpl::free(void * content_ptr)
{
	TX_ASSERT(this->is_initialized());

	MemBlock * block_ptr = address_to_blockptr((size_t) content_ptr - BLOCK_INFO_SIZE);

	if (block_ptr->state != MemBlock::State::Used)
	{
		return 1;
	}

	block_ptr->state = MemBlock::State::Free;
	return 0;
}

//============================== END OF IMPLEMENTATION ====================================



//============================== START OF API =============================================

void LinAllocator::initialize(void * mem_ptr, size_t size)
{
	LinAllocatorImpl * me = (LinAllocatorImpl *) this;
	size_t address_start = (size_t) mem_ptr;

	TX_ASSERT(!me->is_initialized());
	TX_ASSERT((address_start & (sizeof(size_t) - 1)) == 0);
	TX_ASSERT((size & (sizeof(size_t) - 1)) == 0);
	TX_ASSERT(address_start + size > address_start);
	TX_ASSERT(size >= me->BLOCK_INFO_SIZE + (1u << me->MIN_ALLOC_SIZE_LOG2));

	MemBlock * block_ptr = me->address_to_blockptr(address_start);
	block_ptr->state = MemBlock::State::Free;
	block_ptr->size = size;

	me->next_search_block = block_ptr;
	me->address_start = address_start;
	me->address_end = address_start + size;
}

size_t LinAllocator::alloc(void ** content_ptr, size_t content_size)
{
	LinAllocatorImpl * me = (LinAllocatorImpl *) this;
	__disable_irq();
	__DSB();
	size_t result = me->allocate(content_ptr, content_size);
	__enable_irq();
	return result;
}

size_t LinAllocator::free(void * content_ptr)
{
	LinAllocatorImpl * me = (LinAllocatorImpl *) this;
	__disable_irq();
	__DSB();
	size_t result = me->free(content_ptr);
	__enable_irq();
	return result;
}

//============================== END OF API ===============================================




//============================== START OF IMPLEMENTATION ==================================


struct AllocatorSeqFit::MemBlock
{
	size_t 									size;								// Size of the block including info segment
	std::atomic<size_t>			ref_count;					// Number of ptr to this block; this number being zero means this block is free
	size_t									content;						// Start of user content
};


class AllocatorSeqFitImpl : public AllocatorSeqFit
{
public:

	static constexpr size_t const BLOCK_INFO_SIZE = sizeof(MemBlock) - sizeof(size_t); static_assert(BLOCK_INFO_SIZE == 2 * sizeof(size_t));

	static constexpr size_t const MIN_ALLOC_SIZE_LOG2 = 2;
	static constexpr size_t const BYTE_PER_WORD_LOG2 = 2; static_assert((1u << BYTE_PER_WORD_LOG2) == sizeof(size_t));

	static constexpr size_t const BLOCK_REF_COUNT_TRAVERSAL = (size_t)(-1); // This special ref_count means that the block is free and is currently considered for allocation by a thread


public:

	static inline size_t blockptr_to_address(MemBlock const * block_ptr) {return (size_t) block_ptr;}
	static inline MemBlock * address_to_blockptr(size_t address) {return (MemBlock *) address;}

	MemBlock * find_next_block(MemBlock const * block_ptr);
	bool absorb_next_block_if_possible(MemBlock * block_ptr);
	void split_block_if_possible(MemBlock * block_ptr, size_t first_block_size);

public:

	size_t allocate(void ** content_ptr, size_t content_size);
};

AllocatorSeqFit::MemBlock * AllocatorSeqFitImpl::find_next_block(MemBlock const * block_ptr)
{
	size_t next_address = blockptr_to_address(block_ptr) + block_ptr->size;
	if (next_address == this->address_end)
	{
		next_address = this->address_start;
	}
	return address_to_blockptr(next_address);
}

bool AllocatorSeqFitImpl::absorb_next_block_if_possible(MemBlock * block_ptr)
{
	//TX_ASSERT(block_ptr->ref_count.load(std::memory_order_relaxed) == 0);
	size_t next_address = blockptr_to_address(block_ptr) + block_ptr->size;
	if (next_address == this->address_end) {return false;}
	MemBlock * next_block_ptr = address_to_blockptr(next_address);
	if (next_block_ptr->ref_count.load(std::memory_order_relaxed) > 0) {return false;}

	next_block_ptr->size += next_block_ptr->size;
	return true;
}

void AllocatorSeqFitImpl::split_block_if_possible(MemBlock * block_ptr, size_t first_block_size)
{
	if (block_ptr->size >= first_block_size + BLOCK_INFO_SIZE + (1u << MIN_ALLOC_SIZE_LOG2))
	{
		MemBlock * new_block_ptr = address_to_blockptr(blockptr_to_address(block_ptr) + first_block_size);
		new_block_ptr->size = block_ptr->size - first_block_size;
		new_block_ptr->ref_count.store(0, std::memory_order_relaxed);
		block_ptr->size = first_block_size;
	}
}


size_t AllocatorSeqFitImpl::allocate(void ** content_ptr, size_t content_size)
{
	TX_ASSERT(this->is_initialized());

	// Adjust the allocation size to the nearest valid number
	if (content_size < (1u << MIN_ALLOC_SIZE_LOG2))
	{
		content_size = 1u << MIN_ALLOC_SIZE_LOG2;
	}
	content_size = (((content_size - 1) >> BYTE_PER_WORD_LOG2) + 1) << BYTE_PER_WORD_LOG2;

	size_t block_size = content_size + BLOCK_INFO_SIZE;
	size_t search_distance = 0;
	MemBlock * search_block = this->next_search_block;

	while (1)
	{
		if (search_block->ref_count.load(std::memory_order_relaxed) == 0)
		{
			while (absorb_next_block_if_possible(search_block));
			if (search_block->size >= block_size)
			{
				this->split_block_if_possible(search_block, block_size);
				break;
			}
		}

		search_distance += search_block->size;
		if (search_distance >= this->address_end - this->address_start) {return 1;}
		search_block = this->find_next_block(search_block);
	}

	// At this point, $search_block is a pointer to a suitable block for allocation
	search_block->ref_count.store(1, std::memory_order_relaxed);
	*content_ptr = (void**) &search_block->content;
	this->next_search_block = search_block;

	return 0;
}

//============================== END OF IMPLEMENTATION ====================================



//============================== START OF API =============================================

void AllocatorSeqFit::initialize(void * mem_ptr, size_t size)
{
	AllocatorSeqFitImpl * me = (AllocatorSeqFitImpl *) this;
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
}

void * AllocatorSeqFit::alloc(size_t content_size)
{
	AllocatorSeqFitImpl * me = (AllocatorSeqFitImpl *) this;

	__disable_irq();
	__DMB();
	std::atomic_signal_fence(std::memory_order_acquire);

	void * result;
	me->allocate(&result, content_size);

	std::atomic_signal_fence(std::memory_order_release);
	__DMB();
	__enable_irq();

	return result;
}

void AllocatorSeqFit::free(void * content_ptr)
{
	MemBlock * block_ptr = AllocatorSeqFitImpl::address_to_blockptr((size_t) content_ptr - AllocatorSeqFitImpl::BLOCK_INFO_SIZE);
	TX_ASSERT(block_ptr->ref_count == 1);
	block_ptr->ref_count.fetch_sub(1, std::memory_order_release);	// Ensure completion of all memory operations to the (potentially freed) block
}

//============================== END OF API ===============================================
