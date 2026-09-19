#pragma once

#include <types.hpp>
#include <trade.hpp>

class InventoryModel {
    private:
        Position position;
        Cash cash;
        AvgCost avg_cost;
        Pnl realized_pnl;

    public:
        InventoryModel(Cash initial_cash);

        void process_trade(const Trade& trade, Side side);

        Position get_position();
        Cash get_cash();
        AvgCost get_avg_cost();
        Pnl get_realized_pnl();

        Pnl unrealized_pnl(MidPrice ref_price);
        Cash portfolio_value(MidPrice ref_price);
        Cash inventory_exposure(MidPrice ref_price);
};
