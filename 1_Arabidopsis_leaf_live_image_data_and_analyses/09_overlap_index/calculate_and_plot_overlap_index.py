#!/usr/bin/env python3
"""Calculate and plot daughter/non-daughter overlap indices.

The input is the revised neighbor-pair geometry CSV in which exception labels
are stored directly in ``exception_label``.  Exception-labelled rows are
removed before any distributional calculation.

Outputs
-------
``overlap_indices.csv``
    One row per numeric geometry feature.
``top10_overlap_index.png``
    The ten features with the lowest overlap.
``all_features_overlap_index.png``
    All analyzed geometry features.

The overlap index is the sum of the shared probability mass in normalized
daughter and non-daughter histograms.  It ranges from 0 (no histogram overlap)
to 1 (complete histogram overlap).
"""

from __future__ import annotations

import argparse
import math
import os
from pathlib import Path
from typing import Iterable

import matplotlib

matplotlib.use("Agg")
import matplotlib.font_manager as fm
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib.ticker import FormatStrFormatter


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_INPUT_CSV = SCRIPT_DIR / "batch_neighbor_pair_geometry.csv"
DEFAULT_OUTPUT_DIR = SCRIPT_DIR / "overlap_index_results"

OBSERVED_COLUMN = "observed_division"
OBSERVED_POSITIVE_VALUE = 1
EXCEPTION_COLUMN = "exception_label"
EXCEPTION_POSITIVE_VALUE = 1
N_BINS = 40
N_TOP = 10
DPI = 300

FONT_FAMILY = "Arial"
COLOR_MAIN = "#6EA6CD"
COLOR_HIGHLIGHT = "#2E6F9E"
COLOR_GRID = "#D9D9D9"
COLOR_TEXT = "#222222"
COLOR_SPINE = "#333333"

NON_FEATURE_COLUMNS = {
    "filename",
    "pairindex",
    "pair_index",
    "firstpolygonid",
    "first_polygon_id",
    "cell_1_id",
    "secondpolygonid",
    "second_polygon_id",
    "cell_2_id",
    "observed_division",
    "division_timing",
    "estimated_division",
    "exception_label",
}

