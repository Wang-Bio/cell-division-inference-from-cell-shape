# LATEST VERSION: batch WT/rpl4d-3 analysis + junction-threshold/apical-boundary sensitivity
# Sensitivity: junction angle 140-150 deg; apical boundary y=0.40-0.60; PNG figures only.
from itertools import combinations
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import networkx as nx
from matplotlib.patches import Rectangle

# Put this script in the same folder as the CSV files.
# The folder containing this .py file is used as DATA_DIR.
DATA_DIR = Path(__file__).resolve().parent
OUTPUT_DIR = DATA_DIR / 'separate_region_figures_with_sensitivity'
DPI = 600

WT_BATCH_CSV = DATA_DIR / 'WT_fixed_samples_batch_single_geometry_estimation.csv'
RPL_BATCH_CSV = DATA_DIR / 'rpl4d-3_fixed_samples_batch_single_geometry_estimation.csv'

GROUPS = ('WT', 'rpl4d-3')
REGIONS = ('whole', 'apical', 'basal')
BINS = np.arange(0, 91, 10)

FIG_WIDTH = 7.5
ASPECT_RATIO = 1.2
FIG_HEIGHT = FIG_WIDTH / ASPECT_RATIO
HIST_YMAX = 42
# Extra canvas below the plotting area for notes; this is not counted in the main axes aspect ratio.
HIST_NOTE_HEIGHT = 0.55

VERTICAL_PROP_FIG_WIDTH = 2.45
VERTICAL_PROP_FIG_HEIGHT = 2.75
# Extra canvas below the plotting area for P value; this is not counted in the main figure aspect ratio.
VERTICAL_PROP_NOTE_HEIGHT = 0.38
VERTICAL_PROP_COLOR_WT = '#4F8A5B'
VERTICAL_PROP_COLOR_RPL = '#A8D19A'
VERTICAL_PROP_BOX_ALPHA = 0.72
VERTICAL_PROP_POINT_SIZE = 24
VERTICAL_PROP_POINT_EDGE_WIDTH = 0.35
VERTICAL_PROP_JITTER_WIDTH = 0.055
VERTICAL_PROP_JITTER_SEED = 12
VERTICAL_PROP_YLIM_MIN = 0.0
VERTICAL_PROP_YLIM_MAX = 1.0
VERTICAL_PROP_YTICK_STEP = 0.2
PERMUTATION_BATCH_SIZE = 8192
_ASSIGNMENT_BATCH_CACHE = {}

# Sensitivity analysis for Fig. S10G-I.
JUNCTION_THRESHOLDS = np.arange(140.0, 151.0, 1.0)
APICAL_BOUNDARIES = np.round(np.arange(0.40, 0.6001, 0.02), 2)
DEFAULT_JUNCTION_THRESHOLD = 145.0
DEFAULT_APICAL_BOUNDARY = 0.50
VERTICAL_ANGLE_MIN = 60.0
SENSITIVITY_FIG_WIDTH = 3.10
SENSITIVITY_FIG_HEIGHT = 2.75
SENSITIVITY_LINE_COLOR = '#4F8A5B'
SENSITIVITY_POINT_SIZE = 24



def read_csv_auto(fp: Path) -> pd.DataFrame:
    return pd.read_csv(fp, sep=None, engine='python')


def get_col(df: pd.DataFrame, name: str) -> str:
    cols = {str(c).lower(): c for c in df.columns}
    if name.lower() not in cols:
        raise ValueError(f"Column {name!r} not found in {list(df.columns)}")
    return cols[name.lower()]


def resolve_input_csv(preferred: Path) -> Path:
    """
    Resolve an input batch CSV.

    The preferred manuscript/repository filename is used first.  A '(1)' copy is
    accepted as a convenience when a browser/download has renamed a duplicate.
    """
    if preferred.exists():
        return preferred
    duplicate_name = preferred.with_name(f'{preferred.stem}(1){preferred.suffix}')
    if duplicate_name.exists():
        return duplicate_name
    raise FileNotFoundError(
        f'Could not find {preferred.name} (or {duplicate_name.name}) in {DATA_DIR}'
    )


def _unique_cell_centroids(snapshot: pd.DataFrame) -> pd.DataFrame:
    """Return one centroid coordinate per cell ID for one filename/primordium."""
    c1 = snapshot[[
        'cell_1_id', 'cell_1_centroid_x', 'cell_1_centroid_y'
    ]].rename(columns={
        'cell_1_id': 'cell_id',
        'cell_1_centroid_x': 'centroid_x',
        'cell_1_centroid_y': 'centroid_y',
    })
    c2 = snapshot[[
        'cell_2_id', 'cell_2_centroid_x', 'cell_2_centroid_y'
    ]].rename(columns={
        'cell_2_id': 'cell_id',
        'cell_2_centroid_x': 'centroid_x',
        'cell_2_centroid_y': 'centroid_y',
    })
    cells = pd.concat([c1, c2], ignore_index=True)
    cells['cell_id'] = pd.to_numeric(cells['cell_id'], errors='coerce')
    cells['centroid_x'] = pd.to_numeric(cells['centroid_x'], errors='coerce')
    cells['centroid_y'] = pd.to_numeric(cells['centroid_y'], errors='coerce')
    cells = cells.dropna(subset=['cell_id', 'centroid_x', 'centroid_y'])

    # The same cell can occur in many neighboring-pair rows.  Its centroid should
    # therefore be identical each time.  Check this before reducing to one row.
    spread = cells.groupby('cell_id')[['centroid_x', 'centroid_y']].agg(
        lambda x: float(np.max(x) - np.min(x))
    )
    if ((spread['centroid_x'] > 1e-6) | (spread['centroid_y'] > 1e-6)).any():
        bad = spread.loc[(spread['centroid_x'] > 1e-6) | (spread['centroid_y'] > 1e-6)].index.tolist()
        raise ValueError(f'Inconsistent centroid coordinates for cell IDs: {bad}')

    return cells.drop_duplicates(subset=['cell_id']).reset_index(drop=True)


