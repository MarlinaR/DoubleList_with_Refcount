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

#include "rwlock_t.h"

using namespace std;

template<typename T>
class SLListIterator;

template<typename T>
class SLLinkedList;

template<typename T>
class SLPurgatory;

template<typename T>
class SLNode
{
    friend class SLListIterator<T>;
    friend class SLLinkedList<T>;
    friend class SLPurgatory<T>;


private:
    SLNode(SLLinkedList<T>* _list) : list(_list) { }
    SLNode(T _value, SLLinkedList<T>* _list) : value(_value), ref_count(0), prev(this), next(this), list(_list) { }
    SLNode(const SLNode<T>&) = delete;

    ~SLNode() {}

    void operator=(const SLNode<T>&) = delete;

    void release() {

        list->global_mutex.rlock();

        int old_ref_count = --ref_count;

        if (old_ref_count == 0) {
            list->purgatory->put_purgatory(this);
        }

        list->global_mutex.unlock();

    }
    void dec_refcount() {
        if (ref_count != 0) {
            --ref_count;
        }
    }

    void acquire() {
        ref_count++;
    }

    T            value;
    atomic<int>  ref_count = 0;
    atomic<bool> deleted = false;
    atomic<int> purged = 0;
    SLNode<T>* prev;
    SLNode<T>* next;
    rwlock_t m;
    SLLinkedList<T>* list;
};

template <typename T>
class SLPurgatory
{
private:
    friend class SLListIterator<T>;
    friend class SLLinkedList<T>;
    friend class SLNode<T>;

public:

    template <typename T>
    class PurgatoryNode
    {
    public:
        PurgatoryNode(SLNode<T>* value) :
            value(value) { }

        SLNode<T>* value = nullptr;
        PurgatoryNode* next = nullptr;
    };

    SLPurgatory(SLLinkedList<T>* list_ref) :
        list_ref(list_ref),
        head(nullptr),
        cleanThread(&SLPurgatory::clean_purgatory, this) { }

    ~SLPurgatory()
    {
        set_deleted();
        cleanThread.join();
    }

    void set_deleted()
    {
        pur_deleted = true;
    }

    void put_purgatory(SLNode<T>* value)
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
        SLNode<T>* prev = node->value->prev;
        SLNode<T>* next = node->value->next;

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
            list_ref->global_mutex.wlock();

            PurgatoryNode<T>* purge_start = this->head;

            list_ref->global_mutex.unlock();

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

                list_ref->global_mutex.wlock();

                PurgatoryNode<T>* new_purge_start = this->head;

                if (new_purge_start == purge_start)
                {

                    this->head = nullptr;
                }

                list_ref->global_mutex.unlock();

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

    SLLinkedList<T>* list_ref;
    std::atomic<PurgatoryNode<T>*> head;
    std::thread cleanThread;

    bool pur_deleted = false;
};

template <typename T>
class SLListIterator {
public:


    friend class SLLinkedList<T>;
    SLListIterator() noexcept = default;
    SLListIterator(const SLListIterator& other) noexcept {
        other.node->m.wlock();
        node = other.node;
        list = other.list;
        other.node->m.unlock();
        node->acquire();

    }

    SLListIterator(SLNode<T>* _node) : node(_node), list(_node->list)
    {
        node->acquire();
    }

    ~SLListIterator() {
        if (!node) {
            cout << "NODE IS NULL WHEN DELETING ITERATOR";
            return;
        }
        node->release();
    }

    SLListIterator& operator=(const SLListIterator& other) {
        SLNode<T>* previousNode;
        node->m.wlock();

        if (node == other.node) {
            node->m.unlock();
            return *this;
        }

        other.node->m.wlock();
        previousNode = node;

        node = other.node;
        node->acquire();
        list = other.list;


        node->m.unlock();
        other.node->m.unlock();
        if (!previousNode)
            previousNode->release();
        return *this;
    }

    T& operator*() const {
        node->m.rlock();
        if (node->deleted) throw (out_of_range("Invalid index"));
        node->m.unlock();
        return node->value;
    }

    T* operator->() const {
        shared_lock<shared_mutex> lock(node->m);
        if (node->deleted) throw (out_of_range("Invalid index"));
        return &(node->value);
    }
    //SLListIterator& operator=(SLListIterator&& other) {

    //    std::unique_lock<std::shared_mutex> lock(node->m);

    //    if (node == other.node) {
    //        return *this;
    //    }

    //    std::unique_lock<std::shared_mutex> lock_other(other.node->m);
    //    auto* prev_value = node;
    //    node = other.node;
    //    node->acquire();
    //    list = other.list;

    //    lock_other.unlock();
    //    lock.unlock();

    //    prev_value->release();

    //    return *this;
    //}


    SLListIterator& operator++() {

        if (!node->next) throw (out_of_range("Invalid index"));

        if (node && node != list->tail) {
            SLNode<T>* prevNode = nullptr;
            {
                list->global_mutex.rlock();
                SLNode<T>* newNode = node->next;

                prevNode = node;
                newNode->acquire();
                node = newNode;
                list->global_mutex.unlock();
            }
            prevNode->release();
        }
        return *this;
    }

    SLListIterator operator++(int) {

        SLListIterator temp = *this;

        if (!node->next) throw (out_of_range("Invalid index"));

        if (node && node != list->tail) {
            SLNode<T>* prevNode = nullptr;
            {
                list->global_mutex.rlock(); 
                SLNode<T>* newNode = node->next;

                prevNode = node;
                newNode->acquire();
                node = newNode;
                list->global_mutex.unlock();
            }
            prevNode->release();
        }

        return temp;
    }

