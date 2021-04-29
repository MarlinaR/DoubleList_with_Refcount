#pragma once

#include "CLinkedList.hpp"

template<typename ValueType>
class ListIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = ValueType;
    using reference = ValueType&;
    using pointer = ValueType*;

    template<typename> friend class CLinkedList;
    ListIterator()noexcept = default;
    ListIterator(const  ListIterator& other) : ptr(other.ptr), list(other.list)
    {
        list->inc_ref_count(ptr);
    }
    ListIterator(Node<value_type>* _new_ptr, CLinkedList<value_type>* _list) : ptr(_new_ptr), list(_list)
    {
        list->inc_ref_count(ptr);
    }

    ~ListIterator() {
        if (!ptr) return;
        list->dec_ref_count(ptr);
    }

    ListIterator& operator=(const  ListIterator& other) {
        if (list == other.list) {
            std::unique_lock<std::shared_mutex> lock(list->m, std::try_to_lock);
            list->dec_ref_count(ptr);
            this->ptr = other.ptr;
            this->list = other.list;
            list->inc_ref_count(ptr);
            return *this;
        }
        else {
            std::unique_lock<std::shared_mutex> lock1(list->m, std::try_to_lock);
            std::unique_lock<std::shared_mutex> lock2(other.list->m, std::try_to_lock);

            list->dec_ref_count(ptr);
            this->ptr = other.ptr;
            this->list = other.list;
            list->inc_ref_count(ptr);
            return *this;
        }
    }

    value_type get()
    {
        std::shared_lock<std::shared_mutex> lock(list->m);
        return ptr->val;
    }

    void set(value_type _val)
    {
        std::unique_lock<std::shared_mutex> lock(list->m);
        ptr->val = _val;
    }

    reference operator*() const {
        std::shared_lock<std::shared_mutex> lock(list->m);
        if (ptr->deleted) throw (std::out_of_range("Invalid index"));
        return ptr->val;
    }

    pointer operator->() const {
        std::shared_lock<std::shared_mutex> lock(list->m);
        if (ptr->deleted) throw (std::out_of_range("Invalid index"));
        return &(ptr->val);
    }

    // prefix ++
    ListIterator& operator++() {
        std::unique_lock<std::shared_mutex> lock(list->m);
        if (!ptr->next) throw (std::out_of_range("Invalid index"));
        *this = ListIterator(this->ptr->next, list);

        if (ptr) {
            while (this->ptr->deleted && ptr->next) {
                *this = ListIterator(this->ptr->next, list);
            }
        }
        return *this;
    }

    // postfix ++
    ListIterator operator++(int) {
        std::unique_lock<std::shared_mutex> lock(list->m);
        if (!ptr->next) throw (std::out_of_range("Invalid index"));

        ListIterator new_ptr(*this);

        if (new_ptr.ptr->next) {

            *this = ListIterator(new_ptr.ptr->next, list);

            while (ptr->deleted && ptr->next) {
                *this = ListIterator(this->ptr->next, list);
            }
        }
        return new_ptr;
    }

    // prefix --
    ListIterator& operator--() {
        std::unique_lock<std::shared_mutex> lock(list->m);
        if (!ptr->prev->prev) throw std::out_of_range("Invalid index");

        *this = ListIterator(this->ptr->prev, list);

        if (ptr) {
            while (this->ptr->deleted && ptr->prev) {
                *this = ListIterator(this->ptr->prev, list);
            }
        }
        return *this;
    }

    // postfix --
    ListIterator operator--(int) {
        std::unique_lock<std::shared_mutex> lock(list->m);
        if (!ptr->prev->prev) throw std::out_of_range("Invalid index");

        ListIterator new_ptr(*this);

        if (new_ptr.ptr->prev) {
            *this = ListIterator(new_ptr.ptr->prev, list);
            while (ptr->deleted && ptr->prev)
            {
                *this = ListIterator(this->ptr->prev, list);
            }
        }
        return new_ptr;
    }

    friend bool operator==(const ListIterator<ValueType>& a, const ListIterator<ValueType>& b) {
        if (a.list == b.list)
        {
            std::shared_lock<std::shared_mutex> lock(b.list->m, std::try_to_lock);
            return a.ptr == b.ptr;
        }
        else
        {
            std::shared_lock<std::shared_mutex> lock1(a.list->m, std::try_to_lock);
            std::shared_lock<std::shared_mutex> lock2(b.list->m, std::try_to_lock);
            return a.ptr == b.ptr;
        }
    }

    friend bool operator!=(const ListIterator<ValueType>& a, const ListIterator<ValueType>& b) {
        if (a.list == b.list)
        {
            std::shared_lock<std::shared_mutex> lock(b.list->m, std::try_to_lock);
            return !(a.ptr == b.ptr);
        }
        else
        {
            std::shared_lock<std::shared_mutex> lock1(a.list->m, std::try_to_lock);
            std::shared_lock<std::shared_mutex> lock2(b.list->m, std::try_to_lock);
            return !(a.ptr == b.ptr);
        }
    }

    operator bool() {
        std::shared_lock<std::shared_mutex> lock(list->m, std::try_to_lock);
        return ptr;
    }

    int getRefCount()
    {
        std::shared_lock<std::shared_mutex> lock(list->m);
        return ptr->ref_count;
    }

private:
    Node<value_type>* ptr;
    CLinkedList<value_type>* list;
};