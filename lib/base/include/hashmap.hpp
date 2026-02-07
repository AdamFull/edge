#ifndef EDGE_HASHMAP_H
#define EDGE_HASHMAP_H

#include "allocator.hpp"
#include "hash.hpp"
#include "math.hpp"

namespace edge {
namespace detail {
constexpr usize HASHMAP_DEFAULT_BUCKET_COUNT = 16;
constexpr f32 HASHMAP_MAX_LOAD_FACTOR = 0.75f;
} // namespace detail

template <typename K, typename V> struct HashMapEntry {
  K key = {};
  V value = {};
  usize hash = 0;
  HashMapEntry *next = nullptr;
};

template <typename K, typename V, typename Hash = Hash<K>,
          typename KeyEqual = std::equal_to<K>>
struct HashMap {
  using hasher = Hash;
  using key_type = K;
  using mapped_type = V;
  using key_equal = KeyEqual;

  using value_type = HashMapEntry<K, V>;
  using size_type = usize;

  struct Iterator {
    const HashMap *map;
    usize bucket_index;
    HashMapEntry<K, V> *current;

    bool operator==(const Iterator &other) const {
      return current == other.current;
    }

    bool operator!=(const Iterator &other) const {
      return current != other.current;
    }

    HashMapEntry<K, V> &operator*() const { return *current; }
    HashMapEntry<K, V> *operator->() const { return current; }

    Iterator &operator++() {
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

    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }
  };

  bool create(const NotNull<const Allocator *> alloc,
              usize initial_bucket_count = 0ull) {
    if (initial_bucket_count == 0ull) {
      initial_bucket_count = detail::HASHMAP_DEFAULT_BUCKET_COUNT;
    }

    if (!is_pow2(initial_bucket_count)) {
      initial_bucket_count = next_pow2(initial_bucket_count);
    }

    m_buckets =
        alloc->allocate_array<HashMapEntry<K, V> *>(initial_bucket_count);
    if (!m_buckets) {
      return false;
    }

    m_bucket_count = initial_bucket_count;
    m_size = 0ull;

    return true;
  }

  void destroy(const NotNull<const Allocator *> alloc) {
    clear(alloc);

    if (m_buckets) {
      alloc->deallocate_array(m_buckets, m_bucket_count);
    }
  }

  void clear(const NotNull<const Allocator *> alloc) {
    for (usize i = 0; i < m_bucket_count; i++) {
      HashMapEntry<K, V> *entry = m_buckets[i];
      while (entry) {
        entry->key.~K();
        entry->value.~V();

        HashMapEntry<K, V> *next = entry->next;
        alloc->deallocate(entry);
        entry = next;
      }
      m_buckets[i] = nullptr;
    }

    m_size = 0;
  }

  bool rehash(const NotNull<const Allocator *> alloc, usize new_bucket_count) {
    if (new_bucket_count == 0) {
      return false;
    }

    if (!is_pow2(new_bucket_count)) {
      new_bucket_count = next_pow2(new_bucket_count);
    }

    HashMapEntry<K, V> **new_buckets =
        alloc->allocate_array<HashMapEntry<K, V> *>(new_bucket_count);
    if (!new_buckets) {
      return false;
    }

    // Rehash all entries
    for (usize i = 0; i < m_bucket_count; i++) {
      HashMapEntry<K, V> *entry = m_buckets[i];
      while (entry) {
        HashMapEntry<K, V> *next = entry->next;

        usize bucket_index = entry->hash & (new_bucket_count - 1);
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

  f32 load_factor() const {
    if (m_bucket_count == 0) {
      return 0.0f;
    }
    return static_cast<f32>(m_size) / static_cast<f32>(m_bucket_count);
  }

  bool insert(const NotNull<const Allocator *> alloc, const K &key,
              const V &value) {
    // Check load factor and rehash if needed
    if (load_factor() >= detail::HASHMAP_MAX_LOAD_FACTOR) {
      rehash(alloc, m_bucket_count * 2);
    }

    usize hash = Hash{}(key);
    usize bucket_index = hash & (m_bucket_count - 1);

    // Check if key already exists
    HashMapEntry<K, V> *entry = m_buckets[bucket_index];
    while (entry) {
      if (entry->hash == hash && KeyEqual{}(entry->key, key)) {
        // Update existing value
        entry->value = value;
        return true;
      }
      entry = entry->next;
    }

    // Create new entry
    HashMapEntry<K, V> *new_entry =
        alloc->allocate<HashMapEntry<K, V>>(key, value, hash, nullptr);
    if (!new_entry) {
      return false;
    }

    new_entry->next = m_buckets[bucket_index];
    m_buckets[bucket_index] = new_entry;
    m_size++;

    return true;
  }

  Iterator find(const K &key) {
    usize hash = Hash{}(key);
    usize bucket_index = hash & (m_bucket_count - 1);

    HashMapEntry<K, V> *entry = m_buckets[bucket_index];
    while (entry) {
      if (entry->hash == hash && KeyEqual{}(entry->key, key)) {
        return Iterator{
            .map = this, .bucket_index = bucket_index, .current = entry};
      }
      entry = entry->next;
    }

    return end();
  }

  Iterator find(const K &key) const {
    return const_cast<HashMap *>(this)->find(key);
  }

  V &operator[](const K &key) {
    if (auto found = find(key); found != end()) {
      return found->value;
    }

    assert(false && "Key not found in HashMap::operator[]");
    static V dummy{};
    return dummy;
  }

  const V &operator[](const K &key) const {
    if (auto found = find(key); found != end()) {
      return found->value;
    }

    assert(false && "Key not found in HashMap::operator[] const");
    static V dummy{};
    return dummy;
  }

  bool remove(const NotNull<const Allocator *> alloc, const K &key,
              V *out_value = nullptr) {
    usize hash = Hash{}(key);
    usize bucket_index = hash % m_bucket_count;

    HashMapEntry<K, V> *entry = m_buckets[bucket_index];
    HashMapEntry<K, V> *prev = nullptr;

    while (entry) {
      if (entry->hash == hash && KeyEqual{}(entry->key, key)) {
        if (out_value) {
          *out_value = entry->value;
        }

        if (prev) {
          prev->next = entry->next;
        } else {
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

  Iterator begin() {
    Iterator it;
    it.map = this;
    it.bucket_index = 0;
    it.current = nullptr;

    for (usize i = 0; i < m_bucket_count; i++) {
      if (m_buckets[i]) {
        return Iterator{
            .map = this, .bucket_index = i, .current = m_buckets[i]};
      }
    }

    return Iterator{this, 0, nullptr};
  }

  Iterator end() { return Iterator{this, 0, nullptr}; }
  Iterator begin() const { return const_cast<HashMap *>(this)->begin(); }
  Iterator end() const { return const_cast<HashMap *>(this)->end(); }

  [[nodiscard]] bool contains(const K &key) const { return find(key) != end(); }
  [[nodiscard]] bool empty() const { return m_size == 0; }
  [[nodiscard]] usize size() const { return m_size; }

  HashMapEntry<K, V> **m_buckets = nullptr;
  usize m_bucket_count = 0ull;
  usize m_size = 0ull;
};
} // namespace edge

#endif // !EDGE_HASHMAP_H
