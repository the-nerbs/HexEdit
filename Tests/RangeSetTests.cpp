#include "Stdafx.h"
#include "utils/Garbage.h"

#include "range_set.h"

#include <catch.hpp>

#include <string>
#include <sstream>

template <typename T>
class counted : public T
{
private:
    static int count;

public:
    const int id;

    template <typename... TArgs>
    counted(TArgs&&... args) :
        T{ std::forward<TArgs>(args)... },
        id{ ++count }
    { }

    counted(const counted&) = default;
    counted(counted&&) = default;
    counted& operator=(const counted&) = default;
    counted& operator=(counted&&) = default;
};

template <typename T>
int counted<T>::count = 0;

template <typename T>
using counted_less = counted<std::less<T>>;

template <typename T>
using counted_allocator = counted<std::allocator<T>>;


template <typename T, typename Pred, typename Alloc, std::size_t N, typename Comparer = std::equal_to<T>>
bool are_equal(const range_set<T, Pred, Alloc>& set, T (&expected)[N], Comparer comp = {})
{
    if (set.size() != N)
    {
        return false;
    }

    auto expected_itr = std::begin(expected);
    auto expected_end = std::end(expected);

    auto actual_itr = std::begin(set);
    auto actual_end = std::end(set);

    while (expected_itr != expected_end
        && actual_itr != actual_end)
    {
        if (*actual_itr != *expected_itr)
        {
            return false;
        }

        expected_itr++;
        actual_itr++;
    }

    return expected_itr == expected_end
        && actual_itr == actual_end;
}


TEST_CASE("range_set - constructors")
{
    SECTION("range_set(Pred, Alloc) - defaults")
    {
        range_set<int> set;

        CHECK(set.size() == 0);
        CHECK(set.empty() == true);

        CHECK(set.begin() == set.end());
        CHECK(set.rbegin() == set.rend());
    }

    SECTION("range_set(Pred, Alloc) - custom comparer")
    {
        counted_less<int> comp;

        range_set<int, counted_less<int>> set{ comp };

        CHECK(set.key_comp().id == comp.id);
        CHECK(set.value_comp().id == comp.id);
    }

    SECTION("range_set(Pred, Alloc) - custom allocator")
    {
        counted_allocator<int> alloc;

        range_set<int, std::less<int>, counted_allocator<int>> set{ {}, alloc };

        CHECK(set.get_allocator().id == alloc.id);
    }

    SECTION("range_set(init, Pred, Alloc)")
    {
        range_set<int> set{ 5, 6, 10 };

        CHECK(set.size() == 3);

        int expected[] = { 5, 6, 10 };
        CHECK(are_equal(set, expected));
    }

    SECTION("range_set(itr, itr, Pred, Alloc) - initial range")
    {
        counted_less<int> comp;
        counted_allocator<int> alloc;
        int init_data[5] = { 10, 20, 30, 40, 50 };

        range_set<int, counted_less<int>, counted_allocator<int>> set{
            std::begin(init_data), std::end(init_data),
            comp, alloc
        };

        CHECK(set.key_comp().id == comp.id);
        CHECK(set.value_comp().id == comp.id);
        CHECK(set.get_allocator().id == alloc.id);

        CHECK(set.size() == 5);
        CHECK(set.empty() == false);

        CHECK(are_equal(set, init_data));
    }

    SECTION("range_set - reversed comparer")
    {
        range_set<int, std::greater<int>> set;
        set.insert_range(10, 0);

        CHECK(set.size() == 10);

        int expected[] = { 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };
        CHECK(are_equal(set, expected));
    }
}

