#!/usr/bin/env python3
"""Sweep geometry thresholds and calculate the best global-matching F1 score.

This script reads exception labels directly from the revised neighbor-pair
geometry CSV.  All exception-labelled rows are removed once, before threshold
candidate construction, global maximum-weight matching, or performance
evaluation.  No separate ``exception.csv`` is used.

For every feature and operator listed in ``feature_ranges.csv``, the script
exports a complete threshold sweep.  It also writes:

``best_mwm_f1_summary.csv``
    Best threshold for every feature/operator classifier.
``best_mwm_f1_per_feature.csv``
    Best direction and threshold for each feature.
``index.csv``
    Status and runtime for every sweep.
"""

from __future__ import annotations

import argparse
import csv
import math
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Iterable

import numpy as np


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_INPUT_CSV = SCRIPT_DIR / "batch_neighbor_pair_geometry.csv"
DEFAULT_FEATURE_RANGES_CSV = SCRIPT_DIR / "feature_ranges.csv"
DEFAULT_OUTPUT_DIR = SCRIPT_DIR / "best_f1_results"

EXCEPTION_COLUMN = "exception_label"
EXCEPTION_POSITIVE_VALUE = 1
OBSERVED_COLUMN = "observed_division"
OBSERVED_POSITIVE_VALUE = 1
DEFAULT_OPERATORS = [">=", "<="]


class ProgressBar:
    def __init__(self, total: int, prefix: str = "", width: int = 28) -> None:
        self.total = max(0, int(total))
        self.prefix = prefix
        self.width = width
        self.start = time.time()
        self.last_draw = 0.0

    @staticmethod
    def _format_time(seconds: float) -> str:
        seconds = max(0.0, seconds)
        if seconds < 60:
            return f"{seconds:.1f}s"
        minutes, sec = divmod(int(seconds), 60)
        if minutes < 60:
            return f"{minutes}m{sec:02d}s"
        hours, minutes = divmod(minutes, 60)
        return f"{hours}h{minutes:02d}m"

    def update(self, current: int, force: bool = False) -> None:
        now = time.time()
        if not force and now - self.last_draw < 0.08:
            return
        self.last_draw = now
        current = max(0, min(int(current), self.total))
        fraction = current / self.total if self.total else 1.0
        filled = int(round(self.width * fraction))
        bar = "#" * filled + "-" * (self.width - filled)
        elapsed = now - self.start
        eta = elapsed * (1.0 / fraction - 1.0) if fraction > 0 else 0.0
        sys.stdout.write(
            f"\r{self.prefix} [{bar}] {current}/{self.total} "
            f"({fraction * 100:5.1f}%) elapsed {self._format_time(elapsed)} "
            f"eta {self._format_time(eta)}"
        )
        sys.stdout.flush()

    def close(self) -> None:
        self.update(self.total, force=True)
        sys.stdout.write("\n")
        sys.stdout.flush()


@dataclass(frozen=True)
class FeatureRange:
    order: int
    feature: str
    range_start: float
    range_end: float
    step: float
    feature_type: str


@dataclass(frozen=True)
class MatchingRow:
    row_index: int
    file_name: str
    first_polygon_id: int
    second_polygon_id: int
    feature_value: float


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT_CSV)
    parser.add_argument("--feature-ranges", type=Path, default=DEFAULT_FEATURE_RANGES_CSV)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument(
        "--operators",
        nargs="+",
        default=DEFAULT_OPERATORS,
        help="Threshold directions (default: >= <=). Quote them in a shell if supplied explicitly.",
    )
    parser.add_argument("--exception-column", default=EXCEPTION_COLUMN)
    parser.add_argument("--exception-value", default=str(EXCEPTION_POSITIVE_VALUE))
    parser.add_argument("--observed-column", default=OBSERVED_COLUMN)
    parser.add_argument("--observed-positive-value", type=float, default=OBSERVED_POSITIVE_VALUE)
    parser.add_argument(
        "--features",
        nargs="*",
        default=None,
        help="Optional subset of feature names; the default analyzes all rows in feature_ranges.csv.",
    )
    parser.add_argument("--no-progress", action="store_true")
    return parser.parse_args()


