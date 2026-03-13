# Cell Division Inference by Qt-based GUI software

## 1. Installation 
There is no need for installation for windows users. Open the "/portable-software/inferring_cell_division.exe", and there is no need for library deployment. 

### Platform notes
The linux version and mac version is currently under development. 

## 2. Launch the software

Double click the "/portable-software/inferring_cell_division.exe". 

## 3. Example Dataset

Sample files are provided in:

`example-data/`

---

## Trial 1: Use an Existing Polygonal Cell Network

### Step 1: Load a background image

From the main window, select the menu-Open-Open Background, and open the `example-data/exampled_raw_image.jpg`

The background image will be displayed. 

### Step 2: Import polygon cell networks

Select the menu-Import&Export-Import, and open the `example-data/exampled_polygonal_cell_networks.json`.

The polygonal cell networks will be displayed as an overlay above the background mage. 

### Step 3: Compute neighbor-pair geometry

Select the menu-Geometry-Neighbor pair geometry calculation

A preview of neighboring cell pairs and calculated geometric values will be shown.

### Step 4: Estimate division pairs

Open:

`Estimate` → `Estimate Division by Single Geometry`

Use the following settings (default):

- **Geometry feature** `Junction Angle Average (deg)` 
- **Comparison** `Above or equal to threshold`
- **Threshold** `145.0`
- **Matching** `Global maximum weight matching`

Select **OK**. Estimated divisions are displayed as orange arrows.

### Step 5: Compare estimated vs. real divisions

Open:

`Estimate` → `Compare with Real Divisions`

Results are shown as arrows:

- **True positives:** dark red  
- **False positives:** green  
- **False negatives:** blue

### Step 7: Export results

Open:

`Import&Export` → `Export `

In the export panel, select "Geometry JSON", "Vertex information", "Line information", "Polygon information", "Real division pairs", "Estimated division pairs", "Neighbor pair geometry (CSV)", and other options if you needed.

---

## Trial 2: Extract a Polygonal Cell Network from a Binary Image

### Step 1: Load a binary image

Open:

`Open` → `Open Raw Image`

Then select:

`example-data/exampled_binary_image`

The binary polygonal network image will be displayed.

### Step 2: Skeletonize

Open:

`Detect` → `Skeletonization`

This converts the binary image to a one-pixel-wide skeleton.

### Step 3: Detect vertices

Open:

`Detect` → `Vertex Detection`

Vertices are detected where a pixel has three connected neighbors, and please manually add some periphery vertices.

Manual editing shortcuts:

- **Add vertex:** place the cursor at the desired location and press `Ctrl+V`
- **Delete vertex:** select the mislabeled vertex and press `Ctrl+D`

### Step 4: Detect lines

Open:

`Detect` → `Line Detection`

Lines are detected from the skeletonized image.

Manual editing shortcuts:

- **Delete misconnected line:** select the line and press `Ctrl+D`
- **Connect two vertices:** select both vertices and press `Ctrl+L`

### Step 6: Detect polygons

Open:

`Detect` → `Polygon Detection`

Polygons are detected from vertices and lines.

Manual editing shortcuts:

- **Delete misdetected polygon:** select the polygon and press `Ctrl+D`
- **Create polygon from selected vertices:** press `Ctrl+P`

### Step 7: Export polygonal cell networks

Open:

`Import&Export` → `Export`

In the export panel, pleas select In the export panel, select "Geometry JSON", "Vertex information", "Line information", "Polygon information". 