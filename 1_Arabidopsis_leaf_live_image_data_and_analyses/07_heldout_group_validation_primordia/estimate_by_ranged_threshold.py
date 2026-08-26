from __future__ import annotations

import csv
from dataclasses import dataclass
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path
from typing import Any, Callable, Dict, List, Set, Tuple


# =========================
# Parameters to edit
# =========================
# Edit only this section when changing input files, feature column,
# threshold range, or output name. No command-line parser/arguments are used.

SCRIPT_DIR = Path(__file__).resolve().parent

INPUT_CSV = SCRIPT_DIR / "batch_neighbor_pair_geometry.csv"
RESULT_DIR = SCRIPT_DIR / "result"

GEOMETRY_COLUMN = "junctionAngleAverageDegrees"  # case-insensitive lookup works
OPERATOR = ">="
EXCEPTION_COLUMN = "exception_label"  # read directly from INPUT_CSV
EXCEPTION_POSITIVE_VALUE = 1

# Threshold sweep settings. The max value is included when it falls on the step.
THRESHOLD_MIN = 120.0
THRESHOLD_MAX = 170.0
THRESHOLD_STEP = 0.1

OUTPUT_THRESHOLD_SUMMARY_CSV = RESULT_DIR / "threshold_performance_summary.csv"
OUTPUT_PRECISION_RECALL_FIGURE = RESULT_DIR / "precision_recall_vs_threshold.png"

# Publication-style precision/recall plot settings.
# The style is intentionally close to clean line figures commonly seen in
# Nature/Science papers: minimal axes, color-blind-friendly lines, restrained
# grid, sparse markers, and all legends outside the plotting area.
PLOT_DPI = 600  # 300 journal | 600 very high
PLOT_ASPECT_RATIO = 1.45  # plotting panel width / height, excluding right-side legend
PLOT_PANEL_WIDTH = 5.2      # width of the plotting panel only, excluding legend
PLOT_LEGEND_WIDTH = 1.65    # reserved canvas width for the right-side legend
PLOT_COLOR_PRECISION = "#6A3D9A"  # muted purple
PLOT_COLOR_RECALL = "#1B9E77"     # muted teal
PLOT_COLOR_BEST_F1 = "#6E6E6E"    # neutral gray
PLOT_LEGEND_ANCHOR_X = 1.03       # right-side legend anchor; increase to 1.08 if needed
PLOT_X_PADDING_FRACTION = 0.025  # prevents edge markers from being clipped at x-axis ends
PLOT_MARKER_EVERY_N_POINTS = 5   # with 0.1 step, 20 means one marker every 2 degrees
PLOT_SHOW_BEST_F1_LINE = True
PLOT_SHOW_TITLE = False           # Nature/Science-style panels usually omit title
PLOT_X_LABEL = "Junction-angle threshold (°)"
PLOT_Y_LABEL = "Score"
PLOT_SHOW = False  # set True if you want an interactive window after saving

OBSERVED_POSITIVE_VALUE = 1
SKIP_MISSING_OBSERVED = True
SKIP_EXCEPTION_IN_EXPORTS = True
ENCODING = "utf-8"

# Print one short progress line per threshold.
PRINT_PROGRESS = True
PRINT_PROGRESS_EVERY = 1  # with 0.1 step, print once per 1.0 threshold unit

# Keep exact maximum-weight matching behavior. This needs networkx.
# This fast version precomputes per-file candidate edges and reusable evaluation labels.


@dataclass
class FullRow:
    data: Dict[str, Any]


@dataclass(frozen=True)
class EdgeCandidate:
    """One reusable graph edge candidate after duplicate-edge reduction."""

    file_name: str
    u: int
    v: int
    value: float
    row_idx: int


@dataclass(frozen=True)
class EvaluationCache:
    """Reusable labels and row sets for very fast TP/FP/FN/TN counting."""

    evaluated_indices: Set[int]
    positive_indices: Set[int]
    negative_indices: Set[int]
    total_pos: int
    total_neg: int
    evaluated_pairs: int
    skipped_missing_observed: int
    skipped_exception: int


@dataclass(frozen=True)
class PrecomputedThresholdData:
    """All reusable data needed for threshold sweep."""

    per_file_edges: Dict[str, List[EdgeCandidate]]
    eval_cache: EvaluationCache