def _division_orientation_deg(dx: np.ndarray, dy: np.ndarray) -> np.ndarray:
    """
    Angle of the line connecting the two inferred daughter-cell centroids.

    The angle is folded to 0-90 degrees, matching the original center_angle
    convention: 0 = horizontal, 90 = vertical.
    """
    angle = np.degrees(np.arctan2(np.abs(dy), np.abs(dx)))
    return np.clip(angle, 0.0, 90.0)


def collect_division_events(batch_csv: Path, genotype: str) -> pd.DataFrame:
    """
    Read one batch geometry/estimation CSV and reconstruct the event table that
    the original plotting code expected.

    Processing is done separately for every value of `filename`:
      1. Use all cell centroids in that primordium to define x/y minima and maxima.
      2. Keep rows with estimated_division == 1.
      3. Define each division-pair position as the midpoint of its two cell centroids.
      4. Normalize that midpoint to the primordium centroid extent:
           norm = (position - minimum cell centroid) / (maximum - minimum).
      5. Calculate division orientation from the line joining the two cell centroids.

    The existing regional split is intentionally retained unchanged:
    y_norm >= 0.5 -> apical; y_norm < 0.5 -> basal.
    """
    df = read_csv_auto(batch_csv)

    required = [
        'filename', 'estimated_division',
        'cell_1_id', 'cell_2_id',
        'cell_1_centroid_x', 'cell_1_centroid_y',
        'cell_2_centroid_x', 'cell_2_centroid_y',
    ]
    for col in required:
        get_col(df, col)

    # Standardize the required column names if capitalization differs.
    rename = {get_col(df, col): col for col in required}
    df = df.rename(columns=rename).copy()

    rows = []
    for filename, snapshot in df.groupby('filename', sort=True):
        snapshot = snapshot.copy()
        cells = _unique_cell_centroids(snapshot)
        if cells.empty:
            raise ValueError(f'No valid cell centroids found for {filename}')

        x_min = float(cells['centroid_x'].min())
        x_max = float(cells['centroid_x'].max())
        y_min = float(cells['centroid_y'].min())
        y_max = float(cells['centroid_y'].max())
        x_denom = x_max - x_min
        y_denom = y_max - y_min

        est = pd.to_numeric(snapshot['estimated_division'], errors='coerce')
        div = snapshot.loc[est == 1].copy()
        if div.empty:
            raise ValueError(f'No estimated divisions found for {filename}')

        numeric_cols = [
            'cell_1_centroid_x', 'cell_1_centroid_y',
            'cell_2_centroid_x', 'cell_2_centroid_y',
        ]
        for col in numeric_cols:
            div[col] = pd.to_numeric(div[col], errors='coerce')
        div = div.dropna(subset=numeric_cols)

        pair_x = (div['cell_1_centroid_x'].to_numpy(float) +
                  div['cell_2_centroid_x'].to_numpy(float)) / 2.0
        pair_y = (div['cell_1_centroid_y'].to_numpy(float) +
                  div['cell_2_centroid_y'].to_numpy(float)) / 2.0

        if x_denom == 0:
            x_norm = np.full_like(pair_x, 0.5, dtype=float)
        else:
            x_norm = (pair_x - x_min) / x_denom

        if y_denom == 0:
            y_norm = np.full_like(pair_y, 0.5, dtype=float)
        else:
            y_norm = (pair_y - y_min) / y_denom

        dx = div['cell_2_centroid_x'].to_numpy(float) - div['cell_1_centroid_x'].to_numpy(float)
        dy = div['cell_2_centroid_y'].to_numpy(float) - div['cell_1_centroid_y'].to_numpy(float)
        angle = _division_orientation_deg(dx, dy)

        region = np.where(y_norm >= 0.5, 'apical', 'basal')
        pid = f'{genotype}__{Path(str(filename)).stem}'

        out = pd.DataFrame({
            'genotype': genotype,
            'primordium_id': pid,
            'source_file': str(filename),
            'cell_1_id': div['cell_1_id'].to_numpy(),
            'cell_2_id': div['cell_2_id'].to_numpy(),
            'pair_centroid_x': pair_x,
            'pair_centroid_y': pair_y,
            'x_norm': x_norm,
            'y_norm': y_norm,
            'region': region,
            'angle': angle,
        })

        # Retain the junction-angle feature in the exported event table when present.
        if 'junction_angle_mean' in div.columns:
            out['junction_angle_mean'] = pd.to_numeric(
                div['junction_angle_mean'], errors='coerce'
            ).to_numpy()

        rows.append(out)

    return pd.concat(rows, ignore_index=True)


