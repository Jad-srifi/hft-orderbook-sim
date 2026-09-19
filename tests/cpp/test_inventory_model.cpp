#include <inventory_model.hpp>

#include <cassert>
#include <cmath>
#include <iostream>


bool nearly_equal(double a, double b, double epsilon = 1e-9) {
    return std::abs(a - b) < epsilon;
}


Trade make_trade(Price price, Quantity quantity) {
    Trade trade{};
    trade.incoming_order = 0;
    trade.resting_order = 0;
    trade.price = price;
    trade.quantity = quantity;
    return trade;
}


// ============================================================
// INITIAL STATE
// ============================================================

void test_initial_state() {

    InventoryModel inventory(1000000.0);

    assert(inventory.get_position() == 0);
    assert(inventory.get_cash() == 1000000.0);
    assert(inventory.get_avg_cost() == 0.0);
    assert(inventory.get_realized_pnl() == 0.0);

    assert(inventory.unrealized_pnl(10000) == 0.0);
    assert(inventory.portfolio_value(10000) == 1000000.0);
    assert(inventory.inventory_exposure(10000) == 0.0);
}


// ============================================================
// LONG POSITION — BUY
// ============================================================

void test_single_buy() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::BUY);

    assert(inventory.get_position() == 100);
    assert(inventory.get_cash() == 0.0);
    assert(inventory.get_avg_cost() == 10000.0);
    assert(inventory.get_realized_pnl() == 0.0);
}


void test_multiple_buys_weighted_average() {

    InventoryModel inventory(5000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::BUY);
    inventory.process_trade(make_trade(12000, 100), Side::BUY);

    // Cash:
    // 5,000,000 - 1,000,000 - 1,200,000 = 2,800,000

    assert(inventory.get_position() == 200);
    assert(inventory.get_cash() == 2800000.0);
    assert(nearly_equal(inventory.get_avg_cost(), 11000.0));
    assert(inventory.get_realized_pnl() == 0.0);
}


void test_fractional_long_average_cost() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(10000, 3), Side::BUY);
    inventory.process_trade(make_trade(11000, 2), Side::BUY);

    // Average:
    // (3*10,000 + 2*11,000) / 5 = 10,400

    assert(inventory.get_position() == 5);
    assert(inventory.get_cash() == 948000.0);
    assert(nearly_equal(inventory.get_avg_cost(), 10400.0));
    assert(inventory.get_realized_pnl() == 0.0);
}


// ============================================================
// LONG POSITION — SELL
// ============================================================

void test_partial_long_close() {

    InventoryModel inventory(2000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::BUY);
    inventory.process_trade(make_trade(12000, 40), Side::SELL);

    // Cash:
    // 2,000,000 - 1,000,000 + 480,000 = 1,480,000
    //
    // Realized:
    // 40 * (12,000 - 10,000) = 80,000

    assert(inventory.get_position() == 60);
    assert(inventory.get_cash() == 1480000.0);
    assert(inventory.get_avg_cost() == 10000.0);
    assert(inventory.get_realized_pnl() == 80000.0);
}


void test_full_long_close_profit() {

    InventoryModel inventory(2000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::BUY);
    inventory.process_trade(make_trade(12000, 100), Side::SELL);

    assert(inventory.get_position() == 0);
    assert(inventory.get_cash() == 2200000.0);
    assert(inventory.get_avg_cost() == 0.0);
    assert(inventory.get_realized_pnl() == 200000.0);
}


void test_full_long_close_loss() {

    InventoryModel inventory(2000000.0);

    inventory.process_trade(make_trade(12000, 100), Side::BUY);
    inventory.process_trade(make_trade(10000, 100), Side::SELL);

    assert(inventory.get_position() == 0);
    assert(inventory.get_cash() == 1800000.0);
    assert(inventory.get_avg_cost() == 0.0);
    assert(inventory.get_realized_pnl() == -200000.0);
}


// ============================================================
// SHORT POSITION — SELL
// ============================================================

void test_single_sell_opens_short() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(12000, 100), Side::SELL);

    // Cash:
    // 1,000,000 + 1,200,000 = 2,200,000

    assert(inventory.get_position() == -100);
    assert(inventory.get_cash() == 2200000.0);
    assert(inventory.get_avg_cost() == 12000.0);
    assert(inventory.get_realized_pnl() == 0.0);
}


