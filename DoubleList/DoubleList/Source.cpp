#include <functional>
#include <utility>
#include <type_traits>
#include <vector>
#include <limits>
#include <numeric>
#include <cmath>
#include <memory>
#include <iostream>
#include <string>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <iostream>
#include <functional>
#include <utility>
#include <type_traits>
#include <vector>
#include <limits>
#include <numeric>
#include <cmath>
#include <memory>
#include <iostream>
#include <string>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <chrono>
#include <iostream>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <activation.h>

#include "LinkedList.h"

#define CATCH_CONFIG_MAIN 
#include "catch.hpp"

using namespace std;


TEST_CASE("Setup") {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);
}

TEST_CASE("Core mechanics tests", "[Base]") {


    SECTION("Constructor, destructor, iterating, push") {
        LinkedList<int> l;
        l.push_back(1);
        l.push_front(2);
        l.push_back(3);

        auto it = l.begin();

        REQUIRE(*it == 2);
        it++;
        REQUIRE(*it == 1);
        it++;
        REQUIRE(*it == 3);
        it--;
        REQUIRE(*it == 1);
        l.insert_after(it, 10);
        it++;
        REQUIRE(*it == 10);
    }

    SECTION("Initializer list constructor, destructor") {
        LinkedList<int> l2{ 1,2,3,4,5 };
        auto it = l2.begin();
        REQUIRE(*it++ == 1);
        REQUIRE(*it++ == 2);
        REQUIRE(*it == 3);

        LinkedList<bool>* l3 = new LinkedList<bool>{};
        l3->push_back(true);
        REQUIRE(*(l3->begin()) == true);
        delete l3;
    }

    SECTION("Checking ref_count working") {
        LinkedList<int> list{ 1,2,3,4,5 };

        auto it = list.begin();
        REQUIRE(*it == 1);
        it++;
        ++it;

        REQUIRE(*it == 3);

        it--;
        --it;
        REQUIRE(*it == 1);

        ++it;
        auto it_2 = it;
        auto it_3 = it_2;
        it = list.erase(it);
        it = list.erase(it);

        it_2++;
        REQUIRE(*it_2 == 4);
        ++it_3;
        REQUIRE(*it == 4);

        list.insert_after(it_3, 2);
        list.insert_after(it_3, 3);
        it_3 = list.erase(++it_3);
        //it_3--;
        it_3 = list.erase(it_3);

        it_3--;

        it_3 = it_2;

        it_3--;
        --it_2;
        REQUIRE(*it_2 == 1);
        REQUIRE(*it_3 == 1);
    }

    SECTION("==, !=") {
        LinkedList<int> list{ 1,2,3,4,5 };

        auto it = list.begin();
        auto it2 = list.begin();
        REQUIRE(it == it2);

        it2++;
        REQUIRE(it != it2);
        REQUIRE(it.debugRefCount() == 3);
        REQUIRE(it2.debugRefCount() == 3);
        it++;
        REQUIRE(it.debugRefCount() == 4);
    }

    SECTION("push back/push front/insert") {
        LinkedList<int> list;

        list.push_back(3);
        REQUIRE(*--list.end() == 3);

        int a = 4;
        list.push_back(a);
        REQUIRE(*--list.end() == 4);

        list.push_front(2);
        REQUIRE(*list.begin() == 2);

        a = 1;
        list.push_front(a);
        REQUIRE(*list.begin() == 1);

        auto it = list.begin();
        for (int i = 1; i < 4; i++) {
            REQUIRE(*it == i);
            it++;
        }

        it = list.insert_after(it, 5);
    }

    SECTION("erase") {
        LinkedList<int> list{ 1,2,3 };

        auto it = list.begin();
        it = list.erase(it);
        REQUIRE(*it == 2);

        it = list.erase(--list.end());
        REQUIRE(it == list.end());
    }

    SECTION("size") {
        LinkedList<int> list{ 1,2,3 };

        REQUIRE(list.Size() == 3);

        list.push_back(4);
        REQUIRE(list.Size() == 4);

        list.push_front(1);
        REQUIRE(list.Size() == 5);

        auto it = list.begin();
        list.erase(it);
        it++;
        REQUIRE(list.Size() == 4);

        list.erase(list.begin());
        REQUIRE(list.Size() == 3);
    }

    SECTION("->") {
        LinkedList<pair<int, int>> list;
        list.push_back(make_pair(1, 2));
        auto it = list.begin();
        REQUIRE(it->first == 1);
    }

    SECTION("Is empty") {
        LinkedList<int> list;
        REQUIRE(list.Size() == 0);
        list.push_back(4);
        REQUIRE(list.Size() == 1);
    }

    SECTION("Many pushes and erases") {
        int tries = 1;
        while (tries--) {
            auto pushes = 10000;
            //auto pushes = 1000000;
            LinkedList<int>* list = new LinkedList<int>();
            for (int i = 0; i < pushes; i++) {
                list->push_back(i);
            }
            REQUIRE(list->Size() == pushes);
            auto it = list->begin();
            for (int j = 0; j < 1000000; j++) {
                it = list->erase(it);
            }
            delete list;
        }
    }
}

