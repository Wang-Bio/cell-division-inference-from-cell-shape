import json
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

# ============================================================
# Science-style combined plot with before-division extension to 100
# and a grey dotted y=145 reference line
#
# Outputs:
#   result/temporal_analysis_combined_values.csv
#   result/temporal_analysis_mean_sd_by_time_frame.csv
#   result/temporal_analysis_summary.json
#   result/temporal_analysis_combined_plot.png
# ============================================================

SOURCE_CSV = Path("temporal_change_tracks.csv")

OUTPUT_DIR = Path("result")
OUTPUT_PREFIX = OUTPUT_DIR / "temporal_analysis"

BEFORE_TRACK_ID = 11
AFTER_TRACK_IDS = [21, 22]

PLOT_BEFORE_REPEATS = False
EXTEND_BEFORE_TO_100 = True
PLOT_AFTER_INDIVIDUAL_POINTS = False

AFTER_X_POSITIONS = {
    "1 cycle": 100,
    "1 cycle + 2h": 120,
}
TIME_LABELS = ["1 cycle", "1 cycle + 2h"]

# Publication-style colors
# Track 11 and 21 are in the red family; track 22 is light blue.
COLOR_TRACK11 = "#B2182B"   # deep red
COLOR_TRACK21 = "#D6604D"   # warm coral-red
COLOR_TRACK22 = "#92C5DE"   # light blue

ALPHA_SD_TRACK11 = 0.16
ALPHA_SD_TRACK21 = 0.22
ALPHA_SD_TRACK22 = 0.24

# Reference line
REFERENCE_Y = 145
REFERENCE_COLOR = "0.45"

# Add a blank left margin before the first before-division point at x=0
PLOT_X_MIN = -10
PLOT_X_MAX = 125


def load_clean_csv(path):
    df_raw = pd.read_csv(path)
    df = df_raw[df_raw["repeat_i"].astype(str).str.strip() != "repeat_i"].copy()

    numeric_cols = [
        "repeat_i",
        "track",
        "frame",
        "junctionAngleAverageDegrees",
        "pairIndex",
        "firstPolygonId",
        "secondPolygonId",
    ]

    for col in numeric_cols:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")

    df = df.dropna(
        subset=["repeat_i", "track", "frame", "junctionAngleAverageDegrees"]
    )
    return df


def to_jsonable(value):
    """Convert numpy/pandas values into JSON-safe Python values."""
    if pd.isna(value):
        return None
    if isinstance(value, (np.integer,)):
        return int(value)
    if isinstance(value, (np.floating,)):
        return float(value)
    if isinstance(value, (np.ndarray,)):
        return [to_jsonable(v) for v in value]
    return value


def linear_fit_summary(df, x_col, y_col):
    """Return y = slope * x + intercept linear-fit summary."""
    data = df[[x_col, y_col]].replace([np.inf, -np.inf], np.nan).dropna().copy()

    n_points = int(len(data))
    n_unique_x = int(data[x_col].nunique()) if n_points > 0 else 0

    if n_points < 2 or n_unique_x < 2:
        return {
            "status": "not fitted",
            "reason": "fewer than two valid points or fewer than two unique x values",
            "n_points": n_points,
            "n_unique_x": n_unique_x,
            "x_col": x_col,
            "y_col": y_col,
        }

    x = data[x_col].to_numpy(dtype=float)
    y = data[y_col].to_numpy(dtype=float)

    slope, intercept = np.polyfit(x, y, 1)
    y_pred = slope * x + intercept
    residuals = y - y_pred

    ss_res = float(np.sum(residuals ** 2))
    ss_tot = float(np.sum((y - np.mean(y)) ** 2))
    r_squared = None if ss_tot == 0 else float(1 - ss_res / ss_tot)

    if np.std(x, ddof=1) == 0 or np.std(y, ddof=1) == 0:
        pearson_r = None
    else:
        pearson_r = float(np.corrcoef(x, y)[0, 1])

    residual_std_error = None
    if n_points > 2:
        residual_std_error = float(np.sqrt(ss_res / (n_points - 2)))

    return {
        "status": "fitted",
        "model": "y = slope * x + intercept",
        "x_col": x_col,
        "y_col": y_col,
        "slope_deg_per_x_unit": float(slope),
        "intercept_deg": float(intercept),
        "r_squared": r_squared,
        "pearson_r": pearson_r,
        "residual_std_error_deg": residual_std_error,
        "n_points": n_points,
        "n_unique_x": n_unique_x,
        "x_min": float(np.min(x)),
        "x_max": float(np.max(x)),
        "y_mean_deg": float(np.mean(y)),
        "y_sd_deg": None if n_points < 2 else float(np.std(y, ddof=1)),
    }


