# Background-layer regression procedure

Use this reproducible procedure in both the Qt application and FIJI plugin.

1. Create a 64 x 64 solid-red RGB source PNG and two 64 x 64 solid-blue/cyan background PNGs. Also create a 32 x 48 background and an invalid `.png` text file.
2. Open the red source and extract (or manually create) a triangular network. Set polygon/line display to green. Export JSON and record every vertex, line and polygon ID, coordinate, line pixel path, neighbour pair and division result.
3. Open the blue background. Verify an ordinary pixel is blue, no ordinary pixel is red, and all green geometry, labels, vertices, division overlays, selections and debug highlights remain above it. Re-export JSON and byte/diff the recorded structural fields.
4. Open the cyan background. Verify cyan replaces blue and that the scene/item inspector contains at most one replacement raster.
5. Cancel the chooser, then choose the invalid file. In both cases verify the cyan display, zoom, network and results are unchanged.
6. Choose **Open > Remove Background / Show Source Image**. Verify red returns, the command becomes disabled, and the network/result comparison is unchanged.
7. Open the 32 x 48 image. Verify the dialog reports `64 x 64` and `32 x 48`, defaults to Cancel, and Cancel changes nothing. Repeat and explicitly select **Scale Background to Network Canvas**; verify only the raster becomes 64 x 64 and all network coordinates remain unchanged.
8. With replacement mode active, export every available overlay-image variant. Verify background pixels come from the visible replacement (never red) and overlays have the same ordering as the viewer.
9. Start a new source/canvas. Verify replacement mode is cleared and the restore command is disabled.
10. Import a geometry-only JSON, add a same-sized background, and repeat steps 3, 5, 6, and 8. Exercise selection, movement, deletion, reconnection, debugging and division estimation after each switch.

The visual oracle is deterministic: ordinary pixels are `(0,0,255)` after replacement, network pixels are green, and no `(255,0,0)` source pixel is visible until source mode is restored.
