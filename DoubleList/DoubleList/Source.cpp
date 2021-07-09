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
#include <stdio.h>
#include <math.h>

#include "LinkedList.h"
#include "SpinLockLinkedList.h"

using namespace std;

struct testTiming
{
    int threadsAmount;
    float spinlockTime;
    float commonTime;
    testTiming(int _threadsAmount, float _spinlockTime, float _commonTime) :
        threadsAmount(_threadsAmount), spinlockTime(_spinlockTime), commonTime(_commonTime) { }
};

FILE* gpipe = _popen("gnuplot -persist", "w");

void gp(string s)
{
    fprintf(gpipe, s.c_str());
}

void PlotTimingGraphics(vector<testTiming> data)
{
    if (!gpipe)
    {
        cout << "\nINSTALL GNUPLOT :)\n";
        return;
    }

    string plotData;
    for (auto timing : data) {
        plotData += to_string(timing.threadsAmount) + " " + to_string(timing.commonTime) + " " + to_string(timing.spinlockTime) + " \n";
    }
    plotData += " e\n";
    plotData += plotData;

    gp("set terminal qt                                                          \n"
        "set title \"Linked list tests time\" font \"Helvetica Bold, 20\"         \n"
        "set xlabel \"Threads\"                                                   \n"
        "set ylabel \"Time\"                                                      \n"
        "set grid                                                                 \n"
        "set xrange[0:16]                                                         \n"
        "set yrange[0:*]                                                          \n"
        "plot \'-\' using 1:2 title \'Normal lock\' with lines lc rgb \"red\",      "
        "      \'\' using 1:3 title \'Spinlock lock\' with lines lc rgb \"green\" \n"
    );
    gp(plotData);
    gp("pause -1 \"hit enter to exit\"                                           \n"
        "exit                                                                     \n"
    );
    _pclose(gpipe);
}

#define CATCH_CONFIG_MAIN 
#include "catch.hpp"

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
        l.insert(it, 10);
        it++;
        REQUIRE(*it == 10);
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

        list.insert(it, 5);
    }

    SECTION("erase") {
        LinkedList<int> list{ 1,2,3 };

        auto it = list.begin();
        list.erase(it);
        it++;
        REQUIRE(*it == 2);

        list.erase(--list.end());
        it++;
        REQUIRE(it == list.end());
    }

    SECTION("size") {
        LinkedList<int> list{ 1,2,3 };

        REQUIRE(list.size() == 3);

        list.push_back(4);
        REQUIRE(list.size() == 4);

        list.push_front(1);
        REQUIRE(list.size() == 5);

        auto it = list.begin();
        list.erase(it);
        it++;
        REQUIRE(list.size() == 4);

        list.erase(list.begin());
        REQUIRE(list.size() == 3);
    }

    SECTION("->") {
        LinkedList<pair<int, int>> list;
        list.push_back(make_pair(1, 2));
        auto it = list.begin();

        REQUIRE((*it).first == 1);
    }

    SECTION("Is empty") {
        LinkedList<int> list;
        REQUIRE(list.size() == 0);
        list.push_back(4);
        REQUIRE(list.size() == 1);
    }

    SECTION("Many pushes and erases") {
        int tries = 1;
        int pushes = 10000;
        while (tries--) {
            LinkedList<int>* list = new LinkedList<int>();

            for (int i = 0; i < pushes; i++) {
                list->push_back(i);
            }
            REQUIRE(list->size() == pushes);

            auto it = list->begin();
            for (int j = 0; j < pushes; j++) {
                list->erase(it);
                it++;
            }
        }
    }
}

void do_join(thread& t) { t.join(); }
void join_all(vector<thread>& v) { for_each(v.begin(), v.end(), do_join); }

// For time test
auto pusherFunction = [](LinkedList<int>* list, ListIterator<int> it, const int& numberOfPushes, const int& value)
{
    for (auto i = 0; i < numberOfPushes; i++) {
        list->insert(it, value);
        it++;
    }
};

auto deleterFunction = [](LinkedList<int>* list, ListIterator<int> it, const int& numberOfErases)
{
    for (auto i = 0; i < numberOfErases; i++) {
        auto old = it;
        it++;
        list->erase(old);
    }
};

