#ifndef EDGE_HASHMAP_H
#define EDGE_HASHMAP_H

#include "allocator.hpp"
#include "hash.hpp"

namespace edge {
	namespace detail {
		constexpr usize HASHMAP_DEFAULT_BUCKET_COUNT = 16;
		constexpr f32 HASHMAP_MAX_LOAD_FACTOR = 0.75f;
	}

	template<TrivialType K, TrivialType V>
	struct HashMapEntry;

	template<TrivialType K, TrivialType V, typename Hash, typename KeyEqual>
	struct HashMap;

	template<TrivialType K, TrivialType V, typename Hash, typename KeyEqual>
	struct HashMapIterator {
		const HashMap<K, V, Hash, KeyEqual>* map;
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

	template<TrivialType K, TrivialType V,
		typename Hash = Hash<K>,
		typename KeyEqual = std::equal_to<K>>
	struct HashMap {
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
					entry->key.~K();
					entry->value.~V();

					HashMapEntry<K, V>* next = entry->next;
					alloc->deallocate(entry);
					entry = next;
				}
				m_buckets[i] = nullptr;
			}

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

			usize hash = Hash{}(key);
			usize bucket_index = hash % m_bucket_count;

			// Check if key already exists
			HashMapEntry<K, V>* entry = m_buckets[bucket_index];
			while (entry) {
				if (entry->hash == hash && KeyEqual{}(entry->key, key)) {
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
			usize hash = Hash{}(key);
			usize bucket_index = hash % m_bucket_count;

			HashMapEntry<K, V>* entry = m_buckets[bucket_index];
			while (entry) {
				if (entry->hash == hash && KeyEqual{}(entry->key, key)) {
					return &entry->value;
				}
				entry = entry->next;
			}

			return nullptr;
		}

		const V* get(const K& key) const noexcept {
			usize hash = Hash{}(key);
			usize bucket_index = hash % m_bucket_count;

			HashMapEntry<K, V>* entry = m_buckets[bucket_index];
			while (entry) {
				if (entry->hash == hash && KeyEqual{}(entry->key, key)) {
					return &entry->value;
				}
				entry = entry->next;
			}

			return nullptr;
		}

		V& operator[](const K& key) {
			if (V* found = get(key)) {
				return *found;
			}

			assert(false && "Key not found in HashMap::operator[]");
			static V dummy{};
			return dummy;
		}

		const V& operator[](const K& key) const {
			if (V* found = get(key)) {
				return *found;
			}

			assert(false && "Key not found in HashMap::operator[] const");
			static V dummy{};
			return dummy;
		}

		bool remove(NotNull<const Allocator*> alloc, const K& key, V* out_value = nullptr) {
			usize hash = Hash{}(key);
			usize bucket_index = hash % m_bucket_count;

			HashMapEntry<K, V>* entry = m_buckets[bucket_index];
			HashMapEntry<K, V>* prev = nullptr;

			while (entry) {
				if (entry->hash == hash && KeyEqual{}(entry->key, key)) {
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

		usize size() const noexcept {
			return m_size;
		}

		HashMapEntry<K, V>** m_buckets = nullptr;
		usize m_bucket_count = 0ull;
		usize m_size = 0ull;
	};

	template<TrivialType K, TrivialType V, typename Hash, typename KeyEqual>
	inline HashMapIterator<K, V, Hash, KeyEqual> begin(HashMap<K, V, Hash, KeyEqual>& map) {
		HashMapIterator<K, V, Hash, KeyEqual> it;
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

	template<TrivialType K, TrivialType V, typename Hash, typename KeyEqual>
	inline HashMapIterator<K, V, Hash, KeyEqual> end(HashMap<K, V, Hash, KeyEqual>& map) {
		return HashMapIterator<K, V, Hash, KeyEqual>{ &map, 0, nullptr };
	}

	template<TrivialType K, TrivialType V, typename Hash, typename KeyEqual>
	inline HashMapIterator<K, V, Hash, KeyEqual> begin(const HashMap<K, V, Hash, KeyEqual>& map) {
		HashMapIterator<K, V, Hash, KeyEqual> it;
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

	template<TrivialType K, TrivialType V, typename Hash, typename KeyEqual>
	inline HashMapIterator<K, V, Hash, KeyEqual> end(const HashMap<K, V, Hash, KeyEqual>& map) {
		return HashMapIterator<K, V, Hash, KeyEqual>{ &map, 0, nullptr };
	}
}

#endif // !EDGE_HASHMAP_H
