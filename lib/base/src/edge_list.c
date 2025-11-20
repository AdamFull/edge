#include "edge_list.h"
#include "edge_allocator.h"

#include <string.h>

static edge_list_node_t* create_node(const edge_allocator_t* allocator, size_t element_size, const void* element) {
    edge_list_node_t* node = (edge_list_node_t*)edge_allocator_malloc(allocator, sizeof(edge_list_node_t));
    if (!node) {
        return NULL;
    }

    node->data = edge_allocator_malloc(allocator, element_size);
    if (!node->data) {
        edge_allocator_free(allocator, node);
        return NULL;
    }

    memcpy(node->data, element, element_size);
    node->next = NULL;
    node->prev = NULL;

    return node;
}

static void destroy_node(const edge_allocator_t* allocator, edge_list_node_t* node) {
    if (!node) {
        return;
    }

    if (node->data) {
        edge_allocator_free(allocator, node->data);
    }
    edge_allocator_free(allocator, node);
}

edge_list_t* edge_list_create(const edge_allocator_t* allocator, size_t element_size) {
    if (!allocator || element_size == 0) {
        return NULL;
    }

    edge_list_t* list = (edge_list_t*)edge_allocator_malloc(allocator, sizeof(edge_list_t));
    if (!list) {
        return NULL;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    list->element_size = element_size;
    list->allocator = allocator;

    return list;
}

void edge_list_destroy(edge_list_t* list) {
    if (!list) {
        return;
    }

    edge_list_clear(list);
    edge_allocator_free(list->allocator, list);
}

void edge_list_clear(edge_list_t* list) {
    if (!list) {
        return;
    }

    edge_list_node_t* current = list->head;
    while (current) {
        edge_list_node_t* next = current->next;
        destroy_node(list->allocator, current);
        current = next;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

bool edge_list_push_front(edge_list_t* list, const void* element) {
    if (!list || !element) {
        return false;
    }

    edge_list_node_t* node = create_node(list->allocator, list->element_size, element);
    if (!node) {
        return false;
    }

    if (list->head) {
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    else {
        list->head = node;
        list->tail = node;
    }

    list->size++;
    return true;
}

bool edge_list_push_back(edge_list_t* list, const void* element) {
    if (!list || !element) {
        return false;
    }

    edge_list_node_t* node = create_node(list->allocator, list->element_size, element);
    if (!node) {
        return false;
    }

    if (list->tail) {
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }
    else {
        list->head = node;
        list->tail = node;
    }

    list->size++;
    return true;
}

bool edge_list_pop_front(edge_list_t* list, void* out_element) {
    if (!list || !list->head) {
        return false;
    }

    edge_list_node_t* node = list->head;

    if (out_element) {
        memcpy(out_element, node->data, list->element_size);
    }

    list->head = node->next;
    if (list->head) {
        list->head->prev = NULL;
    }
    else {
        list->tail = NULL;
    }

    destroy_node(list->allocator, node);
    list->size--;

    return true;
}

bool edge_list_pop_back(edge_list_t* list, void* out_element) {
    if (!list || !list->tail) {
        return false;
    }

    edge_list_node_t* node = list->tail;

    if (out_element) {
        memcpy(out_element, node->data, list->element_size);
    }

    list->tail = node->prev;
    if (list->tail) {
        list->tail->next = NULL;
    }
    else {
        list->head = NULL;
    }

    destroy_node(list->allocator, node);
    list->size--;

    return true;
}

void* edge_list_front(const edge_list_t* list) {
    if (!list || !list->head) {
        return NULL;
    }
    return list->head->data;
}

void* edge_list_back(const edge_list_t* list) {
    if (!list || !list->tail) {
        return NULL;
    }
    return list->tail->data;
}

void* edge_list_get(const edge_list_t* list, size_t index) {
    if (!list || index >= list->size) {
        return NULL;
    }

    edge_list_node_t* current;

    /* Optimize by choosing direction */
    if (index < list->size / 2) {
        current = list->head;
        for (size_t i = 0; i < index; i++) {
            current = current->next;
        }
    }
    else {
        current = list->tail;
        for (size_t i = list->size - 1; i > index; i--) {
            current = current->prev;
        }
    }

    return current ? current->data : NULL;
}

bool edge_list_insert(edge_list_t* list, size_t index, const void* element) {
    if (!list || !element || index > list->size) {
        return false;
    }

    if (index == 0) {
        return edge_list_push_front(list, element);
    }

    if (index == list->size) {
        return edge_list_push_back(list, element);
    }

    edge_list_node_t* new_node = create_node(list->allocator, list->element_size, element);
    if (!new_node) {
        return false;
    }

    edge_list_node_t* current = list->head;
    for (size_t i = 0; i < index; i++) {
        current = current->next;
    }

    new_node->prev = current->prev;
    new_node->next = current;
    current->prev->next = new_node;
    current->prev = new_node;

    list->size++;
    return true;
}

bool edge_list_remove(edge_list_t* list, size_t index, void* out_element) {
    if (!list || index >= list->size) {
        return false;
    }

    if (index == 0) {
        return edge_list_pop_front(list, out_element);
    }

    if (index == list->size - 1) {
        return edge_list_pop_back(list, out_element);
    }

    edge_list_node_t* current = list->head;
    for (size_t i = 0; i < index; i++) {
        current = current->next;
    }

    if (out_element) {
        memcpy(out_element, current->data, list->element_size);
    }

    current->prev->next = current->next;
    current->next->prev = current->prev;

    destroy_node(list->allocator, current);
    list->size--;

    return true;
}

size_t edge_list_size(const edge_list_t* list) {
    return list ? list->size : 0;
}

bool edge_list_empty(const edge_list_t* list) {
    return !list || list->size == 0;
}

edge_list_node_t* edge_list_find(const edge_list_t* list, const void* element, int (*compare)(const void*, const void*)) {
    if (!list || !element || !compare) {
        return NULL;
    }

    edge_list_node_t* current = list->head;
    while (current) {
        if (compare(current->data, element) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

void edge_list_reverse(edge_list_t* list) {
    if (!list || list->size < 2) {
        return;
    }

    edge_list_node_t* current = list->head;
    edge_list_node_t* temp = NULL;

    while (current) {
        temp = current->prev;
        current->prev = current->next;
        current->next = temp;
        current = current->prev;
    }

    temp = list->head;
    list->head = list->tail;
    list->tail = temp;
}

static edge_list_node_t* merge_sorted(edge_list_node_t* left, edge_list_node_t* right, int (*compare)(const void*, const void*)) {
    if (!left) {
        return right;
    }

    if (!right) {
        return left;
    }

    edge_list_node_t* result = NULL;

    if (compare(left->data, right->data) <= 0) {
        result = left;
        result->next = merge_sorted(left->next, right, compare);
        if (result->next) result->next->prev = result;
        result->prev = NULL;
    }
    else {
        result = right;
        result->next = merge_sorted(left, right->next, compare);
        if (result->next) result->next->prev = result;
        result->prev = NULL;
    }

    return result;
}

static edge_list_node_t* merge_sort_nodes(edge_list_node_t* head, int (*compare)(const void*, const void*)) {
    if (!head || !head->next) {
        return head;
    }

    /* Find middle */
    edge_list_node_t* slow = head;
    edge_list_node_t* fast = head->next;

    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }

    edge_list_node_t* mid = slow->next;
    slow->next = NULL;
    if (mid) mid->prev = NULL;

    /* Recursively sort */
    edge_list_node_t* left = merge_sort_nodes(head, compare);
    edge_list_node_t* right = merge_sort_nodes(mid, compare);

    return merge_sorted(left, right, compare);
}

void edge_list_sort(edge_list_t* list, int (*compare)(const void*, const void*)) {
    if (!list || !compare || list->size < 2) {
        return;
    }

    list->head = merge_sort_nodes(list->head, compare);

    /* Update tail */
    list->tail = list->head;
    while (list->tail && list->tail->next) {
        list->tail = list->tail->next;
    }
}

edge_list_iterator_t edge_list_begin(const edge_list_t* list) {
    edge_list_iterator_t it;
    it.current = list ? list->head : NULL;
    return it;
}

edge_list_iterator_t edge_list_end(const edge_list_t* list) {
    edge_list_iterator_t it;
    it.current = NULL;
    (void)list;
    return it;
}

bool edge_list_iterator_valid(const edge_list_iterator_t* it) {
    return it && it->current != NULL;
}

void edge_list_iterator_next(edge_list_iterator_t* it) {
    if (it && it->current) {
        it->current = it->current->next;
    }
}

void edge_list_iterator_prev(edge_list_iterator_t* it) {
    if (it && it->current) {
        it->current = it->current->prev;
    }
}

void* edge_list_iterator_get(const edge_list_iterator_t* it) {
    if (!it || !it->current) {
        return NULL;
    }
    return it->current->data;
}
