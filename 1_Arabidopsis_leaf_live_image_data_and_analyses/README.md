# Supplementary Data Set 1: Arabidopsis leaf live-imaging data and analyses

## Overview

This directory contains the *Arabidopsis thaliana* leaf live-imaging data and analyses used to develop and evaluate the shape-based inference of recent daughter-cell pairs described in **“Inferring recent cell division from cell shapes.”** It includes four representative image snapshots and polygonal cell networks and ground-truth annotations for 30 analyzed snapshots, analysis-ready geometric measurements, Python analysis scripts, and precomputed results.

The source live-imaging dataset was obtained from Zhao et al. (2026). First foliage leaf primordia were imaged at 2-h intervals. The present study analyzed 30 snapshots from 12 tracked primordia.

### Dataset summary

- Raw neighboring-pair records: **7,624**
- Records excluded using `exception_label = 1`: **51**
- Analyzed neighboring pairs: **7,573**
- Ground-truth daughter pairs: **1,114**
- Ground-truth non-daughter pairs: **6,459**
- Geometric features per neighboring pair: **49**
- Tracked primordia: **12**
- Analyzed snapshots: **30**

A daughter pair is defined as two neighboring cells generated directly by the latest observed division of a common mother cell within the preceding **36 h**, with neither daughter undergoing another division before the analyzed snapshot. All other resolved neighboring pairs, including older lineage-related pairs, are labeled as non-daughter pairs. The variable `division_timing` is expressed in hours.

## Directory contents

| Directory | Contents and purpose |
| --- | --- |
| [`01_Raw_Images_to_Polygonal_Networks_and_Ground_Truth/`](01_Raw_Images_to_Polygonal_Networks_and_Ground_Truth/) | Four representative snapshots from sample 2 at 12, 26, 40, and 54 h. Each folder contains a projected background image, manually traced cell-wall outline, ground-truth daughter-pair file, reconstructed polygonal network, neighboring-pair geometry, inferred/observed division maps, and performance outputs. |
| [`02_Polygonal_Networks_to_Neighbor_Pair_Geometry/`](02_Polygonal_Networks_to_Neighbor_Pair_Geometry/) | Polygonal-network JSON files and ground-truth daughter-pair annotations for all 30 snapshots, together with the complete `batch_neighbor_pair_geometry.csv`. |
| [`03_single_feature_analysis/`](03_single_feature_analysis/) | Fixed-threshold daughter-pair inference for mean junction angle and selected symmetry-, union-, and contact-based features. Includes pair classifications and performance summaries. |
| [`04_threshold_analysis/`](04_threshold_analysis/) | Junction-angle threshold sweep and the resulting precision, recall, and F1-score profile. |
| [`05_mixture_model/`](05_mixture_model/) | Daughter/non-daughter feature-distribution summaries. For junction-angle features, the script additionally fits a two-component axial von Mises mixture and calculates component-to-label 1-Wasserstein distances. |
| [`06_individual_snapshot_primordia/`](06_individual_snapshot_primordia/) | Mean-junction-angle performance summarized separately by snapshot and by tracked primordium. |
| [`07_heldout_group_validation_primordia/`](07_heldout_group_validation_primordia/) | Generation of accepted primordium-grouped train/test partitions and evaluation of thresholds selected from training primordia on held-out primordia. |
| [`08_random_estimator/`](08_random_estimator/) | Random-score negative-control analysis using the same thresholding and matching procedure, repeated across 100 randomizations. |
| [`09_overlap_index/`](09_overlap_index/) | Daughter/non-daughter distributional overlap index for all 49 geometric features, using 40 normalized histogram bins. |
| [`10_best_F1/`](10_best_F1/) | Threshold sweeps in both directions for all features, selection of the best post-matching F1 score per feature, and summary plots. |
| [`11_single_cell_shape/`](11_single_cell_shape/) | Single-cell geometry and the mean and sample SD of cell circularity. |
| [`12_machine_learning/`](12_machine_learning/) | Primordium-grouped evaluation of logistic regression, linear SVM, XGBoost, and LightGBM using either all 49 features or a selected four-feature panel. Precomputed cross-split summaries are in `result_summary/`. |
| [`13_dynamics_tracking/`](13_dynamics_tracking/) | Tracked changes in junction angle before and after successive divisions, including plotted values, mean ± SD summaries, and model summaries. |

