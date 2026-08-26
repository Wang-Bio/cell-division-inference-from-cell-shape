"""

This code analyzes bimodality in a mean-junction-angle distribution on [0, 180].
It will do:
1. separate fitted low-angle and high-angle components
2. find their model-derived intersection cutoff
3. report seven cutoff-based component-separation terms:
   - high-angle component above cutoff
   - low-angle component above cutoff
   - high-angle component below cutoff
   - low-angle component below cutoff
   - high-angle fraction above cutoff
   - high-angle retention
   - component-separation score
4. if a supported observed-label column contains real daughter labels (=1),
   optionally calculate component-to-label distribution closeness
5. if a supported observed-label column exists, also plot four raw labeled
   daughter/non-daughter distributions (count/frequency, each with/without a
   best-threshold line) without fitting curves
5. if a supported exception column exists, exclude exception rows before all analyses

The input CSV can be supplied as the first command-line argument. The script
automatically resolves the feature, observed-label, and exception columns used
by the four manuscript datasets.

Library Requirements:
 pip install numpy pandas matplotlib scipy

"""

import argparse
import json
import re

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path
from scipy.special import i0
from scipy.stats import ks_2samp, wasserstein_distance
from scipy.optimize import brentq

# Settings

# 1) Input file and target column
INPUT_CSV = r"Oxalis_corniculata_4samples_neighbor_geometry.csv"
TARGET_COLUMN = "junctionAngleAverageDegrees"

# Accepted schema aliases. Matching is case-insensitive and ignores spaces,
# underscores, and punctuation.
TARGET_COLUMN_ALIASES = (
    "junctionAngleAverageDegrees",
    "junction_angle_average_degrees",
    "junction_angle_mean",
    "mean_junction_angle",
)

# New neighbor-pair geometry CSVs store exclusions directly in this column.
# Rows with exception_label == 1 are removed before fitting, cutoff estimation,
# observed-label analysis, KS tests, counts, posterior export, and plotting.
# Older CSVs without this column remain supported and are analyzed unchanged.
EXCLUDE_EXCEPTION_ROWS = True
EXCEPTION_COLUMN = "exception_label"
EXCEPTION_COLUMN_ALIASES = ("exception_label", "exception")
EXCEPTION_POSITIVE_VALUE = 1

# 2) Output folder and output files
# All exported CSVs and figures will be saved under RESULT_DIR.
RESULT_DIR = "result"
OUT_FIG = "vonmises_mixture_resampled.png"
OUT_FIG_OVERALL = None        # None = automatically add "_overall_grey.png" to OUT_FIG
OUT_FIG_OVERALL_RAW = None    # None = automatically add "_overall_raw_grey.png" to OUT_FIG
# Four raw observed-label distributions requested for daughter/non-daughter pairs.
OUT_FIG_OBSERVED_COUNT = "observed_division_distribution_pair_count.png"
OUT_FIG_OBSERVED_FREQUENCY = "observed_division_distribution_pair_frequency.png"
OUT_FIG_OBSERVED_COUNT_CUTOFF = "observed_division_distribution_pair_count_with_cutoff.png"
OUT_FIG_OBSERVED_FREQUENCY_CUTOFF = "observed_division_distribution_pair_frequency_with_cutoff.png"

# Legacy fitted observed-label plot. The function remains available, but this
# output is disabled by default because the four new figures show raw data only.
OUT_FIG_OBSERVED_LABELS = "observed_division_labeled_vonmises_distribution.png"
OUT_OBSERVED_LABEL_FIT_CSV = "observed_division_labeled_vonmises_fit_parameters.csv"
OUT_POST_CSV = "vonmises2_posteriors_realdata.csv"
OUT_COMPONENT_SUMMARY_CSV = "vonmises_cutoff_component_summary.csv"
OUT_SEVEN_TERMS_CSV = "component_separation_seven_terms.csv"
OUT_KS_JSON = "ks_group_comparisons.json"
OUT_CLOSENESS_CSV = "component_label_distribution_closeness_summary.csv"
OUT_CLOSENESS_BOOTSTRAP_CSV = "component_label_distribution_closeness_bootstrap.csv"
OUT_CLOSENESS_FIG = "component_label_distribution_closeness_ecdf.png"

# Optional real-label plotting. If OBSERVED_DIVISION_COLUMN exists and contains
# both 0 and 1, the script saves four raw daughter/non-daughter histograms.
PLOT_OBSERVED_LABELS_IF_AVAILABLE = True
OBSERVED_DIVISION_COLUMN = "auto"
OBSERVED_DIVISION_COLUMN_ALIASES = (
    "observed_division",
    "is_real_division",
    "division_real",
    "real_division",
)

# Threshold drawn in the two "with_cutoff" raw-label figures. Use 145.0 for the
# supervised F1-optimized threshold. Set this to None to use the mixture-model
# cutoff calculated from the current input instead.
OBSERVED_LABEL_BEST_THRESHOLD = 145.0
OBSERVED_LABEL_CUTOFF_COLOR = "#808080"

# Set True only when the older, separately fitted one-component curves are also
# wanted. It is False by default so the requested label plots contain no fits.
PLOT_OBSERVED_LABEL_FITS_IF_AVAILABLE = False

# Posterior-weighted component-versus-label distribution-closeness analysis.
# This analysis is run ONLY when the observed-label column contains both
# real daughter labels (=1) and non-daughter labels (=0). Fixed-sample CSVs
# that contain no real division label=1 automatically skip all closeness
# calculations and closeness outputs.
RUN_COMPONENT_LABEL_CLOSENESS = True

# Resample tracked primordia rather than individual neighboring pairs. For this
# dataset, fileName values such as sample4_20h.json are reduced to sample4.
CLOSENESS_BOOTSTRAP_REPLICATES = 1000
CLOSENESS_BOOTSTRAP_GROUP_COLUMN = "fileName"
CLOSENESS_BOOTSTRAP_GROUP_REGEX = r"^(.+?)(?:[_-]\d+(?:\.\d+)?h)?(?:\.json)?$"
CLOSENESS_BOOTSTRAP_MAX_ITER = 200
CLOSENESS_BOOTSTRAP_SEED = 0

# 3) Figure size and labels
FIG_WIDTH = 6.5
ASPECT_RATIO = 1.80
X_AXIS_LABEL = "Mean junction angle (°)"
Y_AXIS_LABEL = "Pair count"
SHOW_FIGURE_TITLES = False   # publication-style figures usually use captions, not internal titles

# 4) Figure colors
# Main line colors use the user-requested palette. Histogram fills are lighter
# tints near the same colors for a clean Science/Nature/Cell-like style.
COLOR_NON_DAUGHTER_LINE = "#4C79A2"   # blue: non-daughter / low-angle component
COLOR_DAUGHTER_LINE = "#DA6752"       # red: daughter / high-angle component
COLOR_NON_DAUGHTER_HIST = "#89A7C3"   # darker blue histogram tint
COLOR_DAUGHTER_HIST = "#E6A08F"       # darker red histogram tint
COLOR_OVERALL_HIST = "#B3B3B3"        # darker neutral grey bars
COLOR_OVERALL_CURVE = "#333333"       # dark grey mixture line
CUTOFF_LINE_COLOR = "#1A1A1A"


# 5) Histogram and axis settings
N_BINS = 35
USE_FIXED_RANGE = False       # True = use FIXED_MIN/FIXED_MAX for bins
FIXED_MIN = 0.0
FIXED_MAX = 180.0
XLIM_MIN = 60.0
XLIM_MAX = 180.0

# 5b) Shared y-axis setting for series comparison
# If USE_SHARED_YMAX is True and FIXED_YMAX is None, the script automatically
# computes one common y-maximum across all figures generated in the current run.
# If you want exactly the same ymax across different datasets / different runs,
# set FIXED_YMAX to a number (for example 2000).
USE_SHARED_YMAX = True
FIXED_YMAX = None
YMAX_MARGIN = 1.08

# 5c) Legend placement
LEGEND_ANCHOR_X = 1.02
LEGEND_LOC = "upper left"

# 5d) Fixed main plot panel size
# FIG_WIDTH now controls the width of the main plotting axes only.
# The right legend area is added outside this width, so changing legend length
# will not change the main plot aspect ratio.
LEGEND_AREA_WIDTH = 2.70
LEFT_MARGIN_WIDTH = 0.70
BOTTOM_MARGIN_HEIGHT = 0.55
TOP_MARGIN_HEIGHT = 0.18

# 6) EM fitting settings
N_INIT = 15
MAX_ITER = 400

# 7) Resampling setting
RESAMPLE_N = 0

# 8) Reproducibility and output quality
SEED = 0
DPI = 600
SHOW_PLOTS = False            # False = save figures without opening plot windows


RUN_CONFIG = dict(
    csv=INPUT_CSV,
    result_dir=RESULT_DIR,
    xcol=TARGET_COLUMN,
    exclude_exception_rows=EXCLUDE_EXCEPTION_ROWS,
    exception_column=EXCEPTION_COLUMN,
    exception_positive_value=EXCEPTION_POSITIVE_VALUE,
    seed=SEED,
    dpi=DPI,
    aspect_ratio=ASPECT_RATIO,
    fig_width=FIG_WIDTH,
    x_axis_label=X_AXIS_LABEL,
    y_axis_label=Y_AXIS_LABEL,
    show_figure_titles=SHOW_FIGURE_TITLES,
    color_non_daughter=COLOR_NON_DAUGHTER_LINE,
    color_daughter=COLOR_DAUGHTER_LINE,
    color_non_daughter_hist=COLOR_NON_DAUGHTER_HIST,
    color_daughter_hist=COLOR_DAUGHTER_HIST,
    color_overall_hist=COLOR_OVERALL_HIST,
    color_overall_curve=COLOR_OVERALL_CURVE,
    cutoff_line_color=CUTOFF_LINE_COLOR,
    n_bins=N_BINS,
    use_fixed_range=USE_FIXED_RANGE,
    fixed_min=FIXED_MIN,
    fixed_max=FIXED_MAX,
    xlim_min=XLIM_MIN,
    xlim_max=XLIM_MAX,
    use_shared_ymax=USE_SHARED_YMAX,
    fixed_ymax=FIXED_YMAX,
    ymax_margin=YMAX_MARGIN,
    legend_anchor_x=LEGEND_ANCHOR_X,
    legend_loc=LEGEND_LOC,
    legend_area_width=LEGEND_AREA_WIDTH,
    left_margin_width=LEFT_MARGIN_WIDTH,
    bottom_margin_height=BOTTOM_MARGIN_HEIGHT,
    top_margin_height=TOP_MARGIN_HEIGHT,
    n_init=N_INIT,
    max_iter=MAX_ITER,
    resample_n=RESAMPLE_N,
    out_fig=OUT_FIG,
    out_fig_overall=OUT_FIG_OVERALL,
    out_fig_overall_raw=OUT_FIG_OVERALL_RAW,
    out_fig_observed_count=OUT_FIG_OBSERVED_COUNT,
    out_fig_observed_frequency=OUT_FIG_OBSERVED_FREQUENCY,
    out_fig_observed_count_cutoff=OUT_FIG_OBSERVED_COUNT_CUTOFF,
    out_fig_observed_frequency_cutoff=OUT_FIG_OBSERVED_FREQUENCY_CUTOFF,
    observed_label_best_threshold=OBSERVED_LABEL_BEST_THRESHOLD,
    observed_label_cutoff_color=OBSERVED_LABEL_CUTOFF_COLOR,
    out_fig_observed_labels=OUT_FIG_OBSERVED_LABELS,
    out_observed_label_fit_csv=OUT_OBSERVED_LABEL_FIT_CSV,
    plot_observed_labels_if_available=PLOT_OBSERVED_LABELS_IF_AVAILABLE,
    plot_observed_label_fits_if_available=PLOT_OBSERVED_LABEL_FITS_IF_AVAILABLE,
    observed_division_column=OBSERVED_DIVISION_COLUMN,
    run_component_label_closeness=RUN_COMPONENT_LABEL_CLOSENESS,
    closeness_bootstrap_replicates=CLOSENESS_BOOTSTRAP_REPLICATES,
    closeness_bootstrap_group_column=CLOSENESS_BOOTSTRAP_GROUP_COLUMN,
    closeness_bootstrap_group_regex=CLOSENESS_BOOTSTRAP_GROUP_REGEX,
    closeness_bootstrap_max_iter=CLOSENESS_BOOTSTRAP_MAX_ITER,
    closeness_bootstrap_seed=CLOSENESS_BOOTSTRAP_SEED,
    out_closeness_csv=OUT_CLOSENESS_CSV,
    out_closeness_bootstrap_csv=OUT_CLOSENESS_BOOTSTRAP_CSV,
    out_closeness_fig=OUT_CLOSENESS_FIG,
    out_post_csv=OUT_POST_CSV,
    out_component_summary_csv=OUT_COMPONENT_SUMMARY_CSV,
    out_seven_terms_csv=OUT_SEVEN_TERMS_CSV,
    out_ks_json=OUT_KS_JSON,
    show_plots=SHOW_PLOTS,
)

# =============================================================================
# End of user settings
# =============================================================================

def _normalized_column_key(name: object) -> str:
    """Normalize a column name for schema-tolerant matching."""
    return re.sub(r"[^a-z0-9]+", "", str(name).casefold())


