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

## Trial 1: Use an Existing Polygonal Cell Network

### Step 1: Load a background image

From the plugin main window, open:

`Open` → `Open Background`

Then select:

`example-data/exampled_raw_image.jpg`

The background image will be displayed.

### Step 2: Import polygonal cell networks

Open:

`Import&Export` → `Import Polygonal Cell Networks`

Then select:

`example-data/exampled_polygonal_cell_networks.json`

The polygonal cell network is displayed as an overlay.

### Step 3: Compute neighbor-pair geometry

Open:

`Geometry` → `Neighbor Pair Geometrical Calculation`

A preview of neighboring cell pairs and calculated geometric values will be shown.

### Step 4: Estimate division pairs

Open:

`Estimate` → `Divided Pair Estimation`

Use the following settings (by default):

- **Geometry feature:** `junctionAngleAverageDegrees`
- **Comparison:** `Above or equal to threshold (>=)`
- **Threshold:** `145`
- **Matching:** `GlobalMaximumWeight`

Select **OK**. Estimated divisions are displayed as orange arrows.

### Step 5: Import real division pairs

Open:

`Import&Export` → `Import Real Division Pairs`

Then select:

`example-data/exampled_real_division_paris.csv`

Real divisions are displayed as pink arrows.

### Step 6: Compare estimated vs. real divisions

Open:

`Estimate` → `Compare Estimated and Real Divisions`

Results are shown as arrows:

- **True positives:** dark red  
- **False positives:** green  
- **False negatives:** blue

### Step 7: Export geometry results

Open:

`Import&Export` → `Export Neighbor Pair Geometrics`

This exports geometric measurements and real division labels for downstream analysis.

### Step 8: Export estimated division pairs

Open:

`Import&Export` → `Export Estimated Division Pairs`

This exports position information for all estimated division pairs.

---

## Trial 2: Extract a Polygonal Cell Network from a Binary Image

### Step 1: Reset the workspace

Either open a new plugin window, or use:

`Edit` → `Delete All and Reset`

### Step 2: Load a binary image

Open:

`Open` → `Binary Image`

Then select:

`example-data/exampled_binary_image`

The binary polygonal network image will be displayed.

### Step 3: Skeletonize

Open:

`Detect` → `Skeletonize`

This converts the binary image to a one-pixel-wide skeleton.

### Step 4: Detect vertices

Open:

`Detect` → `Detect Vertices`

Vertices are detected where a pixel has three connected neighbors.

Manual editing shortcuts:

- **Add vertex:** place the cursor at the desired location and press `Ctrl+V`
- **Delete vertex:** select the mislabeled vertex and press `Ctrl+D`

### Step 5: Detect lines

Open:

`Detect` → `Detect Lines`

Lines are detected from the skeletonized image.

Manual editing shortcuts:

- **Delete misconnected line:** select the line and press `Ctrl+D`
- **Connect two vertices:** select both vertices and press `Ctrl+L`

### Step 6: Detect polygons

Open:

`Detect` → `Detect Polygons`

Polygons are detected from vertices and lines.

Manual editing shortcuts:

- **Delete misdetected polygon:** select the polygon and press `Ctrl+D`
- **Create polygon from selected vertices:** press `Ctrl+P`

### Step 7: Export polygonal cell networks

Open:

`Import&Export` → `Export Polygonal Cell Networks`

This exports all vertex, line, and polygon information, which could be imported and analyzed by Trial 1 similarly (but notice that the cell index could be different and cause mistakes in the real division pairs import).