import lob_cpp

from research.statistics import mean, median


def extract_slippages(results):
    return [float(result.slippage) for result in results]


def mean_slippage(results):
    return mean(extract_slippages(results))


def median_slippage(results):
    return median(extract_slippages(results))


def slippage_by_side(results_by_order, sides):
    result = {}

    for order_id, execution_result in results_by_order.items():
        side = sides.get(order_id)
        if side is None:
            continue
        result.setdefault(side, []).append(float(execution_result.slippage))

    return result


def mean_slippage_by_side(results_by_order, sides):
    grouped = slippage_by_side(results_by_order, sides)
    return {side: mean(values) for side, values in grouped.items()}


def slippage_by_quantity(results):
    return [(int(result.executed_quantity), float(result.slippage)) for result in results]


def slippage_by_order(results_by_order):
    return {int(order_id): float(result.slippage) for order_id, result in results_by_order.items()}