def resolve_column_name(
    df: pd.DataFrame,
    requested: str | None,
    aliases: tuple[str, ...] = (),
    *,
    required: bool,
    role: str,
) -> str | None:
    """Resolve a CSV column using exact, case-insensitive, and normalized aliases."""
    candidates: list[str] = []
    if requested is not None and str(requested).strip().casefold() not in {"", "auto"}:
        candidates.append(str(requested))
    candidates.extend(str(alias) for alias in aliases)

    exact_lookup = {str(column): str(column) for column in df.columns}
    casefold_lookup = {str(column).casefold(): str(column) for column in df.columns}
    normalized_lookup = {
        _normalized_column_key(column): str(column) for column in df.columns
    }

    for candidate in candidates:
        if candidate in exact_lookup:
            return exact_lookup[candidate]
        resolved = casefold_lookup.get(candidate.casefold())
        if resolved is not None:
            return resolved
        resolved = normalized_lookup.get(_normalized_column_key(candidate))
        if resolved is not None:
            return resolved

    if required:
        raise ValueError(
            f"Could not resolve the {role}. Requested={requested!r}; "
            f"accepted aliases={list(aliases)}; available columns={list(df.columns)}"
        )
    return None


def exclude_exception_labeled_rows(
    df: pd.DataFrame,
    exception_column: str = "exception_label",
    exception_aliases: tuple[str, ...] = EXCEPTION_COLUMN_ALIASES,
    exception_positive_value: int | float | str = 1,
    enabled: bool = True,
) -> tuple[pd.DataFrame, int]:
    """
    Remove exception-labelled rows before any analysis.

    If filtering is disabled or no supported exception column is present,
    return the input unchanged.
    """
    if not enabled:
        print("Exception filtering disabled; all input rows will be analyzed.")
        return df.copy(), 0

    resolved_column = resolve_column_name(
        df,
        requested=exception_column,
        aliases=exception_aliases,
        required=False,
        role="exception column",
    )
    if resolved_column is None:
        print(
            f"Exception column '{exception_column}' was not found; "
            "analyzing all input rows for backward compatibility."
        )
        return df.copy(), 0

    raw = df[resolved_column]
    numeric = pd.to_numeric(raw, errors="coerce")
    try:
        numeric_target = float(exception_positive_value)
        exception_mask = numeric.eq(numeric_target)
    except (TypeError, ValueError):
        exception_mask = pd.Series(False, index=df.index)

    # Also support nonnumeric labels when a string positive value is configured.
    string_target = str(exception_positive_value).strip().casefold()
    string_mask = raw.astype("string").str.strip().str.casefold().eq(string_target).fillna(False)
    exception_mask = exception_mask.fillna(False) | string_mask

    excluded_count = int(exception_mask.sum())
    filtered = df.loc[~exception_mask].copy().reset_index(drop=True)
    print(
        f"Exception filtering: excluded {excluded_count} row(s) where "
        f"'{resolved_column}' == {exception_positive_value}; "
        f"{len(filtered)} row(s) remain."
    )
    return filtered, excluded_count

def angle_deg_to_theta_rad(angle_deg: np.ndarray) -> np.ndarray:
    return (2.0 * np.deg2rad(angle_deg)) % (2.0 * np.pi)


def theta_rad_to_angle_deg(theta_rad: np.ndarray) -> np.ndarray:
    ang = np.rad2deg(theta_rad) / 2.0
    return ang % 180.0

def vonmises_pdf(theta: np.ndarray, mu: float, kappa: float) -> np.ndarray:
    return np.exp(kappa * np.cos(theta - mu)) / (2.0 * np.pi * i0(kappa))

def integrate_trapezoid(y: np.ndarray, x: np.ndarray) -> float:
    """Compatibility wrapper for NumPy 1.x and 2.x."""
    if hasattr(np, "trapezoid"):
        return float(np.trapezoid(y, x))
    return float(np.trapz(y, x))

def kappa_from_R(R: np.ndarray) -> np.ndarray:
    R = np.clip(R, 1e-8, 1 - 1e-8)
    kappa = np.empty_like(R)

    mask1 = R < 0.53
    mask2 = (R >= 0.53) & (R < 0.85)
    mask3 = R >= 0.85

    kappa[mask1] = 2 * R[mask1] + R[mask1] ** 3 + (5 * R[mask1] ** 5) / 6
    kappa[mask2] = -0.4 + 1.39 * R[mask2] + 0.43 / (1 - R[mask2])
    kappa[mask3] = 1 / (R[mask3] ** 3 - 4 * R[mask3] ** 2 + 3 * R[mask3])

    return np.clip(kappa, 1e-6, 1e6)

def fit_vonmises_mixture(
    theta: np.ndarray,
    K: int = 2,
    n_init: int = 10,
    max_iter: int = 300,
    tol: float = 1e-6,
    seed: int = 0,
):
    rng = np.random.default_rng(seed)
    N = theta.size

    best = None
    best_ll = -np.inf

    sin_t = np.sin(theta)
    cos_t = np.cos(theta)

    for _init in range(n_init):
        mus = rng.choice(theta, size=K, replace=False)
        kappas = np.full(K, 5.0)
        weights = np.full(K, 1.0 / K)

        prev_ll = -np.inf
        for _it in range(max_iter):
            log_pdf = np.empty((N, K), dtype=float)
            for k in range(K):
                log_norm = np.log(2.0 * np.pi) + np.log(i0(kappas[k]))
                log_pdf[:, k] = (
                    np.log(weights[k] + 1e-300)
                    + kappas[k] * np.cos(theta - mus[k])
                    - log_norm
                )

            m = np.max(log_pdf, axis=1, keepdims=True)
            probs = np.exp(log_pdf - m)
            denom = np.sum(probs, axis=1, keepdims=True) + 1e-300
            resp = probs / denom

            ll = np.sum(m[:, 0] + np.log(denom[:, 0]))
            if np.abs(ll - prev_ll) < tol:
                break
            prev_ll = ll

            Nk = resp.sum(axis=0) + 1e-300
            weights = Nk / N

            S = resp.T @ sin_t
            C = resp.T @ cos_t
            mus = np.arctan2(S, C) % (2.0 * np.pi)

            Rbar = np.sqrt(S**2 + C**2) / Nk
            kappas = kappa_from_R(Rbar)

        if ll > best_ll:
            best_ll = ll
            best = (weights.copy(), mus.copy(), kappas.copy(), ll)

    return best


# Plot styling
def setup_publication_style():
    """Set a clean journal-like matplotlib style."""
    plt.rcParams.update({
        "font.family": "sans-serif",
        "font.sans-serif": ["Arial", "Helvetica", "DejaVu Sans"],
        "font.size": 8,
        "axes.labelsize": 9,
        "axes.titlesize": 9,
        "axes.linewidth": 0.8,
        "xtick.labelsize": 8,
        "ytick.labelsize": 8,
        "xtick.major.size": 3.2,
        "ytick.major.size": 3.2,
        "xtick.major.width": 0.8,
        "ytick.major.width": 0.8,
        "legend.fontsize": 7.5,
        "legend.handlelength": 1.7,
        "legend.borderaxespad": 0.6,
        "figure.facecolor": "white",
        "axes.facecolor": "white",
        "savefig.facecolor": "white",
        "savefig.bbox": "standard",
        "pdf.fonttype": 42,
        "ps.fonttype": 42,
    })


def style_ax(ax):
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    for spine in ("left", "bottom"):
        ax.spines[spine].set_linewidth(0.8)
    ax.tick_params(axis="both", which="major", direction="out", length=3.2, width=0.8, pad=2)
    ax.grid(False)
    ax.margins(x=0.01)


def add_clean_legend(ax, loc="upper left", anchor_x: float = 1.02):
    """Place the legend outside the axes on the right without shrinking the main plot."""
    leg = ax.legend(
        loc=loc,
        bbox_to_anchor=(anchor_x, 1.0),
        borderaxespad=0.0,
        frameon=False,
        borderpad=0.2,
        labelspacing=0.4,
        handletextpad=0.5,
        alignment="left",
    )
    return leg


def create_fixed_main_plot_figure(
    fig_width: float,
    aspect_ratio: float,
    legend_area_width: float = 2.70,
    left_margin_width: float = 0.70,
    bottom_margin_height: float = 0.55,
    top_margin_height: float = 0.18,
):
    """
    Create a figure in which FIG_WIDTH is the main axes width only.

    This keeps the plotted panel aspect ratio identical across figures, while
    reserving a separate fixed-width area on the right for the legend. The saved
    canvas is intentionally not tight-cropped, because tight cropping would make
    files with different legend lengths have different final sizes.
    """
    main_axes_width = float(fig_width)
    main_axes_height = float(fig_width) / float(aspect_ratio)

    total_width = float(left_margin_width) + main_axes_width + float(legend_area_width)
    total_height = float(bottom_margin_height) + main_axes_height + float(top_margin_height)

    fig = plt.figure(figsize=(total_width, total_height))
    ax = fig.add_axes([
        float(left_margin_width) / total_width,
        float(bottom_margin_height) / total_height,
        main_axes_width / total_width,
        main_axes_height / total_height,
    ])
    return fig, ax


def draw_cutoff_line(ax, cutoff_deg: float, label: str | None = None, color: str = CUTOFF_LINE_COLOR):
    """Draw a clean vertical cutoff line after all plotted data have set the y-limits."""
    y_top = ax.get_ylim()[1]
    ax.vlines(
        x=cutoff_deg,
        ymin=0,
        ymax=0.92 * y_top,
        colors=color,
        linestyles=(0, (2.0, 1.8)),
        linewidth=1.2,
        alpha=0.95,
        zorder=6,
        label=label,
    )
    ax.set_ylim(0, y_top)


def _ensure_result_path(path: str | Path, result_dir: str | Path) -> str:
    """Return an output path guaranteed to be inside result_dir unless already absolute."""
    result_dir = Path(result_dir)
    path = Path(path)
    if path.is_absolute():
        out = path
    elif path.parts and path.parts[0] == str(result_dir):
        out = path
    else:
        out = result_dir / path
    out.parent.mkdir(parents=True, exist_ok=True)
    return str(out)



def _remove_stale_output(path: str | Path | None) -> None:
    """Delete a stale optional output so a skipped analysis cannot leave misleading files."""
    if path is None:
        return
    p = Path(path)
    if p.exists() and p.is_file():
        p.unlink()
        print(f"Removed stale skipped-analysis output: {p}")


def observed_label_class_status(
    df: pd.DataFrame,
    observed_col: str | None,
    valid_feature_mask: np.ndarray | pd.Series | None = None,
) -> dict:
    """Summarize whether real 0/1 lineage labels are actually present."""
    status = {
        "column_available": False,
        "n_valid_labels": 0,
        "n_label_0": 0,
        "n_label_1": 0,
        "has_label_0": False,
        "has_label_1": False,
        "has_both_classes": False,
    }
    if not observed_col or observed_col not in df.columns:
        return status

    labels = pd.to_numeric(df[observed_col], errors="coerce")
    if valid_feature_mask is not None:
        mask = np.asarray(valid_feature_mask, dtype=bool)
        labels = labels.loc[mask]

    valid_labels = labels[labels.isin([0, 1])]
    n0 = int((valid_labels == 0).sum())
    n1 = int((valid_labels == 1).sum())
    status.update(
        {
            "column_available": True,
            "n_valid_labels": int(valid_labels.size),
            "n_label_0": n0,
            "n_label_1": n1,
            "has_label_0": n0 > 0,
            "has_label_1": n1 > 0,
            "has_both_classes": (n0 > 0 and n1 > 0),
        }
    )
    return status


def export_component_separation_seven_terms(
    component_summary: dict,
    out_csv: str,
) -> pd.DataFrame:
    """
    Export exactly the seven manuscript-facing cutoff-based terms.

    The first four values are rounded model-estimated component counts.
    The final three values are percentages.
    """
    rows = [
        {
            "term": "High-angle component above cutoff",
            "value": component_summary[
                "model_estimated_high_angle_above_cutoff_count_rounded"
            ],
            "unit": "model-estimated count",
        },
        {
            "term": "Low-angle component above cutoff",
            "value": component_summary[
                "model_estimated_low_angle_above_cutoff_count_rounded"
            ],
            "unit": "model-estimated count",
        },
        {
            "term": "High-angle component below cutoff",
            "value": component_summary[
                "model_estimated_high_angle_below_cutoff_count_rounded"
            ],
            "unit": "model-estimated count",
        },
        {
            "term": "Low-angle component below cutoff",
            "value": component_summary[
                "model_estimated_low_angle_below_cutoff_count_rounded"
            ],
            "unit": "model-estimated count",
        },
        {
            "term": "High-angle fraction above cutoff",
            "value": component_summary["high_angle_fraction_above_cutoff_percent"],
            "unit": "percent",
        },
        {
            "term": "High-angle retention",
            "value": component_summary["high_angle_retention_percent"],
            "unit": "percent",
        },
        {
            "term": "Component-separation score",
            "value": component_summary["component_separation_score_percent"],
            "unit": "percent",
        },
    ]
    out = pd.DataFrame(rows)
    Path(out_csv).parent.mkdir(parents=True, exist_ok=True)
    out.to_csv(out_csv, index=False)
    print(f"Saved seven component-separation terms to: {out_csv}")
    return out


def _derive_overall_outpath(out_fig: str) -> str:
    if out_fig.lower().endswith(".png"):
        return out_fig[:-4] + "_overall_grey.png"
    return out_fig + "_overall_grey.png"


def _derive_overall_raw_outpath(out_fig: str) -> str:
    if out_fig.lower().endswith(".png"):
        return out_fig[:-4] + "_overall_raw_grey.png"
    return out_fig + "_overall_raw_grey.png"



