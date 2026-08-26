from __future__ import annotations

"""
Final plotting script for Supplementary Figure S1.

This script generates a two-panel performance summary figure. In the default
mode it loads estimate_by_single_threshold.py as the analysis engine, so the
same exception filtering, threshold rule, global maximum-weight matching, and
TP/FP/FN/TN classification are used everywhere. A previously summarized
performance CSV remains supported as a backward-compatible input:
  - Panel A: individual-level performance (one dot per fileName / snapshot)
  - Panel B: grouped performance (one dot per filename prefix / primordium)

Both panels show F1 score, precision, and recall as boxplots with jittered
points. Each mean ± SD score is printed below its metric label, outside the
main plotting area. The two panels share the same y-axis range. The y-axis is
displayed as percentages (0-100) and labeled "Score (%)".

Grouped metrics are recomputed by pooling TP / FP / TN / FN within each group,
then recalculating precision, recall, and F1 from those pooled counts.
Groups are inferred directly from each normalized fileName by removing its
terminal time-point suffix. For example, sample1_18h.json, sample1_38h.json,
and sample1_58h.json are grouped as sample1. No snapshot_groups.csv is needed.

In geometry-input mode, rows with exception_label == 1 are removed before
thresholding and before each snapshot's exact global maximum-weight matching.
They therefore cannot occupy polygon vertices or affect TP/FP/FN/TN counts.

Outputs:
  - PNG figure
  - individual-level metrics CSV
  - group-level metrics CSV
  - long-format values CSV for plotting
  - summary CSV of mean ± SD
"""

import argparse
import csv
import importlib.util
import re
import sys
from pathlib import Path
from types import ModuleType
from typing import Any, Callable, Dict, Iterable, List, Sequence, Set, Tuple

import matplotlib.pyplot as plt
import numpy as np


# =========================
# Default configuration
# =========================
DEFAULT_INPUT_GEOMETRY_CSV = "batch_neighbor_pair_geometry.csv"
DEFAULT_INPUT_PERFORMANCE_CSV = None
DEFAULT_ANALYSIS_SCRIPT = "estimate_by_single_threshold(2).py"
DEFAULT_OUTPUT_DIR = "performance_by_individual_grouped"
DEFAULT_BASENAME = "performance_by_individual_grouped"
DEFAULT_DATASET_LABEL = "WT"
DEFAULT_DPI = 600
DEFAULT_RANDOM_SEED = 123

DEFAULT_GEOMETRY_COLUMN = "junctionAngleAverageDegrees"
DEFAULT_THRESHOLD = 145.0
DEFAULT_OPERATOR = ">="
# Retained for the legacy self-contained helper API; command-line geometry mode
# uses estimate_by_single_threshold.py as the canonical implementation.
DEFAULT_FILE_COLUMN = "fileName"
DEFAULT_FIRST_POLYGON_COLUMN = "firstPolygonId"
DEFAULT_SECOND_POLYGON_COLUMN = "secondPolygonId"
DEFAULT_OBSERVED_COLUMN = "observed_division"
DEFAULT_OBSERVED_POSITIVE_VALUE = "1"
DEFAULT_EXCEPTION_COLUMN = "exception_label"
DEFAULT_EXCEPTION_POSITIVE_VALUE = "1"

# Colorblind-friendly palette, used consistently for individual and grouped data.
METRICS: List[Tuple[str, str, str]] = [
    ("f1", "F1 score", "#00A087"),
    ("precision", "Precision", "#E64B35"),
    ("recall", "Recall", "#3C5488"),
]


