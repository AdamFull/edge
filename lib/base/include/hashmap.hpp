#ifndef EDGE_HASHMAP_H
#define EDGE_HASHMAP_H

#include "allocator.hpp"
#include "hash.hpp"

namespace edge {
	namespace detail {
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
	struct HashMapEntry {
		K key;
		V value;
		usize hash;
		HashMapEntry* next;
	};

	template<TrivialType K, TrivialType V>
	struct HashMap {
		HashMapEntry<K, V>** m_buckets;
		usize m_bucket_count;
		usize m_size;
		const Allocator* m_allocator;
		usize(*m_hash_func)(const K&);
		i32(*m_compare_func)(const K&, const K&);
	};

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

	namespace detail {
		constexpr usize HASHMAP_DEFAULT_BUCKET_COUNT = 16;
		constexpr f32 HASHMAP_MAX_LOAD_FACTOR = 0.75f;

		template<TrivialType K, TrivialType V>
		HashMapEntry<K, V>* create_entry(const Allocator* alloc, const K& key, const V& value, usize hash) {
			HashMapEntry<K, V>* entry = allocate<HashMapEntry<K, V>>(alloc);
			if (!entry) {
				return nullptr;
			}

			entry->key = key;
			entry->value = value;
			entry->hash = hash;
			entry->next = nullptr;

			return entry;
		}

		template<TrivialType K, TrivialType V>
		void destroy_entry(const Allocator* alloc, HashMapEntry<K, V>* entry) {
			if (!entry) {
				return;
			}
			deallocate(alloc, entry);
		}
	}

	template<TrivialType K, TrivialType V>
	bool hashmap_create(const Allocator* alloc, HashMap<K, V>* map, usize initial_bucket_count = 0) {
		if (!alloc || !map) {
			return false;
		}

		if (initial_bucket_count == 0) {
			initial_bucket_count = detail::HASHMAP_DEFAULT_BUCKET_COUNT;
		}

		map->m_buckets = allocate_zeroed<HashMapEntry<K, V>*>(alloc, initial_bucket_count);
		if (!map->m_buckets) {
			return false;
		}

		map->m_bucket_count = initial_bucket_count;
		map->m_size = 0;
		map->m_allocator = alloc;
		map->m_hash_func = detail::default_hash<K>;
		map->m_compare_func = detail::default_compare<K>;

		return true;
	}

	template<TrivialType K, TrivialType V>
	bool hashmap_create_custom(const Allocator* alloc, HashMap<K, V>* map,
		usize initial_bucket_count,
		usize(*hash_func)(const K&),
		i32(*compare_func)(const K&, const K&)) {
		if (!hashmap_create(alloc, map, initial_bucket_count)) {
			return false;
		}

		if (hash_func) {
			map->m_hash_func = hash_func;
		}
		if (compare_func) {
			map->m_compare_func = compare_func;
		}

		return true;
	}

	template<TrivialType K, TrivialType V>
	void hashmap_destroy(HashMap<K, V>* map) {
		if (!map) {
			return;
		}

		hashmap_clear(map);

		if (map->m_buckets) {
			deallocate(map->m_allocator, map->m_buckets);
		}
	}

	template<TrivialType K, TrivialType V>
	void hashmap_clear(HashMap<K, V>* map) {
		if (!map) {
			return;
		}

		for (usize i = 0; i < map->m_bucket_count; i++) {
			HashMapEntry<K, V>* entry = map->m_buckets[i];
			while (entry) {
				HashMapEntry<K, V>* next = entry->next;
				detail::destroy_entry(map->m_allocator, entry);
				entry = next;
			}
			map->m_buckets[i] = nullptr;
		}

		map->m_size = 0;
	}