TEST_CASE("range_set - insert methods")
{
    range_set<int> set;

    auto ib = set.insert(5);

    SECTION("check initial insert")
    {
        CHECK(ib.first == set.begin());
        CHECK(ib.second == true);
        CHECK(set.size() == 1);
    }

    SECTION("insert duplicate")
    {
        ib = set.insert(5);

        CHECK(ib.first == set.begin());
        CHECK(ib.second == false);
        CHECK(set.size() == 1);
    }

    SECTION("insert with iterator")
    {
        auto itr = set.insert(set.end(), 10);

        auto check_itr = std::begin(set);
        check_itr++;
        CHECK(itr == check_itr);
    }

    SECTION("insert with iterator before correct position")
    {
        auto itr = set.insert(set.begin(), 10);

        auto check_itr = std::begin(set);
        check_itr++;
        CHECK(itr == check_itr);
    }

    SECTION("insert iterator range")
    {
        int data[5] = { 10, 20, 30, 40, 50 };

        set.insert(std::begin(data), std::end(data));

        CHECK(set.size() == 6);

        auto itr = set.begin();

        REQUIRE(*itr == 5);
        itr++;

        for (int i = 0; i < 5; i++, itr++)
        {
            REQUIRE(*itr == data[i]);
        }

        CHECK(itr == set.end());
    }

    SECTION("insert before all segments")
    {
        ib = set.insert(0);

        CHECK(*ib.first == 0);
        CHECK(ib.second);

        int expected[] = { 0, 5 };
        CHECK(are_equal(set, expected));
    }

    SECTION("insert after all segments")
    {
        ib = set.insert(10);

        CHECK(*ib.first == 10);
        CHECK(ib.second);

        int expected[] = { 5, 10 };
        CHECK(are_equal(set, expected));
    }

    SECTION("insert between segments")
    {
        // setup 2 segments { 5, 10 }
        set.insert(10);

        ib = set.insert(8);

        CHECK(*ib.first == 8);
        CHECK(ib.second);

        int expected[] = { 5, 8, 10 };
        CHECK(are_equal(set, expected));
    }

    SECTION("insert extends segment")
    {
        // setup 2 segments { 5, 7 }
        set.insert(7);

        ib = set.insert(6);

        CHECK(*ib.first == 6);
        CHECK(ib.second);

        int expected[] = { 5, 6, 7 };
        CHECK(are_equal(set, expected));
    }

    SECTION("insert_range")
    {
        set.insert_range(5, 10);

        int expected[] = { 5, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    SECTION("insert_range - end less than start")
    {
        auto range = set.insert_range(11, 10);

        // no change
        int expected[] = { 5 };
        CHECK(are_equal(set, expected));

        CHECK(range.first == set.end());
        CHECK(range.second == set.end());
    }

    SECTION("insert_range - hint at correct location")
    {
        set.insert_range(set.end(), 5, 10);

        int expected[] = { 5, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    SECTION("insert_range - hint before correct location")
    {
        set.insert(4);

        set.insert_range(set.begin(), 5, 10);

        int expected[] = { 4, 5, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    SECTION("insert_range - hint after correct location")
    {
        set.insert(6);

        set.insert_range(set.end(), 5, 10);

        int expected[] = { 5, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }
}

TEST_CASE("range_set - erase methods")
{
    range_set<int> set;

    set.insert_range(0, 10);

    // erase(T)

    SECTION("erase first value")
    {
        std::size_t removedCount = set.erase(0);

        CHECK(removedCount == 1);
        CHECK(set.size() == 9);

        int expected[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase value in middle")
    {
        std::size_t removedCount = set.erase(5);

        CHECK(removedCount == 1);
        CHECK(set.size() == 9);

        int expected[] = { 0, 1, 2, 3, 4, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase last value")
    {
        std::size_t removedCount = set.erase(9);

        CHECK(removedCount == 1);
        CHECK(set.size() == 9);

        int expected[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase nonexistent value")
    {
        std::size_t removedCount = set.erase(64);

        CHECK(removedCount == 0);
        CHECK(set.size() == 10);

        int expected[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    // erase(iterator)

    SECTION("erase at begin iterator")
    {
        auto itr = set.erase(set.begin());

        CHECK(*itr == 1);
        CHECK(set.size() == 9);

        int expected[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase at end")
    {
        auto end = set.end();
        end--;

        auto itr = set.erase(end);

        CHECK(itr == set.end());
        CHECK(set.size() == 9);

        int expected[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase at end of segment")
    {
        set.insert_range(20, 30);
        auto erase_at = set.find(9);

        auto itr = set.erase(erase_at);

        CHECK(*itr == 20);
        CHECK(set.size() == 19);

        int expected[] = {
             0,  1,  2,  3,  4,  5,  6,  7,  8,
            20, 21, 22, 23, 24, 25, 26, 27, 28, 29
        };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase at start of segment")
    {
        set.insert_range(20, 30);
        auto erase_at = set.find(20);

        auto itr = set.erase(erase_at);

        CHECK(*itr == 21);
        CHECK(set.size() == 19);

        int expected[] = {
             0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
                21, 22, 23, 24, 25, 26, 27, 28, 29
        };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase between iterators")
    {
        auto start = set.find(3);
        auto end = set.find(6);
        auto itr = set.erase(start, end);

        CHECK(*itr == 6);
        CHECK(set.size() == 7);

        int expected[] = { 0, 1, 2, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase between iterators from end")
    {
        auto itr = set.erase(set.end(), set.end());

        CHECK(itr == set.end());

        // erase is no-op
        int expected[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase between iterators to end")
    {
        auto start = set.find(3);
        auto itr = set.erase(start, set.end());

        CHECK(itr == set.end());

        int expected[] = { 0, 1, 2 };
        CHECK(are_equal(set, expected));
    }

    // erase_range([hint,] start, end)

    SECTION("erase_range")
    {
        auto itr = set.erase_range(3, 6);

        CHECK(*itr == 6);
        CHECK(set.size() == 7);

        int expected[] = { 0, 1, 2, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase_range - hint at correct location")
    {
        auto hint = set.find(3);
        auto itr = set.erase_range(hint, 3, 6);

        CHECK(*itr == 6);
        CHECK(set.size() == 7);

        int expected[] = { 0, 1, 2, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase_range - hint before location")
    {
        auto hint = set.begin();
        auto itr = set.erase_range(hint, 3, 6);

        CHECK(*itr == 6);
        CHECK(set.size() == 7);

        int expected[] = { 0, 1, 2, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase_range - hint after location")
    {
        auto hint = set.end();
        auto itr = set.erase_range(hint, 3, 6);

        CHECK(*itr == 6);
        CHECK(set.size() == 7);

        int expected[] = { 0, 1, 2, 6, 7, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase_range - erase at end")
    {
        // set up 2 segments { 0-9, 20-29 }
        set.insert_range(20, 30);

        auto itr = set.erase_range(set.end(), 30, 31);

        CHECK(itr == set.end());
        CHECK(set.size() == 20);

        int expected[] = {
             0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
            20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
        };
        CHECK(are_equal(set, expected));
    }

    SECTION("erase_range - erase across multiple segments")
    {
        // set up 3 segments { 0-9, 15, 20-29 }
        set.insert(15);
        set.insert_range(20, 30);

        auto itr = set.erase_range(5, 25);

        CHECK(*itr == 25);
        CHECK(set.size() == 10);

        int expected[] = { 0, 1, 2, 3, 4, 25, 26, 27, 28, 29 };
        CHECK(are_equal(set, expected));

    }

    SECTION("erase_range - erase between segments")
    {
        // set up 2 segments { 0-9, 20-29 }
        set.insert_range(20, 30);

        auto itr = set.erase_range(14, 17);

        CHECK(*itr == 20);
        CHECK(set.size() == 20);

        int expected[] = {
             0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
            20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
        };
        CHECK(are_equal(set, expected));
    }

    // clear()

    SECTION("clear")
    {
        set.clear();

        CHECK(set.size() == 0);
        CHECK(set.begin() == set.end());
    }
}

TEST_CASE("range_set - search methods")
{
    range_set<int> set{};
    set.insert_range(0, 10);

    SECTION("find existing value")
    {
        int i = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);

        auto itr = set.find(i);

        auto expected = set.begin();
        for (int j = 0; j < i; j++)
        {
            expected++;
        }

        REQUIRE(itr != set.end());
        CHECK(*itr == i);
        CHECK(itr == expected);
    }

    SECTION("find value before start")
    {
        auto itr = set.find(-1);

        CHECK(itr == set.end());
    }

    SECTION("find value after end")
    {
        auto itr = set.find(10);

        CHECK(itr == set.end());
    }


    SECTION("count existing value")
    {
        std::size_t count = set.count(2);
        CHECK(count == 1);
    }

    SECTION("count nonexistent value")
    {
        std::size_t count = set.count(64);
        CHECK(count == 0);
    }


    SECTION("lower_bound of existing value")
    {
        auto itr = set.lower_bound(5);

        CHECK(*itr == 5);
    }

    SECTION("lower_bound of value before start")
    {
        auto itr = set.lower_bound(-1);

        // returns first element *not less than* the queried value
        CHECK(itr == set.begin());
    }

    SECTION("lower_bound of value after end")
    {
        auto itr = set.lower_bound(10);

        CHECK(itr == set.end());
    }


    SECTION("upper_bound of existing value")
    {
        auto itr = set.upper_bound(5);

        // returns one past the last element of the queried value
        CHECK(*itr == 6);
    }

    SECTION("upper_bound of value before start")
    {
        auto itr = set.upper_bound(-1);

        // returns first element *not less than* the queried value
        CHECK(itr == set.begin());
    }

    SECTION("upper_bound of value after end")
    {
        auto itr = set.upper_bound(10);

        CHECK(itr == set.end());
    }


    SECTION("equal_range of existing value")
    {
        auto itrs = set.equal_range(5);

        CHECK(*itrs.first == 5);
        CHECK(*itrs.second == 6);
    }

    SECTION("equal_range of value before start")
    {
        auto itrs = set.equal_range(-1);

        CHECK(itrs.first == set.begin());
        CHECK(itrs.second == set.begin());
    }

    SECTION("equal_range of value after end")
    {
        auto itrs = set.equal_range(10);

        CHECK(itrs.first == set.end());
        CHECK(itrs.second == set.end());
    }
}

TEST_CASE("range_set - swap")
{
    range_set<int> set;
    set.insert(5);
    set.insert_range(10, 12);

    // set = { 5, 10, 11 }

    range_set<int> swapped;
    set.swap(swapped);

    CHECK(swapped.size() == 3);

    int expected[] = { 5, 10, 11 };
    CHECK(are_equal(swapped, expected));
}

TEST_CASE("range_set - read from stream")
{
    SECTION("read empty set")
    {
        std::stringstream stream;

        range_set<int> set;
        set.insert(5);

        stream >> set;

        CHECK(set.size() == 0);
    }

    SECTION("read single value")
    {
        std::stringstream stream{ "5" };
        range_set<int> set;

        stream >> set;

        int expected[] = { 5 };
        CHECK(are_equal(set, expected));
    }

    SECTION("read single value range")
    {
        std::string separator = GENERATE(":", "-");
        std::stringstream stream{ std::to_string(5) + separator + std::to_string(10) };
        range_set<int> set;

        stream >> set;

        int expected[] = { 5, 6, 7, 8, 9, 10 };
        CHECK(are_equal(set, expected));
    }

    SECTION("read two values")
    {
        std::stringstream stream{ "5,10" };
        range_set<int> set;

        stream >> set;

        int expected[] = { 5, 10 };
        CHECK(are_equal(set, expected));
    }

    SECTION("read values then range")
    {
        std::stringstream stream{ "5,8-10" };
        range_set<int> set;

        stream >> set;

        int expected[] = { 5, 8, 9, 10 };
        CHECK(are_equal(set, expected));
    }

    SECTION("read range then value")
    {
        std::stringstream stream{ "5-6,10" };
        range_set<int> set;

        stream >> set;

        int expected[] = { 5, 6, 10 };
        CHECK(are_equal(set, expected));
    }

    SECTION("read two ranges")
    {
        std::stringstream stream{ "5-6,8-9" };
        range_set<int> set;

        stream >> set;

        int expected[] = { 5, 6, 8, 9 };
        CHECK(are_equal(set, expected));
    }

    SECTION("read with extra data after")
    {
        std::stringstream stream{ "5 a" };
        range_set<int> set;

        stream >> set;

        int expected[] = { 5 };
        CHECK(are_equal(set, expected));

        // stream should still be positioned at the next item
        char c = 0;
        stream >> c;
        CHECK(c == 'a');
    }

    SECTION("read missing range end")
    {
        std::stringstream stream{ "1,5: a b" };
        range_set<int> set;

        stream >> set;

        // expect that the full initial value was kept, but the incomplete range was not.
        int expected[] = { 1 };
        CHECK(are_equal(set, expected));

        // expect the stream to be put into a failed read state.
        CHECK(stream.fail());
    }

    SECTION("read missing next value")
    {
        std::stringstream stream{ "1,: a b" };
        range_set<int> set;

        stream >> set;

        // expect that the full initial value was kept, but the incomplete range was not.
        int expected[] = { 1 };
        CHECK(are_equal(set, expected));

        // expect the stream to be put into a failed read state.
        CHECK(stream.fail());
    }
}

TEST_CASE("range_set - write to stream")
{
    range_set<int> set;
    std::stringstream stream;

    SECTION("write empty set")
    {
        stream << set;

        CHECK("" == stream.str());
    }

    SECTION("write single value")
    {
        set.insert(5);

        stream << set;

        CHECK("5" == stream.str());
    }

    SECTION("write single range")
    {
        set.insert_range(5, 10);

        stream << set;

        CHECK("5:9" == stream.str());
    }

    SECTION("write two values")
    {
        set.insert(1);
        set.insert(5);

        stream << set;

        CHECK("1,5" == stream.str());
    }

    SECTION("write value then range")
    {
        set.insert(1);
        set.insert_range(5, 10);

        stream << set;

        CHECK("1,5:9" == stream.str());
    }

    SECTION("write range then value")
    {
        set.insert_range(1, 5);
        set.insert(10);

        stream << set;

        CHECK("1:4,10" == stream.str());
    }

    SECTION("write two ranges")
    {
        set.insert_range(1, 5);
        set.insert_range(6, 10);

        stream << set;

        CHECK("1:4,6:9" == stream.str());
    }
}

TEST_CASE("range_set - equality operators")
{
    SECTION("empty sets are equal")
    {
        range_set<int> set1;
        range_set<int> set2;

        CHECK(set1 == set2);
    }

    SECTION("sets with single equal value are equal")
    {
        range_set<int> set1{ 5 };
        range_set<int> set2{ 5 };

        CHECK(set1 == set2);
    }

    SECTION("sets with single unequal value are not equal")
    {
        range_set<int> set1{ 5 };
        range_set<int> set2{ 6 };

        CHECK(set1 != set2);
    }

    SECTION("sets with single equal range are equal")
    {
        range_set<int> set1;
        range_set<int> set2;

        set1.insert_range(5, 10);
        set2.insert_range(5, 10);

        CHECK(set1 == set2);
    }

    SECTION("sets with single unequal range are not equal")
    {
        range_set<int> set1;
        set1.insert_range(0, 3);    // range 1
        set1.insert_range(5, 10);   // range 2

        range_set<int> set2;
        auto span = GENERATE(
            // disjoint ranges
            std::pair<int, int>{ -2, -1 },  // before set1, range 1
            std::pair<int, int>{ 4, 4 },    // between set1, ranges 1 and 2
            std::pair<int, int>{ 15, 20 },  // after set1, range 2

            // set2 is subset of set 1, range 1
            std::pair<int, int>{ 0, 0 },    // start only
            std::pair<int, int>{ 0, 1 },    // start - middle
            std::pair<int, int>{ 1, 2 },    // middle - middle
            std::pair<int, int>{ 2, 2 },    // middle only
            std::pair<int, int>{ 2, 3 },    // middle - end
            std::pair<int, int>{ 3, 3 },    // end only

            // set2 is subset of set 1, range 2
            std::pair<int, int>{ 5, 5 },    // start only
            std::pair<int, int>{ 5, 9 },    // start - middle
            std::pair<int, int>{ 7, 9 },    // middle - middle
            std::pair<int, int>{ 7, 7 },    // middle only
            std::pair<int, int>{ 9, 10 },   // middle - end
            std::pair<int, int>{ 10, 10 }   // end only
        );
        set2.insert_range(span.first, span.second);

        CHECK(set1 != set2);
    }

    SECTION("sets with equal ranges added differently are equal")
    {
        range_set<int> set1;
        range_set<int> set2;

        // add items to set1 as a range
        set1.insert_range(0, 5);

        // add to set2 as individual items
        set2.insert(0);
        set2.insert(1);
        set2.insert(2);
        set2.insert(3);
        set2.insert(4);

        CHECK(set1 == set2);
    }
}

TEST_CASE("range_set - reverse iterators")
{
    range_set<int> set{ 0, 1, 2, 7, 8, 9 };

    SECTION("rbegin can be dereferenced")
    {
        CHECK(9 == *set.rbegin());
    }

    SECTION("rend minus 1 can be dereferenced")
    {
        auto itr = set.rend();
        --itr;
        CHECK(0 == *itr);
    }
}

