#ifndef EDGE_HASHMAP_H
#define EDGE_HASHMAP_H

#include "allocator.hpp"
#include "hash.hpp"

namespace edge {
	namespace detail {
		constexpr usize HASHMAP_DEFAULT_BUCKET_COUNT = 16;
		constexpr f32 HASHMAP_MAX_LOAD_FACTOR = 0.75f;

		template<TrivialType K>
		inline usize default_hash(const K& key) {
			const u8* data = (const u8*)&key;
			return hash_xxh64(data, sizeof(K));
		}

		template<TrivialType K>
		inline i32 default_compare(const K& key1, const K& key2) {
			return memcmp(&key1, &key2, sizeof(K));
		}
	}

	template<TrivialType K, TrivialType V>
	struct HashMapEntry;

	template<TrivialType K, TrivialType V>
	struct HashMap;

	template<TrivialType K, TrivialType V>
	struct HashMapIterator {
		const HashMap<K, V>* map;
		usize bucket_index;
		HashMapEntry<K, V>* current;

		bool operator==(const HashMapIterator& other) const {
			return current == other.current;
		}

		bool operator!=(const HashMapIterator& other) const {
			return current != other.current;
		}

		HashMapEntry<K, V>& operator*() const {
			return *current;
		}

		HashMapEntry<K, V>* operator->() const {
			return current;
		}

		HashMapIterator& operator++() {
			if (!map || !current) {
				return *this;
			}

			// Try next in chain
			if (current->next) {
				current = current->next;
				return *this;
			}

			// Find next non-empty bucket
			for (usize i = bucket_index + 1; i < map->m_bucket_count; i++) {
				if (map->m_buckets[i]) {
					bucket_index = i;
					current = map->m_buckets[i];
					return *this;
				}
			}

			// End of iteration
			current = nullptr;
			return *this;
		}

		HashMapIterator operator++(int) {
			HashMapIterator tmp = *this;
			++(*this);
			return tmp;
		}
	};

	template<TrivialType K, TrivialType V>
	struct HashMapEntry {
		K key = {};
		V value = {};
		usize hash = 0;
		HashMapEntry* next = nullptr;
	};

	template<TrivialType K, TrivialType V>
	struct HashMap {
		HashMapEntry<K, V>** m_buckets = nullptr;
		usize m_bucket_count = 0ull;
		usize m_size = 0ull;
		usize(*m_hash_func)(const K&) = nullptr;
		i32(*m_compare_func)(const K&, const K&) = nullptr;

		bool create(NotNull<const Allocator*> alloc, usize initial_bucket_count = 0ull) {
			if (initial_bucket_count == 0ull) {
				initial_bucket_count = detail::HASHMAP_DEFAULT_BUCKET_COUNT;
			}

			m_buckets = alloc->allocate_array<HashMapEntry<K, V>*>(initial_bucket_count);
			if (!m_buckets) {
				return false;
			}

			m_bucket_count = initial_bucket_count;
			m_size = 0ull;
			m_hash_func = detail::default_hash<K>;
			m_compare_func = detail::default_compare<K>;

			return true;
		}

		bool create(NotNull<const Allocator*> alloc, usize initial_bucket_count,
			usize(*hash_func)(const K&), i32(*compare_func)(const K&, const K&)) {
			if (!create(alloc, initial_bucket_count)) {
				return false;
			}

			if (hash_func) {
				m_hash_func = hash_func;
			}
			if (compare_func) {
				m_compare_func = compare_func;
			}

			return true;
		}

		void destroy(NotNull<const Allocator*> alloc) {
			clear(alloc);

			if (m_buckets) {
				alloc->deallocate_array(m_buckets, m_bucket_count);
			}
		}

		void clear(NotNull<const Allocator*> alloc) {
			for (usize i = 0; i < m_bucket_count; i++) {
				HashMapEntry<K, V>* entry = m_buckets[i];
				while (entry) {
					HashMapEntry<K, V>* next = entry->next;
					alloc->deallocate(entry);
					entry = next;
				}
				m_buckets[i] = nullptr;
			}

			// TODO: Call destructor for not trivially destructable

			m_size = 0;
		}

		bool rehash(NotNull<const Allocator*> alloc, usize new_bucket_count) {
			if (new_bucket_count == 0) {
				return false;
			}

			HashMapEntry<K, V>** new_buckets = alloc->allocate_array<HashMapEntry<K, V>*>(new_bucket_count);
			if (!new_buckets) {
				return false;
			}

			// Rehash all entries
			for (usize i = 0; i < m_bucket_count; i++) {
				HashMapEntry<K, V>* entry = m_buckets[i];
				while (entry) {
					HashMapEntry<K, V>* next = entry->next;

					usize bucket_index = entry->hash % new_bucket_count;
					entry->next = new_buckets[bucket_index];
					new_buckets[bucket_index] = entry;

					entry = next;
				}
			}

			alloc->deallocate_array(m_buckets, m_bucket_count);
			m_buckets = new_buckets;
			m_bucket_count = new_bucket_count;

			return true;
		}

