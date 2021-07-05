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
class Purgatory;

template<typename T>
class Node
{
    friend class ListIterator<T>;
    friend class LinkedList<T>;
    friend class Purgatory<T>;


private:
    Node(LinkedList<T>* _list) : list(_list) { }
    Node(T _value, LinkedList<T>* _list) : value(_value), ref_count(0), prev(this), next(this), list(_list) { }
    Node(const Node<T>&) = delete;

    ~Node() {
        //cout << "Node destructor\n";
    }

    void operator=(const Node<T>&) = delete;

    void release() {

        shared_lock<std::shared_mutex> global_lock(list->global_mutex);

        int old_ref_count = --ref_count;

        if (old_ref_count == 0) {
            list->purgatory->put_purgatory(this);
        }

        global_lock.unlock();

    }

    void acquire() {
        ref_count++;
    }

    T            value;
    atomic<int>  ref_count = 0;
    atomic<bool> deleted = false;
    atomic<int> purged = 0;
    Node<T>* prev;
    Node<T>* next;
    shared_mutex m;
    LinkedList<T>* list;
};

template <typename T>
class Purgatory
{
private:
    friend class ListIterator<T>;
    friend class LinkedList<T>;
    friend class Node<T>;

public:

    template <typename T>
    class PurgatoryNode
    {
    public:
        PurgatoryNode(Node<T>* value) :
            value(value) { }

        Node<T>* value = nullptr;
        PurgatoryNode* next = nullptr;
    };

    Purgatory(LinkedList<T>* list_ref) :
        list_ref(list_ref),
        head(nullptr),
        cleanThread(&Purgatory::clean_purgatory, this) { }

    ~Purgatory()
    {
        set_deleted();
        cleanThread.join();
    }

    void set_deleted()
    {
        pur_deleted = true;
    }

    void put_purgatory(Node<T>* value)
    {
        PurgatoryNode<T>* node = new PurgatoryNode<T>(value);

        do {
            node->next = head.load();
        } while (!head.compare_exchange_strong(node->next, node));
    }

    void remove(PurgatoryNode<T>* prev, PurgatoryNode<T>* node)
    {
        prev->next = node->next;

        free(node);
    }

    void deleted_node(PurgatoryNode<T>* node)
    {
        Node<T>* prev = node->value->prev;
        Node<T>* next = node->value->next;

        if (prev != nullptr)
        {
            prev->release();
        }

        if (next != nullptr)
        {
            next->release();
        }

        free(node->value);
        free(node);
    }

    void clean_purgatory()
    {
        do
        {
            // first faze
            unique_lock<std::shared_mutex> lock(list_ref->global_mutex);

            PurgatoryNode<T>* purge_start = this->head;

            lock.unlock();

            if (purge_start != nullptr)
            {

                PurgatoryNode<T>* prev = purge_start;
                for (PurgatoryNode<T>* node = purge_start; node != nullptr;)
                {
                    PurgatoryNode<T>* cur_val = node;
                    node = node->next;

                    if (cur_val->value->ref_count > 0 || cur_val->value->purged == 1)
                    {
                        remove(prev, cur_val);
                    }
                    else
                    {
                        cur_val->value->purged = 1;
                        prev = cur_val;
                    }
                }
                prev->next = nullptr;

                // second faze

                lock.lock();

                PurgatoryNode<T>* new_purge_start = this->head;

                if (new_purge_start == purge_start)
                {

                    this->head = nullptr;
                }

                lock.unlock();

                prev = new_purge_start;
                PurgatoryNode<T>* node = new_purge_start;
                while (node != purge_start)
                {
                    PurgatoryNode<T>* cur_val = node;
                    node = node->next;

                    if (cur_val->value->purged == 1)
                    {
                        remove(prev, cur_val);
                    }
                    else
                    {
                        prev = cur_val;
                    }
                }

                prev->next = nullptr;

                for (PurgatoryNode<T>* node = purge_start; node != nullptr;)
                {
                    PurgatoryNode<T>* cur_val = node;
                    node = node->next;

                    deleted_node(cur_val);
                }
            }

            if (!pur_deleted)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } while (!pur_deleted || head.load() != nullptr);
    }

    std::thread cleanThread;
    std::atomic<PurgatoryNode<T>*> head;
    std::atomic<int32_t> global_counter = 0;
    LinkedList<T>* list_ref;
    bool pur_deleted = false;
};

template<typename T>
class ListIterator
{
    friend class LinkedList<T>;
    friend class Purgatory<T>;
    friend class Node<T>;
public:

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
        node->release();
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
        if (!previousNode)
            previousNode->release();
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

        if (!node->next) throw (out_of_range("Invalid index"));

        Node<T>* prevNode = node;
        shared_lock<shared_mutex> global_lock(list->global_mutex);
        Node<T>* newNode = node->next;

        newNode->acquire();
        node = newNode;
        global_lock.unlock();
        prevNode->release();

        return *this;
    }

    // Postfix
    ListIterator operator++(int) {
        ++(*this);
        return *this;
    }

    // Prefix
    ListIterator& operator--() {

        if (!node->prev) throw out_of_range("Invalid index");

        Node<T>* prevNode = node;
        shared_lock<shared_mutex> global_lock(list->global_mutex);
        Node<T>* newNode = node->prev;
        newNode->acquire();
        node = newNode;

        global_lock.unlock();

        prevNode->release();

        return *this;
    }

    // Postfix
    ListIterator operator--(int) {
        --(*this);
        return *this;
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
    friend class ListIterator<T>;
    friend class Purgatory<T>;
    friend class Node<T>;
public:
    using iterator = ListIterator<T>;

    LinkedList() : head(nullptr), tail(nullptr), size(0), purgatory(new Purgatory<T>(this)) {
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
        delete(purgatory);

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
                    node->release();
                    node->release();
                    --size;
                }
                else {
                    retry = true;
                }
                prev->release();
                next->release();
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
    std::shared_mutex global_mutex;
    Purgatory<T>* purgatory;
    Node<T>* head;
    Node<T>* tail;
    atomic<size_t> size;
};