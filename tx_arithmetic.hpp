/*
 * tx_arithmetic.hpp
 *
 *  Created on: Feb 13, 2024
 *      Author: tian_
 */

#pragma once

#include <stddef.h>
#include <utility>
#include "tx_assert.h"

namespace TXLib
{

inline std::pair<size_t, size_t> divide(size_t dividend, size_t divisor)
{
	TX_ASSERT(divisor > 0);
	TX_ASSERT((dividend << 1u) >= dividend); // Highest bit cannot be 1 for the algorithm to work properly

	size_t pos = 0b1u;
	while (divisor < dividend)
	{
		divisor = divisor << 1u;
		pos = pos << 1u;
	}
	size_t quotient = 0;
	while (pos > 0u)
	{
		if (dividend >= divisor)
		{
			dividend -= divisor;
			quotient |= pos;
		}
		divisor = divisor >> 1u;
		pos = pos >> 1u;
	}
	return std::pair<size_t, size_t>(quotient, dividend);
}



}
