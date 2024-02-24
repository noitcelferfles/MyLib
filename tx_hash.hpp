/*
 * tx_data_structure.h
 *
 *  Created on: Dec 19, 2023
 *      Author: tian_
 */

#pragma once

#include <stddef.h>
#include <cstring>
#include "tx_assert.h"

namespace TXLib
{



// Open addressing hash table with conflict resolution by linear search
// Once VALUE_CAPACITY has been reached, newly added key will replace existing key
template <typename Key, typename Value, size_t KEY_CAPACITY, size_t VALUE_CAPACITY, size_t hash_func(Key)>
class ForgetfulHash
{

private:

	static constexpr size_t const REF_INVALID = (size_t)(-1);  // 0xFF...FF

private:

	size_t			size;
	size_t			ref_list[KEY_CAPACITY];
	Key					key_list[KEY_CAPACITY];
	Value				value_array[VALUE_CAPACITY];


private:

	inline size_t next_index(size_t index) const
	{
		index++;
		if (index == KEY_CAPACITY) {index = 0;}
		return index;
	}

	inline size_t prev_index(size_t index) const
	{
		if (index == 0) {index = KEY_CAPACITY;}
		return index - 1;
	}

	bool index_is_free(size_t index) const
	{
		return ref_list[index] == REF_INVALID;
	}


public:
	ForgetfulHash(void) : size(0)
	{
		TX_ASSERT(KEY_CAPACITY > VALUE_CAPACITY && VALUE_CAPACITY > 0);
		std::memset(ref_list, 0xFF, KEY_CAPACITY * sizeof(size_t));
	}

	size_t get_size(void) const {return size;}
	size_t get_key_capacity(void) const {return KEY_CAPACITY;}
	size_t get_value_capacity(void) const {return VALUE_CAPACITY;}

	void clear(void)
	{
		size = 0;
		std::memset(ref_list, 0xFF, KEY_CAPACITY * sizeof(size_t));
	}

	Value * find(Key const & key)
	{
		size_t index = hash_func(key);
		TX_ASSERT(index < KEY_CAPACITY);

		while (!index_is_free(index))
		{
			if (key_list[index] == key) {return &value_array[ref_list[index]];}
			index = next_index(index);
		}
		return nullptr;
	}

	// This will move the key closer to its hashed position to speed up future search
	Value * find_and_prioritize(Key const & key)
	{
		size_t index = hash_func(key);
		TX_ASSERT(index < KEY_CAPACITY);

		if (!index_is_free(index) && key_list[index] == key)
		{
			return &value_array[ref_list[index]];
		}

		size_t index_next = next_index(index);
		while (!index_is_free(index))
		{
			if (key_list[index_next] == key)
			{
				Key temp_key = key_list[index_next];
				key_list[index_next] = key_list[index];
				key_list[index] = temp_key;

				size_t temp_num = ref_list[index_next];
				ref_list[index_next] = ref_list[index];
				ref_list[index] = temp_num;

				return &value_array[temp_num];
			}

			index = index_next;
			index_next = next_index(index);
		}
		return nullptr;
	}

	// Replace current value if it exists
	// Remove another existing key if storage is insufficient
	void insert(Key const & key, Value const & value)
	{
		size_t key_index = hash_func(key);
		TX_ASSERT(key_index < KEY_CAPACITY);

		size_t index = key_index;
		while (!index_is_free(index))
		{
			if (key_list[index] == key)
			{
				value_array[ref_list[index]] = value;
				return;
			}
			index = next_index(index);
		}

		// Reaching here means that the key has not been registered
		// key_list[index] is now a legal location to store the key

		// Find a position in value_array to store the new value
		if (size < VALUE_CAPACITY)
		{
			ref_list[index] = size;
			size ++;
		}
		else
		{
			// Reaching here means that value_array is full
			// Remove an existing key-value pair to make space
			size_t index_remove = key_index;
			while (!index_is_free(index_remove))
			{
				index_remove = prev_index(index_remove);
			}
			while (index_is_free(index_remove))
			{
				index_remove = prev_index(index_remove);
			}

			if (next_index(index_remove) == index)
			{
				// Reaching here means that all occupied keys are in a (cyclically) continuous segment
				index = index_remove;
			}
			else
			{
				ref_list[index] = ref_list[index_remove];
				ref_list[index_remove] = REF_INVALID;
			}
		}

		// Store the new key-value pair
		key_list[index] = key;
		value_array[ref_list[index]] = value;
	}


};


template <typename Key, typename Value, size_t CAPACITY, Key const & KEY_INVALID, size_t hash_func(Key)>
class HashTable
{

private:

