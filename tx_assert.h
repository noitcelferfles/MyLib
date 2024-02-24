/*
 * assert.h
 *
 *  Created on: Dec 17, 2023
 *      Author: tian_
 */

#ifndef TX_ASSERT_H_
#define TX_ASSERT_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <stddef.h>


#ifdef TX_NO_ASSERT
	#define TX_ASSERT(expression)
 	inline void tx_assert(size_t) {}
#else
	#define TX_ASSERT(expression)					tx_assert(expression)
	void tx_assert(size_t);
#endif


// Legacy
#ifdef TX_NO_ASSERT
	inline void TX_Assert(size_t condition) {}
#else
	void TX_Assert(size_t condition);
#endif



#ifdef __cplusplus
 }
#endif

#endif /* ASSERT_H_ */
