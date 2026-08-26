from __future__ import annotations

"""
Evaluate train-derived thresholds on held-out primordia across every split.

For each split in all_satisfying_8train_4test_splits.csv:
  1. Use estimate_by_ranged_threshold.py to evaluate the threshold range on
     the TRAINING primordia.
  2. Select the best training threshold by:
         highest F1 -> highest precision -> highest recall
     (same rule as estimate_by_ranged_threshold.py).
  3. Apply that fixed threshold to the held-out TEST primordia with
     estimate_by_single_threshold.py.
  4. Export one result row per split.
  5. Plot:
       A. distribution of best training thresholds
       B. held-out test F1 / precision / recall

Optimization
------------
The ranged-threshold script performs maximum-weight matching independently for
each fileName. Therefore a primordium's result at a given threshold does not
depend on which other primordia are present in the training set.

This wrapper exploits that property:
  - run the ranged-threshold engine once per primordium;
  - cache TP/FP/FN/TN at every threshold;
  - for each train split, pool the cached counts from its training primordia;
  - recompute precision/recall/F1 from the pooled counts.

This is mathematically equivalent to rerunning the ranged-threshold engine
separately on each full training CSV, but is much faster.

The supplied ranged- and single-threshold scripts remain the inference engines;
their exception filtering, threshold rule, and exact maximum-weight matching
are not reimplemented here.

Plotting is fully self-contained in this file. It does NOT import
plot_performance_for_individual_grouped.py, but reproduces its boxplot/stripplot
format and metric styling.
"""

import csv
import importlib.util
import sys
from pathlib import Path
from types import ModuleType
from typing import Any, Dict, List, Sequence, Set, Tuple

import matplotlib.pyplot as plt
import numpy as np


# ============================================================
# Parameters to edit
# ============================================================

SCRIPT_DIR = Path(__file__).resolve().parent

INPUT_GEOMETRY_CSV = SCRIPT_DIR / "batch_neighbor_pair_geometry.csv"
SPLITS_CSV = SCRIPT_DIR / "all_satisfying_primordia_splits.csv"

RANGED_THRESHOLD_SCRIPT = SCRIPT_DIR / "estimate_by_ranged_threshold.py"
SINGLE_THRESHOLD_SCRIPT = SCRIPT_DIR / "estimate_by_single_threshold.py"

OUTPUT_DIR = SCRIPT_DIR / "train_test_threshold_evaluation"
OUTPUT_SUMMARY_CSV = OUTPUT_DIR / "train_test_threshold_evaluation.csv"
OUTPUT_SWEEP_CSV = OUTPUT_DIR / "train_threshold_sweeps_all_splits.csv"
OUTPUT_FIGURE = OUTPUT_DIR / "train_best_threshold_test_performance.png"

# None = inherit the setting from estimate_by_ranged_threshold.py
GEOMETRY_COLUMN_OVERRIDE = None
OPERATOR_OVERRIDE = None
THRESHOLD_MIN_OVERRIDE = None
THRESHOLD_MAX_OVERRIDE = None
THRESHOLD_STEP_OVERRIDE = None

DPI = 600
RANDOM_SEED = 123
FIGURE_SIZE = (8.8, 5.8)
SAVE_ALL_TRAIN_SWEEPS = True

# Same metric order / labels / colors as plot_performance_for_individual_grouped.py
METRICS: List[Tuple[str, str, str]] = [
    ("f1", "F1 score", "#00A087"),
    ("precision", "Precision", "#E64B35"),
    ("recall", "Recall", "#3C5488"),
]


# ============================================================
# Generic helpers
# ============================================================

def load_module(path: str | Path, label: str) -> ModuleType:
    path = Path(path).resolve()
    if not path.is_file():
        raise FileNotFoundError(f"Could not find {label}: {path}")

    name = f"_{label}_{abs(hash(str(path)))}"
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Could not load {label}: {path}")

    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    try:
        spec.loader.exec_module(module)
    except Exception:
        sys.modules.pop(name, None)
        raise
    return module


def read_csv_dicts(path: str | Path) -> List[Dict[str, str]]:
    with Path(path).open("r", encoding="utf-8-sig", newline="") as f:
        return list(csv.DictReader(f))