## Image and network data

### Representative image snapshots

The four `sample2_*` folders in `01_Raw_Images_to_Polygonal_Networks_and_Ground_Truth/` illustrate the workflow from image to division inference:

- `*_background.jpg`: 2D maximum-intensity projection used as the background image;
- `*_outline.jpg`: manually traced cell-wall outline;
- `*_real_division_pairs.csv` or `.txt`: lineage-resolved daughter-cell pairs and elapsed time since division;
- `*_analysis_results/*.json`: reconstructed polygonal cell network;
- `*_analysis_results/*_neighbor_geometry.csv`: neighboring-pair geometric measurements;
- `*_analysis_results/*_estimated_division_pairs.csv`: inferred daughter pairs;
- `*_analysis_results/*_real_division_pairs.csv`: observed daughter pairs; and
- `*_analysis_results/*.png`: geometry, inferred-division, observed-division, and comparison overlays.

The representative `background.jpg` images are projected snapshots rather than the complete original 3D time-lapse z-stacks. The acquired voxel sampling was 0.33 × 0.33 µm in xy with a 1-µm z-step.

The ground-truth pair lists beside the representative images use space-separated cell IDs and division timing despite the `.csv` extension used for some files. The comma-delimited pair tables inside `*_analysis_results/` use conventional CSV formatting.

### Polygonal-network JSON files

Each JSON file in `02_Polygonal_Networks_to_Neighbor_Pair_Geometry/polygonal_networks/` represents one snapshot and contains four top-level arrays:

- `vertices`: vertex IDs and image-coordinate x/y positions;
- `lines`: cell-wall segments connecting vertices;
- `polygons`: cell IDs and their ordered lines/vertices; and
- `neighborPairs`: neighboring polygon pairs sharing a cell wall.

Coordinates are stored in image-pixel units. Accordingly, unnormalized lengths and areas in the geometry tables are expressed in pixel-based coordinate units, whereas ratios and normalized features are dimensionless and angles are in degrees.

## Analysis-ready neighboring-pair geometry

`02_Polygonal_Networks_to_Neighbor_Pair_Geometry/batch_neighbor_pair_geometry.csv` is the principal input for the downstream analyses. Identical convenience copies are placed beside the scripts in the analysis directories.

### Identification and annotation columns

| Column | Meaning |
| --- | --- |
| `fileName` | Polygonal-network filename identifying one snapshot. The prefix before `_<time>h` identifies the tracked primordium. |
| `pairIndex` | Neighboring-pair index within the snapshot. |
| `firstPolygonId`, `secondPolygonId` | IDs of the two neighboring cells. |
| `observed_division` | Ground-truth class: `1` for a daughter pair and `0` for a non-daughter pair. |
| `division_timing` | Hours elapsed since division for daughter pairs; `-1` for non-daughter pairs. |
| `exception_label` | `1` marks an unresolved edge-region pair excluded before all analyses; `0` marks an analyzable pair. |

The remaining 49 columns are geometric features in three manuscript-defined categories:

1. **Pairwise features** — area, perimeter, aspect ratio, circularity, solidity, and vertex-count summaries for the two cells, together with centroid distance and normalized centroid distance.
2. **Union features** — aspect ratio, circularity, and convex deficiency of the merged two-cell region.
3. **Contact features** — shared-cell-wall length, distances and orientation relative to the cells or their union, and mean/minimum/maximum/difference/ratio summaries of the two junction angles.

CSV column names use implementation-oriented forms such as `junctionAngleAverageDegrees`, `normalizedSharedEdgeLength`, `unionConvexDeficiency`, and `unionCircularity`. These correspond to the manuscript terms **mean junction angle**, **normalized shared cell wall length**, **union convex deficiency**, and **union circularity**, respectively.

