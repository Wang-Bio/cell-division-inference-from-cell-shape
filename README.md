# A high-throughput cell division detection method based on inferring from cell shapes

This repository contains the **code, software, and datasets** for the study:

> **A high-throughput cell division detection method based on inferring from cell shapes**  
> Zining Wang, Yujie Zhao, Hokuto Nakayama, Gorou Horiguchi, Yasuhiro Inoue\, Atsushi Mochizuki\, and Hirokazu Tsukaya\*

This study is current under submission/reviewing process, and the algorithm/codes might be improved or additional features could be enhanced.

The study introduces a practical method to infer **recent daughter-cell pairs** from static cell geometry, with a simple junction-angle threshold (default ~145°) and optional machine-learning extensions.

This repository provides a FIJI/ImageJ-plugin and a standalone Qt-based software which could be used for cell division estimation. The example data and the full data set were also provided. 

## Repository structure
```text
.
├── FIJI-plugin/
│   └── UTokyo.TsukayaLab.WangZining-1.0-SNAPSHOT.jar
├── Qt-GUI-standalone-software/
│   ├── portable-software/
│   └── source-code/
├── full-data-set/
│   ├── 13_primordia_30_snapshots/
│   └── all_neighbor_pairs_geometry.csv
├── example-data/
│   ├── exampled_raw_image.jpg
│   ├── exampled_binary_image.png
│   └── real_division_paris.csv
└── LICENSE
```

### Key contents

- `full-data-set/13_primordia_30_snapshots/`  
  Polygon-network files (`.json`) and corresponding observed daughter-pair labels (`*_real_division_pairs.csv` or `.txt`) from live imaging.
- `full-data-set/all_neighbor_pairs_geometry.csv`  
  Aggregated geometric feature table across neighboring pairs.
- `FIJI-plugin/UTokyo.TsukayaLab.WangZining-1.0-SNAPSHOT.jar`  
  Plugin package for FIJI/ImageJ.
- `Qt-GUI-standalone-software/source-code/`  
  C++/Qt/OpenCV source code for network extraction and division inference.
- `example-data/`  
  Minimal files for quick trial runs and software checking.

---

## Biological scope and intended use

This method is validated primarily on **early Arabidopsis leaf primordia** and is most reliable in tissues where:

- cell boundaries can be segmented cleanly;
- bimodel distribution of mean junction angle is significant.

The manuscript further demonstrates transferability checks in non-model species (e.g., *Ipomoea coccinea*, *Oxalis corniculata*) using bimodality of junction-angle distributions.

---

## Typical workflow for fixed-tissue experiments

1. **Prepare sample** (fixation + cell wall staining).
2. **Acquire confocal image** with clear epidermal boundaries.
3. **Binary image** manually draw the polygon cell networks. Or skip step 3,4 and manually label the vertex and generate polygonal cell networks from confocal image. 
4. **Segment/skeletonize** boundaries (within provided tools/workflow).
5. **Extract polygon network** and neighbor pairs.
6. **Compute geometric features**.
7. **Infer daughter pairs** using single-feature rule (recommended default: mean junction angle cutoff near 145°), or
8. **Apply one-to-one matching** to enforce biological constraints (a cell cannot belong to multiple daughter pairs).
9. **Export division map** and downstream metrics (e.g., orientation distribution, regional patterns).

---

## Reproducibility notes

- Ground-truth daughter pairs in this repository were annotated from live-imaging time series.
- The study benchmark reports high precision and recall using a single-feature junction-angle criterion (above 145° mean junction angle, 93.5% precision, 92.2% recall)
- The full neighbor-pair feature table and per-snapshot files are provided to support re-analysis.
- Example files are included for fast verification of file formats and analysis flow.

If you are reviewing methodology transfer to another species/system, we recommend first checking whether the mean junction-angle distribution is bimodal in your dataset, and then set a value cutoff to distinguish the high-angle group and low-angle group. Also We recommend you to only use early leaf primordia, where cells are actively dividing.

---

## Contact

For correspondence regarding materials, method usage, or reproducibility:
- First author:
- Zining Wang: `wangzining2020@g.ecc.u-tokyo.ac.jp`
- Correspondence authors:
- Hirokazu Tsukaya: `tsukaya3946@g.ecc.u-tokyo.ac.jp`
- Yasuhiro Inoue: `inoue.yasuhiro.4n@kyoto-u.ac.jp`
- Atsushi Mochizuki: `mochi@infront.kyoto-u.ac.jp`