def to_number_if_possible(value: str | None) -> Any:
    if value is None:
        return None
    text = value.strip()
    if text == "":
        return ""
    try:
        if "." not in text and "e" not in text.casefold():
            return int(text)
    except ValueError:
        pass
    try:
        return float(text)
    except ValueError:
        return text


def read_rows(input_csv: Path) -> list[dict[str, Any]]:
    with input_csv.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        if not reader.fieldnames:
            raise ValueError(f"No header was found in {input_csv}")
        return [
            {key: to_number_if_possible(value) for key, value in row.items()}
            for row in reader
        ]


def resolve_column(rows: list[dict[str, Any]], wanted: str) -> str:
    if not rows:
        raise ValueError("The input contains no data rows.")
    columns = list(rows[0].keys())
    if wanted in columns:
        return wanted
    wanted_key = wanted.casefold()
    for column in columns:
        if column.casefold() == wanted_key:
            return column
    raise ValueError(f"Column '{wanted}' was not found. Available columns: {columns}")


def label_is_positive(raw: Any, positive_value: str | int | float) -> bool:
    if raw in (None, ""):
        return False
    try:
        return float(raw) == float(positive_value)
    except (TypeError, ValueError):
        return str(raw).strip().casefold() == str(positive_value).strip().casefold()


def filter_exception_rows(
    rows: list[dict[str, Any]],
    exception_column: str,
    exception_value: str | int | float,
) -> tuple[list[dict[str, Any]], int, str]:
    exception_col = resolve_column(rows, exception_column)
    kept: list[dict[str, Any]] = []
    skipped = 0
    for row in rows:
        if label_is_positive(row.get(exception_col), exception_value):
            skipped += 1
        else:
            kept.append(row)
    return kept, skipped, exception_col


def read_feature_ranges(path: Path) -> list[FeatureRange]:
    ranges: list[FeatureRange] = []
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        required = {"feature", "range_start", "range_end", "step", "feature_type"}
        missing = required - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"feature_ranges.csv is missing columns: {sorted(missing)}")
        for order, row in enumerate(reader, start=1):
            feature = (row.get("feature") or "").strip()
            if not feature:
                continue
            ranges.append(
                FeatureRange(
                    order=order,
                    feature=feature,
                    range_start=float(row["range_start"]),
                    range_end=float(row["range_end"]),
                    step=float(row["step"]),
                    feature_type=(row.get("feature_type") or "").strip(),
                )
            )
    if not ranges:
        raise ValueError(f"No usable feature ranges were found in {path}")
    return ranges


def operator_functions() -> dict[str, Callable[[np.ndarray, float], np.ndarray]]:
    return {
        ">": lambda values, threshold: values > threshold,
        ">=": lambda values, threshold: values >= threshold,
        "<": lambda values, threshold: values < threshold,
        "<=": lambda values, threshold: values <= threshold,
    }


def build_thresholds(start: float, end: float, step: float) -> list[float]:
    if step == 0:
        raise ValueError("Threshold step must not be zero.")
    if start < end and step < 0:
        raise ValueError("Threshold step must be positive for an increasing range.")
    if start > end and step > 0:
        raise ValueError("Threshold step must be negative for a decreasing range.")

    thresholds: list[float] = []
    value = float(start)
    epsilon = abs(step) * 1e-9 + 1e-12
    if step > 0:
        while value <= end + epsilon:
            thresholds.append(float(round(value, 12)))
            value += step
    else:
        while value >= end - epsilon:
            thresholds.append(float(round(value, 12)))
            value += step
    return thresholds


def safe_divide(numerator: float, denominator: float) -> float:
    return numerator / denominator if denominator else 0.0


