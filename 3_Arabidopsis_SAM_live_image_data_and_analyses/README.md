# Supplementary Data Set 3: Arabidopsis shoot apical meristem live-imaging data and analyses

## Overview

This directory contains the *Arabidopsis thaliana* shoot apical meristem (SAM) live-imaging data and analyses used to evaluate whether shape-based inference of recent daughter-cell pairs generalizes beyond leaf primordia in **“Inferring recent cell division from cell shapes.”** It includes projected SAM images, polygonal cell networks, live-imaging-derived ground-truth daughter-pair annotations, analysis-ready neighboring-cell-pair geometry, Python scripts, and precomputed validation and feature-analysis outputs.

The source dataset is from Willis et al. (2016) and contains time-lapse imaging of SAM L1 epidermal cells at 4-h intervals over 3–4 days. Six SAM snapshots were analyzed. Daughter pairs were defined as neighboring direct sister cells generated within the preceding **36 h**, with neither daughter undergoing another division before the analyzed frame.

### Dataset summary

- Analyzed SAM snapshots: **6**
- Polygonal cell records across the six snapshots: **873**
- Neighboring-cell-pair records: **2,336**
- Ground-truth daughter pairs: **386**
- Ground-truth non-daughter pairs: **1,950**
- Geometric features per neighboring pair: **49**
- Daughter-pair timing range: **4–36 h**, in 4-h increments
- Mean-junction-angle threshold transferred from the leaf dataset: **≥145°**
- Performance at 145° after maximum-weight matching: **98.4% precision, 97.2% recall, and 97.8% F1 score**

| Snapshot | Polygonal cells | Neighboring pairs | Daughter pairs |
| --- | ---: | ---: | ---: |
| `1-76h` | 171 | 460 | 75 |
| `2-76h` | 162 | 435 | 72 |
| `4-76h` | 152 | 403 | 67 |
| `13-84h` | 164 | 445 | 72 |
| `15-84h` | 140 | 377 | 64 |
| `18-84h` | 84 | 216 | 36 |
| **Total** | **873** | **2,336** | **386** |

## Directory contents

| Directory | Contents and purpose |
| --- | --- |
| [`01_polygonal_network/`](01_polygonal_network/) | Six 512 × 512-pixel grayscale z-projection images, corresponding polygonal-network JSON files, and live-imaging-derived daughter-pair annotations. |
| [`02_neighbor_pair_geometrics/`](02_neighbor_pair_geometrics/) | The principal analysis-ready table, `SAM_6samples_neighbor_pair_geometry.csv`, containing identifiers, ground-truth annotations, exception labels, and 49 geometric features for all 2,336 neighboring pairs. The directory retains the implementation name `geometrics`. |
| [`03_single_threshold_analysis/`](03_single_threshold_analysis/) | Pair-level inference and the performance matrix for the transferred mean-junction-angle threshold of 145°. |
| [`04_ranged_threshold_analysis/`](04_ranged_threshold_analysis/) | Mean-junction-angle threshold sweep from 120° to 170°, the included analysis script, the complete performance table, and the precision/recall plot. |
| [`05_mixture_model/`](05_mixture_model/) | Daughter/non-daughter distribution summary and a two-component axial von Mises analysis of mean junction angle, including component-to-label Wasserstein distances. |
| [`06_Best_F1_overlap_index/`](06_Best_F1_overlap_index/) | Precomputed best post-matching F1 summaries for feature/direction classifiers, overlap indices for the 49 features, and top-10 and all-feature comparison figures. |

## Images, polygonal networks, and ground truth

### Projected images

Each `*_zproj.jpg` or `*-zproj.jpg` file in `01_polygonal_network/` is a 512 × 512-pixel grayscale projection corresponding to the analyzed SAM snapshot. The prefix identifies the sample, and the suffix identifies the analyzed time point in hours. These files are projected images, not the complete original 3D time-lapse stacks.

### Polygonal-network JSON files

Each JSON file represents one snapshot and contains four top-level arrays:

- `vertices`: vertex IDs and image-coordinate x/y positions;
- `lines`: cell-wall segments defined by start and end vertex IDs;
- `polygons`: cell IDs and their ordered vertex IDs; and
- `neighborPairs`: pairs of polygon IDs sharing a cell wall.

Coordinates are stored in image-pixel units. Unnormalized lengths and areas in the geometry tables are therefore expressed in pixel-based coordinate units; normalized values, ratios, and shape indices are dimensionless, and angles are in degrees.