void test_multiple_sells_weighted_average() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::SELL);
    inventory.process_trade(make_trade(12000, 100), Side::SELL);

    // Average:
    // (100*10,000 + 100*12,000) / 200 = 11,000
    //
    // Cash:
    // 1,000,000 + 1,000,000 + 1,200,000 = 3,200,000

    assert(inventory.get_position() == -200);
    assert(inventory.get_cash() == 3200000.0);
    assert(nearly_equal(inventory.get_avg_cost(), 11000.0));
    assert(inventory.get_realized_pnl() == 0.0);
}


void test_fractional_short_average_cost() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(10000, 3), Side::SELL);
    inventory.process_trade(make_trade(11000, 2), Side::SELL);

    // Average:
    // (3*10,000 + 2*11,000) / 5 = 10,400

    assert(inventory.get_position() == -5);
    assert(inventory.get_cash() == 1052000.0);
    assert(nearly_equal(inventory.get_avg_cost(), 10400.0));
    assert(inventory.get_realized_pnl() == 0.0);
}


// ============================================================
// SHORT POSITION — BUY
// ============================================================

void test_partial_short_close() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(12000, 100), Side::SELL);
    inventory.process_trade(make_trade(10000, 40), Side::BUY);

    // Cash:
    // 1,000,000 + 1,200,000 - 400,000 = 1,800,000
    //
    // Realized:
    // 40 * (12,000 - 10,000) = 80,000

    assert(inventory.get_position() == -60);
    assert(inventory.get_cash() == 1800000.0);
    assert(inventory.get_avg_cost() == 12000.0);
    assert(inventory.get_realized_pnl() == 80000.0);
}


void test_full_short_close_profit() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(12000, 100), Side::SELL);
    inventory.process_trade(make_trade(10000, 100), Side::BUY);

    assert(inventory.get_position() == 0);
    assert(inventory.get_cash() == 1200000.0);
    assert(inventory.get_avg_cost() == 0.0);
    assert(inventory.get_realized_pnl() == 200000.0);
}


void test_full_short_close_loss() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::SELL);
    inventory.process_trade(make_trade(12000, 100), Side::BUY);

    assert(inventory.get_position() == 0);
    assert(inventory.get_cash() == 800000.0);
    assert(inventory.get_avg_cost() == 0.0);
    assert(inventory.get_realized_pnl() == -200000.0);
}


// ============================================================
// LONG → SHORT CROSSING
// ============================================================

void test_long_to_short_crossing() {

    InventoryModel inventory(2000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::BUY);
    inventory.process_trade(make_trade(12000, 150), Side::SELL);

    // Buy cost:
    // -1,000,000
    //
    // Sell proceeds:
    // +1,800,000
    //
    // Final cash:
    // 2,800,000
    //
    // First 100 close the long:
    // 100 * (12,000 - 10,000) = +200,000
    //
    // Remaining 50 create a new short @ 12,000

    assert(inventory.get_position() == -50);
    assert(inventory.get_cash() == 2800000.0);
    assert(inventory.get_avg_cost() == 12000.0);
    assert(inventory.get_realized_pnl() == 200000.0);

    assert(inventory.unrealized_pnl(11000) == 50000.0);
    assert(inventory.portfolio_value(11000) == 2250000.0);
    assert(inventory.inventory_exposure(11000) == 550000.0);
}


// ============================================================
// SHORT → LONG CROSSING
// ============================================================

void test_short_to_long_crossing() {

    InventoryModel inventory(2000000.0);

    inventory.process_trade(make_trade(12000, 100), Side::SELL);
    inventory.process_trade(make_trade(10000, 150), Side::BUY);

    // Sell proceeds:
    // +1,200,000
    //
    // Buy cost:
    // -1,500,000
    //
    // Final cash:
    // 1,700,000
    //
    // First 100 close the short:
    // 100 * (12,000 - 10,000) = +200,000
    //
    // Remaining 50 create a new long @ 10,000

    assert(inventory.get_position() == 50);
    assert(inventory.get_cash() == 1700000.0);
    assert(inventory.get_avg_cost() == 10000.0);
    assert(inventory.get_realized_pnl() == 200000.0);

    assert(inventory.unrealized_pnl(11000) == 50000.0);
    assert(inventory.portfolio_value(11000) == 2250000.0);
    assert(inventory.inventory_exposure(11000) == 550000.0);
}