def collect_all_neighbor_pairs(batch_csv: Path, genotype: str) -> pd.DataFrame:
    """
    Read every neighboring pair from one batch CSV and add the normalized
    pair position and division-orientation angle needed for sensitivity tests.

    Unlike collect_division_events(), this function does NOT use the stored
    estimated_division label.  This is essential because changing the junction-
    angle threshold changes the candidate graph and therefore the global matching.
    """
    df = read_csv_auto(batch_csv)
    required = [
        'filename', 'cell_1_id', 'cell_2_id',
        'cell_1_centroid_x', 'cell_1_centroid_y',
        'cell_2_centroid_x', 'cell_2_centroid_y',
        'junction_angle_mean', 'estimated_division',
    ]
    for col in required:
        get_col(df, col)
    df = df.rename(columns={get_col(df, col): col for col in required}).copy()

    rows = []
    for filename, snapshot in df.groupby('filename', sort=True):
        snapshot = snapshot.copy()
        cells = _unique_cell_centroids(snapshot)
        if cells.empty:
            raise ValueError(f'No valid cell centroids found for {filename}')

        x_min = float(cells['centroid_x'].min())
        x_max = float(cells['centroid_x'].max())
        y_min = float(cells['centroid_y'].min())
        y_max = float(cells['centroid_y'].max())
        x_denom = x_max - x_min
        y_denom = y_max - y_min

        numeric_cols = [
            'cell_1_id', 'cell_2_id',
            'cell_1_centroid_x', 'cell_1_centroid_y',
            'cell_2_centroid_x', 'cell_2_centroid_y',
            'junction_angle_mean', 'estimated_division',
        ]
        for col in numeric_cols:
            snapshot[col] = pd.to_numeric(snapshot[col], errors='coerce')
        snapshot = snapshot.dropna(subset=[
            'cell_1_id', 'cell_2_id',
            'cell_1_centroid_x', 'cell_1_centroid_y',
            'cell_2_centroid_x', 'cell_2_centroid_y',
            'junction_angle_mean',
        ]).copy()

        pair_x = (snapshot['cell_1_centroid_x'].to_numpy(float) +
                  snapshot['cell_2_centroid_x'].to_numpy(float)) / 2.0
        pair_y = (snapshot['cell_1_centroid_y'].to_numpy(float) +
                  snapshot['cell_2_centroid_y'].to_numpy(float)) / 2.0

        if x_denom == 0:
            x_norm = np.full_like(pair_x, 0.5, dtype=float)
        else:
            x_norm = (pair_x - x_min) / x_denom
        if y_denom == 0:
            y_norm = np.full_like(pair_y, 0.5, dtype=float)
        else:
            y_norm = (pair_y - y_min) / y_denom

        dx = (snapshot['cell_2_centroid_x'].to_numpy(float) -
              snapshot['cell_1_centroid_x'].to_numpy(float))
        dy = (snapshot['cell_2_centroid_y'].to_numpy(float) -
              snapshot['cell_1_centroid_y'].to_numpy(float))
        angle = _division_orientation_deg(dx, dy)
        pid = f'{genotype}__{Path(str(filename)).stem}'

        out = snapshot.copy()
        out['genotype'] = genotype
        out['primordium_id'] = pid
        out['source_file'] = str(filename)
        out['pair_centroid_x'] = pair_x
        out['pair_centroid_y'] = pair_y
        out['x_norm'] = x_norm
        out['y_norm'] = y_norm
        out['angle'] = angle
        rows.append(out)

    return pd.concat(rows, ignore_index=True)


def infer_matching_at_threshold(snapshot: pd.DataFrame, threshold: float) -> pd.DataFrame:
    """
    Re-run the exact single-threshold division inference for one primordium.

    Candidate rule: junction_angle_mean >= threshold.
    Edge weight: junction_angle_mean - threshold.
    Selection: exact global maximum-weight matching with maxcardinality=False,
    so each cell can be used in at most one inferred daughter pair.
    """
    candidate = snapshot.loc[
        pd.to_numeric(snapshot['junction_angle_mean'], errors='coerce') >= threshold
    ].copy()
    if candidate.empty:
        return candidate

    graph = nx.Graph()
    for row_idx, row in candidate.iterrows():
        graph.add_edge(
            int(row['cell_1_id']),
            int(row['cell_2_id']),
            weight=float(row['junction_angle_mean']) - float(threshold),
            row_idx=int(row_idx),
        )

    matching = nx.max_weight_matching(
        graph,
        maxcardinality=False,
        weight='weight',
    )
    selected_indices = [
        graph.get_edge_data(cell_a, cell_b)['row_idx']
        for cell_a, cell_b in matching
    ]
    return snapshot.loc[selected_indices].copy()


def verify_default_matching(all_pairs: pd.DataFrame) -> None:
    """Confirm that regenerated 145-degree labels reproduce the input CSV exactly."""
    mismatches = []
    for primordium_id, snapshot in all_pairs.groupby('primordium_id', sort=False):
        regenerated = infer_matching_at_threshold(snapshot, DEFAULT_JUNCTION_THRESHOLD)
        regenerated_pairs = {
            tuple(sorted((int(r.cell_1_id), int(r.cell_2_id))))
            for _, r in regenerated.iterrows()
        }
        stored = snapshot.loc[pd.to_numeric(snapshot['estimated_division'], errors='coerce') == 1]
        stored_pairs = {
            tuple(sorted((int(r.cell_1_id), int(r.cell_2_id))))
            for _, r in stored.iterrows()
        }
        if regenerated_pairs != stored_pairs:
            mismatches.append(primordium_id)
    if mismatches:
        raise ValueError(
            'The regenerated 145-degree maximum-weight matching does not match '
            f'the stored estimated_division labels for: {mismatches}'
        )


def exact_permutation_pvalues_matrix(values_matrix: np.ndarray, labels: np.ndarray) -> np.ndarray:
    """
    Two-sided exact permutation P values for many sensitivity combinations at once.

    values_matrix has shape (primordia, parameter combinations), with one
    primordium-level vertical proportion in each entry.
    """
    values_matrix = np.asarray(values_matrix, dtype=float)
    labels = np.asarray(labels)
    if np.isnan(values_matrix).any():
        raise ValueError(
            'At least one threshold/boundary combination has no apical inferred '
            'division for one or more primordia, so the primordium-level comparison '
            'cannot be performed with the full replicate set.'
        )

    n_wt = int(np.sum(labels == GROUPS[0]))
    n_total = len(labels)
    n_mutant = n_total - n_wt
    observed = (
        values_matrix[labels == GROUPS[1]].mean(axis=0) -
        values_matrix[labels == GROUPS[0]].mean(axis=0)
    )
    totals = values_matrix.sum(axis=0)
    extreme_count = np.zeros(values_matrix.shape[1], dtype=np.int64)
    permutation_count = 0

    for membership in get_assignment_batches(n_total, n_wt):
        wt_sums = membership @ values_matrix
        differences = (totals - wt_sums) / n_mutant - wt_sums / n_wt
        extreme_count += np.sum(
            np.abs(differences) >= np.abs(observed)[None, :] - 1e-12,
            axis=0,
        )
        permutation_count += len(membership)

    return extreme_count / permutation_count


