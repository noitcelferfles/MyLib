/*
 * tx_spinlock.hpp
 *
 *  Created on: Feb 6, 2024
 *      Author: tian_
 */

#pragma once

#include <stddef.h>
#include <atomic>
#include "tx_assert.h"

extern "C" {
#include "cmsis_compiler.h"
}



class Spinlock
{
private:
	std::atomic<bool>			m_lock;
	bool									m_primask;

public:
	Spinlock(void) noexcept : m_lock(false) {}
	Spinlock(Spinlock const &) = delete;
	Spinlock(Spinlock &&) = delete;
	~Spinlock(void) noexcept = default;
	void operator=(Spinlock const &) = delete;
	void operator=(Spinlock &&) = delete;

	void acquire(void)
	{
		m_primask = __get_PRIMASK();
		__set_PRIMASK(0b1); // Disable interrupt in critical section
		while (m_lock.exchange(true, std::memory_order_acq_rel));
	}

	// WARNING: The interrupt state may be wrong if multiple spin locks are acquired and released in an interleaving fashion.
	void release(void)
	{
		TX_ASSERT(__get_PRIMASK() & 0b1); // Interrupt should not be enabled by the program
		m_lock.store(false, std::memory_order_release);
		__set_PRIMASK(m_primask); // Revert back to previous interrupt state
	}
};