def parse_prefix_set(text: str) -> Set[str]:
    return {x.strip() for x in str(text).split(";") if x.strip()}


def normalize_filename(name: str) -> str:
    p = Path(str(name).strip())
    return p.stem if p.suffix else p.name


def get_filename(row: Any) -> str:
    data = row.data
    if "fileName" in data:
        return str(data["fileName"])
    for key, value in data.items():
        if str(key).casefold() == "filename":
            return str(value)
    raise ValueError("fileName column not found.")


def filename_matches_prefix(filename: str, prefix: str) -> bool:
    """
    Uses a delimiter boundary, so sample1 does not match sample10.
    Also supports prefixes that themselves contain underscores.
    """
    name = normalize_filename(filename)
    return name == prefix or name.startswith(prefix + "_")


def subset_rows(rows: Sequence[Any], prefixes: Set[str]) -> List[Any]:
    return [
        row
        for row in rows
        if any(filename_matches_prefix(get_filename(row), p) for p in prefixes)
    ]


def infer_dataset_prefixes(rows: Sequence[Any], known_prefixes: Set[str]) -> Set[str]:
    found: Set[str] = set()
    for row in rows:
        filename = get_filename(row)
        matches = [p for p in known_prefixes if filename_matches_prefix(filename, p)]
        if len(matches) != 1:
            raise ValueError(
                f"fileName '{filename}' matched {len(matches)} known prefixes: {matches}"
            )
        found.add(matches[0])
    return found


def safe_div(a: float, b: float) -> float:
    return a / b if b != 0 else 0.0


def metrics_from_counts(tp: int, fp: int, fn: int, tn: int) -> Dict[str, Any]:
    """
    Same formulas used by the supplied threshold scripts.
    """
    precision = safe_div(tp, tp + fp)
    recall = safe_div(tp, tp + fn)
    specificity = safe_div(tn, tn + fp)
    accuracy = safe_div(tp + tn, tp + fp + fn + tn)
    f1 = safe_div(2 * precision * recall, precision + recall)
    balanced_accuracy = 0.5 * (recall + specificity)

    return {
        "TP": int(tp),
        "FP": int(fp),
        "FN": int(fn),
        "TN": int(tn),
        "precision": precision,
        "recall": recall,
        "specificity": specificity,
        "f1": f1,
        "accuracy": accuracy,
        "balanced_accuracy": balanced_accuracy,
        "support_pos": int(tp + fn),
        "support_neg": int(tn + fp),
        "evaluated_pairs": int(tp + fp + fn + tn),
    }


def export_rows(rows: List[Dict[str, Any]], path: str | Path) -> None:
    if not rows:
        raise ValueError(f"No rows to export: {path}")
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


# ============================================================
# Precompute ranged-threshold results once per primordium
# ============================================================

def precompute_primordium_threshold_results(
    ranged: ModuleType,
    all_rows: List[Any],
    prefixes: Sequence[str],
    thresholds: List[float],
    geometry_column: str,
    operator: str,
) -> Dict[str, Dict[float, Dict[str, Any]]]:
    """
    Cache one ranged-threshold summary row for every:
        primordium x threshold
    """
    cache: Dict[str, Dict[float, Dict[str, Any]]] = {}

    for i, prefix in enumerate(prefixes, start=1):
        prim_rows = subset_rows(all_rows, {prefix})
        if not prim_rows:
            raise ValueError(f"No geometry rows found for primordium '{prefix}'.")

        precomputed = ranged.precompute_threshold_data(
            container=prim_rows,
            geometry_col=geometry_column,
            op=operator,
            exception_col=ranged.EXCEPTION_COLUMN,
            exception_positive_value=ranged.EXCEPTION_POSITIVE_VALUE,
            observed_positive_value=ranged.OBSERVED_POSITIVE_VALUE,
            skip_missing_observed=ranged.SKIP_MISSING_OBSERVED,
            skip_exception_in_exports=ranged.SKIP_EXCEPTION_IN_EXPORTS,
        )

        summaries = ranged.build_threshold_summary_rows_fast(
            precomputed=precomputed,
            thresholds=thresholds,
            geometry_col=geometry_column,
            op=operator,
        )

        cache[prefix] = {
            float(row["threshold"]): row
            for row in summaries
        }

        print(
            f"  cached primordium {i:02d}/{len(prefixes):02d}: "
            f"{prefix} ({len(prim_rows)} input rows)"
        )

    return cache