# =========================
# Utilities
# =========================
def read_csv_rows(path: str | Path) -> List[Dict[str, str]]:
    with Path(path).open("r", encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def normalize_filename(name: str) -> str:
    name = str(name).strip()
    p = Path(name)
    return p.stem if p.suffix else p.name


TIMEPOINT_SUFFIX_PATTERN = re.compile(
    r"^(?P<prefix>.+?)_(?P<timepoint>\d+(?:\.\d+)?)h$",
    flags=re.IGNORECASE,
)
NATURAL_SORT_TOKEN_PATTERN = re.compile(r"(\d+)")


def infer_group_from_filename(name: str) -> str:
    """Infer the primordium/group prefix from '<prefix>_<time>h[.extension]'."""
    normalized = normalize_filename(name)
    match = TIMEPOINT_SUFFIX_PATTERN.fullmatch(normalized)
    if match is None:
        raise ValueError(
            f"Could not infer a group from fileName '{name}'. Expected a name "
            "ending in '_<time>h', for example 'sample1_18h.json'."
        )
    prefix = match.group("prefix").strip().rstrip("_-")
    if not prefix:
        raise ValueError(f"The inferred group prefix is empty for fileName '{name}'.")
    return prefix


def natural_sort_key(value: str) -> Tuple[Any, ...]:
    """Sort sample2 before sample10 while preserving general text prefixes."""
    return tuple(
        int(token) if token.isdigit() else token.casefold()
        for token in NATURAL_SORT_TOKEN_PATTERN.split(str(value))
    )


def safe_div(a: float, b: float) -> float:
    return a / b if b != 0 else 0.0


def values_from_rows(rows: Iterable[Dict[str, Any]], metric: str) -> np.ndarray:
    vals = np.array([float(r[metric]) for r in rows], dtype=float)
    return vals[np.isfinite(vals)]


# =========================
# Shared single-threshold analysis engine
# =========================
def resolve_local_input_path(path: str | Path, description: str) -> Path:
    """Resolve an absolute path or a path relative to cwd / this script."""
    requested = Path(path).expanduser()
    candidates = [requested]
    if not requested.is_absolute():
        candidates.extend(
            [
                Path.cwd() / requested,
                Path(__file__).resolve().parent / requested,
            ]
        )
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    checked = ", ".join(str(candidate) for candidate in candidates)
    raise FileNotFoundError(f"Could not find {description} '{path}'. Checked: {checked}")


def load_single_threshold_analysis_module(analysis_script: str | Path) -> Tuple[ModuleType, Path]:
    """Load the supplied single-threshold script without running its main()."""
    try:
        script_path = resolve_local_input_path(analysis_script, "analysis script")
    except FileNotFoundError:
        # Also support the common unsuffixed filename when the downloaded
        # attachment has been renamed by the user.
        if Path(analysis_script).name == DEFAULT_ANALYSIS_SCRIPT:
            script_path = resolve_local_input_path(
                "estimate_by_single_threshold.py",
                "analysis script",
            )
        else:
            raise

    module_name = f"_single_threshold_analysis_{abs(hash(str(script_path)))}"
    spec = importlib.util.spec_from_file_location(module_name, script_path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Could not load the analysis script: {script_path}")

    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    try:
        spec.loader.exec_module(module)
    except Exception:
        sys.modules.pop(module_name, None)
        raise

    required_functions = [
        "read_all_rows",
        "select_indices_by_threshold_then_max_weight_matching",
        "classify_pairs_after_matching",
        "summarize_classified_rows",
    ]
    missing = [name for name in required_functions if not callable(getattr(module, name, None))]
    if missing:
        raise AttributeError(
            f"Analysis script '{script_path}' is missing required function(s): "
            + ", ".join(missing)
        )
    return module, script_path


def compute_individual_metrics_using_analysis_script(
    analysis_script: str | Path,
    geometry_csv: str | Path,
    geometry_column: str = DEFAULT_GEOMETRY_COLUMN,
    threshold: float = DEFAULT_THRESHOLD,
    operator: str = DEFAULT_OPERATOR,
    observed_positive_value: Any = DEFAULT_OBSERVED_POSITIVE_VALUE,
    exception_column: str = DEFAULT_EXCEPTION_COLUMN,
    exception_positive_value: Any = DEFAULT_EXCEPTION_POSITIVE_VALUE,
) -> Tuple[List[Dict[str, Any]], Dict[str, Any]]:
    """Run the supplied single-threshold analysis and return per-file metrics."""
    analysis, analysis_path = load_single_threshold_analysis_module(analysis_script)
    geometry_path = resolve_local_input_path(geometry_csv, "neighbor-pair geometry CSV")

    all_data = analysis.read_all_rows(geometry_path, encoding="utf-8")
    selected_indices = analysis.select_indices_by_threshold_then_max_weight_matching(
        all_data,
        geometry_col=geometry_column,
        threshold=float(threshold),
        op=operator,
        exception_col=exception_column,
        exception_positive_value=exception_positive_value,
    )
    classified_rows, skipped = analysis.classify_pairs_after_matching(
        all_data,
        selected_idx=selected_indices,
        geometry_col=geometry_column,
        threshold=float(threshold),
        op=operator,
        observed_positive_value=observed_positive_value,
        skip_missing_observed=True,
        exception_col=exception_column,
        exception_positive_value=exception_positive_value,
        skip_exception_in_exports=True,
    )

    rows_by_file: Dict[str, List[Dict[str, Any]]] = {}
    for row in classified_rows:
        rows_by_file.setdefault(str(row.get("fileName", "")), []).append(row)

    individual_rows: List[Dict[str, Any]] = []
    for filename in sorted(rows_by_file):
        if not filename:
            continue
        metrics = analysis.summarize_classified_rows(rows_by_file[filename])
        individual_rows.append(
            {
                "scope": filename,
                "geometry_col": f"{geometry_column} (threshold + global MWM)",
                "op": operator,
                "threshold": float(threshold),
                **metrics,
                "filename_norm": normalize_filename(filename),
            }
        )

    if not individual_rows:
        raise ValueError(
            "The single-threshold analysis returned no per-snapshot rows. "
            "Check the geometry CSV and observed_division labels."
        )

    excluded_exception_rows = int(skipped.get("skipped_exception", 0))
    metadata = {
        "analysis_script": str(analysis_path),
        "geometry_csv": str(geometry_path),
        "input_rows": len(all_data),
        "excluded_exception_rows": excluded_exception_rows,
        "analysis_rows": len(all_data) - excluded_exception_rows,
        "selected_pairs": len(selected_indices),
        "evaluated_pairs": len(classified_rows),
        "skipped_missing_observed": int(skipped.get("skipped_missing_observed", 0)),
        "n_individual_files": len(individual_rows),
    }
    return individual_rows, metadata


# =========================
# Legacy self-contained geometry helpers
# =========================
def resolve_column_name(
    rows: Sequence[Dict[str, Any]],
    wanted: str,
    *,
    required: bool = True,
) -> str | None:
    """Resolve a CSV column name case-insensitively."""
    if not rows:
        if required:
            raise ValueError("The input CSV has no data rows.")
        return None

    columns = list(rows[0].keys())
    if wanted in columns:
        return wanted
    wanted_folded = str(wanted).casefold()
    for column in columns:
        if str(column).casefold() == wanted_folded:
            return str(column)

    if required:
        raise ValueError(f"Column '{wanted}' not found. Available columns: {columns}")
    return None


def value_matches_positive(raw: Any, positive_value: Any) -> bool:
    """Compare numeric labels numerically and other labels case-insensitively."""
    if raw is None or str(raw).strip() == "":
        return False
    try:
        return float(raw) == float(positive_value)
    except (TypeError, ValueError):
        return str(raw).strip().casefold() == str(positive_value).strip().casefold()


def parse_observed_label(raw: Any, positive_value: Any) -> bool | None:
    """Return True/False for a numeric observed label, or None when missing/invalid."""
    if raw is None or str(raw).strip() == "":
        return None
    try:
        value = float(raw)
        positive = float(positive_value)
    except (TypeError, ValueError):
        raw_text = str(raw).strip().casefold()
        positive_text = str(positive_value).strip().casefold()
        if raw_text == positive_text:
            return True
        if raw_text in {"0", "false", "no", "non-daughter", "non_daughter"}:
            return False
        return None
    if not np.isfinite(value):
        return None
    return value == positive


def threshold_operators() -> Dict[str, Callable[[float, float], bool]]:
    return {
        ">": lambda value, threshold: value > threshold,
        ">=": lambda value, threshold: value >= threshold,
        "<": lambda value, threshold: value < threshold,
        "<=": lambda value, threshold: value <= threshold,
    }


def metrics_from_counts(tp: int, fp: int, tn: int, fn: int) -> Dict[str, Any]:
    precision = safe_div(tp, tp + fp)
    recall = safe_div(tp, tp + fn)
    f1 = safe_div(2 * precision * recall, precision + recall)
    accuracy = safe_div(tp + tn, tp + fp + tn + fn)
    return {
        "TP": int(tp),
        "FP": int(fp),
        "TN": int(tn),
        "FN": int(fn),
        "precision": precision,
        "recall": recall,
        "f1": f1,
        "accuracy": accuracy,
        "support_pos": int(tp + fn),
        "support_neg": int(tn + fp),
        "evaluated_pairs": int(tp + fp + tn + fn),
    }


def solve_global_maximum_weight_matching(
    file_edges: Dict[Tuple[int, int], Tuple[float, int]],
) -> Tuple[Set[Tuple[int, int]], str]:
    """Solve exact maximum-weight matching with NetworkX or an exact MILP fallback."""
    positive_edges = {
        edge: value for edge, value in file_edges.items() if float(value[0]) > 0.0
    }
    if not positive_edges:
        return set(), "none"

    try:
        import networkx as nx

        graph = nx.Graph()
        for (polygon_a, polygon_b), (margin, row_index) in positive_edges.items():
            graph.add_edge(
                polygon_a,
                polygon_b,
                weight=float(margin),
                row_index=row_index,
            )
        matching = nx.algorithms.matching.max_weight_matching(
            graph,
            maxcardinality=False,
            weight="weight",
        )
        normalized = {
            (a, b) if a < b else (b, a)
            for a, b in matching
        }
        return normalized, "networkx"
    except ImportError:
        pass

    try:
        from scipy.optimize import Bounds, LinearConstraint, milp
    except ImportError as exc:
        raise ImportError(
            "Exact global maximum-weight matching requires either networkx or "
            "SciPy with scipy.optimize.milp. Install one of them; the script "
            "will not silently fall back to greedy matching."
        ) from exc

    edges = list(positive_edges)
    vertices = sorted({vertex for edge in edges for vertex in edge})
    vertex_to_row = {vertex: i for i, vertex in enumerate(vertices)}
    incidence = np.zeros((len(vertices), len(edges)), dtype=float)
    weights = np.empty(len(edges), dtype=float)
    for edge_index, (polygon_a, polygon_b) in enumerate(edges):
        incidence[vertex_to_row[polygon_a], edge_index] = 1.0
        incidence[vertex_to_row[polygon_b], edge_index] = 1.0
        weights[edge_index] = float(positive_edges[(polygon_a, polygon_b)][0])

    result = milp(
        c=-weights,
        integrality=np.ones(len(edges), dtype=int),
        bounds=Bounds(np.zeros(len(edges)), np.ones(len(edges))),
        constraints=LinearConstraint(
            incidence,
            np.zeros(len(vertices)),
            np.ones(len(vertices)),
        ),
        options={"presolve": True},
    )
    if not result.success or result.x is None:
        raise RuntimeError(
            "Exact SciPy MILP matching failed: "
            f"{getattr(result, 'message', 'unknown solver error')}"
        )

    selected = {
        edge for edge, chosen in zip(edges, result.x) if float(chosen) > 0.5
    }
    return selected, "scipy_milp"


def compute_individual_metrics_from_geometry(
    geometry_csv: str | Path,
    geometry_column: str = DEFAULT_GEOMETRY_COLUMN,
    threshold: float = DEFAULT_THRESHOLD,
    operator: str = DEFAULT_OPERATOR,
    file_column: str = DEFAULT_FILE_COLUMN,
    first_polygon_column: str = DEFAULT_FIRST_POLYGON_COLUMN,
    second_polygon_column: str = DEFAULT_SECOND_POLYGON_COLUMN,
    observed_column: str = DEFAULT_OBSERVED_COLUMN,
    observed_positive_value: Any = DEFAULT_OBSERVED_POSITIVE_VALUE,
    exception_column: str = DEFAULT_EXCEPTION_COLUMN,
    exception_positive_value: Any = DEFAULT_EXCEPTION_POSITIVE_VALUE,
) -> Tuple[List[Dict[str, Any]], Dict[str, Any]]:
    """
    Calculate per-snapshot performance directly from neighbor-pair geometry.

    Processing order:
      1. remove exception-labelled rows;
      2. apply the configured threshold within each snapshot;
      3. run exact global maximum-weight matching within each snapshot;
      4. compare selected pairs with observed_division labels.
    """
    operators = threshold_operators()
    if operator not in operators:
        raise ValueError(f"Unsupported operator '{operator}'. Use one of {list(operators)}")

    raw_rows = read_csv_rows(geometry_csv)
    file_col = resolve_column_name(raw_rows, file_column)
    first_col = resolve_column_name(raw_rows, first_polygon_column)
    second_col = resolve_column_name(raw_rows, second_polygon_column)
    geometry_col = resolve_column_name(raw_rows, geometry_column)
    observed_col = resolve_column_name(raw_rows, observed_column)
    exception_col = resolve_column_name(raw_rows, exception_column, required=False)

    if exception_col is None:
        filtered_rows = list(raw_rows)
        excluded_exception_count = 0
        print(
            f"Exception column '{exception_column}' was not found; "
            "all geometry rows will be analyzed for backward compatibility."
        )
    else:
        exception_mask = [
            value_matches_positive(row.get(exception_col), exception_positive_value)
            for row in raw_rows
        ]
        excluded_exception_count = int(sum(exception_mask))
        filtered_rows = [
            row for row, is_exception in zip(raw_rows, exception_mask) if not is_exception
        ]

    threshold_float = float(threshold)
    per_file_best_edges: Dict[str, Dict[Tuple[int, int], Tuple[float, int]]] = {}
    skipped_invalid_candidate_rows = 0

    for row_index, row in enumerate(filtered_rows):
        filename = str(row.get(file_col, "")).strip()
        try:
            polygon_a = int(float(row.get(first_col, "")))
            polygon_b = int(float(row.get(second_col, "")))
            geometry_value = float(row.get(geometry_col, ""))
        except (TypeError, ValueError):
            skipped_invalid_candidate_rows += 1
            continue

        if not filename or polygon_a == polygon_b or not np.isfinite(geometry_value):
            skipped_invalid_candidate_rows += 1
            continue
        if not operators[operator](geometry_value, threshold_float):
            continue

        margin = (
            geometry_value - threshold_float
            if operator in (">", ">=")
            else threshold_float - geometry_value
        )
        if margin < 0:
            continue

        edge = (
            (polygon_a, polygon_b)
            if polygon_a < polygon_b
            else (polygon_b, polygon_a)
        )
        file_edges = per_file_best_edges.setdefault(filename, {})
        previous = file_edges.get(edge)
        if previous is None or margin > previous[0]:
            file_edges[edge] = (float(margin), row_index)

    selected_indices: Set[int] = set()
    matching_solvers: Set[str] = set()
    for file_edges in per_file_best_edges.values():
        matching, matching_solver = solve_global_maximum_weight_matching(file_edges)
        matching_solvers.add(matching_solver)
        for edge in matching:
            selected_indices.add(file_edges[edge][1])

    counts_by_file: Dict[str, Dict[str, int]] = {}
    skipped_missing_observed = 0
    for row_index, row in enumerate(filtered_rows):
        filename = str(row.get(file_col, "")).strip()
        observed = parse_observed_label(
            row.get(observed_col),
            observed_positive_value,
        )
        if not filename or observed is None:
            skipped_missing_observed += 1
            continue

        predicted = row_index in selected_indices
        if predicted and observed:
            outcome = "TP"
        elif predicted and not observed:
            outcome = "FP"
        elif (not predicted) and observed:
            outcome = "FN"
        else:
            outcome = "TN"

        file_counts = counts_by_file.setdefault(
            filename,
            {"TP": 0, "FP": 0, "TN": 0, "FN": 0},
        )
        file_counts[outcome] += 1

    individual_rows: List[Dict[str, Any]] = []
    for filename in sorted(counts_by_file):
        counts = counts_by_file[filename]
        metrics = metrics_from_counts(
            counts["TP"],
            counts["FP"],
            counts["TN"],
            counts["FN"],
        )
        individual_rows.append(
            {
                "scope": filename,
                "geometry_col": f"{geometry_col} (threshold + global MWM)",
                "op": operator,
                "threshold": threshold_float,
                **metrics,
                "filename_norm": normalize_filename(filename),
            }
        )

    if not individual_rows:
        raise ValueError(
            "No per-snapshot performance could be calculated. Check that "
            f"'{observed_col}' contains valid labels and '{file_col}' contains filenames."
        )

    metadata = {
        "input_rows": len(raw_rows),
        "excluded_exception_rows": excluded_exception_count,
        "analysis_rows": len(filtered_rows),
        "selected_pairs": len(selected_indices),
        "skipped_missing_observed": skipped_missing_observed,
        "skipped_invalid_candidate_rows": skipped_invalid_candidate_rows,
        "n_individual_files": len(individual_rows),
        "exception_column_found": exception_col is not None,
        "matching_solver": ";".join(sorted(matching_solvers)),
    }
    return individual_rows, metadata


# =========================
# Filename-prefix grouping
# =========================
def compute_group_and_individual_metrics(
    performance_csv: str | Path,
) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
    performance_rows_raw = [
        r for r in read_csv_rows(performance_csv)
        if r.get("scope") != "__OVERALL__"
    ]

    individual_rows: List[Dict[str, Any]] = []
    for r in performance_rows_raw:
        rr: Dict[str, Any] = dict(r)
        for k in ("TP", "FP", "TN", "FN"):
            rr[k] = int(float(rr[k]))
        for k in ("precision", "recall", "f1", "accuracy"):
            rr[k] = float(rr[k])
        rr["filename_norm"] = normalize_filename(rr["scope"])
        individual_rows.append(rr)

    group_rows = compute_group_metrics(individual_rows)
    return group_rows, individual_rows


def compute_group_metrics(
    individual_rows: Sequence[Dict[str, Any]],
) -> List[Dict[str, Any]]:
    """Pool individual counts by the group prefix inferred from each fileName."""
    rows_by_group: Dict[str, List[Dict[str, Any]]] = {}
    for row in individual_rows:
        filename = str(row.get("filename_norm") or row.get("scope", "")).strip()
        if not filename:
            raise ValueError("An individual performance row has an empty fileName/scope.")
        group_name = infer_group_from_filename(filename)
        rows_by_group.setdefault(group_name, []).append(dict(row))

    group_rows: List[Dict[str, Any]] = []
    for group_name in sorted(rows_by_group, key=natural_sort_key):
        rows = rows_by_group[group_name]
        tp = fp = tn = fn = 0
        used: List[str] = []
        for row in sorted(rows, key=lambda item: natural_sort_key(str(item["filename_norm"]))):
            used.append(str(row["filename_norm"]))
            tp += int(row["TP"])
            fp += int(row["FP"])
            tn += int(row["TN"])
            fn += int(row["FN"])

        metrics = metrics_from_counts(tp, fp, tn, fn)

        group_rows.append(
            {
                "group": group_name,
                "n_files": len(used),
                **metrics,
                "used_files": ";".join(used),
            }
        )

    if not group_rows:
        raise ValueError("No filename-prefix groups could be inferred.")
    return group_rows


# =========================
# Table export
# =========================
def export_metrics_tables(
    group_rows: List[Dict[str, Any]],
    individual_rows: List[Dict[str, Any]],
    out_dir: str | Path,
    basename: str,
) -> Tuple[Path, Path, Path, Path]:
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    individual_metrics_csv = out_dir / f"{basename}_individual_metrics.csv"
    with individual_metrics_csv.open("w", encoding="utf-8", newline="") as f:
        fieldnames = [
            "scope", "geometry_col", "op", "threshold",
            "TP", "FP", "TN", "FN",
            "precision", "recall", "f1", "accuracy",
            "support_pos", "support_neg", "evaluated_pairs",
        ]
        w = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        w.writeheader()
        w.writerows(individual_rows)

    group_metrics_csv = out_dir / f"{basename}_group_metrics.csv"
    with group_metrics_csv.open("w", encoding="utf-8", newline="") as f:
        fieldnames = [
            "group", "n_files",
            "TP", "FP", "TN", "FN",
            "precision", "recall", "f1", "accuracy",
            "support_pos", "support_neg", "evaluated_pairs",
            "used_files",
        ]
        w = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        w.writeheader()
        w.writerows(group_rows)

    values_long_csv = out_dir / f"{basename}_values_long.csv"
    with values_long_csv.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["level", "unit", "metric", "value"])
        w.writeheader()
        for row in individual_rows:
            for metric, _, _ in METRICS:
                w.writerow({"level": "individual", "unit": row["scope"], "metric": metric, "value": row[metric]})
        for row in group_rows:
            for metric, _, _ in METRICS:
                w.writerow({"level": "group", "unit": row["group"], "metric": metric, "value": row[metric]})

    summary_csv = out_dir / f"{basename}_summary.csv"
    with summary_csv.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["level", "metric", "n", "mean", "SD"])
        w.writeheader()
        for level, rows in [("individual", individual_rows), ("group", group_rows)]:
            for metric, _, _ in METRICS:
                vals = values_from_rows(rows, metric)
                w.writerow(
                    {
                        "level": level,
                        "metric": metric,
                        "n": int(vals.size),
                        "mean": float(np.mean(vals)),
                        "SD": float(np.std(vals, ddof=1)) if vals.size > 1 else 0.0,
                    }
                )

    return individual_metrics_csv, group_metrics_csv, values_long_csv, summary_csv


