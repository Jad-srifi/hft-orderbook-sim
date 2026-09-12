#include <order_book.hpp>
#include <cassert>
#include <iostream>

int main() {

    OrderBook book;

    // ============================================================
    // 1. INITIAL ADDITIONS
    // ============================================================

    book.add({1, Side::BUY, 10000, 100});
    book.add({2, Side::BUY, 10000, 200});
    book.add({3, Side::BUY, 10000, 300});

    book.add({4, Side::BUY, 10100, 100});
    book.add({5, Side::BUY, 10100, 200});

    book.add({6, Side::SELL, 10500, 100});
    book.add({7, Side::SELL, 10500, 200});
    book.add({8, Side::SELL, 10600, 300});

    // Every order must exist in OrderMap.
    assert(book.order_map.find(1).has_value());
    assert(book.order_map.find(2).has_value());
    assert(book.order_map.find(3).has_value());
    assert(book.order_map.find(4).has_value());
    assert(book.order_map.find(5).has_value());
    assert(book.order_map.find(6).has_value());
    assert(book.order_map.find(7).has_value());
    assert(book.order_map.find(8).has_value());

    // Check initial indices.
    assert(book.order_map.find(1)->index == 0);
    assert(book.order_map.find(2)->index == 1);
    assert(book.order_map.find(3)->index == 2);

    assert(book.order_map.find(4)->index == 0);
    assert(book.order_map.find(5)->index == 1);

    assert(book.order_map.find(6)->index == 0);
    assert(book.order_map.find(7)->index == 1);

    assert(book.order_map.find(8)->index == 0);

    std::cout << "PASS: initial OrderMap state\n";


    // ============================================================
    // 2. CANCEL MIDDLE ORDER
    // ============================================================

    book.cancel(2);

    assert(!book.order_map.find(2).has_value());

    // Order 3 shifted from index 2 -> index 1.
    assert(book.order_map.find(1)->index == 0);
    assert(book.order_map.find(3)->index == 1);

    // Other price levels unaffected.
    assert(book.order_map.find(4)->index == 0);
    assert(book.order_map.find(5)->index == 1);

    std::cout << "PASS: middle cancellation updates shifted indices\n";


    // ============================================================
    // 3. CANCEL FIRST ORDER
    // ============================================================

    book.cancel(1);

    assert(!book.order_map.find(1).has_value());

    // Order 3 shifts from index 1 -> index 0.
    assert(book.order_map.find(3)->index == 0);

    std::cout << "PASS: first cancellation updates shifted index\n";


    // ============================================================
    // 4. CANCEL LAST ORDER AT PRICE LEVEL
    // ============================================================

    book.cancel(3);

    assert(!book.order_map.find(3).has_value());

    // 10000 price level should now be gone.
    assert(book.best_bid() == 10100);

    std::cout << "PASS: empty price level removed\n";


    // ============================================================
    // 5. PARTIAL FILL
    // ============================================================

    // Best ask = 10500.
    // Order 6 has quantity 100.
    // Incoming BUY takes only 50.
    Order buy_partial = {9, Side::BUY, 10500, 50};

    auto trades = book.process_order(buy_partial);

    assert(trades.size() == 1);

    assert(trades[0].incoming_order == 9);
    assert(trades[0].resting_order == 6);
    assert(trades[0].price == 10500);
    assert(trades[0].quantity == 50);

    // Order 6 remains in the book.
    assert(book.order_map.find(6).has_value());

    // Its location must not change.
    assert(book.order_map.find(6)->price == 10500);
    assert(book.order_map.find(6)->index == 0);

    std::cout << "PASS: partial fill preserves OrderMap entry\n";


    // ============================================================
    // 6. FULL FILL OF FIRST ORDER AT LEVEL
    // ============================================================

    // Order 6 now has 50 remaining.
    Order buy_full = {10, Side::BUY, 10500, 50};

    trades = book.process_order(buy_full);

    assert(trades.size() == 1);

    assert(trades[0].resting_order == 6);
    assert(trades[0].quantity == 50);

    // Order 6 must be completely gone.
    assert(!book.order_map.find(6).has_value());

    // Order 7 should now be at index 0.
    assert(book.order_map.find(7).has_value());
    assert(book.order_map.find(7)->index == 0);

    std::cout << "PASS: full fill removes order and updates shifted index\n";


    // ============================================================
    // 7. MULTI-LEVEL BUY MATCH
    // ============================================================

    // Current asks:
    //
    // 10500 -> Order 7 = 200
    // 10600 -> Order 8 = 300
    //
    // Buy 450:
    //   takes 200 from Order 7
    //   takes 250 from Order 8
    //
    // Order 7 disappears.
    // Order 8 remains with 50.

    Order buy_multi = {11, Side::BUY, 10600, 450};

    trades = book.process_order(buy_multi);

    assert(trades.size() == 2);

    assert(trades[0].resting_order == 7);
    assert(trades[0].price == 10500);
    assert(trades[0].quantity == 200);

    assert(trades[1].resting_order == 8);
    assert(trades[1].price == 10600);
    assert(trades[1].quantity == 250);

    // Order 7 completely consumed.
    assert(!book.order_map.find(7).has_value());

    // Order 8 partially consumed.
    assert(book.order_map.find(8).has_value());
    assert(book.order_map.find(8)->price == 10600);
    assert(book.order_map.find(8)->index == 0);

    // Incoming order completely executed.
    assert(buy_multi.quantity == 0);
    assert(!book.order_map.find(11).has_value());

    std::cout << "PASS: multi-level BUY matching preserves OrderMap\n";


    // ============================================================
    // 8. ADD MULTIPLE BUY ORDERS AGAIN
    // ============================================================

    book.add({12, Side::BUY, 10000, 100});
    book.add({13, Side::BUY, 10000, 200});
    book.add({14, Side::BUY, 9900, 300});

    assert(book.order_map.find(12).has_value());
    assert(book.order_map.find(13).has_value());
    assert(book.order_map.find(14).has_value());

    assert(book.order_map.find(12)->index == 0);
    assert(book.order_map.find(13)->index == 1);
    assert(book.order_map.find(14)->index == 0);

    std::cout << "PASS: new orders correctly inserted after previous deletions\n";


    // ============================================================
    // 9. SELL MULTI-LEVEL MATCH
    // ============================================================

    // Current BUY side:
    //
    // 10100 -> Order 4 = 100
    //          Order 5 = 200
    //
    // 10000 -> Order 12 = 100
    //          Order 13 = 200
    //
    // 9900  -> Order 14 = 300
    //
    // Sell 350 at 9900:
    //
    // Order 4: 100 completely consumed
    // Order 5: 200 completely consumed
    // Order 12: 50 partially consumed
    //
    // NOTE:
    // best bid is 10100, so all three levels cross.

    Order sell_multi = {15, Side::SELL, 9900, 350};

    trades = book.process_order(sell_multi);

    assert(trades.size() == 3);

    assert(trades[0].resting_order == 4);
    assert(trades[0].price == 10100);
    assert(trades[0].quantity == 100);

    assert(trades[1].resting_order == 5);
    assert(trades[1].price == 10100);
    assert(trades[1].quantity == 200);

    assert(trades[2].resting_order == 12);
    assert(trades[2].price == 10000);
    assert(trades[2].quantity == 50);

    // Fully consumed orders removed.
    assert(!book.order_map.find(4).has_value());
    assert(!book.order_map.find(5).has_value());

    // Partially consumed order remains.
    assert(book.order_map.find(12).has_value());

    // Incoming order completely executed.
    assert(sell_multi.quantity == 0);
    assert(!book.order_map.find(15).has_value());

    std::cout << "PASS: multi-level SELL matching preserves OrderMap\n";


    // ============================================================
    // 10. VERIFY SHIFTED INDEX AFTER SELL MATCH
    // ============================================================

    // Order 13 should still be at index 1 in the 10000 level.
    //
    // Order 12 was at index 0 and remains there.
    // Order 13 was at index 1 and should remain index 1.

    assert(book.order_map.find(12)->index == 0);
    assert(book.order_map.find(13)->index == 1);

    std::cout << "PASS: SELL partial fill leaves indices correct\n";


    // ============================================================
    // 11. CANCEL AFTER MATCHING
    // ============================================================

    book.cancel(13);

    assert(!book.order_map.find(13).has_value());

    // Order 12 should remain at index 0.
    assert(book.order_map.find(12)->index == 0);

    std::cout << "PASS: cancellation remains correct after matching\n";


    // ============================================================
    // 12. CANCEL NONEXISTENT ORDERS
    // ============================================================

    book.cancel(999999);

    // Existing orders must remain untouched.
    assert(book.order_map.find(8).has_value());
    assert(book.order_map.find(12).has_value());
    assert(book.order_map.find(14).has_value());

    std::cout << "PASS: nonexistent cancellation does not corrupt book\n";


    // ============================================================
    // 13. FINAL CONSISTENCY CHECK
    // ============================================================

    // Every remaining known resting order must exist in OrderMap.
    assert(book.order_map.find(8).has_value());
    assert(book.order_map.find(12).has_value());
    assert(book.order_map.find(14).has_value());

    // Every fully consumed order must be gone.
    assert(!book.order_map.find(1).has_value());
    assert(!book.order_map.find(2).has_value());
    assert(!book.order_map.find(3).has_value());
    assert(!book.order_map.find(4).has_value());
    assert(!book.order_map.find(5).has_value());
    assert(!book.order_map.find(6).has_value());
    assert(!book.order_map.find(7).has_value());
    assert(!book.order_map.find(11).has_value());
    assert(!book.order_map.find(13).has_value());
    assert(!book.order_map.find(15).has_value());
    
    std::cout << "PASS: final OrderMap consistency\n";
    
    // Quick test. FIND_ORDER HELPER
    assert(book.find_order(8) != nullptr);
    assert(book.find_order(8)->id == 8);
    assert(book.find_order(8)->side == Side::SELL);
    assert(book.find_order(8)->price == 10600);
    assert(book.find_order(8)->quantity == 50);

    assert(book.find_order(999999) == nullptr);

    // ============================================================
    // 14. ORDER MODIFICATION / REPLACE
    // ============================================================

    // ------------------------------------------------------------
    // 14.1 Same-price quantity decrease preserves FIFO
    // ------------------------------------------------------------

    book.add({16, Side::BUY, 10100, 100});
    book.add({17, Side::BUY, 10100, 200});
    book.add({18, Side::BUY, 10100, 300});

    // Order 16 is first, 17 second, 18 third.
    assert(book.order_map.find(16)->index == 0);
    assert(book.order_map.find(17)->index == 1);
    assert(book.order_map.find(18)->index == 2);

    // Decrease Order 17 from 200 -> 100.
    // Same price + smaller quantity = preserve FIFO.
    assert(book.modify(17, 10100, 100));

    assert(book.find_order(17) != nullptr);
    assert(book.find_order(17)->quantity == 100);
    assert(book.find_order(17)->price == 10100);

    // Position must remain unchanged.
    assert(book.order_map.find(17)->index == 1);

    assert(book.order_map.find(16)->index == 0);
    assert(book.order_map.find(18)->index == 2);

    std::cout << "PASS: quantity decrease preserves FIFO\n";


    // ------------------------------------------------------------
    // 14.2 Same-price quantity increase loses FIFO
    // ------------------------------------------------------------

    // Increase Order 16 from 100 -> 150.
    // This requires cancel + re-add, so it goes to the back.
    assert(book.modify(16, 10100, 150));

    assert(book.find_order(16) != nullptr);
    assert(book.find_order(16)->quantity == 150);
    assert(book.find_order(16)->price == 10100);

    // Expected order sequence:
    // 17 -> index 0
    // 18 -> index 1
    // 16 -> index 2

    assert(book.order_map.find(17)->index == 0);
    assert(book.order_map.find(18)->index == 1);
    assert(book.order_map.find(16)->index == 2);

    std::cout << "PASS: quantity increase loses FIFO\n";


    // ------------------------------------------------------------
    // 14.3 Price modification loses FIFO
    // ------------------------------------------------------------

    // Move Order 17 from 10100 -> 10200.
    // It must be removed from the old level and added to the new level.
    assert(book.modify(17, 10200, 100));

    assert(book.find_order(17) != nullptr);
    assert(book.find_order(17)->price == 10200);
    assert(book.find_order(17)->quantity == 100);

    assert(book.order_map.find(17)->price == 10200);
    assert(book.order_map.find(17)->index == 0);

    std::cout << "PASS: price modification moves order to new level\n";


    // ------------------------------------------------------------
    // 14.4 Order ID and side are preserved
    // ------------------------------------------------------------

    assert(book.find_order(17)->id == 17);
    assert(book.find_order(17)->side == Side::BUY);

    std::cout << "PASS: modification preserves Order ID and side\n";


    // ------------------------------------------------------------
    // 14.5 Shifted indices remain correct after modification
    // ------------------------------------------------------------

    // Current 10100 level:
    // Order 18
    // Order 16
    //
    // Order 17 was moved away, so Order 16 must now be index 1.
    assert(book.order_map.find(18)->index == 0);
    assert(book.order_map.find(16)->index == 1);

    std::cout << "PASS: OrderMap indices remain correct after replacement\n";


    // ------------------------------------------------------------
    // 14.6 Zero quantity = cancellation
    // ------------------------------------------------------------

    assert(book.modify(18, 10100, 0));

    assert(book.find_order(18) == nullptr);
    assert(!book.order_map.find(18).has_value());

    std::cout << "PASS: zero quantity cancels order\n";


    // ------------------------------------------------------------
    // 14.7 Nonexistent order
    // ------------------------------------------------------------

    assert(!book.modify(999999, 10000, 100));

    std::cout << "PASS: nonexistent modification returns false\n";


    // ------------------------------------------------------------
    // 14.8 Modification after partial fill
    // ------------------------------------------------------------

    // Order 8 currently has 50 remaining at 10600.
    assert(book.find_order(8) != nullptr);
    assert(book.find_order(8)->quantity == 50);

    // Decrease remaining quantity.
    // Same price + smaller quantity preserves priority.
    assert(book.modify(8, 10600, 25));

    assert(book.find_order(8) != nullptr);
    assert(book.find_order(8)->quantity == 25);
    assert(book.order_map.find(8)->price == 10600);
    assert(book.order_map.find(8)->index == 0);

    std::cout << "PASS: modification after partial fill\n";


    // ------------------------------------------------------------
    // 14.9 Modified order can match correctly
    // ------------------------------------------------------------

    // Order 8 now has 25 at 10600.
    // Buy 25 at 10600 should completely consume it.
    Order buy_modified = {19, Side::BUY, 10600, 25};

    trades = book.process_order(buy_modified);

    assert(trades.size() == 1);
    assert(trades[0].incoming_order == 19);
    assert(trades[0].resting_order == 8);
    assert(trades[0].price == 10600);
    assert(trades[0].quantity == 25);

    assert(buy_modified.quantity == 0);
    assert(book.find_order(8) == nullptr);
    assert(!book.order_map.find(8).has_value());
    assert(!book.order_map.find(19).has_value());

    std::cout << "PASS: modified order matches correctly\n";


    // ------------------------------------------------------------
    // 14.10 FIFO reset after replacement
    // ------------------------------------------------------------

    book.add({20, Side::BUY, 9000, 100});
    book.add({21, Side::BUY, 9000, 100});

    assert(book.order_map.find(20)->index == 0);
    assert(book.order_map.find(21)->index == 1);

    assert(book.modify(20, 9000, 200));

    assert(book.order_map.find(21)->index == 0);
    assert(book.order_map.find(20)->index == 1);

    std::cout << "PASS: FIFO resets after replacement\n";


    // ============================================================
    // FINAL RESULT
    // ============================================================

    std::cout << "\n========================================\n";
    std::cout << "ALL ORDER BOOK TESTS PASSED\n";
    std::cout << "========================================\n";

    return 0;
}