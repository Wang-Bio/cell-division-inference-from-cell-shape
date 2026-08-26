# Cell Division Inference

**Cell Division Inference** is a software package implementing a shape-based framework for the **retrospective inference of recent daughter-cell pairs from static plant tissue images**. The workflow represents an approximately planar tissue as a polygonal cell network, calculates geometric descriptors for neighboring cell pairs, and infers likely daughter-cell relationships from division-associated geometric signatures, particularly junction geometry.

The package provides two graphical implementations:

- a FIJI/ImageJ plugin, which can be used on platforms supported by FIJI, including macOS and Windows; and
- a standalone Qt/C++ graphical user interface, currently distributed as a Windows x64 application.

The software is intended for analysis of tissues composed predominantly of polygonal cells. It is designed to infer the latest or recent daughter-cell relationships from local cell geometry rather than to reconstruct complete lineage trees.


## Repository structure

```text
5Software_Qt_FIJI/
├── FIJI_ImageJ_plugin/
│   ├── plugin/
│   │   └── Cell-Division-Inference-FIJI-plugin.jar
│   ├── src/
│   │   └── CellDivisionInference.java
│   │   └── pom.xml
│   └── Tutorial.md
│
└── Qt_GUI/
    ├── Qt_release/
    │   ├── CellDivisionInference.exe
    │   ├── Qt runtime libraries
    │   ├── OpenCV runtime libraries
    │   └── required Qt plugin folders
    ├── source-code/
    │   ├── *.cpp / *.h
    │   ├── mainwindow.ui
    │   └── polygons_on_white_canvas_final.pro
    └── Tutorial.md
```

## Purpose and scope

The software supports a two-stage cell-shape-based division-inference workflow. Cell-wall outlines are first prepared from projected images or other segmentation results. From these outlines, the software reconstructs the polygonal cell network, calculates geometric features of neighboring cell pairs, and applies a calibrated geometric rule or classifier to infer daughter-cell pairs. The inferred pairs can then be visualized as spatial division maps and exported for downstream analyses such as division-orientation and regional-pattern quantification.

The framework is most appropriate when:

- the analyzed tissue region is approximately planar;
- cells can be represented reasonably well as simple polygons; and
- neighboring cells share a single contiguous interface.

It is not intended for strongly lobed or zigzag-shaped epidermal cells, or for complete reconstruction of older lineage relationships.

## Main functions

The software supports the major steps of the Cell Division Inference workflow, including:

- loading cell-wall outline images and existing polygonal-network data;
- automatic detection of vertices and cell-wall lines from binary cell-wall outlines;
- reconstruction of a **polygonal cell network** consisting of vertices, lines, and cell polygons;
- visual inspection and manual correction of reconstructed networks;
- identification of **neighboring cell pairs** from shared cell-wall interfaces;
- calculation of geometric descriptors for neighboring cell pairs, including **pairwise features, union features, and contact features**;
- **daughter-cell-pair inference** using calibrated single-feature rules or classifiers;

The Qt implementation additionally includes utilities for **single-feature analysis** and **mixture modeling of junction-angle distributions**, which can be used to examine bimodality and estimate a model-derived cutoff in datasets without daughter/non-daughter labels.

## Installation

### FIJI/ImageJ plugin