# =========================
# Plotting
# =========================
def draw_box_strip(
    ax,
    vals: np.ndarray,
    x: float,
    color: str,
    rng: np.random.Generator,
    width: float = 0.48,
    score_y_axes: float = -0.24,
) -> Tuple[float, float]:
    mean = float(np.mean(vals))
    sd = float(np.std(vals, ddof=1)) if len(vals) > 1 else 0.0

    ax.boxplot(
        [vals],
        positions=[x],
        widths=width,
        patch_artist=True,
        showfliers=False,
        boxprops={"facecolor": color, "alpha": 0.18, "edgecolor": color, "linewidth": 1.8},
        medianprops={"color": "black", "linewidth": 2.0},
        whiskerprops={"color": color, "linewidth": 1.6},
        capprops={"color": color, "linewidth": 1.6},
    )

    jitter = rng.uniform(-0.11, 0.11, size=len(vals))
    ax.scatter(
        np.full(len(vals), x) + jitter,
        vals,
        s=42,
        facecolor=color,
        edgecolor="black",
        linewidth=0.75,
        alpha=0.88,
        zorder=3,
    )

    ax.scatter(
        [x], [mean],
        marker="D",
        s=68,
        facecolor="white",
        edgecolor="black",
        linewidth=1.0,
        zorder=4,
    )

    ax.text(
        x,
        score_y_axes,
        rf"${mean * 100:.1f}\ \pm\ {sd * 100:.1f}$",
        transform=ax.get_xaxis_transform(),
        ha="center",
        va="top",
        fontsize=9.0,
        clip_on=False,
    )
    return mean, sd