def metrics_from_boolean_arrays(predicted: np.ndarray, observed: np.ndarray, valid: np.ndarray) -> dict[str, float | int]:
    pred = predicted & valid
    obs = observed & valid
    neg_pred = ~pred & valid
    neg_obs = ~obs & valid

    tp = int(np.sum(pred & obs))
    fp = int(np.sum(pred & neg_obs))
    tn = int(np.sum(neg_pred & neg_obs))
    fn = int(np.sum(neg_pred & obs))
    precision = safe_divide(tp, tp + fp)
    recall = safe_divide(tp, tp + fn)
    specificity = safe_divide(tn, tn + fp)
    accuracy = safe_divide(tp + tn, tp + fp + tn + fn)
    f1 = safe_divide(2 * precision * recall, precision + recall)
    return {
        "TP": tp,
        "FP": fp,
        "TN": tn,
        "FN": fn,
        "precision": precision,
        "recall": recall,
        "specificity": specificity,
        "f1": f1,
        "accuracy": accuracy,
        "balanced_accuracy": 0.5 * (recall + specificity),
        "support_pos": tp + fn,
        "support_neg": tn + fp,
        "pred_pos": tp + fp,
        "pred_neg": tn + fn,
    }


def prepare_feature_arrays(
    rows: list[dict[str, Any]],
    feature: str,
    observed_column: str,
    observed_positive_value: float,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, list[MatchingRow], int]:
    feature_col = resolve_column(rows, feature)
    observed_col = resolve_column(rows, observed_column)
    file_col = resolve_column(rows, "fileName")
    first_col = resolve_column(rows, "firstPolygonId")
    second_col = resolve_column(rows, "secondPolygonId")

    n_rows = len(rows)
    values = np.full(n_rows, np.nan, dtype=float)
    observed = np.zeros(n_rows, dtype=bool)
    valid_observed = np.zeros(n_rows, dtype=bool)
    matching_rows: list[MatchingRow] = []

    for index, row in enumerate(rows):
        raw_observed = row.get(observed_col)
        if raw_observed not in (None, ""):
            try:
                valid_observed[index] = True
                observed[index] = float(raw_observed) == float(observed_positive_value)
            except (TypeError, ValueError):
                pass

        raw_feature = row.get(feature_col)
        try:
            value = float(raw_feature)
        except (TypeError, ValueError):
            continue
        if not math.isfinite(value):
            continue
        values[index] = value

        try:
            file_name = str(row[file_col]).strip()
            first_id = int(row[first_col])
            second_id = int(row[second_col])
        except (KeyError, TypeError, ValueError):
            continue
        if not file_name or first_id == second_id:
            continue
        matching_rows.append(MatchingRow(index, file_name, first_id, second_id, value))

    skipped_missing_observed = int((~valid_observed).sum())
    return values, observed, valid_observed, matching_rows, skipped_missing_observed