def _to_number_if_possible(s: str) -> Any:
    if s is None:
        return None
    s = s.strip()
    if s == "":
        return ""
    try:
        if "." not in s and "e" not in s.lower():
            return int(s)
    except ValueError:
        pass
    try:
        return float(s)
    except ValueError:
        return s


def read_all_rows(input_csv: str | Path, encoding: str = "utf-8") -> List[FullRow]:
    input_csv = Path(input_csv)
    out: List[FullRow] = []
    with input_csv.open("r", encoding=encoding, newline="") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames:
            raise ValueError("No header found in CSV.")
        for row in reader:
            out.append(FullRow({k: _to_number_if_possible(v) for k, v in row.items()}))
    return out


def _resolve_col_name(container: List[FullRow], wanted: str) -> str:
    """Resolve column name robustly with case-insensitive lookup."""
    if not container:
        raise ValueError("Empty container.")
    if wanted in container[0].data:
        return wanted
    wanted_l = wanted.lower()
    for k in container[0].data.keys():
        if k.lower() == wanted_l:
            return k
    raise ValueError(
        f"Column '{wanted}' not found. Available columns include: {list(container[0].data.keys())}"
    )


def _ops() -> Dict[str, Callable[[float, float], bool]]:
    return {
        ">": lambda a, b: a > b,
        ">=": lambda a, b: a >= b,
        "<": lambda a, b: a < b,
        "<=": lambda a, b: a <= b,
        "==": lambda a, b: a == b,
        "!=": lambda a, b: a != b,
    }


# =========================
# Exception handling
# =========================
def _get_exception_bool(
    r: FullRow,
    exception_col: str,
    exception_positive_value: int = 1,
) -> bool:
    """Return True when this row is labelled as an exception in the input CSV."""
    raw = r.data.get(exception_col, None)
    if raw in (None, ""):
        return False
    try:
        return float(raw) == float(exception_positive_value)
    except (TypeError, ValueError):
        return str(raw).strip().casefold() == str(exception_positive_value).strip().casefold()


# =========================
# Threshold values
# =========================
def _decimal_places(x: float | int | str) -> int:
    d = Decimal(str(x)).normalize()
    return max(0, -d.as_tuple().exponent)


def generate_threshold_values(thr_min: float, thr_max: float, step: float) -> List[float]:
    """Generate inclusive threshold values while avoiding floating-point drift."""
    if step <= 0:
        raise ValueError("THRESHOLD_STEP must be > 0.")
    if thr_max < thr_min:
        raise ValueError("THRESHOLD_MAX must be >= THRESHOLD_MIN.")

    d_min = Decimal(str(thr_min))
    d_max = Decimal(str(thr_max))
    d_step = Decimal(str(step))
    places = max(_decimal_places(thr_min), _decimal_places(thr_max), _decimal_places(step))
    quant = Decimal("1") if places == 0 else Decimal("1").scaleb(-places)

    values: List[float] = []
    x = d_min
    while x <= d_max + Decimal("1e-12"):
        values.append(float(x.quantize(quant, rounding=ROUND_HALF_UP)))
        x += d_step
    return values