# ============================================================
# Pool cached training results for one split
# ============================================================

def pooled_training_sweep(
    split_id: str,
    train_prefixes: Set[str],
    thresholds: List[float],
    cache: Dict[str, Dict[float, Dict[str, Any]]],
    geometry_column: str,
    operator: str,
) -> List[Dict[str, Any]]:
    out: List[Dict[str, Any]] = []

    for threshold in thresholds:
        rows = [cache[p][float(threshold)] for p in train_prefixes]

        tp = sum(int(r["TP"]) for r in rows)
        fp = sum(int(r["FP"]) for r in rows)
        fn = sum(int(r["FN"]) for r in rows)
        tn = sum(int(r["TN"]) for r in rows)

        metrics = metrics_from_counts(tp, fp, fn, tn)

        out.append(
            {
                "split_id": split_id,
                "geometry_col": f"{geometry_column} (threshold + MWM)",
                "op": operator,
                "threshold": float(threshold),
                "selected_pairs": sum(int(r["selected_pairs"]) for r in rows),
                "selected_pairs_evaluated": sum(
                    int(r["selected_pairs_evaluated"]) for r in rows
                ),
                **metrics,
                "skipped_missing_observed": sum(
                    int(r["skipped_missing_observed"]) for r in rows
                ),
                "skipped_exception": sum(
                    int(r["skipped_exception"]) for r in rows
                ),
            }
        )

    return out


def choose_best_threshold(
    sweep_rows: List[Dict[str, Any]],
) -> Dict[str, Any]:
    """
    Exactly matches estimate_by_ranged_threshold.py:
      highest F1 -> precision -> recall.
    If all three tie, Python max() keeps the first threshold in sweep order.
    """
    return max(
        sweep_rows,
        key=lambda r: (
            float(r["f1"]),
            float(r["precision"]),
            float(r["recall"]),
        ),
    )


# ============================================================
# Held-out test evaluation with single-threshold engine
# ============================================================

def evaluate_test(
    single: ModuleType,
    test_rows: List[Any],
    threshold: float,
    geometry_column: str,
    operator: str,
) -> Tuple[Dict[str, Any], Dict[str, int], int]:
    selected = single.select_indices_by_threshold_then_max_weight_matching(
        test_rows,
        geometry_col=geometry_column,
        threshold=float(threshold),
        op=operator,
        exception_col=single.EXCEPTION_COLUMN,
        exception_positive_value=single.EXCEPTION_POSITIVE_VALUE,
    )

    classified, skipped = single.classify_pairs_after_matching(
        test_rows,
        selected_idx=selected,
        geometry_col=geometry_column,
        threshold=float(threshold),
        op=operator,
        observed_positive_value=single.OBSERVED_POSITIVE_VALUE,
        skip_missing_observed=single.SKIP_MISSING_OBSERVED,
        exception_col=single.EXCEPTION_COLUMN,
        exception_positive_value=single.EXCEPTION_POSITIVE_VALUE,
        skip_exception_in_exports=single.SKIP_EXCEPTION_IN_EXPORTS,
    )

    metrics = single.summarize_classified_rows(classified)
    return metrics, skipped, len(selected)


# ============================================================
# Plot
# ============================================================

