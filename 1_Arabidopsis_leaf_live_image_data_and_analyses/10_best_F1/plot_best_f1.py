#!/usr/bin/env python3
"""Plot best global-matching F1 scores calculated by calculate_best_f1.py.

The script is intentionally plot-only: it never reads the geometry dataset and
never recalculates thresholds or matching.  By default it reads
``best_f1_results/best_mwm_f1_per_feature.csv`` and creates top-10 and
all-feature figures.
"""

from __future__ import annotations

import argparse
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
DEFAULT_INPUT_CSV = SCRIPT_DIR / "best_f1_results" / "best_mwm_f1_per_feature.csv"
DEFAULT_OUTPUT_DIR = SCRIPT_DIR / "best_f1_figures"

N_TOP = 10
DPI = 300
FONT_FAMILY = "Arial"
COLOR_MAIN = "#6EA6CD"
COLOR_HIGHLIGHT = "#2E6F9E"
COLOR_GRID = "#D9D9D9"
COLOR_TEXT = "#222222"
COLOR_SPINE = "#333333"

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
    parser.add_argument("--top", type=int, default=N_TOP)
    parser.add_argument("--dpi", type=int, default=DPI)
    parser.add_argument("--svg", action="store_true", help="Also export SVG figures.")
    parser.add_argument(
        "--hide-operator",
        action="store_true",
        help="Omit the selected >= or <= direction from y-axis labels.",
    )
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


def standardize_feature_name(feature: str) -> str:
    return FEATURE_NAME_MAP.get(feature, feature)


def operator_symbol(operator: str) -> str:
    return {">=": "≥", "<=": "≤", ">": ">", "<": "<"}.get(operator.strip(), operator.strip())


def load_best_f1(input_csv: Path, show_operator: bool) -> pd.DataFrame:
    data = pd.read_csv(input_csv, encoding="utf-8-sig")
    feature_col = resolve_column(data.columns, "feature")
    f1_col = resolve_column(data.columns, "best_mwm_f1")
    try:
        op_col = resolve_column(data.columns, "op")
    except ValueError:
        op_col = ""

    data[f1_col] = pd.to_numeric(data[f1_col], errors="coerce")
    data = data.dropna(subset=[feature_col, f1_col]).copy()
    if data.empty:
        raise ValueError(f"No valid best-F1 rows were found in {input_csv}")

    # Accept either the per-feature file or the full two-direction summary.  If
    # both directions are present, retain the higher F1 for each feature.
    data = data.sort_values(f1_col, ascending=False, kind="mergesort")
    data = data.drop_duplicates(subset=[feature_col], keep="first").copy()
    data["feature"] = data[feature_col].astype(str)
    data["best_mwm_f1"] = data[f1_col].astype(float)

    if data["best_mwm_f1"].max() <= 1.000001:
        data["best_mwm_f1_percent"] = data["best_mwm_f1"] * 100.0
    else:
        data["best_mwm_f1_percent"] = data["best_mwm_f1"]

    labels: list[str] = []
    for _, row in data.iterrows():
        label = standardize_feature_name(str(row["feature"]))
        if show_operator and op_col:
            label = f"{label} {operator_symbol(str(row[op_col]))}"
        labels.append(label)
    data["plot_label"] = labels
    return data.sort_values("best_mwm_f1_percent", ascending=False).reset_index(drop=True)


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


def draw_f1_figure(
    plot_data: pd.DataFrame,
    title: str,
    output_stem: Path,
    dpi: int,
    export_svg: bool,
) -> None:
    n_rows = len(plot_data)
    if n_rows == 0:
        raise ValueError("There are no F1 values to plot.")

    fig_height = max(6.0, 0.38 * n_rows + 2.2)
    fig, ax = plt.subplots(figsize=(12.0, fig_height))
    y = np.arange(n_rows)
    values = plot_data["best_mwm_f1_percent"].to_numpy(dtype=float)
    colors = [COLOR_HIGHLIGHT if i < min(10, n_rows) else COLOR_MAIN for i in range(n_rows)]

    ax.barh(y, values, height=0.76, color=colors, edgecolor="none")
    ax.set_yticks(y)
    ax.set_yticklabels(plot_data["plot_label"].tolist())
    ax.invert_yaxis()
    ax.set_xlim(0.0, 105.0)
    ax.set_xticks([0, 25, 50, 75, 100])
    ax.xaxis.set_major_formatter(FormatStrFormatter("%.0f%%"))
    ax.xaxis.tick_top()
    ax.xaxis.set_label_position("top")
    ax.set_xlabel("Best F1 score (%)", labelpad=10)
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

    fig.subplots_adjust(left=0.42, right=0.94, top=0.88, bottom=0.04)
    fig.savefig(output_stem.with_suffix(".png"), dpi=dpi, bbox_inches="tight")
    if export_svg:
        fig.savefig(output_stem.with_suffix(".svg"), bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    args = parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    data = load_best_f1(args.input, show_operator=not args.hide_operator)
    configure_plot_style()

    top_n = min(max(1, args.top), len(data))
    draw_f1_figure(
        data.head(top_n),
        f"Top {top_n} features by best F1 score",
        args.out_dir / "top10_best_f1",
        args.dpi,
        args.svg,
    )
    draw_f1_figure(
        data,
        "All features by best F1 score",
        args.out_dir / "all_features_best_f1",
        args.dpi,
        args.svg,
    )

    print(f"Features plotted: {len(data)}")
    print(f"Results: {args.out_dir}")


if __name__ == "__main__":
    main()
