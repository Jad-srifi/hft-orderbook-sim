import lob_cpp


def get_position(inventory):
    return int(inventory.get_position())


def get_cash(inventory):
    return float(inventory.get_cash())


def get_average_cost(inventory):
    return float(inventory.get_avg_cost())


def get_realized_pnl(inventory):
    return float(inventory.get_realized_pnl())


def get_unrealized_pnl(inventory, mark_price):
    return float(inventory.unrealized_pnl(mark_price))


def get_portfolio_value(inventory, mark_price):
    return float(inventory.portfolio_value(mark_price))


def get_inventory_exposure(inventory, mark_price):
    return float(inventory.inventory_exposure(mark_price))


def summarize_inventory(inventory, mark_price):
    return {
        "position": get_position(inventory),
        "cash": get_cash(inventory),
        "average_cost": get_average_cost(inventory),
        "realized_pnl": get_realized_pnl(inventory),
        "unrealized_pnl": get_unrealized_pnl(inventory, mark_price),
        "portfolio_value": get_portfolio_value(inventory, mark_price),
        "inventory_exposure": get_inventory_exposure(inventory, mark_price),
    }