FEATURE_NAME_MAP = {
    # Pairwise features
    "areaRatio": "Area ratio",
    "area_ratio": "Area ratio",
    "areaMean": "Area mean",
    "area_mean": "Area mean",
    "areaMin": "Area minimum",
    "area_min": "Area minimum",
    "areaMax": "Area maximum",
    "area_max": "Area maximum",
    "areaDiff": "Area difference",
    "area_diff": "Area difference",

    "perimeterRatio": "Perimeter ratio",
    "perimeter_ratio": "Perimeter ratio",
    "perimeterMean": "Perimeter mean",
    "perimeter_mean": "Perimeter mean",
    "perimeterMin": "Perimeter minimum",
    "perimeter_min": "Perimeter minimum",
    "perimeterMax": "Perimeter maximum",
    "perimeter_max": "Perimeter maximum",
    "perimeterDiff": "Perimeter difference",
    "perimeter_diff": "Perimeter difference",

    "aspectRatio": "Ratio of aspect ratio",
    "aspect_ratio": "Ratio of aspect ratio",
    "aspectMean": "Aspect ratio mean",
    "aspect_mean": "Aspect ratio mean",
    "aspectMin": "Aspect ratio minimum",
    "aspect_min": "Aspect ratio minimum",
    "aspectMax": "Aspect ratio maximum",
    "aspect_max": "Aspect ratio maximum",
    "aspectDiff": "Aspect ratio difference",
    "aspect_diff": "Aspect ratio difference",

    "circularityRatio": "Circularity ratio",
    "circularity_ratio": "Circularity ratio",
    "circularityMean": "Circularity mean",
    "circularity_mean": "Circularity mean",
    "circularityMin": "Circularity minimum",
    "circularity_min": "Circularity minimum",
    "circularityMax": "Circularity maximum",
    "circularity_max": "Circularity maximum",
    "circularityDiff": "Circularity difference",
    "circularity_diff": "Circularity difference",

    "solidityRatio": "Solidity ratio",
    "solidity_ratio": "Solidity ratio",
    "solidityMean": "Solidity mean",
    "solidity_mean": "Solidity mean",
    "solidityMin": "Solidity minimum",
    "solidity_min": "Solidity minimum",
    "solidityMax": "Solidity maximum",
    "solidity_max": "Solidity maximum",
    "solidityDiff": "Solidity difference",
    "solidity_diff": "Solidity difference",

    "vertexCountRatio": "Vertex count ratio",
    "vertex_count_ratio": "Vertex count ratio",
    "vertexCountMean": "Vertex count mean",
    "vertex_count_mean": "Vertex count mean",
    "vertexCountMin": "Vertex count minimum",
    "vertex_count_min": "Vertex count minimum",
    "vertexCountMax": "Vertex count maximum",
    "vertex_count_max": "Vertex count maximum",
    "vertexCountDiff": "Vertex count difference",
    "vertex_count_diff": "Vertex count difference",

    "centroidDistance": "Centroid distance",
    "centroid_distance": "Centroid distance",
    "centroidDistanceNormalized": "Centroid distance normalized",
    "centroid_distance_normalized": "Centroid distance normalized",

    # Union features
    "unionAspectRatio": "Union aspect ratio",
    "union_aspect_ratio": "Union aspect ratio",
    "unionCircularity": "Union circularity",
    "union_circularity": "Union circularity",
    "unionConvexDeficiency": "Union convex deficiency",
    "convex_deficiency": "Union convex deficiency",
    "union_convex_deficiency": "Union convex deficiency",

    # Contact features
    "sharedEdgeLength": "Shared cell wall length",
    "shared_edge_length": "Shared cell wall length",
    "shared_cell_wall_length": "Shared cell wall length",

    "normalizedSharedEdgeLength": "Normalized shared cell wall length",
    "shared_cell_wall_length_normalized": "Normalized shared cell wall length",
    "shared_edge_length_normalized": "Normalized shared cell wall length",

    "sharedEdgeUnsharedVerticesDistance": "Distance between shared edge and unshared vertices",
    "shared_cell_wall_vertex_distance": "Distance between shared edge and unshared vertices",
    "shared_edge_unshared_vertices_distance": "Distance between shared edge and unshared vertices",

    "sharedEdgeUnsharedVerticesDistanceNormalized": "Normalized distance between shared edge and unshared vertices",
    "shared_edge_vertex_distance_normalized": "Normalized distance between shared edge and unshared vertices",
    "shared_edge_unshared_vertices_distance_normalized": "Normalized distance between shared edge and unshared vertices",

    "centroidSharedEdgeDistance": "Distance between centroids and shared edge",
    "centroid_shared_edge_distance": "Distance between centroids and shared edge",

    "centroidSharedEdgeDistanceNormalized": "Normalized distance between centroids and shared edge",
    "centroid_shared_edge_distance_normalized": "Normalized distance between centroids and shared edge",

    "sharedEdgeUnionCentroidDistance": "Distance between union centroid and shared edge",
    "union_centroid_edge_distance": "Distance between union centroid and shared edge",
    "shared_edge_union_centroid_distance": "Distance between union centroid and shared edge",

    "sharedEdgeUnionCentroidDistanceNormalized": "Normalized distance between union centroid and shared edge",
    "union_centroid_edge_distance_normalized": "Normalized distance between union centroid and shared edge",
    "shared_edge_union_centroid_distance_normalized": "Normalized distance between union centroid and shared edge",

    "sharedEdgeUnionAxisAngleDegrees": "Angle between shared edge and union axis",
    "union_axis_shared_edge_angle": "Angle between shared edge and union axis",
    "shared_edge_union_axis_angle_degrees": "Angle between shared edge and union axis",

    "junctionAngleAverageDegrees": "Junction angle mean",
    "junction_angle_mean": "Junction angle mean",
    "junctionAngleMaxDegrees": "Junction angle maximum",
    "junction_angle_max": "Junction angle maximum",
    "junctionAngleMinDegrees": "Junction angle minimum",
    "junction_angle_min": "Junction angle minimum",
    "junctionAngleRatio": "Junction angle ratio",
    "junction_angle_ratio": "Junction angle ratio",
    "junctionAngleDifferenceDegrees": "Junction angle difference",
    "junction_angle_diff": "Junction angle difference",
}



def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT_CSV)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--bins", type=int, default=N_BINS)
    parser.add_argument("--top", type=int, default=N_TOP)
    parser.add_argument("--exception-column", default=EXCEPTION_COLUMN)
    parser.add_argument("--exception-value", default=str(EXCEPTION_POSITIVE_VALUE))
    parser.add_argument("--observed-column", default=OBSERVED_COLUMN)
    parser.add_argument("--observed-positive-value", type=float, default=OBSERVED_POSITIVE_VALUE)
    parser.add_argument("--dpi", type=int, default=DPI)
    parser.add_argument("--svg", action="store_true", help="Also export SVG figures.")
    return parser.parse_args()