def style_axis(ax, ylabel: str = "") -> None:
    ax.set_ylim(0.0, 1.08)
    yticks = np.linspace(0, 1, 6)
    ax.set_yticks(yticks)
    ax.set_yticklabels([f"{v * 100:.0f}" for v in yticks], fontsize=11.5)
    ax.set_axisbelow(True)
    ax.grid(axis="y", linestyle=(0, (2, 2)), linewidth=0.9, color="#D9D9D9")

    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_linewidth(1.8)
        ax.spines[side].set_color("black")

    ax.tick_params(axis="x", width=1.6, length=0, pad=8)
    ax.tick_params(axis="y", width=1.6, length=5)
    ax.set_ylabel(ylabel, fontsize=15)


def plot_group_vs_individual_AB(
    group_rows: List[Dict[str, Any]],
    individual_rows: List[Dict[str, Any]],
    out_dir: str | Path,
    basename: str,
    dataset_label: str = DEFAULT_DATASET_LABEL,
    dpi: int = DEFAULT_DPI,
    random_seed: int = DEFAULT_RANDOM_SEED,
) -> Dict[str, Path]:
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    plt.rcParams.update(
        {
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "savefig.facecolor": "white",
            "font.size": 11,
            "font.family": "sans-serif",
        }
    )

    rng = np.random.default_rng(random_seed)
    # A taller canvas gives the plot and its bottom score labels more vertical
    # breathing room while retaining the original horizontal proportions.
    fig, (axA, axB) = plt.subplots(1, 2, figsize=(8.8, 5.8), sharey=True)

    x_positions = np.array([1, 2, 3], dtype=float)

    # Panel A: individual
    for x, (_, label, color) in zip(x_positions, METRICS):
        metric = METRICS[list(x_positions).index(x)][0]
        vals = values_from_rows(individual_rows, metric)
        draw_box_strip(axA, vals=vals, x=x, color=color, rng=rng)
    axA.set_xlim(0.35, 3.65)
    axA.set_xticks(x_positions)
    tick_objs_A = axA.set_xticklabels([label for _, label, _ in METRICS], fontsize=11.6)
    for tick, (_, _, color) in zip(tick_objs_A, METRICS):
        tick.set_color(color)
    axA.text(0.5, 1.06, f"{dataset_label}, individual\n(n = {len(individual_rows)})", transform=axA.transAxes,
             ha="center", va="top", fontsize=12.5)
    axA.text(-0.22, 1.03, "A", transform=axA.transAxes, fontsize=16, fontweight="bold", va="top")
    style_axis(axA, ylabel="Score (%)")

    # Panel B: grouped
    for x, (_, label, color) in zip(x_positions, METRICS):
        metric = METRICS[list(x_positions).index(x)][0]
        vals = values_from_rows(group_rows, metric)
        draw_box_strip(axB, vals=vals, x=x, color=color, rng=rng)
    axB.set_xlim(0.35, 3.65)
    axB.set_xticks(x_positions)
    tick_objs_B = axB.set_xticklabels([label for _, label, _ in METRICS], fontsize=11.6)
    for tick, (_, _, color) in zip(tick_objs_B, METRICS):
        tick.set_color(color)
    axB.text(0.5, 1.06, f"{dataset_label}, grouped\n(n = {len(group_rows)})", transform=axB.transAxes,
             ha="center", va="top", fontsize=12.5)
    axB.text(-0.14, 1.03, "B", transform=axB.transAxes, fontsize=16, fontweight="bold", va="top")
    style_axis(axB, ylabel="")
    plt.setp(axB.get_yticklabels(), visible=False)
    axB.tick_params(axis="y", length=0)

    # Reserve space for the mean ± SD scores drawn below the metric labels.
    fig.tight_layout(w_pad=2.0, rect=(0.0, 0.12, 1.0, 1.0))

    out = out_dir / f"{basename}.png"
    fig.savefig(out, dpi=dpi, bbox_inches="tight")
    plt.close(fig)
    return {"png": out}