def run_parameter_sensitivity(all_pairs: pd.DataFrame) -> tuple[pd.DataFrame, pd.DataFrame]:
    """
    Sweep junction-angle threshold (140-150 degrees, 1-degree steps) and
    normalized apical boundary (y=0.40-0.60, 0.02 steps).

    Returns:
      combination_summary: one row per threshold x boundary combination.
      primordium_summary: one row per primordium per combination.
    """
    verify_default_matching(all_pairs)

    meta = (
        all_pairs[['genotype', 'primordium_id', 'source_file']]
        .drop_duplicates()
        .sort_values(['genotype', 'primordium_id'])
        .reset_index(drop=True)
    )
    primordium_ids = meta['primordium_id'].tolist()
    labels = meta['genotype'].to_numpy()

    combination_rows = []
    primordium_rows = []
    value_columns = []

    for threshold in JUNCTION_THRESHOLDS:
        selected_parts = []
        for primordium_id, snapshot in all_pairs.groupby('primordium_id', sort=False):
            selected = infer_matching_at_threshold(snapshot, float(threshold))
            if selected.empty:
                raise ValueError(
                    f'No inferred divisions for {primordium_id} at threshold {threshold:g}'
                )
            selected_parts.append(selected)
        selected_all = pd.concat(selected_parts, ignore_index=True)

        for boundary in APICAL_BOUNDARIES:
            values = []
            wt_apical_total = 0
            rpl_apical_total = 0
            for _, m in meta.iterrows():
                sub = selected_all.loc[
                    selected_all['primordium_id'] == m['primordium_id']
                ]
                apical = sub.loc[sub['y_norm'] >= float(boundary)]
                total = int(len(apical))
                vertical_n = int(np.count_nonzero(apical['angle'].to_numpy(float) >= VERTICAL_ANGLE_MIN))
                vertical_prop = vertical_n / total if total > 0 else np.nan

                values.append(vertical_prop)
                if m['genotype'] == GROUPS[0]:
                    wt_apical_total += total
                else:
                    rpl_apical_total += total

                primordium_rows.append({
                    'junction_threshold_deg': float(threshold),
                    'apical_boundary_y': float(boundary),
                    'genotype': m['genotype'],
                    'primordium_id': m['primordium_id'],
                    'source_file': m['source_file'],
                    'apical_inferred_divisions': total,
                    'vertical_n': vertical_n,
                    'vertical_prop': vertical_prop,
                })

            values = np.asarray(values, dtype=float)
            value_columns.append(values)
            wt_values = values[labels == GROUPS[0]]
            rpl_values = values[labels == GROUPS[1]]
            difference = float(rpl_values.mean() - wt_values.mean())
            combination_rows.append({
                'junction_threshold_deg': float(threshold),
                'apical_boundary_y': float(boundary),
                'mean_vertical_prop_WT': float(wt_values.mean()),
                'mean_vertical_prop_rpl4d_3': float(rpl_values.mean()),
                'difference_rpl4d_minus_WT': difference,
                'difference_percentage_points': difference * 100.0,
                'apical_inferred_divisions_WT': wt_apical_total,
                'apical_inferred_divisions_rpl4d_3': rpl_apical_total,
                'primordia_WT': int(np.sum(labels == GROUPS[0])),
                'primordia_rpl4d_3': int(np.sum(labels == GROUPS[1])),
            })

    values_matrix = np.column_stack(value_columns)
    p_values = exact_permutation_pvalues_matrix(values_matrix, labels)
    for row, p_value in zip(combination_rows, p_values):
        row['p_exact'] = float(p_value)
        row['significant_p_lt_0_05'] = bool(p_value < 0.05)

    combination_summary = pd.DataFrame(combination_rows)
    primordium_summary = pd.DataFrame(primordium_rows)
    return combination_summary, primordium_summary


def _sensitivity_common_axes(ax, y_max: float) -> None:
    ax.set_ylim(0, y_max)
    ax.set_yticks(np.arange(0, y_max + 0.001, 5))
    ax.axhline(0, color='0.55', linewidth=0.7)
    ax.yaxis.grid(True, linestyle=(0, (1.5, 2.5)), linewidth=0.45, color='0.88')
    ax.set_axisbelow(True)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.tick_params(axis='x', pad=2)
    ax.tick_params(axis='y', pad=2)
    ax.set_ylabel('rpl4d-3 - WT vertical proportion\n(percentage points)', fontsize=8)


def plot_threshold_sensitivity(summary: pd.DataFrame, output_base: Path, y_max: float) -> None:
    sub = summary.loc[np.isclose(summary['apical_boundary_y'], DEFAULT_APICAL_BOUNDARY)].copy()
    sub = sub.sort_values('junction_threshold_deg')
    _set_publication_style()
    fig, ax = plt.subplots(figsize=(SENSITIVITY_FIG_WIDTH, SENSITIVITY_FIG_HEIGHT))
    fig.patch.set_facecolor('white')
    ax.set_facecolor('white')
    ax.set_box_aspect(SENSITIVITY_FIG_HEIGHT / SENSITIVITY_FIG_WIDTH)

    x = sub['junction_threshold_deg'].to_numpy(float)
    y = sub['difference_percentage_points'].to_numpy(float)
    sig = sub['significant_p_lt_0_05'].to_numpy(bool)
    ax.plot(x, y, color=SENSITIVITY_LINE_COLOR, linewidth=1.2, zorder=2)
    ax.scatter(x[sig], y[sig], s=SENSITIVITY_POINT_SIZE,
               color=SENSITIVITY_LINE_COLOR, edgecolor='black', linewidth=0.35, zorder=3)
    if (~sig).any():
        ax.scatter(x[~sig], y[~sig], s=SENSITIVITY_POINT_SIZE,
                   facecolor='white', edgecolor='black', linewidth=0.65, zorder=3)
    ax.axvline(DEFAULT_JUNCTION_THRESHOLD, color='0.45', linestyle='--', linewidth=0.8)
    _sensitivity_common_axes(ax, y_max)
    ax.set_xlim(JUNCTION_THRESHOLDS.min() - 0.4, JUNCTION_THRESHOLDS.max() + 0.4)
    ax.set_xticks(JUNCTION_THRESHOLDS[::2])
    ax.set_xlabel('Junction-angle threshold (°)', fontsize=8)
    ax.set_title(f'Apical boundary y = {DEFAULT_APICAL_BOUNDARY:.2f}', fontsize=9, fontweight='bold', pad=5)
    fig.subplots_adjust(left=0.24, right=0.97, top=0.88, bottom=0.18)
    fig.savefig(output_base.with_suffix('.png'), dpi=DPI, bbox_inches='tight', facecolor='white')
    plt.close(fig)