	static constexpr size_t const INDEX_INVALID = 0xFFFFFFFF;

private:

	size_t			size;
	Key					key_list[CAPACITY];
	Value				value_list[CAPACITY];

	// Assumptions on data:
	//   key_list is not full (at least one key is KEY_INVALID)


private:

	inline size_t next_index(size_t index) const
	{
		index++;
		if (index == CAPACITY) {index = 0;}
		return index;
	}

	inline size_t prev_index(size_t index) const
	{
		if (index == 0) {index = CAPACITY;}
		return index - 1;
	}

	size_t compute_distance(size_t index) const
	{
		size_t index_opt = hash_func(key_list[index]);
		return (index >= index_opt) ? index - index_opt : index_opt + CAPACITY - index;
	}


public:
	HashTable(void) : size(0)
	{
		TX_ASSERT(CAPACITY > 0);
		for (size_t i = 0; i < CAPACITY; i++)
		{
			key_list[i] = KEY_INVALID;
		}
	}

	size_t get_size(void) const {return size;}
	size_t get_capacity(void) const {return CAPACITY;}

	size_t find_index(Key const & key) const
	{
		size_t index = hash_func(key);
		TX_ASSERT(index < CAPACITY);

		while (key_list[index] != key)
		{
			if (key_list[index] == KEY_INVALID) {return INDEX_INVALID;}
			index = next_index(index);
		}

		return index;
	}

	Value * find(Key const & key)
	{
		size_t index = hash_func(key);
		TX_ASSERT(index < CAPACITY);

		while (key_list[index] != key)
		{
			if (key_list[index] == KEY_INVALID) {return nullptr;}
			index = next_index(index);
		}
		return &value_list[index];
	}

	void clear(void)
	{
		size = 0;
		for (size_t i = 0; i < CAPACITY; i++)
		{
			key_list[i] = KEY_INVALID;
		}
	}

	// Replace current value if it exists
	void insert(Key const & key, Value const & value)
	{
		TX_ASSERT(key != KEY_INVALID);

		size_t index = hash_func(key);
		TX_ASSERT(index < CAPACITY);

		while (key_list[index] != KEY_INVALID && key_list[index] != key)
		{
			index = next_index(index);
		}

		if (key_list[index] != key)
		{
			key_list[index] = key;
			size++;
		}
		value_list[index] = value;

		TX_ASSERT(size < CAPACITY); // Ensure that key_list is not full
	}

	// Insert without growing the size of the table
	// An existing key is removed if necessary
//	void insert_replace(Key key, Value const & value)
//	{
//		TX_ASSERT(key != KEY_INVALID);
//
//		size_t index = hash_func(key);
//		TX_ASSERT(index < CAPACITY);
//
//		while (key_list[index] != KEY_INVALID && key_list[index] != key)
//		{
//			index = next_index(index);
//		}
//		if (key_list[index] == KEY_INVALID)
//		{
//			index = hash_func(key);
//		}
//
//		if (key_list[index] == KEY_INVALID)
//		{
//			size_t index_remove = prev_index(index);
//			while (key_list[index_remove] == KEY_INVALID)
//			{
//				index_remove = prev_index(index_remove);
//			}
//
//			key_list[index_remove] = KEY_INVALID;
//		}
//
//		key_list[index] = key;
//		value_list[index] = value;
//	}

	// Remove the key if it exists
	void remove(Key key)
	{
		TX_ASSERT(key != KEY_INVALID);

		size_t index_remove = hash_func(key);
		TX_ASSERT(index_remove < CAPACITY);

		while (key_list[index_remove] != key)
		{
			if (key_list[index_remove] == KEY_INVALID) {return;}
			index_remove = next_index(index_remove);
		}

		// Shift table up
		size_t distance = 1;
		size_t index_replace = next_index(index_remove);
		while (key_list[index_replace] != KEY_INVALID)
		{
			if (compute_distance(index_replace) >= distance)
			{
				key_list[index_remove] = key_list[index_replace];
				value_list[index_remove] = value_list[index_replace];
				distance = 0;
				index_remove = index_replace;
			}
			distance ++;
			index_replace = next_index(index_replace);
		}

		key_list[index_remove] = KEY_INVALID;
		size --;
	}


};



}

