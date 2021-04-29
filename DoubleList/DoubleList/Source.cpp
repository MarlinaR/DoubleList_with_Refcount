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

#include "CLinkedList.hpp"  
#include "Iterator.cpp"

#define CATCH_CONFIG_MAIN 
#include "catch.hpp"



TEST_CASE("LinkedList sample", "[CLinkedList]") {
    SECTION("prefix/postfix ++/--") {
        CLinkedList<int> list{ 1,2,3,4,5 };

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

        list.insert(it_3,2);
        list.insert(it_3, 3);
        list.erase(it_3);
        it_3--;
        list.erase(it_3);
        it_3 = it_2;

        it_3--;
        --it_2;
        REQUIRE(*it_2 == 2);
        REQUIRE(*it_3 == 2);
    }
    
    SECTION("operator==/!=") {
        CLinkedList<int> list{ 1,2,3,4,5 };

        auto it = list.begin();
        auto it2 = list.begin();
        REQUIRE(it == it2);

        it2++;
        REQUIRE(it != it2);
        REQUIRE(it.getRefCount() == 3);
        REQUIRE(it2.getRefCount() == 3);
        it++;
        REQUIRE(it.getRefCount() == 4);

    }
    
    SECTION("push back/push front/insert") {
        CLinkedList<int> list;

        list.push_back(3);
        REQUIRE( *--list.end() == 3);

        int a = 4;
        list.push_back(a);
        REQUIRE(*--list.end() == 4);

        list.push_front(2);
        REQUIRE(*list.begin() == 2);

        a = 1;
        list.push_front(a);
        REQUIRE(*list.begin() == 1);

        auto it = list.begin();
        for (int i = 1; i < 5; i++) {
            REQUIRE(*it == i);
            it++;
        }

        it = list.insert(it,5);
        REQUIRE(*it == 5);

    }
    
    SECTION("erase") {
        CLinkedList<int> list{ 1,2,3 };

        auto it = list.begin();
        it = list.erase(it);
        REQUIRE(*it == 2);

        it = list.erase(--list.end());
        REQUIRE(it == list.end());

    }
    
    SECTION("size") {
        CLinkedList<int> list{ 1,2,3 };

        REQUIRE(list.size() == 3);

        list.push_back(4);
        REQUIRE(list.size() == 4);

        list.push_front(1);
        REQUIRE(list.size() == 5);

        list.erase(list.begin());
        REQUIRE(list.size() == 4);

        list.clear();
        REQUIRE(list.size() == 0);

    }
    
    SECTION("->") {
        CLinkedList<std::pair<int,int>> list;
        list.push_back(std::make_pair(1, 2));
        auto it = list.begin();
        REQUIRE(it->first == 1);
    }

    SECTION("Is empty") {
        CLinkedList<int> list;
        REQUIRE(list.empty());

        list.push_back(4);
        REQUIRE(!list.empty());
    }

    
    SECTION("Threaded pushing, correct size of list, erase after threaded pushing") {
        {
            CLinkedList<int> list;
            int threadsAmount = 100;
            int pushes = 100;

            std::vector<std::thread> vec;
            for (int z = 0; z < threadsAmount; z++) {
                vec.push_back(std::thread([&]() {
                    auto it = list.begin();
                    for (unsigned long i = 0l; i < pushes; i++) {
                        list.push_back(1);
                    }
                }));
            }

            for (int z = 0; z < threadsAmount; z++)
            {
                vec[z].join();
            }

            REQUIRE(list.size() == (threadsAmount * pushes));

            std::thread t2([&]() {
                list.clear();
            });

            t2.join();

            REQUIRE(list.size() == 0);
        }
    }
    
    SECTION("Checks if read operations works in parallel") {
        {
            std::cout << "\n Begin test that require some time...\n";
            CLinkedList<int> list{ 1, 2, 3, 4, 5};
            int threadsAmount = 2;
            int totalOperations = 5000000;

            CHECK(totalOperations % threadsAmount == 0);

            // Multi-threaded operations
            auto startThreaded = std::chrono::high_resolution_clock::now();
            std::vector<std::thread> vec;
            for (int z = 0; z < threadsAmount; z++) {
                vec.push_back(std::thread([&]() {
                    for (unsigned long i = 0l; i < totalOperations / threadsAmount; i++) {
                        list.size(); list.size(); list.size(); list.size(); list.size(); list.size();
                        list.empty(); list.empty(); list.empty(); list.empty(); list.empty(); list.empty();
                    }
                }));
            }
            for (int z = 0; z < threadsAmount; z++)
            {
                vec[z].join();
            }
            auto endThreaded = std::chrono::high_resolution_clock::now();

            // Single-thread operations
            auto startSingle = std::chrono::high_resolution_clock::now();
            for (unsigned long i = 0l; i < totalOperations; i++) {
                list.size(); list.size(); list.size(); list.size(); list.size(); list.size();
                list.empty(); list.empty(); list.empty(); list.empty(); list.empty(); list.empty();
            }
            auto endSingle = std::chrono::high_resolution_clock::now();

            // Check if multi-thread is faster
            auto durationThreaded = std::chrono::duration_cast<std::chrono::microseconds>(endThreaded - startThreaded);
            auto durationSingle = std::chrono::duration_cast<std::chrono::microseconds>(endSingle - startSingle);

            CHECK(durationSingle > durationThreaded);
        }
    }
    
    SECTION("Erasing, iterating attack, checks end(), begin()") {
        {
            std::shared_mutex m;
            std::condition_variable_any cv;

            int threads = 10;
            int elements = 1000;

            CLinkedList<int> list;
            std::vector<int> savedValues;
            std::atomic<bool> dataReady{ false };
            std::atomic<int> consumed{ 0 };
            std::vector<int> wrongValues;

            std::vector<std::thread> checkers;
            for (int i = 0; i < threads; i++)
            {
                checkers.push_back(std::thread([&]() {
                    std::cout << "Checker waiting\n";
                    std::shared_lock<std::shared_mutex> lck(m);
                    cv.wait(lck, [&] { return dataReady.load(); });

                    std::cout << "Running checks [ERASE, it++]\n";
                    auto it = list.begin();
                    for (int i = 0; i < elements / (threads * 2); i++)
                    {
                        it++;
                        it = list.erase(it);
                    }

                    dataReady = false;
                    consumed++;
                    std::cout << "Checks complete\n";
                    cv.notify_all();
                    cv.wait(lck, [&]() {return dataReady == true; });

                    std::cout << "Running checks [END, it++, GET, BEGIN]\n";
                    it = list.begin();
                    while (it != list.end())
                    {
                        it++;
                    }
                    auto val = it.get() + it.get() % 10;
                    savedValues.push_back(val);

                    dataReady = false;
                    consumed++;
                    std::cout << "Checks complete\n";
                    cv.notify_all();
                }));
            }

            std::thread producer([&]() {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::unique_lock<std::shared_mutex> lck(m);

                for (int i = 0; i < elements; i++) {
                    list.push_front(rand());
                }
                dataReady = true;

                std::cout << "Data prepared\n";
                cv.notify_all();
                cv.wait(lck, [&]() { return consumed.load() == threads; });

                std::cout << "Data preparing\n";
                list.push_front(rand());
                dataReady = true;
                consumed = 0;


                std::cout << "Data prepared\n";
                cv.notify_all();
                cv.wait(lck, [&]() { return consumed.load() == threads; });

                std::cout << "Checking results\n";
                auto correctValue = savedValues[0];
                std::copy_if(savedValues.begin(), savedValues.end(), std::back_inserter(wrongValues), [&](int i) {return i != correctValue; });
            });

            for (auto& th : checkers)
                th.join();
            producer.join();

            REQUIRE(wrongValues.size() == 0);
        }
    }
    
}