# ==========================================================
# Precomputation
# ==========================================================
def precompute_threshold_data(
    container: List[FullRow],
    geometry_col: str,
    op: str,
    exception_col: str = "exception_label",
    exception_positive_value: int = 1,
    observed_positive_value: int = 1,
    skip_missing_observed: bool = True,
    skip_exception_in_exports: bool = True,
) -> PrecomputedThresholdData:
    """
    Precompute the reusable sparse adjacency edge list and evaluation labels.

    Why this is faster than the original threshold-range version:
      1. Column names are resolved once.
      2. Every CSV row is parsed once.
      3. Exception filtering is checked once.
      4. observed_division labels are converted once.
      5. Duplicate candidate edges are reduced once per file/pair.

    Matching is still recalculated at every threshold because the eligible graph
    and edge weights change with threshold.
    """
    if not container:
        raise ValueError("Container is empty; nothing to precompute.")

    if op not in (">", ">=", "<", "<="):
        raise ValueError("Margin scoring is defined only for: >, >=, <, <=")

    file_col = _resolve_col_name(container, "fileName")
    a_col = _resolve_col_name(container, "firstPolygonId")
    b_col = _resolve_col_name(container, "secondPolygonId")
    obs_col = _resolve_col_name(container, "observed_division")
    geo_col = _resolve_col_name(container, geometry_col)
    exc_col = _resolve_col_name(container, exception_col)

    # For duplicate edges in the same file, keep the row that always gives the
    # largest margin: highest value for above-threshold operators, lowest value
    # for below-threshold operators.
    per_file_best: Dict[str, Dict[Tuple[int, int], EdgeCandidate]] = {}

    evaluated_indices: Set[int] = set()
    positive_indices: Set[int] = set()
    negative_indices: Set[int] = set()
    skipped_missing_observed = 0
    skipped_exception = 0

    keep_higher_value = op in (">", ">=")

    for row_idx, r in enumerate(container):
        # Match the Qt C++ order of operations: discard exceptions before
        # constructing the reusable candidate-edge cache. Consequently, an
        # exception can never occupy a polygon vertex in any threshold-specific
        # global maximum-weight matching graph.
        is_exception = _get_exception_bool(
            r,
            exception_col=exc_col,
            exception_positive_value=exception_positive_value,
        )

        fn = r.data.get(file_col, None)
        a = r.data.get(a_col, None)
        b = r.data.get(b_col, None)
        val_raw = r.data.get(geo_col, None)

        if (
            not is_exception
            and fn not in (None, "")
            and a not in (None, "")
            and b not in (None, "")
            and val_raw not in (None, "")
        ):
            try:
                u = int(a)
                v = int(b)
                value = float(val_raw)
            except (TypeError, ValueError):
                u = v = 0
                value = float("nan")
            if u != v and value == value:  # value == value filters out NaN
                x, y = (u, v) if u < v else (v, u)
                file_key = str(fn)
                candidate = EdgeCandidate(file_name=file_key, u=x, v=y, value=value, row_idx=row_idx)

                bucket = per_file_best.setdefault(file_key, {})
                old = bucket.get((x, y))
                if old is None:
                    bucket[(x, y)] = candidate
                elif keep_higher_value and candidate.value > old.value:
                    bucket[(x, y)] = candidate
                elif (not keep_higher_value) and candidate.value < old.value:
                    bucket[(x, y)] = candidate

        if skip_exception_in_exports and is_exception:
            skipped_exception += 1
            continue

        obs_raw = r.data.get(obs_col, None)
        if obs_raw in (None, ""):
            if skip_missing_observed:
                skipped_missing_observed += 1
                continue
            obs_bool = False
        else:
            try:
                obs_bool = int(obs_raw) == int(observed_positive_value)
            except (TypeError, ValueError):
                if skip_missing_observed:
                    skipped_missing_observed += 1
                    continue
                obs_bool = False

        evaluated_indices.add(row_idx)
        if obs_bool:
            positive_indices.add(row_idx)
        else:
            negative_indices.add(row_idx)

    per_file_edges: Dict[str, List[EdgeCandidate]] = {
        file_name: list(edge_dict.values()) for file_name, edge_dict in per_file_best.items()
    }

    # Sort edges so threshold filtering is deterministic and slightly cache-friendly.
    for file_name in per_file_edges:
        per_file_edges[file_name].sort(key=lambda e: e.value, reverse=keep_higher_value)

    eval_cache = EvaluationCache(
        evaluated_indices=evaluated_indices,
        positive_indices=positive_indices,
        negative_indices=negative_indices,
        total_pos=len(positive_indices),
        total_neg=len(negative_indices),
        evaluated_pairs=len(evaluated_indices),
        skipped_missing_observed=skipped_missing_observed,
        skipped_exception=skipped_exception,
    )

    return PrecomputedThresholdData(per_file_edges=per_file_edges, eval_cache=eval_cache)


def _edge_passes_threshold(value: float, threshold: float, op: str) -> bool:
    if op == ">":
        return value > threshold
    if op == ">=":
        return value >= threshold
    if op == "<":
        return value < threshold
    if op == "<=":
        return value <= threshold
    raise ValueError(f"Unsupported op '{op}'. Use one of >, >=, <, <=")