`real_division_pairs/exception.csv` is retained as a legacy record. Current scripts use the integrated `exception_label` column in `batch_neighbor_pair_geometry.csv`; the separate exception file is not required.

## Shared analysis conventions

The following conventions are used throughout the threshold-based and multivariate analyses:

1. Rows with `exception_label = 1` are removed before thresholding, model fitting, matching, or performance calculation.
2. A feature threshold or model-score threshold first defines candidate daughter-pair edges.
3. Candidate edges are filtered by **exact maximum-weight matching within each snapshot**, enforcing the biological constraint that one cell can belong to at most one direct daughter pair.
4. Inferred pairs are compared with `observed_division` to calculate TP, FP, FN, TN, precision, recall, and F1 score.

The supervised F1-optimized threshold and the unsupervised mixture-derived cutoff are distinct. The supervised threshold is selected against ground-truth labels after matching. The mixture-derived cutoff is obtained without labels from the intersection of the two weighted fitted component densities.

## Requirements

Python **3.10 or later** is recommended. A single environment covering all included Python analyses can be installed with:

```bash
python -m pip install numpy pandas scipy matplotlib networkx "scikit-learn>=1.1" xgboost lightgbm joblib tqdm
```

Reconstruction of polygonal networks and regeneration of geometry tables from cell-wall outlines require the Cell Division Inference software provided in [`../5_Software_Qt_FIJI/`](../5_Software_Qt_FIJI/). The Python scripts in this directory start from the supplied CSV or JSON files and do not repeat the upstream image-tracing step.

## Running the analyses

Run each script from its own directory unless an explicit path is supplied. Precomputed outputs are already included; rerunning a script may replace files if the same output directory is used.

### Single-feature threshold analysis

```bash
cd 03_single_feature_analysis
python estimate_by_single_threshold.py
```

The editable settings at the beginning of the script define the input CSV, feature, threshold, direction, and output directory. The supplied default is `junctionAngleAverageDegrees >= 145.0`, followed by maximum-weight matching. The named `result_single_threshold_*` directories contain archived results for the corresponding features and thresholds.

### Junction-angle threshold sweep

```bash
cd 04_threshold_analysis
python estimate_by_ranged_threshold.py
```

The default script scans mean junction-angle thresholds from 120° to 170° in 1° steps. Threshold range, step, operator, and output directory are editable in the settings block.

### Feature distributions and mixture modeling

For mean junction angle:

```bash
cd 05_mixture_model
python bimodality_cutoff_analysis.py batch_neighbor_pair_geometry.csv \
  --feature junctionAngleAverageDegrees \
  --result-dir result_junction_angles
```

For non-junction-angle features, the script reports daughter- and non-daughter-pair mean ± SD. For junction-angle features, it additionally reports the fitted low- and high-angle component means and concentrations and their Wasserstein distances from the observed label distributions. The fit uses seed `0`.

### Snapshot- and primordium-level performance

```bash
cd 06_individual_snapshot_primordia
python plot_performance_for_individual_grouped.py \
  --analysis-script estimate_by_single_threshold.py
```

The script evaluates mean junction angle ≥145° and exports individual-snapshot metrics, pooled primordium-level metrics, plotting values, summary statistics, and the two-panel figure.

### Held-out primordium validation

```bash
cd 07_heldout_group_validation_primordia
python find_all_satisfying_primordia_splits.py
python evaluate_train_threshold_on_test.py
```

The first script enumerates primordium-grouped partitions and retains those with 70 ± 2% of analyzable rows in training and a daughter-pair-frequency difference of less than one percentage point between training and test data. The supplied table contains 75 accepted partitions. The second script selects the best mean-junction-angle threshold from the training primordia only and applies it unchanged to held-out primordia.

### Random-estimator control

```bash
cd 08_random_estimator
python random_estimator.py
```

This analysis assigns uniform random scores, uses the same threshold-plus-matching procedure, and summarizes the best metrics across 100 seeds. Seed `0` is retained as the representative run. The archived tables are in `output/`; the current script’s `OUTPUT_DIR` setting can be edited to reproduce that folder name.

