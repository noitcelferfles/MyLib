/*
 * tx_assert.c
 *
 *  Created on: Dec 17, 2023
 *      Author: tian_
 */

#include "tx_assert.h"

void tx_api_assert(size_t condition)
{
	while (!condition)
	{
		__asm volatile ("bkpt");
	}
}


#ifndef TX_NO_ASSERT

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