# =========================
# Prepare output folder
# =========================
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)


# =========================
# Load source data once
# =========================
source_df = load_clean_csv(SOURCE_CSV)


# =========================
# Before division
# =========================
before_track = source_df[source_df["track"] == BEFORE_TRACK_ID].copy()

before_track = (
    before_track
    .groupby(["repeat_i", "frame"], as_index=False)["junctionAngleAverageDegrees"]
    .mean()
)
before_track = before_track.sort_values(["repeat_i", "frame"])

before_norm_list = []

for repeat_i, g in before_track.groupby("repeat_i"):
    g = g.sort_values("frame").copy()

    initial_frame = g["frame"].min()
    final_frame = g["frame"].max()
    denominator = final_frame + 1 - initial_frame

    g["normalized_time_percent"] = 100 * (g["frame"] - initial_frame) / denominator
    g["initial_frame"] = initial_frame
    g["final_frame"] = final_frame
    g["normalization_denominator"] = denominator

    before_norm_list.append(g)

if len(before_norm_list) == 0:
    raise ValueError(f"No before-division data found for track {BEFORE_TRACK_ID}.")

before_norm_df = pd.concat(before_norm_list, ignore_index=True)

# Extend to 100 using last-value carry-forward.
before_grid_end = 100 if EXTEND_BEFORE_TO_100 else before_norm_df["normalized_time_percent"].max()
before_grid = np.linspace(0, before_grid_end, 201)

before_interp = {}

for repeat_i, g in before_norm_df.groupby("repeat_i"):
    g = g.sort_values("normalized_time_percent")

    x = g["normalized_time_percent"].to_numpy()
    y = g["junctionAngleAverageDegrees"].to_numpy()

    right_value = y[-1] if EXTEND_BEFORE_TO_100 else np.nan

    y_interp = np.interp(
        before_grid,
        x,
        y,
        left=np.nan,
        right=right_value,
    )

    before_interp[int(repeat_i)] = y_interp

before_interp_df = pd.DataFrame(before_interp, index=before_grid)

before_stats = pd.DataFrame({
    "x": before_grid,
    "track": BEFORE_TRACK_ID,
    "phase": "before_division",
    "time_label": "before division normalized",
    "mean_junction_angle_deg": before_interp_df.mean(axis=1, skipna=True).to_numpy(),
    "sd_junction_angle_deg": before_interp_df.std(axis=1, ddof=1, skipna=True).to_numpy(),
    "n_values": before_interp_df.notna().sum(axis=1).to_numpy(),
    "n_repeat_i": before_interp_df.notna().sum(axis=1).to_numpy(),
})


# =========================
# After division
# =========================
after_df = source_df[source_df["track"].isin(AFTER_TRACK_IDS)].copy()

sort_cols = [
    col for col in
    ["repeat_i", "track", "frame", "pairIndex", "firstPolygonId", "secondPolygonId"]
    if col in after_df.columns
]
after_df = after_df.sort_values(sort_cols).copy()

labeled_after_list = []

for repeat_i, g_repeat in after_df.groupby("repeat_i"):
    frames = sorted(g_repeat["frame"].unique())

    if len(frames) < 2:
        print(f"Warning: repeat {int(repeat_i)} has fewer than two frames. Skipping.")
        continue

    if len(frames) != 2:
        print(
            f"Warning: repeat {int(repeat_i)} has {len(frames)} frames: {frames}. "
            "Only the first two frames are used."
        )

    used_frames = frames[:2]
    frame_to_label = {
        used_frames[0]: TIME_LABELS[0],
        used_frames[1]: TIME_LABELS[1],
    }

    g_repeat = g_repeat[g_repeat["frame"].isin(used_frames)].copy()
    g_repeat["time_label"] = g_repeat["frame"].map(frame_to_label)
    g_repeat["x"] = g_repeat["time_label"].map(AFTER_X_POSITIONS)

    labeled_after_list.append(g_repeat)

if len(labeled_after_list) == 0:
    raise ValueError(f"No after-division data found for tracks {AFTER_TRACK_IDS}.")

after_labeled_df = pd.concat(labeled_after_list, ignore_index=True)

after_labeled_df["occurrence_i"] = (
    after_labeled_df
    .groupby(["repeat_i", "track", "time_label"])
    .cumcount() + 1
)

after_stats = (
    after_labeled_df
    .groupby(["track", "time_label", "x"], as_index=False)
    .agg(
        mean_junction_angle_deg=("junctionAngleAverageDegrees", "mean"),
        sd_junction_angle_deg=("junctionAngleAverageDegrees", "std"),
        n_values=("junctionAngleAverageDegrees", "count"),
        n_repeat_i=("repeat_i", "nunique"),
    )
)
after_stats["phase"] = "after_division"


