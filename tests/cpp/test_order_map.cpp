#include <types.hpp>
#include <order_map.hpp>

#include <cassert>
#include <iostream>

int main() {

    OrderMap order_map;

    // --------------------------------------------------
    // Test 1: Add and lookup
    // --------------------------------------------------

    OrderLocation location1 = {
        Side::BUY,
        10000,
        0
    };

    order_map.add(1, location1);

    std::optional<OrderLocation> result = order_map.find(1);

    assert(result.has_value());
    assert(result->side == Side::BUY);
    assert(result->price == 10000);
    assert(result->index == 0);

    std::cout << "Test 1 passed: add and lookup\n";


    // --------------------------------------------------
    // Test 2: Lookup nonexistent order
    // --------------------------------------------------

    std::optional<OrderLocation> missing = order_map.find(999);

    assert(!missing.has_value());

    std::cout << "Test 2 passed: nonexistent lookup\n";


    // --------------------------------------------------
    // Test 3: Update location
    // --------------------------------------------------

    OrderLocation new_location = {
        Side::BUY,
        10000,
        3
    };

    order_map.update(1, new_location);

    result = order_map.find(1);

    assert(result.has_value());
    assert(result->side == Side::BUY);
    assert(result->price == 10000);
    assert(result->index == 3);

    std::cout << "Test 3 passed: update\n";


    // --------------------------------------------------
    // Test 4: Add multiple orders
    // --------------------------------------------------

    OrderLocation location2 = {
        Side::BUY,
        10000,
        4
    };

    OrderLocation location3 = {
        Side::SELL,
        10500,
        0
    };

    order_map.add(2, location2);
    order_map.add(3, location3);

    result = order_map.find(2);

    assert(result.has_value());
    assert(result->side == Side::BUY);
    assert(result->price == 10000);
    assert(result->index == 4);

    result = order_map.find(3);

    assert(result.has_value());
    assert(result->side == Side::SELL);
    assert(result->price == 10500);
    assert(result->index == 0);

    std::cout << "Test 4 passed: multiple orders\n";


    // --------------------------------------------------
    // Test 5: Remove order
    // --------------------------------------------------

    order_map.remove(2);

    result = order_map.find(2);

    assert(!result.has_value());

    std::cout << "Test 5 passed: remove\n";


    // --------------------------------------------------
    // Test 6: Other orders remain after removal
    // --------------------------------------------------

    result = order_map.find(1);

    assert(result.has_value());
    assert(result->index == 3);

    result = order_map.find(3);

    assert(result.has_value());
    assert(result->side == Side::SELL);
    assert(result->price == 10500);
    assert(result->index == 0);

    std::cout << "Test 6 passed: other entries remain\n";


    // --------------------------------------------------
    // Test 7: Remove nonexistent order
    // --------------------------------------------------

    order_map.remove(999);

    result = order_map.find(999);

    assert(!result.has_value());

    std::cout << "Test 7 passed: remove nonexistent order\n";


    // --------------------------------------------------
    // Test 8: Update nonexistent order
    // --------------------------------------------------

    OrderLocation nonexistent_location = {
        Side::SELL,
        11000,
        5
    };

    order_map.update(999, nonexistent_location);

    result = order_map.find(999);

    assert(!result.has_value());

    std::cout << "Test 8 passed: update nonexistent order\n";


    std::cout << "\nAll OrderMap tests passed.\n";

    return 0;
}