def plot_boundary_sensitivity(summary: pd.DataFrame, output_base: Path, y_max: float) -> None:
    sub = summary.loc[np.isclose(summary['junction_threshold_deg'], DEFAULT_JUNCTION_THRESHOLD)].copy()
    sub = sub.sort_values('apical_boundary_y')
    _set_publication_style()
    fig, ax = plt.subplots(figsize=(SENSITIVITY_FIG_WIDTH, SENSITIVITY_FIG_HEIGHT))
    fig.patch.set_facecolor('white')
    ax.set_facecolor('white')
    ax.set_box_aspect(SENSITIVITY_FIG_HEIGHT / SENSITIVITY_FIG_WIDTH)

    x = sub['apical_boundary_y'].to_numpy(float)
    y = sub['difference_percentage_points'].to_numpy(float)
    sig = sub['significant_p_lt_0_05'].to_numpy(bool)
    ax.plot(x, y, color=SENSITIVITY_LINE_COLOR, linewidth=1.2, zorder=2)
    ax.scatter(x[sig], y[sig], s=SENSITIVITY_POINT_SIZE,
               color=SENSITIVITY_LINE_COLOR, edgecolor='black', linewidth=0.35, zorder=3)
    if (~sig).any():
        ax.scatter(x[~sig], y[~sig], s=SENSITIVITY_POINT_SIZE,
                   facecolor='white', edgecolor='black', linewidth=0.65, zorder=3)
    ax.axvline(DEFAULT_APICAL_BOUNDARY, color='0.45', linestyle='--', linewidth=0.8)
    _sensitivity_common_axes(ax, y_max)
    ax.set_xlim(APICAL_BOUNDARIES.min() - 0.008, APICAL_BOUNDARIES.max() + 0.008)
    ax.set_xticks(APICAL_BOUNDARIES[::2])
    ax.set_xlabel('Normalized apical boundary (y)', fontsize=8)
    ax.set_title(f'Junction threshold = {DEFAULT_JUNCTION_THRESHOLD:.0f}°', fontsize=9, fontweight='bold', pad=5)
    fig.subplots_adjust(left=0.24, right=0.97, top=0.88, bottom=0.18)
    fig.savefig(output_base.with_suffix('.png'), dpi=DPI, bbox_inches='tight', facecolor='white')
    plt.close(fig)


def plot_sensitivity_heatmap(summary: pd.DataFrame, output_base: Path) -> None:
    pivot_diff = summary.pivot(
        index='apical_boundary_y',
        columns='junction_threshold_deg',
        values='difference_percentage_points',
    ).reindex(index=APICAL_BOUNDARIES, columns=JUNCTION_THRESHOLDS)
    pivot_sig = summary.pivot(
        index='apical_boundary_y',
        columns='junction_threshold_deg',
        values='significant_p_lt_0_05',
    ).reindex(index=APICAL_BOUNDARIES, columns=JUNCTION_THRESHOLDS)

    _set_publication_style()
    fig, ax = plt.subplots(figsize=(4.25, 3.55))
    fig.patch.set_facecolor('white')
    ax.set_facecolor('white')
    image = ax.imshow(
        pivot_diff.to_numpy(float),
        origin='lower',
        aspect='auto',
        interpolation='nearest',
        cmap='Greens',
    )

    ax.set_xticks(np.arange(len(JUNCTION_THRESHOLDS)))
    ax.set_xticklabels([f'{x:.0f}' for x in JUNCTION_THRESHOLDS], fontsize=6)
    ax.set_yticks(np.arange(len(APICAL_BOUNDARIES)))
    ax.set_yticklabels([f'{y:.2f}' for y in APICAL_BOUNDARIES], fontsize=6)
    ax.set_xlabel('Junction-angle threshold (°)', fontsize=8)
    ax.set_ylabel('Normalized apical boundary (y)', fontsize=8)
    ax.set_title('Parameter-combination sensitivity', fontsize=9, fontweight='bold', pad=5)

    # Mark only non-significant combinations to keep a mostly significant matrix uncluttered.
    sig_array = pivot_sig.to_numpy(bool)
    for i in range(sig_array.shape[0]):
        for j in range(sig_array.shape[1]):
            if not sig_array[i, j]:
                ax.text(j, i, '×', ha='center', va='center', fontsize=7, color='black')

    default_x = int(np.where(np.isclose(JUNCTION_THRESHOLDS, DEFAULT_JUNCTION_THRESHOLD))[0][0])
    default_y = int(np.where(np.isclose(APICAL_BOUNDARIES, DEFAULT_APICAL_BOUNDARY))[0][0])
    ax.add_patch(Rectangle(
        (default_x - 0.5, default_y - 0.5), 1, 1,
        fill=False, edgecolor='black', linewidth=1.2,
    ))

    cbar = fig.colorbar(image, ax=ax, fraction=0.046, pad=0.04)
    cbar.set_label('rpl4d-3 - WT\n(percentage points)', fontsize=7)
    cbar.ax.tick_params(labelsize=6, width=0.6, length=2)
    fig.text(0.5, 0.025, 'Black box: default 145°, y = 0.50; ×: P ≥ 0.05',
             ha='center', va='bottom', fontsize=6.5)
    fig.subplots_adjust(left=0.17, right=0.88, top=0.90, bottom=0.18)
    fig.savefig(output_base.with_suffix('.png'), dpi=DPI, bbox_inches='tight', facecolor='white')
    plt.close(fig)


