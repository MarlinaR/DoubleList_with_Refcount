#pragma once

#include <functional>
#include <utility>
#include <type_traits>
#include <queue>
#include <limits>
#include <numeric>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <shared_mutex>
#include <mutex>
#include <iostream>
#include <atomic>
#include <cassert>

using namespace std;

template<typename T>
class ListIterator;

template<typename T>
class LinkedList;

template<typename T>
class Node
{
    friend class ListIterator<T>;
    friend class LinkedList<T>;

private:
    Node(LinkedList<T>* _list) : list(_list) { }
    Node(T _value, LinkedList<T>* _list) : value(_value), ref_count(0), prev(this), next(this), list(_list) { }
    Node(const Node<T>&) = delete;

    ~Node() {
        //cout << "Node destructor\n";
    }

    void operator=(const Node<T>&) = delete;

    void release() {

        if (--ref_count != 0) {
            return;
        }
        list->DeleteNode(this);

    }

    void acquire() {
        ref_count++;
    }

    T            value;
    atomic<int>  ref_count = 0;
    atomic<bool> deleted = false;
    Node<T>*     prev;
    Node<T>*     next;
    shared_mutex m;
    LinkedList<T>* list;
};

template<typename T>
class ListIterator
{
public:
    friend class LinkedList<T>;

    ListIterator() noexcept = default;
    ListIterator(const ListIterator& other) : node(other.node), list(other.node->list)
    {
        node->acquire();
    }
    ListIterator(Node<T>* _node) : node(_node), list(_node->list)
    {
        node->acquire();
    }

    ~ListIterator() {
        if (!node) {
            cout << "NODE IS NULL WHEN DELETING ITERATOR";
            return;
        }
        list->ReleaseNode(node);
    }

    ListIterator& operator=(const  ListIterator& other) {
        Node<T>* previousNode;
        {
            unique_lock<shared_mutex> lock1(node->m);
            if (node == other.node) {
                return *this;
            }
            shared_lock<shared_mutex> lock2(other.node->m);
            previousNode = node;

            node = other.node;
            node->acquire();
        }
        list->ReleaseNode(previousNode);
        //previousNode->release();
        return *this;
    }

    T& operator*() const {
        shared_lock<shared_mutex> lock(node->m);
        if (node->deleted) throw (out_of_range("Invalid index"));
        return node->value;
    }

    T* operator->() const {
        shared_lock<shared_mutex> lock(node->m);
        if (node->deleted) throw (out_of_range("Invalid index"));
        return &(node->value);
    }

    // Prefix
    ListIterator& operator++() {
        unique_lock<shared_mutex> lock_first(node->m);
        if (!node->next) throw (out_of_range("Invalid index"));

        Node<T>* prevNode = node;
        Node<T>* newNode = node->next;
  
        while (newNode->deleted && newNode->next) {
            shared_lock<shared_mutex> lock(newNode->m);
            newNode = newNode->next;
        }
        unique_lock<shared_mutex> lock(newNode->m);
        newNode->acquire();
        node = newNode;
        list->ReleaseNode(prevNode);
        //prevNode->release();

        return *this;
    }

    // Postfix
    ListIterator operator++(int) {
        unique_lock<shared_mutex> lock_first(node->m);
        if (!node->next) throw (out_of_range("Invalid index"));

        Node<T>* prevNode = node;
        Node<T>* newNode = node->next;

        while (newNode->deleted && newNode->next) {
            shared_lock<shared_mutex> lock(newNode->m);
            newNode = newNode->next;
        }

        unique_lock<shared_mutex> lock(newNode->m);
        newNode->acquire();
        node = newNode;
        list->ReleaseNode(prevNode);

        return ListIterator(prevNode);
    }

    // Prefix
    ListIterator& operator--() {
        unique_lock<shared_mutex> lock_first(node->m);
        if (!node->prev) throw out_of_range("Invalid index");

        Node<T>* prevNode = node;
        Node<T>* newNode = node->prev;

        while (newNode->deleted && newNode->prev) {
            shared_lock<shared_mutex> lock(newNode->m);
            newNode = newNode->prev;


        }
        unique_lock<shared_mutex> lock(newNode->m);
        newNode->acquire();
        node = newNode;
        list->ReleaseNode(prevNode);

        return *this;
    }

    // Postfix
    ListIterator operator--(int) {
        unique_lock<shared_mutex> lock_first(node->m);
        if (!node->prev) throw out_of_range("Invalid index");

        Node<T>* prevNode = node;

        Node<T>* newNode = node->prev;
        while (newNode->deleted && newNode->prev) {
            shared_lock<shared_mutex> lock(newNode->m);
            newNode = newNode->prev;
        }
        unique_lock<shared_mutex> lock(newNode->m);
        newNode->acquire();
        node = newNode;
        list->ReleaseNode(prevNode);

        return ListIterator(node);
    }

    bool isEqual(const ListIterator<T>& other) const {
        return node == other.node;
    }

    friend bool operator==(const ListIterator<T>& a, const ListIterator<T>& b) {
        return a.isEqual(b);
    }

    friend bool operator!=(const ListIterator<T>& a, const ListIterator<T>& b) {
        return !a.isEqual(b);
    }

    operator bool() {
        shared_lock<shared_mutex> lock(node->m);
        return node;
    }

    int debugRefCount()
    {
        shared_lock<shared_mutex> lock(node->m);
        return node->ref_count;
    }

private:
    Node<T>* node;
    LinkedList<T>* list;
};

template<typename T>
class LinkedList
{
public:
    using iterator = ListIterator<T>;

    friend class ListIterator<T>;

