#ifndef EDGE_LIST_H
#define EDGE_LIST_H

#include "allocator.hpp"

namespace edge {
	template<TrivialType T>
	struct ListNode {
		T data;
		ListNode* next = nullptr;
		ListNode* prev = nullptr;
	};

	template<TrivialType T>
	struct List {
		ListNode<T>* m_head = nullptr;
		ListNode<T>* m_tail = nullptr;
		usize m_size = 0ull;
		const Allocator* m_allocator = nullptr;
	};

	template<TrivialType T>
	struct ListIterator {
		ListNode<T>* current;

		bool operator==(const ListIterator& other) const {
			return current == other.current;
		}

		bool operator!=(const ListIterator& other) const {
			return current != other.current;
		}

		T& operator*() const {
			return current->data;
		}

		T* operator->() const {
			return &current->data;
		}

		ListIterator& operator++() {
			if (current) {
				current = current->next;
			}
			return *this;
		}

		ListIterator operator++(int) {
			ListIterator tmp = *this;
			++(*this);
			return tmp;
		}

		ListIterator& operator--() {
			if (current) {
				current = current->prev;
			}
			return *this;
		}

		ListIterator operator--(int) {
			ListIterator tmp = *this;
			--(*this);
			return tmp;
		}
	};

	namespace detail {
		template<TrivialType T, typename Comparator>
		ListNode<T>* merge_sorted(ListNode<T>* left, ListNode<T>* right, Comparator&& compare) {
			if (!left) {
				return right;
			}

			if (!right) {
				return left;
			}

			ListNode<T>* result = nullptr;

			if (compare(left->data, right->data) <= 0) {
				result = left;
				result->next = merge_sorted(left->next, right, compare);
				if (result->next) {
					result->next->prev = result;
				}
				result->prev = nullptr;
			}
			else {
				result = right;
				result->next = merge_sorted(left, right->next, compare);
				if (result->next) {
					result->next->prev = result;
				}
				result->prev = nullptr;
			}

			return result;
		}

		template<TrivialType T, typename Comparator>
		ListNode<T>* merge_sort_nodes(ListNode<T>* head, Comparator&& compare) {
			if (!head || !head->next) {
				return head;
			}

			ListNode<T>* slow = head;
			ListNode<T>* fast = head->next;

			while (fast && fast->next) {
				slow = slow->next;
				fast = fast->next->next;
			}

			ListNode<T>* mid = slow->next;
			slow->next = nullptr;
			if (mid) {
				mid->prev = nullptr;
			}

			ListNode<T>* left = merge_sort_nodes(head, compare);
			ListNode<T>* right = merge_sort_nodes(mid, compare);

			return merge_sorted(left, right, compare);
		}
	}

	template<TrivialType T>
	bool list_create(const Allocator* alloc, List<T>* list) {
		if (!alloc || !list) {
			return false;
		}

		list->m_head = nullptr;
		list->m_tail = nullptr;
		list->m_size = 0;
		list->m_allocator = alloc;

		return true;
	}

	template<TrivialType T>
	void list_clear(List<T>* list) {
		if (!list) {
			return;
		}

		ListNode<T>* current = list->m_head;
		while (current) {
			ListNode<T>* next = current->next;
			list->m_allocator->deallocate(current);
			current = next;
		}

		// TODO: Call destructor for not trivially destructable

		list->m_head = nullptr;
		list->m_tail = nullptr;
		list->m_size = 0;
	}

	template<TrivialType T>
	bool list_push_front(List<T>* list, const T& element) {
		if (!list) {
			return false;
		}

		ListNode<T>* node = list->m_allocator->allocate<ListNode<T>>(element, nullptr, nullptr);
		if (!node) {
			return false;
		}

		if (list->m_head) {
			node->next = list->m_head;
			list->m_head->prev = node;
			list->m_head = node;
		}
		else {
			list->m_head = node;
			list->m_tail = node;
		}

		list->m_size++;
		return true;
	}

	template<TrivialType T>
	void list_destroy(List<T>* list) {
		if (!list) {
			return;
		}

		list_clear(list);
	}

	template<TrivialType T>
	bool list_push_back(List<T>* list, const T& element) {
		if (!list) {
			return false;
		}

		ListNode<T>* node = list->m_allocator->allocate<ListNode<T>>(element, nullptr, nullptr);
		if (!node) {
			return false;
		}

		if (list->m_tail) {
			node->prev = list->m_tail;
			list->m_tail->next = node;
			list->m_tail = node;
		}
		else {
			list->m_head = node;
			list->m_tail = node;
		}

		list->m_size++;
		return true;
	}

	template<TrivialType T>
	bool list_pop_front(List<T>* list, T* out_element) {
		if (!list || !list->m_head) {
			return false;
		}

		ListNode<T>* node = list->m_head;

		if (out_element) {
			*out_element = node->data;
		}

		list->m_head = node->next;
		if (list->m_head) {
			list->m_head->prev = nullptr;
		}
		else {
			list->m_tail = nullptr;
		}

		list->m_allocator->deallocate(node);
		list->m_size--;

		return true;
	}

