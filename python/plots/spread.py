import matplotlib.pyplot as plt


def plot_spread(spreads):
    fig, ax = plt.subplots()
    ax.plot(spreads, label="Spread")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Spread")
    ax.set_title("Bid-Ask Spread")
    ax.legend()

    return fig, ax


def plot_relative_spread(relative_spreads):
    fig, ax = plt.subplots()
    ax.plot(relative_spreads, label="Relative Spread")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Relative Spread (%)")
    ax.set_title("Relative Bid-Ask Spread")
    ax.legend()

    return fig, ax


def plot_spread_distribution(spreads):
    fig, ax = plt.subplots()
    ax.hist(spreads, bins=30)

    ax.set_xlabel("Spread")
    ax.set_ylabel("Frequency")
    ax.set_title("Spread Distribution")

    return fig, ax