def print_sensitivity_summary(summary: pd.DataFrame) -> None:
    threshold_only = summary.loc[np.isclose(summary['apical_boundary_y'], DEFAULT_APICAL_BOUNDARY)]
    boundary_only = summary.loc[np.isclose(summary['junction_threshold_deg'], DEFAULT_JUNCTION_THRESHOLD)]
    significant = int(summary['significant_p_lt_0_05'].sum())
    positive = int((summary['difference_percentage_points'] > 0).sum())
    total = len(summary)

    print('\nParameter sensitivity summary')
    print('-----------------------------')
    print(
        f'All combinations: positive mutant-WT difference in {positive}/{total}; '
        f'P < 0.05 in {significant}/{total}; difference range = '
        f"{summary['difference_percentage_points'].min():.1f} to "
        f"{summary['difference_percentage_points'].max():.1f} percentage points."
    )
    print(
        f'Threshold 140-150° at y=0.50: difference range = '
        f"{threshold_only['difference_percentage_points'].min():.1f} to "
        f"{threshold_only['difference_percentage_points'].max():.1f} percentage points; "
        f"P < 0.05 in {int(threshold_only['significant_p_lt_0_05'].sum())}/{len(threshold_only)}."
    )
    print(
        f'Boundary y=0.40-0.60 at 145°: difference range = '
        f"{boundary_only['difference_percentage_points'].min():.1f} to "
        f"{boundary_only['difference_percentage_points'].max():.1f} percentage points; "
        f"P < 0.05 in {int(boundary_only['significant_p_lt_0_05'].sum())}/{len(boundary_only)}."
    )


def validate_design(events: pd.DataFrame) -> pd.DataFrame:
    meta = (
        events[['genotype', 'primordium_id', 'source_file']]
        .drop_duplicates()
        .sort_values(['genotype', 'primordium_id'])
        .reset_index(drop=True)
    )
    return meta


def select_region(events: pd.DataFrame, region: str) -> pd.DataFrame:
    if region == 'whole':
        return events.copy()
    return events.loc[events['region'] == region].copy()


def ensure_region_has_events_per_primordium(regional_events: pd.DataFrame, meta: pd.DataFrame, region: str) -> None:
    observed_ids = set(regional_events['primordium_id'].unique())
    missing_ids = [pid for pid in meta['primordium_id'] if pid not in observed_ids]
    if missing_ids:
        raise ValueError(f'Region {region} has no inferred events for: {missing_ids}')


def plot_pooled_angle_histogram(regional_events: pd.DataFrame, genotype: str, region: str, output_base: Path) -> None:
    """
    Plot pooled inferred division-angle histogram for one genotype and one region.

    The main plotting area keeps the original FIG_WIDTH:FIG_HEIGHT aspect ratio.
    The descriptive note is placed below the axes on extra canvas, so it is not
    counted as part of the main figure aspect ratio.
    """
    sub = regional_events.loc[regional_events['genotype'] == genotype]
    angles = sub['angle'].to_numpy(float)

    if angles.size == 0:
        raise ValueError(f'No data for {genotype}, {region}')

    counts, edges = np.histogram(angles, bins=BINS)
    freq_pct = counts / counts.sum() * 100.0

    bin_colors = []
    for left in edges[:-1]:
        if left < 30:
            bin_colors.append('#bfe9ff')
        elif left < 60:
            bin_colors.append('#9fe7dd')
        else:
            bin_colors.append('#ffb6b6')

    # Main axes aspect ratio is preserved by set_box_aspect().
    # The figure is taller only because of HIST_NOTE_HEIGHT for the note below.
    fig, ax = plt.subplots(figsize=(FIG_WIDTH, FIG_HEIGHT + HIST_NOTE_HEIGHT))
    ax.set_box_aspect(FIG_HEIGHT / FIG_WIDTH)

    lefts = edges[:-1]
    ax.bar(
        lefts + 0.5,
        freq_pct,
        width=9.0,
        align='edge',
        edgecolor='black',
        linewidth=1.0,
        color=bin_colors,
        alpha=0.8,
    )

    n_primordia = int(sub['primordium_id'].nunique())

    ax.set_xlim(0, 90)
    ax.set_ylim(0, HIST_YMAX)
    ax.set_xticks([0, 30, 60, 90])
    ax.set_yticks(np.arange(0, HIST_YMAX + 1, 10))
    ax.set_xlabel('Division angle (°)')
    ax.set_ylabel('Frequency (%)')
    ax.set_title(f'{genotype} ({region})', fontweight='bold')

    ax.minorticks_on()
    ax.grid(True, which='major', axis='both', linestyle='--', linewidth=0.8, alpha=0.35)
    ax.grid(True, which='minor', axis='both', linestyle='--', linewidth=0.5, alpha=0.20)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)

    # Leave bottom space for the note, then place note on the figure canvas.
    fig.subplots_adjust(left=0.13, right=0.98, top=0.90, bottom=0.18)
    fig.text(
        0.5,
        0.045,
        f'Pooled events for visualization only; {angles.size} inferred divisions; {n_primordia} primordia',
        ha='center',
        va='bottom',
        fontsize=9,
    )

    fig.savefig(output_base.with_suffix('.png'), dpi=DPI, bbox_inches='tight', facecolor='white')

    plt.close(fig)

def orientation_category(angle: pd.Series) -> pd.Categorical:
    values = np.select(
        [angle < 30, angle < 60, angle <= 90],
        ['horizontal', 'diagonal', 'vertical'],
        default=None,
    )
    return pd.Categorical(values, categories=['horizontal', 'diagonal', 'vertical'], ordered=True)


