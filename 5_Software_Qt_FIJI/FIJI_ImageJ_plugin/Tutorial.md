# Cell Division Inference FIJI Plugin Tutorial

## 1. Installation

Install the **Cell Division Inference** plugin by copying the plugin JAR file into the FIJI plugins directory.

- **Plugin file:** `FIJI-plugin/plugin/Cell-Division-Inference-FIJI-plugin.jar`
- **Destination:** `<FIJI_main_directory>/plugins/`

### Platform notes

- **Windows example:** `C:/tool/Fiji-win.app/plugins`
- **macOS example:** `/Applications/Fiji.app/plugins`

After copying the JAR file, restart FIJI if it is already running.

## 2. Launch the Plugin

In FIJI, open:

`Plugins` → `Cell Division Inference`

The plugin main window will appear.

## 3. Example Dataset

Sample files are provided in:

`example-data/`

---

## Trial 1: Extract a Polygonal Cell Network from a Binary Image

### Step 1: Load a binary image

Open: 

`Open` → `Open Raw Image`

Then select:

`example-data/sample2_12h/sample2_12h_outline`

The binary polygonal network image will be displayed.

### Step 2: Skeletonize

Open:

`Detect` → `Skeletonization`

This converts the binary image to a one-pixel-wide skeleton.

### Step 3: Detect vertices

Open:

`Detect` → `Vertex Detection`

Vertices are detected where a pixel has three connected neighbors. 

You can also do manual edit by shortcuts, or usually "Polygon detection" step will delete the orphan vertices and lines, which belongs to no polygons.
- **Add vertex:** place the cursor at the desired location and press `Ctrl+V`
- **Delete vertex:** select the mislabeled vertex and press `Ctrl+D`

### Step 4: Detect lines

Open:

`Detect` → `Line Detection`

Lines are detected from the skeletonized image.

You can also do manual edit by shortcuts:

- **Delete misconnected line:** select the line and press `Ctrl+D`
- **Connect two vertices:** select both vertices and press `Ctrl+L`

### Step 5: Detect polygons

Open:

`Detect` → `Polygon Detection`

Polygons are detected from vertices and lines.

Manual editing shortcuts:

- **Delete misdetected polygon:** select the polygon and press `Ctrl+D`
- **Create polygon from selected vertices:** press `Ctrl+P`

### Step 6: Export polygonal cell networks
> **Alternatively:** After completing polygon detection, you can continue directly to **Trial 2** without exporting and re-importing the polygonal network. In this case, skip **Trial 2, Step 2: Import the polygonal cell network**, because the reconstructed network is already loaded in the current session.

Open:

`Import&Export` → `Export Polygonal Cell Networks`

This exports all vertex, line, and polygon information, which could be imported and analyzed by Trial 2 similarly (but notice that if cells are manually edited, then the cell index could be different and cause mistakes in the real division comparison and the performance matrix).

---

## Trial 2: Use an Existing Polygonal Cell Network

### Step 1: Load a background image

From the plugin main window, open:

`Open` → `Open Background`

Then select:

`example-data/sample2_12h/sample2_12h_background.jpg`

The background image will be displayed.

### Step 2: Import polygonal cell networks

Open:

`Import&Export` → `Import`

Then select:

`example-data/sample2_12h_polygonal_network.json`

The polygonal cell network is displayed as an overlay.

### Step 3: Detect neighbor pairs

Open:

`Detect` → `Detect Neighbor Pairs`

Neighbor Pairs will be displayed as orange dotted-lines. 
If you find this display disturbing, you can make this neighbor-pair orange dotted lines invisible by "Display-Neighbor Pair Display Setting-Show neighbor links".

### Step 4: Compute neighbor-pair geometry

Open:

`Geometry` → `Neighbor Pair Geometry Calculation`

A preview of neighboring cell pairs and calculated geometric values will be shown.

### Step 5: Estimate division pairs

Open:

`Estimate` → `Estimate division by single geometry`

Use the following settings (by default):

- **Geometry feature:** `junctionAngleAverageDegrees`
- **Comparison:** `Above or equal to threshold (>=)`
- **Threshold:** `145`
- **Matching:** `GlobalMaximumWeight`

Select **OK**. Estimated divisions are displayed as orange arrows.

### Step 6: Compare estimated vs. real divisions

Open:

`Estimate` → `Compare with real division`

When prompted, select the real division pair file:

`example-data/sample2_12h_real_division_pairs.csv`

Results are shown as arrows:

- **True positives:** dark red  
- **False positives:** green  
- **False negatives:** blue

### Step 7: Export geometry results

Open:

`Import & Export` → `Export`

For downstream geometric analysis, select:

- **Neighbor pair geometry (CSV)**

The exported file:

`<base_name>_neighbor_geometry.csv`

contains one row for each neighboring cell pair, including pair identifiers and geometric features used for division inference.

The features include:

- **Pairwise features** — properties of the two individual cells;
- **Union features** — properties of the merged two-cell region; and
- **Contact features** — properties of the shared cell wall and junctions.

If ground-truth daughter-cell pairs have been imported, the table can also include `observed_division` and `division_timing`.

> **Note:** For fixed-tissue datasets without lineage information, `observed_division` should not be interpreted as experimentally validated daughter/non-daughter ground truth.

The same **Export** dialog can also export polygonal-network JSON files, real and estimated division pairs, comparison results, and overlay images.

For most downstream analyses, **Neighbor pair geometry (CSV)** is the main analysis-ready output.


---

## Processing the Supplementary Data Set

The same workflow described in **Trial 2** can also be applied to the polygonal cell networks provided in the Supplementary Data Set.

For each sample:

1. Import the corresponding polygonal cell network `.json` file.
2. Detect neighboring cell pairs.
3. Calculate neighbor-pair geometry.
4. Estimate daughter-cell pairs using the desired geometric feature and threshold.
5. When a corresponding real division-pair file is available, import the matching `real_division_pairs` file and compare the estimated divisions with the observed daughter-cell pairs.
6. Export the geometry tables, estimated division pairs, comparison results, and other outputs as needed.

Thus, all provided polygonal cell network `.json` files and their corresponding real division-pair files in the Supplementary Data Set can be processed using the same procedure demonstrated above.

> **Important:** The polygon IDs in a `real_division_pairs` file correspond to the polygon IDs in its associated polygonal-network `.json` file. If the network is manually edited and polygons are deleted, added, or reconstructed, polygon IDs may change. In that case, the original real division-pair file may no longer correspond correctly to the edited network, which can lead to incorrect comparison and performance results.