# Supplementary Data Set 2: *Arabidopsis rpl4d-3* and wild-type fixed leaf-primordium data and analyses

## Overview

This directory contains the fixed *Arabidopsis thaliana* leaf-primordium data and analyses used to compare wild type and the pointed-leaf mutant *rpl4d-3* in **“Inferring recent cell division from cell shapes.”** It includes primordium images and outlines, polygonal cell networks, analysis-ready neighboring-cell-pair geometry, inferred daughter-cell pairs, division-orientation analyses, sensitivity analyses, Python scripts, and precomputed outputs.

Wild type is the trichome-deficient Col-0 background `gl1`; *rpl4d-3* is in the same background. To compare primordia of similar size despite delayed mutant growth, first foliage leaf primordia were collected at **4 days after sowing (DAS)** for wild type and **4.5 DAS** for *rpl4d-3*.

### Dataset summary

- Fixed primordia used for cell-network and division-orientation analysis: **20** (**10 wild type and 10 *rpl4d-3**)
- Polygonal cells represented in the network files: **1,173 wild type and 1,307 *rpl4d-3***
- Neighboring-cell-pair records: **3,064 wild type and 3,441 *rpl4d-3***
- Geometric features per neighboring pair: **49**
- Daughter pairs inferred at the default mean-junction-angle threshold of 145 degrees: **460 wild type and 541 *rpl4d-3***


## Directory contents

| Directory | Contents and purpose |
| --- | --- |
| [`01-Primordia-leaf-shape/`](01-Primordia-leaf-shape/) | Primordium images, headerless outline-coordinate tables, and scripts for plotting individual or mean outlines and calculating local curvature. |
| [`01-Primordia-leaf-shape/WT_primordia_shape/`](01-Primordia-leaf-shape/WT_primordia_shape/) | Wild-type image/outline pairs.|
| [`01-Primordia-leaf-shape/mutant_primordia_shape/`](01-Primordia-leaf-shape/mutant_primordia_shape/) | *rpl4d-3* TIFF image/outline pairs. |
| [`02-Primordia-cell-polygonal-networks/`](02-Primordia-cell-polygonal-networks/) | Polygonal-network JSON files for 10 primordia per genotype and the corresponding batch neighboring-pair geometry tables. |
| [`03-Estimated-cell-divisions/`](03-Estimated-cell-divisions/) | Analysis-ready geometry with `estimated_division` labels, the current and legacy orientation-analysis scripts, and precomputed orientation and sensitivity outputs. |


## Primordium outlines and curvature

The outlines are treated as closed contours ordered from one basal margin, through the distal tip, to the opposite basal margin. 

`outline_plot.py` is an interactive single-sample example. `outline_plots.py` summarizes the mean contour and normal-direction SD band; its supplied configuration reads wild-type outlines and plot.

 `curvature_plot.py` summarizes curvature; its supplied configuration reads the mutant outlines and plot. Edit the glob pattern and output filename near the end of either script to analyze the other genotype.

## Polygonal-network JSON files

The `WT/` and `rpl4d-3/` subdirectories of `02-Primordia-cell-polygonal-networks/` contain 10 network files each. Wild-type filenames begin with `4d-WT-`; mutant filenames begin with `4.5d-rpl4d-3-`.

The network representation uses:

- `vertices`: vertex IDs and image-coordinate x/y positions;
- `lines`: cell-wall segments defined by start and end vertex IDs;
- `polygons`: cell IDs and ordered vertex IDs, with line IDs included in some files;
- `neighborPairs`: an optional stored list of adjacent polygon IDs; and
- `canvas` and `display`: optional viewing metadata.



## Neighboring-pair geometry tables

`02-Primordia-cell-polygonal-networks/` contains the camelCase geometry exports:

- `WT_fixed_samples_batch_neighbor_pair_geometry.csv`: 3,064 rows x 60 columns;
- `rpl4d-3_fixed_samples_batch_neighbor_pair_geometry.csv`: 3,441 rows x 60 columns.

Each row represents one neighboring cell pair and contains identifiers, both cell centroids, placeholder annotation fields, and 49 geometric features. The features follow the manuscript categories of pairwise cell geometry, union geometry, and shared-interface/contact geometry. Important names include:

| CamelCase column | Manuscript term |
| --- | --- |
| `junctionAngleAverageDegrees` | Mean junction angle |
| `normalizedSharedEdgeLength` | Normalized shared cell wall length |
| `unionConvexDeficiency` | Union convex deficiency |
| `unionCircularity` | Union circularity |

