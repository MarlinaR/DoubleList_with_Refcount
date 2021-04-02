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
#include "CLinkedList.hpp"  

#define CATCH_CONFIG_MAIN 
#include "catch.hpp"

using namespace LinkedList;

TEST_CASE("LinkedList sample", "[CLinkedList]") {
    SECTION("prefix/postfix ++/--") {
        CLinkedList<int> list{ 1,2,3,4,5 };

        auto it = list.begin();
        REQUIRE(*it == 1);

        REQUIRE(*it++ == 1);
        REQUIRE(*++it == 3);

        REQUIRE(*it-- == 3);
        REQUIRE(*--it == 1);

        auto it_2 = ++it;
        auto it_3 = it_2;
        it = list.erase(it);
        it = list.erase(it);

        it_2++;
        REQUIRE(*it_2 == 4);
        REQUIRE(*++it_3 == 4);

        list.inserts(it_3,2);
        list.inserts(it_3, 3);
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

        it = list.inserts(it,5);
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

}
