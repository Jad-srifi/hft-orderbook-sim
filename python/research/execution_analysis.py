import lob_cpp


def group_trades_by_incoming_order(trades: list[lob_cpp.Trade]) -> dict[int, list[lob_cpp.Trade]]:
    return lob_cpp.group_trades_by_incoming_order(trades)


def calculate_executed_quantity(trades: list[lob_cpp.Trade]) -> int:
    return lob_cpp.calculate_executed_quantity(trades)


def calculate_executed_value(trades: list[lob_cpp.Trade]) -> int:
    return lob_cpp.calculate_executed_value(trades)


def calculate_VWAP(trades: list[lob_cpp.Trade]) -> float:
    return lob_cpp.calculate_VWAP(trades)


def calculate_slippage(trades: list[lob_cpp.Trade], side: lob_cpp.Side, reference_price: float) -> float:
    return lob_cpp.calculate_slippage(
        trades,
        side,
        reference_price,
    )


def calculate_execution_cost(trades: list[lob_cpp.Trade], side: lob_cpp.Side, reference_price: float) -> float:
    return lob_cpp.calculate_execution_cost(
        trades,
        side,
        reference_price,
    )


def calculate_liquidity_consumed(trades: list[lob_cpp.Trade]) -> int:
    return lob_cpp.calculate_liquidity_consumed(trades)


def calculate_execution_result(trades: list[lob_cpp.Trade], side: lob_cpp.Side, reference_price: float) -> lob_cpp.ExecutionResult:
    return lob_cpp.calculate_execution_result(
        trades,
        side,
        reference_price,
    )


def calculate_execution_results_by_order(trades: list[lob_cpp.Trade], sides: dict[int, lob_cpp.Side], reference_prices: dict[int, float]) -> dict[int, lob_cpp.ExecutionResult]:
    return lob_cpp.calculate_execution_results_by_order(
        trades,
        sides,
        reference_prices,
    )