def make_angle_bins(
    angles: np.ndarray,
    n_bins: int = 35,
    use_fixed_range: bool = False,
    fixed_min: float = 0.0,
    fixed_max: float = 180.0,
) -> np.ndarray:
    """Create histogram bins from all valid angles or from a fixed range."""
    if use_fixed_range:
        return np.linspace(fixed_min, fixed_max, n_bins + 1)

    vmin = float(np.min(angles))
    vmax = float(np.max(angles))
    if np.isclose(vmin, vmax):
        vmin = max(0.0, vmin - 0.5)
        vmax = min(180.0, vmax + 0.5)
    return np.linspace(vmin, vmax, n_bins + 1)




def fit_single_vonmises_axial(angles_deg: np.ndarray) -> dict:
    """
    Fit a one-component axial von Mises distribution to angles in degrees.

    The angle is transformed as theta = 2 * angle, so 0 and 180 degrees are
    treated consistently for line-orientation data.
    """
    angles_deg = np.asarray(angles_deg, dtype=float)
    angles_deg = angles_deg[np.isfinite(angles_deg)]
    if angles_deg.size < 2:
        raise ValueError("At least two valid angles are required for a von Mises fit.")

    theta = angle_deg_to_theta_rad(np.clip(angles_deg, 0.0, 180.0))
    s_bar = float(np.mean(np.sin(theta)))
    c_bar = float(np.mean(np.cos(theta)))
    mu = float(np.arctan2(s_bar, c_bar) % (2.0 * np.pi))
    R = float(np.sqrt(s_bar ** 2 + c_bar ** 2))
    kappa = float(kappa_from_R(np.array([R], dtype=float))[0])
    mean_angle_deg = float(theta_rad_to_angle_deg(np.array([mu], dtype=float))[0])

    return {
        "n": int(angles_deg.size),
        "mu_theta_rad": mu,
        "mean_angle_deg": mean_angle_deg,
        "kappa": kappa,
        "resultant_length_R": R,
    }


def vonmises_count_curve_for_angles(
    x_angle_grid: np.ndarray,
    mu: float,
    kappa: float,
    n_observations: int,
    bin_width: float,
) -> np.ndarray:
    """
    Convert a fitted von Mises PDF into expected histogram counts per bin.

    The PDF is normalized on the angle-degree domain [0, 180], then multiplied
    by sample size and bin width so the curve overlays a count histogram.
    """
    x_angle_grid = np.asarray(x_angle_grid, dtype=float)
    theta_grid = angle_deg_to_theta_rad(x_angle_grid)

    dtheta_da = 2.0 * np.pi / 180.0
    pdf_angle = vonmises_pdf(theta_grid, mu, kappa) * dtheta_da

    # Normalize on the full biologically possible range to keep count scaling stable.
    norm_grid = np.linspace(0.0, 180.0, 4000)
    norm_theta = angle_deg_to_theta_rad(norm_grid)
    norm_pdf = vonmises_pdf(norm_theta, mu, kappa) * dtheta_da
    norm = integrate_trapezoid(norm_pdf, norm_grid)
    if norm > 0:
        pdf_angle = pdf_angle / norm

    return n_observations * bin_width * pdf_angle


def compute_plot_ymax(*arrays: np.ndarray, margin: float = 1.08, minimum: float = 1.0) -> float:
    """Compute a clean shared y-maximum from one or more count-like arrays."""
    maxima = []
    for arr in arrays:
        if arr is None:
            continue
        arr = np.asarray(arr, dtype=float)
        arr = arr[np.isfinite(arr)]
        if arr.size > 0:
            maxima.append(float(np.max(arr)))
    if not maxima:
        return float(minimum)
    ymax = max(maxima) * float(margin)
    return max(float(minimum), float(ymax))


def apply_shared_ymax(ax, shared_ymax: float | None = None):
    """Apply a shared y-axis limit if provided."""
    if shared_ymax is not None:
        ax.set_ylim(0, shared_ymax)


def plot_observed_division_raw_distributions(
    df: pd.DataFrame,
    xcol: str,
    cutoff_deg: float,
    observed_col: str = "observed_division",
    bins: np.ndarray | None = None,
    out_count: str = "observed_division_distribution_pair_count.png",
    out_frequency: str = "observed_division_distribution_pair_frequency.png",
    out_count_cutoff: str = "observed_division_distribution_pair_count_with_cutoff.png",
    out_frequency_cutoff: str = "observed_division_distribution_pair_frequency_with_cutoff.png",
    dpi: int = 600,
    fig_width: float = 6.5,
    aspect_ratio: float = 1.80,
    x_axis_label: str | None = "Mean junction angle (°)",
    color_non_daughter_hist: str = "#89A7C3",
    color_daughter_hist: str = "#E6A08F",
    cutoff_color: str = "#808080",
    xlim_min: float = 60.0,
    xlim_max: float = 180.0,
    ymax_margin: float = 1.08,
    legend_anchor_x: float = 1.02,
    legend_loc: str = "upper left",
    legend_area_width: float = 2.70,
    left_margin_width: float = 0.70,
    bottom_margin_height: float = 0.55,
    top_margin_height: float = 0.18,
    show_figure_titles: bool = False,
    show_plots: bool = False,
) -> list[str]:
    """
    Save four raw daughter/non-daughter histograms with no fitted curves.

    The count figures show the number of pairs in each bin. The frequency
    figures normalize each observed class separately, so the bin heights for
    each class sum to 1. The two cutoff versions add only a grey dotted vertical
    line at ``cutoff_deg``; otherwise they are identical to their no-cutoff
    counterparts.
    """
    if observed_col not in df.columns:
        print(
            f"Skipped raw observed-label distributions: column "
            f"'{observed_col}' was not found."
        )
        return []
    if xcol not in df.columns:
        raise ValueError(f"Column '{xcol}' not found. Available: {list(df.columns)}")

    angle_values = pd.to_numeric(df[xcol], errors="coerce")
    label_values = pd.to_numeric(df[observed_col], errors="coerce")
    valid = angle_values.notna() & label_values.isin([0, 1])
    if int(valid.sum()) == 0:
        print(
            f"Skipped raw observed-label distributions: no valid 0/1 labels "
            f"in '{observed_col}'."
        )
        return []

    labeled_angles = angle_values[valid].astype(float).clip(0.0, 180.0)
    labeled_values = label_values[valid].astype(int)
    non_daughter = labeled_angles[labeled_values == 0].to_numpy()
    daughter = labeled_angles[labeled_values == 1].to_numpy()

    if non_daughter.size == 0 or daughter.size == 0:
        print(
            "Skipped raw observed-label distributions: both classes are required "
            f"(daughter n={daughter.size}, non-daughter n={non_daughter.size})."
        )
        return []

    if bins is None:
        bins = make_angle_bins(labeled_angles.to_numpy(), n_bins=35)
    bins = np.asarray(bins, dtype=float)
    if bins.ndim != 1 or bins.size < 2 or np.any(np.diff(bins) <= 0):
        raise ValueError("Histogram bins must be a strictly increasing one-dimensional array.")

    non_counts, _ = np.histogram(non_daughter, bins=bins)
    daughter_counts, _ = np.histogram(daughter, bins=bins)
    non_frequency = non_counts.astype(float) / float(non_daughter.size)
    daughter_frequency = daughter_counts.astype(float) / float(daughter.size)

    count_ymax = compute_plot_ymax(
        non_counts, daughter_counts, margin=ymax_margin, minimum=1.0
    )
    frequency_ymax = compute_plot_ymax(
        non_frequency, daughter_frequency, margin=ymax_margin, minimum=0.01
    )

    plot_specs = [
        (out_count, "count", False),
        (out_frequency, "frequency", False),
        (out_count_cutoff, "count", True),
        (out_frequency_cutoff, "frequency", True),
    ]
    saved_paths: list[str] = []

    for out_path, scale, add_cutoff in plot_specs:
        fig, ax = create_fixed_main_plot_figure(
            fig_width=fig_width,
            aspect_ratio=aspect_ratio,
            legend_area_width=legend_area_width,
            left_margin_width=left_margin_width,
            bottom_margin_height=bottom_margin_height,
            top_margin_height=top_margin_height,
        )

        if scale == "frequency":
            non_weights = np.full(non_daughter.size, 1.0 / non_daughter.size)
            daughter_weights = np.full(daughter.size, 1.0 / daughter.size)
            y_axis_label = "Pair frequency"
            y_max = frequency_ymax
        else:
            non_weights = None
            daughter_weights = None
            y_axis_label = "Pair count"
            y_max = count_ymax

        ax.hist(
            non_daughter,
            bins=bins,
            weights=non_weights,
            color=color_non_daughter_hist,
            alpha=0.82,
            edgecolor="white",
            linewidth=0.45,
            label=f"Non-daughter pair (n={non_daughter.size})",
            zorder=1,
        )
        ax.hist(
            daughter,
            bins=bins,
            weights=daughter_weights,
            color=color_daughter_hist,
            alpha=0.86,
            edgecolor="white",
            linewidth=0.45,
            label=f"Daughter pair (n={daughter.size})",
            zorder=2,
        )

        if add_cutoff:
            ax.axvline(
                float(cutoff_deg),
                color=cutoff_color,
                linestyle=(0, (2.0, 2.0)),
                linewidth=1.2,
                alpha=1.0,
                label=f"Best threshold = {float(cutoff_deg):.1f}°",
                zorder=5,
            )

        if show_figure_titles:
            title = "Observed daughter/non-daughter pair distribution"
            if add_cutoff:
                title += " with best threshold"
            ax.set_title(title, pad=5)

        ax.set_xlabel(x_axis_label if x_axis_label is not None else xcol)
        ax.set_ylabel(y_axis_label)
        style_ax(ax)
        ax.set_xlim(xlim_min, xlim_max)
        ax.set_ylim(0, y_max)
        add_clean_legend(ax, loc=legend_loc, anchor_x=legend_anchor_x)

        fig.savefig(out_path, dpi=dpi, facecolor="white")
        if show_plots:
            plt.show()
        else:
            plt.close(fig)
        saved_paths.append(str(out_path))

    print(
        "Saved four raw observed-label distributions "
        f"(daughter n={daughter.size}, non-daughter n={non_daughter.size}; "
        f"best threshold={float(cutoff_deg):.3f}°):"
    )
    for path in saved_paths:
        print(f"  {path}")
    return saved_paths