def draw_box_strip(
    ax,
    vals: np.ndarray,
    x: float,
    color: str,
    rng: np.random.Generator,
    width: float = 0.48,
    score_y_axes: float = -0.24,
) -> Tuple[float, float]:
    """
    Same boxplot + jitter + mean-diamond + mean +/- SD layout used by
    plot_performance_for_individual_grouped.py.
    """
    vals = np.asarray(vals, dtype=float)
    vals = vals[np.isfinite(vals)]

    mean = float(np.mean(vals))
    sd = float(np.std(vals, ddof=1)) if len(vals) > 1 else 0.0

    ax.boxplot(
        [vals],
        positions=[x],
        widths=width,
        patch_artist=True,
        showfliers=False,
        boxprops={
            "facecolor": color,
            "alpha": 0.18,
            "edgecolor": color,
            "linewidth": 1.8,
        },
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
        [x],
        [mean],
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
    """
    Same performance-axis formatting as plot_performance_for_individual_grouped.py.
    """
    ax.set_ylim(0.0, 1.08)

    yticks = np.linspace(0, 1, 6)
    ax.set_yticks(yticks)
    ax.set_yticklabels(
        [f"{v * 100:.0f}" for v in yticks],
        fontsize=11.5,
    )

    ax.set_axisbelow(True)
    ax.grid(
        axis="y",
        linestyle=(0, (2, 2)),
        linewidth=0.9,
        color="#D9D9D9",
    )

    for side in ("top", "right"):
        ax.spines[side].set_visible(False)

    for side in ("left", "bottom"):
        ax.spines[side].set_linewidth(1.8)
        ax.spines[side].set_color("black")

    ax.tick_params(axis="x", width=1.6, length=0, pad=8)
    ax.tick_params(axis="y", width=1.6, length=5)
    ax.set_ylabel(ylabel, fontsize=15)


def draw_threshold_box_strip(
    ax,
    vals: np.ndarray,
    rng: np.random.Generator,
    score_y_axes: float = -0.24,
) -> Tuple[float, float]:
    vals = np.asarray(vals, dtype=float)
    vals = vals[np.isfinite(vals)]

    mean = float(np.mean(vals))
    sd = float(np.std(vals, ddof=1)) if vals.size > 1 else 0.0

    # Use Matplotlib's current default series color.
    color = plt.rcParams["axes.prop_cycle"].by_key()["color"][0]

    ax.boxplot(
        [vals],
        positions=[1.0],
        widths=0.48,
        patch_artist=True,
        showfliers=False,
        boxprops={
            "facecolor": color,
            "alpha": 0.18,
            "edgecolor": color,
            "linewidth": 1.8,
        },
        medianprops={"color": "black", "linewidth": 2.0},
        whiskerprops={"color": color, "linewidth": 1.6},
        capprops={"color": color, "linewidth": 1.6},
    )

    jitter = rng.uniform(-0.11, 0.11, size=len(vals))
    ax.scatter(
        np.full(len(vals), 1.0) + jitter,
        vals,
        s=42,
        facecolor=color,
        edgecolor="black",
        linewidth=0.75,
        alpha=0.88,
        zorder=3,
    )

    ax.scatter(
        [1.0],
        [mean],
        marker="D",
        s=68,
        facecolor="white",
        edgecolor="black",
        linewidth=1.0,
        zorder=4,
    )

    ax.text(
        1.0,
        score_y_axes,
        rf"${mean:.1f}\ \pm\ {sd:.1f}^\circ$",
        transform=ax.get_xaxis_transform(),
        ha="center",
        va="top",
        fontsize=9.0,
        clip_on=False,
    )

    return mean, sd


def style_threshold_axis(
    ax,
    threshold_min: float,
    threshold_max: float,
) -> None:
    span = max(threshold_max - threshold_min, 1.0)
    pad = max(1.0, 0.04 * span)

    ax.set_ylim(threshold_min - pad, threshold_max + pad)
    ax.set_axisbelow(True)
    ax.grid(axis="y", linestyle=(0, (2, 2)), linewidth=0.9)

    for side in ("top", "right"):
        ax.spines[side].set_visible(False)

    for side in ("left", "bottom"):
        ax.spines[side].set_linewidth(1.8)

    ax.tick_params(axis="x", width=1.6, length=0, pad=8)
    ax.tick_params(axis="y", width=1.6, length=5, labelsize=11.5)
    ax.set_ylabel("Best threshold (°)", fontsize=15)


def plot_summary(
    result_rows: List[Dict[str, Any]],
    output_png: str | Path,
    threshold_min: float,
    threshold_max: float,
) -> None:
    rng = np.random.default_rng(RANDOM_SEED)

    plt.rcParams.update(
        {
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "savefig.facecolor": "white",
            "font.size": 11,
            "font.family": "sans-serif",
        }
    )

    fig, (axA, axB) = plt.subplots(
        1,
        2,
        figsize=FIGURE_SIZE,
    )

    # Panel A: best threshold from training
    best_thresholds = np.array(
        [float(r["best_train_threshold"]) for r in result_rows]
    )

    draw_threshold_box_strip(
        axA,
        vals=best_thresholds,
        rng=rng,
    )

    axA.set_xlim(0.35, 1.65)
    axA.set_xticks([1])
    axA.set_xticklabels(["Best threshold"], fontsize=11.6)

    axA.text(
        0.5,
        1.06,
        f"Training data\n(n = {len(result_rows)} splits)",
        transform=axA.transAxes,
        ha="center",
        va="top",
        fontsize=12.5,
    )
    axA.text(
        -0.22,
        1.03,
        "A",
        transform=axA.transAxes,
        fontsize=16,
        fontweight="bold",
        va="top",
    )

    style_threshold_axis(
        axA,
        threshold_min=threshold_min,
        threshold_max=threshold_max,
    )

    # Panel B: held-out test performance
    metric_specs = [
        ("test_f1", METRICS[0][1], METRICS[0][2]),
        ("test_precision", METRICS[1][1], METRICS[1][2]),
        ("test_recall", METRICS[2][1], METRICS[2][2]),
    ]

    x_positions = np.array([1, 2, 3], dtype=float)

    for x, (metric, label, color) in zip(x_positions, metric_specs):
        vals = np.array(
            [float(r[metric]) for r in result_rows],
            dtype=float,
        )
        draw_box_strip(
            axB,
            vals=vals,
            x=x,
            color=color,
            rng=rng,
        )

    axB.set_xlim(0.35, 3.65)
    axB.set_xticks(x_positions)

    tick_objects = axB.set_xticklabels(
        [label for _, label, _ in metric_specs],
        fontsize=11.6,
    )
    for tick, (_, _, color) in zip(tick_objects, metric_specs):
        tick.set_color(color)

    axB.text(
        0.5,
        1.06,
        f"Held-out test data\n(n = {len(result_rows)} splits)",
        transform=axB.transAxes,
        ha="center",
        va="top",
        fontsize=12.5,
    )
    axB.text(
        -0.14,
        1.03,
        "B",
        transform=axB.transAxes,
        fontsize=16,
        fontweight="bold",
        va="top",
    )

    style_axis(axB, ylabel="Score (%)")

    fig.tight_layout(
        w_pad=2.0,
        rect=(0.0, 0.12, 1.0, 1.0),
    )

    output_png = Path(output_png)
    output_png.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_png, dpi=DPI, bbox_inches="tight")
    plt.close(fig)


