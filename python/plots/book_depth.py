import matplotlib.pyplot as plt


def plot_bid_ask_depth(snapshot):
    bid_prices = [price for price, _ in snapshot.bid_levels]
    bid_quantities = [quantity for _, quantity in snapshot.bid_levels]
    ask_prices = [price for price, _ in snapshot.ask_levels]
    ask_quantities = [quantity for _, quantity in snapshot.ask_levels]

    fig, ax = plt.subplots()

    if bid_prices:
        ax.barh(bid_prices, bid_quantities, label="Bid Depth")

    if ask_prices:
        ax.barh(ask_prices, ask_quantities, label="Ask Depth")

    ax.set_xlabel("Quantity")
    ax.set_ylabel("Price")
    ax.set_title("Order Book Depth")
    ax.legend()

    return fig, ax


def plot_depth_by_level(snapshot):
    bid_levels = list(enumerate((quantity for _, quantity in snapshot.bid_levels), start=1))
    ask_levels = list(enumerate((quantity for _, quantity in snapshot.ask_levels), start=1))

    fig, ax = plt.subplots()

    if bid_levels:
        ax.plot([level for level, _ in bid_levels], [quantity for _, quantity in bid_levels], marker="o", label="Bid Depth")

    if ask_levels:
        ax.plot([level for level, _ in ask_levels], [quantity for _, quantity in ask_levels], marker="o", label="Ask Depth")

    ax.set_xlabel("Book Level")
    ax.set_ylabel("Quantity")
    ax.set_title("Depth by Book Level")
    ax.legend()

    return fig, ax


def plot_total_depth(snapshots):
    bid_depth = [sum(quantity for _, quantity in snapshot.bid_levels) for snapshot in snapshots]
    ask_depth = [sum(quantity for _, quantity in snapshot.ask_levels) for snapshot in snapshots]

    fig, ax = plt.subplots()

    ax.plot(bid_depth, label="Bid Depth")
    ax.plot(ask_depth, label="Ask Depth")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Total Quantity")
    ax.set_title("Total Bid and Ask Depth")
    ax.legend()

    return fig, ax


def plot_bid_ask_depth_change(snapshots):
    bid_depth = [sum(quantity for _, quantity in snapshot.bid_levels) for snapshot in snapshots]
    ask_depth = [sum(quantity for _, quantity in snapshot.ask_levels) for snapshot in snapshots]

    bid_change = [0]
    ask_change = [0]

    for index in range(1, len(snapshots)):
        bid_change.append(bid_depth[index] - bid_depth[index - 1])
        ask_change.append(ask_depth[index] - ask_depth[index - 1])

    fig, ax = plt.subplots()

    ax.plot(bid_change, label="Bid Depth Change")
    ax.plot(ask_change, label="Ask Depth Change")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Quantity Change")
    ax.set_title("Bid and Ask Depth Changes")
    ax.legend()

    return fig, ax