def resolve_column(columns: Iterable[str], wanted: str) -> str:
    columns = list(columns)
    if wanted in columns:
        return wanted
    wanted_key = wanted.casefold()
    for column in columns:
        if column.casefold() == wanted_key:
            return column
    raise ValueError(f"Column '{wanted}' was not found. Available columns: {columns}")


def positive_value_mask(series: pd.Series, positive_value: str | int | float) -> pd.Series:
    """Match numeric labels first and fall back to case-insensitive text."""
    numeric = pd.to_numeric(series, errors="coerce")
    try:
        target_numeric = float(positive_value)
        numeric_match = numeric.eq(target_numeric)
    except (TypeError, ValueError):
        numeric_match = pd.Series(False, index=series.index)
    text_match = (
        series.fillna("").astype(str).str.strip().str.casefold()
        == str(positive_value).strip().casefold()
    )
    return numeric_match | text_match


def load_and_filter_input(
    input_csv: Path,
    exception_column: str,
    exception_value: str,
    observed_column: str,
) -> tuple[pd.DataFrame, str, int]:
    data = pd.read_csv(input_csv, encoding="utf-8-sig")
    if data.empty:
        raise ValueError(f"Input CSV is empty: {input_csv}")

    exception_col = resolve_column(data.columns, exception_column)
    observed_col = resolve_column(data.columns, observed_column)
    is_exception = positive_value_mask(data[exception_col], exception_value)
    skipped_exception = int(is_exception.sum())

    # This filtering is intentionally performed before group masks, histogram
    # construction, range selection, or any other analysis.
    filtered = data.loc[~is_exception].copy()
    filtered[observed_col] = pd.to_numeric(filtered[observed_col], errors="coerce")
    return filtered, observed_col, skipped_exception


def standardized_feature_label(raw_name: str) -> str:
    return FEATURE_NAME_MAP.get(raw_name, raw_name)


def numeric_feature_columns(data: pd.DataFrame, observed_col: str) -> list[str]:
    features: list[str] = []
    for column in data.columns:
        if column == observed_col or column.casefold() in NON_FEATURE_COLUMNS:
            continue
        numeric = pd.to_numeric(data[column], errors="coerce")
        if numeric.notna().any():
            data[column] = numeric
            features.append(column)
    return features


def histogram_overlap(x_daughter: np.ndarray, x_non_daughter: np.ndarray, n_bins: int) -> float:
    pooled = np.concatenate([x_daughter, x_non_daughter])
    vmin = float(np.min(pooled))
    vmax = float(np.max(pooled))
    if np.isclose(vmin, vmax):
        return 1.0

    edges = np.linspace(vmin, vmax, n_bins + 1)
    daughter_counts, _ = np.histogram(x_daughter, bins=edges)
    non_counts, _ = np.histogram(x_non_daughter, bins=edges)
    daughter_prob = daughter_counts / daughter_counts.sum()
    non_prob = non_counts / non_counts.sum()
    return float(np.minimum(daughter_prob, non_prob).sum())


def calculate_overlap_indices(
    data: pd.DataFrame,
    observed_col: str,
    observed_positive_value: float,
    n_bins: int,
) -> pd.DataFrame:
    if n_bins < 2:
        raise ValueError("--bins must be at least 2.")

    observed = data[observed_col]
    daughter_mask = observed.eq(float(observed_positive_value))
    non_daughter_mask = observed.notna() & ~daughter_mask

    results: list[dict[str, object]] = []
    for feature in numeric_feature_columns(data, observed_col):
        daughter = data.loc[daughter_mask, feature].dropna().to_numpy(dtype=float)
        non_daughter = data.loc[non_daughter_mask, feature].dropna().to_numpy(dtype=float)

        overlap = math.nan
        if len(daughter) >= 2 and len(non_daughter) >= 2:
            overlap = histogram_overlap(daughter, non_daughter, n_bins)

        results.append(
            {
                "feature": feature,
                "plot_label": standardized_feature_label(feature),
                "overlap_index": overlap,
                "overlap_percent": overlap * 100.0 if math.isfinite(overlap) else math.nan,
                "n_daughter": len(daughter),
                "n_non_daughter": len(non_daughter),
                "n_bins": n_bins,
            }
        )

    return (
        pd.DataFrame(results)
        .sort_values("overlap_index", ascending=True, na_position="last")
        .reset_index(drop=True)
    )