// ============================================================
// CONTINUING AFTER A CROSSING
// ============================================================

void test_add_to_new_short_after_long_to_short_crossing() {

    InventoryModel inventory(2000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::BUY);
    inventory.process_trade(make_trade(12000, 150), Side::SELL);
    inventory.process_trade(make_trade(10000, 50), Side::SELL);

    // Existing short: 50 @ 12,000
    // New short:     50 @ 10,000
    //
    // New average:
    // (50*12,000 + 50*10,000) / 100 = 11,000

    assert(inventory.get_position() == -100);
    assert(inventory.get_cash() == 3300000.0);
    assert(inventory.get_avg_cost() == 11000.0);
    assert(inventory.get_realized_pnl() == 200000.0);

    assert(inventory.unrealized_pnl(11000) == 0.0);
    assert(inventory.portfolio_value(11000) == 2200000.0);
}


void test_add_to_new_long_after_short_to_long_crossing() {

    InventoryModel inventory(2000000.0);

    inventory.process_trade(make_trade(12000, 100), Side::SELL);
    inventory.process_trade(make_trade(10000, 150), Side::BUY);
    inventory.process_trade(make_trade(8000, 50), Side::BUY);

    // Existing long: 50 @ 10,000
    // New long:      50 @ 8,000
    //
    // New average:
    // (50*10,000 + 50*8,000) / 100 = 9,000

    assert(inventory.get_position() == 100);
    assert(inventory.get_cash() == 1300000.0);
    assert(inventory.get_avg_cost() == 9000.0);
    assert(inventory.get_realized_pnl() == 200000.0);

    assert(inventory.unrealized_pnl(9000) == 0.0);
    assert(inventory.portfolio_value(9000) == 2200000.0);
}


// ============================================================
// COST BASIS RESET
// ============================================================

void test_cost_basis_resets_after_flat() {

    InventoryModel inventory(2000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::BUY);
    inventory.process_trade(make_trade(12000, 100), Side::SELL);

    assert(inventory.get_position() == 0);
    assert(inventory.get_avg_cost() == 0.0);

    inventory.process_trade(make_trade(15000, 50), Side::BUY);

    assert(inventory.get_position() == 50);
    assert(inventory.get_avg_cost() == 15000.0);
}


// ============================================================
// ZERO QUANTITY
// ============================================================

void test_zero_quantity_flat() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(10000, 0), Side::BUY);

    assert(inventory.get_position() == 0);
    assert(inventory.get_cash() == 1000000.0);
    assert(inventory.get_avg_cost() == 0.0);
    assert(inventory.get_realized_pnl() == 0.0);

    inventory.process_trade(make_trade(12000, 0), Side::SELL);

    assert(inventory.get_position() == 0);
    assert(inventory.get_cash() == 1000000.0);
    assert(inventory.get_avg_cost() == 0.0);
    assert(inventory.get_realized_pnl() == 0.0);
}


void test_zero_quantity_does_not_change_position() {

    InventoryModel inventory(2000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::BUY);

    Position position_before = inventory.get_position();
    Cash cash_before = inventory.get_cash();
    AvgCost avg_cost_before = inventory.get_avg_cost();
    Pnl realized_before = inventory.get_realized_pnl();

    inventory.process_trade(make_trade(20000, 0), Side::SELL);

    assert(inventory.get_position() == position_before);
    assert(inventory.get_cash() == cash_before);
    assert(inventory.get_avg_cost() == avg_cost_before);
    assert(inventory.get_realized_pnl() == realized_before);
}


// ============================================================
// UNREALIZED P&L
// ============================================================

void test_long_unrealized_pnl() {

    InventoryModel inventory(2000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::BUY);

    assert(inventory.unrealized_pnl(11000) == 100000.0);
    assert(inventory.unrealized_pnl(9000) == -100000.0);
    assert(inventory.unrealized_pnl(10000) == 0.0);
}


void test_short_unrealized_pnl() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(12000, 100), Side::SELL);

    assert(inventory.unrealized_pnl(11000) == 100000.0);
    assert(inventory.unrealized_pnl(13000) == -100000.0);
    assert(inventory.unrealized_pnl(12000) == 0.0);
}


