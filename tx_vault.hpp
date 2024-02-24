/*
 * tx_vault.hpp
 *
 *  Created on: Jan 30, 2024
 *      Author: tian_
 */

#pragma once

#include "tx_array.hpp"
#include <stddef.h>
#include <utility>


namespace TXLib
{

// Resizable container with constant-time access, insertion, removal
// Members of the container are accessed using a key obtained during insertion
// The structure is not iterable
template <typename Type>
class DynamicVault
{
public:

	typedef				void * (*Alloc)(size_t);
	typedef				void (*Free)(void *);


	class Key
	{
		friend DynamicVault<Type>;

	private:
		static constexpr size_t const INVALID = ~((size_t) 0);

	private:
		size_t			m_index;

	private:
		inline Key(size_t index) noexcept : m_index(index) {}

	public:
		inline Key(void) noexcept : m_index(INVALID) {}
		inline Key(Key const &) noexcept = default;
		inline Key(Key &&) noexcept = default;
		inline void operator=(Key const & b) noexcept {m_index = b.m_index;}
		inline void operator=(Key && b) noexcept {m_index = b.m_index;}
		inline ~Key(void) noexcept = default;
		inline bool is_invalid(void) const {return m_index == INVALID;}
		inline void set_invalid(void) {m_index = INVALID;}
		inline bool operator==(Key const & b) const {return m_index == b.m_index;}
		inline bool operator!=(Key const & b) const {return m_index != b.m_index;}
	};

private:

	DynamicArray<Type>				m_content;
	LightDynamicArray<size_t> 			m_removed_index;


public:
	DynamicVault(void) noexcept : m_content(), m_removed_index() {}
	DynamicVault(DynamicVault<Type> const &) = delete;
	DynamicVault(DynamicVault<Type> &&) = delete;
	void operator=(DynamicVault<Type> const &) = delete;
	void operator=(DynamicVault<Type> &&) = delete;
	~DynamicVault(void) noexcept = default;

	bool is_initialized(void) const {return m_content.is_initialized();}

	void initialize(Alloc alloc, Free free)
	{
		m_content.initialize(alloc, free, 2);
		m_removed_index.initialize(alloc, free, 2);
	}

	DynamicVault(Alloc alloc, Free free) {initialize(alloc, free);}

	size_t get_size(void) const {return m_content.get_size() - m_removed_index.get_size();}

	Type & operator[](Key const & key)
	{
		TX_ASSERT(!key.is_invalid());
		return m_content[key.m_index];
	}

	Type const & operator[](Key const & key) const
	{
		TX_ASSERT(!key.is_invalid());
		return m_content[key.m_index];
	}

	Key insert(void)
	{
		Key key;
		if (m_removed_index.get_size() > 0)
		{
			key.m_index = m_removed_index.pop_back();
		}
		else
		{
			key.m_index = m_content.get_size();
			m_content.push_back();
		}
		return key;
	}
	Key insert(Type const & item)
	{
		Key key = insert();
		m_content[key.m_index] = item;
		return key;
	}
	Key insert(Type && item)
	{
		Key key = insert();
		m_content[key.m_index] = item;
		return key;
	}

	Type remove(Key & key)
	{
		Type temp = std::move(m_content[key.m_index]);
		m_removed_index.push_back(key.m_index);
		key.set_invalid();
		return temp;
	}

};



}
