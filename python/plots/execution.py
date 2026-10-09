import matplotlib.pyplot as plt


def plot_vwap(vwaps):
    fig, ax = plt.subplots()
    ax.plot(vwaps, label="VWAP")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Price")
    ax.set_title("Execution VWAP")
    ax.legend()

    return fig, ax


def plot_vwap_vs_reference(vwaps, reference_prices):
    fig, ax = plt.subplots()

    ax.plot(vwaps, label="VWAP")
    ax.plot(reference_prices, label="Reference Price")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Price")
    ax.set_title("VWAP vs Reference Price")
    ax.legend()

    return fig, ax


def plot_execution_cost(execution_costs):
    fig, ax = plt.subplots()
    ax.plot(execution_costs, label="Execution Cost")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Cost")
    ax.set_title("Execution Cost")
    ax.legend()

    return fig, ax


def plot_executed_quantity(executed_quantities):
    fig, ax = plt.subplots()
    ax.plot(executed_quantities, label="Executed Quantity")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Quantity")
    ax.set_title("Executed Quantity")
    ax.legend()

    return fig, ax


def plot_executed_value(executed_values):
    fig, ax = plt.subplots()
    ax.plot(executed_values, label="Executed Value")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Value")
    ax.set_title("Executed Value")
    ax.legend()

    return fig, ax


def plot_liquidity_consumed(liquidity_consumed):
    fig, ax = plt.subplots()
    ax.plot(liquidity_consumed, label="Liquidity Consumed")

    ax.set_xlabel("Observation")
    ax.set_ylabel("Quantity")
    ax.set_title("Liquidity Consumed")
    ax.legend()

    return fig, ax