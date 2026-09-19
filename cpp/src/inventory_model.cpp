#include <inventory_model.hpp>
#include <cmath>

InventoryModel::InventoryModel(Cash initial_cash)
    :   position(0),
        cash(initial_cash),
        avg_cost(0),
        realized_pnl(0)
{
}

void InventoryModel::process_trade(const Trade& trade, Side side) {

    Position quantity = static_cast<Position>(trade.quantity);
    Price price = trade.price;

    if (quantity == 0) {
        return;
    }

    if (side == Side::BUY) {

        if (position >= 0) {

            if (position == 0) {
                avg_cost = price;
            }
            else {
                avg_cost = (avg_cost * position + price * quantity) / (position + quantity);
            }

            position += quantity;
        }

        else {

            Position short_quantity = -position;

            if (quantity < short_quantity) {

                realized_pnl += (avg_cost - price) * quantity;

                position += quantity;
            }
            else if (quantity == short_quantity) {

                realized_pnl += (avg_cost - price) * quantity;

                position = 0;
                avg_cost = 0;
            }
            else {

                realized_pnl += (avg_cost - price) * short_quantity;

                Position remaining_long = quantity - short_quantity;

                position = remaining_long;
                avg_cost = price;
            }
        }

        cash -= price * static_cast<Quantity>(quantity);
    }

    else if (side == Side::SELL) {

        if (position <= 0) {

            if (position == 0) {
                avg_cost = price;
            }
            else {

                Position short_quantity = -position;

                avg_cost = (avg_cost * short_quantity + price * quantity) / (short_quantity + quantity);
            }

            position -= quantity;
        }

        else {

            if (quantity < position) {

                realized_pnl += (price - avg_cost) * quantity;

                position -= quantity;
            }
            else if (quantity == position) {

                realized_pnl += (price - avg_cost) * quantity;

                position = 0;
                avg_cost = 0;
            }
            else {

                realized_pnl += (price - avg_cost) * position;

                Position remaining_short = quantity - position;

                position = -remaining_short;
                avg_cost = price;
            }
        }

        cash += price * static_cast<Quantity>(quantity);
    }
}

Position InventoryModel::get_position(){
    return this->position;
}

Cash InventoryModel::get_cash(){
    return this->cash;
}
    
AvgCost InventoryModel::get_avg_cost(){
    return this->avg_cost;
}
    
Pnl InventoryModel::get_realized_pnl(){
    return this->realized_pnl;
}

Pnl InventoryModel::unrealized_pnl(MidPrice ref_price){
    return this->position * (ref_price - this->avg_cost);
}

Cash InventoryModel::portfolio_value(MidPrice ref_price){
    return this->cash + this->position * ref_price;
}

Cash InventoryModel::inventory_exposure(MidPrice ref_price){
    return abs(this->position) * ref_price;
}