void do_join(thread& t) { t.join(); }
void join_all(vector<thread>& v) { for_each(v.begin(), v.end(), do_join); }

auto pusherFunction = [](LinkedList<int>* list, ListIterator<int> it, const int& numberOfPushes, const int& value)
{
    for (auto i = 0; i < numberOfPushes; i++) {
        it = list->insert_after(it, value);
    }
    //printf("Pusher %i ended with %i pushes\n", value, numberOfPushes);
};

auto deleterFunction = [](LinkedList<int>* list, ListIterator<int> it, const int& numberOfErases)
{
    for (auto i = 0; i < numberOfErases; i++) {
        it = list->erase(it);
    }
    //printf("Eraser ended with %i erases\n", numberOfErases);
};

void ThreadedTest1(const int threads, int tries, const int totalPushes) {
    const int pushesPerThread = totalPushes / threads;
    while (tries--) {

        LinkedList<int>* list = new LinkedList<int>();
        vector<ListIterator<int>> iterators;
        ListIterator<int>* it = new ListIterator<int>(list->begin());
        for (int i = 0; i < threads; i++) {
            *it = list->insert_after(*it, i);
            iterators.push_back(*it);
            *it = list->insert_after(*it, i);
        }
        delete it;

        REQUIRE(list->Size() == threads * 2);

        // Pushers
        {
            vector<thread> pushers;
            pushers.reserve(threads);
            for (int i = 0; i < threads; i++) {
                pushers.push_back(
                    thread(pusherFunction, list, iterators[i], pushesPerThread, i)
                );
            }
            join_all(pushers);
        }

        REQUIRE(list->Size() == threads * (pushesPerThread + 2));

        {
            vector<thread> deleters;
            deleters.reserve(threads);
            for (int i = 0; i < threads; i++) {
                deleters.push_back(
                    thread(deleterFunction, list, iterators[i], pushesPerThread)
                );
            }
            join_all(deleters);
        }



        REQUIRE(list->Size() == threads * 2);
        delete list;
    }
}


TEST_CASE("Threaded tests", "[threads]") {
    auto hardwareThreadsAmount = thread::hardware_concurrency();

    SECTION("Threaded pushing, correct size of list, erase after threaded pushing") {
        auto repeatEveryTest = 5;
        auto totalPushes = 100000;
        int threadAmount = 1;
        //int threadAmount = 4;
        int pushesPerThread;

        while (threadAmount <= hardwareThreadsAmount) {
            //std::cout << "Multithreaded test, " << threadAmount << " threads\n\n";
            pushesPerThread = totalPushes / threadAmount;
            clock_t start = clock();

            ThreadedTest1(threadAmount, repeatEveryTest, totalPushes);
            cout << "\nThreaded test. "
                << threadAmount << " threads, "
                << pushesPerThread << " elements per thread, "
                << totalPushes << "total elements,"
                << float(clock() - start) / CLOCKS_PER_SEC * 1000 << " msec." << endl;

            threadAmount *= 2;
        }
    }
}


