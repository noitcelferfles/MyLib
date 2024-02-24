/*
 * tx_heap.hpp
 *
 *  Created on: Jan 29, 2024
 *      Author: tian_
 */

#pragma once

#include <stddef.h>
#include <utility>
#include "tx_assert.h"

namespace TXLib
{

// Max heap structure
// The top is larger or equal to every other item
// Inserted item will only replace the top if it is strictly larger than the top
template <typename Type, bool is_larger_or_equal(Type const & a, Type const & b), size_t CAPACITY>
class Heap
{
private:
	Type 				m_heap[CAPACITY];
	size_t	 		m_size;


public:

	Heap(void) noexcept : m_size(0) {}
	~Heap(void) noexcept = default;
	Heap(Heap<Type, is_larger_or_equal, CAPACITY> const &) = delete;
	Heap(Heap<Type, is_larger_or_equal, CAPACITY> &&) = delete;
	void operator=(Heap<Type, is_larger_or_equal, CAPACITY> const &) = delete;
	void operator=(Heap<Type, is_larger_or_equal, CAPACITY> &&) = delete;

	Type const & get_top(void) const
	{
		TX_ASSERT(m_size > 0);
		return m_heap[0];
	}

	Type & get_top(void)
	{
		TX_ASSERT(m_size > 0);
		return m_heap[0];
	}

	size_t get_size(void) const {return m_size;}



	Type pop_top(void)
	{
		TX_ASSERT(m_size > 0);
		m_size--;
		Type top = std::move(m_heap[0]);

		size_t index_dst = 0;
		size_t index_src;
		size_t index;

		do
		{
			index_src = m_size;
			index = index_dst * 2 + 2;
			if (index < m_size && !is_larger_or_equal(m_heap[index_src], m_heap[index]))
			{
				index_src = index;
			}
			index = index_dst * 2 + 1;
			if (index < m_size && !is_larger_or_equal(m_heap[index_src], m_heap[index]))
			{
				index_src = index;
			}

			m_heap[index_dst] = std::move(m_heap[index_src]);
			index_dst = index_src;
		}
		while (index_src != m_size);

		return top;
	}

	void insert(Type const & item)
	{
		TX_ASSERT(m_size < CAPACITY);

		size_t index_current = m_size;
		m_size ++;

		while (index_current != 0)
		{
			size_t index_swap = (index_current - 1) >> 1u;

			if (is_larger_or_equal(m_heap[index_swap], item))
			{
				break;
			}

			m_heap[index_current] = std::move(m_heap[index_swap]);
			index_current = index_swap;
		}
		m_heap[index_current] = item;
	}

	Type replace_top(Type const & item)
	{
		m_heap[m_size] = item;
		m_size++;
		return pop_top();
	}

};



template <typename Type, bool is_larger_or_equal(Type const & a, Type const & b)>
class DynamicHeap
{
public:
	typedef				void * (*Alloc)(size_t);
	typedef				void (*Free)(void *);

private:
	Type *			m_heap;
	size_t	 		m_size;
	size_t			m_capacity_log2;

	Alloc				m_alloc;
	Free				m_free;

private:

	size_t parent_index(size_t index) const {return (index - 1) >> 1u;}
	size_t child_index(size_t index) const {return 2 * index + 1;}

	void grow_capacity(void)
	{
		m_capacity_log2 ++;
		Type * heap = (Type *) m_alloc((1u << m_capacity_log2) * sizeof(Type));
		for (size_t i = 0; i < m_size; i++)
		{
			::new(heap + i) Type(std::move(m_heap[i]));
			m_heap[i].~Type();
		}
		m_free(m_heap);
		m_heap = heap;
	}

	void insert_and_heapify_up(Type & item, size_t index_hole)
	/* Place @member at index @index_hole of the heap, and push it up until it is smaller than its parent
	 * The element at index @index_hole is assumed to be destructed
	 * The class instance @member will be moved, but will not be destructed
	 */
	{
		while (index_hole != 0)
		{
			size_t index_swap = parent_index(index_hole);
			if (is_larger_or_equal(m_heap[index_swap], item)) {break;}

			::new(m_heap + index_hole) Type(std::move(m_heap[index_swap])); static_assert(noexcept(Type(std::move(m_heap[index_swap]))));
			m_heap[index_swap].~Type();
			index_hole = index_swap;
		}
		::new(m_heap + index_hole) Type(std::move(item));
	}

