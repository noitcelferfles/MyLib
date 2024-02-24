/*
 * tx_hfmem.h
 *
 *  Created on: Dec 24, 2023
 *      Author: tian_
 */

#pragma once

#include <stddef.h>


class LinAllocator
{
	//============================== START OF TYPEDEF =========================================

protected:

	class MemBlock;

	//============================== END OF TYPEDEF ===========================================





	//============================== START OF MEMBERS =========================================

protected:

	MemBlock *			next_search_block;

	size_t   				address_start; // Start of memory pool
	size_t					address_end;   // End of memory pool

	//============================== END OF MEMBERS ===========================================




	//============================== START OF METHODS =========================================

public:

	inline LinAllocator(void) : address_start(0), address_end(0) {}

	void initialize(void * mem_ptr, size_t size);
	size_t alloc(void ** content_ptr, size_t content_size);
	size_t free(void * content_ptr);

	inline bool is_initialized(void) const {return (address_start != address_end);}

	//============================== END OF METHODS ===========================================
};


class AllocatorSeqFit
{
	//============================== START OF TYPEDEF =========================================

protected:

	struct MemBlock;

	//============================== END OF TYPEDEF ===========================================





	//============================== START OF MEMBERS =========================================

protected:

	MemBlock *			next_search_block;

	size_t   				address_start; // Start of memory pool
	size_t					address_end;   // End of memory pool

	//============================== END OF MEMBERS ===========================================




	//============================== START OF METHODS =========================================

public:

	inline AllocatorSeqFit(void) : address_start(0), address_end(0) {}

	void initialize(void * mem_ptr, size_t size);
	void * alloc(size_t content_size);
	void free(void * content_ptr);

	inline bool is_initialized(void) const {return (address_start != address_end);}

	//============================== END OF METHODS ===========================================
};
