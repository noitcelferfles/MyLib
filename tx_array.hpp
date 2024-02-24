/*
 * tx_array.hpp
 *
 *  Created on: Jan 29, 2024
 *      Author: tian_
 */

#pragma once

#include <stddef.h>
#include <utility>
#include <new>
#include "tx_assert.h"

namespace TXLib
{

// Static array with constant-time access, insertion, removal
template <typename Type, size_t const CAPACITY>
class Array
{
private:
	Type						m_array[CAPACITY];
	size_t					m_size;

public:

	Array(void) noexcept : m_size(0) {}
	Array(Array<Type, CAPACITY> const &) = delete;
	Array(Array<Type, CAPACITY> &&) = delete;
	void operator=(Array<Type, CAPACITY> const &) = delete;
	void operator=(Array<Type, CAPACITY> &&) = delete;

	size_t get_size(void) const {return m_size;}

	Type & operator[](size_t index)
	{
		TX_ASSERT(index < m_size);
		return m_array[index];
	}

	Type const & operator[](size_t index) const
	{
		TX_ASSERT(index < m_size);
		return m_array[index];
	}

	Type & get_last_item(void)
	{
		TX_ASSERT(m_size > 0);
		return m_array[m_size - 1];
	}

	Type const & get_last_item(void) const
	{
		TX_ASSERT(m_size > 0);
		return m_array[m_size - 1];
	}

	void push_back(Type const & item)
	{
		TX_ASSERT(m_size < CAPACITY);
		m_array[m_size] = item;
		m_size ++;
	}

	// Add an uninitialized object
	// Return the pointer to the uninitialized object
	Type & push_back(void)
	{
		TX_ASSERT(m_size < CAPACITY);
		m_size ++;
		return m_array[m_size-1];
	}

	// Return popped object
	Type pop_item_at(size_t index)
	{
		TX_ASSERT(index < m_size);
		Type temp = std::move(m_array[index]);
		m_size--;
		m_array[index] = std::move(m_array[m_size]);
		return temp;
	}
	Type pop_back(void)
	{
		TX_ASSERT(0 < m_size);
		m_size--;
		return std::move(m_array[m_size]);
	}

	void clear(void)
	{
		m_size = 0;
	}
};



// Dynamic array with constant-time access, insertion, removal
template <typename Type>
class DynamicArray
{
public:
	typedef				void * (*Alloc)(size_t);
	typedef				void (*Free)(void *);


private:
	Type *				m_array;
	Type *				m_array_backup; // Half the size of m_array
	// m_array_backup is kept to avoid copying the entire array when allocating new space
	// Data is copied from m_array_backup to m_array during insertions
	// The index-i element is stored in exactly one of the two arrays

	size_t				m_size; // Size used by the user

	// 2^(m_capacity_log2) + m_capacity_add is the maximum number of constructed Type
	// This number determines which array is used to store the index-i element
	size_t				m_capacity_log2;
	size_t				m_capacity_add;

	Alloc					m_alloc;
	Free					m_free;


private:
	inline bool use_backup_array(size_t index) const
	{
		size_t mask = (1u << m_capacity_log2) - 1;
		return (index & mask) >= m_capacity_add;
	}

	Type * get_index_ptr(size_t index) const
	{
		return use_backup_array(index) ? (m_array_backup + index) : (m_array + index);
	}

	void grow_capacity(void)
	{
		::new(m_array + m_capacity_add) Type(std::move(m_array_backup[m_capacity_add]));
		m_array_backup[m_capacity_add].~Type();
		m_capacity_add++;
		if (m_capacity_add == (1u << m_capacity_log2)) // Allocate if necessary
		{
			m_free(m_array_backup);
			m_array_backup = m_array;
			m_array = (Type *) m_alloc((m_capacity_add << 2u) * sizeof(Type));
			m_capacity_add = 0;
			m_capacity_log2++;
		}
	}


public:

	DynamicArray(void) noexcept : m_array(nullptr) {}
	DynamicArray(DynamicArray<Type> const &) = delete;
	DynamicArray(DynamicArray<Type> &&) = delete;
	DynamicArray(Alloc alloc, Free free, size_t capacity_log2) {initialize(alloc, free, capacity_log2);}
	void operator=(DynamicArray<Type> const &) = delete;
	void operator=(DynamicArray<Type> &&) = delete;

	bool is_initialized(void) const {return m_array != nullptr;}

	void initialize(Alloc alloc, Free free, size_t capcity_log2)
	{
		TX_ASSERT(!is_initialized());

		m_size = 0;
		m_capacity_log2 = capcity_log2;
		m_capacity_add = 0;
		m_alloc = alloc;
		m_free = free;

		// Allocate raw memory
		m_array_backup = (Type *) m_alloc((1u << m_capacity_log2) * sizeof(Type));
		m_array = (Type *) m_alloc((1u << (m_capacity_log2 + 1)) * sizeof(Type));
	}

	void uninitialize(void)
	{
		if (!is_initialized()) {return;}

		for (size_t i = 0; i < m_size; i++)
		{
			get_index_ptr(i)->~Type();
		}
		m_free(m_array);
		m_free(m_array_backup);
		m_array = nullptr;
	}

	~DynamicArray(void) noexcept
	{
		uninitialize();
	}

	size_t get_size(void) const {return m_size;}
	size_t get_capacity(void) const {return (1u << m_capacity_log2) + m_capacity_add;}