def _edge_weight(value: float, threshold: float, op: str) -> float:
    return (value - threshold) if op in (">", ">=") else (threshold - value)


# ==========================================================
# Threshold + maximum-weight matching from precomputed edges
# ==========================================================
def select_indices_from_precomputed_edges(
    per_file_edges: Dict[str, List[EdgeCandidate]],
    threshold: float,
    op: str,
) -> Set[int]:
    """Run exact maximum-weight matching using the precomputed sparse edge lists."""
    try:
        import networkx as nx
    except ImportError as e:
        raise ImportError(
            "This step needs networkx for exact maximum-weight matching.\n"
            "Install it with: pip install networkx"
        ) from e

    selected: Set[int] = set()
    thr = float(threshold)

    for edges in per_file_edges.values():
        graph = nx.Graph()
        edge_to_row: Dict[Tuple[int, int], int] = {}

        for edge in edges:
            if not _edge_passes_threshold(edge.value, thr, op):
                # Edges are sorted by value. For >=/> we can stop after first fail.
                if op in (">", ">="):
                    break
                # For <=/<, sorted low to high, first failures may still be followed by failures only.
                # Because precompute sorting uses ascending for below-threshold operators, we can also stop.
                if op in ("<", "<=") and edge.value >= thr:
                    break
                continue

            weight = _edge_weight(edge.value, thr, op)
            if weight < 0:
                continue

            graph.add_edge(edge.u, edge.v, weight=weight)
            edge_to_row[(edge.u, edge.v)] = edge.row_idx

        if graph.number_of_edges() == 0:
            continue

        matching = nx.algorithms.matching.max_weight_matching(
            graph, maxcardinality=False, weight="weight"
        )
        for u, v in matching:
            key = (u, v) if u < v else (v, u)
            row_idx = edge_to_row.get(key)
            if row_idx is not None:
                selected.add(row_idx)

    return selected


# =========================
# Fast post-matching evaluation
# =========================
def _safe_div(a: float, b: float) -> float:
    return (a / b) if b != 0 else 0.0


def summarize_selected_indices_fast(
    selected_idx: Set[int],
    eval_cache: EvaluationCache,
) -> Dict[str, Any]:
    """Count TP/FP/FN/TN without rebuilding classified rows for every threshold."""
    selected_evaluated = selected_idx & eval_cache.evaluated_indices

    tp = len(selected_evaluated & eval_cache.positive_indices)
    fp = len(selected_evaluated & eval_cache.negative_indices)
    fn = eval_cache.total_pos - tp
    tn = eval_cache.total_neg - fp

    precision = _safe_div(tp, tp + fp)
    recall = _safe_div(tp, tp + fn)
    specificity = _safe_div(tn, tn + fp)
    accuracy = _safe_div(tp + tn, tp + fp + fn + tn)
    f1 = _safe_div(2 * precision * recall, precision + recall)
    balanced_accuracy = 0.5 * (recall + specificity)

    return {
        "TP": tp,
        "FP": fp,
        "FN": fn,
        "TN": tn,
        "precision": precision,
        "recall": recall,
        "specificity": specificity,
        "f1": f1,
        "accuracy": accuracy,
        "balanced_accuracy": balanced_accuracy,
        "support_pos": tp + fn,
        "support_neg": tn + fp,
        "evaluated_pairs": tp + fp + fn + tn,
        "selected_pairs_evaluated": len(selected_evaluated),
    }


# =========================
# Threshold range processing
# =========================
def build_threshold_summary_rows_fast(
    precomputed: PrecomputedThresholdData,
    thresholds: List[float],
    geometry_col: str,
    op: str,
) -> List[Dict[str, Any]]:
    summary_rows: List[Dict[str, Any]] = []

    for threshold_idx, threshold in enumerate(thresholds, start=1):
        selected_idx = select_indices_from_precomputed_edges(
            precomputed.per_file_edges,
            threshold=threshold,
            op=op,
        )

        metrics = summarize_selected_indices_fast(selected_idx, precomputed.eval_cache)
        row = {
            "geometry_col": f"{geometry_col} (threshold + MWM)",
            "op": op,
            "threshold": threshold,
            "selected_pairs": len(selected_idx),
            **metrics,
            "skipped_missing_observed": precomputed.eval_cache.skipped_missing_observed,
            "skipped_exception": precomputed.eval_cache.skipped_exception,
        }
        summary_rows.append(row)

        if PRINT_PROGRESS and (
            threshold_idx == 1
            or threshold_idx == len(thresholds)
            or threshold_idx % max(1, PRINT_PROGRESS_EVERY) == 0
        ):
            print(
                f"threshold {op} {threshold:g}: "
                f"TP={metrics['TP']} FP={metrics['FP']} FN={metrics['FN']} TN={metrics['TN']} "
                f"precision={metrics['precision']:.4f} recall={metrics['recall']:.4f} f1={metrics['f1']:.4f}"
            )

    return summary_rows


