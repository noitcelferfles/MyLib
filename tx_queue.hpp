/*
 * tx_queue.hpp
 *
 *  Created on: Feb 6, 2024
 *      Author: tian_
 */

#pragma once

#include <stddef.h>
#include <utility>
#include "tx_assert.h"

namespace TXLib
{


template <typename Type>
class Queue
{
public:
	typedef				void * (*Alloc)(size_t);
	typedef				void (*Free)(void *);


private:
	Type * 				m_array;
	size_t				m_front; // take values in [0, m_capacity)
	size_t				m_back;  // take values in [0, m_capacity]; if m_back < m_front, the last used index is m_back and not m_back - 1
	size_t				m_capacity;

	Alloc					m_alloc;
	Free					m_free;

private:
	size_t last_used_index(void) const {return (m_front <= m_back) ? (m_back - 1) : m_back;}

public:
	Queue(void) noexcept : m_array(nullptr) {}
	Queue(Queue<Type> const &) = delete;
	Queue(Queue<Type> &&) = delete;
	Queue(Alloc alloc, Free free, size_t capacity) {initialize(alloc, free, capacity);}
	void operator=(Queue<Type> const &) = delete;
	void operator=(Queue<Type> &&) = delete;

	bool is_initialized(void) const {return m_array != nullptr;}

	void initialize(Alloc alloc, Free free, size_t capacity)
	{
		TX_ASSERT(!is_initialized());

		m_front = 0;
		m_back = 0;
		m_capacity = capacity;

		m_alloc = alloc;
		m_free = free;

		// Allocate raw memory
		m_array = (Type *) m_alloc(m_capacity * sizeof(Type));
	}

	void uninitialize(void)
	{
		if (!is_initialized()) {return;}
		clear();
		m_free(m_array);
		m_array = nullptr;
	}

	~Queue(void) noexcept
	{
		uninitialize();
	}

	void clear(void)
	{
		if (m_front <= m_back)
		{
			for (size_t i = m_front; i < m_back; i++)
			{
				m_array[i].~Type();
			}
		}
		else
		{
			for (size_t i = m_front; i < m_capacity; i++)
			{
				m_array[i].~Type();
			}
			for (size_t i = 0; i <= m_back; i++)
			{
				m_array[i].~Type();
			}
		}
		m_front = 0;
		m_back = 0;
	}

	bool is_empty(void) const {return m_front == m_back;}
	bool is_full(void) const {return (m_front > 0) ? (m_front == m_back + 1) : (m_back == m_capacity);}
	size_t get_size(void) const {return (m_front <= m_back) ? (m_back - m_front) : (m_back + 1 + m_capacity - m_front);}
	size_t get_capacity(void) const {return m_capacity;}

	Type & front(void) {TX_ASSERT(!is_empty()); return m_array[m_front];}
	Type const & front(void) const {TX_ASSERT(!is_empty()); return m_array[m_front];}
	Type & back(void) {TX_ASSERT(!is_empty()); return m_array[last_used_index()];}
	Type const & back(void) const {TX_ASSERT(!is_empty()); return m_array[last_used_index()];}

	template <typename... Args>
	Type & push_back(Args && ... args)
	{
		TX_ASSERT(is_initialized());
		TX_ASSERT(!is_full());
		m_back++;
		if (m_back > m_capacity) {m_back = 0;}
		Type * ptr = ::new(m_array + last_used_index()) Type(std::forward<Args>(args) ...); static_assert(noexcept(Type(std::forward<Args>(args) ...)));
		return *ptr;
	}

	Type pop_front(void)
	{
		TX_ASSERT(!is_empty());
		Type temp = std::move(m_array[m_front]);
		m_array[m_front].~Type();
		m_front++;
		if (m_front >= m_capacity)
		{
			m_front = 0;
			m_back = (m_back == m_capacity) ? 0 : (m_back + 1);
		}
		return temp;
	}
	Type pop_back(void)
	{
		TX_ASSERT(!is_empty());
		Type temp = std::move(m_array[last_used_index()]);
		m_array[last_used_index()].~Type();
		m_back = (m_back == 0) ? m_capacity : (m_back - 1);
		return temp;
	}

};



}