# =========================
# Mean/SD by time frame CSV export
# =========================
# This table is the direct source for the plotted mean ± SD values.
# Track 11 contains the before-division normalized time points (%).
# Tracks 21 and 22 contain the post-division positions defined in AFTER_X_POSITIONS.
before_stats_export = before_stats.copy()
before_stats_export["time_frame_percent"] = before_stats_export["x"]
before_stats_export["x_position"] = before_stats_export["x"]
before_stats_export["time_frame_type"] = "normalized_before_division_percent"

after_stats_export = after_stats.copy()
after_stats_export["time_frame_percent"] = after_stats_export["x"]
after_stats_export["x_position"] = after_stats_export["x"]
after_stats_export["time_frame_type"] = "post_division_aligned_position"

mean_sd_by_time_frame = pd.concat(
    [before_stats_export, after_stats_export],
    ignore_index=True,
    sort=False,
)

mean_sd_cols = [
    "phase",
    "track",
    "time_label",
    "time_frame_type",
    "time_frame_percent",
    "x_position",
    "mean_junction_angle_deg",
    "sd_junction_angle_deg",
    "n_values",
    "n_repeat_i",
]
mean_sd_cols = [col for col in mean_sd_cols if col in mean_sd_by_time_frame.columns]
mean_sd_by_time_frame = mean_sd_by_time_frame[mean_sd_cols]

mean_sd_csv_path = Path(f"{OUTPUT_PREFIX}_mean_sd_by_time_frame.csv")
mean_sd_by_time_frame.to_csv(mean_sd_csv_path, index=False)


# =========================
# Combined CSV export
# =========================
before_export = before_norm_df.copy()
before_export["phase"] = "before_division"
before_export["time_label"] = "before division normalized"
before_export["x"] = before_export["normalized_time_percent"]
before_export["occurrence_i"] = np.nan

after_export = after_labeled_df.copy()
after_export["phase"] = "after_division"
after_export["normalized_time_percent"] = np.nan
after_export["initial_frame"] = np.nan
after_export["final_frame"] = np.nan
after_export["normalization_denominator"] = np.nan

combined_values = pd.concat([before_export, after_export], ignore_index=True, sort=False)

preferred_cols = [
    "phase",
    "track",
    "repeat_i",
    "frame",
    "x",
    "time_label",
    "normalized_time_percent",
    "junctionAngleAverageDegrees",
    "occurrence_i",
    "initial_frame",
    "final_frame",
    "normalization_denominator",
    "pairIndex",
    "firstPolygonId",
    "secondPolygonId",
]
ordered_cols = [col for col in preferred_cols if col in combined_values.columns]
remaining_cols = [col for col in combined_values.columns if col not in ordered_cols]
combined_values = combined_values[ordered_cols + remaining_cols]

combined_csv_path = Path(f"{OUTPUT_PREFIX}_combined_values.csv")
combined_values.to_csv(combined_csv_path, index=False)


# =========================
# JSON summary export
# =========================
track_summaries = {}

# Track 11: fit both individual before values and the plotted mean curve.
track11_plot_df = before_stats.rename(columns={"mean_junction_angle_deg": "y_for_fit"})
track_summaries[str(BEFORE_TRACK_ID)] = {
    "phase": "before_division",
    "n_independent_values_for_fit": int(len(before_norm_df)),
    "n_repeat_i": int(before_norm_df["repeat_i"].nunique()),
    "n_plot_points": int(len(before_stats)),
    "linear_fit_independent_values": linear_fit_summary(
        before_norm_df.rename(columns={"normalized_time_percent": "x_for_fit"}),
        "x_for_fit",
        "junctionAngleAverageDegrees",
    ),
    "linear_fit_plotted_mean_curve": linear_fit_summary(
        track11_plot_df,
        "x",
        "y_for_fit",
    ),
}

# Tracks 21 and 22: fit both independent after values and the plotted mean points.
for track_id in AFTER_TRACK_IDS:
    g_values = after_labeled_df[after_labeled_df["track"] == track_id].copy()
    g_plot = (
        after_stats[after_stats["track"] == track_id]
        .sort_values("x")
        .rename(columns={"mean_junction_angle_deg": "y_for_fit"})
    )

    track_summaries[str(track_id)] = {
        "phase": "after_division",
        "n_independent_values_for_fit": int(len(g_values)),
        "n_repeat_i": int(g_values["repeat_i"].nunique()),
        "n_plot_points": int(len(g_plot)),
        "linear_fit_independent_values": linear_fit_summary(
            g_values,
            "x",
            "junctionAngleAverageDegrees",
        ),
        "linear_fit_plotted_mean_points": linear_fit_summary(
            g_plot,
            "x",
            "y_for_fit",
        ),
    }

