# Data, analysis code, and software for “Inferring recent cell division from cell shape”

## Overview

This repository accompanies the manuscript **“Inferring recent cell division from cell shape.”** It contains the five Supplementary Data Sets used to develop, validate, and apply a shape-based framework for inferring recent daughter-cell pairs from static polygonal cell networks.

The repository includes image data, cell-wall outlines, polygonal-network files, live-imaging-derived ground-truth daughter-pair annotations, geometric measurements, analysis scripts and outputs, and FIJI/ImageJ and Qt/C++ software.

## Repository contents

1. [`1_Arabidopsis_leaf_live_image_data_and_analyses/`](1_Arabidopsis_leaf_live_image_data_and_analyses/) — *Arabidopsis thaliana* leaf live-imaging data, polygonal cell networks, ground-truth daughter pairs, neighboring-cell-pair geometry, and the principal validation and feature analyses.

2. [`2_Arabidopsis_rpl4d3_WT_fixed_primordia_data_and_analyses/`](2_Arabidopsis_rpl4d3_WT_fixed_primordia_data_and_analyses/) — Fixed wild-type and *rpl4d-3* leaf-primordium data, inferred divisions, leaf-shape and division-orientation analyses, and sensitivity analyses.

3. [`3_Arabidopsis_SAM_live_image_data_and_analyses/`](3_Arabidopsis_SAM_live_image_data_and_analyses/) — *Arabidopsis* shoot apical meristem (SAM) live-imaging data, polygonal cell networks, ground-truth daughter pairs, neighboring-cell-pair geometry, and independent validation analyses.

4. [`4_Non_model_fixed_sample_analysis/`](4_Non_model_fixed_sample_analysis/) — Fixed-sample images, extracted cell-shape data, neighboring-cell-pair geometry, and mixture-model analyses for *Ipomoea coccinea* and *Oxalis corniculata*.

5. [`5_Software_Qt_FIJI/`](5_Software_Qt_FIJI/) — The Cell Division Inference FIJI/ImageJ plugin and Qt graphical user interface, including source code, compiled distributions, example data, and tutorials.

Detailed data definitions, analysis instructions, and software requirements are provided in the README files within the corresponding numbered directories.
