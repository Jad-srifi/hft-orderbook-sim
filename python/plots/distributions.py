import matplotlib.pyplot as plt


def plot_distribution(values, title="Distribution", xlabel="Value", bins=30):
    fig, ax = plt.subplots()
    ax.hist(values, bins=bins)

    ax.set_xlabel(xlabel)
    ax.set_ylabel("Frequency")
    ax.set_title(title)

    return fig, ax


def plot_comparison_distribution(first, second, first_label="First", second_label="Second", bins=30):
    fig, ax = plt.subplots()

    ax.hist(first, bins=bins, alpha=0.6, label=first_label)
    ax.hist(second, bins=bins, alpha=0.6, label=second_label)

    ax.set_xlabel("Value")
    ax.set_ylabel("Frequency")
    ax.set_title("Distribution Comparison")
    ax.legend()

    return fig, ax


def plot_box_distribution(values, labels=None):
    fig, ax = plt.subplots()
    ax.boxplot(values, tick_labels=labels)

    ax.set_ylabel("Value")
    ax.set_title("Distribution Summary")

    return fig, ax


def plot_cumulative_distribution(values):
    sorted_values = sorted(values)
    cumulative = [(index + 1) / len(sorted_values) for index in range(len(sorted_values))]

    fig, ax = plt.subplots()
    ax.plot(sorted_values, cumulative)

    ax.set_xlabel("Value")
    ax.set_ylabel("Cumulative Probability")
    ax.set_title("Cumulative Distribution")

    return fig, ax