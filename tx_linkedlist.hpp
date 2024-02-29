/*
 * tx_linkedlist.hpp
 *
 *  Created on: Feb 8, 2024
 *      Author: tian_
 */

#pragma once

#include <stddef.h>
#include "tx_assert.h"

namespace TXLib
{

class LinkedCycle;

class LinkedCycleUnsafe
/* Represents a link in a cyclically linked list.
 * Can be linked with instances of LinkedCycle.
 * The only difference between LinkedCycleUnsafe and LinkedCycle is that LinkedCycleUnsafe has weaker requirement on the its legal state:
 * If an instance of LinkedCycle is single (not linked in a cycle of length at least two), its @m_prev and @m_next fields must point to itself.
 * In contrast, this requirement is not imposed for LinkedCycleUnsafe; the user keep track of the single links manually
 */
{
	friend LinkedCycle;

private:
	LinkedCycleUnsafe *				m_prev;
	LinkedCycleUnsafe *				m_next;


private:
	LinkedCycleUnsafe(LinkedCycleUnsafe * prev, LinkedCycleUnsafe * next) noexcept : m_prev(prev), m_next(next) {}

public:
	LinkedCycleUnsafe(void) noexcept = default;
	LinkedCycleUnsafe(LinkedCycleUnsafe const &) = delete;
	LinkedCycleUnsafe(LinkedCycleUnsafe && b) = delete;
	~LinkedCycleUnsafe(void) noexcept = default;
	void operator=(LinkedCycleUnsafe const & b) = delete;
	void operator=(LinkedCycleUnsafe && b) = delete;

	LinkedCycleUnsafe const & next(void) const {return *m_next;} // @this cannot be single
	LinkedCycleUnsafe & next(void) {return *m_next;} // @this cannot be single
	LinkedCycleUnsafe const & prev(void) const {return *m_prev;} // @this cannot be single
	LinkedCycleUnsafe & prev(void) {return *m_prev;} // @this cannot be single

	bool is_double(void) const {return m_next == m_prev;} // @this cannot be single

	void remove_from_cycle(void)
	// @this cannot be single
	{
		m_next->m_prev = m_prev;
		m_prev->m_next = m_next;
	}

	void become_safe(void)
	// Make @this a legal instance of LinkedCycle that is single
	// @this must be single
	{
		m_next = this;
		m_prev = this;
	}

	void insert_single_as_prev_of(LinkedCycle & anchor); // @this must be single
	void insert_as_prev_of(LinkedCycle & anchor); // @this cannot be single
	void insert_single_as_next_of(LinkedCycle & anchor); // @this must be single
	void insert_as_next_of(LinkedCycle & anchor); // @this cannot be single
};




class LinkedCycle : public LinkedCycleUnsafe
/* Represents a link in a cyclically linked list.
 */
{

public:
	LinkedCycle(void) noexcept : LinkedCycleUnsafe(this, this) {}
	LinkedCycle(LinkedCycle const &) = delete;
	LinkedCycle(LinkedCycle && b) = delete;
	~LinkedCycle(void) noexcept = default;
	void operator=(LinkedCycle const & b) = delete;
	void operator=(LinkedCycle && b) = delete;

	LinkedCycle const & next(void) const {return *reinterpret_cast<LinkedCycle const *>(m_next);}
	LinkedCycle & next(void) {return *reinterpret_cast<LinkedCycle *>(m_next);}
	LinkedCycle const & prev(void) const {return *reinterpret_cast<LinkedCycle const *>(m_prev);}
	LinkedCycle & prev(void) {return *reinterpret_cast<LinkedCycle *>(m_prev);}

	bool is_single(void) const {return m_next == this;}
	bool is_single_or_double(void) const {return m_next == m_prev;}

	void remove_from_cycle(void)
	{
		m_next->m_prev = m_prev;
		m_prev->m_next = m_next;
		m_next = this;
		m_prev = this;
	}

	void insert_single_as_prev_of(LinkedCycle & anchor)
	{
		TX_ASSERT(is_single());
		m_prev = anchor.m_prev;
		m_next = &anchor;
		anchor.m_prev->m_next = this;
		anchor.m_prev = this;
	}

	void insert_single_as_next_of(LinkedCycle & anchor)
	{
		TX_ASSERT(is_single());
		m_next = anchor.m_next;
		m_prev = &anchor;
		anchor.m_next->m_prev = this;
		anchor.m_next = this;
	}

	void criss_cross_with(LinkedCycle & target)
	// Criss cross the edge between @this and @this->m_next with the edge between @target and @target->m_prev
	{
		this->m_next->m_prev = target.m_prev;
		target.m_prev->m_next = this->m_next;
		this->m_next = &target;
		target.m_prev = this;
	}
};


inline void LinkedCycleUnsafe::insert_single_as_prev_of(LinkedCycle & anchor)
// @this must be single
{
	m_prev = anchor.m_prev;
	m_next = &anchor;
	anchor.m_prev->m_next = this;
	anchor.m_prev = this;
}

inline void LinkedCycleUnsafe::insert_as_prev_of(LinkedCycle & anchor)
// @this cannot be single
{
	m_prev->m_next = m_next;
	m_next->m_prev = m_prev;

	m_prev = anchor.m_prev;
	m_next = &anchor;
	anchor.m_prev->m_next = this;
	anchor.m_prev = this;
}

inline void LinkedCycleUnsafe::insert_single_as_next_of(LinkedCycle & anchor)
// @this must be single
{
	m_next = anchor.m_next;
	m_prev = &anchor;
	anchor.m_next->m_prev = this;
	anchor.m_next = this;
}

inline void LinkedCycleUnsafe::insert_as_next_of(LinkedCycle & anchor)
// @this cannot be single
{
	m_prev->m_next = m_next;
	m_next->m_prev = m_prev;

	m_next = anchor.m_next;
	m_prev = &anchor;
	anchor.m_next->m_prev = this;
	anchor.m_next = this;
}





}