### Overlap index

```bash
cd 09_overlap_index
python calculate_and_plot_overlap_index.py
```

The default analysis uses 40 normalized histogram bins and exports the complete feature table plus top-10 and all-feature figures to `overlap_index_results/`.

### Best F1 score across all features

```bash
cd 10_best_F1
python calculate_best_f1.py
python plot_best_f1.py
```

`calculate_best_f1.py` uses the feature ranges in `feature_ranges.csv`, evaluates both `>=` and `<=` rules, applies maximum-weight matching, and saves each threshold sweep and its best result. `plot_best_f1.py` reads these tables without recalculating the analysis.

### Single-cell circularity

```bash
cd 11_single_cell_shape
python calculate_circularity_mean_sd.py
```

This script calculates the mean and sample SD of `circularity` from `batch_single_cell_geometry.csv`.

### Multivariate machine learning

This analysis compares logistic regression, linear SVM, XGBoost, and LightGBM using either all 49 geometric features or the following four-feature panel:

* `junctionAngleAverageDegrees`
* `normalizedSharedEdgeLength`
* `unionConvexDeficiency`
* `unionCircularity`

The models were evaluated across 75 predefined outer train–test partitions generated from 12 tracked primordia. All snapshots from the same primordium were retained within the same partition. The accepted partitions contained 70% ± 2% of the analyzable neighboring pairs in training and differed by less than one percentage point in daughter-pair frequency between training and test sets.

Within each outer training partition, model selection used three-fold `StratifiedGroupKFold` cross-validation with shuffling and random seed `42`, with tracked primordium as the grouping variable. Logistic regression and linear SVM tested `C = 0.1, 1, 10`; XGBoost tested maximum depths of `2` and `4` and minimum child weights of `1` and `5`; and LightGBM tested `15`, `31`, and `63` leaves and minimum child-sample values of `20` and `50`.

For each hyperparameter candidate, 201 equally spaced decision thresholds from 0 to 1 were evaluated using out-of-fold training predictions. At each threshold, exact maximum-weight matching was applied separately within each snapshot. The hyperparameter configuration and threshold maximizing post-matching F1 were selected exclusively from the outer training data. The selected model was then refitted on the complete outer training partition and applied, with the selected threshold unchanged, to the held-out test primordia.

The script expects the following input files in this directory:

* `batch_neighbor_pair_geometry.csv`
* `all_satisfying_primordia_splits.csv`

Run the complete analysis from the repository root:

```bash
cd 12_machine_learning
python train_all_models_all_accepted_splits.py
```

The output directory contains per-split predictions and performance summaries, aggregate results across the 75 partitions, and an `analysis_configuration.json` file recording the exact runtime settings, parameter grids, software versions, random seed, and input-file and script hashes.

### Junction-angle dynamics

```bash
cd 13_dynamics_tracking
python temporal_analysis.py
```

The script reads `temporal_change_tracks.csv` and exports the plotted values, time-point summaries, JSON model summary, and combined figure to `result/`.

## Scope and limitations

- Quantitative geometry was restricted to approximately planar regions with clearly visible, consistently fluorescent cell walls.
- Cell-wall outlines were manually traced, and reconstructed networks were visually inspected before geometric calculation.
- Boundary cells and strongly curved, folded, or tilted regions were excluded.
- The representation assumes predominantly polygonal cells and a single contiguous shared interface between retained neighboring cells; it is not intended for strongly lobed or zigzag-shaped epidermal cells.

## Citation and data provenance

When using this directory, cite the associated article and Zenodo record. The underlying live-imaging dataset should also be cited:

Zhao Y, Nakayama H, Okuda S, Higashiyama T, and Tsukaya H. **A live-imaging system for Arabidopsis leaf primordia at early stages.** bioRxiv (2026). [https://doi.org/10.64898/2026.02.08.704715](https://doi.org/10.64898/2026.02.08.704715)

See the [archive-level README](../README.md) for the associated manuscript citation and links to the other Supplementary Data Sets.