def plot_observed_division_labeled_distribution(
    df: pd.DataFrame,
    xcol: str,
    observed_col: str = "observed_division",
    bins: np.ndarray | None = None,
    out_fig: str = "observed_division_labeled_vonmises_distribution.png",
    out_fit_csv: str | None = "observed_division_labeled_vonmises_fit_parameters.csv",
    dpi: int = 600,
    fig_width: float = 6.5,
    aspect_ratio: float = 1.55,
    x_axis_label: str | None = "Mean junction angle (°)",
    y_axis_label: str = "Pair count",
    show_figure_titles: bool = False,
    color_non_daughter_hist: str = "#D9E4EE",
    color_daughter_hist: str = "#F4D5CE",
    color_non_daughter_line: str = "#4C79A2",
    color_daughter_line: str = "#DA6752",
    xlim_min: float = 60.0,
    xlim_max: float = 180.0,
    shared_ymax: float | None = None,
    legend_anchor_x: float = 1.02,
    legend_loc: str = "upper left",
    legend_area_width: float = 2.70,
    left_margin_width: float = 0.70,
    bottom_margin_height: float = 0.55,
    top_margin_height: float = 0.18,
    show_plots: bool = False,
) -> str | None:
    """
    Plot real daughter/non-daughter distributions from observed_division labels.

    This function uses only the observed labels:
    - observed_division == 1: Daughter pair
    - observed_division == 0: Non-daughter pair

    It does not draw a cutoff line and does not use the two-component mixture
    fitted from all data. Instead, it fits one von Mises distribution separately
    to the observed daughter group and one von Mises distribution separately to
    the observed non-daughter group, then overlays these two fitted curves on the
    observed histograms.
    """
    if observed_col not in df.columns:
        print(f"Skipped labeled observed-division plot: column '{observed_col}' was not found.")
        return None

    if xcol not in df.columns:
        raise ValueError(f"Column '{xcol}' not found. Available: {list(df.columns)}")

    angle_values = pd.to_numeric(df[xcol], errors="coerce")
    label_values = pd.to_numeric(df[observed_col], errors="coerce")

    valid = angle_values.notna() & label_values.isin([0, 1])
    if int(valid.sum()) == 0:
        print(f"Skipped labeled observed-division plot: no valid 0/1 labels in '{observed_col}'.")
        return None

    plot_df = pd.DataFrame({
        "angle": angle_values[valid].astype(float).clip(0.0, 180.0),
        "observed_division": label_values[valid].astype(int),
    })

    daughter = plot_df.loc[plot_df["observed_division"] == 1, "angle"].to_numpy()
    non_daughter = plot_df.loc[plot_df["observed_division"] == 0, "angle"].to_numpy()

    if daughter.size < 2 or non_daughter.size < 2:
        print(
            "Skipped labeled observed-division von Mises plot: both classes need at least 2 angles "
            f"(daughter n={daughter.size}, non-daughter n={non_daughter.size})."
        )
        return None

    if bins is None:
        bins = make_angle_bins(plot_df["angle"].to_numpy(), n_bins=35)

    bin_width = float(bins[1] - bins[0])
    x_min, x_max = float(bins[0]), float(bins[-1])
    xx_ang = np.linspace(x_min, x_max, 1600)

    fit_non_daughter = fit_single_vonmises_axial(non_daughter)
    fit_daughter = fit_single_vonmises_axial(daughter)

    non_daughter_curve = vonmises_count_curve_for_angles(
        xx_ang,
        mu=fit_non_daughter["mu_theta_rad"],
        kappa=fit_non_daughter["kappa"],
        n_observations=non_daughter.size,
        bin_width=bin_width,
    )
    daughter_curve = vonmises_count_curve_for_angles(
        xx_ang,
        mu=fit_daughter["mu_theta_rad"],
        kappa=fit_daughter["kappa"],
        n_observations=daughter.size,
        bin_width=bin_width,
    )

    if out_fit_csv is not None:
        fit_df = pd.DataFrame([
            {
                "group": "Non-daughter pair",
                "observed_division": 0,
                "n": fit_non_daughter["n"],
                "mean_angle_deg": fit_non_daughter["mean_angle_deg"],
                "kappa": fit_non_daughter["kappa"],
                "resultant_length_R": fit_non_daughter["resultant_length_R"],
            },
            {
                "group": "Daughter pair",
                "observed_division": 1,
                "n": fit_daughter["n"],
                "mean_angle_deg": fit_daughter["mean_angle_deg"],
                "kappa": fit_daughter["kappa"],
                "resultant_length_R": fit_daughter["resultant_length_R"],
            },
        ])
        fit_df.to_csv(out_fit_csv, index=False)
        print(f"Saved observed-label von Mises fit parameters to: {out_fit_csv}")

    fig_height = fig_width / aspect_ratio
    fig, ax = create_fixed_main_plot_figure(
        fig_width=fig_width,
        aspect_ratio=aspect_ratio,
        legend_area_width=legend_area_width,
        left_margin_width=left_margin_width,
        bottom_margin_height=bottom_margin_height,
        top_margin_height=top_margin_height,
    )

    # Real observed labels only. Non-daughter is drawn first because it is the larger class.
    plt.hist(
        non_daughter,
        bins=bins,
        color=color_non_daughter_hist,
        alpha=0.88,
        edgecolor="white",
        linewidth=0.45,
        label=f"Non-daughter pair (n={non_daughter.size})",
        zorder=1,
    )
    plt.hist(
        daughter,
        bins=bins,
        color=color_daughter_hist,
        alpha=0.90,
        edgecolor="white",
        linewidth=0.45,
        label=f"Daughter pair (n={daughter.size})",
        zorder=2,
    )

    plt.plot(
        xx_ang,
        non_daughter_curve,
        linewidth=2.25,
        color=color_non_daughter_line,
        alpha=1.0,
        label="Non-daughter von Mises fit",
        zorder=5,
    )
    plt.plot(
        xx_ang,
        daughter_curve,
        linewidth=2.25,
        color=color_daughter_line,
        alpha=1.0,
        label="Daughter von Mises fit",
        zorder=6,
    )

    if show_figure_titles:
        plt.title("Observed daughter/non-daughter distributions + von Mises fits", pad=5)
    plt.xlabel(x_axis_label if x_axis_label is not None else xcol)
    plt.ylabel(y_axis_label)

    style_ax(ax)
    ax.set_xlim(xlim_min, xlim_max)
    apply_shared_ymax(ax, shared_ymax)
    add_clean_legend(ax, loc=legend_loc, anchor_x=legend_anchor_x)

    plt.savefig(out_fig, dpi=dpi, facecolor="white")
    if show_plots:
        plt.show()
    else:
        plt.close()

    print(
        f"Saved labeled observed-division von Mises distribution: {out_fig} "
        f"(daughter n={daughter.size}, non-daughter n={non_daughter.size}; "
        f"daughter mean={fit_daughter['mean_angle_deg']:.2f}°, "
        f"non-daughter mean={fit_non_daughter['mean_angle_deg']:.2f}°)"
    )
    return out_fig



def _json_safe_float(value) -> float | None:
    """Convert numeric values to JSON-safe floats, using None for non-finite values."""
    if value is None:
        return None
    value = float(value)
    if not np.isfinite(value):
        return None
    return value


def summarize_angle_group(values: np.ndarray | None) -> dict:
    """Summarize one angle group for the KS-test JSON export."""
    if values is None:
        return {
            "n": 0,
            "mean_angle_deg": None,
            "median_angle_deg": None,
            "std_angle_deg": None,
            "min_angle_deg": None,
            "max_angle_deg": None,
        }

    arr = np.asarray(values, dtype=float)
    arr = arr[np.isfinite(arr)]
    if arr.size == 0:
        return {
            "n": 0,
            "mean_angle_deg": None,
            "median_angle_deg": None,
            "std_angle_deg": None,
            "min_angle_deg": None,
            "max_angle_deg": None,
        }

    return {
        "n": int(arr.size),
        "mean_angle_deg": _json_safe_float(np.mean(arr)),
        "median_angle_deg": _json_safe_float(np.median(arr)),
        "std_angle_deg": _json_safe_float(np.std(arr, ddof=1)) if arr.size >= 2 else None,
        "min_angle_deg": _json_safe_float(np.min(arr)),
        "max_angle_deg": _json_safe_float(np.max(arr)),
    }


def ks_2sample_comparison_summary(
    group_a: np.ndarray | None,
    group_b: np.ndarray | None,
    group_a_name: str,
    group_b_name: str,
    alternative: str = "two-sided",
    mode: str = "auto",
) -> dict:
    """
    Return a JSON-friendly two-sample KS-test summary for two angle groups.

    Empty or unavailable groups are reported as skipped instead of raising an
    error, so the output JSON always documents all requested comparisons.
    """
    a = None if group_a is None else np.asarray(group_a, dtype=float)
    b = None if group_b is None else np.asarray(group_b, dtype=float)

    if a is not None:
        a = a[np.isfinite(a)]
    if b is not None:
        b = b[np.isfinite(b)]

    n_a = 0 if a is None else int(a.size)
    n_b = 0 if b is None else int(b.size)

    result = {
        "group_a": group_a_name,
        "group_b": group_b_name,
        "n_group_a": n_a,
        "n_group_b": n_b,
        "test": "scipy.stats.ks_2samp",
        "alternative": alternative,
        "mode": mode,
        "status": "not_run",
        "statistic_D": None,
        "pvalue": None,
        "statistic_location": None,
        "statistic_sign": None,
        "note": None,
    }

    if a is None or b is None:
        result["status"] = "skipped_missing_group"
        result["note"] = "One or both groups were unavailable."
        return result

    if n_a == 0 or n_b == 0:
        result["status"] = "skipped_empty_group"
        result["note"] = "Both groups need at least one valid angle for scipy.stats.ks_2samp."
        return result

    ks = ks_2samp(a, b, alternative=alternative, mode=mode)
    result["status"] = "ok"
    result["statistic_D"] = _json_safe_float(ks.statistic)
    result["pvalue"] = _json_safe_float(ks.pvalue)

    # Newer SciPy versions expose the angle where the maximum ECDF difference
    # occurs and the sign of that difference. Keep these fields when available.
    result["statistic_location"] = _json_safe_float(getattr(ks, "statistic_location", None))
    statistic_sign = getattr(ks, "statistic_sign", None)
    result["statistic_sign"] = None if statistic_sign is None else int(statistic_sign)
    return result


def export_ks_group_comparisons_json(
    df: pd.DataFrame,
    xcol: str,
    cutoff_deg: float,
    out_json: str,
    observed_col: str = "observed_division",
) -> dict:
    """
    Export requested KS comparisons among cutoff-defined and observed-label groups.

    Groups:
    - low_angle_group: all valid angles below the fitted cutoff
    - high_angle_group: all valid angles at or above the fitted cutoff
    - non_daughter_pairs: observed_division == 0, if the label column exists
    - daughter_pairs: observed_division == 1, if the label column exists

    Comparisons exported:
    1. low_angle_group vs non_daughter_pairs
    2. high_angle_group vs daughter_pairs
    3. low_angle_group vs high_angle_group
    4. daughter_pairs vs non_daughter_pairs
    """
    if xcol not in df.columns:
        raise ValueError(f"Column '{xcol}' not found. Available: {list(df.columns)}")

    angle_values = pd.to_numeric(df[xcol], errors="coerce")
    valid_angles = angle_values.notna()
    all_angles = angle_values[valid_angles].astype(float).clip(0.0, 180.0).to_numpy()

    low_angle_group = all_angles[all_angles < cutoff_deg]
    high_angle_group = all_angles[all_angles >= cutoff_deg]

    daughter_pairs = None
    non_daughter_pairs = None
    observed_label_status = "missing_column"
    n_valid_observed_labels = 0

    if observed_col in df.columns:
        label_values = pd.to_numeric(df[observed_col], errors="coerce")
        valid_observed = valid_angles & label_values.isin([0, 1])
        n_valid_observed_labels = int(valid_observed.sum())
        if n_valid_observed_labels > 0:
            observed_angles = angle_values[valid_observed].astype(float).clip(0.0, 180.0)
            observed_labels = label_values[valid_observed].astype(int)
            daughter_pairs = observed_angles[observed_labels == 1].to_numpy()
            non_daughter_pairs = observed_angles[observed_labels == 0].to_numpy()
            observed_label_status = "ok"
        else:
            observed_label_status = "no_valid_0_1_labels"

    groups = {
        "low_angle_group": low_angle_group,
        "high_angle_group": high_angle_group,
        "daughter_pairs": daughter_pairs,
        "non_daughter_pairs": non_daughter_pairs,
    }

    comparisons = {
        "low_angle_group_vs_non_daughter_pairs": ks_2sample_comparison_summary(
            groups["low_angle_group"],
            groups["non_daughter_pairs"],
            "low_angle_group",
            "non_daughter_pairs",
        ),
        "high_angle_group_vs_daughter_pairs": ks_2sample_comparison_summary(
            groups["high_angle_group"],
            groups["daughter_pairs"],
            "high_angle_group",
            "daughter_pairs",
        ),
        "low_angle_group_vs_high_angle_group": ks_2sample_comparison_summary(
            groups["low_angle_group"],
            groups["high_angle_group"],
            "low_angle_group",
            "high_angle_group",
        ),
        "daughter_pairs_vs_non_daughter_pairs": ks_2sample_comparison_summary(
            groups["daughter_pairs"],
            groups["non_daughter_pairs"],
            "daughter_pairs",
            "non_daughter_pairs",
        ),
    }

    output = {
        "metadata": {
            "angle_column": xcol,
            "observed_division_column": observed_col,
            "observed_label_status": observed_label_status,
            "cutoff_angle_deg": _json_safe_float(cutoff_deg),
            "low_angle_rule": "angle < cutoff_angle_deg",
            "high_angle_rule": "angle >= cutoff_angle_deg",
            "n_valid_angles": int(all_angles.size),
            "n_valid_observed_labels": n_valid_observed_labels,
        },
        "groups": {name: summarize_angle_group(values) for name, values in groups.items()},
        "comparisons": comparisons,
    }

    out_path = Path(out_json)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(output, f, indent=2, allow_nan=False)

    print(f"Saved KS group comparison statistics to: {out_json}")
    return output


def posterior_probabilities(
    theta: np.ndarray,
    weights: np.ndarray,
    mus: np.ndarray,
    kappas: np.ndarray,
) -> np.ndarray:
    """Return stable posterior probabilities for a fitted von Mises mixture."""
    theta = np.asarray(theta, dtype=float)
    K = len(weights)
    log_pdf = np.empty((theta.size, K), dtype=float)
    for k in range(K):
        log_norm = np.log(2.0 * np.pi) + np.log(i0(kappas[k]))
        log_pdf[:, k] = (
            np.log(weights[k] + 1e-300)
            + kappas[k] * np.cos(theta - mus[k])
            - log_norm
        )
    row_max = np.max(log_pdf, axis=1, keepdims=True)
    probs = np.exp(log_pdf - row_max)
    return probs / (np.sum(probs, axis=1, keepdims=True) + 1e-300)


def fit_vonmises_mixture_initialized(
    theta: np.ndarray,
    initial_weights: np.ndarray,
    initial_mus: np.ndarray,
    initial_kappas: np.ndarray,
    max_iter: int = 200,
    tol: float = 1e-6,
):
    """Fast EM refit initialized from the full-data fit for grouped bootstrap."""
    theta = np.asarray(theta, dtype=float)
    if theta.size < 10:
        raise ValueError("At least 10 angles are required for a bootstrap mixture refit.")

    weights = np.asarray(initial_weights, dtype=float).copy()
    mus = np.asarray(initial_mus, dtype=float).copy()
    kappas = np.asarray(initial_kappas, dtype=float).copy()
    weights = np.clip(weights, 1e-8, None)
    weights /= weights.sum()

    sin_t = np.sin(theta)
    cos_t = np.cos(theta)
    previous_ll = -np.inf
    ll = -np.inf

    for _ in range(max_iter):
        resp = posterior_probabilities(theta, weights, mus, kappas)
        Nk = resp.sum(axis=0) + 1e-300
        weights = Nk / theta.size

        S = resp.T @ sin_t
        C = resp.T @ cos_t
        mus = np.arctan2(S, C) % (2.0 * np.pi)
        Rbar = np.sqrt(S**2 + C**2) / Nk
        kappas = kappa_from_R(Rbar)

        log_pdf = np.empty((theta.size, len(weights)), dtype=float)
        for k in range(len(weights)):
            log_norm = np.log(2.0 * np.pi) + np.log(i0(kappas[k]))
            log_pdf[:, k] = (
                np.log(weights[k] + 1e-300)
                + kappas[k] * np.cos(theta - mus[k])
                - log_norm
            )
        row_max = np.max(log_pdf, axis=1, keepdims=True)
        ll = float(np.sum(row_max[:, 0] + np.log(np.sum(np.exp(log_pdf - row_max), axis=1) + 1e-300)))
        if np.isfinite(previous_ll) and abs(ll - previous_ll) < tol:
            break
        previous_ll = ll

    return weights, mus, kappas, ll