def export_threshold_summary_csv(
    rows: List[Dict[str, Any]],
    output_csv: str | Path,
    encoding: str = "utf-8",
) -> None:
    output_csv = Path(output_csv)
    output_csv.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        "geometry_col",
        "op",
        "threshold",
        "selected_pairs",
        "selected_pairs_evaluated",
        "TP",
        "FP",
        "FN",
        "TN",
        "precision",
        "recall",
        "specificity",
        "f1",
        "accuracy",
        "balanced_accuracy",
        "support_pos",
        "support_neg",
        "evaluated_pairs",
        "skipped_missing_observed",
        "skipped_exception",
    ]

    with output_csv.open("w", encoding=encoding, newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def export_precision_recall_curve_from_summary_rows(
    rows: List[Dict[str, Any]],
    output_png: str | Path,
    dpi: int = 600,
    aspect_ratio: float = 1.45,
    panel_width: float = 5.2,
    legend_width: float = 1.65,
    color_precision: str = "#6A3D9A",
    color_recall: str = "#1B9E77",
    color_best_f1: str = "#6E6E6E",
    legend_anchor_x: float = 1.03,
    x_padding_fraction: float = 0.025,
    marker_every_n_points: int = 20,
    show_best_f1_line: bool = True,
    show_title: bool = False,
    x_label: str = "Junction-angle threshold (°)",
    y_label: str = "Score",
    show: bool = False,
) -> None:
    """
    Export a clean, publication-style precision/recall line figure.

    Design choices:
      - smooth line curves with sparse markers, suitable for 0.1 threshold step
      - color-blind-friendly purple/teal pairing
      - minimal top/right spines, outward ticks, and light horizontal grid
      - legend outside the plotting area on the right
      - optional best-F1 threshold reference line, labeled in the right-side legend
      - PNG output only
    """
    if not rows:
        raise ValueError("No summary rows available for plotting.")

    try:
        import matplotlib.pyplot as plt
        from matplotlib.ticker import AutoMinorLocator, MultipleLocator
    except ImportError as e:
        raise ImportError(
            "The precision/recall plot needs matplotlib. Install it with: pip install matplotlib"
        ) from e

    output_png = Path(output_png)
    output_png.parent.mkdir(parents=True, exist_ok=True)

    threshold = [float(r["threshold"]) for r in rows]
    precision = [float(r["precision"]) for r in rows]
    recall = [float(r["recall"]) for r in rows]

    best = max(
        rows,
        key=lambda r: (float(r["f1"]), float(r["precision"]), float(r["recall"])),
    )
    best_threshold = float(best["threshold"])
    best_f1 = float(best["f1"])

    # The aspect ratio is applied to the plotting panel only, not to the
    # extra canvas reserved for the right-side legend. This keeps the data
    # panel shape stable while allowing an external legend.
    panel_height = panel_width / aspect_ratio
    left_margin = 0.58
    bottom_margin = 0.48
    top_margin = 0.16 if not show_title else 0.38
    legend_gap = 0.24
    right_margin = 0.12
    total_width = left_margin + panel_width + legend_gap + legend_width + right_margin
    total_height = bottom_margin + panel_height + top_margin
    marker_every = None
    if marker_every_n_points and marker_every_n_points > 0:
        marker_every = max(1, int(marker_every_n_points))

    # Use a local rc_context so the script does not affect other figures.
    with plt.rc_context({
        "font.family": "sans-serif",
        "font.sans-serif": ["Arial", "Helvetica", "DejaVu Sans", "Liberation Sans"],
        "font.size": 8,
        "axes.labelsize": 8.5,
        "axes.titlesize": 9,
        "xtick.labelsize": 7.5,
        "ytick.labelsize": 7.5,
        "legend.fontsize": 8,
        "axes.linewidth": 1.0,
        "xtick.major.width": 1.0,
        "ytick.major.width": 1.0,
        "xtick.minor.width": 0.8,
        "ytick.minor.width": 0.8,
        "pdf.fonttype": 42,
        "ps.fonttype": 42,
        "svg.fonttype": "none",
    }):
        fig = plt.figure(figsize=(total_width, total_height))
        ax = fig.add_axes([
            left_margin / total_width,
            bottom_margin / total_height,
            panel_width / total_width,
            panel_height / total_height,
        ])

        # Subtle horizontal guide lines, similar to many high-quality journal line plots.
        ax.grid(axis="y", which="major", color="#E6E6E6", linewidth=0.6, linestyle="-", zorder=0)
        ax.grid(axis="y", which="minor", color="#F2F2F2", linewidth=0.4, linestyle="-", zorder=0)
        ax.grid(axis="x", visible=False)

        ax.plot(
            threshold,
            precision,
            color=color_precision,
            linewidth=2.1,
            marker="o",
            markevery=marker_every,
            markersize=4.2,
            markerfacecolor="white",
            markeredgecolor=color_precision,
            markeredgewidth=1.0,
            solid_capstyle="round",
            solid_joinstyle="round",
            label="Precision",
            zorder=3,
        )

        ax.plot(
            threshold,
            recall,
            color=color_recall,
            linewidth=2.1,
            marker="s",
            markevery=marker_every,
            markersize=4.0,
            markerfacecolor="white",
            markeredgecolor=color_recall,
            markeredgewidth=1.0,
            solid_capstyle="round",
            solid_joinstyle="round",
            label="Recall",
            zorder=3,
        )

        if show_best_f1_line:
            ax.axvline(
                best_threshold,
                color=color_best_f1,
                linewidth=1.0,
                linestyle=(0, (2.2, 2.2)),
                label=f"Best F1 = {best_f1:.3f} ({best_threshold:g}°)",
                zorder=1,
            )

        ax.set_xlabel(x_label)
        ax.set_ylabel(y_label)
        if show_title:
            ax.set_title("Precision and recall across thresholds", pad=8)

        # Clean journal-style axes.
        # Add a small x-axis padding so first/last markers are fully visible.
        # Without this, markers exactly at THRESHOLD_MIN or THRESHOLD_MAX are clipped by the axes.
        x_min = min(threshold)
        x_max = max(threshold)
        x_span = max(x_max - x_min, 1.0)
        x_pad = max(0.5, x_span * float(x_padding_fraction)) if x_padding_fraction > 0 else 0.0
        ax.set_xlim(x_min - x_pad, x_max + x_pad)
        ax.set_ylim(0.0, 1.02)
        ax.set_yticks([0.0, 0.2, 0.4, 0.6, 0.8, 1.0])
        ax.yaxis.set_minor_locator(MultipleLocator(0.1))
        ax.xaxis.set_minor_locator(AutoMinorLocator(2))

        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.spines["left"].set_linewidth(1.0)
        ax.spines["bottom"].set_linewidth(1.0)
        ax.tick_params(axis="both", which="major", direction="out", length=4.0, width=1.0, pad=3)
        ax.tick_params(axis="both", which="minor", direction="out", length=2.3, width=0.8)

        ax.legend(
            frameon=False,
            loc="center left",
            bbox_to_anchor=(legend_anchor_x, 0.5),
            borderaxespad=0.0,
            handlelength=2.4,
            handletextpad=0.6,
            labelspacing=0.75,
        )

        # Save without bbox_inches="tight" so the exported image keeps the
        # intended fixed canvas: plot panel + reserved legend area.
        fig.savefig(output_png, dpi=dpi, facecolor="white")

        if show:
            plt.show()
        else:
            plt.close(fig)


def main() -> None:
    """
    Run threshold-range inference using only the parameters defined at the top.
    One output CSV is written into RESULT_DIR, with one row per threshold.
    """
    all_data = read_all_rows(INPUT_CSV, encoding=ENCODING)
    thresholds = generate_threshold_values(THRESHOLD_MIN, THRESHOLD_MAX, THRESHOLD_STEP)

    print("\n[FAST THRESHOLD RANGE INFERENCE]")
    print(f"  input CSV       = {INPUT_CSV}")
    print(
        f"  exception label = {EXCEPTION_COLUMN} "
        f"(value={EXCEPTION_POSITIVE_VALUE})"
    )
    print(f"  result folder   = {RESULT_DIR}")
    print(f"  geometry column = {GEOMETRY_COLUMN}")
    print(f"  operator        = {OPERATOR}")
    print(f"  threshold range = {THRESHOLD_MIN} to {THRESHOLD_MAX}, step {THRESHOLD_STEP}")
    print(f"  threshold count = {len(thresholds)}")

    precomputed = precompute_threshold_data(
        container=all_data,
        geometry_col=GEOMETRY_COLUMN,
        op=OPERATOR,
        exception_col=EXCEPTION_COLUMN,
        exception_positive_value=EXCEPTION_POSITIVE_VALUE,
        observed_positive_value=OBSERVED_POSITIVE_VALUE,
        skip_missing_observed=SKIP_MISSING_OBSERVED,
        skip_exception_in_exports=SKIP_EXCEPTION_IN_EXPORTS,
    )

    total_precomputed_edges = sum(len(edges) for edges in precomputed.per_file_edges.values())
    print("\n[PRECOMPUTED CACHE]")
    print(f"  files with candidate edges = {len(precomputed.per_file_edges)}")
    print(f"  reusable candidate edges   = {total_precomputed_edges}")
    print(f"  evaluated pairs            = {precomputed.eval_cache.evaluated_pairs}")
    print(f"  positives / negatives      = {precomputed.eval_cache.total_pos} / {precomputed.eval_cache.total_neg}")
    print(f"  skipped_missing_observed   = {precomputed.eval_cache.skipped_missing_observed}")
    print(f"  skipped_exception          = {precomputed.eval_cache.skipped_exception}")

    summary_rows = build_threshold_summary_rows_fast(
        precomputed=precomputed,
        thresholds=thresholds,
        geometry_col=GEOMETRY_COLUMN,
        op=OPERATOR,
    )

    export_threshold_summary_csv(
        summary_rows,
        OUTPUT_THRESHOLD_SUMMARY_CSV,
        encoding=ENCODING,
    )

    export_precision_recall_curve_from_summary_rows(
        rows=summary_rows,
        output_png=OUTPUT_PRECISION_RECALL_FIGURE,
        dpi=PLOT_DPI,
        aspect_ratio=PLOT_ASPECT_RATIO,
        panel_width=PLOT_PANEL_WIDTH,
        legend_width=PLOT_LEGEND_WIDTH,
        color_precision=PLOT_COLOR_PRECISION,
        color_recall=PLOT_COLOR_RECALL,
        color_best_f1=PLOT_COLOR_BEST_F1,
        legend_anchor_x=PLOT_LEGEND_ANCHOR_X,
        x_padding_fraction=PLOT_X_PADDING_FRACTION,
        marker_every_n_points=PLOT_MARKER_EVERY_N_POINTS,
        show_best_f1_line=PLOT_SHOW_BEST_F1_LINE,
        show_title=PLOT_SHOW_TITLE,
        x_label=PLOT_X_LABEL,
        y_label=PLOT_Y_LABEL,
        show=PLOT_SHOW,
    )

    # Report best threshold by F1, using precision then recall as tie-breakers.
    best = max(
        summary_rows,
        key=lambda r: (float(r["f1"]), float(r["precision"]), float(r["recall"])),
    )

    print("\n[OUTPUT]")
    print(f"  threshold summary CSV:     {OUTPUT_THRESHOLD_SUMMARY_CSV}")
    print(f"  precision/recall PNG:      {OUTPUT_PRECISION_RECALL_FIGURE}")
    print("\n[BEST THRESHOLD BY F1]")
    print(
        f"  threshold {best['op']} {best['threshold']:g}: "
        f"TP={best['TP']} FP={best['FP']} FN={best['FN']} TN={best['TN']} "
        f"precision={best['precision']:.6f} recall={best['recall']:.6f} f1={best['f1']:.6f}"
    )


if __name__ == "__main__":
    main()
