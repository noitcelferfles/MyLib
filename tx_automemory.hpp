/*
 * tx_automemory.hpp
 *
 *  Created on: Jan 18, 2024
 *      Author: tian_
 */

// Memory allocator that self-manages garbage recycling
// The memory location is freed once all shared pointers are dtored

#pragma once

#include <stddef.h>
#include <atomic>


class AutoLinAlloc
{
	//============================== START OF TYPEDEF =========================================

protected:

	struct MemBlock;

public:

	class SharedPtr
	{
		friend class AutoLinAlloc;
	private:
		void *		mem_ptr;

	private:
		void increase_ref_count(void) const;
		void decrease_ref_count(void) const;

	public:
		SharedPtr(void) : mem_ptr(nullptr) {}
		SharedPtr(SharedPtr const & b)
		{
			if (b.mem_ptr != nullptr) {b.increase_ref_count();}
			this->mem_ptr = b.mem_ptr;
		}
		SharedPtr(SharedPtr && b)
		{
			this->mem_ptr = b.mem_ptr;
			b.mem_ptr = nullptr;
		}
		~SharedPtr(void)
		{
			if (mem_ptr != nullptr) {decrease_ref_count();}
		}
		void operator=(SharedPtr const & b)
		{
			if (b.mem_ptr != nullptr) {b.increase_ref_count();}
			if (this->mem_ptr != nullptr) {this->decrease_ref_count();}
			this->mem_ptr = b.mem_ptr;
		}
		void operator=(SharedPtr && b)
		{
			if (this->mem_ptr != nullptr) {this->decrease_ref_count();}
			this->mem_ptr = b.mem_ptr;
		}
		void swap(SharedPtr & b)
		{
			this->mem_ptr = (void *)((size_t) this->mem_ptr ^ (size_t) b.mem_ptr);
			b.mem_ptr = (void *)((size_t) this->mem_ptr ^ (size_t) b.mem_ptr);
			this->mem_ptr = (void *)((size_t) this->mem_ptr ^ (size_t) b.mem_ptr);
		}
		bool operator==(SharedPtr const & b) {return this->mem_ptr == b.mem_ptr;}
		bool operator!=(SharedPtr const & b) {return this->mem_ptr != b.mem_ptr;}


		bool is_allocated(void) const {return mem_ptr != nullptr;}
		void * get_ptr(void) const {return mem_ptr;}
		size_t get_size(void) const;  // Return 0 if not allocated
		size_t get_ref_count(void) const;  // Return 0 if not allocated
	};

	//============================== END OF TYPEDEF ===========================================





	//============================== START OF MEMBERS =========================================

protected:

	MemBlock *						next_search_block;
	size_t   							address_start; // Start of memory pool
	size_t								address_end;   // End of memory pool
	std::atomic<bool>			allocation_lock;

	//============================== END OF MEMBERS ===========================================




	//============================== START OF METHODS =========================================

public:

	AutoLinAlloc(void) : address_start(0), address_end(0) {}

	bool is_initialized(void) const {return (address_start != address_end);}

	void initialize(void * mem_ptr, size_t size);
	SharedPtr alloc(size_t content_size);

	//============================== END OF METHODS ===========================================



};