void ThreadedTest(const int threads, int tries, const int totalPushes) {
    const int pushesPerThread = totalPushes / threads;
    while (tries--) {
        LinkedList<int> list;
        list.push_back(0);

        vector<ListIterator<int>> iterators;
        ListIterator<int>* it = new ListIterator<int>(list.begin());

        for (int i = 0; i < threads; i++) {
            list.insert(*it, i);
            (*it)++;

            iterators.push_back(*it);

            list.insert(*it, i);
            (*it)++;
        }
        delete it;

        REQUIRE(list.size() == threads * 2 + 1);
        {
            vector<thread> pushers;
            pushers.reserve(threads);
            for (int i = 0; i < threads; i++) {
                pushers.push_back(
                    thread(pusherFunction, &list, iterators[i], pushesPerThread, i)
                );
            }
            join_all(pushers);
        }
        REQUIRE(list.size() == threads * (pushesPerThread + 2) + 1);
        {
            vector<thread> deleters;
            deleters.reserve(threads);
            for (int i = 0; i < threads; i++) {
                deleters.push_back(
                    thread(deleterFunction, &list, iterators[i], pushesPerThread)
                );
            }
            join_all(deleters);
        }
        REQUIRE(list.size() == threads * 2 + 1);
    }
}

auto SLpusherFunction = [](SLLinkedList<int>* list, SLListIterator<int> it, const int& numberOfPushes, const int& value)
{
    for (auto i = 0; i < numberOfPushes; i++) {
        list->insert(it, value);
        it++;
    }
};

auto SLdeleterFunction = [](SLLinkedList<int>* list, SLListIterator<int> it, const int& numberOfErases)
{
    for (auto i = 0; i < numberOfErases; i++) {
        auto old = it;
        it++;
        list->erase(old);
    }
};

void SLThreadedTest(const int threads, int tries, const int totalPushes) {
    const int pushesPerThread = totalPushes / threads;
    while (tries--) {
        SLLinkedList<int> list;
        list.push_back(0);

        vector<SLListIterator<int>> iterators;
        SLListIterator<int>* it = new SLListIterator<int>(list.begin());

        for (int i = 0; i < threads; i++) {
            list.insert(*it, i);
            (*it)++;

            iterators.push_back(*it);

            list.insert(*it, i);
            (*it)++;
        }
        delete it;

        REQUIRE(list.size() == threads * 2 + 1);
        {
            vector<thread> pushers;
            pushers.reserve(threads);
            for (int i = 0; i < threads; i++) {
                pushers.push_back(
                    thread(SLpusherFunction, &list, iterators[i], pushesPerThread, i)
                );
            }
            join_all(pushers);
        }
        REQUIRE(list.size() == threads * (pushesPerThread + 2) + 1);
        {
            vector<thread> deleters;
            deleters.reserve(threads);
            for (int i = 0; i < threads; i++) {
                deleters.push_back(
                    thread(SLdeleterFunction, &list, iterators[i], pushesPerThread)
                );
            }
            join_all(deleters);
        }
        REQUIRE(list.size() == threads * 2 + 1);
    }
}

// For random push erase test
auto pusher1Function = [&](LinkedList<int>* list, int theadId, int elementsPerThread) {
    for (int j = 0; j < elementsPerThread * 2; ++j) {
        list->push_back(j + theadId * elementsPerThread);
    }
};

auto pusher2Function = [&](LinkedList<int>* list, int theadId, int elementsPerThread, int threadAmount) {
    for (int j = 0; j < elementsPerThread * 2; ++j) {
        list->push_front(j + (theadId + threadAmount) * elementsPerThread);
    }
};

auto eraser2Function = [&](LinkedList<int>* list, int elementsPerThread) {
    for (int j = 0; j < elementsPerThread; ++j) {
        list->erase(list->begin());
    }
};

// For concurrent iteration erase test
auto pusher3Function = [&](LinkedList<int>* list, int threadId, int pushes) {
    for (int j = 0; j < pushes; ++j) {
        list->push_back(j + threadId * pushes);
    }
};