    void ReleaseNode(Node<T>* node) {
        --node->ref_count;
        if (node->ref_count != 0) {
            return;
        }

        queue<Node<T>*> nodesToDelete;
        nodesToDelete.push(node);
        while (!nodesToDelete.empty()) {
            Node<T>* currentNode = nodesToDelete.front();
            Node<T>* prev = currentNode->prev;
            Node<T>* next = currentNode->next;
            if (prev) {
                --prev->ref_count;
                if (prev->ref_count == 0) {
                    nodesToDelete.push(prev);
                }
            }
            if (next) {
                --next->ref_count;
                if (next->ref_count == 0) {
                    nodesToDelete.push(next);
                }
            }
            nodesToDelete.pop();
            delete currentNode;
        }
    }

    void DeleteNode(Node<T>* node) {
        queue<Node<T>*> nodesToDelete;
        nodesToDelete.push(node);

        while (!nodesToDelete.empty()) {
            Node<T>* currentNode = nodesToDelete.front();
            Node<T>* prev = currentNode->prev;
            Node<T>* next = currentNode->next;
            if (prev) {
                prev->ref_count--;
                if (prev->ref_count == 0) {
                    nodesToDelete.push(prev);
                }
            }
            if (next) {
                next->ref_count--;
                if (next->ref_count == 0) {
                    nodesToDelete.push(next);
                }
            }
            nodesToDelete.pop();
            delete (currentNode);
        }
    }

    LinkedList() : head(nullptr), tail(nullptr), size(0) {
        tail = new Node<T>(this);
        head = new Node<T>(this);
        tail->prev = head;
        head->next = tail;

        head->acquire();
        head->acquire();
        tail->acquire();
        tail->acquire();
    }

    LinkedList(const LinkedList& other) = delete;
    LinkedList(LinkedList&& x) = delete;
    LinkedList(initializer_list<T> l) : LinkedList() {
        iterator it(head);
        for (auto item : l) {
            push_back(item);
        }
    }

    ~LinkedList() {

        Node<T>* nextNode;
        for (auto it = head; it != tail; it = nextNode) {
            nextNode = it->next;
            delete it;
        }
        delete tail;

    }

    LinkedList& operator=(const LinkedList& other) = delete;
    LinkedList& operator=(LinkedList&& x) = delete;

    // Returns iterator on next to erased node
    iterator erase(iterator it) {

        Node<T>* node = it.node;

        if (node->deleted)
            return nullptr;
        if (node == head || node == tail) return it;

        Node<T>* prev;
        Node<T>* next;

        for (bool retry = true; retry;) {
            retry = false;
            {
                shared_lock<shared_mutex> currentLock(node->m);
                prev = node->prev;
                prev->acquire();
                next = node->next;
                next->acquire();
                assert(prev && prev->ref_count);
                assert(next && next->ref_count);
            }
            // RACES
            { 
                unique_lock<shared_mutex> prevLock(prev->m);
                shared_lock<shared_mutex> currentLock(node->m);
                unique_lock<shared_mutex> nextLock(next->m);

                if (prev->next == node && next->prev == node) {

                    prev->next = next;

                    next->acquire();

                    next->prev = prev;

                    prev->acquire();


                    node->deleted = true;
                    ReleaseNode(node);
                    ReleaseNode(node);
                    //node->release();
                    //node->release();
                    --size;
                }
                else {
                    retry = true;
                }
                ReleaseNode(prev);
                ReleaseNode(next);
                //prev->release();
                //next->release();
            }
        }
        return iterator(node->next);
    }

    void push_front(T value) {
        iterator it(head);
        insert_after(it, value);
    }

    void push_back(T value) {
        Node<T>* node = tail->prev;
        iterator it(node);
        insert_after(it, value);
    }

    // Returns iterator on inserted node
    iterator insert_after(iterator it, T value) {
        Node<T>* prev = it.node;

        // If called after begin() in empty list (where begin() == end())
        if (prev == tail) {
            prev = prev->prev;
        }

        if (prev == nullptr) return it;
        prev->m.lock();

        Node<T>* next = nullptr;

        for (bool retry = true; retry;) {
            retry = false;

            next = prev->next;
            next->m.lock();

            if (next->prev != prev) {
                retry = true;
                next->m.unlock();
            }
        }

        Node<T>* node = new Node<T>(value, this);

        node->prev = prev;
        node->next = next;

        prev->next = node;
        node->acquire();
        next->prev = node;
        node->acquire();

        ++size;

        prev->m.unlock();
        next->m.unlock();

        return iterator(node);
    }


    iterator begin() noexcept {
        auto node = head->next;
        unique_lock<shared_mutex> lock(node->m);
        return iterator(node);
    }

    iterator end() noexcept {
        auto node = tail;
        unique_lock<shared_mutex> lock(node->m);
        return iterator(node);
    }

    bool empty() noexcept {
        return head->next == tail;
    }


    size_t Size() const
    {
        return size;
    }

    string debug() {
        string output = "\n";

        if (head->next == tail) return "\n[Empty list]\n";

        Node<T>* current = head->next;
        while (current != tail) {

            if (!current || current->deleted)
                throw new overflow_error("something gone wrong");

            if (!current->next)
                throw new overflow_error("something gone wrong 2");

            output += "["
                + to_string(current->value)
                + ",ref:"
                + to_string(current->ref_count)
                + ",del:"
                + to_string(current->deleted)
                + "]\n";
            current = current->next;
        }
        return output;
    }

private:
    shared_mutex   m;
    Node<T>*       head;
    Node<T>*       tail;
    atomic<size_t> size;
};