	void insert_and_heapify_down(Type & item, size_t index_hole)
	/* Place @member at index @index_hole of the heap, and push it down until it is larger than its children
	 * The element at index @index_hole is assumed to be destructed
	 * The class instance @member will be moved, but will not be destructed
	 */
	{
		while (2 * index_hole + 2 < m_size)
		{
			size_t index_swap = is_larger_or_equal(m_heap[child_index(index_hole) + 1], m_heap[child_index(index_hole)]) ? child_index(index_hole) + 1 : child_index(index_hole);
			if (is_larger_or_equal(item, m_heap[index_swap])) {break;}

			::new(m_heap + index_hole) Type(std::move(m_heap[index_swap])); static_assert(noexcept(Type(std::move(m_heap[index_swap]))));
			m_heap[index_swap].~Type();
			index_hole = index_swap;
		}

		if (child_index(index_hole) < m_size)
		{
			size_t index_swap = child_index(index_hole);
			if (!is_larger_or_equal(item, m_heap[index_swap]))
			{
				::new(m_heap + index_hole) Type(std::move(m_heap[index_swap]));
				m_heap[index_swap].~Type();
				index_hole = index_swap;
			}
		}
		::new(m_heap + index_hole) Type(std::move(item));
	}

public:

	DynamicHeap(void) noexcept : m_heap(nullptr) {}
	~DynamicHeap(void) noexcept {uninitialize();}
	DynamicHeap(DynamicHeap<Type, is_larger_or_equal> const &) = delete;
	DynamicHeap(DynamicHeap<Type, is_larger_or_equal> &&) = delete;
	void operator=(DynamicHeap<Type, is_larger_or_equal> const &) = delete;
	void operator=(DynamicHeap<Type, is_larger_or_equal> &&) = delete;

	bool is_initialized(void) const {return m_heap != nullptr;}

	void initialize(Alloc alloc, Free free, size_t capcity_log2)
	{
		TX_ASSERT(!is_initialized());

		m_size = 0;
		m_capacity_log2 = capcity_log2;
		m_alloc = alloc;
		m_free = free;

		// Allocate raw memory
		m_heap = (Type *) m_alloc((1u << m_capacity_log2) * sizeof(Type));
	}

	void uninitialize(void)
	{
		if (!is_initialized()) {return;}

		for (size_t i = 0; i < m_size; i++)
		{
			m_heap[i].~Type();
		}
		m_free(m_heap);
		m_heap = nullptr;
	}

	Type const & get_top(void) const
	{
		TX_ASSERT(m_size > 0);
		return m_heap[0];
	}
	Type & get_top(void)
	{
		TX_ASSERT(m_size > 0);
		return m_heap[0];
	}

	size_t get_size(void) const {return m_size;}

	Type pop_top(void)
	{
		TX_ASSERT(m_size > 0);
		m_size--;
		Type top = std::move(m_heap[0]);
		m_heap[0].~Type();

		insert_and_heapify_down(m_heap[m_size], 0);
		m_heap[m_size].~Type();

		return top;
	}

	template <typename... Args>
	void insert(Args && ... args)
	{
		if (m_size >= (1u << m_capacity_log2))
		{
			grow_capacity();
		}

		Type item = Type(std::forward<Args>(args) ...);

		m_size ++;
		insert_and_heapify_up(item, m_size - 1);
	}

	template <typename... Args>
	Type replace_top(Args && ... args)
	{
		Type top = std::move(m_heap[0]);
		m_heap[0].~Type();

		Type member = Type(std::forward<Args>(args) ...);
		add_and_heapify_down(member, 0);

		return top;
	}

