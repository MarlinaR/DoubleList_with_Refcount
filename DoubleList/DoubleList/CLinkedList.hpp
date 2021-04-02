#pragma once

#include <functional>
#include <utility>
#include <type_traits>
#include <queue>
#include <limits>
#include <numeric>
#include <cmath>
#include <memory>

namespace LinkedList
{
    template<typename ValueType>
    class ListIterator;

    template<typename ValueType>
    class CLinkedList;
    
    template<typename ValueType>
    class Node
    {
    public:

        template<typename>
        friend class CLinkedList;

        template<typename>
        friend class ListIterator;

        using value_type = ValueType;
        using iterator = ListIterator<value_type>;

        Node() = default;
        ~Node() = default; ;
        Node(ValueType value, int ref_count)
            : val(std::move(value)), prev(this), next(this), deleted(false), ref_count(ref_count) {}
        Node(ValueType value, CLinkedList<value_type>* list) : Node(value, 2) {}
        Node(const Node<ValueType>&) = delete;

        void operator=(const Node<ValueType>&) = delete;

    private:
        value_type val;
        Node<value_type>* prev;
        Node<value_type>* next;
        bool deleted;
        int ref_count;

    };


    template<typename ValueType>
    class ListIterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = ValueType;
        using reference = ValueType&;
        using pointer = ValueType*;

        template<typename>
        friend class CLinkedList;


        ListIterator()noexcept = default;
        ListIterator(const  ListIterator& other) : ptr(other.ptr)
        {
            CLinkedList<ValueType>::inc_ref_count(ptr);
        }
        ListIterator(Node<value_type>* _new_ptr) : ptr(_new_ptr)
        {
            CLinkedList<ValueType>::inc_ref_count(ptr);
        }

        ~ListIterator() {
            if (!ptr) return;

            if (ptr->ref_count == 1) {
                CLinkedList<ValueType>::dec_ref_count(ptr);
                ptr = nullptr;
            } else {
                CLinkedList<ValueType>::dec_ref_count(ptr);
            }
        }

        ListIterator& operator=(const  ListIterator& other) {
            CLinkedList<ValueType>::dec_ref_count(ptr);

            this->ptr = other.ptr;

            CLinkedList<ValueType>::inc_ref_count(ptr);

            return *this;
        }

        reference operator*() const {
            if (ptr->deleted) throw (std::out_of_range("Invalid index"));

            return ptr->val;
        }

        pointer operator->() const {
            if (ptr->deleted) throw (std::out_of_range("Invalid index"));

            return &(ptr->val);
        }

        // prefix ++
        ListIterator& operator++() {
            if (!ptr->next) throw (std::out_of_range("Invalid index"));

            *this = this->ptr->next;

            if (ptr) {
                while (this->ptr->deleted && ptr->next) {
                    *this = this->ptr->next;
                }
            }

            return *this;

        }

        // postfix ++
        ListIterator operator++(int) {
            if (!ptr->next) throw (std::out_of_range("Invalid index"));

            ListIterator new_ptr(*this);

            if (new_ptr.ptr->next) {
                *this = new_ptr.ptr->next;
                while (ptr->deleted && ptr->next) {
                    *this = this->ptr->next;
                }
            }

            return new_ptr.ptr; 
        }

        // prefix --
        ListIterator& operator--() {
            if (!ptr->prev->prev) throw std::out_of_range("Invalid index");

            *this = this->ptr->prev;

            if (ptr) {
                while (this->ptr->deleted && ptr->prev) {
                    *this = this->ptr->prev;
                }
            }

            return *this;
        }

        // postfix --
        ListIterator operator--(int) {
            if (!ptr->prev->prev) throw std::out_of_range("Invalid index");

            ListIterator new_ptr(*this);

            if (new_ptr.ptr->prev) {
                *this = new_ptr.ptr->prev;
                while (ptr->deleted && ptr->prev)
                    *this = this->ptr->prev;
            }

            return new_ptr.ptr;
        }