def configure_plot_style() -> None:
    candidate_dirs = [
        "/usr/share/fonts/truetype/msttcorefonts",
        os.path.expanduser("~/.local/share/fonts/windows"),
        "/mnt/c/Windows/Fonts",
    ]
    for font_dir in candidate_dirs:
        if not os.path.isdir(font_dir):
            continue
        for filename in os.listdir(font_dir):
            if filename.lower().startswith("arial") and filename.lower().endswith((".ttf", ".ttc", ".otf")):
                try:
                    fm.fontManager.addfont(os.path.join(font_dir, filename))
                except Exception:
                    pass

    available = {font.name for font in fm.fontManager.ttflist}
    family = FONT_FAMILY if FONT_FAMILY in available else "DejaVu Sans"
    plt.rcParams.update(
        {
            "font.family": family,
            "font.size": 14,
            "axes.labelsize": 15,
            "axes.titlesize": 16,
            "xtick.labelsize": 13,
            "ytick.labelsize": 13,
            "axes.linewidth": 1.0,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
            "savefig.facecolor": "white",
            "figure.facecolor": "white",
        }
    )


def draw_overlap_figure(
    plot_data: pd.DataFrame,
    title: str,
    output_stem: Path,
    dpi: int,
    export_svg: bool,
) -> None:
    plot_data = plot_data.dropna(subset=["overlap_percent"]).copy()
    n_rows = len(plot_data)
    if n_rows == 0:
        raise ValueError("No finite overlap indices are available for plotting.")

    fig_height = max(6.0, 0.38 * n_rows + 2.2)
    fig, ax = plt.subplots(figsize=(12.0, fig_height))
    y = np.arange(n_rows)
    colors = [COLOR_HIGHLIGHT if i < min(10, n_rows) else COLOR_MAIN for i in range(n_rows)]
    values = plot_data["overlap_percent"].to_numpy(dtype=float)

    ax.barh(y, values, height=0.76, color=colors, edgecolor="none")
    ax.set_yticks(y)
    ax.set_yticklabels(plot_data["plot_label"].tolist())
    ax.invert_yaxis()
    ax.set_xlim(0.0, 105.0)
    ax.set_xticks([0, 25, 50, 75, 100])
    ax.xaxis.set_major_formatter(FormatStrFormatter("%.0f%%"))
    ax.xaxis.tick_top()
    ax.xaxis.set_label_position("top")
    ax.set_xlabel("Overlap index (%)", labelpad=10)
    ax.set_title(title, pad=24, color=COLOR_TEXT)
    ax.grid(axis="x", color=COLOR_GRID, linewidth=0.8, alpha=0.8)
    ax.set_axisbelow(True)
    ax.spines["right"].set_visible(False)
    ax.spines["bottom"].set_visible(False)
    ax.spines["left"].set_visible(False)
    ax.spines["top"].set_color(COLOR_SPINE)
    ax.tick_params(axis="y", length=0, colors=COLOR_TEXT)

    for yi, value in zip(y, values):
        ax.text(value + 1.1, yi, f"{value:.1f}%", va="center", ha="left", fontsize=12, color=COLOR_TEXT)

    fig.subplots_adjust(left=0.40, right=0.94, top=0.88, bottom=0.04)
    fig.savefig(output_stem.with_suffix(".png"), dpi=dpi, bbox_inches="tight")
    if export_svg:
        fig.savefig(output_stem.with_suffix(".svg"), bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    args = parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)

    filtered, observed_col, skipped_exception = load_and_filter_input(
        args.input,
        args.exception_column,
        args.exception_value,
        args.observed_column,
    )
    overlap_df = calculate_overlap_indices(
        filtered,
        observed_col,
        args.observed_positive_value,
        args.bins,
    )

    output_csv = args.out_dir / "overlap_indices.csv"
    overlap_df.to_csv(output_csv, index=False, float_format="%.10g")

    configure_plot_style()
    finite = overlap_df.dropna(subset=["overlap_percent"])
    draw_overlap_figure(
        finite.head(max(1, args.top)),
        f"Top {min(args.top, len(finite))} features by lowest overlap index",
        args.out_dir / "top10_overlap_index",
        args.dpi,
        args.svg,
    )
    draw_overlap_figure(
        finite,
        "All features by overlap index",
        args.out_dir / "all_features_overlap_index",
        args.dpi,
        args.svg,
    )

    observed = filtered[observed_col]
    n_daughter = int(observed.eq(float(args.observed_positive_value)).sum())
    n_non_daughter = int((observed.notna() & ~observed.eq(float(args.observed_positive_value))).sum())
    print(f"Input rows: {len(filtered) + skipped_exception}")
    print(f"Excluded exception rows before analysis: {skipped_exception}")
    print(f"Analyzed rows: {len(filtered)} ({n_daughter} daughter, {n_non_daughter} non-daughter)")
    print(f"Features analyzed: {len(overlap_df)}")
    print(f"Results: {args.out_dir}")


if __name__ == "__main__":
    main()
