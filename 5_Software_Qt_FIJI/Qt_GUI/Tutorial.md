# Cell Division Inference Qt Application Tutorial

## 1. Installation

The current standalone **Cell Division Inference Qt application** is provided for **Windows x64**.

The portable application is located in:

`Qt_GUI/Qt_release/`

Keep all files and subfolders inside `Qt_release/` together, because the application depends on the included Qt, OpenCV, and runtime libraries.

> **Platform note:** The current Qt application is distributed for Windows only. macOS users should use the **FIJI/ImageJ plugin**.

---

## 2. Launch the Application

Open:

`Qt_GUI/Qt_release/CellDivisionInference.exe`

The Cell Division Inference main window will appear.

---

## 3. Example Dataset

Sample files are provided in:

`example-data/`

---

# Trial 1: Extract a Polygonal Cell Network from a Binary Image

## Step 1: Load a binary image

Open:

`Open` → `Open Raw Image`

Then select the binary cell-outline image, for example:

`example-data/sample2_12h/sample2_12h_outline`

The image will be displayed in the main workspace.

---

## Step 2: Skeletonize

Open:

`Detect` → `Skeletonization`

This converts the binary cell-wall image into a one-pixel-wide skeleton used for polygonal-network reconstruction.

---

## Step 3: Detect vertices

Open:

`Detect` → `Vertex Detection`

Vertices are automatically detected from junctions in the skeletonized cell-wall network.

If necessary, vertices can be manually edited:

- **Add vertex:** place the cursor at the desired position and press `Ctrl+V`
- **Delete vertex:** select the vertex and press `Ctrl+D`

Vertices can also be added or deleted through the `Edit` menu.

---

## Step 4: Detect lines

Open:

`Detect` → `Line Detection`

Cell-wall lines connecting detected vertices will be reconstructed from the skeleton.

Manual editing is available when necessary:

- **Delete a line:** select the line and press `Ctrl+D`
- **Connect two vertices:** select exactly two vertices and press `Ctrl+L`

---

## Step 5: Detect polygons

Open:

`Detect` → `Polygon Detection`

Cell polygons are reconstructed from the detected vertices and lines.

Manual editing shortcuts:

- **Delete a polygon:** select the polygon and press `Ctrl+D`
- **Create a polygon:** select at least three vertices in polygon order and press `Ctrl+P`

The reconstructed polygonal cell network should be visually inspected before downstream analysis.

---

## Step 6: Export the polygonal cell network

> **Alternatively:** After completing polygon detection, you can continue directly to **Trial 2** without exporting and re-importing the polygonal network. In this case, skip **Trial 2, Step 2: Import the polygonal cell network**, because the reconstructed network is already loaded in the current session.

Open:

`Import & Export` → `Export`

Select:

- **Geometry JSON**

Choose the destination folder and file name, then click **OK**.

The exported `.json` file stores the reconstructed polygonal cell network, including vertices, lines, and polygons, and can subsequently be imported for analysis in **Trial 2**.

> **Important:** If the polygonal network is manually edited after ground-truth division labels have been assigned, polygon IDs may change. This can cause mismatches with the corresponding `real_division_pairs` file and lead to incorrect comparison or performance results.

---

# Trial 2: Use an Existing Polygonal Cell Network

## Step 1: Load a background image

Open:

`Open` → `Open Background`

Then select, for example:

`example-data/sample2_12h/sample2_12h_background.jpg`

The background image will be displayed in the workspace.

---

## Step 2: Import the polygonal cell network

Open:

`Import & Export` → `Import`

Then select:

`example-data/sample2_12h_polygonal_network.json`

The vertices, lines, and polygons stored in the JSON file will be displayed over the background image.

---

## Step 3: Detect neighbor pairs

Open:

`Detect` → `Detect Neighbor Pairs`

Pairs of neighboring cells are identified from the polygonal network and displayed as orange dashed lines connecting the cell centroids.

If the neighbor-pair lines interfere with visualization, open:

`Display` → `Neighbor Pair Display Setting`

and disable their display.

---

## Step 4: Compute neighbor-pair geometry

Open:

`Geometry` → `Neighbor Pair Geometry Calculation`

The software calculates geometric descriptors for every neighboring cell pair.

These measurements include three major groups of features:

- **Pairwise features** — properties calculated from the two individual cells;
- **Union features** — properties of the two-cell merged region; and
- **Contact features** — properties of the shared cell wall and junction geometry.

These geometric features are used for subsequent daughter-cell-pair inference.

---

## Step 5: Estimate division pairs

Open:

`Estimate` → `Estimate division by single geometry`

For the recommended default junction-angle method, use:

- **Geometry feature:** `Junction Angle Average (deg)`
- **Comparison:** `Above or equal to threshold`
- **Threshold:** `145`
- **Matching:** `Global maximum weight matching`

Click **OK**.

Neighboring cell pairs satisfying the geometric criterion are treated as candidate daughter pairs, and global maximum-weight matching resolves competing assignments so that one cell is not assigned to multiple inferred daughter pairs.

Estimated daughter-cell pairs are displayed as orange double-headed arrows.

---

## Step 6: Compare estimated vs. real divisions

For datasets with lineage-resolved ground-truth daughter-cell pairs, open:

`Estimate` → `Compare with real division`

When prompted, select the corresponding real division-pair file, for example:

`example-data/sample2_12h_real_division_pairs.csv`

The estimated and observed divisions are compared and classified as:

- **True positives:** dark red
- **False positives:** green
- **False negatives:** blue

The corresponding performance statistics are displayed in the application.

> This comparison is only applicable when experimentally resolved daughter-cell labels are available. Fixed-tissue samples without lineage information should not be treated as having validated daughter/non-daughter labels.

---

## Step 7: Export geometry results

Open:

`Import & Export` → `Export`

For downstream geometric analysis, select:

- **Neighbor pair geometry (CSV)**

The exported file:

`<base_name>_neighbor_geometry.csv`

contains one row for each neighboring cell pair, including pair identifiers and geometric features used for division inference.

The exported features include:

- **Pairwise features** — properties of the two individual cells;
- **Union features** — properties of the merged two-cell region; and
- **Contact features** — properties of the shared cell wall and junctions.

If ground-truth daughter-cell pairs have been loaded, the exported table can also contain observed-division information for evaluation.

The same **Export** dialog can additionally export:

- **Geometry JSON**
- **Real division pairs**
- **Estimated division pairs**
- **Performance matrix (CSV)**
- background images
- images with polygonal geometry
- images with real or estimated divisions
- comparison images

For most downstream geometric analyses, **Neighbor pair geometry (CSV)** is the main analysis-ready output.

---

# Processing the Supplementary Data Set

The same workflow described in **Trial 2** can be applied to the polygonal cell networks provided in the Supplementary Data Set.

For each sample:

1. Import the corresponding polygonal-network `.json` file.
2. Detect neighboring cell pairs.
3. Calculate neighbor-pair geometry.
4. Estimate daughter-cell pairs using the desired geometric feature and threshold.
5. When a corresponding `real_division_pairs` file is available, compare the estimated pairs with the observed daughter-cell pairs.
6. Export the geometry tables, inferred division pairs, comparison results, or images as needed.

Thus, the polygonal-network `.json` files and corresponding real division-pair files provided in the Supplementary Data Set can be processed using the same procedure demonstrated above.

> **Important:** Polygon IDs in a `real_division_pairs` file correspond to the polygon IDs in its associated polygonal-network `.json` file. If polygons are added, deleted, or reconstructed manually, these IDs may change and the original real division-pair file may no longer correspond correctly to the edited network.
