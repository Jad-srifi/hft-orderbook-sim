import matplotlib.pyplot as plt


def plot_position(position):
    fig, ax = plt.subplots()
    ax.plot(position, label="Position")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Position")
    ax.set_title("Inventory Position")
    ax.legend()

    return fig, ax


def plot_inventory_exposure(exposure):
    fig, ax = plt.subplots()
    ax.plot(exposure, label="Inventory Exposure")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Exposure")
    ax.set_title("Inventory Exposure")
    ax.legend()

    return fig, ax


def plot_realized_pnl(realized_pnl):
    fig, ax = plt.subplots()
    ax.plot(realized_pnl, label="Realized P&L")

    ax.set_xlabel("Observation")
    ax.set_ylabel("P&L")
    ax.set_title("Realized P&L")
    ax.legend()

    return fig, ax


def plot_unrealized_pnl(unrealized_pnl):
    fig, ax = plt.subplots()
    ax.plot(unrealized_pnl, label="Unrealized P&L")

    ax.set_xlabel("Observation")
    ax.set_ylabel("P&L")
    ax.set_title("Unrealized P&L")
    ax.legend()

    return fig, ax


def plot_portfolio_value(portfolio_value):
    fig, ax = plt.subplots()
    ax.plot(portfolio_value, label="Portfolio Value")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Value")
    ax.set_title("Portfolio Value")
    ax.legend()

    return fig, ax


def plot_inventory_summary(position, realized_pnl, unrealized_pnl, portfolio_value):
    fig, axes = plt.subplots(4, 1, figsize=(8, 10))

    axes[0].plot(position, label="Position")
    axes[0].set_ylabel("Position")
    axes[0].legend()

    axes[1].plot(realized_pnl, label="Realized P&L")
    axes[1].set_ylabel("P&L")
    axes[1].legend()

    axes[2].plot(unrealized_pnl, label="Unrealized P&L")
    axes[2].set_ylabel("P&L")
    axes[2].legend()

    axes[3].plot(portfolio_value, label="Portfolio Value")
    axes[3].set_xlabel("Observation")
    axes[3].set_ylabel("Value")
    axes[3].legend()

    fig.suptitle("Inventory Summary")
    fig.tight_layout()

    return fig, axes