#!/usr/bin/env python3
"""
Combined publication-style plotting script for:
1. A top-10 composite figure with overlap index (top) and best F1 (bottom)
2. An all-features composite figure with overlap index on the left and
   all F1 classifiers split into two aligned columns on the right.

Plot labels use the human-readable feature names defined in Supplementary Table S1; internal calculation/file variable names are left unchanged.
"""

import os
import re
from typing import Optional, Tuple

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.ticker import FormatStrFormatter
import matplotlib.font_manager as fm
from matplotlib.gridspec import GridSpec

# =============================================================================
# USER PARAMETERS
# =============================================================================
F1_CSV_PATH = "best_mwm_f1_summary.csv"
OVERLAP_CSV_PATH = "SAM_6samples_neighbor_pair_geometry.csv"
OUT_DIR = "feature_summary_f1_overlap"
DPI = 300
ALL_FEATURES_DPI = 300  # Higher resolution for the all-features composite figure
EXPORT_SVG = False

# ----- shared typography / styling -----
FONT_FAMILY = "Arial"
BASE_FONT_SIZE = 16
AXIS_LABEL_SIZE = 16
TICK_LABEL_SIZE = 16
TITLE_SIZE = 16
VALUE_LABEL_SIZE = 16

COLOR_BAR_MAIN = "#6EA6CD"
COLOR_BAR_HIGHLIGHT = "#2E6F9E"
COLOR_BAR_LIGHT = "#C8DBE8"
COLOR_GRID = "#D9D9D9"
COLOR_TEXT = "#222222"
COLOR_SPINE = "#333333"

X_LIMIT = (0.0, 105.0)
X_TICKS = [0, 25, 50, 75, 100]
BAR_HEIGHT = 0.76
TITLE_PAD = 28

# ----- content -----
N_TOP = 10
N_BINS = 40
USE_FIXED_RANGE = False
FIXED_RANGE = None
USE_OPERATOR_SYMBOLS = True