1. Install [FIJI](https://fiji.sc/) if it is not already available.
2. Copy:

   ```text
   FIJI_ImageJ_plugin/plugin/Cell-Division-Inference-FIJI-plugin.jar
   ```

   into the FIJI plugins directory:

   ```text
   <Fiji.app>/plugins/
   ```

3. Restart FIJI.
4. Launch the plugin from:

   ```text
   Plugins > Cell Division Inference
   ```

The plugin is distributed as a compiled JAR. Its source code is provided in `FIJI_ImageJ_plugin/src/`.

### Qt/C++ application — Windows

A portable Windows x64 release is provided in:

```text
Qt_GUI/Qt_release/
```

To run it:

1. Keep **all files and subfolders** inside `Qt_release/` together.
2. Launch:

   ```text
   CellDivisionInference.exe
   ```

No separate Qt installation should be required for the packaged release because the required Qt, MinGW, OpenCV, and plugin runtime files are included in the release directory.

> Do not move only the `.exe` out of the release directory, because the application depends on the accompanying DLLs and Qt plugin folders.

## Typical workflow for fixed-tissue experiments

1. **Prepare the sample** using an appropriate fixation and cell-wall staining method.

2. **Acquire confocal images** with clearly visible cell boundaries.

3. **Prepare cell-wall outlines.** Manually trace the polygonal cell-wall network from the projected confocal image to generate a binary cell-outline image. In this study, tracing was performed using MediBang Paint on an iPad with an Apple Pencil, with the Brush set to Pencil and a 2-pixel brush size. Other drawing or image-processing software can also be used, provided that it produces clear and continuous cell-wall outlines suitable for subsequent network reconstruction.

   Alternatively, users may skip preparation of a binary outline image and the subsequent skeletonization step, and instead manually annotate vertices and cell-wall lines directly on the confocal image within the software to construct the polygonal cell network.

4. **Reconstruct the polygonal cell network.** For binary cell-outline images, use the provided workflow to skeletonize the cell-wall outlines, detect vertices and cell-wall lines, and reconstruct cell polygons. Visually inspect the reconstructed network and manually correct vertices, lines, or polygons when necessary.

5. **Identify neighboring cell pairs** from the reconstructed polygonal cell network.

6. **Calculate geometric features** for cells and neighboring cell pairs.

7. **Infer candidate daughter-cell pairs** using a selected geometric criterion. The recommended default is **mean junction angle ≥ 145°**.

8. **Export the inferred division map and analysis results** for downstream analyses, such as division-orientation distributions and regional-pattern quantification.

## Tutorials for softwares

Separate tutorials are included for the two interfaces:

- `FIJI_ImageJ_plugin/Tutorial.md`
- `Qt_GUI/Tutorial.md`

These tutorials are **currently under development**. They will provide step-by-step instructions, example input files, recommended settings, expected outputs, and example analyses. Some tutorial paths or example-data references may therefore be incomplete in the present version.

## Input and output

Depending on the analysis, the software can work with:

- cell-wall outline or background image files;
- polygonal-network JSON files;
- neighboring-cell-pair geometry CSV files; and
- daughter-cell-pair or ground-truth label files when available.

Typical outputs include:

- reconstructed polygonal cell networks;
- neighboring-cell-pair geometry tables;
- single-cell geometry tables;
- inferred daughter-cell-pair tables;
- spatial division maps; and
- performance statistics when ground-truth daughter-cell labels are available.

Detailed file formats and example files will be documented in the tutorials.

## Building from source

### FIJI plugin

The Java source code is provided in:

```text
FIJI_ImageJ_plugin/src/
```

The distributed plugin uses FIJI/ImageJ together with Java dependencies including Gson and JGraphT. The compiled JAR is provided for users who do not need to rebuild the plugin.

### Qt/C++ application

The Qt source code is provided in:

```text
Qt_GUI/source-code/
```

The current development build uses:

- C++17;
- Qt 6;
- OpenCV 4.8; and
- Boost.Graph.

The current qmake project contains development-machine paths for OpenCV and Boost. These paths may need to be changed to match the local installation before compiling on another computer.

## Versioned archive and latest development

This deposit is a versioned archive of the Cell Division Inference software used in the associated manuscript. It is intended to preserve a stable and reproducible software version corresponding to the study.

The software may continue to be updated after this archived version, including bug fixes, documentation improvements, and additional functions. For the latest version and future updates, please check the GitHub repository:

https://github.com/Wang-Bio/cell-division-inference-from-cell-shape

For reproduction of the analyses reported in the manuscript, please use the archived software version associated with the paper. For general use, we recommend checking the GitHub repository for the most recent version.

## Citation

Wang Z, Zhao Y, Nakayama H, Horiguchi G, Inoue Y, Mochizuki A, Tsukaya H. Inferring recent cell division from cell shapes. Manuscript currently under revision at The Plant Cell.

A final journal citation and article DOI are not yet available. 

## License

Cell Division Inference is released under the MIT License.

## Contact

Developed in the **Tsukaya Laboratory, The University of Tokyo**.

### First author

- **Zining Wang** — `wangzining2020@g.ecc.u-tokyo.ac.jp`

### Corresponding authors

- **Hirokazu Tsukaya** — `tsukaya3946@g.ecc.u-tokyo.ac.jp`
- **Yasuhiro Inoue** — `inoue.yasuhiro.4n@kyoto-u.ac.jp`
- **Atsushi Mochizuki** — `mochi@infront.kyoto-u.ac.jp`