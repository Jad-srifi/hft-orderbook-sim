import matplotlib.pyplot as plt


def plot_midpoint(midpoints):
    fig, ax = plt.subplots()
    ax.plot(midpoints, label="Midpoint")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Price")
    ax.set_title("Midpoint Over Time")
    ax.legend()

    return fig, ax


def plot_midpoint_returns(returns):
    fig, ax = plt.subplots()
    ax.plot(returns, label="Midpoint Return")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Return")
    ax.set_title("Midpoint Returns")
    ax.legend()

    return fig, ax


def plot_midpoint_distribution(midpoints):
    fig, ax = plt.subplots()
    ax.hist(midpoints, bins=30)

    ax.set_xlabel("Midpoint")
    ax.set_ylabel("Frequency")
    ax.set_title("Midpoint Distribution")

    return fig, ax


def plot_return_distribution(returns):
    fig, ax = plt.subplots()
    ax.hist(returns, bins=30)

    ax.set_xlabel("Return")
    ax.set_ylabel("Frequency")
    ax.set_title("Midpoint Return Distribution")

    return fig, ax