# =========================
# Main
# =========================
def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Generate the grouped-vs-individual performance plot directly from "
            "neighbor-pair geometry (recommended) or from an existing performance CSV."
        )
    )
    input_group = parser.add_mutually_exclusive_group()
    input_group.add_argument(
        "--geometry-csv",
        default=DEFAULT_INPUT_GEOMETRY_CSV,
        help=(
            "Neighbor-pair geometry CSV. Exception-labelled rows are removed "
            "before thresholding and global matching (default mode)."
        ),
    )
    input_group.add_argument(
        "--performance-csv",
        default=DEFAULT_INPUT_PERFORMANCE_CSV,
        help="Previously summarized per-file performance CSV (legacy mode).",
    )
    parser.add_argument(
        "--analysis-script",
        default=DEFAULT_ANALYSIS_SCRIPT,
        help=(
            "Path to estimate_by_single_threshold.py, used as the canonical "
            "geometry-analysis engine."
        ),
    )
    parser.add_argument("--geometry-column", default=DEFAULT_GEOMETRY_COLUMN)
    parser.add_argument("--threshold", type=float, default=DEFAULT_THRESHOLD)
    parser.add_argument(
        "--operator",
        choices=[">", ">=", "<", "<="],
        default=DEFAULT_OPERATOR,
    )
    parser.add_argument("--observed-positive-value", default=DEFAULT_OBSERVED_POSITIVE_VALUE)
    parser.add_argument("--exception-column", default=DEFAULT_EXCEPTION_COLUMN)
    parser.add_argument("--exception-positive-value", default=DEFAULT_EXCEPTION_POSITIVE_VALUE)
    parser.add_argument("--output-dir", default=DEFAULT_OUTPUT_DIR, help="Dedicated output directory.")
    parser.add_argument("--basename", default=DEFAULT_BASENAME, help="Base filename for exported outputs.")
    parser.add_argument("--dataset-label", default=DEFAULT_DATASET_LABEL, help="Dataset label shown in the figure.")
    parser.add_argument("--dpi", type=int, default=DEFAULT_DPI, help="Output DPI for raster export.")
    parser.add_argument("--random-seed", type=int, default=DEFAULT_RANDOM_SEED, help="Random seed for jitter.")
    return parser


