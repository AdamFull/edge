#ifndef EDGE_LIST_H
#define EDGE_LIST_H

#include "allocator.hpp"

namespace edge {
template <TrivialType T> struct ListNode {
  T data;
  ListNode *next = nullptr;
  ListNode *prev = nullptr;
};

namespace detail {
template <TrivialType T, typename Comparator>
ListNode<T> *merge_sorted(ListNode<T> *left, ListNode<T> *right,
                          Comparator &&compare) {
  if (!left) {
    return right;
  }

  if (!right) {
    return left;
  }

  ListNode<T> *result = nullptr;

  if (compare(left->data, right->data) <= 0) {
    result = left;
    result->next = merge_sorted(left->next, right, compare);
    if (result->next) {
      result->next->prev = result;
    }
    result->prev = nullptr;
  } else {
    result = right;
    result->next = merge_sorted(left, right->next, compare);
    if (result->next) {
      result->next->prev = result;
    }
    result->prev = nullptr;
  }

  return result;
}

template <TrivialType T, typename Comparator>
ListNode<T> *merge_sort_nodes(ListNode<T> *head, Comparator &&compare) {
  if (!head || !head->next) {
    return head;
  }

  ListNode<T> *slow = head;
  ListNode<T> *fast = head->next;

  while (fast && fast->next) {
    slow = slow->next;
    fast = fast->next->next;
  }

  ListNode<T> *mid = slow->next;
  slow->next = nullptr;
  if (mid) {
    mid->prev = nullptr;
  }

  ListNode<T> *left = merge_sort_nodes(head, compare);
  ListNode<T> *right = merge_sort_nodes(mid, compare);

  return merge_sorted(left, right, compare);
}
} // namespace detail

template <TrivialType T> struct List {
  ListNode<T> *m_head = nullptr;
  ListNode<T> *m_tail = nullptr;
  usize m_size = 0ull;

  struct Iterator {
    ListNode<T> *current;

    bool operator==(const Iterator &other) const {
      return current == other.current;
    }

    bool operator!=(const Iterator &other) const {
      return current != other.current;
    }

    T &operator*() const { return current->data; }

    T *operator->() const { return &current->data; }

    Iterator &operator++() {
      if (current) {
        current = current->next;
      }
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp = *this;
      ++*this;
      return tmp;
    }

    Iterator &operator--() {
      if (current) {
        current = current->prev;
      }
      return *this;
    }

    Iterator operator--(int) {
      Iterator tmp = *this;
      --*this;
      return tmp;
    }
  };

  void clear(const NotNull<const Allocator *> alloc) {
    ListNode<T> *current = m_head;
    while (current) {
      ListNode<T> *next = current->next;
      alloc->deallocate(current);
      current = next;
    }

    m_head = nullptr;
    m_tail = nullptr;
    m_size = 0;
  }

  bool push_front(const NotNull<const Allocator *> alloc, const T &element) {
    auto *node = alloc->allocate<ListNode<T>>(element, nullptr, nullptr);
    if (!node) {
      return false;
    }

    if (m_head) {
      node->next = m_head;
      m_head->prev = node;
      m_head = node;
    } else {
      m_head = node;
      m_tail = node;
    }

    m_size++;
    return true;
  }

  void destroy(const NotNull<const Allocator *> alloc) { clear(alloc); }

  bool push_back(const NotNull<const Allocator *> alloc, const T &element) {
    ListNode<T> *node = alloc->allocate<ListNode<T>>(element, nullptr, nullptr);
    if (!node) {
      return false;
    }

    if (m_tail) {
      node->prev = m_tail;
      m_tail->next = node;
      m_tail = node;
    } else {
      m_head = node;
      m_tail = node;
    }

    m_size++;
    return true;
  }

  bool pop_front(const NotNull<const Allocator *> alloc, T *out_element) {
    if (!m_head) {
      return false;
    }

    ListNode<T> *node = m_head;

    if (out_element) {
      *out_element = node->data;
    }

    m_head = node->next;
    if (m_head) {
      m_head->prev = nullptr;
    } else {
      m_tail = nullptr;
    }

    alloc->deallocate(node);
    m_size--;

    return true;
  }

  bool pop_back(const NotNull<const Allocator *> alloc, T *out_element) {
    if (!m_tail) {
      return false;
    }

    ListNode<T> *node = m_tail;

    if (out_element) {
      *out_element = node->data;
    }

    m_tail = node->prev;
    if (m_tail) {
      m_tail->next = nullptr;
    } else {
      m_head = nullptr;
    }

    alloc->deallocate(node);
    m_size--;

    return true;
  }

  T *front() {
    if (!m_head) {
      return nullptr;
    }
    return &m_head->data;
  }

  T *back() {
    if (!m_tail) {
      return nullptr;
    }
    return &m_tail->data;
  }

  T *get(const usize index) const {
    if (index >= m_size) {
      return nullptr;
    }

    ListNode<T> *current;

    if (index < m_size / 2) {
      current = m_head;
      for (usize i = 0; i < index; i++) {
        current = current->next;
      }
    } else {
      current = m_tail;
      for (usize i = m_size - 1; i > index; i--) {
        current = current->prev;
      }
    }

    return current ? &current->data : nullptr;
  }

  bool insert(const NotNull<const Allocator *> alloc, const usize index, const T &element) {
    if (index > m_size) {
      return false;
    }

    if (index == 0) {
      return push_front(alloc, element);
    }

    if (index == m_size) {
      return push_back(alloc, element);
    }

    ListNode<T> *new_node =
        alloc->allocate<ListNode<T>>(element, nullptr, nullptr);
    if (!new_node) {
      return false;
    }

    ListNode<T> *current = m_head;
    for (usize i = 0; i < index; i++) {
      current = current->next;
    }

    new_node->prev = current->prev;
    new_node->next = current;
    current->prev->next = new_node;
    current->prev = new_node;

    m_size++;
    return true;
  }

  bool remove(const NotNull<const Allocator *> alloc, const usize index, T *out_element) {
    if (index >= m_size) {
      return false;
    }

    if (index == 0) {
      return pop_front(alloc, out_element);
    }

    if (index == m_size - 1) {
      return pop_back(alloc, out_element);
    }

    ListNode<T> *current = m_head;
    for (usize i = 0; i < index; i++) {
      current = current->next;
    }

    if (out_element) {
      *out_element = current->data;
    }

    current->prev->next = current->next;
    current->next->prev = current->prev;

    alloc->deallocate(current);
    m_size--;

    return true;
  }

  bool empty() const { return m_size == 0; }

  usize size() const { return m_size; }

  ListNode<T> *find(const T &element) {
    ListNode<T> *current = m_head;
    while (current) {
      if (current->data == element) {
        return current;
      }
      current = current->next;
    }

    return nullptr;
  }

  template <typename Predicate> ListNode<T> *find_if(Predicate pred) {
    ListNode<T> *current = m_head;
    while (current) {
      if (pred(current->data)) {
        return current;
      }
      current = current->next;
    }

    return nullptr;
  }

  void reverse() {
    if (m_size < 2) {
      return;
    }

    ListNode<T> *current = m_head;
    ListNode<T> *temp = nullptr;

    while (current) {
      temp = current->prev;
      current->prev = current->next;
      current->next = temp;
      current = current->prev;
    }

    temp = m_head;
    m_head = m_tail;
    m_tail = temp;
  }

  template <typename Comparator> void sort(Comparator &&compare) {
    if (m_size < 2) {
      return;
    }

    m_head = detail::merge_sort_nodes(m_head, compare);

    m_tail = m_head;
    while (m_tail && m_tail->next) {
      m_tail = m_tail->next;
    }
  }

  Iterator begin() {
    return Iterator{m_head};
  }

  Iterator end() {
    return Iterator{nullptr};
  }

  Iterator begin() const {
    return Iterator{m_head};
  }

  Iterator end() const {
    return {nullptr};
  }
};
} // namespace edge

#endif