import matplotlib.pyplot as plt


def plot_slippage(slippages):
    fig, ax = plt.subplots()
    ax.plot(slippages, label="Slippage")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Slippage")
    ax.set_title("Execution Slippage")
    ax.legend()

    return fig, ax


def plot_slippage_vs_quantity(quantities, slippages):
    fig, ax = plt.subplots()
    ax.scatter(quantities, slippages)

    ax.set_xlabel("Executed Quantity")
    ax.set_ylabel("Slippage")
    ax.set_title("Slippage vs Executed Quantity")

    return fig, ax


def plot_slippage_by_side(buy_slippages, sell_slippages):
    fig, ax = plt.subplots()

    ax.plot(buy_slippages, label="BUY")
    ax.plot(sell_slippages, label="SELL")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Slippage")
    ax.set_title("Slippage by Side")
    ax.legend()

    return fig, ax


def plot_slippage_distribution(slippages):
    fig, ax = plt.subplots()
    ax.hist(slippages, bins=30)

    ax.set_xlabel("Slippage")
    ax.set_ylabel("Frequency")
    ax.set_title("Slippage Distribution")

    return fig, ax