summary = {
    "source_csv": str(SOURCE_CSV),
    "output_folder": str(OUTPUT_DIR),
    "combined_csv": str(combined_csv_path),
    "mean_sd_by_time_frame_csv": str(mean_sd_csv_path),
    "plot_png": f"{OUTPUT_PREFIX}_combined_plot.png",
    "settings": {
        "before_track_id": BEFORE_TRACK_ID,
        "after_track_ids": AFTER_TRACK_IDS,
        "extend_before_to_100": EXTEND_BEFORE_TO_100,
        "after_x_positions": AFTER_X_POSITIONS,
        "reference_y_deg": REFERENCE_Y,
        "plot_x_min": PLOT_X_MIN,
        "plot_x_max": PLOT_X_MAX,
    },
    "overall_counts": {
        "clean_source_rows": int(len(source_df)),
        "combined_export_rows": int(len(combined_values)),
        "mean_sd_by_time_frame_rows": int(len(mean_sd_by_time_frame)),
        "before_rows": int(len(before_norm_df)),
        "after_rows": int(len(after_labeled_df)),
        "n_repeat_i_total": int(combined_values["repeat_i"].nunique()),
    },
    "tracks": track_summaries,
}

summary = json.loads(json.dumps(summary, default=to_jsonable))
summary_json_path = Path(f"{OUTPUT_PREFIX}_summary.json")
with open(summary_json_path, "w", encoding="utf-8") as f:
    json.dump(summary, f, indent=2, ensure_ascii=False)


# =========================
# Plot
# =========================
fig, ax = plt.subplots(figsize=(8.6, 4.8))

# Grey dotted y=145 reference line
ax.axhline(
    REFERENCE_Y,
    color=REFERENCE_COLOR,
    linestyle=":",
    linewidth=1.8,
    alpha=0.85,
    label="145° reference",
)

x_before = before_stats["x"].to_numpy()
y_before = before_stats["mean_junction_angle_deg"].to_numpy()
sd_before = before_stats["sd_junction_angle_deg"].to_numpy()

ax.fill_between(
    x_before,
    y_before - sd_before,
    y_before + sd_before,
    alpha=ALPHA_SD_TRACK11,
    color=COLOR_TRACK11,
    linewidth=0,
    label="Before division: track 11 ± SD",
)

ax.plot(
    x_before,
    y_before,
    linewidth=3.2,
    color=COLOR_TRACK11,
    label="Before division: track 11 mean",
)

ax.axvline(0, linestyle="--", linewidth=1.1, alpha=0.55, color="0.45")
ax.axvline(100, linestyle="--", linewidth=1.1, alpha=0.55, color="0.45")

track_colors = {21: COLOR_TRACK21, 22: COLOR_TRACK22}
track_alphas = {21: ALPHA_SD_TRACK21, 22: ALPHA_SD_TRACK22}

for track_id in AFTER_TRACK_IDS:
    g_stats = after_stats[after_stats["track"] == track_id].sort_values("x")

    x_after = g_stats["x"].to_numpy()
    y_after = g_stats["mean_junction_angle_deg"].to_numpy()
    sd_after = g_stats["sd_junction_angle_deg"].to_numpy()
    color = track_colors[track_id]
    alpha_sd = track_alphas[track_id]

    ax.fill_between(
        x_after,
        y_after - sd_after,
        y_after + sd_after,
        alpha=alpha_sd,
        color=color,
        linewidth=0,
        label=f"After division: track {track_id} ± SD",
    )

    ax.plot(
        x_after,
        y_after,
        marker="o",
        markersize=5.8,
        linewidth=3.0,
        color=color,
        label=f"After division: track {track_id} mean",
    )

ax.set_xlabel("Aligned time: before-division normalized time (%) and post-division time")
ax.set_ylabel("Average junction angle (degrees)")
ax.set_title("Before and after division junction-angle dynamics")

ax.set_xlim(PLOT_X_MIN, PLOT_X_MAX)
ax.set_ylim(100, 180)

ax.set_xticks([0, 25, 50, 75, 100, 120])
ax.set_xticklabels(["0", "25", "50", "75", "100\n1 cycle", "1 cycle\n+2h"])

ax.grid(True, alpha=0.18)
ax.spines["top"].set_visible(False)
ax.spines["right"].set_visible(False)

fig.tight_layout()

fig.savefig(f"{OUTPUT_PREFIX}_combined_plot.png", dpi=300)

plt.show()
