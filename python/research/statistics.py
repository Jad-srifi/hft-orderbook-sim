import numpy as np


def mean(x):
    if len(x) == 0:
        return 0.0

    return float(np.mean(x))


def median(x):
    if len(x) == 0:
        return 0.0

    return float(np.median(x))


def variance(x):
    if len(x) < 2:
        return 0.0

    return float(np.var(x, ddof=1))


def standard_deviation(x):
    if len(x) < 2:
        return 0.0

    return float(np.std(x, ddof=1))


def quantile(x, q):
    if len(x) == 0:
        return 0.0

    if not 0.0 <= q <= 1.0:
        raise ValueError("q must be between 0 and 1")

    return float(np.quantile(x, q))


def correlation(x, y):
    if len(x) != len(y):
        raise ValueError("x and y must have the same length")

    if len(x) < 2:
        return 0.0

    if np.std(x) == 0.0 or np.std(y) == 0.0:
        return 0.0

    return float(np.corrcoef(x, y)[0, 1])


def autocorrelation(x, lag):
    if lag < 1:
        raise ValueError("lag must be at least 1")

    if lag >= len(x):
        raise ValueError("lag must be smaller than the length of x")

    return correlation(x[:-lag], x[lag:])


def describe(x, y=None, q=0.5, lag=1):
    result = f"""Mean: {mean(x)}
Median: {median(x)}
Variance: {variance(x)}
STD: {standard_deviation(x)}
Quantile({q * 100}%): {quantile(x, q)}
Autocorrelation(Lag={lag}): {autocorrelation(x, lag)}"""

    if y is not None:
        result += f"""
Correlation: {correlation(x, y)}"""

    return result