    SLListIterator& operator--() {

        if (!node->prev) throw out_of_range("Invalid index");

        if (node && node != list->head) {
            SLNode<T>* prevNode = nullptr;
            {
                list->global_mutex.rlock(); 
                SLNode<T>* newNode = node->prev;

                prevNode = node;
                newNode->acquire();
                node = newNode;

                list->global_mutex.unlock();
            }
            prevNode->release();
        }

        return *this;
    }

    SLListIterator operator--(int) {
        SLListIterator temp = *this;

        if (node && node != list->head) {
            SLNode<T>* prevNode = nullptr;
            {
                list->global_mutex.rlock(); 
                SLNode<T>* newNode = node->prev;

                prevNode = node;
                newNode->acquire();
                node = newNode;
                
                list->global_mutex.unlock();
            }
            prevNode->release();
        }

        return temp;
    }

    bool isEqual(const SLListIterator<T>& other) const {
        return node == other.node;
    }

    friend bool operator==(const SLListIterator<T>& a, const SLListIterator<T>& b) {
        return a.isEqual(b);
    }

    friend bool operator!=(const SLListIterator<T>& a, const SLListIterator<T>& b) {
        return !a.isEqual(b);
    }

    operator bool() {
        shared_lock<shared_mutex> lock(node->m);
        return node;
    }

    int debugRefCount()
    {
        return node->ref_count;
    }

private:
    SLNode<T>* node = nullptr;
    SLLinkedList<T>* list;

    SLListIterator(SLNode<T>* node, SLLinkedList<T>* list) noexcept {
        this->node = node;
        this->node->acquire();
        this->list = list;
    }
};



template <typename T>
class SLLinkedList
{

    friend class SLListIterator<T>;
    friend class SLPurgatory<T>;
    friend class SLNode<T>;
public:
    using iterator = SLListIterator<T>;

    SLLinkedList() : head(new SLNode<T>(this)), tail(new SLNode<T>(this)), m_size(0), purgatory(new SLPurgatory<T>(this)) {

        tail->prev = head;
        head->next = tail;

        tail->acquire();
        head->acquire();
        tail->acquire();
        head->acquire();
    }

    SLLinkedList(const SLLinkedList& other) = delete;
    SLLinkedList(SLLinkedList&& x) = delete;
    SLLinkedList(initializer_list<T> l) : SLLinkedList() {
        iterator it(head);
        for (auto item : l) {
            push_back(item);
        }
    }

    ~SLLinkedList() {
        delete purgatory;

        SLNode<T>* nextNode;
        for (auto it = head; it != tail; it = nextNode) {
            nextNode = it->next;
            delete it;
        }
        delete tail;
    }

    void erase(iterator it) {

        SLNode<T>* node = it.node;

        if (node == head || node == tail) return;

        SLNode<T>* prev = nullptr;
        SLNode<T>* next = nullptr;

        for (bool retry = true; retry;) {
            retry = false;
            {
                node->m.rlock();
                if (node->deleted == true) {
                    node->m.unlock();
                    return;
                }
                prev = node->prev;
                prev->acquire();
                next = node->next;
                next->acquire();
                assert(prev && prev->ref_count);
                assert(next && next->ref_count);
            }

            {
                prev->m.wlock();
                node->m.rlock();
                next->m.wlock();

                if (prev->next == node && next->prev == node) {

                    prev->next = next;

                    next->acquire();

                    next->prev = prev;

                    prev->acquire();

                    node->deleted = true;
                    node->release();
                    node->release();
                    --m_size;

                }
                else {
                    retry = true;
                }
                prev->m.unlock();
                node->m.unlock();
                next->m.unlock();
            }

            prev->release();
            next->release();
        }
    }
    void push_front(T value) {
        iterator it(head);
        insert(it, value);
    }

    void push_back(T value) {
        SLNode<T>* node = tail->prev;
        iterator it(node);
        insert(it, value);
    }

    // Returns iterator on inserted node
    void insert(iterator it, T value) {
        SLNode<T>* prev = it.node;

        if (prev == tail) {
            prev = prev->prev;
        }

        if (prev == nullptr) return;
        prev->m.wlock();

        SLNode<T>* next = nullptr;

        for (bool retry = true; retry;) {
            retry = false;

            next = prev->next;
            next->m.wlock();

            if (next->prev != prev) {
                retry = true;
            }
        }

        SLNode<T>* node = new SLNode<T>(value, this);

        node->prev = prev;
        node->next = next;

        prev->next = node;
        node->acquire();
        next->prev = node;
        node->acquire();

        ++m_size;

        prev->m.unlock();
        next->m.unlock();

        return;
    }

    iterator begin() {


        head->m.rlock();
        auto node = head->next;
        head->m.unlock();
        return iterator(node);
    }

    iterator end() {

        tail->m.rlock();
        auto node = tail;
        tail->m.unlock();
        return iterator(node);
    }

    bool empty() {
        return m_size == 0;
    }

    std::size_t size() {
        return m_size;
    }

private:
    SLNode<T>* head = nullptr;
    SLNode<T>* tail = nullptr;
    SLPurgatory<T>* purgatory;
    std::atomic<std::size_t> m_size = 0;
    rwlock_t global_mutex;
};