### Ground-truth daughter-pair files

Each `*_real_division_pairs.txt` file begins with the header `real_division_pair`. Subsequent lines are space-separated records:

```text
first_cell_id second_cell_id division_timing_hours
```

Cell-pair order is not biologically meaningful. When these files are read directly, pairs should be treated as unordered and deduplicated by cell-ID pair. The analysis-ready geometry table already represents each neighboring pair once; see [Data consistency note](#data-consistency-note).

## Analysis-ready neighboring-pair geometry

`02_neighbor_pair_geometrics/SAM_6samples_neighbor_pair_geometry.csv` is the principal input for downstream analysis. An identical convenience copy is supplied in `04_ranged_threshold_analysis/`.

The table contains 56 columns: seven identification/annotation columns followed by 49 geometric features.

| Column | Meaning |
| --- | --- |
| `fileName` | Polygonal-network filename identifying the SAM snapshot. |
| `pairIndex` | Neighboring-pair index within the snapshot. |
| `firstPolygonId`, `secondPolygonId` | IDs of the two neighboring cells. |
| `observed_division` | Ground-truth class: `1` for a daughter pair and `0` for a non-daughter pair. |
| `division_timing` | Hours elapsed since division for daughter pairs; `-1` for non-daughter pairs. |


The 49 features follow the manuscript-defined categories of pairwise cell geometry, union geometry, and shared-interface/contact geometry. Important implementation-to-manuscript mappings include:

| CSV column | Manuscript term |
| --- | --- |
| `junctionAngleAverageDegrees` | Mean junction angle |
| `normalizedSharedEdgeLength` | Normalized shared cell wall length |
| `unionConvexDeficiency` | Union convex deficiency |
| `unionCircularity` | Union circularity |

The copy in `05_mixture_model/` contains the same 2,336 neighboring pairs and 49 features but uses the legacy annotation names `is_real_division` and `divided_how_many_hours_ago` and does not contain `exception_label`. This difference does not change the supplied SAM result because the principal table contains no exception-labeled rows.

## Division-inference convention

The threshold-based analyses follow the same procedure used for the leaf-primordium dataset:

1. Rows labeled as exceptions are removed before candidate construction; no SAM rows are excluded in the supplied table.
2. A feature threshold defines candidate daughter-pair edges.
3. Candidate edges are weighted by their margin beyond the threshold. For mean junction angle with a `>=` rule, the weight is `junction angle − threshold`.
4. Exact maximum-weight matching is applied separately within each snapshot with `maxcardinality=False`, so one cell can occur in at most one inferred direct daughter pair.
5. The matched pairs are compared with `observed_division` to calculate TP, FP, FN, TN, precision, recall, and F1 score.

The supervised F1-optimized threshold and the unsupervised mixture-model cutoff are distinct. The former is selected against ground-truth labels after matching; the latter is derived from the fitted unlabeled junction-angle distribution.


## Requirements

Python **3.10 or later** is recommended. Install the packages used by the two included scripts with:

```bash
python -m pip install numpy pandas scipy matplotlib networkx
```

Reconstruction or editing of polygonal networks and regeneration of geometry tables from images require the Cell Division Inference software in [`../5_Software_Qt_FIJI/`](../5_Software_Qt_FIJI/). The included Python scripts start from the supplied geometry CSV files.

## Running the included analyses

Run each script from its own directory. Precomputed outputs are included; rerunning may replace files in the configured output directory.

### Junction-angle threshold sweep

```bash
cd 04_ranged_threshold_analysis
python estimate_by_ranged_threshold.py
```

The input filename, feature, operator, threshold range, step, and output paths are set near the beginning of the script. The supplied configuration evaluates `junctionAngleAverageDegrees >= threshold` from 120° to 170° in 1° steps and recalculates exact maximum-weight matching at every threshold.

### Feature distribution and mixture analysis

```bash
cd 05_mixture_model
python bimodality_cutoff_analysis.py
```

Equivalent explicit arguments are:

```bash
python bimodality_cutoff_analysis.py SAM_6samples_neighbor_pair_geometry.csv \
  --feature junctionAngleAverageDegrees \
  --result-dir result_SAM_junction_angles
```

For any selected feature, the script reports daughter- and non-daughter-pair mean ± sample SD. For feature names containing both `junction` and `angle`, it additionally fits the axial von Mises mixture and reports the two Wasserstein distances.