		f32 load_factor() const noexcept {
			if (m_bucket_count == 0) {
				return 0.0f;
			}
			return (f32)m_size / (f32)m_bucket_count;
		}

		bool insert(NotNull<const Allocator*> alloc, const K& key, const V& value) {
			// Check load factor and rehash if needed
			if (load_factor() >= detail::HASHMAP_MAX_LOAD_FACTOR) {
				rehash(alloc, m_bucket_count * 2);
			}

			usize hash = m_hash_func(key);
			usize bucket_index = hash % m_bucket_count;

			// Check if key already exists
			HashMapEntry<K, V>* entry = m_buckets[bucket_index];
			while (entry) {
				if (entry->hash == hash && m_compare_func(entry->key, key) == 0) {
					// Update existing value
					entry->value = value;
					return true;
				}
				entry = entry->next;
			}

			// Create new entry
			HashMapEntry<K, V>* new_entry = alloc->allocate<HashMapEntry<K, V>>(key, value, hash, nullptr);
			if (!new_entry) {
				return false;
			}

			new_entry->next = m_buckets[bucket_index];
			m_buckets[bucket_index] = new_entry;
			m_size++;

			return true;
		}

		V* get(const K& key) noexcept {
			usize hash = m_hash_func(key);
			usize bucket_index = hash % m_bucket_count;

			HashMapEntry<K, V>* entry = m_buckets[bucket_index];
			while (entry) {
				if (entry->hash == hash && m_compare_func(entry->key, key) == 0) {
					return &entry->value;
				}
				entry = entry->next;
			}

			return nullptr;
		}

		V const* get(const K& key) const noexcept {
			usize hash = m_hash_func(key);
			usize bucket_index = hash % m_bucket_count;

			HashMapEntry<K, V>* entry = m_buckets[bucket_index];
			while (entry) {
				if (entry->hash == hash && m_compare_func(entry->key, key) == 0) {
					return &entry->value;
				}
				entry = entry->next;
			}

			return nullptr;
		}

		bool remove(NotNull<const Allocator*> alloc, const K& key, V* out_value) {
			usize hash = m_hash_func(key);
			usize bucket_index = hash % m_bucket_count;

			HashMapEntry<K, V>* entry = m_buckets[bucket_index];
			HashMapEntry<K, V>* prev = nullptr;

			while (entry) {
				if (entry->hash == hash && m_compare_func(entry->key, key) == 0) {
					if (out_value) {
						*out_value = entry->value;
					}

					if (prev) {
						prev->next = entry->next;
					}
					else {
						m_buckets[bucket_index] = entry->next;
					}

					alloc->deallocate(entry);
					m_size--;

					return true;
				}

				prev = entry;
				entry = entry->next;
			}

			return false;
		}

		bool contains(const K& key) const noexcept {
			return get(key) != nullptr;
		}

		bool empty() const noexcept {
			return m_size == 0;
		}
	};

	template<TrivialType K, TrivialType V>
	inline HashMapIterator<K, V> begin(HashMap<K, V>& map) {
		HashMapIterator<K, V> it;
		it.map = &map;
		it.bucket_index = 0;
		it.current = nullptr;

		// Find first non-empty bucket
		for (usize i = 0; i < map.m_bucket_count; i++) {
			if (map.m_buckets[i]) {
				it.bucket_index = i;
				it.current = map.m_buckets[i];
				break;
			}
		}

		return it;
	}

	template<TrivialType K, TrivialType V>
	inline HashMapIterator<K, V> end(HashMap<K, V>& map) {
		return HashMapIterator<K, V>{ &map, 0, nullptr };
	}

	template<TrivialType K, TrivialType V>
	inline HashMapIterator<K, V> begin(const HashMap<K, V>& map) {
		HashMapIterator<K, V> it;
		it.map = &map;
		it.bucket_index = 0;
		it.current = nullptr;

		// Find first non-empty bucket
		for (usize i = 0; i < map.m_bucket_count; i++) {
			if (map.m_buckets[i]) {
				it.bucket_index = i;
				it.current = map.m_buckets[i];
				break;
			}
		}

		return it;
	}

	template<TrivialType K, TrivialType V>
	inline HashMapIterator<K, V> end(const HashMap<K, V>& map) {
		return HashMapIterator<K, V>{ &map, 0, nullptr };
	}
}

#endif // !EDGE_HASHMAP_H