	bool remove(Type const & object)
	// Remove an element of the heap equal to @object
	// Return true if a matching element is found and removed
	{
		for (size_t i = 0; i < m_size; i++)
		{
			if (m_heap[i] == object)
			{
				m_size--;
				m_heap[i].~Type();
				if (i == 0 || !is_larger_or_equal(m_heap[m_size], m_heap[parent_index(i)]))
				{
					insert_and_heapify_down(m_heap[m_size], i);
				}
				else
				{
					insert_and_heapify_up(m_heap[m_size], i);
				}
				m_heap[m_size].~Type();
				return true;
			}
		}
		return false;
	}

	void clear(void)
	{
		for (size_t i = 0; i < m_size; i++)
		{
			m_heap[i].~Type();
		}
		m_size = 0;
	}

};




template <typename Type, bool is_larger_or_equal(Type const & a, Type const & b), size_t CAPACITY>
class MinMaxHeap
/* A binary tree in which nodes in even rows are smaller than their descendants, and nodes in odd rows are larger than their descendants
 * The row containing the row is indexed by 0.
 */
{
private:
	Type 				m_heap[CAPACITY];
	size_t	 		m_size;


private:

	size_t grandparent_index(size_t index) const {return (index - 3) >> 2u;}
	size_t parent_index(size_t index) const {return (index - 1) >> 1u;}
	size_t grandchild_index(size_t index) const {return 4 * index + 3;};

	bool is_min_row(size_t index) const
	{
//		size_t row_num = 8 * sizeof(size_t) - __builtin_clz(index + 1);
		return (__builtin_clz(index + 1) & 0b1u);
	}

	void insert_and_heapify_max_up(Type & item, size_t index_hole)
	{
		while (index_hole > 2)
		{
			size_t index_swap = (index_hole + 1) >> 2u;
			if (is_larger_or_equal(m_heap[index_swap], item)) {break;}

			m_heap[index_hole] = std::move(m_heap[index_swap]);
			index_hole = index_swap;
		}
		m_heap[index_hole] = std::move(item);
	}

	void insert_and_heapify_min_up(Type & item, size_t index_hole)
	{
		while (index_hole > 2)
		{
			size_t index_swap = (index_hole + 1) >> 2u;
			if (is_larger_or_equal(item, m_heap[index_swap])) {break;}

			m_heap[index_hole] = std::move(m_heap[index_swap]);
			index_hole = index_swap;
		}
		m_heap[index_hole] = std::move(item);
	}

	void insert_and_heapify_max_down(Type & item, size_t index_hole)
	{
		while (4 * index_hole + 6 < m_size)
		{
			size_t index_base = grandchild_index(index_hole);
			size_t index01 = is_larger_or_equal(m_heap[index_base + 1], m_heap[index_base]) ? index_base + 1 : index_base;
			size_t index23 = is_larger_or_equal(m_heap[index_base + 3], m_heap[index_base + 2]) ? index_base + 3 : index_base + 2;
			size_t index_swap = is_larger_or_equal(m_heap[index23], m_heap[index01]) ? index23 : index01;
			if (is_larger_or_equal(item, m_heap[index_swap])) {break;}

			m_heap[index_hole] = std::move(m_heap[index_swap]);
			index_hole = index_swap;
		}

		if (grandchild_index(index_hole) < m_size)
		{
			size_t index_base = grandchild_index(index_hole);
			size_t index_swap = index_base;
			for (size_t i = index_base + 1; i < m_size; i++)
			{
				if (!is_larger_or_equal(m_heap[index_swap], m_heap[index_base + i]))
				{
					index_swap = index_base + i;
				}
			}
			if (!is_larger_or_equal(item, m_heap[index_swap]))
			{
				m_heap[index_hole] = std::move(m_heap[index_swap]);
				index_hole = index_swap;
			}
		}
		m_heap[index_hole] = std::move(item);
	}

	void insert_and_heapify_min_down(Type & item, size_t index_hole)
	{
		while (4 * index_hole + 6 < m_size)
		{
			size_t index_base = grandchild_index(index_hole);
			size_t index01 = is_larger_or_equal(m_heap[index_base], m_heap[index_base + 1]) ? index_base + 1 : index_base;
			size_t index23 = is_larger_or_equal(m_heap[index_base + 2], m_heap[index_base + 3]) ? index_base + 3 : index_base + 2;
			size_t index_swap = is_larger_or_equal(m_heap[index01], m_heap[index23]) ? index23 : index01;
			if (is_larger_or_equal(m_heap[index_swap], item)) {break;}

			m_heap[index_hole] = std::move(m_heap[index_swap]);
			index_hole = index_swap;
		}

		if (grandchild_index(index_hole) < m_size)
		{
			size_t index_base = grandchild_index(index_hole);
			size_t index_swap = index_base;
			for (size_t i = index_base + 1; i < m_size; i++)
			{
				if (!is_larger_or_equal(m_heap[index_base + i], m_heap[index_swap]))
				{
					index_swap = index_base + i;
				}
			}
			if (!is_larger_or_equal(m_heap[index_swap], item))
			{
				m_heap[index_hole] = std::move(m_heap[index_swap]);
				index_hole = index_swap;
			}
		}
		m_heap[index_hole] = std::move(item);
	}


public:

	MinMaxHeap(void) noexcept : m_size(0) {}
	~MinMaxHeap(void) noexcept = default;
	MinMaxHeap(MinMaxHeap<Type, is_larger_or_equal, CAPACITY> const &) = delete;
	MinMaxHeap(MinMaxHeap<Type, is_larger_or_equal, CAPACITY> &&) = delete;
	void operator=(MinMaxHeap<Type, is_larger_or_equal, CAPACITY> const &) = delete;
	void operator=(MinMaxHeap<Type, is_larger_or_equal, CAPACITY> &&) = delete;

	Type const & get_min(void) const
	{
		TX_ASSERT(m_size > 0);
		return m_heap[0];
	}

	Type & get_min(void)
	{
		TX_ASSERT(m_size > 0);
		return m_heap[0];
	}

	Type & get_max(void) const
	{
		TX_ASSERT(m_size > 0);
		if (m_size == 1) {return m_heap[0];}
		else if (m_size == 2) {return m_heap[1];}
		else {return is_larger_or_equal(m_heap[2], m_heap[1]) ? m_heap[2] : m_heap[1];}
	}

	size_t get_size(void) const {return m_size;}



	Type pop_top(void)
	{
		TX_ASSERT(m_size > 0);
		m_size--;
		Type top = std::move(m_heap[0]);

		if (m_size < 3)
		{
			m_heap[0] = std::move(m_heap[m_size]);
			return top;
		}

		size_t index_max = is_larger_or_equal(m_heap[2], m_heap[1]) ? 2 : 1;
//		if (is_larger_or_equal())


		size_t index_dst = 0;
		size_t index_src;
		size_t index;

		do
		{
			index_src = m_size;
			index = index_dst * 2 + 2;
			if (index < m_size && !is_larger_or_equal(m_heap[index_src], m_heap[index]))
			{
				index_src = index;
			}
			index = index_dst * 2 + 1;
			if (index < m_size && !is_larger_or_equal(m_heap[index_src], m_heap[index]))
			{
				index_src = index;
			}

			m_heap[index_dst] = std::move(m_heap[index_src]);
			index_dst = index_src;
		}
		while (index_src != m_size);

		return top;
	}

	void insert(Type const & item)
	{
		TX_ASSERT(m_size < CAPACITY);

		size_t index_hole = m_size;
		m_size++;

		if (index_hole >= 1)
		{
			size_t min_index = is_min_row(index_hole) ? grandparent_index(index_hole) : parent_index(index_hole);
			if (!is_larger_or_equal(item, m_heap[min_index]))
			{
				m_heap[index_hole] = std::move(m_heap[min_index]);
				insert_and_heapify_min_up(item, min_index);
				return;
			}
		}

		if (index_hole >= 3)
		{
			size_t max_index = !is_min_row(index_hole) ? grandparent_index(index_hole) : parent_index(index_hole);
			if (!is_larger_or_equal(m_heap[max_index], item))
			{
				m_heap[index_hole] = std::move(m_heap[max_index]);
				insert_and_heapify_max_up(item, max_index);
			}
		}
	}

};




}
