/*
 * tx_memory_halffit.hpp
 *
 *  Created on: Jan 29, 2024
 *      Author: tian_
 */

#pragma once

#include <stddef.h>
#include "tx_spinlock.hpp"

class AllocatorHalfFit
{
	//============================== START OF TYPEDEF =========================================

protected:

	struct MemBlock;

	//============================== END OF TYPEDEF ===========================================





	//============================== START OF MEMBERS =========================================

protected:

	MemBlock **					free_block_list;  			// Addresses of the free blocks
	size_t							free_block_list_size;  	// Number of addresses stored in the list

	size_t	   					address_start; // Start of memory pool (does not include the free block list)
	size_t							address_end;   // End of memory pool

	Spinlock						m_lock;

	//============================== END OF MEMBERS ===========================================




	//============================== START OF METHODS =========================================

public:
	static void run_unit_tests(void);


public:

	AllocatorHalfFit(void) noexcept : address_start(0), address_end(0) {}
	AllocatorHalfFit(AllocatorHalfFit const &) noexcept = delete;
	AllocatorHalfFit(AllocatorHalfFit &&) noexcept = delete;
	~AllocatorHalfFit(void) noexcept {uninitialize();}
	void operator=(AllocatorHalfFit const &) noexcept = delete;
	void operator=(AllocatorHalfFit &&) noexcept = delete;

	bool is_initialized(void) const {return (address_start != address_end);}
	void initialize(void * mem_ptr, size_t size) noexcept;
	void uninitialize(void) noexcept;

	void * alloc(size_t content_size) noexcept; // Reentrant
	void free(void * content_ptr) noexcept; // Reentrant
	void clear(void) noexcept;

	size_t get_total_size(void) const {return address_end - address_start;}
	size_t get_unused_size(void);
	size_t get_used_size(void) {return get_total_size() - get_unused_size();}

	//============================== END OF METHODS ===========================================
};