auto iteratorFunction = [&](LinkedList<int>* list, condition_variable* cv, int totalElements) {
    {
        mutex mutex_;
        unique_lock<mutex> cvlock(mutex_);
        cv->wait(cvlock);
    }
    auto it = list->begin();
    int numberofiterations = rand() % totalElements;
    for (int j = 0; j < numberofiterations; ++j) {
        ++it;
    }
};

auto erasionFunction = [&](LinkedList<int>* list, condition_variable* cv, int eraseAmount) {
    {
        mutex mutex_;
        unique_lock<mutex> cvlock(mutex_);
        cv->wait(cvlock);
    }
    for (int j = 0; j < eraseAmount; ++j) {
        auto it = list->begin();
        list->erase(it);
    }
};

TEST_CASE("Threaded tests", "[threads]") {
    srand(time(nullptr));

    SECTION("Random push, erase") {
        LinkedList<int> list;
        int threadAmount = 4;
        int elementsPerThread = 10000;

        vector<thread> threads;

        clock_t start = clock();

        for (int i = 0; i < threadAmount; ++i) {
            threads.push_back(thread(pusher1Function, &list, i, elementsPerThread));
            threads.push_back(thread(pusher2Function, &list, i, elementsPerThread, threadAmount));
        }
        join_all(threads);
        threads.clear();

        threads.push_back(thread(eraser2Function, &list, elementsPerThread));
        join_all(threads);

        float duration = float(clock() - start) / CLOCKS_PER_SEC * 1000;
        cout << "Random push + erase done in " << duration << "ms" << endl;

        REQUIRE(list.size() >= (size_t)(elementsPerThread * threadAmount));
    }

    SECTION("Concurrent iteration, erase") {

        LinkedList<int> list;
        int threadsAmount = 4;
        int totalElements = 1000000;
        int totalDeletions = totalElements / 2;
        vector<thread> threads;
        condition_variable cv;
        clock_t start = clock();

        // Filling list
        for (int i = 0; i < threadsAmount; ++i) {
            threads.push_back(thread(pusher3Function, &list, i, totalElements / threadsAmount));
        }
        join_all(threads);
        threads.clear();

        REQUIRE(list.size() == totalElements);

        for (int i = 0; i < threadsAmount / 2; ++i) {
            threads.push_back(thread(erasionFunction, &list, &cv, totalDeletions / threadsAmount / 2));
            threads.push_back(thread(iteratorFunction, &list, &cv, totalElements));
        }

        this_thread::sleep_for(chrono::milliseconds(100));
        cv.notify_all();
        join_all(threads);

        float duration = float(clock() - start) / CLOCKS_PER_SEC * 1000;
        cout << "Random iteration + erase done within " << duration << " ms" << endl;
        REQUIRE(list.size() >= (size_t)(totalElements - totalDeletions));
    }

    SECTION("Threaded pushing, correct size of list, erase after threaded pushing") {
        auto neededThreadsAmount = 16;
        auto repeatEveryTest = 1; // 5;
        auto totalPushes = 100000;
        int threadAmount = 1;
        int pushesPerThread;
        vector<testTiming> testData;

        while (threadAmount <= neededThreadsAmount) {
            pushesPerThread = totalPushes / threadAmount;

            clock_t normalStart = clock();
            ThreadedTest(threadAmount, repeatEveryTest, totalPushes);
            float normalDuration = float(clock() - normalStart) / CLOCKS_PER_SEC * 1000;
            cout << "Normal lock; " << threadAmount << " threads; " << normalDuration << " ms." << endl;

            clock_t spinlockStart = clock();

            // TODO ЗАМЕНИТЬ ВОТ ЭТО НА ВОТ ТО
            SLThreadedTest(threadAmount, repeatEveryTest, totalPushes);
            //ThreadedTest(threadAmount, repeatEveryTest, totalPushes);

            float spinlockDuration = float(clock() - spinlockStart) / CLOCKS_PER_SEC * 1000;
            cout << "Spinlock   ; " << threadAmount << " threads; " << spinlockDuration << " ms." << endl;
            testData.push_back(testTiming(threadAmount, spinlockDuration, normalDuration));

            threadAmount += 1;
        }
        PlotTimingGraphics(testData);
    }
}