// ============================================================
// PORTFOLIO VALUE
// ============================================================

void test_long_portfolio_value() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::BUY);

    assert(inventory.portfolio_value(9000) == 900000.0);
    assert(inventory.portfolio_value(10000) == 1000000.0);
    assert(inventory.portfolio_value(11000) == 1100000.0);
}


void test_short_portfolio_value() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(12000, 100), Side::SELL);

    assert(inventory.portfolio_value(11000) == 1100000.0);
    assert(inventory.portfolio_value(12000) == 1000000.0);
    assert(inventory.portfolio_value(13000) == 900000.0);
}


// ============================================================
// INVENTORY EXPOSURE
// ============================================================

void test_long_inventory_exposure() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::BUY);

    assert(inventory.inventory_exposure(9000) == 900000.0);
    assert(inventory.inventory_exposure(11000) == 1100000.0);
}


void test_short_inventory_exposure() {

    InventoryModel inventory(1000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::SELL);

    assert(inventory.inventory_exposure(9000) == 900000.0);
    assert(inventory.inventory_exposure(11000) == 1100000.0);
}


// ============================================================
// VALUATION MUST NOT MUTATE STATE
// ============================================================

void test_valuation_does_not_mutate_state() {

    InventoryModel inventory(2000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::BUY);

    Position position_before = inventory.get_position();
    Cash cash_before = inventory.get_cash();
    AvgCost avg_cost_before = inventory.get_avg_cost();
    Pnl realized_before = inventory.get_realized_pnl();

    inventory.unrealized_pnl(9000);
    inventory.unrealized_pnl(11000);
    inventory.portfolio_value(12000);
    inventory.inventory_exposure(13000);

    assert(inventory.get_position() == position_before);
    assert(inventory.get_cash() == cash_before);
    assert(inventory.get_avg_cost() == avg_cost_before);
    assert(inventory.get_realized_pnl() == realized_before);
}


// ============================================================
// ACCOUNTING IDENTITY
// ============================================================

void test_long_accounting_identity() {

    InventoryModel inventory(2000000.0);

    inventory.process_trade(make_trade(10000, 100), Side::BUY);
    inventory.process_trade(make_trade(12000, 40), Side::SELL);

    Pnl unrealized = inventory.unrealized_pnl(11000);
    Pnl realized = inventory.get_realized_pnl();

    Cash portfolio_value = inventory.portfolio_value(11000);

    assert(nearly_equal(
        portfolio_value,
        2000000.0 + realized + unrealized
    ));
}


void test_short_accounting_identity() {

    InventoryModel inventory(2000000.0);

    inventory.process_trade(make_trade(12000, 100), Side::SELL);
    inventory.process_trade(make_trade(10000, 40), Side::BUY);

    Pnl unrealized = inventory.unrealized_pnl(11000);
    Pnl realized = inventory.get_realized_pnl();

    Cash portfolio_value = inventory.portfolio_value(11000);

    assert(nearly_equal(
        portfolio_value,
        2000000.0 + realized + unrealized
    ));
}


// ============================================================
// MAIN
// ============================================================

int main() {

    test_initial_state();

    test_single_buy();
    test_multiple_buys_weighted_average();
    test_fractional_long_average_cost();

    test_partial_long_close();
    test_full_long_close_profit();
    test_full_long_close_loss();

    test_single_sell_opens_short();
    test_multiple_sells_weighted_average();
    test_fractional_short_average_cost();

    test_partial_short_close();
    test_full_short_close_profit();
    test_full_short_close_loss();

    test_long_to_short_crossing();
    test_short_to_long_crossing();

    test_add_to_new_short_after_long_to_short_crossing();
    test_add_to_new_long_after_short_to_long_crossing();

    test_cost_basis_resets_after_flat();

    test_zero_quantity_flat();
    test_zero_quantity_does_not_change_position();

    test_long_unrealized_pnl();
    test_short_unrealized_pnl();

    test_long_portfolio_value();
    test_short_portfolio_value();

    test_long_inventory_exposure();
    test_short_inventory_exposure();

    test_valuation_does_not_mutate_state();

    test_long_accounting_identity();
    test_short_accounting_identity();

    std::cout << "All InventoryModel tests passed.\n";

    return 0;
}