def summarize_orientation_proportions(events: pd.DataFrame, meta: pd.DataFrame, regions: tuple[str, ...]) -> pd.DataFrame:
    rows = []
    for region in regions:
        regional_events = select_region(events, region)
        ensure_region_has_events_per_primordium(regional_events, meta, region)
        regional_events = regional_events.copy()
        regional_events['orientation'] = orientation_category(regional_events['angle'])
        for _, m in meta.iterrows():
            sub = regional_events.loc[regional_events['primordium_id'] == m['primordium_id']]
            counts = sub['orientation'].value_counts().reindex(['horizontal', 'diagonal', 'vertical'], fill_value=0)
            total = int(counts.sum())
            rows.append({
                'region': region,
                'genotype': m['genotype'],
                'primordium_id': m['primordium_id'],
                'source_file': m['source_file'],
                'total_inferred_divisions': total,
                'horizontal_n': int(counts['horizontal']),
                'diagonal_n': int(counts['diagonal']),
                'vertical_n': int(counts['vertical']),
                'horizontal_prop': counts['horizontal'] / total,
                'diagonal_prop': counts['diagonal'] / total,
                'vertical_prop': counts['vertical'] / total,
                'median_angle': float(sub['angle'].median()),
            })
    return pd.DataFrame(rows)


def get_assignment_batches(n_total: int, n_wt: int) -> list[np.ndarray]:
    key = (n_total, n_wt, PERMUTATION_BATCH_SIZE)
    if key in _ASSIGNMENT_BATCH_CACHE:
        return _ASSIGNMENT_BATCH_CACHE[key]
    batches = []
    rows = []
    for wt_indices in combinations(range(n_total), n_wt):
        row = np.zeros(n_total, dtype=float)
        row[list(wt_indices)] = 1.0
        rows.append(row)
        if len(rows) == PERMUTATION_BATCH_SIZE:
            batches.append(np.asarray(rows, dtype=float))
            rows = []
    if rows:
        batches.append(np.asarray(rows, dtype=float))
    _ASSIGNMENT_BATCH_CACHE[key] = batches
    return batches


def exact_sample_summary_permutation_test(summary: pd.DataFrame, region: str, value_col: str) -> dict:
    sub = summary.loc[summary['region'] == region].copy()
    values = sub[value_col].to_numpy(dtype=float)
    labels = sub['genotype'].to_numpy()
    n_wt = int(np.sum(labels == GROUPS[0]))
    n_total = len(sub)
    n_mutant = n_total - n_wt
    observed_signed = float(values[labels == GROUPS[1]].mean() - values[labels == GROUPS[0]].mean())
    total_value = float(values.sum())
    extreme_count = 0
    permutation_count = 0
    for membership in get_assignment_batches(n_total, n_wt):
        wt_sums = membership @ values
        differences = (total_value - wt_sums) / n_mutant - wt_sums / n_wt
        extreme_count += int(np.count_nonzero(np.abs(differences) >= abs(observed_signed) - 1e-12))
        permutation_count += int(len(differences))
    return {
        'region': region,
        'analysis': f'primordium-level {value_col}',
        'difference_rpl4d_minus_WT': observed_signed,
        'p_exact': float(extreme_count / permutation_count),
        'permutations': permutation_count,
        f'mean_{value_col}_{GROUPS[0]}': float(values[labels == GROUPS[0]].mean()),
        f'mean_{value_col}_{GROUPS[1]}': float(values[labels == GROUPS[1]].mean()),
        f'primordia_{GROUPS[0]}': n_wt,
        f'primordia_{GROUPS[1]}': n_mutant,
    }


def _set_publication_style() -> None:
    plt.rcdefaults()
    plt.rcParams.update(
        {
            'font.family': 'DejaVu Sans',
            'font.size': 7,
            'axes.linewidth': 0.8,
            'xtick.major.width': 0.8,
            'ytick.major.width': 0.8,
            'xtick.major.size': 2.5,
            'ytick.major.size': 2.5,
        }
    )


def plot_vertical_proportion_by_primordium(orientation_summary: pd.DataFrame, vertical_test: dict, region: str, output_base: Path) -> None:
    """
    Plot vertical-division proportion per primordium as boxplot + points.

    The main plotting area keeps the original VERTICAL_PROP_FIG_WIDTH:VERTICAL_PROP_FIG_HEIGHT
    aspect ratio. The exact-permutation P value is placed below the axes on extra canvas,
    so it is not counted as part of the main figure aspect ratio.
    """
    sub = orientation_summary.loc[orientation_summary['region'] == region].copy()
    _set_publication_style()

    fig, ax = plt.subplots(
        figsize=(VERTICAL_PROP_FIG_WIDTH, VERTICAL_PROP_FIG_HEIGHT + VERTICAL_PROP_NOTE_HEIGHT)
    )
    fig.patch.set_facecolor('white')
    ax.set_facecolor('white')
    ax.set_box_aspect(VERTICAL_PROP_FIG_HEIGHT / VERTICAL_PROP_FIG_WIDTH)

    group_values = [
        sub.loc[sub['genotype'] == GROUPS[0], 'vertical_prop'].to_numpy(dtype=float),
        sub.loc[sub['genotype'] == GROUPS[1], 'vertical_prop'].to_numpy(dtype=float),
    ]

    bp = ax.boxplot(
        group_values,
        positions=[1, 2],
        widths=0.45,
        patch_artist=True,
        showfliers=False,
        whis=(5, 95),
        medianprops={'color': 'black', 'linewidth': 1.2},
        boxprops={'linewidth': 0.85, 'color': 'black'},
        whiskerprops={'linewidth': 0.85, 'color': 'black'},
        capprops={'linewidth': 0.85, 'color': 'black'},
    )

    box_colors = [VERTICAL_PROP_COLOR_WT, VERTICAL_PROP_COLOR_RPL]
    for patch, color in zip(bp['boxes'], box_colors):
        patch.set_facecolor(color)
        patch.set_alpha(VERTICAL_PROP_BOX_ALPHA)

    rng = np.random.default_rng(VERTICAL_PROP_JITTER_SEED)
    for i, (values, color) in enumerate(zip(group_values, box_colors), start=1):
        jitter = rng.uniform(-VERTICAL_PROP_JITTER_WIDTH, VERTICAL_PROP_JITTER_WIDTH, size=len(values))
        ax.scatter(
            np.full(len(values), i) + jitter,
            values,
            s=VERTICAL_PROP_POINT_SIZE,
            color=color,
            edgecolor='black',
            linewidth=VERTICAL_PROP_POINT_EDGE_WIDTH,
            alpha=0.96,
            zorder=3,
        )

    ax.set_xlim(0.5, 2.5)
    ax.set_ylim(VERTICAL_PROP_YLIM_MIN, VERTICAL_PROP_YLIM_MAX)
    ax.set_yticks(np.arange(VERTICAL_PROP_YLIM_MIN, VERTICAL_PROP_YLIM_MAX + 1e-9, VERTICAL_PROP_YTICK_STEP))
    ax.set_xticks(
        [1, 2],
        labels=[
            f"{GROUPS[0]}\n(n={int((sub['genotype'] == GROUPS[0]).sum())})",
            f"{GROUPS[1]}\n(n={int((sub['genotype'] == GROUPS[1]).sum())})",
        ],
    )
    ax.set_ylabel('Proportion of vertical\ninferred divisions', fontsize=8)
    ax.set_title(f'{region.capitalize()} region', fontsize=10, fontweight='bold', pad=6)

    ax.yaxis.grid(True, linestyle=(0, (1.5, 2.5)), linewidth=0.45, color='0.88')
    ax.set_axisbelow(True)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.tick_params(axis='x', length=0, pad=3)
    ax.tick_params(axis='y', pad=2)

    # Leave bottom space for the P value, then place it on the figure canvas.
    fig.subplots_adjust(left=0.28, right=0.96, top=0.88, bottom=0.24)
    fig.text(
        0.5,
        0.055,
        f"Exact permutation P = {vertical_test['p_exact']:.4f}",
        ha='center',
        va='bottom',
        fontsize=7,
    )

    fig.savefig(output_base.with_suffix('.png'), dpi=DPI, bbox_inches='tight', facecolor='white')

    plt.close(fig)

