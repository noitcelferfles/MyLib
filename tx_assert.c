/*
 * tx_assert.c
 *
 *  Created on: Dec 17, 2023
 *      Author: tian_
 */

#include "tx_assert.h"

#ifdef TX_NO_ASSERT

#else

void tx_assert(size_t condition)
{
	while (!condition)
	{
		__asm volatile ("bkpt");
	}
}

void TX_Assert(size_t condition)
{
	while (!condition)
	{
		__asm volatile ("bkpt");
	}
}

#endif