	template<TrivialType K, TrivialType V>
	bool hashmap_rehash(HashMap<K, V>* map, usize new_bucket_count) {
		if (!map || new_bucket_count == 0) {
			return false;
		}

		HashMapEntry<K, V>** new_buckets = allocate_zeroed<HashMapEntry<K, V>*>(map->m_allocator, new_bucket_count);
		if (!new_buckets) {
			return false;
		}

		// Rehash all entries
		for (usize i = 0; i < map->m_bucket_count; i++) {
			HashMapEntry<K, V>* entry = map->m_buckets[i];
			while (entry) {
				HashMapEntry<K, V>* next = entry->next;

				usize bucket_index = entry->hash % new_bucket_count;
				entry->next = new_buckets[bucket_index];
				new_buckets[bucket_index] = entry;

				entry = next;
			}
		}

		deallocate(map->m_allocator, map->m_buckets);
		map->m_buckets = new_buckets;
		map->m_bucket_count = new_bucket_count;

		return true;
	}

	template<TrivialType K, TrivialType V>
	f32 hashmap_load_factor(const HashMap<K, V>* map) {
		if (!map || map->m_bucket_count == 0) {
			return 0.0f;
		}
		return (f32)map->m_size / (f32)map->m_bucket_count;
	}

	template<TrivialType K, TrivialType V>
	bool hashmap_insert(HashMap<K, V>* map, const K& key, const V& value) {
		if (!map) {
			return false;
		}

		// Check load factor and rehash if needed
		if (hashmap_load_factor(map) >= detail::HASHMAP_MAX_LOAD_FACTOR) {
			hashmap_rehash(map, map->m_bucket_count * 2);
		}

		usize hash = map->m_hash_func(key);
		usize bucket_index = hash % map->m_bucket_count;

		// Check if key already exists
		HashMapEntry<K, V>* entry = map->m_buckets[bucket_index];
		while (entry) {
			if (entry->hash == hash && map->m_compare_func(entry->key, key) == 0) {
				// Update existing value
				entry->value = value;
				return true;
			}
			entry = entry->next;
		}

		// Create new entry
		HashMapEntry<K, V>* new_entry = detail::create_entry(map->m_allocator, key, value, hash);
		if (!new_entry) {
			return false;
		}

		new_entry->next = map->m_buckets[bucket_index];
		map->m_buckets[bucket_index] = new_entry;
		map->m_size++;

		return true;
	}

	template<TrivialType K, TrivialType V>
	V* hashmap_get(const HashMap<K, V>* map, const K& key) {
		if (!map) {
			return nullptr;
		}

		usize hash = map->m_hash_func(key);
		usize bucket_index = hash % map->m_bucket_count;

		HashMapEntry<K, V>* entry = map->m_buckets[bucket_index];
		while (entry) {
			if (entry->hash == hash && map->m_compare_func(entry->key, key) == 0) {
				return &entry->value;
			}
			entry = entry->next;
		}

		return nullptr;
	}

	template<TrivialType K, TrivialType V>
	bool hashmap_remove(HashMap<K, V>* map, const K& key, V* out_value) {
		if (!map) {
			return false;
		}

		usize hash = map->m_hash_func(key);
		usize bucket_index = hash % map->m_bucket_count;

		HashMapEntry<K, V>* entry = map->m_buckets[bucket_index];
		HashMapEntry<K, V>* prev = nullptr;

		while (entry) {
			if (entry->hash == hash && map->m_compare_func(entry->key, key) == 0) {
				if (out_value) {
					*out_value = entry->value;
				}

				if (prev) {
					prev->next = entry->next;
				}
				else {
					map->m_buckets[bucket_index] = entry->next;
				}

				detail::destroy_entry(map->m_allocator, entry);
				map->m_size--;

				return true;
			}

			prev = entry;
			entry = entry->next;
		}

		return false;
	}

	template<TrivialType K, TrivialType V>
	bool hashmap_contains(const HashMap<K, V>* map, const K& key) {
		return hashmap_get(map, key) != nullptr;
	}

	template<TrivialType K, TrivialType V>
	usize hashmap_size(const HashMap<K, V>* map) {
		return map ? map->m_size : 0;
	}

	template<TrivialType K, TrivialType V>
	bool hashmap_empty(const HashMap<K, V>* map) {
		return !map || map->m_size == 0;
	}

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