def main() -> None:
    args = build_arg_parser().parse_args()

    geometry_metadata: Dict[str, Any] | None = None
    if args.performance_csv is not None:
        group_rows, individual_rows = compute_group_and_individual_metrics(
            args.performance_csv,
        )
        print(f"Input mode: summarized performance CSV ({args.performance_csv})")
    else:
        individual_rows, geometry_metadata = compute_individual_metrics_using_analysis_script(
            analysis_script=args.analysis_script,
            geometry_csv=args.geometry_csv,
            geometry_column=args.geometry_column,
            threshold=args.threshold,
            operator=args.operator,
            observed_positive_value=args.observed_positive_value,
            exception_column=args.exception_column,
            exception_positive_value=args.exception_positive_value,
        )
        group_rows = compute_group_metrics(individual_rows)
        print(f"Input mode: neighbor-pair geometry CSV ({args.geometry_csv})")
        print(f"Analysis engine: {geometry_metadata['analysis_script']}")
        print(
            "Exception filtering: "
            f"excluded {geometry_metadata['excluded_exception_rows']} of "
            f"{geometry_metadata['input_rows']} rows before global matching; "
            f"{geometry_metadata['analysis_rows']} rows remained."
        )
        print(
            f"Selected pairs: {geometry_metadata['selected_pairs']}; "
            f"individual snapshots: {geometry_metadata['n_individual_files']}."
        )
    print(f"Filename-prefix groups inferred: {len(group_rows)}.")

    individual_metrics_csv, group_metrics_csv, values_long_csv, summary_csv = export_metrics_tables(
        group_rows,
        individual_rows,
        args.output_dir,
        args.basename,
    )

    outputs = plot_group_vs_individual_AB(
        group_rows,
        individual_rows,
        args.output_dir,
        args.basename,
        dataset_label=args.dataset_label,
        dpi=args.dpi,
        random_seed=args.random_seed,
    )

    print("Generated figure files:")
    print(outputs["png"])
    print("Generated table files:")
    print(individual_metrics_csv)
    print(group_metrics_csv)
    print(values_long_csv)
    print(summary_csv)


if __name__ == "__main__":
    main()
