import matplotlib.pyplot as plt


def plot_imbalance(imbalances):
    fig, ax = plt.subplots()
    ax.plot(imbalances, label="Imbalance")
    ax.axhline(0.0, linestyle="--", label="Neutral")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Imbalance")
    ax.set_title("Order Book Imbalance")
    ax.legend()

    return fig, ax


def plot_bid_ask_depth(bid_depth, ask_depth):
    fig, ax = plt.subplots()

    ax.plot(bid_depth, label="Bid Depth")
    ax.plot(ask_depth, label="Ask Depth")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Quantity")
    ax.set_title("Bid and Ask Depth")
    ax.legend()

    return fig, ax


def plot_imbalance_distribution(imbalances):
    fig, ax = plt.subplots()
    ax.hist(imbalances, bins=30)

    ax.set_xlabel("Imbalance")
    ax.set_ylabel("Frequency")
    ax.set_title("Imbalance Distribution")

    return fig, ax


def plot_imbalance_vs_return(imbalances, returns):
    fig, ax = plt.subplots()
    ax.scatter(imbalances, returns)

    ax.set_xlabel("Imbalance")
    ax.set_ylabel("Future Return")
    ax.set_title("Imbalance vs Future Return")

    return fig, ax