	template<TrivialType T>
	bool list_pop_back(List<T>* list, T* out_element) {
		if (!list || !list->m_tail) {
			return false;
		}

		ListNode<T>* node = list->m_tail;

		if (out_element) {
			*out_element = node->data;
		}

		list->m_tail = node->prev;
		if (list->m_tail) {
			list->m_tail->next = nullptr;
		}
		else {
			list->m_head = nullptr;
		}

		list->m_allocator->deallocate(node);
		list->m_size--;

		return true;
	}

	template<TrivialType T>
	T* list_front(const List<T>* list) {
		if (!list || !list->m_head) {
			return nullptr;
		}
		return &list->m_head->data;
	}

	template<TrivialType T>
	T* list_back(const List<T>* list) {
		if (!list || !list->m_tail) {
			return nullptr;
		}
		return &list->m_tail->data;
	}

	template<TrivialType T>
	T* list_get(const List<T>* list, usize index) {
		if (!list || index >= list->m_size) {
			return nullptr;
		}

		ListNode<T>* current;

		if (index < list->m_size / 2) {
			current = list->m_head;
			for (usize i = 0; i < index; i++) {
				current = current->next;
			}
		}
		else {
			current = list->m_tail;
			for (usize i = list->m_size - 1; i > index; i--) {
				current = current->prev;
			}
		}

		return current ? &current->data : nullptr;
	}

	template<TrivialType T>
	bool list_insert(List<T>* list, usize index, const T& element) {
		if (!list || index > list->m_size) {
			return false;
		}

		if (index == 0) {
			return list_push_front(list, element);
		}

		if (index == list->m_size) {
			return list_push_back(list, element);
		}

		ListNode<T>* new_node = list->m_allocator->allocate<ListNode<T>>(element, nullptr, nullptr);
		if (!new_node) {
			return false;
		}

		ListNode<T>* current = list->m_head;
		for (usize i = 0; i < index; i++) {
			current = current->next;
		}

		new_node->prev = current->prev;
		new_node->next = current;
		current->prev->next = new_node;
		current->prev = new_node;

		list->m_size++;
		return true;
	}

	template<TrivialType T>
	bool list_remove(List<T>* list, usize index, T* out_element) {
		if (!list || index >= list->m_size) {
			return false;
		}

		if (index == 0) {
			return list_pop_front(list, out_element);
		}

		if (index == list->m_size - 1) {
			return list_pop_back(list, out_element);
		}

		ListNode<T>* current = list->m_head;
		for (usize i = 0; i < index; i++) {
			current = current->next;
		}

		if (out_element) {
			*out_element = current->data;
		}

		current->prev->next = current->next;
		current->next->prev = current->prev;

		list->m_allocator->deallocate(current);
		list->m_size--;

		return true;
	}

	template<TrivialType T>
	usize list_size(const List<T>* list) {
		return list ? list->m_size : 0;
	}

	template<TrivialType T>
	bool list_empty(const List<T>* list) {
		return !list || list->m_size == 0;
	}

	template<TrivialType T>
	ListNode<T>* list_find(const List<T>* list, const T& element) {
		if (!list) {
			return nullptr;
		}

		ListNode<T>* current = list->m_head;
		while (current) {
			if (current->data == element) {
				return current;
			}
			current = current->next;
		}

		return nullptr;
	}

	template<TrivialType T, typename Predicate>
	ListNode<T>* list_find_if(const List<T>* list, Predicate pred) {
		if (!list) {
			return nullptr;
		}

		ListNode<T>* current = list->m_head;
		while (current) {
			if (pred(current->data)) {
				return current;
			}
			current = current->next;
		}

		return nullptr;
	}

	template<TrivialType T>
	void list_reverse(List<T>* list) {
		if (!list || list->m_size < 2) {
			return;
		}

		ListNode<T>* current = list->m_head;
		ListNode<T>* temp = nullptr;

		while (current) {
			temp = current->prev;
			current->prev = current->next;
			current->next = temp;
			current = current->prev;
		}

		temp = list->m_head;
		list->m_head = list->m_tail;
		list->m_tail = temp;
	}

	template<TrivialType T, typename Comparator>
	void list_sort(List<T>* list, Comparator&& compare) {
		if (!list || list->m_size < 2) {
			return;
		}

		list->m_head = detail::merge_sort_nodes(list->m_head, compare);

		list->m_tail = list->m_head;
		while (list->m_tail && list->m_tail->next) {
			list->m_tail = list->m_tail->next;
		}
	}

	template<TrivialType T>
	inline ListIterator<T> begin(List<T>& list) {
		return ListIterator<T>{ list.m_head };
	}

	template<TrivialType T>
	inline ListIterator<T> end(List<T>& list) {
		return ListIterator<T>{ nullptr };
	}

	template<TrivialType T>
	inline ListIterator<T> begin(const List<T>& list) {
		return ListIterator<T>{ list.m_head };
	}

	template<TrivialType T>
	inline ListIterator<T> end(const List<T>& list) {
		return ListIterator<T>{ nullptr };
	}
}

#endif