def select_global_maximum_weight_matching(
    matching_rows: list[MatchingRow],
    threshold: float,
    operator: str,
) -> set[int]:
    try:
        import networkx as nx
    except ImportError:
        nx = None

    if operator not in {">", ">=", "<", "<="}:
        raise ValueError(f"Unsupported matching operator: {operator}")

    per_file_best: dict[str, dict[tuple[int, int], tuple[float, int]]] = {}
    high_direction = operator in {">", ">="}
    inclusive = operator in {">=", "<="}

    for row in matching_rows:
        if high_direction:
            passes = row.feature_value >= threshold if inclusive else row.feature_value > threshold
            score = row.feature_value - threshold
        else:
            passes = row.feature_value <= threshold if inclusive else row.feature_value < threshold
            score = threshold - row.feature_value
        # With maxcardinality=False, a zero-weight edge does not improve the
        # objective and should remain unmatched.  Omitting it also makes the
        # NetworkX and SciPy backends agree at exact threshold boundaries.
        if not passes or score <= 0:
            continue

        edge = (
            (row.first_polygon_id, row.second_polygon_id)
            if row.first_polygon_id < row.second_polygon_id
            else (row.second_polygon_id, row.first_polygon_id)
        )
        best_edges = per_file_best.setdefault(row.file_name, {})
        previous = best_edges.get(edge)
        if previous is None or score > previous[0]:
            best_edges[edge] = (float(score), row.row_index)

    selected: set[int] = set()
    for file_name in sorted(per_file_best):
        best_edges = per_file_best[file_name]
        if nx is not None:
            graph = nx.Graph()
            for (first_id, second_id), (score, row_index) in sorted(best_edges.items()):
                graph.add_edge(first_id, second_id, weight=score, row_index=row_index)

            matching = nx.algorithms.matching.max_weight_matching(
                graph,
                maxcardinality=False,
                weight="weight",
            )
            for first_id, second_id in matching:
                edge = (first_id, second_id) if first_id < second_id else (second_id, first_id)
                selected.add(best_edges[edge][1])
        else:
            # Exact fallback: binary edge variables with one incidence
            # constraint per polygon.  This solves the same maximum-weight
            # matching objective, but is slower than NetworkX's blossom
            # implementation and is mainly intended for portability.
            try:
                from scipy.optimize import Bounds, LinearConstraint, milp
                from scipy.sparse import coo_matrix
            except ImportError as error:
                raise ImportError(
                    "Exact matching requires either networkx or scipy. "
                    "Install networkx for the fastest full feature sweep."
                ) from error

            edge_items = sorted(best_edges.items())
            vertices = sorted({vertex for edge, _ in edge_items for vertex in edge})
            vertex_index = {vertex: index for index, vertex in enumerate(vertices)}
            row_indices: list[int] = []
            col_indices: list[int] = []
            coefficients: list[float] = []
            weights = np.empty(len(edge_items), dtype=float)

            for edge_index, ((first_id, second_id), (score, _)) in enumerate(edge_items):
                row_indices.extend([vertex_index[first_id], vertex_index[second_id]])
                col_indices.extend([edge_index, edge_index])
                coefficients.extend([1.0, 1.0])
                weights[edge_index] = score

            incidence = coo_matrix(
                (coefficients, (row_indices, col_indices)),
                shape=(len(vertices), len(edge_items)),
            ).tocsr()
            result = milp(
                c=-weights,
                integrality=np.ones(len(edge_items), dtype=int),
                bounds=Bounds(np.zeros(len(edge_items)), np.ones(len(edge_items))),
                constraints=LinearConstraint(
                    incidence,
                    np.zeros(len(vertices)),
                    np.ones(len(vertices)),
                ),
                options={"disp": False},
            )
            if not result.success or result.x is None:
                raise RuntimeError(
                    f"Exact matching failed for file '{file_name}': {result.message}"
                )
            for edge_index in np.flatnonzero(result.x > 0.5):
                _, (_, row_index) = edge_items[int(edge_index)]
                selected.add(row_index)
    return selected


METRIC_NAMES = [
    "TP",
    "FP",
    "TN",
    "FN",
    "precision",
    "recall",
    "specificity",
    "f1",
    "accuracy",
    "balanced_accuracy",
    "support_pos",
    "support_neg",
    "pred_pos",
    "pred_neg",
]


