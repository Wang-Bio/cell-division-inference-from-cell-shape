1. Install "Cell Division Inference" FIJI plugin into your FIJI

For windows users, please copy and paste the "FIJI-plugin/plugin/Cell-Division-Inference-FIJI-plugin.jar" into your "FIJI_main_directory/plugins/". 
(Under this /plugins/, usually there are many other plugins' .jar file by default)
(For example, the directory is C:/tool/Fiji-win.app/plugins in Wang's windows pc)

For mac users, please copy and paste the "FIJI-plugin/plugin/Cell-Division-Inference-FIJI-plugin.jar" into your ... (ChatGPT, please help me fix this).

2. Open this plugin by opening FIJI-menu-Plugins-"Cell Division Inference", then the window will open

3. Please find the example data set in "example-data/"

--- Trial 1: with identified polygonal cell network

4. In the "Cell Division Inference" plugin main window, please select menu-"Open-Open Background", and open the "example-data/exampled_raw_image.jpg". Then the background image will be shown.

5. Select menu-Import&export-Import Polygonal Cell Networks, and import the "example-data/exampled_polygonal_cell_networks.json". Then the detected polygonal cell network will be displayed as an overlay on the background image. 

6. Select the menu-Geometry-Neighbor Pair Geometrical Calculation. Then a preview of neighbor pair cells and calculation results will be shown. 

7. Select the menu-Estimate-divided pair estimation, and keep Geometry feature: "junctionAngleAverageDegrees" by default, also keep Comparison: "Above or equal to threshold (>=)", and threshold: 145, and matching: GlobalMaximumWeight by default, and then select "OK". Then the estimated divisions will be displayed by orange arrows. 

8. Select the menu-Import&Export-Import Real Division Pairs, and import the "example-data/exampled_real_division_paris.csv". Then the real divisions will be displayed by pink arrows. 

9. Select the menu-Estimate-Compare Estimated and Real Divisions. Then the true positive, false positive, and false negative of division estimation will be displayed by arrows. (true positives by dark red arrows, false positives by green arrows, false negatives by blue arrows)

10. Select the menu-Import&Export-Export Neighbor Pair Geometrics, which will export all the geometrical results and also the real divisions for further analysis. 

11. Select the menu-Import&Export-Export Estimated Division Pairs, which will export the position informaiton of all estimated divided pairs, for further analysis. 


----- Trial 2: Extract polygonal cell networks from binary image -----

12. Open a new window of plugin "Cell Division Inference" or select the menu-Edit-Delete All and Reset

13. Select menu-Open-Binary Image, and open the "exampled-data/exampled_binary_image". Then binary image of polygonal cell networks will be displayed.

14. Select menu-Detect-Skeletonize, which will make the binary image to be a one-pixel width skeletonized image.

15. Select menu-Detect-Detect Vertices, which will detect the vertices if any pixel has three connected pixels, then users have to manually add some vertices in the outermost part (put mouse onto the desired place and then press ctrl+V, will create a vertex; to delete a miss labeled vertex, just select the vertex and press ctrl+D). 

16. Select menu-Detect-Detect Lines, which will detect the lines based on the skeletonized image. Some lines might be misconnected, you manually delete (by select the misconnected line and press ctrl+D) and reconnect (by select the two vertices and press ctrl+L). 

17. Select menu-Detect-Detect Polygons, which will detect the polygons based on the vertices nad lines. Basically all polygons should be correct, but still you can manuaally delete (by selecting the misconnected polygon and press ctrl+D) and reconnect (by selecting the vertices and press ctrl+P).

18. Select menu-Import&Export-Export Polygonal Cell Networks, which will export all information about the vertices, lines, and polygons. 