        friend bool operator==(const ListIterator<ValueType>& a, const ListIterator<ValueType>& b) {
            return a.ptr == b.ptr;
        }

        friend bool operator!=(const ListIterator<ValueType>& a, const ListIterator<ValueType>& b) {
            return !(a == b);
        }

        operator bool() const {
            return ptr; 
        }

    private:
        Node<value_type>* ptr;

    };

    template<typename ValueType>

    class CLinkedList
    {
    public:
        using size_type = std::size_t;
        using value_type = ValueType;
        using iterator = ListIterator<value_type>;

        template<typename>
        friend class ListIterator;


        CLinkedList() : head(nullptr), tail(nullptr), m_size(0) {
            tail = new Node<value_type>();
            head = new Node<value_type>();
            tail->prev = head;
            head->next = tail;

            inc_ref_count(tail);
            inc_ref_count(head);
            inc_ref_count(tail);
            inc_ref_count(head);

        }

        CLinkedList(const CLinkedList& other) = delete;
        CLinkedList(CLinkedList&& x) = delete;
        CLinkedList(std::initializer_list<value_type> l):CLinkedList() {
            for (auto i = l.begin(); i < l.end(); i++)
                push_back(*i);

        }

        ~CLinkedList() {
            Node<value_type>* current = head;

            while (current != nullptr)
            {
                Node<value_type>* next = current->next;
                delete current;
                current = next;
            }
        }

        static void dec_ref_count(Node<value_type>* ptr) {
            if (!ptr) return;
            ptr->ref_count--;

            if (ptr->ref_count == 0) {
                if (ptr->next) dec_ref_count(ptr->next);
                if (ptr->prev) dec_ref_count(ptr->prev);
                delete (ptr);
            }

        }

        static void inc_ref_count(Node<value_type>* ptr) {
            if (!ptr) return;

            ptr->ref_count++;

        }

        CLinkedList& operator=(const CLinkedList& other) = delete;
        CLinkedList& operator=(CLinkedList&& x) = delete;

        void push_back(const value_type& value) {
            push_back(value_type(value));
        }
     
        void push_back(value_type&& value) {

            iterator it(tail);
            inserts(it,value);

        }

        void push_front(const value_type& value) {
            push_front(value_type(value));
        }

        void push_front(value_type&& value) {

            iterator it(head->next);
            inserts(it, value);

        }

        iterator erase(iterator position) {
            auto output = position.ptr->next;

            inc_ref_count(position.ptr->next);
            inc_ref_count(position.ptr->prev);

            if (position.ptr == head->next) {
                head->next = position.ptr->next;
            }
            else {
                position.ptr->prev->next = position.ptr->next;
            }

            if (position.ptr == tail->prev) {
                tail->prev = position.ptr->prev;
            }
            else {
                position.ptr->next->prev = position.ptr->prev;
            }

            m_size--;

            position.ptr->deleted = true;
            dec_ref_count(position.ptr);
            dec_ref_count(position.ptr);

            return output;

        }

        iterator inserts(iterator ptr, value_type value) {
            if (!ptr) return ptr;
            
            Node<ValueType>* node = new Node<value_type>{ std::move(value), 2 };
            
            node->prev = ptr.ptr->prev;
            node->next = ptr.ptr;
            ptr.ptr->prev->next = node;
            ptr.ptr->prev = node;
            
            iterator it(node);
            
            m_size++;
            return it;

        }

        iterator begin() noexcept {
            iterator ptr(head->next);

            return ptr;
        }
        iterator end() noexcept { 
            iterator ptr(tail);
      
            return ptr;

        }

        bool empty() noexcept {
            return begin() == end();

        }

        void clear() noexcept {
            iterator current = head->next;

            while (current != tail)
            {
                current = erase(current);

            }

        }

        size_type size() const noexcept {
            return m_size;

        }

    private:
        Node<ValueType>* head; 
        Node<ValueType>* tail;
        std::queue<Node<ValueType>*> deleted_nodes;
        size_type m_size;

    };

}