def sweep_one_classifier(
    rows: list[dict[str, Any]],
    feature_range: FeatureRange,
    operator: str,
    output_csv: Path,
    observed_column: str,
    observed_positive_value: float,
    skipped_exception: int,
    show_progress: bool,
) -> dict[str, Any]:
    op_functions = operator_functions()
    if operator not in op_functions:
        raise ValueError(f"Unsupported operator '{operator}'. Use one of {list(op_functions)}")

    thresholds = build_thresholds(
        feature_range.range_start,
        feature_range.range_end,
        feature_range.step,
    )
    values, observed, valid_observed, matching_rows, skipped_missing = prepare_feature_arrays(
        rows,
        feature_range.feature,
        observed_column,
        observed_positive_value,
    )

    fieldnames = (
        [
            "feature",
            "feature_type",
            "op",
            "threshold_start",
            "threshold_end",
            "threshold_step",
            "threshold",
            "skipped_exception",
            "skipped_missing_observed",
        ]
        + [f"thr_{name}" for name in METRIC_NAMES]
        + [f"mwm_{name}" for name in METRIC_NAMES]
    )

    output_csv.parent.mkdir(parents=True, exist_ok=True)
    best_mwm_row: dict[str, Any] | None = None
    best_threshold_row: dict[str, Any] | None = None
    progress = ProgressBar(len(thresholds), prefix=f"  {feature_range.feature} {operator}") if show_progress else None
    update_every = max(1, len(thresholds) // 100)

    with output_csv.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()

        for index, threshold in enumerate(thresholds, start=1):
            finite = np.isfinite(values)
            threshold_prediction = finite & op_functions[operator](values, threshold)
            threshold_metrics = metrics_from_boolean_arrays(threshold_prediction, observed, valid_observed)

            selected_indices = select_global_maximum_weight_matching(matching_rows, threshold, operator)
            matching_prediction = np.zeros(len(rows), dtype=bool)
            if selected_indices:
                matching_prediction[np.fromiter(selected_indices, dtype=int)] = True
            matching_metrics = metrics_from_boolean_arrays(matching_prediction, observed, valid_observed)

            result: dict[str, Any] = {
                "feature": feature_range.feature,
                "feature_type": feature_range.feature_type,
                "op": operator,
                "threshold_start": feature_range.range_start,
                "threshold_end": feature_range.range_end,
                "threshold_step": feature_range.step,
                "threshold": threshold,
                "skipped_exception": skipped_exception,
                "skipped_missing_observed": skipped_missing,
            }
            result.update({f"thr_{name}": threshold_metrics[name] for name in METRIC_NAMES})
            result.update({f"mwm_{name}": matching_metrics[name] for name in METRIC_NAMES})
            writer.writerow(result)

            if best_threshold_row is None or float(result["thr_f1"]) > float(best_threshold_row["thr_f1"]):
                best_threshold_row = result.copy()
            if best_mwm_row is None or float(result["mwm_f1"]) > float(best_mwm_row["mwm_f1"]):
                best_mwm_row = result.copy()

            if progress and (index % update_every == 0 or index == len(thresholds)):
                progress.update(index)

    if progress:
        progress.close()
    if best_mwm_row is None or best_threshold_row is None:
        raise ValueError(f"No thresholds were generated for {feature_range.feature}")

    summary: dict[str, Any] = {
        "feature": feature_range.feature,
        "feature_type": feature_range.feature_type,
        "op": operator,
        "best_threshold": best_mwm_row["threshold"],
        "best_mwm_f1": best_mwm_row["mwm_f1"],
        "best_mwm_precision": best_mwm_row["mwm_precision"],
        "best_mwm_recall": best_mwm_row["mwm_recall"],
        "best_mwm_specificity": best_mwm_row["mwm_specificity"],
        "best_mwm_accuracy": best_mwm_row["mwm_accuracy"],
        "best_mwm_balanced_accuracy": best_mwm_row["mwm_balanced_accuracy"],
        "best_mwm_TP": best_mwm_row["mwm_TP"],
        "best_mwm_FP": best_mwm_row["mwm_FP"],
        "best_mwm_TN": best_mwm_row["mwm_TN"],
        "best_mwm_FN": best_mwm_row["mwm_FN"],
        "best_threshold_only_threshold": best_threshold_row["threshold"],
        "best_threshold_only_f1": best_threshold_row["thr_f1"],
        "best_threshold_only_precision": best_threshold_row["thr_precision"],
        "best_threshold_only_recall": best_threshold_row["thr_recall"],
        "n_analyzed_rows": len(rows),
        "skipped_exception": skipped_exception,
        "skipped_missing_observed": skipped_missing,
    }
    return summary


def safe_filename(text: str) -> str:
    return "".join(character if character.isalnum() or character in "-_." else "_" for character in text)


def operator_tag(operator: str) -> str:
    return {">=": "ge", "<=": "le", ">": "gt", "<": "lt"}[operator]


def write_dict_rows(path: Path, rows: list[dict[str, Any]], fieldnames: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def best_per_feature(summary_rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    best: dict[str, dict[str, Any]] = {}
    order: list[str] = []
    for row in summary_rows:
        feature = str(row["feature"])
        if feature not in best:
            best[feature] = row
            order.append(feature)
        elif float(row["best_mwm_f1"]) > float(best[feature]["best_mwm_f1"]):
            best[feature] = row
    return [best[feature] for feature in order]


def main() -> None:
    args = parse_args()
    valid_operators = set(operator_functions())
    invalid = [operator for operator in args.operators if operator not in valid_operators]
    if invalid:
        raise ValueError(f"Unsupported operators: {invalid}; use one of {sorted(valid_operators)}")

    raw_rows = read_rows(args.input)
    rows, skipped_exception, resolved_exception_col = filter_exception_rows(
        raw_rows,
        args.exception_column,
        args.exception_value,
    )
    if not rows:
        raise ValueError("All input rows were excluded as exceptions.")

    feature_ranges = read_feature_ranges(args.feature_ranges)
    if args.features:
        requested = {feature.casefold() for feature in args.features}
        feature_ranges = [item for item in feature_ranges if item.feature.casefold() in requested]
        found = {item.feature.casefold() for item in feature_ranges}
        missing = sorted(requested - found)
        if missing:
            raise ValueError(f"Requested features not found in feature_ranges.csv: {missing}")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    print(f"Input: {args.input}")
    print(f"Input rows: {len(raw_rows)}")
    print(
        f"Excluded before matching: {skipped_exception} rows where "
        f"{resolved_exception_col}={args.exception_value}"
    )
    print(f"Analyzed rows: {len(rows)}")
    print(f"Classifiers: {len(feature_ranges)} features x {len(args.operators)} operators")

    summary_rows: list[dict[str, Any]] = []
    index_rows: list[dict[str, Any]] = []
    total_tasks = len(feature_ranges) * len(args.operators)
    task_number = 0

    for feature_range in feature_ranges:
        for operator in args.operators:
            task_number += 1
            output_name = (
                f"{feature_range.order:03d}_{safe_filename(feature_range.feature)}"
                f"__{operator_tag(operator)}.csv"
            )
            output_csv = args.out_dir / output_name
            print(f"[{task_number}/{total_tasks}] {feature_range.feature} {operator}")
            start_time = time.time()
            status = "ok"
            error_message = ""
            try:
                summary = sweep_one_classifier(
                    rows,
                    feature_range,
                    operator,
                    output_csv,
                    args.observed_column,
                    args.observed_positive_value,
                    skipped_exception,
                    show_progress=not args.no_progress,
                )
                summary["file"] = output_name
                summary["order"] = feature_range.order
                summary_rows.append(summary)
                print(
                    f"  best threshold={summary['best_threshold']:g}, "
                    f"F1={float(summary['best_mwm_f1']):.6f}"
                )
            except Exception as error:
                status = "fail"
                error_message = str(error)
                print(f"  FAILED: {error_message}")

            index_rows.append(
                {
                    "order": feature_range.order,
                    "feature": feature_range.feature,
                    "feature_type": feature_range.feature_type,
                    "range_start": feature_range.range_start,
                    "range_end": feature_range.range_end,
                    "step": feature_range.step,
                    "op": operator,
                    "output_csv": output_name,
                    "status": status,
                    "error": error_message,
                    "elapsed_sec": round(time.time() - start_time, 3),
                }
            )

    index_fields = [
        "order",
        "feature",
        "feature_type",
        "range_start",
        "range_end",
        "step",
        "op",
        "output_csv",
        "status",
        "error",
        "elapsed_sec",
    ]
    write_dict_rows(args.out_dir / "index.csv", index_rows, index_fields)

    if not summary_rows:
        raise RuntimeError("Every classifier failed; inspect index.csv for details.")

    summary_fields = [
        "file",
        "order",
        "feature",
        "feature_type",
        "op",
        "best_threshold",
        "best_mwm_f1",
        "best_mwm_precision",
        "best_mwm_recall",
        "best_mwm_specificity",
        "best_mwm_accuracy",
        "best_mwm_balanced_accuracy",
        "best_mwm_TP",
        "best_mwm_FP",
        "best_mwm_TN",
        "best_mwm_FN",
        "best_threshold_only_threshold",
        "best_threshold_only_f1",
        "best_threshold_only_precision",
        "best_threshold_only_recall",
        "n_analyzed_rows",
        "skipped_exception",
        "skipped_missing_observed",
    ]
    write_dict_rows(args.out_dir / "best_mwm_f1_summary.csv", summary_rows, summary_fields)
    per_feature = best_per_feature(summary_rows)
    write_dict_rows(args.out_dir / "best_mwm_f1_per_feature.csv", per_feature, summary_fields)

    failures = sum(row["status"] != "ok" for row in index_rows)
    print(f"Completed classifiers: {len(summary_rows)}")
    print(f"Failed classifiers: {failures}")
    print(f"Results: {args.out_dir}")


if __name__ == "__main__":
    main()