def weighted_ecdf_at(
    grid: np.ndarray,
    values: np.ndarray,
    weights: np.ndarray | None = None,
) -> np.ndarray:
    """Evaluate an empirical or weighted empirical CDF on a supplied grid."""
    grid = np.asarray(grid, dtype=float)
    values = np.asarray(values, dtype=float)
    if weights is None:
        weights = np.ones(values.size, dtype=float)
    else:
        weights = np.asarray(weights, dtype=float)

    valid = np.isfinite(values) & np.isfinite(weights) & (weights >= 0)
    values = values[valid]
    weights = weights[valid]
    if values.size == 0 or float(weights.sum()) <= 0:
        return np.full(grid.shape, np.nan, dtype=float)

    order = np.argsort(values, kind="mergesort")
    sorted_values = values[order]
    cumulative = np.cumsum(weights[order])
    cumulative /= cumulative[-1]
    positions = np.searchsorted(sorted_values, grid, side="right") - 1
    out = np.zeros(grid.shape, dtype=float)
    inside = positions >= 0
    out[inside] = cumulative[positions[inside]]
    return out


def axial_mean_angle_deg(values: np.ndarray, weights: np.ndarray | None = None) -> float:
    """Return the circular mean for 180-degree-periodic axial angles."""
    values = np.asarray(values, dtype=float)
    if weights is None:
        weights = np.ones(values.size, dtype=float)
    else:
        weights = np.asarray(weights, dtype=float)
    valid = np.isfinite(values) & np.isfinite(weights) & (weights >= 0)
    values = values[valid]
    weights = weights[valid]
    if values.size == 0 or float(weights.sum()) <= 0:
        return float("nan")
    theta = angle_deg_to_theta_rad(values)
    S = float(np.sum(weights * np.sin(theta)))
    C = float(np.sum(weights * np.cos(theta)))
    return float(theta_rad_to_angle_deg(np.array([np.arctan2(S, C) % (2.0 * np.pi)]))[0])


def axial_abs_difference_deg(a: float, b: float) -> float:
    """Smallest absolute difference between two 180-degree-periodic angles."""
    return float(abs((float(a) - float(b) + 90.0) % 180.0 - 90.0))


def component_label_wasserstein_metrics(
    angles: np.ndarray,
    observed_labels: np.ndarray,
    component_weights: np.ndarray,
    observed_label: int,
) -> dict:
    """Return descriptive Wasserstein closeness in the original angle units."""
    angles = np.asarray(angles, dtype=float)
    observed_labels = np.asarray(observed_labels, dtype=float)
    component_weights = np.asarray(component_weights, dtype=float)

    valid_all = np.isfinite(angles) & np.isfinite(component_weights) & (component_weights >= 0)
    component_angles = angles[valid_all]
    component_w = component_weights[valid_all]
    observed = angles[valid_all & (observed_labels == int(observed_label))]

    if observed.size < 2 or component_angles.size < 2 or float(component_w.sum()) <= 0:
        raise ValueError("Both the observed group and weighted component need valid observations.")

    w_distance = float(
        wasserstein_distance(
            observed,
            component_angles,
            u_weights=None,
            v_weights=component_w,
        )
    )
    return {
        "n_observed": int(observed.size),
        "component_effective_count": float(component_w.sum()),
        "wasserstein_distance_deg": w_distance,
    }


def derive_bootstrap_group_ids(
    df: pd.DataFrame,
    group_column: str,
    group_regex: str | None,
) -> tuple[np.ndarray, str, int]:
    """Derive cluster IDs, falling back to row bootstrap when clusters are unavailable."""
    column_lookup = {str(column).casefold(): column for column in df.columns}
    resolved = column_lookup.get(str(group_column).casefold())
    if resolved is not None:
        raw = df[resolved].astype(str)
        if group_regex:
            extracted = raw.str.extract(group_regex, expand=False)
            if isinstance(extracted, pd.DataFrame):
                extracted = extracted.iloc[:, 0]
            groups = extracted.fillna(raw).astype(str).to_numpy()
        else:
            groups = raw.to_numpy()
        n_groups = int(pd.Series(groups).nunique())
        if n_groups >= 2:
            return groups, f"cluster:{resolved}", n_groups

    print(
        "Grouped bootstrap could not identify at least two clusters; "
        "falling back to row bootstrap."
    )
    groups = np.array([f"row_{i}" for i in range(len(df))], dtype=object)
    return groups, "row", int(len(df))


def _bootstrap_summary(values: list[float]) -> dict:
    arr = np.asarray(values, dtype=float)
    arr = arr[np.isfinite(arr)]
    if arr.size == 0:
        return {
            "bootstrap_ci_lower_95": np.nan,
            "bootstrap_ci_upper_95": np.nan,
            "bootstrap_one_sided_upper_95": np.nan,
            "n_bootstrap_success": 0,
        }
    return {
        "bootstrap_ci_lower_95": float(np.quantile(arr, 0.025)),
        "bootstrap_ci_upper_95": float(np.quantile(arr, 0.975)),
        "bootstrap_one_sided_upper_95": float(np.quantile(arr, 0.95)),
        "n_bootstrap_success": int(arr.size),
    }


def plot_component_label_closeness_ecdf(
    angles: np.ndarray,
    observed_labels: np.ndarray,
    responsibilities: np.ndarray,
    summary_df: pd.DataFrame,
    out_fig: str,
    dpi: int = 600,
    xlim_min: float = 60.0,
    xlim_max: float = 180.0,
    color_low: str = "#4C79A2",
    color_high: str = "#DA6752",
    show_plots: bool = False,
) -> None:
    """Plot labeled ECDFs against posterior-weighted component ECDFs."""
    comparisons = [
        ("non_daughter_vs_low_component", 0, 0, "Non-daughter vs low component", color_low),
        ("daughter_vs_high_component", 1, 1, "Daughter vs high component", color_high),
    ]
    grid = np.linspace(0.0, 180.0, 1801)
    fig, axes = plt.subplots(1, 2, figsize=(9.2, 3.8), sharey=True)

    for ax, (name, label, comp_idx, title, color) in zip(axes, comparisons):
        observed = angles[observed_labels == label]
        observed_cdf = weighted_ecdf_at(grid, observed)
        component_cdf = weighted_ecdf_at(grid, angles, responsibilities[:, comp_idx])
        row = summary_df.loc[summary_df["comparison"] == name].iloc[0]

        ax.plot(grid, observed_cdf, color="#222222", linewidth=2.0, label="Observed label")
        ax.plot(
            grid,
            component_cdf,
            color=color,
            linewidth=2.0,
            linestyle=(0, (4.0, 2.0)),
            label="Posterior-weighted component",
        )
        ax.set_title(title, fontsize=9)
        ax.set_xlim(xlim_min, xlim_max)
        ax.set_ylim(0.0, 1.02)
        ax.set_xlabel("Mean junction angle (°)")
        ax.grid(axis="y", color="#E8E8E8", linewidth=0.6)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.tick_params(direction="out", length=3.5)
        ax.text(
            0.03,
            0.97,
            (
                f"W₁ point estimate = {row['wasserstein_distance_deg']:.2f}°\n"
                "One-sided 95% upper bound\n"
                f"= {row['bootstrap_one_sided_upper_95_deg']:.2f}°"
            ),
            transform=ax.transAxes,
            ha="left",
            va="top",
            fontsize=7.5,
            bbox=dict(boxstyle="round,pad=0.35", facecolor="white", edgecolor="#D0D0D0"),
        )
        ax.legend(frameon=False, fontsize=7.5, loc="lower right")

    axes[0].set_ylabel("Cumulative proportion")
    fig.tight_layout()
    out_path = Path(out_fig)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=dpi, facecolor="white")
    if show_plots:
        plt.show()
    else:
        plt.close(fig)