IGNORE_COLS = {
    "fileName", "filename",
    "pairIndex", "pair_index",
    "firstPolygonId", "first_polygon_id", "cell_1_id",
    "secondPolygonId", "second_polygon_id", "cell_2_id",
    "division_timing", "estimated_division",
    "exception_label", "exception",
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


# =============================================================================
# HELPERS
# =============================================================================

def try_register_arial_fonts():
    candidate_dirs = [
        "/usr/share/fonts/truetype/msttcorefonts",
        os.path.expanduser("~/.local/share/fonts/windows"),
        "/mnt/c/Windows/Fonts",
    ]
    for font_dir in candidate_dirs:
        if os.path.isdir(font_dir):
            for fname in os.listdir(font_dir):
                low = fname.lower()
                if low.startswith("arial") and low.endswith((".ttf", ".ttc", ".otf")):
                    try:
                        fm.fontManager.addfont(os.path.join(font_dir, fname))
                    except Exception:
                        pass


def set_publication_rcparams():
    try_register_arial_fonts()
    plt.rcParams.update({
        "font.family": FONT_FAMILY,
        "font.size": BASE_FONT_SIZE,
        "axes.labelsize": AXIS_LABEL_SIZE,
        "axes.titlesize": TITLE_SIZE,
        "xtick.labelsize": TICK_LABEL_SIZE,
        "ytick.labelsize": TICK_LABEL_SIZE,
        "xtick.direction": "out",
        "ytick.direction": "out",
        "xtick.major.width": 1.0,
        "ytick.major.width": 1.0,
        "xtick.major.size": 4,
        "ytick.major.size": 3,
        "axes.linewidth": 1.0,
        "pdf.fonttype": 42,
        "ps.fonttype": 42,
        "savefig.facecolor": "white",
        "figure.facecolor": "white",
    })


def standardized_feature_label(raw_name: str) -> str:
    return FEATURE_NAME_MAP.get(raw_name, raw_name)


def save_figure(fig, output_path_no_ext: str, dpi: int = DPI):
    fig.savefig(f"{output_path_no_ext}.png", dpi=dpi, bbox_inches="tight")
    if EXPORT_SVG:
        fig.savefig(f"{output_path_no_ext}.svg", bbox_inches="tight")


def style_bar_axes(ax, xlabel: str):
    ax.spines["right"].set_visible(False)
    ax.spines["bottom"].set_visible(False)
    ax.spines["left"].set_visible(False)
    ax.spines["top"].set_visible(True)
    ax.spines["top"].set_color(COLOR_SPINE)

    ax.xaxis.set_ticks_position("top")
    ax.xaxis.set_label_position("top")
    ax.tick_params(axis="x", colors=COLOR_TEXT, top=True, bottom=False,
                   labeltop=True, labelbottom=False, pad=6)
    ax.tick_params(axis="y", length=0, colors=COLOR_TEXT)

    ax.set_xlim(*X_LIMIT)
    ax.set_xticks(X_TICKS)
    ax.xaxis.set_major_formatter(FormatStrFormatter("%.0f%%"))
    ax.set_xlabel(xlabel, labelpad=10)
    ax.grid(axis="x", color=COLOR_GRID, linewidth=0.8, alpha=0.8)
    ax.set_axisbelow(True)


def add_bar_value_labels(ax, values_percent, ypos, offset=1.2):
    for yi, value in zip(ypos, values_percent):
        if not np.isfinite(value):
            continue
        ax.text(value + offset, yi, f"{value:.1f}%", va="center", ha="left",
                fontsize=VALUE_LABEL_SIZE, color=COLOR_TEXT, clip_on=False)


def draw_bar_panel(ax, plot_df: pd.DataFrame, value_col: str, xlabel: str,
                   title: Optional[str], highlight_top_n: int, show_values: bool,
                   value_offset: float = 1.2, ytick_labelsize: Optional[int] = None):
    n = len(plot_df)
    ypos = np.arange(n)

    colors = [
        COLOR_BAR_HIGHLIGHT if i < min(highlight_top_n, n) else COLOR_BAR_LIGHT
        for i in range(n)
    ]
    if highlight_top_n >= n:
        colors = [COLOR_BAR_HIGHLIGHT] + [COLOR_BAR_MAIN] * max(0, n - 1)

    ax.barh(
        ypos,
        plot_df[value_col].to_numpy(),
        height=BAR_HEIGHT,
        color=colors,
        edgecolor="none",
    )
    ax.set_yticks(ypos)
    ax.set_yticklabels(plot_df["plot_label"].tolist())
    if ytick_labelsize is not None:
        ax.tick_params(axis="y", labelsize=ytick_labelsize)
    ax.invert_yaxis()
    ax.set_ylim(n - 0.5, -0.5)

    if show_values:
        add_bar_value_labels(ax, plot_df[value_col].to_numpy(), ypos, offset=value_offset)

    style_bar_axes(ax, xlabel=xlabel)
    if title:
        ax.set_title(title, pad=TITLE_PAD, color=COLOR_TEXT)

# =============================================================================
# DATA PREPARATION
# =============================================================================

def parse_classifier_file(file_name: str) -> Tuple[str, str]:
    base = str(file_name).strip().rsplit("/", 1)[-1]
    m = re.match(r"^\d+_(.+?)__(ge|le)\.csv$", base, flags=re.IGNORECASE)
    if not m:
        return base, ""
    return m.group(1), m.group(2).lower()


def make_f1_all_label(file_name: str) -> str:
    raw_feat, direction = parse_classifier_file(file_name)
    feat = standardized_feature_label(raw_feat)
    if not direction:
        return feat
    op = "≥" if direction == "ge" else "≤"
    return f"{feat} {op}" if USE_OPERATOR_SYMBOLS else f"{feat} {'>=' if direction == 'ge' else '<='}"


def make_f1_top_label(file_name: str) -> str:
    raw_feat, _ = parse_classifier_file(file_name)
    return standardized_feature_label(raw_feat)


def compute_f1_df() -> pd.DataFrame:
    df = pd.read_csv(F1_CSV_PATH).copy()
    required = {"file", "best_mwm_f1"}
    missing = required - set(df.columns)
    if missing:
        raise ValueError(f"Missing required column(s) in F1 CSV: {missing}")

    df["best_mwm_f1"] = pd.to_numeric(df["best_mwm_f1"], errors="coerce")
    df = df.dropna(subset=["best_mwm_f1"]).copy()

    df["plot_label_all"] = df["file"].apply(make_f1_all_label)
    df["plot_label_top"] = df["file"].apply(make_f1_top_label)
    df["best_mwm_f1_percent"] = df["best_mwm_f1"] * 100.0
    df = df.sort_values("best_mwm_f1", ascending=False).reset_index(drop=True)
    return df


def overlap_from_hist_probs(p1: np.ndarray, p2: np.ndarray) -> float:
    return float(np.sum(np.minimum(p1, p2)))


def compute_overlap_df() -> pd.DataFrame:
    df = pd.read_csv(OVERLAP_CSV_PATH)
    if "observed_division" not in df.columns:
        raise ValueError("Missing required column 'observed_division' in overlap CSV.")

    # Remove unresolved tracking records before any overlap-index calculation.
    # This matches the analysis-ready dataset used elsewhere in the manuscript.
    exception_col = None
    for candidate in ("exception_label", "exception"):
        if candidate in df.columns:
            exception_col = candidate
            break

    if exception_col is not None:
        exception_numeric = pd.to_numeric(df[exception_col], errors="coerce")
        df = df.loc[~exception_numeric.eq(1)].copy()

    # Metadata/label columns are excluded from the numeric feature set.
    df_work = df.drop(columns=[c for c in IGNORE_COLS if c in df.columns], errors="ignore").copy()
    numeric_cols = df_work.select_dtypes(include=[np.number]).columns.tolist()
    feature_cols = [c for c in numeric_cols if c != "observed_division"]

    daughter_mask = df_work["observed_division"] == 1
    non_mask = df_work["observed_division"] == 0

    results = []
    for feat in feature_cols:
        x_d = df_work.loc[daughter_mask, feat].dropna().to_numpy()
        x_n = df_work.loc[non_mask, feat].dropna().to_numpy()

        if len(x_d) < 2 or len(x_n) < 2:
            overlap = np.nan
        else:
            all_x = np.concatenate([x_d, x_n])
            if USE_FIXED_RANGE and FIXED_RANGE is not None:
                vmin, vmax = FIXED_RANGE
            else:
                vmin, vmax = float(np.min(all_x)), float(np.max(all_x))

            if np.isclose(vmin, vmax):
                overlap = 1.0
            else:
                bins = np.linspace(vmin, vmax, N_BINS + 1)
                c_d, _ = np.histogram(x_d, bins=bins)
                c_n, _ = np.histogram(x_n, bins=bins)
                p_d = c_d / c_d.sum() if c_d.sum() > 0 else np.zeros_like(c_d, dtype=float)
                p_n = c_n / c_n.sum() if c_n.sum() > 0 else np.zeros_like(c_n, dtype=float)
                overlap = overlap_from_hist_probs(p_d, p_n)

        results.append({
            "feature": feat,
            "plot_label": standardized_feature_label(feat),
            "overlap_index": overlap,
            "overlap_percent": overlap * 100.0 if pd.notna(overlap) else np.nan,
            "n_daughter": len(x_d),
            "n_non_daughter": len(x_n),
        })

    overlap_df = pd.DataFrame(results)
    overlap_df = overlap_df.sort_values(by="overlap_index", ascending=True, na_position="last").reset_index(drop=True)
    return overlap_df

# =============================================================================
# FIGURE BUILDERS
# =============================================================================

def make_top10_composite(f1_df: pd.DataFrame, overlap_df: pd.DataFrame):
    overlap_top = overlap_df.dropna(subset=["overlap_percent"]).head(N_TOP).copy()
    f1_top = f1_df.head(N_TOP).copy()
    f1_top["plot_label"] = f1_top["plot_label_top"]

    fig = plt.figure(figsize=(12.0, 13.6))
    gs = GridSpec(2, 1, height_ratios=[1, 1], hspace=0.48)
    ax1 = fig.add_subplot(gs[0, 0])
    ax2 = fig.add_subplot(gs[1, 0])

    draw_bar_panel(
        ax1, overlap_top, "overlap_percent", "Overlap index (%)",
        "Top 10 features by lowest overlap index (%)",
        highlight_top_n=N_TOP, show_values=True, value_offset=1.2
    )
    draw_bar_panel(
        ax2, f1_top[["plot_label", "best_mwm_f1_percent"]], "best_mwm_f1_percent", "Best F1 score (%)",
        "Top 10 features by best F1 score (%)",
        highlight_top_n=N_TOP, show_values=True, value_offset=1.2
    )

    fig.subplots_adjust(left=0.40, right=0.92, top=0.96, bottom=0.05)
    save_figure(fig, os.path.join(OUT_DIR, "top10_overlap_and_f1_combined"))
    plt.close(fig)


def make_all_features_composite(f1_df: pd.DataFrame, overlap_df: pd.DataFrame):
    overlap_all = overlap_df.dropna(subset=["overlap_percent"]).copy().reset_index(drop=True)
    n_overlap = len(overlap_all)

    f1_all = f1_df.copy().reset_index(drop=True)
    f1_all["plot_label"] = f1_all["plot_label_all"]

    # Split F1 into two columns with the same number of rows as overlap features.
    # Here, 98 F1 classifiers become 49 rows x 2 columns, matching 49 overlap features.
    n_rows = n_overlap
    left_f1 = f1_all.iloc[:n_rows].copy()
    right_f1 = f1_all.iloc[n_rows:n_rows * 2].copy()

    def pad_df(df_part: pd.DataFrame, target_n: int, value_col: str) -> pd.DataFrame:
        """Pad a column to target_n rows so the two F1 columns align with the overlap column."""
        if len(df_part) >= target_n:
            return df_part.iloc[:target_n].copy()
        n_missing = target_n - len(df_part)
        pad = pd.DataFrame({
            "plot_label": [""] * n_missing,
            value_col: [np.nan] * n_missing,
        })
        return pd.concat([df_part[["plot_label", value_col]], pad], ignore_index=True)

    overlap_plot = overlap_all[["plot_label", "overlap_percent"]].copy()
    left_f1_plot = pad_df(left_f1, n_rows, "best_mwm_f1_percent")
    right_f1_plot = pad_df(right_f1, n_rows, "best_mwm_f1_percent")

    # Spacious layout to avoid label/title intersection.
    fig_h = max(20.0, 0.44 * n_rows + 3.4)
    fig = plt.figure(figsize=(38.0, fig_h))
    gs = GridSpec(1, 3, width_ratios=[1.55, 1.15, 1.15], wspace=0.52)
    ax_overlap = fig.add_subplot(gs[0, 0])
    ax_f1_left = fig.add_subplot(gs[0, 1])
    ax_f1_right = fig.add_subplot(gs[0, 2])

    # Use no axis title here. Both section titles are added below with fig.text(),
    # so overlap and F1 titles share exactly the same y-coordinate.
    draw_bar_panel(
        ax_overlap, overlap_plot, "overlap_percent", "Overlap index (%)",
        None,
        highlight_top_n=10, show_values=True, value_offset=1.2, ytick_labelsize=13
    )
    draw_bar_panel(
        ax_f1_left, left_f1_plot, "best_mwm_f1_percent", "Best F1 score (%)",
        None,
        highlight_top_n=10, show_values=True, value_offset=1.2, ytick_labelsize=12
    )
    draw_bar_panel(
        ax_f1_right, right_f1_plot, "best_mwm_f1_percent", "Best F1 score (%)",
        None,
        highlight_top_n=0, show_values=True, value_offset=1.2, ytick_labelsize=12
    )

    # Leave enough top margin for both section titles and the top x-axis labels.
    fig.subplots_adjust(left=0.16, right=0.99, top=0.91, bottom=0.03)

    # Get final axes positions after the layout has been adjusted.
    fig.canvas.draw()
    pos_overlap = ax_overlap.get_position()
    pos_f1_left = ax_f1_left.get_position()
    pos_f1_right = ax_f1_right.get_position()

    # Aligned section titles: same y-coordinate, centered over their respective panel groups.
    title_y = 0.975
    overlap_title_x = (pos_overlap.x0 + pos_overlap.x1) / 2
    f1_title_x = (pos_f1_left.x0 + pos_f1_right.x1) / 2

    fig.text(
        overlap_title_x, title_y,
        "All features by overlap index (%)",
        ha="center", va="top", fontsize=TITLE_SIZE, color=COLOR_TEXT
    )
    fig.text(
        f1_title_x, title_y,
        "All classifiers by best F1 score (%)",
        ha="center", va="top", fontsize=TITLE_SIZE, color=COLOR_TEXT
    )

    save_figure(
        fig,
        os.path.join(OUT_DIR, "all_features_overlap_and_f1_combined"),
        dpi=ALL_FEATURES_DPI,
    )
    plt.close(fig)

# =============================================================================
# MAIN
# =============================================================================

def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    set_publication_rcparams()

    f1_df = compute_f1_df()
    overlap_df = compute_overlap_df()

    # export tables
    f1_df.to_csv(os.path.join(OUT_DIR, "best_mwm_f1_summary_with_labels.csv"), index=False)
    overlap_df.to_csv(os.path.join(OUT_DIR, "overlap_indices.csv"), index=False)

    make_top10_composite(f1_df, overlap_df)
    make_all_features_composite(f1_df, overlap_df)

    print(f"Done. Results saved to: {OUT_DIR}")


if __name__ == "__main__":
    main()