	Type & operator[](size_t index)
	{
		TX_ASSERT(index < m_size);
		return *get_index_ptr(index);
	}

	Type const & operator[](size_t index) const
	{
		TX_ASSERT(index < m_size);
		return *get_index_ptr(index);
	}

	Type & get_last_item(void)
	{
		TX_ASSERT(m_size > 0);
		return *get_index_ptr(m_size - 1);
	}

	Type const & get_last_item(void) const
	{
		TX_ASSERT(m_size > 0);
		return *get_index_ptr(m_size - 1);
	}

	// Constant-time, constant number of memory allocation/free
	template <typename ... Args>
	Type & push_back(Args && ... args)
	{
		TX_ASSERT(is_initialized());
		if (m_size >= get_capacity())
		{
			grow_capacity();
		}
		Type * ptr = ::new(get_index_ptr(m_size)) Type(std::forward<Args>(args) ...); static_assert(noexcept(Type(std::forward<Args>(args) ...)));
		m_size++;
		return *ptr;
	}

	// Contant-time, no allocation
	Type pop_item_at(size_t index)
	{
		TX_ASSERT(index < m_size);
		Type temp = std::move(*get_index_ptr(index));
		m_size--;
		*get_index_ptr(index) = std::move(*get_index_ptr(m_size));
		get_index_ptr(m_size)->~Type();
		return temp;
	}
	Type pop_back(void)
	{
		TX_ASSERT(0 < m_size);
		m_size--;
		Type temp = std::move(*get_index_ptr(m_size));
		get_index_ptr(m_size)->~Type();
		return temp;
	}

	void clear(void)
	{
		for (size_t i = 0; i < m_size; i++)
		{
			get_index_ptr(i)->~Type();
		}
		m_size = 0;
	}

};


// Dynamic array with amortized constant-time access, insertion, removal
template <typename Type>
class LightDynamicArray
{
public:
	typedef				void * (*Alloc)(size_t);
	typedef				void (*Free)(void *);


private:
	Type *				m_array;

	size_t				m_size; // Size used by the user

	size_t				m_capacity_log2;

	Alloc					m_alloc;
	Free					m_free;


private:

	void grow_capacity(void)
	{
		m_capacity_log2 ++;
		Type * array = (Type *) m_alloc((1u << m_capacity_log2) * sizeof(Type));
		for (size_t i = 0; i < m_size; i++)
		{
			::new(array + i) Type(std::move(m_array[i]));
			m_array[i].~Type();
		}
		m_free(m_array);
		m_array = array;
	}

public:

	LightDynamicArray(void) noexcept : m_array(nullptr) {}
	LightDynamicArray(LightDynamicArray<Type> const &) = delete;
	LightDynamicArray(LightDynamicArray<Type> &&) = delete;
	LightDynamicArray(Alloc alloc, Free free, size_t capacity_log2) {initialize(alloc, free, capacity_log2);}
	void operator=(LightDynamicArray<Type> const &) = delete;
	void operator=(LightDynamicArray<Type> &&) = delete;

	bool is_initialized(void) const {return m_array != nullptr;}

	void initialize(Alloc alloc, Free free, size_t capcity_log2)
	{
		TX_ASSERT(!is_initialized());

		m_size = 0;
		m_capacity_log2 = capcity_log2;
		m_alloc = alloc;
		m_free = free;

		// Allocate raw memory
		m_array = (Type *) m_alloc((1u << m_capacity_log2) * sizeof(Type));
	}

	void uninitialize(void)
	{
		if (!is_initialized()) {return;}

		for (size_t i = 0; i < m_size; i++)
		{
			m_array[i].~Type();
		}
		m_free(m_array);
		m_array = nullptr;
	}

	~LightDynamicArray(void) noexcept
	{
		uninitialize();
	}

	size_t get_size(void) const {return m_size;}
	size_t get_capacity(void) const {return 1u << m_capacity_log2;}

	Type & operator[](size_t index) {TX_ASSERT(index < m_size); return m_array[index];}

	Type const & operator[](size_t index) const {TX_ASSERT(index < m_size); return m_array[index];}

	Type & get_last_item(void) {TX_ASSERT(m_size > 0); return m_array[m_size - 1];}

	Type const & get_last_item(void) const {TX_ASSERT(m_size > 0); return m_array[m_size - 1];}

	template <typename... Args>
	Type & push_back(Args && ... args)
	{
		TX_ASSERT(is_initialized());
		if (m_size >= (1u << m_capacity_log2))
		{
			grow_capacity();
		}
		Type * ptr = ::new(m_array + m_size) Type(std::forward<Args>(args) ...); static_assert(noexcept(Type(std::forward<Args>(args) ...)));
		m_size ++;
		return *ptr;
	}

	Type pop_item_at(size_t index)
	{
		TX_ASSERT(index < m_size);
		Type temp = std::move(m_array[index]);
		m_size--;
		m_array[index] = std::move(m_array[m_size]);
		m_array[m_size].~Type();
		return temp;
	}
	Type pop_back(void)
	{
		TX_ASSERT(0 < m_size);
		m_size--;
		Type temp = std::move(m_array[m_size]);
		m_array[m_size].~Type();
		return temp;
	}

	void clear(void)
	{
		for (size_t i = 0; i < m_size; i++)
		{
			m_array[i].~Type();
		}
		m_size = 0;
	}

};


}