def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    # Remove stale non-PNG figure files left by older versions of this script.
    for pattern in ('*.pdf', '*.svg', '*.html', '*.htm'):
        for old_file in OUTPUT_DIR.glob(pattern):
            old_file.unlink()
    wt_csv = resolve_input_csv(WT_BATCH_CSV)
    rpl_csv = resolve_input_csv(RPL_BATCH_CSV)

    events = pd.concat([
        collect_division_events(wt_csv, GROUPS[0]),
        collect_division_events(rpl_csv, GROUPS[1]),
    ], ignore_index=True)
    meta = validate_design(events)
    events.to_csv(OUTPUT_DIR / 'division_events_with_primordium_identity.csv', index=False)
    meta.to_csv(OUTPUT_DIR / 'included_primordia.csv', index=False)
    for region in REGIONS:
        regional_events = select_region(events, region)
        for genotype in GROUPS:
            safe_group = genotype.replace('-', '_')
            plot_pooled_angle_histogram(
                regional_events,
                genotype,
                region,
                OUTPUT_DIR / f'pooled_hist_{safe_group}_{region}',
            )
    orientation_summary = summarize_orientation_proportions(events, meta, REGIONS)
    orientation_summary.to_csv(OUTPUT_DIR / 'primordium_orientation_proportions_all_regions.csv', index=False)
    vertical_results = pd.DataFrame([
        exact_sample_summary_permutation_test(orientation_summary, region, 'vertical_prop')
        for region in REGIONS
    ])
    vertical_results.to_csv(OUTPUT_DIR / 'vertical_proportion_exact_permutation_results_all_regions.csv', index=False)
    for region in REGIONS:
        row = vertical_results.loc[vertical_results['region'] == region].iloc[0].to_dict()
        plot_vertical_proportion_by_primordium(
            orientation_summary,
            row,
            region,
            OUTPUT_DIR / f'replicate_points_vertical_proportion_{region}',
        )

    # Sensitivity analysis for junction-angle threshold and apical boundary.
    all_pairs = pd.concat([
        collect_all_neighbor_pairs(wt_csv, GROUPS[0]),
        collect_all_neighbor_pairs(rpl_csv, GROUPS[1]),
    ], ignore_index=True)
    sensitivity, sensitivity_by_primordium = run_parameter_sensitivity(all_pairs)
    sensitivity.to_csv(OUTPUT_DIR / 'parameter_sensitivity_threshold_x_apical_boundary.csv', index=False)
    sensitivity_by_primordium.to_csv(OUTPUT_DIR / 'parameter_sensitivity_by_primordium.csv', index=False)

    threshold_only = sensitivity.loc[
        np.isclose(sensitivity['apical_boundary_y'], DEFAULT_APICAL_BOUNDARY)
    ].copy()
    boundary_only = sensitivity.loc[
        np.isclose(sensitivity['junction_threshold_deg'], DEFAULT_JUNCTION_THRESHOLD)
    ].copy()
    threshold_only.to_csv(OUTPUT_DIR / 'sensitivity_junction_threshold_at_y_0_50.csv', index=False)
    boundary_only.to_csv(OUTPUT_DIR / 'sensitivity_apical_boundary_at_145deg.csv', index=False)

    max_line_difference = max(
        float(threshold_only['difference_percentage_points'].max()),
        float(boundary_only['difference_percentage_points'].max()),
    )
    sensitivity_ymax = max(5.0, float(np.ceil((max_line_difference + 0.5) / 5.0) * 5.0))
    plot_threshold_sensitivity(
        sensitivity,
        OUTPUT_DIR / 'FigS10G_sensitivity_junction_angle_threshold',
        sensitivity_ymax,
    )
    plot_boundary_sensitivity(
        sensitivity,
        OUTPUT_DIR / 'FigS10H_sensitivity_apical_boundary',
        sensitivity_ymax,
    )
    plot_sensitivity_heatmap(
        sensitivity,
        OUTPUT_DIR / 'FigS10I_sensitivity_threshold_x_boundary_heatmap',
    )

    print('Saved outputs to', OUTPUT_DIR)
    print(vertical_results.to_string(index=False))
    print_sensitivity_summary(sensitivity)


if __name__ == '__main__':
    main()
