# Supplementary Data Set 4: Fixed sample data and analyses for non-model plants

This directory contains fixed leaf-primordium images, extracted polygonal cell networks, neighboring-cell-pair geometry, inferred candidate daughter-cell pairs, and label-free junction-angle mixture analysis for *Ipomoea coccinea* and *Oxalis corniculata*. These data support the non-model-species applicability analysis described in the associated manuscript.

## Important interpretation

The samples are fixed and have no time-lapse lineage ground truth. Accordingly:

- inferred pairs in this directory are **candidate daughter-cell pairs**, not experimentally observed daughter pairs;
- the `observed_division` or `is_real_division` columns contain placeholder zeros and must not be interpreted as biological negative labels; and
- high-angle fraction above cutoff and high-angle retention are fitted-component separation measures, not empirical precision or recall.

## Biological material and imaging

- *I. coccinea* seeds were collected from a naturalized population in Kamakura, Japan; leaf primordia were collected at 4 days after sowing (DAS).
- *O. corniculata* seeds were collected from the Hongo Campus of the University of Tokyo; leaf primordia were collected at 15 DAS.
- Primordia were fixed overnight in FAA, stained with 0.5 mg/L Calcofluor White in ClearSee solution for at least three days at room temperature, and imaged with an Olympus FV3000 confocal microscope using a 40x objective, 405-nm excitation, and 425-475-nm detection.

The deposited image files are 2D source/projection images and exported views; original confocal z-stacks are not included in this directory.

## Directory structure

```text
4_Non_model_fixed_sample_analysis/
├── 01_Ipomoea_coccinea_raw_image_and_cell_shapes/
│   ├── Ipomoea_coccinea_sample1/
│   ├── Ipomoea_coccinea_sample2/
│   └── Ipomoea_coccinea_sample3/
├── 01_Oxalis_corniculata_raw_image_and_cell_shapes/
│   ├── Oxalis_corniculata_sample2/
│   ├── Oxalis_corniculata_sample3/
│   ├── Oxalis_corniculata_sample4/
│   └── Oxalis_corniculata_sample5/
└── 02_Neighbor_pair_geometry_analysis/
    ├── Ipomoea_coccinea_3samples_neighbor_geometry.csv
    ├── Oxalis_corniculata_4samples_neighbor_geometry.csv
    └── bimodality_cutoff_analysis_junction_angle.py
```



## Reproducing the pooled mixture analysis

### Requirements

- Python 3.10 or later
- NumPy
- pandas
- Matplotlib
- SciPy

Example installation:

```bash
python -m pip install numpy pandas matplotlib scipy
```

From `02_Neighbor_pair_geometry_analysis`, run:

```bash
python bimodality_cutoff_analysis_junction_angle.py \
  Ipomoea_coccinea_3samples_neighbor_geometry.csv \
  --result-dir result_Ipomoea_coccinea \
  --no-component-label-closeness \
  --no-observed-label-plots

python bimodality_cutoff_analysis_junction_angle.py \
  Oxalis_corniculata_4samples_neighbor_geometry.csv \
  --result-dir result_Oxalis_corniculata \
  --no-component-label-closeness \
  --no-observed-label-plots
```

The two `--no-*` options are required for these fixed-sample tables because optional label-based comparisons require both observed daughter and non-daughter labels, which are unavailable here. Without `--no-component-label-closeness`, the supplied all-zero placeholder label column causes that optional analysis to stop.

The script treats junction angles as axial circular data on 0-180°, fits a two-component von Mises mixture by expectation-maximization, orders the components by mean angle, and defines the model-derived cutoff as the intersection of their weighted densities. Principal defaults are random seed 0, 15 initializations, up to 400 EM iterations, 35 histogram bins, and 600-dpi figure output.