# ============================================================
# Main
# ============================================================

def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    ranged = load_module(
        RANGED_THRESHOLD_SCRIPT,
        "estimate_by_ranged_threshold",
    )
    single = load_module(
        SINGLE_THRESHOLD_SCRIPT,
        "estimate_by_single_threshold",
    )
    # Suppress one line of progress per threshold from the ranged module.
    if hasattr(ranged, "PRINT_PROGRESS"):
        ranged.PRINT_PROGRESS = False

    geometry_column = (
        GEOMETRY_COLUMN_OVERRIDE
        if GEOMETRY_COLUMN_OVERRIDE is not None
        else ranged.GEOMETRY_COLUMN
    )
    operator = (
        OPERATOR_OVERRIDE
        if OPERATOR_OVERRIDE is not None
        else ranged.OPERATOR
    )
    threshold_min = float(
        THRESHOLD_MIN_OVERRIDE
        if THRESHOLD_MIN_OVERRIDE is not None
        else ranged.THRESHOLD_MIN
    )
    threshold_max = float(
        THRESHOLD_MAX_OVERRIDE
        if THRESHOLD_MAX_OVERRIDE is not None
        else ranged.THRESHOLD_MAX
    )
    threshold_step = float(
        THRESHOLD_STEP_OVERRIDE
        if THRESHOLD_STEP_OVERRIDE is not None
        else ranged.THRESHOLD_STEP
    )

    thresholds = ranged.generate_threshold_values(
        threshold_min,
        threshold_max,
        threshold_step,
    )

    split_rows = read_csv_dicts(SPLITS_CSV)
    if not split_rows:
        raise ValueError(f"No rows found in split CSV: {SPLITS_CSV}")

    required_cols = {"split_id", "train_set", "test_set"}
    missing = required_cols - set(split_rows[0].keys())
    if missing:
        raise ValueError(
            f"Split CSV is missing required column(s): {sorted(missing)}"
        )

    # Gather every prefix mentioned by the split table.
    known_prefixes: Set[str] = set()
    for split in split_rows:
        known_prefixes |= parse_prefix_set(split["train_set"])
        known_prefixes |= parse_prefix_set(split["test_set"])

    ranged_all_rows = ranged.read_all_rows(
        INPUT_GEOMETRY_CSV,
        encoding=getattr(ranged, "ENCODING", "utf-8"),
    )
    single_all_rows = single.read_all_rows(
        INPUT_GEOMETRY_CSV,
        encoding=getattr(single, "ENCODING", "utf-8"),
    )

    dataset_prefixes = infer_dataset_prefixes(
        ranged_all_rows,
        known_prefixes,
    )

    if dataset_prefixes != known_prefixes:
        raise ValueError(
            "Prefixes in split CSV do not exactly match prefixes in geometry data.\n"
            f"Geometry: {sorted(dataset_prefixes)}\n"
            f"Splits:   {sorted(known_prefixes)}"
        )

    print("\n[TRAIN -> TEST EVALUATION]")
    print(f"  geometry CSV    = {INPUT_GEOMETRY_CSV}")
    print(f"  split CSV       = {SPLITS_CSV}")
    print(f"  splits          = {len(split_rows)}")
    print(f"  primordia       = {len(known_prefixes)}")
    print(f"  geometry column = {geometry_column}")
    print(f"  operator        = {operator}")
    print(
        f"  thresholds      = {threshold_min:g} to "
        f"{threshold_max:g}, step {threshold_step:g}"
    )

    # --------------------------------------------------------
    # Expensive part: perform each primordium threshold sweep ONCE.
    # --------------------------------------------------------
    print("\n[PRECOMPUTING PRIMORDIUM THRESHOLD SWEEPS]")

    threshold_cache = precompute_primordium_threshold_results(
        ranged=ranged,
        all_rows=ranged_all_rows,
        prefixes=sorted(known_prefixes),
        thresholds=thresholds,
        geometry_column=geometry_column,
        operator=operator,
    )

    # --------------------------------------------------------
    # Evaluate every split
    # --------------------------------------------------------
    result_rows: List[Dict[str, Any]] = []
    sweep_export_rows: List[Dict[str, Any]] = []

    print("\n[EVALUATING SPLITS]")

    for i, split in enumerate(split_rows, start=1):
        split_id = str(split["split_id"]).strip()
        train_prefixes = parse_prefix_set(split["train_set"])
        test_prefixes = parse_prefix_set(split["test_set"])

        if train_prefixes & test_prefixes:
            raise ValueError(
                f"{split_id}: train/test overlap: "
                f"{sorted(train_prefixes & test_prefixes)}"
            )

        if train_prefixes | test_prefixes != known_prefixes:
            raise ValueError(
                f"{split_id}: train + test does not cover all primordia."
            )

        train_sweep = pooled_training_sweep(
            split_id=split_id,
            train_prefixes=train_prefixes,
            thresholds=thresholds,
            cache=threshold_cache,
            geometry_column=geometry_column,
            operator=operator,
        )

        best_train = choose_best_threshold(train_sweep)
        best_threshold = float(best_train["threshold"])

        # Test data are analyzed only at the train-derived threshold.
        test_rows = subset_rows(single_all_rows, test_prefixes)
        test_metrics, test_skipped, test_selected = evaluate_test(
            single=single,
            test_rows=test_rows,
            threshold=best_threshold,
            geometry_column=geometry_column,
            operator=operator,
        )

        train_input_rows = len(subset_rows(ranged_all_rows, train_prefixes))

        result_rows.append(
            {
                "split_id": split_id,
                "train_set": split["train_set"],
                "test_set": split["test_set"],
                "n_train_primordia": len(train_prefixes),
                "n_test_primordia": len(test_prefixes),
                "train_input_rows": train_input_rows,
                "test_input_rows": len(test_rows),

                "geometry_column": geometry_column,
                "operator": operator,
                "threshold_min": threshold_min,
                "threshold_max": threshold_max,
                "threshold_step": threshold_step,

                "best_train_threshold": best_threshold,

                "train_TP": int(best_train["TP"]),
                "train_FP": int(best_train["FP"]),
                "train_FN": int(best_train["FN"]),
                "train_TN": int(best_train["TN"]),
                "train_precision": float(best_train["precision"]),
                "train_recall": float(best_train["recall"]),
                "train_f1": float(best_train["f1"]),
                "train_specificity": float(best_train["specificity"]),
                "train_accuracy": float(best_train["accuracy"]),
                "train_balanced_accuracy": float(
                    best_train["balanced_accuracy"]
                ),
                "train_evaluated_pairs": int(
                    best_train["evaluated_pairs"]
                ),
                "train_selected_pairs": int(
                    best_train["selected_pairs"]
                ),
                "train_skipped_missing_observed": int(
                    best_train["skipped_missing_observed"]
                ),
                "train_skipped_exception": int(
                    best_train["skipped_exception"]
                ),

                "test_TP": int(test_metrics["TP"]),
                "test_FP": int(test_metrics["FP"]),
                "test_FN": int(test_metrics["FN"]),
                "test_TN": int(test_metrics["TN"]),
                "test_precision": float(test_metrics["precision"]),
                "test_recall": float(test_metrics["recall"]),
                "test_f1": float(test_metrics["f1"]),
                "test_specificity": float(test_metrics["specificity"]),
                "test_accuracy": float(test_metrics["accuracy"]),
                "test_balanced_accuracy": float(
                    test_metrics["balanced_accuracy"]
                ),
                "test_evaluated_pairs": int(
                    test_metrics["evaluated_pairs"]
                ),
                "test_selected_pairs": int(test_selected),
                "test_skipped_missing_observed": int(
                    test_skipped["skipped_missing_observed"]
                ),
                "test_skipped_exception": int(
                    test_skipped["skipped_exception"]
                ),
            }
        )

        if SAVE_ALL_TRAIN_SWEEPS:
            for row in train_sweep:
                sweep_export_rows.append(
                    {
                        "train_set": split["train_set"],
                        "test_set": split["test_set"],
                        **row,
                    }
                )

        print(
            f"  [{i:02d}/{len(split_rows):02d}] {split_id}: "
            f"best train threshold={best_threshold:g}°, "
            f"train F1={float(best_train['f1']):.4f}, "
            f"test F1={float(test_metrics['f1']):.4f}, "
            f"P={float(test_metrics['precision']):.4f}, "
            f"R={float(test_metrics['recall']):.4f}"
        )

    # --------------------------------------------------------
    # Export
    # --------------------------------------------------------
    export_rows(result_rows, OUTPUT_SUMMARY_CSV)

    if SAVE_ALL_TRAIN_SWEEPS:
        export_rows(sweep_export_rows, OUTPUT_SWEEP_CSV)

    plot_summary(
        result_rows=result_rows,
        output_png=OUTPUT_FIGURE,
        threshold_min=threshold_min,
        threshold_max=threshold_max,
    )

    # --------------------------------------------------------
    # Summary
    # --------------------------------------------------------
    def mean_sd(key: str) -> Tuple[float, float]:
        vals = np.array([float(r[key]) for r in result_rows], dtype=float)
        return (
            float(np.mean(vals)),
            float(np.std(vals, ddof=1)) if len(vals) > 1 else 0.0,
        )

    thr_mean, thr_sd = mean_sd("best_train_threshold")
    f1_mean, f1_sd = mean_sd("test_f1")
    p_mean, p_sd = mean_sd("test_precision")
    r_mean, r_sd = mean_sd("test_recall")

    print("\n[SUMMARY ACROSS SPLITS]")
    print(f"  best threshold = {thr_mean:.3f} +/- {thr_sd:.3f} degrees")
    print(f"  test F1        = {f1_mean:.4f} +/- {f1_sd:.4f}")
    print(f"  test precision = {p_mean:.4f} +/- {p_sd:.4f}")
    print(f"  test recall    = {r_mean:.4f} +/- {r_sd:.4f}")

    print("\n[OUTPUTS]")
    print(f"  per-split summary = {OUTPUT_SUMMARY_CSV}")
    if SAVE_ALL_TRAIN_SWEEPS:
        print(f"  train sweeps      = {OUTPUT_SWEEP_CSV}")
    print(f"  figure            = {OUTPUT_FIGURE}")


if __name__ == "__main__":
    main()
