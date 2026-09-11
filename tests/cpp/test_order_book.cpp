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


    // ============================================================
    // FINAL RESULT
    // ============================================================

    std::cout << "\n========================================\n";
    std::cout << "ALL ORDER BOOK TESTS PASSED\n";
    std::cout << "========================================\n";

    return 0;
}