def run_component_label_closeness_analysis(
    df_valid: pd.DataFrame,
    angles: np.ndarray,
    responsibilities: np.ndarray,
    fitted_weights: np.ndarray,
    fitted_mus: np.ndarray,
    fitted_kappas: np.ndarray,
    observed_col: str,
    out_summary_csv: str,
    out_bootstrap_csv: str,
    out_fig: str,
    bootstrap_replicates: int = 1000,
    bootstrap_group_column: str = "fileName",
    bootstrap_group_regex: str | None = r"^(sample\d+)",
    bootstrap_max_iter: int = 200,
    bootstrap_seed: int = 0,
    dpi: int = 600,
    xlim_min: float = 60.0,
    xlim_max: float = 180.0,
    color_low: str = "#4C79A2",
    color_high: str = "#DA6752",
    show_plots: bool = False,
) -> pd.DataFrame:
    """
    Describe closeness between observed labels and posterior-weighted components.

    The bootstrap resamples biological clusters and refits the mixture in every
    replicate. Wasserstein distance is reported in degrees with two-sided 95%
    intervals and a one-sided 95% upper bound. No identity or formal-equivalence
    claim is made.
    """
    if observed_col not in df_valid.columns:
        raise ValueError(f"Observed-label column '{observed_col}' was not found.")

    observed_labels = pd.to_numeric(df_valid[observed_col], errors="coerce").to_numpy(dtype=float)
    if not ({0, 1} <= set(observed_labels[np.isfinite(observed_labels)].astype(int))):
        raise ValueError("Closeness analysis requires both observed labels 0 and 1.")

    comparisons = [
        {
            "comparison": "non_daughter_vs_low_component",
            "observed_group": "non-daughter pairs",
            "observed_label": 0,
            "component": "posterior-weighted low-angle component",
            "component_index": 0,
        },
        {
            "comparison": "daughter_vs_high_component",
            "observed_group": "daughter pairs",
            "observed_label": 1,
            "component": "posterior-weighted high-angle component",
            "component_index": 1,
        },
    ]

    base_rows = []
    for spec in comparisons:
        metrics = component_label_wasserstein_metrics(
            angles,
            observed_labels,
            responsibilities[:, spec["component_index"]],
            spec["observed_label"],
        )
        base_rows.append({**spec, **metrics})

    group_ids, bootstrap_unit, n_groups = derive_bootstrap_group_ids(
        df_valid,
        group_column=bootstrap_group_column,
        group_regex=bootstrap_group_regex,
    )
    unique_groups = np.unique(group_ids)
    indices_by_group = {group: np.flatnonzero(group_ids == group) for group in unique_groups}
    rng = np.random.default_rng(bootstrap_seed)
    bootstrap_rows: list[dict] = []

    for replicate in range(int(bootstrap_replicates)):
        chosen_groups = rng.choice(unique_groups, size=unique_groups.size, replace=True)
        sampled_indices = np.concatenate([indices_by_group[group] for group in chosen_groups])
        sampled_angles = angles[sampled_indices]
        sampled_labels = observed_labels[sampled_indices]
        sampled_theta = angle_deg_to_theta_rad(sampled_angles)

        try:
            b_weights, b_mus, b_kappas, _ = fit_vonmises_mixture_initialized(
                sampled_theta,
                fitted_weights,
                fitted_mus,
                fitted_kappas,
                max_iter=bootstrap_max_iter,
            )
            b_means = theta_rad_to_angle_deg(b_mus)
            b_order = np.argsort(b_means)
            b_weights = b_weights[b_order]
            b_mus = b_mus[b_order]
            b_kappas = b_kappas[b_order]
            b_resp = posterior_probabilities(sampled_theta, b_weights, b_mus, b_kappas)

            for spec in comparisons:
                metrics = component_label_wasserstein_metrics(
                    sampled_angles,
                    sampled_labels,
                    b_resp[:, spec["component_index"]],
                    spec["observed_label"],
                )
                bootstrap_rows.append(
                    {
                        "replicate": replicate + 1,
                        "comparison": spec["comparison"],
                        "wasserstein_distance_deg": metrics["wasserstein_distance_deg"],
                    }
                )
        except (ValueError, FloatingPointError, np.linalg.LinAlgError):
            continue

        if bootstrap_replicates >= 10 and (replicate + 1) % max(1, bootstrap_replicates // 10) == 0:
            print(f"Closeness bootstrap progress: {replicate + 1}/{bootstrap_replicates}")

    bootstrap_df = pd.DataFrame(bootstrap_rows)
    Path(out_bootstrap_csv).parent.mkdir(parents=True, exist_ok=True)
    bootstrap_df.to_csv(out_bootstrap_csv, index=False)

    summary_rows = []
    for base in base_rows:
        comparison_boot = bootstrap_df.loc[bootstrap_df["comparison"] == base["comparison"]]
        row = dict(base)
        stats = _bootstrap_summary(comparison_boot["wasserstein_distance_deg"].tolist())
        row["bootstrap_ci_lower_95_deg"] = stats["bootstrap_ci_lower_95"]
        row["bootstrap_ci_upper_95_deg"] = stats["bootstrap_ci_upper_95"]
        row["bootstrap_one_sided_upper_95_deg"] = stats["bootstrap_one_sided_upper_95"]
        row["n_bootstrap_success"] = stats["n_bootstrap_success"]
        row["bootstrap_unit"] = bootstrap_unit
        row["n_bootstrap_groups"] = n_groups
        row["n_bootstrap_requested"] = int(bootstrap_replicates)
        row["interpretation"] = (
            f"Point estimate indicates {row['wasserstein_distance_deg']:.3f} degrees "
            "of average distributional displacement; this is descriptive, not a claim of identity"
        )
        row["validation_scope"] = (
            "Internal component-label correspondence; mixture fitted to the same dataset"
        )
        summary_rows.append(row)

    summary_df = pd.DataFrame(summary_rows)
    Path(out_summary_csv).parent.mkdir(parents=True, exist_ok=True)
    summary_df.to_csv(out_summary_csv, index=False)

    plot_component_label_closeness_ecdf(
        angles=angles,
        observed_labels=observed_labels,
        responsibilities=responsibilities,
        summary_df=summary_df,
        out_fig=out_fig,
        dpi=dpi,
        xlim_min=xlim_min,
        xlim_max=xlim_max,
        color_low=color_low,
        color_high=color_high,
        show_plots=show_plots,
    )

    print("\n=== Posterior-weighted component-label distribution closeness ===")
    for _, row in summary_df.iterrows():
        print(
            f"{row['comparison']}: W1={row['wasserstein_distance_deg']:.3f}°, "
            f"one-sided 95% upper bound="
            f"{row['bootstrap_one_sided_upper_95_deg']:.3f}°"
        )
    print(f"Saved distribution-closeness summary to: {out_summary_csv}")
    print(f"Saved bootstrap replicates to: {out_bootstrap_csv}")
    print(f"Saved distribution-closeness ECDF figure to: {out_fig}")
    return summary_df


def model_estimated_cutoff_summary(
    cutoff_deg: float,
    weights: np.ndarray,
    mus: np.ndarray,
    kappas: np.ndarray,
    n_total: int | float | None = None,
    n_grid: int = 20000,
) -> dict:
    """
    Summarize how a cutoff partitions the two fitted mixture components.

    This does not use true biological labels. It integrates the fitted low-angle
    and high-angle von Mises component densities on the angle scale and reports:
    (1) four weighted component masses/counts around the cutoff,
    (2) high-angle fraction above cutoff,
    (3) high-angle retention, and
    (4) component-separation score, defined as the harmonic mean of (2) and (3).

    High-angle fraction above cutoff and high-angle retention have the same
    algebraic forms as precision and recall when the fitted high-angle component
    is treated as the target, but
    they are not biological classification metrics because no lineage labels
    enter their calculation.

    Assumptions:
    - component 0 = low-angle component
    - component 1 = high-angle component
    - angle >= cutoff_deg defines the above-cutoff group
    """
    a_grid = np.linspace(0.0, 180.0, n_grid)
    theta_grid = angle_deg_to_theta_rad(a_grid)

    # Change of variables from theta to angle degree:
    # theta = 2 * angle * pi / 180
    dtheta_da = 2.0 * np.pi / 180.0

    pdf_low = vonmises_pdf(theta_grid, mus[0], kappas[0]) * dtheta_da
    pdf_high = vonmises_pdf(theta_grid, mus[1], kappas[1]) * dtheta_da

    # Numerical normalization protects against small integration errors.
    pdf_low = pdf_low / integrate_trapezoid(pdf_low, a_grid)
    pdf_high = pdf_high / integrate_trapezoid(pdf_high, a_grid)

    pred_high = a_grid >= cutoff_deg
    pred_low = ~pred_high

    # Component-conditional probabilities above and below the cutoff.
    low_above_cutoff = integrate_trapezoid(pdf_low[pred_high], a_grid[pred_high])
    low_below_cutoff = integrate_trapezoid(pdf_low[pred_low], a_grid[pred_low])
    high_above_cutoff = integrate_trapezoid(pdf_high[pred_high], a_grid[pred_high])
    high_below_cutoff = integrate_trapezoid(pdf_high[pred_low], a_grid[pred_low])

    # Ensure the two sides of each component sum exactly to one despite the
    # finite integration grid.
    low_split_total = low_above_cutoff + low_below_cutoff
    high_split_total = high_above_cutoff + high_below_cutoff
    low_above_cutoff /= low_split_total
    low_below_cutoff /= low_split_total
    high_above_cutoff /= high_split_total
    high_below_cutoff /= high_split_total

    weight_low = float(weights[0])
    weight_high = float(weights[1])

    # Weighted fitted masses in the four component/cutoff cells.
    mass_high_above = weight_high * high_above_cutoff
    mass_low_above = weight_low * low_above_cutoff
    mass_high_below = weight_high * high_below_cutoff
    mass_low_below = weight_low * low_below_cutoff

    # Current manuscript terminology.
    high_angle_fraction_above_cutoff = mass_high_above / (
        mass_high_above + mass_low_above
    )
    high_angle_retention = mass_high_above / (
        mass_high_above + mass_high_below
    )

    # Component-separation score: harmonic mean of the two cutoff-based
    # component-separation quantities. It is deliberately not called F1 because
    # no biological ground-truth labels enter this calculation.
    separation_denominator = (
        high_angle_fraction_above_cutoff + high_angle_retention
    )
    if separation_denominator > 0:
        component_separation_score = (
            2.0
            * high_angle_fraction_above_cutoff
            * high_angle_retention
            / separation_denominator
        )
    else:
        component_separation_score = np.nan

    # Convert model-estimated fractions into estimated counts.
    # These are expected counts under the fitted model, not observed labels.
    if n_total is not None:
        n_total_float = float(n_total)
        estimated_low_component_count = n_total_float * weight_low
        estimated_high_component_count = n_total_float * weight_high
        estimated_high_angle_above_cutoff_count = n_total_float * mass_high_above
        estimated_low_angle_above_cutoff_count = n_total_float * mass_low_above
        estimated_high_angle_below_cutoff_count = n_total_float * mass_high_below
        estimated_low_angle_below_cutoff_count = n_total_float * mass_low_below
    else:
        n_total_float = np.nan
        estimated_low_component_count = np.nan
        estimated_high_component_count = np.nan
        estimated_high_angle_above_cutoff_count = np.nan
        estimated_low_angle_above_cutoff_count = np.nan
        estimated_high_angle_below_cutoff_count = np.nan
        estimated_low_angle_below_cutoff_count = np.nan

    def rounded_count(value: float) -> int | float:
        return int(np.rint(value)) if np.isfinite(value) else np.nan

    return {
        "cutoff_angle_deg": float(cutoff_deg),
        "n_total": n_total_float,
        "low_component_weight": weight_low,
        "high_component_weight": weight_high,
        "model_estimated_low_component_count": float(estimated_low_component_count),
        "model_estimated_high_component_count": float(estimated_high_component_count),
        "high_angle_above_cutoff_mass_fraction": float(mass_high_above),
        "low_angle_above_cutoff_mass_fraction": float(mass_low_above),
        "high_angle_below_cutoff_mass_fraction": float(mass_high_below),
        "low_angle_below_cutoff_mass_fraction": float(mass_low_below),
        "model_estimated_high_angle_above_cutoff_count": float(
            estimated_high_angle_above_cutoff_count
        ),
        "model_estimated_low_angle_above_cutoff_count": float(
            estimated_low_angle_above_cutoff_count
        ),
        "model_estimated_high_angle_below_cutoff_count": float(
            estimated_high_angle_below_cutoff_count
        ),
        "model_estimated_low_angle_below_cutoff_count": float(
            estimated_low_angle_below_cutoff_count
        ),
        "model_estimated_high_angle_above_cutoff_count_rounded": rounded_count(
            estimated_high_angle_above_cutoff_count
        ),
        "model_estimated_low_angle_above_cutoff_count_rounded": rounded_count(
            estimated_low_angle_above_cutoff_count
        ),
        "model_estimated_high_angle_below_cutoff_count_rounded": rounded_count(
            estimated_high_angle_below_cutoff_count
        ),
        "model_estimated_low_angle_below_cutoff_count_rounded": rounded_count(
            estimated_low_angle_below_cutoff_count
        ),
        "high_angle_fraction_above_cutoff": float(high_angle_fraction_above_cutoff),
        "high_angle_fraction_above_cutoff_percent": float(
            100.0 * high_angle_fraction_above_cutoff
        ),
        "high_angle_retention": float(high_angle_retention),
        "high_angle_retention_percent": float(100.0 * high_angle_retention),
        "component_separation_score": float(component_separation_score),
        "component_separation_score_percent": float(
            100.0 * component_separation_score
        ),
    }



def main(
    csv: str,
    result_dir: str = "result",
    xcol: str = "junction_angle_mean",
    exclude_exception_rows: bool = True,
    exception_column: str = "exception_label",
    exception_positive_value: int | float | str = 1,
    seed: int = 0,
    dpi: int = 600,
    aspect_ratio: float = 1.5,
    fig_width: float = 6.5,
    x_axis_label: str | None = "Mean junction angle (°)",
    y_axis_label: str = "Pair count",
    show_figure_titles: bool = False,
    color_non_daughter: str = "#4C79A2",
    color_daughter: str = "#DA6752",
    color_non_daughter_hist: str = "#BBCCDB",
    color_daughter_hist: str = "#F1C9C1",
    color_overall_hist: str = "#939393",
    color_overall_curve: str = "#4D4D4D",
    cutoff_line_color: str = "black",
    n_bins: int = 35,
    use_fixed_range: bool = False,
    fixed_min: float = 0.0,
    fixed_max: float = 180.0,
    xlim_min: float = 60.0,
    xlim_max: float = 180.0,
    use_shared_ymax: bool = True,
    fixed_ymax: float | None = None,
    ymax_margin: float = 1.08,
    legend_anchor_x: float = 1.02,
    legend_loc: str = "upper left",
    legend_area_width: float = 2.70,
    left_margin_width: float = 0.70,
    bottom_margin_height: float = 0.55,
    top_margin_height: float = 0.18,
    n_init: int = 15,
    max_iter: int = 400,
    resample_n: int = 0,
    out_fig: str = "vonmises_mixture_resampled_counts.png",
    out_fig_overall: str | None = None,
    out_fig_overall_raw: str | None = None,
    out_fig_observed_count: str = "observed_division_distribution_pair_count.png",
    out_fig_observed_frequency: str = "observed_division_distribution_pair_frequency.png",
    out_fig_observed_count_cutoff: str = "observed_division_distribution_pair_count_with_cutoff.png",
    out_fig_observed_frequency_cutoff: str = "observed_division_distribution_pair_frequency_with_cutoff.png",
    observed_label_best_threshold: float | None = 145.0,
    observed_label_cutoff_color: str = "#808080",
    out_fig_observed_labels: str = "observed_division_labeled_vonmises_distribution.png",
    out_observed_label_fit_csv: str | None = "observed_division_labeled_vonmises_fit_parameters.csv",
    plot_observed_labels_if_available: bool = True,
    plot_observed_label_fits_if_available: bool = False,
    observed_division_column: str = "auto",
    run_component_label_closeness: bool = True,
    closeness_bootstrap_replicates: int = 1000,
    closeness_bootstrap_group_column: str = "fileName",
    closeness_bootstrap_group_regex: str | None = (
        r"^(.+?)(?:[_-]\d+(?:\.\d+)?h)?(?:\.json)?$"
    ),
    closeness_bootstrap_max_iter: int = 200,
    closeness_bootstrap_seed: int = 0,
    out_closeness_csv: str = "component_label_distribution_closeness_summary.csv",
    out_closeness_bootstrap_csv: str = "component_label_distribution_closeness_bootstrap.csv",
    out_closeness_fig: str = "component_label_distribution_closeness_ecdf.png",
    out_post_csv: str = "vonmises2_posteriors_realdata.csv",
    out_component_summary_csv: str = "vonmises_cutoff_component_summary.csv",
    out_seven_terms_csv: str = "component_separation_seven_terms.csv",
    out_ks_json: str = "ks_group_comparisons.json",
    show_plots: bool = True,
):
    setup_publication_style()
    fig_height = fig_width / aspect_ratio

    # Force every export into result_dir. If OUT_FIG_OVERALL/RAW are None,
    # derive their names from OUT_FIG first, then place them in result_dir.
    if out_post_csv is None:
        out_post_csv = "vonmises2_posteriors_realdata.csv"
    if out_fig_overall is None:
        out_fig_overall = _derive_overall_outpath(out_fig)
    if out_fig_overall_raw is None:
        out_fig_overall_raw = _derive_overall_raw_outpath(out_fig)

    out_fig = _ensure_result_path(out_fig, result_dir)
    out_fig_overall = _ensure_result_path(out_fig_overall, result_dir)
    out_fig_overall_raw = _ensure_result_path(out_fig_overall_raw, result_dir)
    out_fig_observed_count = _ensure_result_path(out_fig_observed_count, result_dir)
    out_fig_observed_frequency = _ensure_result_path(out_fig_observed_frequency, result_dir)
    out_fig_observed_count_cutoff = _ensure_result_path(
        out_fig_observed_count_cutoff, result_dir
    )
    out_fig_observed_frequency_cutoff = _ensure_result_path(
        out_fig_observed_frequency_cutoff, result_dir
    )
    out_fig_observed_labels = _ensure_result_path(out_fig_observed_labels, result_dir)
    if out_observed_label_fit_csv is not None:
        out_observed_label_fit_csv = _ensure_result_path(out_observed_label_fit_csv, result_dir)
    out_closeness_csv = _ensure_result_path(out_closeness_csv, result_dir)
    out_closeness_bootstrap_csv = _ensure_result_path(
        out_closeness_bootstrap_csv, result_dir
    )
    out_closeness_fig = _ensure_result_path(out_closeness_fig, result_dir)
    out_post_csv = _ensure_result_path(out_post_csv, result_dir)
    out_component_summary_csv = _ensure_result_path(
        out_component_summary_csv, result_dir
    )
    out_seven_terms_csv = _ensure_result_path(out_seven_terms_csv, result_dir)
    out_ks_json = _ensure_result_path(out_ks_json, result_dir)

    df = pd.read_csv(csv)
    input_row_count = len(df)

    xcol = resolve_column_name(
        df,
        requested=xcol,
        aliases=TARGET_COLUMN_ALIASES,
        required=True,
        role="feature column",
    )
    resolved_observed_column = resolve_column_name(
        df,
        requested=observed_division_column,
        aliases=OBSERVED_DIVISION_COLUMN_ALIASES,
        required=False,
        role="observed-division label column",
    )
    observed_division_column = resolved_observed_column or ""

    if x_axis_label is None:
        x_axis_label = xcol

    print(f"Input CSV: {Path(csv).resolve()}")
    print(f"Feature analyzed: {xcol}")
    if resolved_observed_column is None:
        print("Observed-division label column: not available")
    else:
        print(f"Observed-division label column: {resolved_observed_column}")

    df, excluded_exception_count = exclude_exception_labeled_rows(
        df,
        exception_column=exception_column,
        exception_aliases=EXCEPTION_COLUMN_ALIASES,
        exception_positive_value=exception_positive_value,
        enabled=exclude_exception_rows,
    )
    print(
        f"Input rows: {input_row_count}; excluded exceptions: "
        f"{excluded_exception_count}; analysis rows: {len(df)}"
    )
    angles = pd.to_numeric(df[xcol], errors="coerce").to_numpy()
    valid = np.isfinite(angles)
    ang = angles[valid]

    if ang.size < 10:
        raise ValueError(f"Not enough valid numeric angles in '{xcol}' (found {ang.size}).")

    ang = np.clip(ang, 0.0, 180.0)

    bins = make_angle_bins(
        ang,
        n_bins=n_bins,
        use_fixed_range=use_fixed_range,
        fixed_min=fixed_min,
        fixed_max=fixed_max,
    )

    theta = angle_deg_to_theta_rad(ang)

    weights, mus, kappas, ll = fit_vonmises_mixture(
        theta, K=2, n_init=n_init, max_iter=max_iter, tol=1e-6, seed=seed
    )

    mean_angles = theta_rad_to_angle_deg(mus)
    order = np.argsort(mean_angles)
    weights = weights[order]
    mus = mus[order]
    kappas = kappas[order]
    mean_angles = mean_angles[order]

    print("=== 2-component von Mises mixture fit (axial; sorted by mean angle) ===")
    for k in range(2):
        print(
            f"Component {k}: weight={weights[k]:.6f}, "
            f"mean_angle_deg={mean_angles[k]:.6f}, kappa={kappas[k]:.6f}"
        )
    print(f"Log-likelihood: {ll:.3f}")

    theta_all = np.full(len(df), np.nan, dtype=float)
    theta_all[valid] = angle_deg_to_theta_rad(np.clip(angles[valid], 0.0, 180.0))

    resp_full = np.full((len(df), 2), np.nan, dtype=float)
    label_full = np.full(len(df), np.nan, dtype=float)

    t_valid = theta_all[valid]
    resp_valid = posterior_probabilities(t_valid, weights, mus, kappas)

    resp_full[valid, :] = resp_valid
    label_full[valid] = np.argmax(resp_valid, axis=1)

    df_out = df.copy()
    df_out["vm_p_comp0"] = resp_full[:, 0]
    df_out["vm_p_comp1"] = resp_full[:, 1]
    df_out["vm_label"] = label_full
    df_out.to_csv(out_post_csv, index=False)
    print(f"Saved von Mises posteriors for real data to: {out_post_csv}")

    label_status = observed_label_class_status(
        df,
        observed_division_column,
        valid_feature_mask=valid,
    )
    if label_status["column_available"]:
        print(
            "Observed real-label counts among valid feature rows: "
            f"label=0: {label_status['n_label_0']}; "
            f"label=1: {label_status['n_label_1']}"
        )

    if run_component_label_closeness:
        if not label_status["column_available"]:
            print(
                "Skipped component-label closeness: no supported real division "
                "label column was found."
            )
            _remove_stale_output(out_closeness_csv)
            _remove_stale_output(out_closeness_bootstrap_csv)
            _remove_stale_output(out_closeness_fig)
        elif not label_status["has_label_1"]:
            print(
                "Skipped component-label closeness: the CSV contains no real "
                "division label=1. Mixture-component separation is still analyzed."
            )
            _remove_stale_output(out_closeness_csv)
            _remove_stale_output(out_closeness_bootstrap_csv)
            _remove_stale_output(out_closeness_fig)
        elif not label_status["has_label_0"]:
            print(
                "Skipped component-label closeness: the CSV contains real "
                "division label=1 but no label=0 comparison group."
            )
            _remove_stale_output(out_closeness_csv)
            _remove_stale_output(out_closeness_bootstrap_csv)
            _remove_stale_output(out_closeness_fig)
        else:
            df_valid = df.loc[valid].copy().reset_index(drop=True)
            run_component_label_closeness_analysis(
                df_valid=df_valid,
                angles=ang,
                responsibilities=resp_valid,
                fitted_weights=weights,
                fitted_mus=mus,
                fitted_kappas=kappas,
                observed_col=observed_division_column,
                out_summary_csv=out_closeness_csv,
                out_bootstrap_csv=out_closeness_bootstrap_csv,
                out_fig=out_closeness_fig,
                bootstrap_replicates=closeness_bootstrap_replicates,
                bootstrap_group_column=closeness_bootstrap_group_column,
                bootstrap_group_regex=closeness_bootstrap_group_regex,
                bootstrap_max_iter=closeness_bootstrap_max_iter,
                bootstrap_seed=closeness_bootstrap_seed,
                dpi=dpi,
                xlim_min=xlim_min,
                xlim_max=xlim_max,
                color_low=color_non_daughter,
                color_high=color_daughter,
                show_plots=show_plots,
            )

    rng = np.random.default_rng(seed)
    N_real = ang.size
    N_resample = N_real if resample_n == 0 else resample_n

    z = rng.choice(2, size=N_resample, p=weights)

    theta_s = np.empty(N_resample, dtype=float)
    for k in range(2):
        idx = np.where(z == k)[0]
        if idx.size == 0:
            continue
        theta_s[idx] = rng.vonmises(mus[k], kappas[k], size=idx.size) % (2.0 * np.pi)

    ang_s = theta_rad_to_angle_deg(theta_s)

    non_daughter_sim = ang_s[z == 0]
    daughter_sim = ang_s[z == 1]

    ks = ks_2samp(ang, ang_s, alternative="two-sided", mode="auto")
    print("\n=== KS test (original angles vs combined resampled angles) ===")
    print(f"D = {ks.statistic:.6f}")
    print(f"p = {ks.pvalue:.3e}")
    print(f"N_original = {ang.size}, N_resampled = {ang_s.size}")

    def log_component(theta_val, w, mu, kappa):
        return (
            np.log(w + 1e-300)
            + kappa * np.cos(theta_val - mu)
            - (np.log(2.0 * np.pi) + np.log(i0(kappa)))
        )

    def g_angle_deg(a_deg):
        th = angle_deg_to_theta_rad(np.array([a_deg], dtype=float))[0]
        return log_component(th, weights[0], mus[0], kappas[0]) - log_component(
            th, weights[1], mus[1], kappas[1]
        )

    a_lo = float(mean_angles[0])
    a_hi = float(mean_angles[1])

    grid = np.linspace(a_lo, a_hi, 4001)
    vals = np.array([g_angle_deg(a) for a in grid])

    sign = np.sign(vals)
    idx = np.where(sign[:-1] * sign[1:] < 0)[0]

    if idx.size > 0:
        i = idx[0]
        cutoff_deg = brentq(g_angle_deg, grid[i], grid[i + 1])
    else:
        cutoff_deg = float(grid[np.argmin(np.abs(vals))])

    print(f"\n=== Mixture cut-off (low vs high component) ===")
    print(f"cutoff_angle_deg = {cutoff_deg:.3f}")
    print("Rule: angle >= cutoff -> high-angle component")

    # === Four raw observed-label distributions, with no fitted curves ===
    # By default the cutoff versions use the supervised best threshold (145°).
    # Setting observed_label_best_threshold=None uses the fitted mixture cutoff.
    if plot_observed_labels_if_available:
        raw_label_cutoff = (
            float(cutoff_deg)
            if observed_label_best_threshold is None
            else float(observed_label_best_threshold)
        )
        if not np.isfinite(raw_label_cutoff):
            raise ValueError("observed_label_best_threshold must be finite or None.")
        plot_observed_division_raw_distributions(
            df=df,
            xcol=xcol,
            cutoff_deg=raw_label_cutoff,
            observed_col=observed_division_column,
            bins=bins,
            out_count=out_fig_observed_count,
            out_frequency=out_fig_observed_frequency,
            out_count_cutoff=out_fig_observed_count_cutoff,
            out_frequency_cutoff=out_fig_observed_frequency_cutoff,
            dpi=dpi,
            fig_width=fig_width,
            aspect_ratio=aspect_ratio,
            x_axis_label=x_axis_label,
            color_non_daughter_hist=color_non_daughter_hist,
            color_daughter_hist=color_daughter_hist,
            cutoff_color=observed_label_cutoff_color,
            xlim_min=xlim_min,
            xlim_max=xlim_max,
            ymax_margin=ymax_margin,
            legend_anchor_x=legend_anchor_x,
            legend_loc=legend_loc,
            legend_area_width=legend_area_width,
            left_margin_width=left_margin_width,
            bottom_margin_height=bottom_margin_height,
            top_margin_height=top_margin_height,
            show_figure_titles=show_figure_titles,
            show_plots=show_plots,
        )

    # === Requested KS comparisons among cutoff-defined and observed-label groups ===
    # The low/high angle groups are defined directly by the fitted cutoff.
    # The daughter/non-daughter groups come from OBSERVED_DIVISION_COLUMN when available.
    ks_group_stats = export_ks_group_comparisons_json(
        df=df,
        xcol=xcol,
        cutoff_deg=cutoff_deg,
        out_json=out_ks_json,
        observed_col=observed_division_column,
    )

    print("\n=== KS tests for cutoff-defined groups and observed daughter labels ===")
    for comparison_name, comparison in ks_group_stats["comparisons"].items():
        if comparison["status"] == "ok":
            print(
                f"{comparison_name}: "
                f"D = {comparison['statistic_D']:.6f}, "
                f"p = {comparison['pvalue']:.3e}, "
                f"n_a = {comparison['n_group_a']}, "
                f"n_b = {comparison['n_group_b']}"
            )
        else:
            print(
                f"{comparison_name}: {comparison['status']} "
                f"(n_a = {comparison['n_group_a']}, n_b = {comparison['n_group_b']})"
            )

    # === Cutoff-based summary of fitted mixture components ===
    # This does not use biological labels and does not use bootstrap.
    component_summary = model_estimated_cutoff_summary(
        cutoff_deg=cutoff_deg,
        weights=weights,
        mus=mus,
        kappas=kappas,
        n_total=N_real,
    )

    observed_predicted_high_count = int(np.sum(ang >= cutoff_deg))
    observed_predicted_low_count = int(np.sum(ang < cutoff_deg))

    output_summary = {
        "input_csv": str(Path(csv).resolve()),
        "feature_column": xcol,
        "observed_label_column": resolved_observed_column or "",
        "input_row_count": int(input_row_count),
        "excluded_exception_row_count": int(excluded_exception_count),
        "analysis_row_count": int(len(df)),
        "valid_feature_row_count": int(N_real),
        "mixture_log_likelihood": float(ll),
        "low_component_mean_angle_deg": float(mean_angles[0]),
        "high_component_mean_angle_deg": float(mean_angles[1]),
        "low_component_kappa": float(kappas[0]),
        "high_component_kappa": float(kappas[1]),
        **component_summary,
        "observed_below_cutoff_count": observed_predicted_low_count,
        "observed_above_cutoff_count": observed_predicted_high_count,
    }
    pd.DataFrame([output_summary]).to_csv(out_component_summary_csv, index=False)
    export_component_separation_seven_terms(
        component_summary=component_summary,
        out_csv=out_seven_terms_csv,
    )

    print("\n=== Cutoff-based fitted-component summary ===")
    print(f"Feature = {xcol}")
    print(f"Saved component summary to: {out_component_summary_csv}")
    print(f"N analyzed = {N_real}")
    print(f"Model-derived cutoff = {cutoff_deg:.3f}°")
    print("\nSeven manuscript-facing component-separation terms:")
    print(
        "High-angle component above cutoff = "
        f"{component_summary['model_estimated_high_angle_above_cutoff_count_rounded']}"
    )
    print(
        "Low-angle component above cutoff  = "
        f"{component_summary['model_estimated_low_angle_above_cutoff_count_rounded']}"
    )
    print(
        "High-angle component below cutoff = "
        f"{component_summary['model_estimated_high_angle_below_cutoff_count_rounded']}"
    )
    print(
        "Low-angle component below cutoff  = "
        f"{component_summary['model_estimated_low_angle_below_cutoff_count_rounded']}"
    )
    print(
        "\nHigh-angle fraction above cutoff = "
        f"{component_summary['high_angle_fraction_above_cutoff']:.4f} "
        f"({component_summary['high_angle_fraction_above_cutoff_percent']:.2f}%)"
    )
    print(
        "High-angle retention = "
        f"{component_summary['high_angle_retention']:.4f} "
        f"({component_summary['high_angle_retention_percent']:.2f}%)"
    )
    print(
        "Component-separation score = "
        f"{component_summary['component_separation_score']:.4f} "
        f"({component_summary['component_separation_score_percent']:.2f}%)"
    )
    print(
        "These are cutoff-based fitted-component measures, not biological "
        "classification metrics."
    )

    bin_width = bins[1] - bins[0]

    x_min, x_max = bins[0], bins[-1]
    xx_ang = np.linspace(x_min, x_max, 1400)
    xx_theta = angle_deg_to_theta_rad(xx_ang)

    dtheta_da = 2.0 * np.pi / 180.0
    comp0_pdf_ang = weights[0] * vonmises_pdf(xx_theta, mus[0], kappas[0]) * dtheta_da
    comp1_pdf_ang = weights[1] * vonmises_pdf(xx_theta, mus[1], kappas[1]) * dtheta_da
    mix_pdf_ang = comp0_pdf_ang + comp1_pdf_ang

    comp0_counts = N_resample * bin_width * comp0_pdf_ang
    comp1_counts = N_resample * bin_width * comp1_pdf_ang

    comp0_counts_real = N_real * bin_width * comp0_pdf_ang
    comp1_counts_real = N_real * bin_width * comp1_pdf_ang
    mix_counts_real = N_real * bin_width * mix_pdf_ang

    # Optional real-label summary to support a shared y-axis across all figures.
    observed_plot_kwargs = None
    if plot_observed_label_fits_if_available and observed_division_column in df.columns:
        angle_values = pd.to_numeric(df[xcol], errors="coerce")
        label_values = pd.to_numeric(df[observed_division_column], errors="coerce")
        valid_obs = angle_values.notna() & label_values.isin([0, 1])
        if int(valid_obs.sum()) > 0:
            obs_df = pd.DataFrame({
                "angle": angle_values[valid_obs].astype(float).clip(0.0, 180.0),
                "observed_division": label_values[valid_obs].astype(int),
            })
            daughter_obs = obs_df.loc[obs_df["observed_division"] == 1, "angle"].to_numpy()
            non_daughter_obs = obs_df.loc[obs_df["observed_division"] == 0, "angle"].to_numpy()
            if daughter_obs.size >= 2 and non_daughter_obs.size >= 2:
                fit_non_daughter_obs = fit_single_vonmises_axial(non_daughter_obs)
                fit_daughter_obs = fit_single_vonmises_axial(daughter_obs)
                non_daughter_curve_obs = vonmises_count_curve_for_angles(
                    xx_ang,
                    mu=fit_non_daughter_obs["mu_theta_rad"],
                    kappa=fit_non_daughter_obs["kappa"],
                    n_observations=non_daughter_obs.size,
                    bin_width=bin_width,
                )
                daughter_curve_obs = vonmises_count_curve_for_angles(
                    xx_ang,
                    mu=fit_daughter_obs["mu_theta_rad"],
                    kappa=fit_daughter_obs["kappa"],
                    n_observations=daughter_obs.size,
                    bin_width=bin_width,
                )
                obs_non_counts, _ = np.histogram(non_daughter_obs, bins=bins)
                obs_dau_counts, _ = np.histogram(daughter_obs, bins=bins)
                observed_plot_kwargs = dict(
                    df=df,
                    xcol=xcol,
                    observed_col=observed_division_column,
                    bins=bins,
                    out_fig=out_fig_observed_labels,
                    out_fit_csv=out_observed_label_fit_csv,
                    dpi=dpi,
                    fig_width=fig_width,
                    aspect_ratio=aspect_ratio,
                    x_axis_label=x_axis_label,
                    y_axis_label=y_axis_label,
                    show_figure_titles=show_figure_titles,
                    color_non_daughter_hist=color_non_daughter_hist,
                    color_daughter_hist=color_daughter_hist,
                    color_non_daughter_line=color_non_daughter,
                    color_daughter_line=color_daughter,
                    xlim_min=xlim_min,
                    xlim_max=xlim_max,
                    legend_anchor_x=legend_anchor_x,
                    legend_loc=legend_loc,
                    legend_area_width=legend_area_width,
                    left_margin_width=left_margin_width,
                    bottom_margin_height=bottom_margin_height,
                    top_margin_height=top_margin_height,
                    show_plots=show_plots,
                )
            else:
                obs_non_counts = obs_dau_counts = non_daughter_curve_obs = daughter_curve_obs = None
        else:
            obs_non_counts = obs_dau_counts = non_daughter_curve_obs = daughter_curve_obs = None
    else:
        obs_non_counts = obs_dau_counts = non_daughter_curve_obs = daughter_curve_obs = None

    sim_non_counts, _ = np.histogram(non_daughter_sim, bins=bins)
    sim_dau_counts, _ = np.histogram(daughter_sim, bins=bins)
    all_counts, _ = np.histogram(ang, bins=bins)

    if fixed_ymax is not None:
        shared_ymax = float(fixed_ymax)
    elif use_shared_ymax:
        shared_ymax = compute_plot_ymax(
            sim_non_counts, sim_dau_counts, comp0_counts, comp1_counts,
            all_counts, comp0_counts_real, comp1_counts_real, mix_counts_real,
            obs_non_counts, obs_dau_counts, non_daughter_curve_obs, daughter_curve_obs,
            margin=ymax_margin,
            minimum=1.0,
        )
    else:
        shared_ymax = None

    if shared_ymax is not None:
        print(f"Using shared ymax for all figures: {shared_ymax:.2f}")

    # Optional legacy real-label plot with separate one-component von Mises fits.
    # Disabled by default because the four requested observed-label plots above
    # contain raw histograms only.
    if observed_plot_kwargs is not None:
        observed_plot_kwargs["shared_ymax"] = shared_ymax
        plot_observed_division_labeled_distribution(**observed_plot_kwargs)

    fig, ax = create_fixed_main_plot_figure(
        fig_width=fig_width,
        aspect_ratio=aspect_ratio,
        legend_area_width=legend_area_width,
        left_margin_width=left_margin_width,
        bottom_margin_height=bottom_margin_height,
        top_margin_height=top_margin_height,
    )

    plt.hist(
        non_daughter_sim,
        bins=bins,
        color=color_non_daughter_hist,
        alpha=0.95,
        edgecolor="white",
        linewidth=0.45,
        label="Low-angle component",
        zorder=1,
    )
    plt.hist(
        daughter_sim,
        bins=bins,
        color=color_daughter_hist,
        alpha=0.95,
        edgecolor="white",
        linewidth=0.45,
        label="High-angle component",
        zorder=2,
    )

    plt.plot(xx_ang, comp0_counts, linewidth=2.1, color=color_non_daughter, alpha=1.0, zorder=4)
    plt.plot(xx_ang, comp1_counts, linewidth=2.1, color=color_daughter, alpha=1.0, zorder=5)

    if show_figure_titles:
        plt.title("von Mises mixture-resampled distributions", pad=5)
    plt.xlabel(x_axis_label)
    plt.ylabel(y_axis_label)

    draw_cutoff_line(ax, cutoff_deg, label=f"Cutoff = {cutoff_deg:.1f}°", color=cutoff_line_color)
    style_ax(ax)
    ax.set_xlim(xlim_min, xlim_max)
    apply_shared_ymax(ax, shared_ymax)
    add_clean_legend(ax, loc=legend_loc, anchor_x=legend_anchor_x)

    plt.savefig(out_fig, dpi=dpi, facecolor="white")
    if show_plots:
        plt.show()
    else:
        plt.close()

    print("Saved figure:", out_fig)

    fig2, ax2 = create_fixed_main_plot_figure(
        fig_width=fig_width,
        aspect_ratio=aspect_ratio,
        legend_area_width=legend_area_width,
        left_margin_width=left_margin_width,
        bottom_margin_height=bottom_margin_height,
        top_margin_height=top_margin_height,
    )

    plt.hist(
        ang,
        bins=bins,
        color=color_overall_hist,
        alpha=0.82,
        edgecolor="white",
        linewidth=0.45,
        label="All pairs",
        zorder=1,
    )

    plt.plot(
        xx_ang,
        comp0_counts_real,
        linewidth=2.1,
        color=color_non_daughter,   # blue
        alpha=1.0,
        label="Low-angle component",
        zorder=3,
    )
    plt.plot(
        xx_ang,
        comp1_counts_real,
        linewidth=2.1,
        color=color_daughter,       # red
        alpha=1.0,
        label="High-angle component",
        zorder=4,
    )

    plt.plot(
        xx_ang,
        mix_counts_real,
        linewidth=2.2,
        color=color_overall_curve,   # grey
        alpha=0.95,
        label="Mixture sum",
        linestyle=(0, (3.2, 1.7)),
        zorder=5,
    )

    if show_figure_titles:
        plt.title("Overall distribution + fitted von Mises mixture", pad=5)
    plt.xlabel(x_axis_label)
    plt.ylabel(y_axis_label)

    # Requested: add the cutoff line to vonmises_mixture_resampled_overall_grey.
    draw_cutoff_line(ax2, cutoff_deg, label=f"Cutoff = {cutoff_deg:.1f}°", color=cutoff_line_color)
    style_ax(ax2)
    ax2.set_xlim(xlim_min, xlim_max)
    apply_shared_ymax(ax2, shared_ymax)
    add_clean_legend(ax2, loc=legend_loc, anchor_x=legend_anchor_x)

    plt.savefig(out_fig_overall, dpi=dpi, facecolor="white")
    if show_plots:
        plt.show()
    else:
        plt.close()

    print("Saved overall-grey figure:", out_fig_overall)

    # Overall distribution only: grey histogram, no fitting lines.
    fig3, ax3 = create_fixed_main_plot_figure(
        fig_width=fig_width,
        aspect_ratio=aspect_ratio,
        legend_area_width=legend_area_width,
        left_margin_width=left_margin_width,
        bottom_margin_height=bottom_margin_height,
        top_margin_height=top_margin_height,
    )

    plt.hist(
        ang,
        bins=bins,
        color=color_overall_hist,
        alpha=0.82,
        edgecolor="white",
        linewidth=0.45,
        label="All pairs",
        zorder=1,
    )

    if show_figure_titles:
        plt.title("Overall distribution", pad=5)
    plt.xlabel(x_axis_label)
    plt.ylabel(y_axis_label)

    style_ax(ax3)
    ax3.set_xlim(xlim_min, xlim_max)
    apply_shared_ymax(ax3, shared_ymax)
    add_clean_legend(ax3, loc=legend_loc, anchor_x=legend_anchor_x)

    plt.savefig(out_fig_overall_raw, dpi=dpi, facecolor="white")
    if show_plots:
        plt.show()
    else:
        plt.close()

    print("Saved overall raw-grey figure:", out_fig_overall_raw)

def parse_command_line() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Fit a two-component axial von Mises mixture to a junction-angle "
            "CSV and report the seven cutoff-based component-separation terms."
        )
    )
    parser.add_argument(
        "csv",
        nargs="?",
        default=None,
        help="Input neighbor-pair geometry CSV (overrides INPUT_CSV).",
    )
    parser.add_argument(
        "--feature",
        default=None,
        help=(
            "Feature column to analyze (default: TARGET_COLUMN). Use 'auto' "
            "to select the first supported junction-angle alias."
        ),
    )
    parser.add_argument(
        "--result-dir",
        default=None,
        help="Output directory (overrides RESULT_DIR).",
    )
    parser.add_argument(
        "--observed-label-column",
        default=None,
        help=(
            "Observed lineage-label column. The default 'auto' supports both "
            "observed_division and is_real_division."
        ),
    )
    parser.add_argument(
        "--no-component-label-closeness",
        action="store_true",
        help="Skip the optional label-versus-component bootstrap analysis.",
    )
    parser.add_argument(
        "--no-observed-label-plots",
        action="store_true",
        help="Skip optional plots based on observed lineage labels.",
    )
    parser.add_argument(
        "--bootstrap-replicates",
        type=int,
        default=None,
        help="Bootstrap replicate count for component-label closeness.",
    )
    parser.add_argument(
        "--dpi",
        type=int,
        default=None,
        help="Figure resolution in dots per inch.",
    )
    parser.add_argument(
        "--show-plots",
        action="store_true",
        help="Display figures interactively in addition to saving them.",
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_command_line()
    config = dict(RUN_CONFIG)
    if args.csv is not None:
        config["csv"] = args.csv
    if args.feature is not None:
        config["xcol"] = args.feature
    if args.result_dir is not None:
        config["result_dir"] = args.result_dir
    if args.observed_label_column is not None:
        config["observed_division_column"] = args.observed_label_column
    if args.no_component_label_closeness:
        config["run_component_label_closeness"] = False
    if args.no_observed_label_plots:
        config["plot_observed_labels_if_available"] = False
    if args.bootstrap_replicates is not None:
        config["closeness_bootstrap_replicates"] = args.bootstrap_replicates
    if args.dpi is not None:
        config["dpi"] = args.dpi
    if args.show_plots:
        config["show_plots"] = True
    main(**config)