Because these are fixed samples, all rows in these tables have `observed_division = 0`, `division_timing = -1`, and `exception_label = 0`. These values are placeholders, not negative ground-truth annotations.

The two `*_batch_single_geometry_estimation.csv` files in `03-Estimated-cell-divisions/` contain the same neighboring-pair sets in snake_case form, together with the inferred class `estimated_division`. Key equivalents are `filename`, `cell_1_id`, `cell_2_id`, `junction_angle_mean`, `shared_edge_length_normalized`, `convex_deficiency`, and `union_circularity`.

## Division-inference and orientation conventions

The default analysis follows the manuscript conventions:

1. Candidate pairs satisfy `junction_angle_mean >= 145` degrees.
2. The candidate-edge weight is `junction_angle_mean - 145`.
3. Exact maximum-weight matching is applied separately within each primordium, with `maxcardinality=False`, so a cell can occur in at most one inferred daughter pair.
4. `estimated_division = 1` marks the selected matched pairs.
5. The division axis is the line connecting the two inferred daughter-cell centroids.
6. Division orientation is the acute angle from the medio-lateral (ML) axis: 0 degrees is ML-aligned/horizontal and 90 degrees is proximo-distal (PD)-aligned/vertical.
7. The pair-centroid PD position is normalized within each primordium. `y_norm >= 0.5` is apical and `y_norm < 0.5` is basal.
8. For visualization, angles are classified as horizontal (`0 <= angle < 30` degrees), diagonal (`30 <= angle < 60` degrees), or vertical (`60 <= angle <= 90` degrees).

Genotypes are compared using the per-primordium proportion of vertical inferred divisions. The script performs a two-sided exact permutation test over all **184,756** assignments of 10 of the 20 primordia to the wild-type group. Pooled histograms are descriptive visualizations; the primordium is the statistical replicate.

### Sensitivity analysis

The current script repeats inference across:

- mean-junction-angle thresholds from **140 to 150 degrees** in 1-degree steps; and
- normalized apical boundaries from **0.40 to 0.60** in 0.02 steps.

Maximum-weight matching is recalculated at every threshold before the regional summaries and exact permutation tests are computed. The output contains 121 threshold-by-boundary combinations and 2,420 primordium-level records. `p_exact` values are per-combination exact P values; the script does not apply a multiple-testing correction.


## Requirements

Python **3.10 or later** is recommended. Install the packages used by the included scripts with:

```bash
python -m pip install numpy pandas matplotlib networkx
```

Reconstruction or editing of polygonal networks and regeneration of geometry tables from cell-wall outlines require the Cell Division Inference software in [`../5_Software_Qt_FIJI/`](../5_Software_Qt_FIJI/). The Python analyses here start from the supplied outline or geometry tables.

## Running the analyses

Run scripts from their own directory because the configured inputs are relative to the script or working directory. Precomputed results are included; rerunning may replace same-named outputs.

### Outline examples and summaries

```bash
cd 01-Primordia-leaf-shape
python outline_plot.py
python outline_plots.py
python curvature_plot.py
```

The scripts contain editable input globs and output filenames rather than command-line arguments. On a headless system, set a noninteractive Matplotlib backend if needed, for example `MPLBACKEND=Agg`.

### Division-orientation and sensitivity analysis

```bash
cd 03-Estimated-cell-divisions
python histogram_and_box_plot_division_angles_batch.py
```

The current script reads the two supplied `*_batch_single_geometry_estimation.csv` files and writes the complete result set to `separate_region_figures_with_sensitivity/`. `histogram_and_box_plot_division_angles_batch_old.py` is retained for provenance and should not be used in place of the current sensitivity-enabled script.

## Scope and interpretation

- This directory infers recent daughter-cell pairs from static geometry; it does not contain live-imaging lineage ground truth.
- The fixed 145-degree threshold was established in the live-imaging leaf-primordium analysis in Supplementary Data Set 1 and is applied here to compare genotypes.
- Inferred division orientation refers to the centroid-to-centroid axis of the inferred daughter pair, not a directly observed cell plate.
- Network construction assumes clearly resolved, approximately polygonal epidermal cells and a single contiguous shared interface for retained neighboring pairs.
- Sensitivity results assess robustness over the stated threshold and regional-boundary ranges; they are not a new ground-truth validation of the fixed samples.
