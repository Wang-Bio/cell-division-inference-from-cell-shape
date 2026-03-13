package jp.utokyo.tsukayalab;

import ij.IJ;
import ij.ImagePlus;
import ij.io.OpenDialog;
import ij.plugin.PlugIn;
import ij.process.ByteProcessor;
import ij.process.ImageProcessor;
import ij.io.SaveDialog;

import javax.swing.*;
import javax.swing.table.AbstractTableModel;
import javax.swing.table.DefaultTableCellRenderer;
import javax.swing.border.TitledBorder;

import java.awt.*;
import java.awt.event.*;
import java.awt.image.BufferedImage;
import java.util.*;
import java.util.List;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.awt.geom.AffineTransform;
import java.awt.geom.Area;
import java.awt.geom.Path2D;
import java.awt.geom.Point2D;
import java.awt.geom.Rectangle2D;
import java.awt.geom.PathIterator;
import java.awt.geom.FlatteningPathIterator;
import java.text.DecimalFormat;

import com.google.gson.*;

public class CellDivisionInference implements PlugIn {

    private static class CenteredViewPanel extends JPanel {
        private final JComponent content;

        CenteredViewPanel(JComponent content){
            super(new GridBagLayout());
            this.content = content;
            setOpaque(true);
            add(content, new GridBagConstraints());
        }

        @Override
        public Dimension getPreferredSize(){
            Dimension pref = content.getPreferredSize();
            Container parent = getParent();
            if(parent instanceof JViewport){
                Dimension extent = ((JViewport)parent).getExtentSize();
                return new Dimension(Math.max(pref.width, extent.width), Math.max(pref.height, extent.height));
            }
            return pref;
        }
    }


    private ImagePlus currentImp;
    private BufferedImage currentBI;
    private String currentGeometrySourceFileName = "unknown.json";

    private ImagePanel imagePanel;
    private JPanel centeredImageContainer;
    private JScrollPane scrollPane;
    private JFrame mainFrame;
    private JPanel infoPanel;

    // ====== Left panel information =====
    private JLabel infoSourceValue;
    private JLabel infoCanvasSizeValue;
    private JLabel infoMousePositionValue;
    private JLabel infoSelectedItemValue;
    private JLabel infoSelectedItemIdValue;
    private JLabel infoSelectedItemPosValue;
    private JLabel infoVertexCountValue;
    private JLabel infoLineCountValue;
    private JLabel infoPolygonCountValue;
    private JLabel infoNeighborCountValue;
    private JLabel infoEstimatedCountValue;
    private JLabel infoRealCountValue;
    private JLabel infoTruePositiveCountValue;
    private JLabel infoFalsePositiveCountValue;
    private JLabel infoFalseNegativeCountValue;
    private JLabel infoTrueNegativeCountValue;
    private JLabel infoPrecisionValue;
    private JLabel infoRecallValue;
    private JLabel infoF1Value;
    private JLabel infoAccuracyValue;
    private JLabel infoSpecificityValue;

    // ===== Vertex display =====
    private Color vertexColor = new Color(0xAA, 0xFF, 0xFF);
    private int vertexRadius = 8;

    // ===== Line display =====
    private Color lineColor = Color.GRAY;   
    private float lineWidth = 2.0f;          

    // ===== Polygon display =====
    private boolean polygonRandomColors = false;               // uniform vs random
    private Color polygonUniformColor = new Color(128, 128, 128);
    private int polygonFillAlpha = 64;

    // ===== Cell ID label display =====
    private boolean showCellId = true;
    private Color cellIdColor = Color.WHITE;
    private int cellIdFontSize = 10;                        

    // ===== Neighbor link display style =====
    private boolean showNeighborLinks = true;
    private Color neighborLinkColor = new Color(255,85,0);
    private float neighborLinkWidth = 1.5f;

    // ===== Estimated division arrow display style =====
    private boolean showDivisionArrows = true;
    private Color divisionArrowColor = new Color(255, 140, 0);
    private float divisionArrowWidth = 2.0f;                    
    private double divisionArrowHeadLength = 5.0;                 
    private double divisionArrowHeadWidth  = 3.0;  
    
    // ===== Real division arrow display style =====
    private boolean showRealDivisionArrows = true;
    private Color realDivisionArrowColor = new Color(255,153,153);
    private float realDivisionArrowWidth = 2.0f;
    private double realDivisionArrowHeadLength = 5.0;
    private double realDivisionArrowHeadWidth = 3.0;
    private DivisionArrowType realDivisionArrowType = DivisionArrowType.DoubleHeaded;

    // ===== TP / FP / FN arrow display style (Qt defaults) =====
    // Qt colors from divisionestimator.cpp:
    // TP "#AA1E1E", FP "#50AA78", FN "#3C78C8"
    private boolean showTruePositiveDivisionArrows = true;
    private Color truePositiveArrowColor = new Color(170, 30, 30);
    private float truePositiveArrowWidth = 2.0f;
    private double truePositiveArrowHeadLength = 5.0;
    private double truePositiveArrowHeadWidth  = 3.0;
    private DivisionArrowType truePositiveArrowType = DivisionArrowType.DoubleHeaded;

    private boolean showFalsePositiveDivisionArrows = true;
    private Color falsePositiveArrowColor = new Color(80, 170, 120);
    private float falsePositiveArrowWidth = 2.0f;
    private double falsePositiveArrowHeadLength = 5.0;
    private double falsePositiveArrowHeadWidth  = 3.0;
    private DivisionArrowType falsePositiveArrowType = DivisionArrowType.DoubleHeaded;

    private boolean showFalseNegativeDivisionArrows = true;
    private Color falseNegativeArrowColor = new Color(60, 120, 200);
    private float falseNegativeArrowWidth = 2.0f;
    private double falseNegativeArrowHeadLength = 5.0;
    private double falseNegativeArrowHeadWidth  = 3.0;
    private DivisionArrowType falseNegativeArrowType = DivisionArrowType.DoubleHeaded;

    private enum DivisionArrowType { DoubleHeaded, SingleHeaded, LineOnly }
    private DivisionArrowType divisionArrowType = DivisionArrowType.DoubleHeaded;

    // ===== Vertex ====
    private List<Point> vertexPoints = new ArrayList<>();

    // ===== Lines =====
    static class LineEdge {

        final int v1;
        final int v2;

        LineEdge(int a,int b){

            v1=Math.min(a,b);
            v2=Math.max(a,b);
        }

        public boolean equals(Object o){

            if(!(o instanceof LineEdge)) return false;

            LineEdge e=(LineEdge)o;

            return v1==e.v1 && v2==e.v2;
        }

        public int hashCode(){

            return v1*1000003 + v2;
        }
    }

    private List<LineEdge> lineEdges=new ArrayList<>();

    // ===== Polygons =====
    private List<List<Integer>> polygons=new ArrayList<>();

    // ===== Polygon Neighbors =====
    // ===== Polygon neighbor pair key =====
    static class PolyPair {
        final int a;
        final int b;
        PolyPair(int i, int j){
            a = Math.min(i, j);
            b = Math.max(i, j);
        }
        @Override public boolean equals(Object o){
            if(!(o instanceof PolyPair)) return false;
            PolyPair p = (PolyPair)o;
            return a == p.a && b == p.b;
        }
        @Override public int hashCode(){
            return a * 1000003 + b;
        }
    }

    // undirected edge key (v1,v2) -> long
    private static long edgeKey(int v1, int v2){
        int lo = Math.min(v1, v2);
        int hi = Math.max(v1, v2);
        return (((long)lo) << 32) ^ (hi & 0xffffffffL);
    }

    @Override
    public void run(String arg){

        SwingUtilities.invokeLater(() -> {

            JFrame frame=new JFrame("Cell Division Inference");
            mainFrame = frame;
            frame.setDefaultCloseOperation(WindowConstants.DISPOSE_ON_CLOSE);

            imagePanel=new ImagePanel();
            centeredImageContainer = new CenteredViewPanel(imagePanel);
            centeredImageContainer.setBackground(Color.BLACK);
            scrollPane=new JScrollPane(centeredImageContainer);
            infoPanel = createInfoPanel();

            scrollPane.getVerticalScrollBar().setUnitIncrement(16);
            scrollPane.getHorizontalScrollBar().setUnitIncrement(16);

            JSplitPane splitPane = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, infoPanel, scrollPane);
            splitPane.setResizeWeight(0.0);
            splitPane.setDividerSize(8);
            splitPane.setContinuousLayout(true);
            splitPane.setBorder(BorderFactory.createEmptyBorder());
            frame.add(splitPane, BorderLayout.CENTER);

            JMenuBar bar=new JMenuBar();

            JMenu fileMenu=new JMenu("Open");

            fileMenu.add(new JMenuItem(new AbstractAction("Binary Image…"){
                public void actionPerformed(ActionEvent e){
                    openImage(frame);
                }
            }));

            fileMenu.add(new JMenuItem(new AbstractAction("Create Canvas..."){
                public void actionPerformed(ActionEvent e){
                    createCanvas(frame);
                }
            }));

            fileMenu.add(new JMenuItem(new AbstractAction("Open Background..."){
                public void actionPerformed(ActionEvent e){
                    openBackground(frame);
                }
            }));

            JMenu processMenu=new JMenu("Detect");

            processMenu.add(new JMenuItem(new AbstractAction("Skeletonize"){
                public void actionPerformed(ActionEvent e){
                    skeletonize2D(frame);
                }
            }));

            processMenu.add(new JMenuItem(new AbstractAction("Detect Vertices"){
                public void actionPerformed(ActionEvent e){
                    detectVertices3Neighbors(frame);
                }
            }));

            processMenu.add(new JMenuItem(new AbstractAction("Detect Lines"){
                public void actionPerformed(ActionEvent e){
                    detectLines(frame);
                }
            }));

            processMenu.add(new JMenuItem(new AbstractAction("Detect Polygons"){
                public void actionPerformed(ActionEvent e){
                    detectPolygons(frame);
                }
            }));

            processMenu.add(new JMenuItem(new AbstractAction("Detect Neighbor Polygons"){
                public void actionPerformed(ActionEvent e){
                    detectNeighborPairs(frame);
                }
            }));

            /* 
            processMenu.add(new JMenuItem(new AbstractAction("Neighbor Pair Preview..."){
                @Override public void actionPerformed(ActionEvent e){
                    showNeighborPairPreviewDialog(frame);
                }
            }));
            */
            JMenu geometryMenu=new JMenu("Geometry");

            geometryMenu.add(new JMenuItem(new AbstractAction("Neighbor Pair Geometrical Calculation...") {
                @Override public void actionPerformed(ActionEvent e){
                    calculateAllNeighborPairGeometries(frame);
                }                
            }));

            JMenu estimateMenu=new JMenu("Estimate");

            estimateMenu.add(new JMenuItem(new AbstractAction("divided pair estimation...") {
                @Override public void actionPerformed(ActionEvent e){
                    showNeighborPairEstimationDialog(frame);
                }
            }));

            estimateMenu.add(new JMenuItem(new AbstractAction("Compare Estimated and Real Division...") {
                @Override public void actionPerformed(ActionEvent e){
                    compareEstimatedAndRealDivision(frame);
                }
            }));

            JMenu editMenu = new JMenu("Edit");

            editMenu.add(new JMenuItem(new AbstractAction("Add vertex...") {
                @Override public void actionPerformed(ActionEvent e){
                    showAddVertexDialog(frame);
                }
            }));

            editMenu.add(new JMenuItem(new AbstractAction("Add line...") {
                @Override public void actionPerformed(ActionEvent e){
                    showAddLineDialog(frame);
                }
            }));

            editMenu.add(new JMenuItem(new AbstractAction("Add polygon...") {
                @Override public void actionPerformed(ActionEvent e){
                    showAddPolygonDialog(frame);
                }
            }));

            editMenu.addSeparator();

            editMenu.add(new JMenuItem(new AbstractAction("Delete vertex...") {
                @Override public void actionPerformed(ActionEvent e){
                    showDeleteVertexDialog(frame);
                }
            }));

            editMenu.add(new JMenuItem(new AbstractAction("Delete line...") {
                @Override public void actionPerformed(ActionEvent e){
                    showDeleteLineDialog(frame);
                }
            }));

            editMenu.add(new JMenuItem(new AbstractAction("Delete polygon...") {
                @Override public void actionPerformed(ActionEvent e){
                    showDeletePolygonDialog(frame);
                }
            }));

            editMenu.addSeparator();

            editMenu.add(new JMenuItem(new AbstractAction("Delete all vertices") {
                @Override public void actionPerformed(ActionEvent e){
                    deleteAllVertices();
                }
            }));

            editMenu.add(new JMenuItem(new AbstractAction("Delete all lines") {
                @Override public void actionPerformed(ActionEvent e){
                    deleteAllLines();
                }
            }));

            editMenu.add(new JMenuItem(new AbstractAction("Delete all polygons") {
                @Override public void actionPerformed(ActionEvent e){
                    deleteAllPolygons();
                }
            }));

            editMenu.addSeparator();

            editMenu.add(new JMenuItem(new AbstractAction("Delete whole polygonal cell network"){
                @Override public void actionPerformed(ActionEvent e){
                    deleteAllPolygons();
                    deleteAllLines();
                    deleteAllVertices();
                }
            }));

            editMenu.add(new JMenuItem(new AbstractAction("Delete all and reset") {
                @Override public void actionPerformed(ActionEvent e){
                    resetWholeWindow(frame);
                }
            }));

            JMenu displayMenu = new JMenu("Display");

            displayMenu.add(new JMenuItem(new AbstractAction("Vertex Settings...") {
                @Override
                public void actionPerformed(ActionEvent e) {
                    showVertexSettingsDialog(frame);
                }
            }));

            displayMenu.add(new JMenuItem(new AbstractAction("Line Settings...") {
                @Override
                public void actionPerformed(ActionEvent e) {
                    showLineSettingsDialog(frame);
                }
            }));

            displayMenu.add(new JMenuItem(new AbstractAction("Polygon Settings...") {
                @Override
                public void actionPerformed(ActionEvent e) {
                    showPolygonSettingsDialog(frame);
                }
            }));

            displayMenu.add(new JMenuItem(new AbstractAction("Neighbor Link Settings..."){
                public void actionPerformed(ActionEvent e){
                    showNeighborLinkSettings(frame);
                }
            }));

            displayMenu.add(new JMenuItem(new AbstractAction("Division Arrow Settings..."){
                @Override public void actionPerformed(ActionEvent e){
                    showDivisionArrowSettings(frame);
                }
            }));

            JMenu findMenu = new JMenu("Find");

            findMenu.add(new JMenuItem(new AbstractAction("Find vertex...") {
                @Override public void actionPerformed(ActionEvent e){
                    showFindVertexDialog(frame);
                }
            }));

            findMenu.add(new JMenuItem(new AbstractAction("Find line...") {
                @Override public void actionPerformed(ActionEvent e){
                    showFindLineDialog(frame);
                }
            }));

            findMenu.add(new JMenuItem(new AbstractAction("Find polygon...") {
                @Override public void actionPerformed(ActionEvent e){
                    showFindPolygonDialog(frame);
                }
            }));

            JMenu debugMenu = new JMenu("Debug");

            debugMenu.add(new JMenuItem(new AbstractAction("Check unconnected vertices and lines") {
                @Override public void actionPerformed(ActionEvent e){
                    checkUnconnectedVerticesAndLines(frame);
                }
            }));


            JMenu ioMenu = new JMenu("Import & Export");

            ioMenu.add(new JMenuItem(new AbstractAction("Import Polygonal Cell Networks...") {
                @Override public void actionPerformed(ActionEvent e){
                    importData(frame);
                }                
            }));

            ioMenu.add(new JMenuItem(new AbstractAction("Import Real Division Pairs..."){
                @Override public void actionPerformed(ActionEvent e){
                    importRealDivisionPairs(frame);
                }
            }));

            ioMenu.add(new JMenuItem(new AbstractAction("Export Polygonal Cell Networks..."){
                @Override public void actionPerformed(ActionEvent e){
                    exportData(frame);
                }
            }));

            ioMenu.add(new JMenuItem(new AbstractAction("Export Neighbor Pair Geometrics..."){
                @Override public void actionPerformed(ActionEvent e){
                    exportNeighborPairGeometrics(frame);
                }
            }));

            ioMenu.add(new JMenuItem(new AbstractAction("Export Estimated Division Pairs..."){
                @Override public void actionPerformed(ActionEvent e){
                    exportEstimatedDivisionPairs(frame);
                }
            }));

            bar.add(fileMenu);
            bar.add(processMenu);
            bar.add(geometryMenu);
            bar.add(estimateMenu);
            bar.add(displayMenu);
            bar.add(editMenu);
            bar.add(findMenu);
            bar.add(debugMenu);
            bar.add(ioMenu);

            frame.setJMenuBar(bar);
            updateInfoPanel();
            frame.setSize(1200, 800);
            frame.setLocationRelativeTo(null);
            adjustWindowToContent();
            frame.setVisible(true);
        });
    }

    private void checkUnconnectedVerticesAndLines(JFrame frame){
        if(imagePanel == null) return;

        List<Point> verts = imagePanel.getOverlayPoints();
        int vertexCount = (verts == null) ? 0 : verts.size();
        int lineCount = lineEdges.size();

        int[] vertexLineDegree = new int[Math.max(0, vertexCount)];
        boolean[] vertexInPolygon = new boolean[Math.max(0, vertexCount)];
        boolean[] lineInPolygon = new boolean[Math.max(0, lineCount)];

        HashMap<Long, ArrayList<Integer>> lineIndexByEdge = new HashMap<>();

        for(int i = 0; i < lineEdges.size(); i++){
            LineEdge e = lineEdges.get(i);
            if(e == null) continue;

            if(e.v1 >= 0 && e.v1 < vertexCount) vertexLineDegree[e.v1]++;
            if(e.v2 >= 0 && e.v2 < vertexCount) vertexLineDegree[e.v2]++;

            if(e.v1 >= 0 && e.v1 < vertexCount && e.v2 >= 0 && e.v2 < vertexCount){
                long key = edgeKey(e.v1, e.v2);
                lineIndexByEdge.computeIfAbsent(key, k -> new ArrayList<>()).add(i);
            }
        }

        for(List<Integer> poly : polygons){
            if(poly == null || poly.isEmpty()) continue;

            for(int v : poly){
                if(v >= 0 && v < vertexCount){
                    vertexInPolygon[v] = true;
                }
            }

            if(poly.size() < 2) continue;

            int m = poly.size();
            for(int k = 0; k < m; k++){
                int a = poly.get(k);
                int b = poly.get((k + 1) % m);
                if(a < 0 || a >= vertexCount || b < 0 || b >= vertexCount) continue;

                ArrayList<Integer> lineIndices = lineIndexByEdge.get(edgeKey(a, b));
                if(lineIndices == null) continue;
                for(int lineIdx : lineIndices){
                    if(lineIdx >= 0 && lineIdx < lineInPolygon.length){
                        lineInPolygon[lineIdx] = true;
                    }
                }
            }
        }

        ArrayList<Integer> unconnectedVertices = new ArrayList<>();
        for(int i = 0; i < vertexCount; i++){
            if(vertexLineDegree[i] <= 1 || !vertexInPolygon[i]){
                unconnectedVertices.add(i);
            }
        }

        ArrayList<Integer> unconnectedLines = new ArrayList<>();
        for(int i = 0; i < lineCount; i++){
            if(!lineInPolygon[i]){
                unconnectedLines.add(i);
            }
        }

        imagePanel.setDebugHighlights(unconnectedVertices, unconnectedLines);

        IJ.log("[Debug] Unconnected vertices and lines check");
        IJ.log("[Debug] Unconnected vertices: " + unconnectedVertices.size());
        IJ.log("[Debug] Vertex indices: " + (unconnectedVertices.isEmpty() ? "(none)" : unconnectedVertices.toString()));
        IJ.log("[Debug] Unconnected lines: " + unconnectedLines.size());
        IJ.log("[Debug] Line indices: " + (unconnectedLines.isEmpty() ? "(none)" : unconnectedLines.toString()));

        IJ.showStatus("Debug check done: " + unconnectedVertices.size() + " unconnected vertices, " + unconnectedLines.size() + " unconnected lines.");
        if(frame != null){
            frame.toFront();
        }
    }


    private JPanel createInfoPanel(){
        JPanel panel = new JPanel(new GridBagLayout());
        panel.setBorder(BorderFactory.createTitledBorder(BorderFactory.createEtchedBorder(), "Information", TitledBorder.LEFT, TitledBorder.TOP));

        infoSourceValue = new JLabel("-");
        infoCanvasSizeValue = new JLabel("-");
        infoMousePositionValue = new JLabel("-");
        infoSelectedItemValue = new JLabel("-");
        infoSelectedItemIdValue = new JLabel("-");
        infoSelectedItemPosValue = new JLabel("-");
        infoVertexCountValue = new JLabel("-");
        infoLineCountValue = new JLabel("-");
        infoPolygonCountValue = new JLabel("-");
        infoNeighborCountValue = new JLabel("-");
        infoEstimatedCountValue = new JLabel("-");
        infoRealCountValue = new JLabel("-");
        infoTruePositiveCountValue = new JLabel("-");
        infoFalsePositiveCountValue = new JLabel("-");
        infoFalseNegativeCountValue = new JLabel("-");
        infoTrueNegativeCountValue = new JLabel("-");
        infoPrecisionValue = new JLabel("-");
        infoRecallValue = new JLabel("-");
        infoF1Value = new JLabel("-");
        infoAccuracyValue = new JLabel("-");
        infoSpecificityValue = new JLabel("-");

        GridBagConstraints gbc = new GridBagConstraints();
        gbc.insets = new Insets(3, 6, 3, 6);
        gbc.anchor = GridBagConstraints.NORTHWEST;
        gbc.fill = GridBagConstraints.HORIZONTAL;
        gbc.weightx = 0.0;

        int row = 0;
        addInfoRow(panel, gbc, row++, "Source", infoSourceValue);
        addInfoRow(panel, gbc, row++, "Canvas Size", infoCanvasSizeValue);
        addInfoRow(panel, gbc, row++, "Mouse Position", infoMousePositionValue);
        addInfoRow(panel, gbc, row++, "Selected Item", infoSelectedItemValue);
        addInfoRow(panel, gbc, row++, "Selected Item Id", infoSelectedItemIdValue);
        addInfoRow(panel, gbc, row++, "Selected Item Pos", infoSelectedItemPosValue);
        addInfoRow(panel, gbc, row++, "Vertex Num", infoVertexCountValue);
        addInfoRow(panel, gbc, row++, "Line Num", infoLineCountValue);
        addInfoRow(panel, gbc, row++, "Polygon Num", infoPolygonCountValue);
        addInfoRow(panel, gbc, row++, "Neighbor Pair Num", infoNeighborCountValue);
        addInfoRow(panel, gbc, row++, "Estimated Division Num", infoEstimatedCountValue);
        addInfoRow(panel, gbc, row++, "Real Division Num", infoRealCountValue);
        addInfoRow(panel, gbc, row++, "True Positive Num", infoTruePositiveCountValue);
        addInfoRow(panel, gbc, row++, "False Positive Num", infoFalsePositiveCountValue);
        addInfoRow(panel, gbc, row++, "False Negative Num", infoFalseNegativeCountValue);
        addInfoRow(panel, gbc, row++, "True Negative Num", infoTrueNegativeCountValue);
        addInfoRow(panel, gbc, row++, "Precision", infoPrecisionValue);
        addInfoRow(panel, gbc, row++, "Recall", infoRecallValue);
        addInfoRow(panel, gbc, row++, "F1", infoF1Value);
        addInfoRow(panel, gbc, row++, "Accuracy", infoAccuracyValue);
        addInfoRow(panel, gbc, row++, "Specificity", infoSpecificityValue);

        gbc.gridx = 0;
        gbc.gridy = row;
        gbc.gridwidth = 2;
        gbc.weighty = 1.0;
        panel.add(Box.createVerticalGlue(), gbc);

        panel.setPreferredSize(new Dimension(300, 620));
        panel.setMinimumSize(new Dimension(250, 200));
        return panel;
    }

    private void addInfoRow(JPanel panel, GridBagConstraints gbc, int row, String labelText, JLabel valueLabel){
        gbc.gridy = row;
        gbc.gridwidth = 1;
        gbc.weightx = 0.0;
        gbc.gridx = 0;
        panel.add(new JLabel(labelText), gbc);

        gbc.gridx = 1;
        gbc.weightx = 1.0;
        valueLabel.setHorizontalAlignment(SwingConstants.LEFT);
        panel.add(valueLabel, gbc);
    }

    private void adjustWindowToContent(){
        if(scrollPane == null || imagePanel == null || centeredImageContainer == null) return;

        centeredImageContainer.revalidate();
        scrollPane.revalidate();
    }

    private void updateInfoPanel(){
        if(infoSourceValue == null) return;

        boolean hasCanvas = currentBI != null || (imagePanel != null && imagePanel.hasBackgroundImage());
        boolean hasDivisionData = !neighborPairEstimationCache.isEmpty()
                || !realDivisionPairCache.isEmpty()
                || !truePositivePairCache.isEmpty()
                || !falsePositivePairCache.isEmpty()
                || !falseNegativePairCache.isEmpty();

        infoSourceValue.setText(currentGeometrySourceFileName == null ? "-" : currentGeometrySourceFileName);

        Dimension backgroundSize = (imagePanel == null) ? null : imagePanel.getBackgroundImageSize();
        if(backgroundSize != null){
            infoCanvasSizeValue.setText(backgroundSize.width + " x " + backgroundSize.height);
        }else if(currentBI != null){
            infoCanvasSizeValue.setText(currentBI.getWidth() + " x " + currentBI.getHeight());
        }else{
            infoCanvasSizeValue.setText("-");
        }

        infoVertexCountValue.setText(formatCount(hasCanvas, vertexPoints == null ? 0 : vertexPoints.size()));
        infoLineCountValue.setText(formatCount(hasCanvas, lineEdges == null ? 0 : lineEdges.size()));
        infoPolygonCountValue.setText(formatCount(hasCanvas, polygons == null ? 0 : polygons.size()));
        infoNeighborCountValue.setText(formatCount(hasCanvas, (imagePanel == null || imagePanel.getNeighborPairs() == null) ? 0 : imagePanel.getNeighborPairs().size()));

        int estimatedCount = 0;
        for(EstimationEntry e : neighborPairEstimationCache.values()){
            if(e != null && e.selected) estimatedCount++;
        }
        infoEstimatedCountValue.setText(formatCount(hasDivisionData, estimatedCount));
        infoRealCountValue.setText(formatCount(hasDivisionData, realDivisionPairCache.size()));
        infoTruePositiveCountValue.setText(formatCount(hasDivisionData, truePositivePairCache.size()));
        infoFalsePositiveCountValue.setText(formatCount(hasDivisionData, falsePositivePairCache.size()));
        infoFalseNegativeCountValue.setText(formatCount(hasDivisionData, falseNegativePairCache.size()));

        if(lastDivisionMetrics != null){
            infoTrueNegativeCountValue.setText(Integer.toString(lastDivisionMetrics.trueNegatives));
            infoPrecisionValue.setText(formatMetric(lastDivisionMetrics.precision));
            infoRecallValue.setText(formatMetric(lastDivisionMetrics.recall));
            infoF1Value.setText(formatMetric(lastDivisionMetrics.f1));
            infoAccuracyValue.setText(formatMetric(lastDivisionMetrics.accuracy));
            infoSpecificityValue.setText(formatMetric(lastDivisionMetrics.specificity));
        }else{
            infoTrueNegativeCountValue.setText("-");
            infoPrecisionValue.setText("-");
            infoRecallValue.setText("-");
            infoF1Value.setText("-");
            infoAccuracyValue.setText("-");
            infoSpecificityValue.setText("-");
        }

        if(imagePanel != null){
            imagePanel.updateSelectionInfoLabels();
        }
    }

    private static String formatMetric(double value){
        if(!Double.isFinite(value) || value < 0.0) return "-";
        return String.format(Locale.ROOT, "%.4f", value);
    }
    private static String formatCount(boolean available, int value){
        return available ? Integer.toString(value) : "-";
    }

    private void showAddVertexDialog(JFrame frame){
        JTextField xField = new JTextField(8);
        JTextField yField = new JTextField(8);

        JPanel p = new JPanel(new GridLayout(0, 2, 8, 8));
        p.add(new JLabel("X:"));
        p.add(xField);
        p.add(new JLabel("Y:"));
        p.add(yField);

        int ret = JOptionPane.showConfirmDialog(frame, p, "Add Vertex", JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE);
        if(ret != JOptionPane.OK_OPTION) return;

        Integer x = parseIntegerField(xField.getText());
        Integer y = parseIntegerField(yField.getText());
        if(x == null || y == null){
            JOptionPane.showMessageDialog(frame, "Please enter valid integer coordinates.", "Add Vertex", JOptionPane.ERROR_MESSAGE);
            return;
        }

        vertexPoints.add(new Point(x, y));
        imagePanel.setOverlayPoints(vertexPoints);
    }

    private void showAddLineDialog(JFrame frame){
        JTextField aField = new JTextField(20);
        JTextField bField = new JTextField(20);

        JPanel p = new JPanel(new GridLayout(0, 1, 8, 8));
        p.add(new JLabel("Vertex A (index [start from 0] or position x:y or x,y):"));
        p.add(aField);
        p.add(new JLabel("Vertex B (index [start from 0] or position x:y or x,y):"));
        p.add(bField);

        int ret = JOptionPane.showConfirmDialog(frame, p, "Add Line", JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE);
        if(ret != JOptionPane.OK_OPTION) return;

        Integer aIdx = resolveVertexReference(aField.getText(), true);
        Integer bIdx = resolveVertexReference(bField.getText(), true);
        if(aIdx == null || bIdx == null){
            JOptionPane.showMessageDialog(frame, "Invalid vertex reference. Use an index (0-based) or coordinates (x:y / x,y).", "Add Line", JOptionPane.ERROR_MESSAGE);
            return;
        }
        if(aIdx.equals(bIdx)){
            JOptionPane.showMessageDialog(frame, "A line needs two different vertices.", "Add Line", JOptionPane.ERROR_MESSAGE);
            return;
        }

        LineEdge edge = new LineEdge(aIdx, bIdx);
        if(lineEdges.contains(edge)){
            JOptionPane.showMessageDialog(frame, "This line already exists.", "Add Line", JOptionPane.INFORMATION_MESSAGE);
            return;
        }

        lineEdges.add(edge);
        imagePanel.setOverlayPoints(vertexPoints);
        imagePanel.setLines(lineEdges);
    }

    private void showAddPolygonDialog(JFrame frame){
        JTextField vertexCountField = new JTextField(8);
        JTextField vertexRefsField = new JTextField(40);
        JTextField lineCountField = new JTextField(8);
        JTextField lineRefsField = new JTextField(40);

        JPanel p = new JPanel(new GridLayout(0, 1, 8, 8));
        p.add(new JLabel("Use either vertices OR lines."));
        p.add(new JLabel("Vertex count:"));
        p.add(vertexCountField);
        p.add(new JLabel("Vertices (indices or coordinates; separate by semicolon)."));
        p.add(new JLabel("Example: 0; 3; 10:20; 35,40"));
        p.add(vertexRefsField);
        p.add(new JLabel("Line count:"));
        p.add(lineCountField);
        p.add(new JLabel("Line indices (separated by comma/semicolon)."));
        p.add(lineRefsField);

        int ret = JOptionPane.showConfirmDialog(frame, p, "Add Polygon", JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE);
        if(ret != JOptionPane.OK_OPTION) return;

        List<Integer> polygonVertices;
        if(!vertexRefsField.getText().trim().isEmpty()){
            Integer expected = parseIntegerField(vertexCountField.getText());
            polygonVertices = parseVertexReferenceList(vertexRefsField.getText(), true);
            if(polygonVertices == null || polygonVertices.size() < 3){
                JOptionPane.showMessageDialog(frame, "Please provide at least 3 valid vertex references.", "Add Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
            if(expected != null && expected != polygonVertices.size()){
                JOptionPane.showMessageDialog(frame, "Vertex count does not match number of parsed vertex references.", "Add Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
        }else if(!lineRefsField.getText().trim().isEmpty()){
            Integer expected = parseIntegerField(lineCountField.getText());
            List<Integer> lineIndices = parseIndexList(lineRefsField.getText());
            if(lineIndices == null || lineIndices.size() < 3){
                JOptionPane.showMessageDialog(frame, "Please provide at least 3 valid line indices.", "Add Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
            if(expected != null && expected != lineIndices.size()){
                JOptionPane.showMessageDialog(frame, "Line count does not match number of parsed line indices.", "Add Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
            polygonVertices = buildPolygonFromLines(lineIndices);
            if(polygonVertices == null || polygonVertices.size() < 3){
                JOptionPane.showMessageDialog(frame, "The specified lines must form one closed polygonal cycle.", "Add Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
        }else{
            JOptionPane.showMessageDialog(frame, "Provide vertex references or line indices.", "Add Polygon", JOptionPane.ERROR_MESSAGE);
            return;
        }

        if(addPolygonIfAbsent(polygonVertices)){
            imagePanel.setOverlayPoints(vertexPoints);
            imagePanel.setLines(lineEdges);
            imagePanel.setPolygons(polygons);
        }else{
            JOptionPane.showMessageDialog(frame, "This polygon already exists.", "Add Polygon", JOptionPane.INFORMATION_MESSAGE);
        }
    }

    private void showDeleteVertexDialog(JFrame frame){
        JTextField indexField = new JTextField(8);
        JTextField xField = new JTextField(8);
        JTextField yField = new JTextField(8);

        JPanel p = new JPanel(new GridLayout(0, 2, 8, 8));
        p.add(new JLabel("Vertex index:"));
        p.add(indexField);
        p.add(new JLabel("OR vertex X:"));
        p.add(xField);
        p.add(new JLabel("vertex Y:"));
        p.add(yField);

        int ret = JOptionPane.showConfirmDialog(frame, p, "Delete Vertex", JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE);
        if(ret != JOptionPane.OK_OPTION) return;

        Integer idx = null;
        Integer zeroBased = parseIntegerField(indexField.getText());
        if(zeroBased != null){
            int candidate = zeroBased;
            if(candidate < 0 || candidate >= vertexPoints.size()){
                JOptionPane.showMessageDialog(frame, "Vertex index is out of range.", "Delete Vertex", JOptionPane.ERROR_MESSAGE);
                return;
            }
            idx = candidate;
        }else{
            Integer x = parseIntegerField(xField.getText());
            Integer y = parseIntegerField(yField.getText());
            if(x == null || y == null){
                JOptionPane.showMessageDialog(frame, "Please provide either a valid index, or both X and Y coordinates.", "Delete Vertex", JOptionPane.ERROR_MESSAGE);
                return;
            }
            for(int i = 0; i < vertexPoints.size(); i++){
                Point vp = vertexPoints.get(i);
                if(vp.x == x && vp.y == y){
                    idx = i;
                    break;
                }
            }
            if(idx == null){
                JOptionPane.showMessageDialog(frame, "No vertex found at the specified coordinates.", "Delete Vertex", JOptionPane.INFORMATION_MESSAGE);
                return;
            }
        }

        deleteVertexAndUpdateTopology(idx);
        imagePanel.setOverlayPoints(vertexPoints);
        imagePanel.setLines(lineEdges);
        imagePanel.setPolygons(polygons);
        updateInfoPanel();
    }

    private void showDeleteLineDialog(JFrame frame){
        JTextField aField = new JTextField(20);
        JTextField bField = new JTextField(20);

        JPanel p = new JPanel(new GridLayout(0, 1, 8, 8));
        p.add(new JLabel("Vertex A (index or position x:y or x,y):"));
        p.add(aField);
        p.add(new JLabel("Vertex B (index or position x:y or x,y):"));
        p.add(bField);

        int ret = JOptionPane.showConfirmDialog(frame, p, "Delete Line", JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE);
        if(ret != JOptionPane.OK_OPTION) return;

        Integer aIdx = resolveVertexReference(aField.getText(), false);
        Integer bIdx = resolveVertexReference(bField.getText(), false);
        if(aIdx == null || bIdx == null){
            JOptionPane.showMessageDialog(frame, "Invalid vertex reference. Use an existing index (0-based) or existing coordinates (x:y / x,y).", "Delete Line", JOptionPane.ERROR_MESSAGE);
            return;
        }
        if(aIdx.equals(bIdx)){
            JOptionPane.showMessageDialog(frame, "A line requires two different vertices.", "Delete Line", JOptionPane.ERROR_MESSAGE);
            return;
        }

        LineEdge target = new LineEdge(aIdx, bIdx);
        int removeIdx = -1;
        for(int i = 0; i < lineEdges.size(); i++){
            if(target.equals(lineEdges.get(i))){
                removeIdx = i;
                break;
            }
        }
        if(removeIdx < 0){
            JOptionPane.showMessageDialog(frame, "The specified line was not found.", "Delete Line", JOptionPane.INFORMATION_MESSAGE);
            return;
        }

        LineEdge removed = lineEdges.remove(removeIdx);
        deletePolygonsContainingEdge(removed.v1, removed.v2);
        imagePanel.setLines(lineEdges);
        imagePanel.setPolygons(polygons);
        updateInfoPanel();
    }

    private void showDeletePolygonDialog(JFrame frame){
        JTextField polygonIndexField = new JTextField(8);
        JTextField vertexCountField = new JTextField(8);
        JTextField vertexRefsField = new JTextField(40);
        JTextField lineCountField = new JTextField(8);
        JTextField lineRefsField = new JTextField(40);

        JPanel p = new JPanel(new GridLayout(0, 1, 8, 8));
        p.add(new JLabel("Delete polygon by index OR by the same references used to create it."));
        p.add(new JLabel("Polygon index:"));
        p.add(polygonIndexField);
        p.add(new JLabel("Vertex count:"));
        p.add(vertexCountField);
        p.add(new JLabel("Vertices (indices or coordinates; separate by semicolon)."));
        p.add(new JLabel("Example: 0; 3; 10:20; 35,40"));
        p.add(vertexRefsField);
        p.add(new JLabel("Line count:"));
        p.add(lineCountField);
        p.add(new JLabel("Line indices (separated by comma/semicolon)."));
        p.add(lineRefsField);

        int ret = JOptionPane.showConfirmDialog(frame, p, "Delete Polygon", JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE);
        if(ret != JOptionPane.OK_OPTION) return;

        Integer polygonZeroBased = parseIntegerField(polygonIndexField.getText());
        if(polygonZeroBased != null){
            int polygonIdx = polygonZeroBased;
            if(polygonIdx < 0 || polygonIdx >= polygons.size()){
                JOptionPane.showMessageDialog(frame, "Polygon index is out of range.", "Delete Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
            polygons.remove(polygonIdx);
            imagePanel.setPolygons(polygons);
            updateInfoPanel();
            return;
        }

        List<Integer> polygonVertices;
        if(!vertexRefsField.getText().trim().isEmpty()){
            Integer expected = parseIntegerField(vertexCountField.getText());
            polygonVertices = parseVertexReferenceList(vertexRefsField.getText(), false);
            if(polygonVertices == null || polygonVertices.size() < 3){
                JOptionPane.showMessageDialog(frame, "Please provide at least 3 valid existing vertex references.", "Delete Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
            if(expected != null && expected != polygonVertices.size()){
                JOptionPane.showMessageDialog(frame, "Vertex count does not match number of parsed vertex references.", "Delete Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
        }else if(!lineRefsField.getText().trim().isEmpty()){
            Integer expected = parseIntegerField(lineCountField.getText());
            List<Integer> lineIndices = parseIndexList(lineRefsField.getText());
            if(lineIndices == null || lineIndices.size() < 3){
                JOptionPane.showMessageDialog(frame, "Please provide at least 3 valid line indices.", "Delete Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
            if(expected != null && expected != lineIndices.size()){
                JOptionPane.showMessageDialog(frame, "Line count does not match number of parsed line indices.", "Delete Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
            polygonVertices = buildPolygonFromLines(lineIndices);
            if(polygonVertices == null || polygonVertices.size() < 3){
                JOptionPane.showMessageDialog(frame, "The specified lines must form one closed polygonal cycle.", "Delete Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
        }else{
            JOptionPane.showMessageDialog(frame, "Provide a polygon index, vertex references, or line indices.", "Delete Polygon", JOptionPane.ERROR_MESSAGE);
            return;
        }

        int polygonIdx = findPolygonIndexByCycle(polygonVertices);
        if(polygonIdx < 0){
            JOptionPane.showMessageDialog(frame, "No matching polygon was found.", "Delete Polygon", JOptionPane.INFORMATION_MESSAGE);
            return;
        }

        polygons.remove(polygonIdx);
        imagePanel.setPolygons(polygons);
        updateInfoPanel();
    }

    private void showFindVertexDialog(JFrame frame){
        JTextField indexField = new JTextField(8);
        JTextField xField = new JTextField(8);
        JTextField yField = new JTextField(8);

        JPanel p = new JPanel(new GridLayout(0, 2, 8, 8));
        p.add(new JLabel("Vertex index:"));
        p.add(indexField);
        p.add(new JLabel("OR vertex X:"));
        p.add(xField);
        p.add(new JLabel("vertex Y:"));
        p.add(yField);

        int ret = JOptionPane.showConfirmDialog(frame, p, "Find Vertex", JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE);
        if(ret != JOptionPane.OK_OPTION) return;

        Integer idx = null;
        Integer zeroBased = parseIntegerField(indexField.getText());
        if(zeroBased != null){
            int candidate = zeroBased;
            if(candidate < 0 || candidate >= vertexPoints.size()){
                JOptionPane.showMessageDialog(frame, "Vertex index is out of range.", "Find Vertex", JOptionPane.ERROR_MESSAGE);
                return;
            }
            idx = candidate;
        }else{
            Integer x = parseIntegerField(xField.getText());
            Integer y = parseIntegerField(yField.getText());
            if(x == null || y == null){
                JOptionPane.showMessageDialog(frame, "Please provide either a valid index, or both X and Y coordinates.", "Find Vertex", JOptionPane.ERROR_MESSAGE);
                return;
            }
            for(int i = 0; i < vertexPoints.size(); i++){
                Point vp = vertexPoints.get(i);
                if(vp.x == x && vp.y == y){
                    idx = i;
                    break;
                }
            }
            if(idx == null){
                JOptionPane.showMessageDialog(frame, "No vertex found at the specified coordinates.", "Find Vertex", JOptionPane.INFORMATION_MESSAGE);
                return;
            }
        }

        imagePanel.selectVertex(idx);
    }

    private void showFindLineDialog(JFrame frame){
        JTextField aField = new JTextField(20);
        JTextField bField = new JTextField(20);

        JPanel p = new JPanel(new GridLayout(0, 1, 8, 8));
        p.add(new JLabel("Vertex A (index or position x:y or x,y):"));
        p.add(aField);
        p.add(new JLabel("Vertex B (index or position x:y or x,y):"));
        p.add(bField);

        int ret = JOptionPane.showConfirmDialog(frame, p, "Find Line", JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE);
        if(ret != JOptionPane.OK_OPTION) return;

        Integer aIdx = resolveVertexReference(aField.getText(), false);
        Integer bIdx = resolveVertexReference(bField.getText(), false);
        if(aIdx == null || bIdx == null){
            JOptionPane.showMessageDialog(frame, "Invalid vertex reference. Use an existing index or existing coordinates (x:y / x,y).", "Find Line", JOptionPane.ERROR_MESSAGE);
            return;
        }
        if(aIdx.equals(bIdx)){
            JOptionPane.showMessageDialog(frame, "A line requires two different vertices.", "Find Line", JOptionPane.ERROR_MESSAGE);
            return;
        }

        LineEdge target = new LineEdge(aIdx, bIdx);
        int lineIdx = -1;
        for(int i = 0; i < lineEdges.size(); i++){
            if(target.equals(lineEdges.get(i))){
                lineIdx = i;
                break;
            }
        }
        if(lineIdx < 0){
            JOptionPane.showMessageDialog(frame, "The specified line was not found.", "Find Line", JOptionPane.INFORMATION_MESSAGE);
            return;
        }

        imagePanel.selectLine(lineIdx);
    }

    private void showFindPolygonDialog(JFrame frame){
        JTextField polygonIndexField = new JTextField(8);
        JTextField vertexCountField = new JTextField(8);
        JTextField vertexRefsField = new JTextField(40);
        JTextField lineCountField = new JTextField(8);
        JTextField lineRefsField = new JTextField(40);

        JPanel p = new JPanel(new GridLayout(0, 1, 8, 8));
        p.add(new JLabel("Find polygon by index OR by the same references used to create it."));
        p.add(new JLabel("Polygon index:"));
        p.add(polygonIndexField);
        p.add(new JLabel("Vertex count:"));
        p.add(vertexCountField);
        p.add(new JLabel("Vertices (indices or coordinates; separate by semicolon)."));
        p.add(new JLabel("Example: 0; 3; 10:20; 35,40"));
        p.add(vertexRefsField);
        p.add(new JLabel("Line count:"));
        p.add(lineCountField);
        p.add(new JLabel("Line indices (separated by comma/semicolon)."));
        p.add(lineRefsField);

        int ret = JOptionPane.showConfirmDialog(frame, p, "Find Polygon", JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE);
        if(ret != JOptionPane.OK_OPTION) return;

        Integer polygonZeroBased = parseIntegerField(polygonIndexField.getText());
        if(polygonZeroBased != null){
            int polygonIdx = polygonZeroBased;
            if(polygonIdx < 0 || polygonIdx >= polygons.size()){
                JOptionPane.showMessageDialog(frame, "Polygon index is out of range.", "Find Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
            imagePanel.selectPolygon(polygonIdx);
            return;
        }

        List<Integer> polygonVertices;
        if(!vertexRefsField.getText().trim().isEmpty()){
            Integer expected = parseIntegerField(vertexCountField.getText());
            polygonVertices = parseVertexReferenceList(vertexRefsField.getText(), false);
            if(polygonVertices == null || polygonVertices.size() < 3){
                JOptionPane.showMessageDialog(frame, "Please provide at least 3 valid existing vertex references.", "Find Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
            if(expected != null && expected != polygonVertices.size()){
                JOptionPane.showMessageDialog(frame, "Vertex count does not match number of parsed vertex references.", "Find Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
        }else if(!lineRefsField.getText().trim().isEmpty()){
            Integer expected = parseIntegerField(lineCountField.getText());
            List<Integer> lineIndices = parseIndexList(lineRefsField.getText());
            if(lineIndices == null || lineIndices.size() < 3){
                JOptionPane.showMessageDialog(frame, "Please provide at least 3 valid line indices.", "Find Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
            if(expected != null && expected != lineIndices.size()){
                JOptionPane.showMessageDialog(frame, "Line count does not match number of parsed line indices.", "Find Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
            polygonVertices = buildPolygonFromLines(lineIndices);
            if(polygonVertices == null || polygonVertices.size() < 3){
                JOptionPane.showMessageDialog(frame, "The specified lines must form one closed polygonal cycle.", "Find Polygon", JOptionPane.ERROR_MESSAGE);
                return;
            }
        }else{
            JOptionPane.showMessageDialog(frame, "Provide a polygon index, vertex references, or line indices.", "Find Polygon", JOptionPane.ERROR_MESSAGE);
            return;
        }

        int polygonIdx = findPolygonIndexByCycle(polygonVertices);
        if(polygonIdx < 0){
            JOptionPane.showMessageDialog(frame, "No matching polygon was found.", "Find Polygon", JOptionPane.INFORMATION_MESSAGE);
            return;
        }

        imagePanel.selectPolygon(polygonIdx);
    }

    private Integer parseIntegerField(String text){
        String t = text == null ? "" : text.trim();
        if(t.isEmpty()) return null;
        try{
            return Integer.parseInt(t);
        }catch(NumberFormatException ex){
            return null;
        }
    }

    private Integer resolveVertexReference(String ref, boolean createIfPosition){
        if(ref == null) return null;
        String t = ref.trim();
        if(t.isEmpty()) return null;

        Integer idxDirect = parseIntegerField(t);
        if(idxDirect != null){
            int idx = idxDirect;
            if(idx < 0 || idx >= vertexPoints.size()) return null;
            return idx;
        }

        String normalized = t.replace("(", "").replace(")", "").trim();
        String[] parts = normalized.contains(":") ? normalized.split("\\s*:\\s*") : normalized.split("\\s*,\\s*");
        if(parts.length != 2) return null;
        Integer x = parseIntegerField(parts[0]);
        Integer y = parseIntegerField(parts[1]);
        if(x == null || y == null) return null;

        for(int i = 0; i < vertexPoints.size(); i++){
            Point p = vertexPoints.get(i);
            if(p.x == x && p.y == y) return i;
        }

        if(!createIfPosition) return null;

        vertexPoints.add(new Point(x, y));
        return vertexPoints.size() - 1;
    }

    private List<Integer> parseVertexReferenceList(String text, boolean createIfPosition){
        if(text == null || text.trim().isEmpty()) return Collections.emptyList();
        String[] tokens = text.split("\\s*[;\\n]+\\s*|\\s{2,}");
        ArrayList<Integer> out = new ArrayList<>();
        for(String token : tokens){
            String t = token.trim();
            if(t.isEmpty()) continue;
            Integer idx = resolveVertexReference(t, createIfPosition);
            if(idx == null) return null;
            out.add(idx);
        }
        return out;
    }

    private List<Integer> parseIndexList(String text){
        if(text == null || text.trim().isEmpty()) return Collections.emptyList();
        String[] tokens = text.split("\\s*[,;\\n]\\s*|\\s+");
        ArrayList<Integer> out = new ArrayList<>();
        for(String token : tokens){
            if(token.trim().isEmpty()) continue;
            Integer idxRaw = parseIntegerField(token);
            if(idxRaw == null) return null;
            int idx = idxRaw;
            if(idx < 0 || idx >= lineEdges.size()) return null;
            out.add(idx);
        }
        return out;
    }

    private List<Integer> buildPolygonFromLines(List<Integer> lineIndices){
        HashMap<Integer, ArrayList<Integer>> adjacency = new HashMap<>();
        HashSet<Long> seenEdges = new HashSet<>();

        for(int idx : lineIndices){
            LineEdge edge = lineEdges.get(idx);
            long key = edgeKey(edge.v1, edge.v2);
            if(!seenEdges.add(key)) return null;

            adjacency.computeIfAbsent(edge.v1, k -> new ArrayList<>()).add(edge.v2);
            adjacency.computeIfAbsent(edge.v2, k -> new ArrayList<>()).add(edge.v1);
        }

        if(adjacency.size() < 3) return null;
        for(ArrayList<Integer> nb : adjacency.values()){
            if(nb.size() != 2) return null;
        }

        int start = Integer.MAX_VALUE;
        for(int v : adjacency.keySet()) start = Math.min(start, v);

        ArrayList<Integer> order = new ArrayList<>();
        order.add(start);
        int prev = start;
        int cur = adjacency.get(start).get(0);

        while(cur != start){
            if(order.contains(cur)) return null;
            order.add(cur);
            ArrayList<Integer> nbs = adjacency.get(cur);
            if(nbs == null || nbs.size() != 2) return null;
            int next = (nbs.get(0) != prev) ? nbs.get(0) : nbs.get(1);
            prev = cur;
            cur = next;
            if(order.size() > adjacency.size() + 1) return null;
        }

        if(order.size() != adjacency.size()) return null;
        if(signedArea(order) < 0) Collections.reverse(order);
        return order;
    }

    private double signedArea(List<Integer> poly){
        double s = 0.0;
        int m = poly.size();
        for(int i = 0; i < m; i++){
            Point p = vertexPoints.get(poly.get(i));
            Point q = vertexPoints.get(poly.get((i + 1) % m));
            s += (p.x + 0.5) * (q.y + 0.5) - (q.x + 0.5) * (p.y + 0.5);
        }
        return 0.5 * s;
    }

    private boolean addPolygonIfAbsent(List<Integer> poly){
        if(poly == null || poly.size() < 3) return false;
        String key = polygonKey(poly);
        for(List<Integer> existing : polygons){
            if(existing != null && existing.size() >= 3 && polygonKey(existing).equals(key)) return false;
        }
        polygons.add(new ArrayList<>(poly));
        return true;
    }

    private void deleteAllVertices(){
        vertexPoints = new ArrayList<>();
        lineEdges = new ArrayList<>();
        polygons = new ArrayList<>();
        imagePanel.setOverlayPoints(vertexPoints);
        imagePanel.setLines(lineEdges);
        imagePanel.setPolygons(polygons);
        updateInfoPanel();
    }

    private void deleteAllLines(){
        lineEdges = new ArrayList<>();
        polygons = new ArrayList<>();
        imagePanel.setLines(lineEdges);
        imagePanel.setPolygons(polygons);
        updateInfoPanel();
    }

    private void deleteAllPolygons(){
        polygons = new ArrayList<>();
        imagePanel.setPolygons(polygons);
        updateInfoPanel();
    }

    private void resetWholeWindow(JFrame frame){
        int ans = JOptionPane.showConfirmDialog(
                frame,
                "Delete all data and reset this window to a fresh state?",
                "Delete all and reset",
                JOptionPane.OK_CANCEL_OPTION,
                JOptionPane.WARNING_MESSAGE
        );
        if(ans != JOptionPane.OK_OPTION) return;

        currentImp = null;
        currentBI = null;
        currentGeometrySourceFileName = "unknown.json";

        vertexPoints = new ArrayList<>();
        lineEdges = new ArrayList<>();
        polygons = new ArrayList<>();

        neighborPreviewCache.clear();
        clearNeighborPairGeometryCache();
        clearNeighborPairEstimationCache();
        clearRealDivisionPairCache();
        clearComparisonPairCaches();

        imagePanel.resetSelectionsAndPicks();
        imagePanel.setImage(null);
        imagePanel.setBackgroundImage(null);
        imagePanel.setOverlayPoints(vertexPoints);
        imagePanel.setLines(lineEdges);
        imagePanel.setPolygons(polygons);
        imagePanel.updateSelectionInfoLabels();

        if(infoMousePositionValue != null){
            infoMousePositionValue.setText("-");
        }

        scrollPane.getViewport().setViewPosition(new Point(0, 0));

        updateInfoPanel();
    }


    private void deleteVertexAndUpdateTopology(int delIdx){
        if(delIdx < 0 || delIdx >= vertexPoints.size()) return;

        vertexPoints.remove(delIdx);

        ArrayList<LineEdge> newLines = new ArrayList<>();
        LinkedHashSet<LineEdge> unique = new LinkedHashSet<>();
        for(LineEdge e : lineEdges){
            int a = e.v1;
            int b = e.v2;
            if(a == delIdx || b == delIdx) continue;
            if(a > delIdx) a--;
            if(b > delIdx) b--;
            if(a == b) continue;
            unique.add(new LineEdge(a, b));
        }
        newLines.addAll(unique);
        lineEdges = newLines;

        ArrayList<List<Integer>> keptPolygons = new ArrayList<>();
        for(List<Integer> poly : polygons){
            if(poly == null || poly.isEmpty()) continue;
            boolean containsDeleted = false;
            ArrayList<Integer> mapped = new ArrayList<>(poly.size());
            for(int v : poly){
                if(v == delIdx){
                    containsDeleted = true;
                    break;
                }
                if(v < 0 || v >= vertexPoints.size() + 1){
                    containsDeleted = true;
                    break;
                }
                mapped.add(v > delIdx ? v - 1 : v);
            }
            if(containsDeleted) continue;
            if(mapped.size() < 3) continue;
            if(new HashSet<>(mapped).size() < 3) continue;
            keptPolygons.add(mapped);
        }
        polygons = keptPolygons;
    }

    private void deletePolygonsContainingEdge(int a, int b){
        long target = edgeKey(a, b);
        for(int i = polygons.size() - 1; i >= 0; i--){
            List<Integer> poly = polygons.get(i);
            if(poly == null || poly.size() < 2) continue;
            int m = poly.size();
            for(int k = 0; k < m; k++){
                int u = poly.get(k);
                int v = poly.get((k + 1) % m);
                if(edgeKey(u, v) == target){
                    polygons.remove(i);
                    break;
                }
            }
        }
    }

    private int findPolygonIndexByCycle(List<Integer> targetPolygon){
        String targetKey = normalizedPolygonCycleKey(targetPolygon);
        if(targetKey.isEmpty()) return -1;
        for(int i = 0; i < polygons.size(); i++){
            if(targetKey.equals(normalizedPolygonCycleKey(polygons.get(i)))) return i;
        }
        return -1;
    }

    private String normalizedPolygonCycleKey(List<Integer> ids){
        if(ids == null || ids.size() < 3) return "";
        ArrayList<Integer> base = new ArrayList<>(ids);
        if(base.size() > 1 && base.get(0).equals(base.get(base.size() - 1))){
            base.remove(base.size() - 1);
        }
        if(base.size() < 3) return "";

        ArrayList<Integer> reversed = new ArrayList<>(base);
        Collections.reverse(reversed);
        String forward = minimalRotationKey(base);
        String backward = minimalRotationKey(reversed);
        return (forward.compareTo(backward) <= 0) ? forward : backward;
    }

    private String minimalRotationKey(List<Integer> ids){
        int m = ids.size();
        String best = null;
        for(int start = 0; start < m; start++){
            StringBuilder sb = new StringBuilder(m * 4);
            for(int k = 0; k < m; k++){
                sb.append(ids.get((start + k) % m)).append(':');
            }
            String key = sb.toString();
            if(best == null || key.compareTo(best) < 0) best = key;
        }
        return best == null ? "" : best;
    }

    private void openImage(JFrame frame){

        OpenDialog od=new OpenDialog("Open image",null);

        String dir=od.getDirectory();
        String name=od.getFileName();

        if(name==null) return;

        ImagePlus imp=IJ.openImage(dir+name);

        if(imp==null){
            IJ.error("Cannot open image");
            return;
        }

        currentImp=imp;
        currentBI=imp.getProcessor().getBufferedImage();
        currentGeometrySourceFileName = name;

        vertexPoints.clear();
        lineEdges.clear();
        polygons.clear();

        imagePanel.setImage(currentBI);
        imagePanel.setOverlayPoints(vertexPoints);
        imagePanel.setLines(lineEdges);
        imagePanel.setPolygons(polygons);

        frame.setTitle("Cell Division Inference — "+name);
        updateInfoPanel();
        adjustWindowToContent();
    }

    private void createCanvas(JFrame frame){

        JSpinner widthSpinner = new JSpinner(new SpinnerNumberModel(512, 1, 100000, 1));
        JSpinner heightSpinner = new JSpinner(new SpinnerNumberModel(512, 1, 100000, 1));

        Dimension spinnerSize = new Dimension(120, widthSpinner.getPreferredSize().height);
        widthSpinner.setPreferredSize(spinnerSize);
        heightSpinner.setPreferredSize(spinnerSize);

        JPanel colorPreview = new JPanel();
        colorPreview.setPreferredSize(new Dimension(40, 20));
        colorPreview.setBorder(BorderFactory.createLineBorder(Color.DARK_GRAY));

        final Color[] selectedColor = new Color[]{Color.BLACK};
        colorPreview.setBackground(selectedColor[0]);

        JButton chooseColorButton = new JButton("Choose Color");
        chooseColorButton.addActionListener(e -> {
            Color chosen = JColorChooser.showDialog(frame, "Choose Background Color", selectedColor[0]);
            if(chosen != null){
                selectedColor[0] = chosen;
                colorPreview.setBackground(chosen);
            }
        });

        JPanel colorRow = new JPanel(new FlowLayout(FlowLayout.LEFT, 8, 0));
        colorRow.add(colorPreview);
        colorRow.add(chooseColorButton);

        JPanel form = new JPanel(new GridBagLayout());
        GridBagConstraints gbc = new GridBagConstraints();
        gbc.insets = new Insets(4, 4, 4, 4);
        gbc.anchor = GridBagConstraints.WEST;

        gbc.gridx = 0; gbc.gridy = 0;
        form.add(new JLabel("Width:"), gbc);
        gbc.gridx = 1;
        form.add(widthSpinner, gbc);

        gbc.gridx = 0; gbc.gridy = 1;
        form.add(new JLabel("Height:"), gbc);
        gbc.gridx = 1;
        form.add(heightSpinner, gbc);

        gbc.gridx = 0; gbc.gridy = 2;
        form.add(new JLabel("Color:"), gbc);
        gbc.gridx = 1;
        form.add(colorRow, gbc);

        int ret = JOptionPane.showConfirmDialog(
                frame,
                form,
                "Create Canvas",
                JOptionPane.OK_CANCEL_OPTION,
                JOptionPane.PLAIN_MESSAGE
        );

        if(ret != JOptionPane.OK_OPTION) return;

        int width = (Integer)widthSpinner.getValue();
        int height = (Integer)heightSpinner.getValue();

        currentImp = null;
        currentBI = new BufferedImage(width, height, BufferedImage.TYPE_INT_RGB);
        Graphics2D g2 = currentBI.createGraphics();
        g2.setColor(selectedColor[0]);
        g2.fillRect(0, 0, width, height);
        g2.dispose();

        currentGeometrySourceFileName = "canvas";

        vertexPoints.clear();
        lineEdges.clear();
        polygons.clear();

        imagePanel.setImage(currentBI);
        imagePanel.setOverlayPoints(vertexPoints);
        imagePanel.setLines(lineEdges);
        imagePanel.setPolygons(polygons);
        imagePanel.resetSelectionsAndPicks();

        frame.setTitle("Cell Division Inference — Canvas " + width + "x" + height);
        updateInfoPanel();
        adjustWindowToContent();
    }

    private void openBackground(JFrame frame){

        OpenDialog od = new OpenDialog("Open background image", null);

        String dir = od.getDirectory();
        String name = od.getFileName();
        if(name == null) return;

        ImagePlus imp = IJ.openImage(dir + name);
        if(imp == null || imp.getProcessor() == null){
            IJ.error("Cannot open background image");
            return;
        }

        BufferedImage bg = imp.getProcessor().getBufferedImage();
        if(bg == null){
            IJ.error("Cannot decode background image");
            return;
        }

        imagePanel.setBackgroundImage(bg);
        updateInfoPanel();
        adjustWindowToContent();
    }

    private void skeletonize2D(JFrame frame){

        if(currentImp==null) return;

        new Thread(() -> {

            ImagePlus work=currentImp.duplicate();

            IJ.run(work,"Make Binary","");
            IJ.run(work,"Skeletonize","");

            currentImp=work;
            currentBI=work.getProcessor().getBufferedImage();

            SwingUtilities.invokeLater(() -> {
                imagePanel.setImage(currentBI);
                updateInfoPanel();
                adjustWindowToContent();
            });

        }).start();
    }

    // =========================
    // Import & Export (Qt JSON)
    // =========================

    private static class ImportResult{
        ArrayList<Point> vertices = new ArrayList<>();
        ArrayList<LineEdge> lines = new ArrayList<>();
        ArrayList<List<Integer>> polygons = new ArrayList<>();
        ArrayList<Integer> polygonIds = new ArrayList<>();
        ArrayList<int[]> neighborPairs = new ArrayList<>();
        ArrayList<String> warnings = new ArrayList<>();
        int maxX = 0;
        int maxY = 0;
    }

    private void importData(JFrame frame){

        OpenDialog od = new OpenDialog("Import topology JSON", null);
        String dir = od.getDirectory();
        String name = od.getFileName();
        if(name == null) return;

        File file = new File(dir, name);
        currentGeometrySourceFileName = file.getName();

        ImportResult result;
        try{
            result = parseTopologyJson(file);
        }catch(Exception ex){
            IJ.error("Import failed", ex.getMessage());
            return;
        }

        if(result.vertices == null || result.vertices.isEmpty()){
            IJ.error("Import failed", "No vertices found in the JSON file.");
            return;
        }

        // If no image/background loaded, create a blank canvas so paintComponent can draw something
        if(currentBI == null && !imagePanel.hasBackgroundImage()){
            int w = Math.max(64, result.maxX + 50);
            int h = Math.max(64, result.maxY + 50);

            currentBI = new BufferedImage(w, h, BufferedImage.TYPE_INT_RGB);
            Graphics2D g2 = currentBI.createGraphics();
            g2.setColor(Color.white);
            g2.fillRect(0, 0, w, h);
            g2.dispose();

            imagePanel.setImage(currentBI);
            updateInfoPanel();
            adjustWindowToContent();
        }else{
            int w = (currentBI != null) ? currentBI.getWidth() : imagePanel.getCanvasWidth();
            int h = (currentBI != null) ? currentBI.getHeight() : imagePanel.getCanvasHeight();
            if(result.maxX >= w || result.maxY >= h){
                IJ.log("[Import] Warning: Imported vertices exceed current image size ("+w+"x"+h+").");
            }
        }

        // Replace current topology
        vertexPoints = result.vertices;
        lineEdges = result.lines;
        polygons = result.polygons;

        // If neighborPairs exist, setPolygons() will sort polygons, so we must remap pairs after sorting
        ArrayList<String> prePolyKeys = new ArrayList<>();
        boolean hasNeighborPairs = (result.neighborPairs != null && !result.neighborPairs.isEmpty());
        if(hasNeighborPairs){
            for(List<Integer> poly : polygons){
                prePolyKeys.add(polygonKey(poly));
            }
        }

        imagePanel.setOverlayPoints(vertexPoints);
        imagePanel.setLines(lineEdges);
        imagePanel.setPolygonsKeepOrder(polygons);
        imagePanel.setPolygonDisplayIds(result.polygonIds);

        // Remap neighbor pairs if needed
        if(hasNeighborPairs){
            HashMap<String, Integer> keyToNewIndex = new HashMap<>();
            for(int i=0; i<polygons.size(); i++){
                keyToNewIndex.put(polygonKey(polygons.get(i)), i);
            }

            ArrayList<int[]> remapped = new ArrayList<>();
            HashSet<Long> seen = new HashSet<>();

            for(int[] pr : result.neighborPairs){
                if(pr == null || pr.length < 2) continue;
                int aOld = pr[0], bOld = pr[1];
                if(aOld < 0 || bOld < 0 || aOld >= prePolyKeys.size() || bOld >= prePolyKeys.size()) continue;

                Integer aNew = keyToNewIndex.get(prePolyKeys.get(aOld));
                Integer bNew = keyToNewIndex.get(prePolyKeys.get(bOld));
                if(aNew == null || bNew == null || aNew.equals(bNew)) continue;

                int i = Math.min(aNew, bNew);
                int j = Math.max(aNew, bNew);
                long key = (((long)i) << 32) ^ (long)j;
                if(seen.add(key)){
                    remapped.add(new int[]{i, j});
                }
            }
            imagePanel.setNeighborPairs(remapped);
        }else{
            imagePanel.setNeighborPairs(new ArrayList<>());
        }

        // Clear selections/picks (avoid stale indices)
        imagePanel.resetSelectionsAndPicks();

        // Report
        StringBuilder sb = new StringBuilder();
        sb.append("Imported topology from:\n").append(file.getName()).append("\n\n");
        sb.append("Vertices: ").append(vertexPoints.size()).append("\n");
        sb.append("Lines: ").append(lineEdges == null ? 0 : lineEdges.size()).append("\n");
        sb.append("Polygons: ").append(polygons == null ? 0 : polygons.size()).append("\n");

        if(result.warnings != null && !result.warnings.isEmpty()){
            sb.append("\nWarnings (first 10):\n");
            for(int i=0; i<Math.min(10, result.warnings.size()); i++){
                sb.append("• ").append(result.warnings.get(i)).append("\n");
            }
            if(result.warnings.size() > 10){
                sb.append("... (").append(result.warnings.size()-10).append(" more)\n");
            }
        }

        JOptionPane.showMessageDialog(frame, sb.toString(), "Import Data", JOptionPane.INFORMATION_MESSAGE);
    }

    private void importRealDivisionPairs(JFrame frame){
        if(polygons == null || polygons.isEmpty()){
            JOptionPane.showMessageDialog(frame,
                "No polygons loaded.\nImport topology (or detect polygons before importing real division pairs.",
                "Import Real Division Pairs",
                JOptionPane.WARNING_MESSAGE);
            return;
        }

        String defaultDirectory = OpenDialog.getDefaultDirectory();
        JFileChooser chooser = (defaultDirectory != null && !defaultDirectory.trim().isEmpty())
                ? new JFileChooser(defaultDirectory)
                : new JFileChooser();
        chooser.setDialogTitle("Select real division pairs CSV");

        int ret = chooser.showOpenDialog(frame);
        if(ret != JFileChooser.APPROVE_OPTION) return;

        File file = chooser.getSelectedFile();
        if(file == null || !file.exists()){
            JOptionPane.showMessageDialog(frame, "File not found.", "Import Real Division Pairs", JOptionPane.ERROR_MESSAGE);
            return;
        }
        
        File parentDir = file.getParentFile();
        if(parentDir != null){
            OpenDialog.setDefaultDirectory(parentDir.getAbsolutePath() + File.separator);
        }

        int polyN = polygons.size();
        int imported = 0;
        int skipped = 0;
        int outOfRange = 0;
        int badLine = 0;

        ArrayList<int[]> rawPairs = new ArrayList<>();
        ArrayList<Double> rawTimes = new ArrayList<>();

        try(BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8))){

                String line;
                boolean firstNonEmptySeen = false;

                while((line = br.readLine()) != null){
                    line = line.trim();
                    if(line.isEmpty()) continue;
                    if(line.startsWith("#")) continue;

                    // tolerate the first label line "real_division_pair"
                    if(!firstNonEmptySeen){
                        firstNonEmptySeen = true;
                        String lower = line.toLowerCase(Locale.ROOT);
                        if(lower.equals("real_division_pair") || lower.equals("real division pair")){
                            continue;
                        }
                        // also tolerate a CSV header line
                        if(lower.contains("first") || lower.contains("second") || lower.contains("cell") || lower.contains("file_name")){
                            // skip header
                            continue;
                        }
                    }

                    // Split by comma if present, otherwise whitespace
                    String[] tok = line.contains(",") ? line.split("\\s*,\\s*", -1) : line.split("\\s+");
                    for(int i = 0; i < tok.length; i++){
                        if(tok[i] == null) continue;
                        tok[i] = tok[i].trim();
                        if(tok[i].length() >= 2 && tok[i].startsWith("\"") && tok[i].endsWith("\"")){
                            tok[i] = tok[i].substring(1, tok[i].length() - 1).trim();
                        }
                    }

                    if(tok.length < 2){
                        badLine++;
                        continue;
                    }

                    // Supported formats:
                    // (A) id1,id2,time
                    // (B) fileName,pairIndex,firstPolygonId,secondPolygonId,observed_division,division_timing,...  (Qt geometry CSV)
                    // (C) fileName,id1,id2,time   (legacy file-name-first)
                    Integer id1 = null, id2 = null;
                    Double time = Double.NaN;

                    // ---- (B) Qt geometry CSV ----
                    if(tok.length >= 6){
                        Integer qId1 = tryParseInt(tok[2]);
                        Integer qId2 = tryParseInt(tok[3]);
                        Integer observed = tryParseInt(tok[4]);
                        Double qTime = tryParseDouble(tok[5]);

                        if(qId1 != null && qId2 != null && observed != null){
                            // only import true real-division rows
                            if(observed == 0){
                                continue;
                            }
                            id1 = qId1;
                            id2 = qId2;
                            if(qTime != null) time = qTime;
                        }
                    }

                    // ---- (A) simple 3-column: id1,id2,time ----
                    if(id1 == null || id2 == null){
                        Integer t0 = tryParseInt(tok[0]);
                        Integer t1 = tryParseInt(tok[1]);
                        if(t0 != null && t1 != null){
                            id1 = t0;
                            id2 = t1;
                            if(tok.length >= 3){
                                Double t2 = tryParseDouble(tok[2]);
                                if(t2 != null) time = t2;
                            }
                        }
                    }

                    // ---- (C) legacy file-name-first: fileName,id1,id2,time ----
                    if((id1 == null || id2 == null) && tok.length >= 3){
                        Integer tA = tryParseInt(tok[1]);
                        Integer tB = tryParseInt(tok[2]);
                        if(tA != null && tB != null){
                            id1 = tA;
                            id2 = tB;
                            if(tok.length >= 4){
                                Double t3 = tryParseDouble(tok[3]);
                                if(t3 != null) time = t3;
                            }
                        }
                    }

                    if(id1 == null || id2 == null || id1.equals(id2)){
                        badLine++;
                        continue;
                    }

                    rawPairs.add(new int[]{id1, id2});
                    rawTimes.add(time);
                }

            }catch(Exception ex){
                JOptionPane.showMessageDialog(frame,
                        "Failed to read CSV:\n" + ex.getMessage(),
                        "Import Real Division Pairs",
                        JOptionPane.ERROR_MESSAGE);
                return;
            }

            if(rawPairs.isEmpty()){
                JOptionPane.showMessageDialog(frame,
                        "No valid pairs found in the file.",
                        "Import Real Division Pairs",
                        JOptionPane.WARNING_MESSAGE);
                return;
            }

            // Heuristic 1-based conversion ONLY if needed:
            // If any ID is out of range for 0-based but becomes valid after -1, then shift all by -1.
            boolean needShift = false;
            for(int[] pr : rawPairs){
                int a = pr[0], b = pr[1];
                if(a < 0 || b < 0) continue;
                if(a >= polyN || b >= polyN){
                    if((a-1) >= 0 && (a-1) < polyN && (b-1) >= 0 && (b-1) < polyN){
                        needShift = true;
                        break;
                    }
                }
            }

            // Clear old real pairs
            clearRealDivisionPairCache();

            HashSet<Long> seen = new HashSet<>();

            for(int i=0; i<rawPairs.size(); i++){
                int a = rawPairs.get(i)[0];
                int b = rawPairs.get(i)[1];
                double time = rawTimes.get(i);

                if(needShift){
                    a -= 1;
                    b -= 1;
                }

                if(a < 0 || b < 0 || a == b){
                    skipped++;
                    continue;
                }
                if(a >= polyN || b >= polyN){
                    outOfRange++;
                    continue;
                }

                long k = pairKey(a, b);
                if(!seen.add(k)){
                    // duplicate
                    continue;
                }

                RealDivisionEntry e = new RealDivisionEntry();
                e.timing = time;
                realDivisionPairCache.put(k, e);
                imported++;
            }

            if(imagePanel != null) imagePanel.repaint();
            updateInfoPanel();

            JOptionPane.showMessageDialog(frame,
                    "Imported real division pairs: " + imported +
                            "\nSkipped (invalid): " + skipped +
                            "\nOut of range: " + outOfRange +
                            "\nBad lines: " + badLine +
                            (needShift ? "\n(Detected 1-based IDs → shifted by -1)" : ""),
                    "Import Real Division Pairs",
                    JOptionPane.INFORMATION_MESSAGE);
    }

    private static Integer tryParseInt(String s){
        if(s == null) return null;
        s = s.trim();
        if(s.isEmpty()) return null;
        try{
            // allow "37" or "37.0" accidentally
            if(s.contains(".")){
                double d = Double.parseDouble(s);
                int v = (int)Math.round(d);
                if(Math.abs(d - v) < 1e-9) return v;
                return null;
            }
            return Integer.parseInt(s);
        }catch(Exception e){
            return null;
        }
    }

    private static Double tryParseDouble(String s){
            if(s == null) return null;
            s = s.trim();
            if(s.isEmpty()) return null;
            try{
                return Double.parseDouble(s);
            }catch(Exception e){
                return null;
        }
    }

    private void exportData(JFrame frame){

        if(vertexPoints == null || vertexPoints.isEmpty()){
            JOptionPane.showMessageDialog(frame, "No vertices to export.", "Export Data", JOptionPane.WARNING_MESSAGE);
            return;
        }

        // ---- choose export content ----
        JRadioButton rbV  = new JRadioButton("Vertex only", true);
        JRadioButton rbVL = new JRadioButton("Vertex + Line");
        JRadioButton rbVLP= new JRadioButton("Vertex + Line + Polygon");

        // Optional: disable options that have no data (still lets user choose vertex-only)
        if(lineEdges == null || lineEdges.isEmpty()){
            rbVL.setEnabled(false);
            rbVLP.setEnabled(false);
        }else if(polygons == null || polygons.isEmpty()){
            rbVLP.setEnabled(false);
        }

        ButtonGroup bg = new ButtonGroup();
        bg.add(rbV); bg.add(rbVL); bg.add(rbVLP);

        JPanel p = new JPanel();
        p.setLayout(new BoxLayout(p, BoxLayout.Y_AXIS));
        p.add(new JLabel("Choose what to export:"));
        p.add(rbV);
        p.add(rbVL);
        p.add(rbVLP);

        int ok = JOptionPane.showConfirmDialog(frame, p, "Export Options", JOptionPane.OK_CANCEL_OPTION, JOptionPane.PLAIN_MESSAGE);
        if(ok != JOptionPane.OK_OPTION) return;

        boolean includeLines = rbVL.isSelected() || rbVLP.isSelected();
        boolean includePolygons = rbVLP.isSelected();

        // If user selected a disabled option via code change later, still be safe:
        if(includeLines && (lineEdges == null || lineEdges.isEmpty())){
            JOptionPane.showMessageDialog(frame, "No lines exist. Exporting vertices only.", "Export Data", JOptionPane.WARNING_MESSAGE);
            includeLines = false;
            includePolygons = false;
        }
        if(includePolygons && (polygons == null || polygons.isEmpty())){
            JOptionPane.showMessageDialog(frame, "No polygons exist. Exporting vertices + lines.", "Export Data", JOptionPane.WARNING_MESSAGE);
            includePolygons = false;
        }

        // ---- file dialog ----
        SaveDialog sd = new SaveDialog("Export topology JSON", "topology", ".json");
        String dir = sd.getDirectory();
        String name = sd.getFileName();
        if(name == null) return;

        if(!name.toLowerCase().endsWith(".json")) name = name + ".json";
        File file = new File(dir, name);

        try{
            writeTopologyJson(file, includeLines, includePolygons);
        }catch(Exception ex){
            IJ.error("Export failed", ex.getMessage());
            return;
        }

        JOptionPane.showMessageDialog(frame, "Exported:\n" + file.getAbsolutePath(), "Export Data", JOptionPane.INFORMATION_MESSAGE);
    }

    private ImportResult parseTopologyJson(File file) throws IOException{

        ImportResult result = new ImportResult();

        JsonObject root;
        try(Reader reader = new InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8)){
            JsonElement el = new JsonParser().parse(reader);
            if(el == null || !el.isJsonObject()){
                throw new IOException("Top-level JSON is not an object.");
            }
            root = el.getAsJsonObject();
        }catch(JsonParseException jpe){
            throw new IOException("Invalid JSON: " + jpe.getMessage(), jpe);
        }

        // -------- vertices (required) --------
        JsonArray vertexArray = root.getAsJsonArray("vertices");
        if(vertexArray == null || vertexArray.size() == 0){
            throw new IOException("No 'vertices' array found in JSON.");
        }

        class VRec{
            final int id;
            final int x;
            final int y;
            VRec(int id, int x, int y){ this.id=id; this.x=x; this.y=y; }
        }

        ArrayList<VRec> vlist = new ArrayList<>();
        for(JsonElement ve : vertexArray){
            if(ve == null || !ve.isJsonObject()){
                result.warnings.add("Skipped a non-object vertex entry.");
                continue;
            }
            JsonObject o = ve.getAsJsonObject();

            Integer id = readInt(o, "id");
            Double xD = readDouble(o, "x");
            Double yD = readDouble(o, "y");

            if(id == null || xD == null || yD == null){
                result.warnings.add("Skipped a vertex missing id/x/y.");
                continue;
            }

            int x = (int)Math.round(xD);
            int y = (int)Math.round(yD);

            vlist.add(new VRec(id, x, y));
            result.maxX = Math.max(result.maxX, x);
            result.maxY = Math.max(result.maxY, y);
        }

        if(vlist.isEmpty()){
            throw new IOException("No valid vertices could be parsed.");
        }

        HashMap<Integer, Integer> vIdToIndex = new HashMap<>();
        for(VRec vr : vlist){
            if(vIdToIndex.containsKey(vr.id)){
                result.warnings.add("Duplicate vertex id " + vr.id + " (using the last one for references).");
            }
            result.vertices.add(new Point(vr.x, vr.y));
            vIdToIndex.put(vr.id, result.vertices.size() - 1);
        }

        // -------- lines (optional) --------
        JsonArray lineArray = root.getAsJsonArray("lines");
        if(lineArray != null && lineArray.size() > 0){

            LinkedHashSet<LineEdge> set = new LinkedHashSet<>();

            for(JsonElement le : lineArray){
                if(le == null || !le.isJsonObject()){
                    result.warnings.add("Skipped a non-object line entry.");
                    continue;
                }
                JsonObject o = le.getAsJsonObject();
                if(!o.has("id") || !o.has("startVertexId") || !o.has("endVertexId")){
                    continue;
                }
                Integer sId = readInt(o, "startVertexId");
                Integer eId = readInt(o, "endVertexId");

                if(sId == null || eId == null){
                    continue;
                }

                Integer a = vIdToIndex.get(sId);
                Integer b = vIdToIndex.get(eId);

                if(a == null || b == null){
                    result.warnings.add("Line references missing vertices ("+sId+", "+eId+").");
                    continue;
                }

                if(a.equals(b)){
                    result.warnings.add("Skipped a degenerate line with identical endpoints (vertex id "+sId+").");
                    continue;
                }

                set.add(new LineEdge(a, b));
            }

            result.lines.addAll(set);
        }

        // -------- polygons (optional) --------
        JsonArray polyArray = root.getAsJsonArray("polygons");
        if(polyArray != null && polyArray.size() > 0){

            for(JsonElement pe : polyArray){
                if(pe == null || !pe.isJsonObject()){
                    result.warnings.add("Skipped a non-object polygon entry.");
                    continue;
                }
                JsonObject o = pe.getAsJsonObject();
                
                if(!o.has("id") || !o.has("vertexIds")){
                    continue;
                }

                Integer polyId = readInt(o, "id");
                if(polyId == null){
                    continue;
                }

                JsonArray vIds = o.getAsJsonArray("vertexIds");
                if(vIds == null || vIds.size() < 3){
                    result.warnings.add("Skipped polygon " + polyId + " (need ≥3 vertices).");
                    continue;
                }

                ArrayList<Integer> idxs = new ArrayList<>();
                boolean missing = false;

                for(JsonElement vidEl : vIds){
                    if(vidEl == null) { missing = true; break; }
                    int vId;
                    try{
                        vId = (int)Math.round(vidEl.getAsDouble());
                    }catch(Exception ex){
                        missing = true;
                        break;
                    }

                    Integer idx = vIdToIndex.get(vId);
                    if(idx == null){
                        result.warnings.add("Polygon references missing vertex " + vId + ".");
                        missing = true;
                        break;
                    }
                    idxs.add(idx);
                }

                if(missing) continue;

                result.polygons.add(idxs);
                result.polygonIds.add(polyId);
            }
        }

        // -------- neighborPairs (optional; Qt format) --------
        // (Your provided JSONs don't include this, but supported for future)
        // NOTE: This stores pairs as polygon LIST INDICES (before ImagePanel sorts them).
        JsonArray neighArray = root.getAsJsonArray("neighborPairs");
        if(neighArray != null && neighArray.size() > 0){
            HashMap<Integer, Integer> polyIdToIndex = new HashMap<>();
            for(int idx = 0; idx < result.polygonIds.size(); idx++){
                Integer id = result.polygonIds.get(idx);
                if(id == null) continue;
                if(polyIdToIndex.containsKey(id)){
                    result.warnings.add("Duplicate polygon id " + id + " (using the last one for neighborPairs).");
                }
                polyIdToIndex.put(id, idx);
            }

            HashSet<Long> seen = new HashSet<>();

            for(JsonElement ne : neighArray){
                if(ne == null || !ne.isJsonObject()){
                    result.warnings.add("Skipped a non-object neighborPairs entry.");
                    continue;
                }
                JsonObject o = ne.getAsJsonObject();
                Integer aId = readInt(o, "firstPolygonId");
                Integer bId = readInt(o, "secondPolygonId");
                if(aId == null || bId == null) continue;

                Integer aIdx = polyIdToIndex.get(aId);
                Integer bIdx = polyIdToIndex.get(bId);
                if(aIdx == null || bIdx == null){
                    result.warnings.add("Neighbor pair (" + aId + ", " + bId + ") references missing polygons.");
                    continue;
                }

                int a = aIdx;
                int b = bIdx;
                if(a < 0 || b < 0 || a >= result.polygons.size() || b >= result.polygons.size()) continue;
                if(a == b) continue;

                int i = Math.min(a, b);
                int j = Math.max(a, b);
                long key = (((long)i) << 32) ^ (long)j;

                if(seen.add(key)){
                    result.neighborPairs.add(new int[]{i, j});
                }
            }
        }

        return result;
    }

    private static Integer readInt(JsonObject obj, String key){
        if(obj == null || key == null) return null;
        if(!obj.has(key) || obj.get(key).isJsonNull()) return null;
        try{
            return (int)obj.get(key).getAsDouble();
        }catch(Exception ex){
            return null;
        }
    }

    private static Double readDouble(JsonObject obj, String key){
        if(obj == null || key == null) return null;
        if(!obj.has(key) || obj.get(key).isJsonNull()) return null;
        try{
            return obj.get(key).getAsDouble();
        }catch(Exception ex){
            return null;
        }
    }

    private void writeTopologyJson(File file, boolean includeLines, boolean includePolygons) throws IOException{

        Gson gson = new GsonBuilder().setPrettyPrinting().create();
        JsonObject root = new JsonObject();

        // vertices 
        JsonArray vArr = new JsonArray();
        for(int i=0; i<vertexPoints.size(); i++){
            Point p = vertexPoints.get(i);
            JsonObject o = new JsonObject();
            o.addProperty("id", i+1);
            o.addProperty("x", p.x);
            o.addProperty("y", p.y);
            vArr.add(o);
        }
        root.add("vertices", vArr);

        // lines (optional)
        if(includeLines && lineEdges != null && !lineEdges.isEmpty()){
            JsonArray lArr = new JsonArray();
            int lid = 1;
            for(LineEdge e : lineEdges){
                JsonObject o = new JsonObject();
                o.addProperty("id", lid++);
                o.addProperty("startVertexId", e.v1 + 1);
                o.addProperty("endVertexId", e.v2 + 1);
                lArr.add(o);
            }
            root.add("lines", lArr);
        }

        // polygons (optional)
        if(includePolygons && polygons != null && !polygons.isEmpty()){
            JsonArray pArr = new JsonArray();
            for(int pid=0; pid<polygons.size(); pid++){
                List<Integer> poly = polygons.get(pid);
                if(poly == null || poly.size() < 3) continue;

                JsonObject o = new JsonObject();
                o.addProperty("id", pid); // Qt polygon id style
                JsonArray ids = new JsonArray();
                for(Integer idx : poly){
                    if(idx == null) continue;
                    ids.add(idx + 1); // back to Qt vertex id
                }
                o.add("vertexIds", ids);
                pArr.add(o);
            }
            root.add("polygons", pArr);
        }

        // neighborPairs: only meaningful if polygons are exported
        if(includePolygons){
            List<int[]> pairs = (imagePanel == null) ? null : imagePanel.getNeighborPairs();
            if(pairs != null && !pairs.isEmpty()){
                JsonArray nArr = new JsonArray();
                for(int[] pair : pairs){
                    if(pair == null || pair.length < 2) continue;
                    JsonObject o = new JsonObject();
                    o.addProperty("firstPolygonId", pair[0]);
                    o.addProperty("secondPolygonId", pair[1]);
                    nArr.add(o);
                }
                root.add("neighborPairs", nArr);
            }
        }

        try(Writer writer = new OutputStreamWriter(new FileOutputStream(file), StandardCharsets.UTF_8)){
            gson.toJson(root, writer);
        }
    }

    // =========================
    // Export Neighbor Pair Geometry CSV (Qt/sample-compatible)
    // =========================
    private static final String[] QT_NEIGHBOR_GEOMETRY_CSV_HEADERS = new String[]{
            "fileName","pairIndex","firstPolygonId","secondPolygonId","observed_division","division_timing",
            "areaRatio","areaMean","areaMin","areaMax","areaDiff",
            "perimeterRatio","perimeterMean","perimeterMin","perimeterMax","perimeterDiff",
            "aspectRatio","aspectMean","aspectMin","aspectMax","aspectDiff",
            "circularityRatio","circularityMean","circularityMin","circularityMax","circularityDiff",
            "solidityRatio","solidityMean","solidityMin","solidityMax","solidityDiff",
            "vertexCountRatio","vertexCountMean","vertexCountMin","vertexCountMax","vertexCountDiff",
            "centroidDistance","centroidDistanceNormalized",
            "unionAspectRatio","unionCircularity","unionConvexDeficiency",
            "normalizedSharedEdgeLength","sharedEdgeLength",
            "sharedEdgeUnsharedVerticesDistance","sharedEdgeUnsharedVerticesDistanceNormalized",
            "centroidSharedEdgeDistance","centroidSharedEdgeDistanceNormalized",
            "sharedEdgeUnionCentroidDistance","sharedEdgeUnionCentroidDistanceNormalized","sharedEdgeUnionAxisAngleDegrees",
            "junctionAngleAverageDegrees","junctionAngleMaxDegrees","junctionAngleMinDegrees","junctionAngleDifferenceDegrees","junctionAngleRatio"
    };

    private static final String[] ESTIMATED_DIVISION_PAIR_CSV_HEADERS = new String[]{
            "fileName",
            "firstCellId",
            "secondCellId",
            "firstCentroidX",
            "firstCentroidY",
            "secondCentroidX",
            "secondCentroidY",
            "divisionAngle0to90",
            "divisionAngle0to180",
            "averageCentroidX",
            "averageCentroidY"
    };

    private void exportNeighborPairGeometrics(JFrame frame){

        if(polygons == null || polygons.size() < 2){
            JOptionPane.showMessageDialog(frame,
                    "Need at least 2 polygons.",
                    "Export Neighbor Pair Geometrics",
                    JOptionPane.WARNING_MESSAGE);
            return;
        }

        List<int[]> pairs = (imagePanel == null) ? null : imagePanel.getNeighborPairs();
        if(pairs == null || pairs.isEmpty()){
            pairs = detectNeighborPairsFromPolygons(polygons);
        }

        if(pairs == null || pairs.isEmpty()){
            JOptionPane.showMessageDialog(frame,
                    "No neighboring polygon pairs found.\n(Need pairs sharing >=2 vertices.)",
                    "Export Neighbor Pair Geometrics",
                    JOptionPane.INFORMATION_MESSAGE);
            return;
        }

        ArrayList<int[]> sortedPairs = normalizeAndSortPairs(pairs);

        // ensure geometry exists for all exported pairs
        for(int[] pr : sortedPairs){
            int a = pr[0];
            int b = pr[1];
            long k = pairKey(a, b);
            if(!neighborPairGeometryCache.containsKey(k)){
                neighborPairGeometryCache.put(k, calculateGeometryForPair(a, b));
            }
        }

        String base = baseNameWithoutExtension(currentGeometrySourceFileName);
        if(base == null || base.trim().isEmpty()){
            base = "neighbor_pair_geometry";
        }

        SaveDialog sd = new SaveDialog(
                "Export Neighbor Pair Geometry CSV",
                base + "_neighbor_geometry",
                ".csv"
        );

        String dir = sd.getDirectory();
        String name = sd.getFileName();
        if(name == null) return;

        if(!name.toLowerCase(Locale.ROOT).endsWith(".csv")){
            name += ".csv";
        }

        File file = new File(dir, name);

        try{
            writeNeighborPairGeometryCsv(file, sortedPairs);
        }catch(Exception ex){
            IJ.error("Export Neighbor Pair Geometrics failed", ex.getMessage());
            return;
        }

        JOptionPane.showMessageDialog(frame,
                "Exported " + sortedPairs.size() + " neighbor pairs to:\n" + file.getAbsolutePath(),
                "Export Neighbor Pair Geometrics",
                JOptionPane.INFORMATION_MESSAGE);
    }

    private void exportEstimatedDivisionPairs(JFrame frame){

        if(polygons == null || polygons.size() < 2){
            JOptionPane.showMessageDialog(frame,
                    "Need at least 2 polygons.",
                    "Export Estimated Division Pairs",
                    JOptionPane.WARNING_MESSAGE);
            return;
        }

        ArrayList<int[]> selectedPairs = new ArrayList<>();
        for(Map.Entry<Long, EstimationEntry> e : neighborPairEstimationCache.entrySet()){
            if(e.getValue() == null || !e.getValue().selected) continue;
            long key = e.getKey();
            int a = (int)(key >> 32);
            int b = (int)(key & 0xffffffffL);
            if(a < 0 || b < 0 || a >= polygons.size() || b >= polygons.size()) continue;
            if(a == b) continue;
            selectedPairs.add(new int[]{Math.min(a, b), Math.max(a, b)});
        }

        ArrayList<int[]> sortedPairs = normalizeAndSortPairs(selectedPairs);
        if(sortedPairs.isEmpty()){
            JOptionPane.showMessageDialog(frame,
                    "No estimated division pairs selected.\nRun: Process → divided pair estimation... first.",
                    "Export Estimated Division Pairs",
                    JOptionPane.INFORMATION_MESSAGE);
            return;
        }

        String base = baseNameWithoutExtension(currentGeometrySourceFileName);
        if(base == null || base.trim().isEmpty()){
            base = "estimated_division_pairs";
        }

        SaveDialog sd = new SaveDialog(
                "Export Estimated Division Pairs CSV",
                base + "_estimated_division_pairs",
                ".csv"
        );

        String dir = sd.getDirectory();
        String name = sd.getFileName();
        if(name == null) return;

        if(!name.toLowerCase(Locale.ROOT).endsWith(".csv")){
            name += ".csv";
        }

        File file = new File(dir, name);

        try{
            writeEstimatedDivisionPairsCsv(file, sortedPairs);
        }catch(Exception ex){
            IJ.error("Export Estimated Division Pairs failed", ex.getMessage());
            return;
        }

        JOptionPane.showMessageDialog(frame,
                "Exported " + sortedPairs.size() + " estimated division pairs to:\n" + file.getAbsolutePath(),
                "Export Estimated Division Pairs",
                JOptionPane.INFORMATION_MESSAGE);
    }

    private void writeEstimatedDivisionPairsCsv(File file, List<int[]> sortedPairs) throws IOException{

        try(BufferedWriter bw = new BufferedWriter(
                new OutputStreamWriter(new FileOutputStream(file), StandardCharsets.UTF_8))){

            for(int i=0; i<ESTIMATED_DIVISION_PAIR_CSV_HEADERS.length; i++){
                if(i > 0) bw.write(",");
                bw.write(escapeCsvField(ESTIMATED_DIVISION_PAIR_CSV_HEADERS[i]));
            }
            bw.write("\n");

            String fileNameForCsv = currentGeometrySourceFileName;
            if(fileNameForCsv == null || fileNameForCsv.trim().isEmpty()){
                fileNameForCsv = "unknown.json";
            }

            for(int[] pr : sortedPairs){
                int firstCellId = pr[0];
                int secondCellId = pr[1];

                Point2D.Double c1 = polygonCentroid_PolygonItemLike(polygons.get(firstCellId));
                Point2D.Double c2 = polygonCentroid_PolygonItemLike(polygons.get(secondCellId));

                double dx = c2.x - c1.x;
                double dy = c2.y - c1.y;

                double divisionAngle0to90 = Math.toDegrees(Math.atan2(Math.abs(dy), Math.abs(dx)));

                double divisionAngle0to180 = Math.toDegrees(Math.atan2(dy, dx));
                if(divisionAngle0to180 < 0.0){
                    divisionAngle0to180 += 360.0;
                }
                if(divisionAngle0to180 > 180.0){
                    divisionAngle0to180 = 360.0 - divisionAngle0to180;
                }

                double avgX = (c1.x + c2.x) * 0.5;
                double avgY = (c1.y + c2.y) * 0.5;

                ArrayList<String> row = new ArrayList<>(ESTIMATED_DIVISION_PAIR_CSV_HEADERS.length);
                row.add(fileNameForCsv);
                row.add(String.valueOf(firstCellId));
                row.add(String.valueOf(secondCellId));
                row.add(formatQtCsvDouble(c1.x));
                row.add(formatQtCsvDouble(c1.y));
                row.add(formatQtCsvDouble(c2.x));
                row.add(formatQtCsvDouble(c2.y));
                row.add(formatQtCsvDouble(divisionAngle0to90));
                row.add(formatQtCsvDouble(divisionAngle0to180));
                row.add(formatQtCsvDouble(avgX));
                row.add(formatQtCsvDouble(avgY));

                for(int i=0; i<row.size(); i++){
                    if(i > 0) bw.write(",");
                    bw.write(escapeCsvField(row.get(i)));
                }
                bw.write("\n");
            }
        }
    }

    private static ArrayList<int[]> normalizeAndSortPairs(List<int[]> pairs){
        ArrayList<int[]> out = new ArrayList<>();
        HashSet<Long> seen = new HashSet<>();

        if(pairs != null){
            for(int[] pr : pairs){
                if(pr == null || pr.length < 2) continue;
                int a = Math.min(pr[0], pr[1]);
                int b = Math.max(pr[0], pr[1]);
                if(a == b) continue;
                long k = pairKey(a, b);
                if(seen.add(k)){
                    out.add(new int[]{a, b});
                }
            }
        }

        Collections.sort(out, (p1, p2) -> {
            if(p1[0] != p2[0]) return Integer.compare(p1[0], p2[0]);
            return Integer.compare(p1[1], p2[1]);
        });

        return out;
    }

    private void writeNeighborPairGeometryCsv(File file, List<int[]> sortedPairs) throws IOException{

        try(BufferedWriter bw = new BufferedWriter(
                new OutputStreamWriter(new FileOutputStream(file), StandardCharsets.UTF_8))){

            // header
            for(int i=0; i<QT_NEIGHBOR_GEOMETRY_CSV_HEADERS.length; i++){
                if(i > 0) bw.write(",");
                bw.write(escapeCsvField(QT_NEIGHBOR_GEOMETRY_CSV_HEADERS[i]));
            }
            bw.write("\n");

            String fileNameForCsv = currentGeometrySourceFileName;
            if(fileNameForCsv == null || fileNameForCsv.trim().isEmpty()){
                fileNameForCsv = "unknown.json";
            }

            int pairIndex = 0;
            for(int[] pr : sortedPairs){
                int a = pr[0];
                int b = pr[1];
                long k = pairKey(a, b);

                NeighborPairGeometryResult r = neighborPairGeometryCache.get(k);
                if(r == null){
                    r = calculateGeometryForPair(a, b);
                    neighborPairGeometryCache.put(k, r);
                }

                RealDivisionEntry real = realDivisionPairCache.get(k);
                boolean observedDivision = (real != null);
                double divisionTiming = (real != null && Double.isFinite(real.timing)) ? real.timing : -1.0;

                ArrayList<String> row = buildQtNeighborGeometryCsvRow(
                        fileNameForCsv,
                        ++pairIndex,
                        a,
                        b,
                        observedDivision,
                        divisionTiming,
                        r
                );

                for(int i=0; i<row.size(); i++){
                    if(i > 0) bw.write(",");
                    bw.write(escapeCsvField(row.get(i)));
                }
                bw.write("\n");
            }
        }
    }

    private ArrayList<String> buildQtNeighborGeometryCsvRow(String fileName,
                                                            int pairIndex,
                                                            int firstPolygonId,
                                                            int secondPolygonId,
                                                            boolean observedDivision,
                                                            double divisionTiming,
                                                            NeighborPairGeometryResult r){

        ArrayList<String> row = new ArrayList<>(QT_NEIGHBOR_GEOMETRY_CSV_HEADERS.length);

        row.add(fileName);
        row.add(String.valueOf(pairIndex));
        row.add(String.valueOf(firstPolygonId));
        row.add(String.valueOf(secondPolygonId));
        row.add(observedDivision ? "1" : "0");
        row.add((observedDivision && Double.isFinite(divisionTiming)) ? formatTimingValue(divisionTiming) : "-1");

        row.add(formatQtCsvDouble(r.areaRatio));
        row.add(formatQtCsvDouble(r.areaMean));
        row.add(formatQtCsvDouble(r.areaMin));
        row.add(formatQtCsvDouble(r.areaMax));
        row.add(formatQtCsvDouble(r.areaDiff));

        row.add(formatQtCsvDouble(r.perimeterRatio));
        row.add(formatQtCsvDouble(r.perimeterMean));
        row.add(formatQtCsvDouble(r.perimeterMin));
        row.add(formatQtCsvDouble(r.perimeterMax));
        row.add(formatQtCsvDouble(r.perimeterDiff));

        row.add(formatQtCsvDouble(r.aspectRatio));
        row.add(formatQtCsvDouble(r.aspectMean));
        row.add(formatQtCsvDouble(r.aspectMin));
        row.add(formatQtCsvDouble(r.aspectMax));
        row.add(formatQtCsvDouble(r.aspectDiff));

        row.add(formatQtCsvDouble(r.circularityRatio));
        row.add(formatQtCsvDouble(r.circularityMean));
        row.add(formatQtCsvDouble(r.circularityMin));
        row.add(formatQtCsvDouble(r.circularityMax));
        row.add(formatQtCsvDouble(r.circularityDiff));

        row.add(formatQtCsvDouble(r.solidityRatio));
        row.add(formatQtCsvDouble(r.solidityMean));
        row.add(formatQtCsvDouble(r.solidityMin));
        row.add(formatQtCsvDouble(r.solidityMax));
        row.add(formatQtCsvDouble(r.solidityDiff));

        row.add(formatQtCsvDouble(r.vertexCountRatio));
        row.add(formatQtCsvDouble(r.vertexCountMean));
        row.add(formatQtCsvDouble(r.vertexCountMin));
        row.add(formatQtCsvDouble(r.vertexCountMax));
        row.add(formatQtCsvDouble(r.vertexCountDiff));

        row.add(formatQtCsvDouble(r.centroidDistance));
        row.add(formatQtCsvDouble(r.centroidDistanceNormalized));

        row.add(formatQtCsvDouble(r.unionAspectRatio));
        row.add(formatQtCsvDouble(r.unionCircularity));
        row.add(formatQtCsvDouble(r.unionConvexDeficiency));

        row.add(formatQtCsvDouble(r.normalizedSharedEdgeLength));
        row.add(formatQtCsvDouble(r.sharedEdgeLength));

        row.add(formatQtCsvDouble(r.sharedEdgeUnsharedVerticesDistance));
        row.add(formatQtCsvDouble(r.sharedEdgeUnsharedVerticesDistanceNormalized));

        row.add(formatQtCsvDouble(r.centroidSharedEdgeDistance));
        row.add(formatQtCsvDouble(r.centroidSharedEdgeDistanceNormalized));

        row.add(formatQtCsvDouble(r.sharedEdgeUnionCentroidDistance));
        row.add(formatQtCsvDouble(r.sharedEdgeUnionCentroidDistanceNormalized));
        row.add(formatQtCsvDouble(r.sharedEdgeUnionAxisAngleDegrees));

        row.add(formatQtCsvDouble(r.junctionAngleAverageDegrees));
        row.add(formatQtCsvDouble(r.junctionAngleMaxDegrees));
        row.add(formatQtCsvDouble(r.junctionAngleMinDegrees));
        row.add(formatQtCsvDouble(r.junctionAngleDifferenceDegrees));
        row.add(formatQtCsvDouble(r.junctionAngleRatio));

        return row;
    }

    private static String formatQtCsvDouble(double value){
        if(!Double.isFinite(value)) return "N/A";
        return String.format(Locale.ROOT, "%.6f", value);
    }

    private static String formatTimingValue(double value){
        if(!Double.isFinite(value)) return "-1";

        double rounded = Math.rint(value);
        if(Math.abs(value - rounded) < 1e-9){
            return Long.toString((long)rounded);
        }

        DecimalFormat df = new DecimalFormat("0.###############");
        df.setDecimalSeparatorAlwaysShown(false);
        return df.format(value);
    }

    private static String escapeCsvField(String value){
        if(value == null) value = "";
        String escaped = value.replace("\"", "\"\"");
        if(escaped.contains(",") || escaped.contains("\"") || escaped.contains("\n") || escaped.contains("\r")){
            escaped = "\"" + escaped + "\"";
        }
        return escaped;
    }

    private static String baseNameWithoutExtension(String fileName){
        if(fileName == null || fileName.trim().isEmpty()) return "unknown";
        String base = new File(fileName).getName();
        int dot = base.lastIndexOf('.');
        if(dot > 0) return base.substring(0, dot);
        return base;
    }

    // =========================
    // Vertex detection
    // =========================

    private static List<Point> findPixelsWithNeighborCount(ByteProcessor bp, int target){

        int w = bp.getWidth();
        int h = bp.getHeight();

        List<Point> out = new ArrayList<>();

        for (int y = 1; y < h - 1; y++){
            for (int x = 1; x < w - 1; x++){

                if ((bp.get(x,y) & 0xff) == 0)
                    continue;

                int c = 0;

                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++){

                        if (dx == 0 && dy == 0) continue;

                        if ((bp.get(x+dx,y+dy) & 0xff) != 0)
                            c++;
                    }

                if (c == target)
                    out.add(new Point(x,y));
            }
        }

        return out;
    }

    private static List<Point> clusterPoints(List<Point> pts, double r){

        double r2 = r*r;

        boolean[] visited = new boolean[pts.size()];

        List<Point> out = new ArrayList<>();

        for(int i=0;i<pts.size();i++){

            if(visited[i]) continue;

            List<Integer> stack = new ArrayList<>();
            stack.add(i);
            visited[i] = true;

            double sx=0;
            double sy=0;
            int n=0;

            while(!stack.isEmpty()){

                int idx = stack.remove(stack.size()-1);

                Point p = pts.get(idx);

                sx += p.x;
                sy += p.y;
                n++;

                for(int j=0;j<pts.size();j++){

                    if(visited[j]) continue;

                    Point q = pts.get(j);

                    double dx = q.x - p.x;
                    double dy = q.y - p.y;

                    if(dx*dx + dy*dy <= r2){

                        visited[j] = true;
                        stack.add(j);
                    }
                }
            }

            out.add(new Point(
                    (int)Math.round(sx/n),
                    (int)Math.round(sy/n)
            ));
        }

        return out;
    }

    private void detectVertices3Neighbors(JFrame frame){

        if(currentImp==null) return;

        new Thread(() -> {

            ImageProcessor ip0=currentImp.getProcessor();
            ByteProcessor bp=(ByteProcessor)ip0.convertToByte(true);

            List<Point> raw=findPixelsWithNeighborCount(bp,3);
            List<Point> clustered=clusterPoints(raw,2.0);

            vertexPoints=clustered;

            lineEdges.clear();
            polygons.clear();

            SwingUtilities.invokeLater(() -> {
                imagePanel.setOverlayPoints(vertexPoints);
                imagePanel.setLines(lineEdges);
                imagePanel.setPolygons(polygons);
            });

        }).start();
    }

    // =========================
    // Line detection
    // =========================

    private void detectLines(JFrame frame){

        if(currentImp==null) return;

        new Thread(() -> {

            ImageProcessor ip0=currentImp.getProcessor();
            ByteProcessor bp=(ByteProcessor)ip0.convertToByte(true);

            int w=bp.getWidth();
            int h=bp.getHeight();

            // If vertices are slightly off the skeleton (e.g. after clustering),
            // this tolerance makes endpoint matching robust.
            final int hitRadius = 2;   // try 1..3

            // Use overlayPoints so manual edits (drag/add/delete) are respected.
            List<Point> verts = imagePanel.getOverlayPoints();

            // For each skeleton pixel, store which vertex index it belongs to (within hitRadius), else -1.
            int[] vertexAtPixel = buildVertexAtPixelMap(bp, verts, hitRadius);

            HashSet<Long> visitedPixelEdges = new HashSet<>();
            HashSet<LineEdge> edges = new HashSet<>();

            for(int vi=0; vi<verts.size(); vi++){

                Point v = verts.get(vi);

                // If vertex coordinates can be out of bounds (rare), guard:
                if(v.x < 0 || v.y < 0 || v.x >= w || v.y >= h) continue;

                int vIdx = v.y*w + v.x;

                // Start from each adjacent skeleton pixel
                for(int dy=-1; dy<=1; dy++)
                for(int dx=-1; dx<=1; dx++){

                    if(dx==0 && dy==0) continue;

                    int sx = v.x + dx;
                    int sy = v.y + dy;

                    if(sx<0 || sy<0 || sx>=w || sy>=h) continue;
                    if(bp.get(sx,sy)==0) continue;

                    int sIdx = sy*w + sx;

                    // If we already explored this edge, skip
                    if(wasVisited(visitedPixelEdges, vIdx, sIdx)) continue;

                    // DFS stack items: (prevIdx, curIdx)
                    ArrayDeque<int[]> stack = new ArrayDeque<>();
                    stack.push(new int[]{vIdx, sIdx});

                    while(!stack.isEmpty()){

                        int[] state = stack.pop();
                        int prev = state[0];
                        int cur  = state[1];

                        if(wasVisited(visitedPixelEdges, prev, cur)) continue;

                        addVisited(visitedPixelEdges, prev, cur);

                        // If current pixel is mapped to a vertex (not itself), we found an edge
                        int foundVertex = vertexAtPixel[cur];
                        if(foundVertex >= 0 && foundVertex != vi){
                            edges.add(new LineEdge(vi, foundVertex));
                            continue;
                        }

                        int cx = cur % w;
                        int cy = cur / w;

                        // Explore all skeleton neighbors (no greedy first-choice)
                        for(int ndy=-1; ndy<=1; ndy++)
                        for(int ndx=-1; ndx<=1; ndx++){

                            if(ndx==0 && ndy==0) continue;

                            int nx = cx + ndx;
                            int ny = cy + ndy;

                            if(nx<0 || ny<0 || nx>=w || ny>=h) continue;

                            int nIdx = ny*w + nx;

                            if(nIdx == prev) continue;

                            // If neighbor is a vertex pixel, connect immediately
                            int neighborVertex = vertexAtPixel[nIdx];
                            if(neighborVertex >= 0 && neighborVertex != vi){
                                edges.add(new LineEdge(vi, neighborVertex));
                                addVisited(visitedPixelEdges, cur, nIdx);
                                continue;
                            }

                            if(bp.get(nx,ny)==0) continue;
                            if(wasVisited(visitedPixelEdges, cur, nIdx)) continue;

                            stack.push(new int[]{cur, nIdx});
                        }
                    }
                }
            }

            lineEdges = new ArrayList<>(edges);

            SwingUtilities.invokeLater(() -> imagePanel.setLines(lineEdges));

        }).start();
    }

    private static void addVisited(HashSet<Long> visited, int fromIdx, int toIdx){
        visited.add(edgeKey(fromIdx,toIdx));
        visited.add(edgeKey(toIdx,fromIdx));
    }

    private static boolean wasVisited(HashSet<Long> visited, int fromIdx, int toIdx){
        return visited.contains(edgeKey(fromIdx,toIdx));
    }

    /**
     * Build a lookup table: for each skeleton pixel, which vertex index is within hitRadius (closest d^2 wins).
     * Non-skeleton pixels remain -1.
     */
    private int[] buildVertexAtPixelMap(ByteProcessor bp, List<Point> vertices, int hitRadius){

        int w = bp.getWidth();
        int h = bp.getHeight();

        int[] vertexAt = new int[w*h];
        int[] bestD2   = new int[w*h];

        Arrays.fill(vertexAt, -1);
        Arrays.fill(bestD2, Integer.MAX_VALUE);

        int r2 = hitRadius * hitRadius;

        for(int i=0; i<vertices.size(); i++){

            Point v = vertices.get(i);

            for(int dy=-hitRadius; dy<=hitRadius; dy++)
            for(int dx=-hitRadius; dx<=hitRadius; dx++){

                int d2 = dx*dx + dy*dy;
                if(d2 > r2) continue;

                int x = v.x + dx;
                int y = v.y + dy;

                if(x<0 || y<0 || x>=w || y>=h) continue;
                if(bp.get(x,y)==0) continue; // only map skeleton pixels

                int idx = y*w + x;

                if(d2 < bestD2[idx]){
                    bestD2[idx] = d2;
                    vertexAt[idx] = i;
                }
            }
        }

        return vertexAt;
    }

    // =========================
    // Polygon detection (CCW face walking)
    // =========================

    private void detectPolygons(JFrame frame){

        // Run in background thread (prevents UI freeze)
        new Thread(() -> {

            // Use overlayPoints to respect manual vertex edits
            List<Point> verts = imagePanel.getOverlayPoints();
            List<LineEdge> edges = lineEdges;   // line deletion updates this because ImagePanel holds same list

            int n = verts.size();
            if(n < 3 || edges.size() < 3){
                polygons = new ArrayList<>();
                SwingUtilities.invokeLater(() -> imagePanel.setPolygons(polygons));
                return;
            }

            // ----- Half-edge structure -----
            class HalfEdge {
                int origin;   // vertex id
                int dest;     // vertex id
                int twin;     // half-edge id
                int next;     // half-edge id (CCW boundary successor)
                HalfEdge(int o, int d){ origin=o; dest=d; twin=-1; next=-1; }
            }

            ArrayList<HalfEdge> halfEdges = new ArrayList<>(edges.size() * 2);

            // Add 2 directed half-edges for each undirected edge
            for(LineEdge e : edges){
                if(e.v1 == e.v2) continue;
                if(e.v1 < 0 || e.v2 < 0 || e.v1 >= n || e.v2 >= n) continue;

                int id1 = halfEdges.size();
                halfEdges.add(new HalfEdge(e.v1, e.v2));
                int id2 = halfEdges.size();
                halfEdges.add(new HalfEdge(e.v2, e.v1));

                halfEdges.get(id1).twin = id2;
                halfEdges.get(id2).twin = id1;
            }

            if(halfEdges.size() < 6){
                polygons = new ArrayList<>();
                SwingUtilities.invokeLater(() -> imagePanel.setPolygons(polygons));
                return;
            }

            // ----- Build vertex "star": outgoing half-edges sorted by angle -----
            class OutEdge {
                int hid;
                double ang;
                OutEdge(int h, double a){ hid=h; ang=a; }
            }

            HashMap<Integer, ArrayList<OutEdge>> star = new HashMap<>();

            for(int hid=0; hid<halfEdges.size(); hid++){
                HalfEdge he = halfEdges.get(hid);
                Point a = verts.get(he.origin);
                Point b = verts.get(he.dest);
                double ang = normalizedAngle(a, b);

                star.computeIfAbsent(he.origin, k -> new ArrayList<>()).add(new OutEdge(hid, ang));
            }

            for(ArrayList<OutEdge> list : star.values()){
                list.sort((p,q) -> {
                    if(p.ang == q.ang) return Integer.compare(p.hid, q.hid);
                    return Double.compare(p.ang, q.ang);
                });
            }

            // ----- Compute "next" pointers (CCW face walking rule) -----
            // For half-edge u->v, look at vertex v's outgoing list, find twin (v->u),
            // then take the previous edge in CCW order (i.e., turn "left" to hug a face).
            for(int hid=0; hid<halfEdges.size(); hid++){
                HalfEdge he = halfEdges.get(hid);
                if(he.twin < 0) continue;

                ArrayList<OutEdge> out = star.get(he.dest);
                if(out == null || out.isEmpty()) continue;

                int twinId = he.twin;

                int idx = -1;
                for(int i=0; i<out.size(); i++){
                    if(out.get(i).hid == twinId){ idx = i; break; }
                }
                if(idx < 0) continue;

                int prevIdx = (idx - 1 + out.size()) % out.size();
                he.next = out.get(prevIdx).hid;
            }

            // ----- Traverse faces -----
            boolean[] visited = new boolean[halfEdges.size()];
            ArrayList<List<Integer>> found = new ArrayList<>();
            HashSet<String> seen = new HashSet<>();

            final double AREA_EPS = 1e-6;

            for(int start=0; start<halfEdges.size(); start++){
                if(visited[start]) continue;

                ArrayList<Integer> traversal = new ArrayList<>(16);
                int cur = start;
                boolean closed = false;

                while(cur >= 0 && cur < halfEdges.size() && !visited[cur]){
                    visited[cur] = true;
                    traversal.add(cur);

                    int nxt = halfEdges.get(cur).next;
                    if(nxt < 0){
                        cur = -1;
                        break;
                    }
                    cur = nxt;

                    if(cur == start){
                        closed = true;
                        break;
                    }
                }

                if(!closed) continue;

                ArrayList<Integer> cycle = new ArrayList<>(traversal.size());
                for(int hid : traversal){
                    cycle.add(halfEdges.get(hid).origin);
                }

                if(cycle.size() < 3) continue;

                double area = signedArea(cycle, verts);
                if(area <= AREA_EPS) continue; // discard outer face / CW loops

                List<Integer> canon = canonicalizePolygon(cycle);
                String key = polygonKey(canon);
                if(seen.contains(key)) continue;

                seen.add(key);
                found.add(cycle);
            }

            polygons = found;

            SwingUtilities.invokeLater(() -> imagePanel.setPolygons(polygons));

        }).start();
    }

    private static double normalizedAngle(Point a, Point b){
        double ang = Math.atan2((double)b.y - a.y, (double)b.x - a.x);
        if(ang < 0) ang += Math.PI * 2.0;
        return ang;
    }

    // Positive area => CCW polygon
    private static double signedArea(List<Integer> poly, List<Point> verts){
        double s = 0.0;
        int m = poly.size();
        for(int i=0; i<m; i++){
            Point p = verts.get(poly.get(i));
            Point q = verts.get(poly.get((i+1) % m));
            s += (double)p.x * (double)q.y - (double)q.x * (double)p.y;
        }
        return 0.5 * s;
    }

    // Rotate to canonical: start at smallest vertex id, pick lexicographically smallest rotation
    private static List<Integer> canonicalizePolygon(List<Integer> ids){
        int m = ids.size();
        if(m == 0) return Collections.emptyList();

        int minVal = ids.get(0);
        for(int v : ids) minVal = Math.min(minVal, v);

        ArrayList<Integer> best = null;

        for(int i=0; i<m; i++){
            if(ids.get(i) != minVal) continue;

            ArrayList<Integer> rot = new ArrayList<>(m);
            for(int k=0; k<m; k++) rot.add(ids.get((i+k) % m));

            if(best == null || lexLess(rot, best)){
                best = rot;
            }
        }

        return best == null ? new ArrayList<>(ids) : best;
    }

    private static boolean lexLess(List<Integer> a, List<Integer> b){
        int m = Math.min(a.size(), b.size());
        for(int i=0; i<m; i++){
            int da = a.get(i), db = b.get(i);
            if(da != db) return da < db;
        }
        return a.size() < b.size();
    }

    private static String polygonKey(List<Integer> ids){
        StringBuilder sb = new StringBuilder(ids.size() * 4);
        for(int v : ids){
            sb.append(v).append(':');
        }
        return sb.toString();
    }

    // =========================
    // Neighbor Pair Detection
    // =========================
    private void detectNeighborPairs(JFrame frame){

        if(polygons == null || polygons.size() < 2){
            JOptionPane.showMessageDialog(frame, "No polygons (need ≥2). Run Detect Polygons first.");
            imagePanel.setNeighborPairs(new ArrayList<>());
            updateInfoPanel();
            return;
        }

        // --- collect pairs in a set to avoid duplicates ---
        HashSet<PolyPair> pairSet = new HashSet<>();

        // ===== A) vertex-touch neighbors (share ≥1 vertex) =====
        HashMap<Integer, ArrayList<Integer>> vertexToPolys = new HashMap<>();

        for(int pi = 0; pi < polygons.size(); pi++){
            List<Integer> poly = polygons.get(pi);
            if(poly == null) continue;

            // unique vertices within this polygon
            HashSet<Integer> uniq = new HashSet<>();
            for(int vid : poly){
                if(vid < 0) continue;
                if(!uniq.add(vid)) continue;
                vertexToPolys.computeIfAbsent(vid, k -> new ArrayList<>()).add(pi);
            }
        }

        for(Map.Entry<Integer, ArrayList<Integer>> en : vertexToPolys.entrySet()){
            ArrayList<Integer> lst = en.getValue();
            int k = lst.size();
            for(int i = 0; i < k; i++){
                for(int j = i + 1; j < k; j++){
                    pairSet.add(new PolyPair(lst.get(i), lst.get(j)));
                }
            }
        }

        // ===== B) shared-line neighbors (share a line edge in lineEdges) =====
        // Build a set of existing line edges
        HashSet<Long> existingLineKeys = new HashSet<>();
        if(lineEdges != null){
            for(LineEdge e : lineEdges){
                existingLineKeys.add(edgeKey(e.v1, e.v2));
            }
        }

        // Map edgeKey -> first polygon that owns it; when second polygon hits same edge => neighbor pair
        HashMap<Long, Integer> edgeOwner = new HashMap<>();

        for(int pi = 0; pi < polygons.size(); pi++){
            List<Integer> poly = polygons.get(pi);
            if(poly == null || poly.size() < 2) continue;

            int m = poly.size();
            for(int i = 0; i < m; i++){
                int va = poly.get(i);
                int vb = poly.get((i + 1) % m);
                if(va < 0 || vb < 0) continue;

                long key = edgeKey(va, vb);

                // Only count if this edge is a real detected line
                if(!existingLineKeys.contains(key)) continue;

                Integer prev = edgeOwner.putIfAbsent(key, pi);
                if(prev != null && prev != pi){
                    pairSet.add(new PolyPair(prev, pi));
                }
            }
        }

        // ===== Convert to ImagePanel format =====
        ArrayList<int[]> pairsForPanel = new ArrayList<>();
        for(PolyPair p : pairSet){
            pairsForPanel.add(new int[]{p.a, p.b});
        }

        // optional: stable order
        pairsForPanel.sort((x, y) -> {
            if(x[0] != y[0]) return Integer.compare(x[0], y[0]);
            return Integer.compare(x[1], y[1]);
        });

        imagePanel.setNeighborPairs(pairsForPanel);
        updateInfoPanel();
        IJ.showStatus("Neighbor pairs detected: " + pairsForPanel.size());
        JOptionPane.showMessageDialog(frame, "Neighbor pairs detected: " + pairsForPanel.size());
    }

    // =====================================================
    // Neighbor Pair Preview (Qt-like table + 3 preview images)
    // =====================================================

    private static final int PREVIEW_SIZE = 120;
    private static final double PREVIEW_MARGIN = 6.0;

    // Cache to avoid huge RAM usage when there are many pairs.
    // Stores 3 icons per pair (preview/union/contact), evicts old entries.
    private static final int PREVIEW_CACHE_MAX = 250;
    private final LinkedHashMap<Long, ImageIcon[]> neighborPreviewCache =
            new LinkedHashMap<Long, ImageIcon[]>(512, 0.75f, true) {
                @Override
                protected boolean removeEldestEntry(Map.Entry<Long, ImageIcon[]> eldest) {
                    return size() > PREVIEW_CACHE_MAX;
                }
            };

    // ===== Neighbor pair geometry cache (computed 49 features) =====
    private final HashMap<Long, NeighborPairGeometryResult> neighborPairGeometryCache = new HashMap<>();
    private void clearNeighborPairGeometryCache(){
        neighborPairGeometryCache.clear();
    }

    // ===== Real (groud-truth) division pairs cache =====
    // key: pairKey (polyA, polyB) using polygon indices (0-based, like your polygons list)
    private final HashMap<Long, RealDivisionEntry> realDivisionPairCache = new HashMap<>();

    private void clearRealDivisionPairCache(){
        realDivisionPairCache.clear();
    }

    private static class RealDivisionEntry {
        double timing = Double.NaN; // third column (e.g., 2). keep for later calculations
    }

    // ===== Comparison result caches (Qt: TruePositive / FalsePositive / FalseNegative arrows) =====
    private final HashMap<Long, RealDivisionEntry> truePositivePairCache = new HashMap<>();
    private final HashMap<Long, RealDivisionEntry> falsePositivePairCache = new HashMap<>();
    private final HashMap<Long, RealDivisionEntry> falseNegativePairCache = new HashMap<>();

    private void clearComparisonPairCaches(){
        truePositivePairCache.clear();
        falsePositivePairCache.clear();
        falseNegativePairCache.clear();
        clearDivisionMetricsCache();
    }

    private void clearDivisionMetricsCache(){
        lastDivisionMetrics = null;
    }

    // ===== divided pair estimation cache (threshold + matching results) =====
    private final HashMap<Long, EstimationEntry> neighborPairEstimationCache = new HashMap<>();
    private Criterion lastEstimationCriterion = null;
    private void clearNeighborPairEstimationCache(){
        neighborPairEstimationCache.clear();
        lastEstimationCriterion = null;
    }

    // ---- Estimation structs (Qt: DivisionEstimator::Criterion / CandidateMatch / EstimationEntry) ----
    private static class Criterion {
        enum MatchingMode { LocalOptimal, GlobalMaximumWeight, Unconstrained }

        String featureKey = "junctionAngleAverageDegrees";
        boolean requireAbove = true;     // true: >= threshold, false: <= threshold
        double threshold = 145.0;        // Qt default
        MatchingMode matchingMode = MatchingMode.GlobalMaximumWeight;
    }

    private static class EstimationEntry {
        boolean passed = false;
        double featureValue = Double.NaN;
        double score = Double.NaN;
        boolean selected = false;
    }

    private static class CandidateMatch {
        int first;
        int second;
        double score;
        double featureValue;
        CandidateMatch(int first, int second, double score, double featureValue){
            this.first = first;
            this.second = second;
            this.score = score;
            this.featureValue = featureValue;
        }
    }

    private static class Solution {
        double weight;
        ArrayList<CandidateMatch> matches;
        Solution(){
            this.weight = 0.0;
            this.matches = new ArrayList<>();
        }
        Solution(double w, ArrayList<CandidateMatch> m){
            this.weight = w;
            this.matches = m;
        }
    }

    private static long pairKey(int a, int b){
        int lo = Math.min(a, b);
        int hi = Math.max(a, b);
        return (((long)lo) << 32) ^ (hi & 0xffffffffL);
    }

    private void showNeighborPairPreviewDialog(JFrame frame){

        if(polygons == null || polygons.size() < 2){
            JOptionPane.showMessageDialog(frame, "Need at least 2 polygons.", "Neighbor Pair Preview",
                    JOptionPane.WARNING_MESSAGE);
            return;
        }

        List<int[]> pairs = null;
        if(imagePanel != null){
            pairs = imagePanel.getNeighborPairs();
        }

        // If user hasn't run "Detect Neighbor Polygons", fallback: compute neighbors by >=2 shared vertices (Qt rule).
        if(pairs == null || pairs.isEmpty()){
            pairs = detectNeighborPairsFromPolygons(polygons);
        }

        if(pairs.isEmpty()){
            JOptionPane.showMessageDialog(frame, "No neighboring polygon pairs found.\n(Need pairs sharing >=2 vertices.)",
                    "Neighbor Pair Preview", JOptionPane.INFORMATION_MESSAGE);
            return;
        }

        boolean showGeom = hasGeometryForPairs(pairs);
        boolean showEst  = showGeom && hasEstimationForPairs(pairs);

        AbstractTableModel model;
        if(showGeom && showEst){
            model = new NeighborPairGeometryEstimationTableModel(pairs);
        }else if(showGeom){
            model = new NeighborPairGeometryTableModel(pairs);
        }else{
            model = new NeighborPairPreviewTableModel(pairs);
        }

        JTable table = new JTable(model);
        table.setRowHeight(PREVIEW_SIZE);
        if(showGeom){
            table.setAutoResizeMode(JTable.AUTO_RESIZE_OFF);
        }

        // Center text in ID columns
        DefaultTableCellRenderer center = new DefaultTableCellRenderer();
        center.setHorizontalAlignment(SwingConstants.CENTER);
        table.getColumnModel().getColumn(0).setCellRenderer(center);
        table.getColumnModel().getColumn(1).setCellRenderer(center);

        // Icon renderer for preview columns
        DefaultTableCellRenderer iconRenderer = new DefaultTableCellRenderer(){
            @Override
            public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected,
                                                        boolean hasFocus, int row, int col){
                JLabel lbl = (JLabel)super.getTableCellRendererComponent(table, "", isSelected, hasFocus, row, col);
                lbl.setHorizontalAlignment(SwingConstants.CENTER);
                lbl.setVerticalAlignment(SwingConstants.CENTER);
                lbl.setText("");
                if(value instanceof Icon){
                    lbl.setIcon((Icon)value);
                }else{
                    lbl.setIcon(null);
                }
                return lbl;
            }
        };
        table.getColumnModel().getColumn(2).setCellRenderer(iconRenderer);
        table.getColumnModel().getColumn(3).setCellRenderer(iconRenderer);
        table.getColumnModel().getColumn(4).setCellRenderer(iconRenderer);

        // Make preview columns a bit wider
        table.getColumnModel().getColumn(0).setPreferredWidth(70);
        table.getColumnModel().getColumn(1).setPreferredWidth(70);
        table.getColumnModel().getColumn(2).setPreferredWidth(PREVIEW_SIZE + 20);
        table.getColumnModel().getColumn(3).setPreferredWidth(PREVIEW_SIZE + 20);
        table.getColumnModel().getColumn(4).setPreferredWidth(PREVIEW_SIZE + 20);

        if(showGeom){
            DoubleCellRenderer dbl = new DoubleCellRenderer();
            for(int c=5; c<table.getColumnCount(); c++){
                Class<?> cls = model.getColumnClass(c);
                if(cls == Double.class){
                    table.getColumnModel().getColumn(c).setCellRenderer(dbl);
                    table.getColumnModel().getColumn(c).setPreferredWidth(110);
                }else if(cls == Boolean.class){
                    table.getColumnModel().getColumn(c).setPreferredWidth(80);
                }else{
                    table.getColumnModel().getColumn(c).setPreferredWidth(110);
                }
            }
        }

        JDialog dialog = new JDialog(frame, "Neighbor Pair Preview", false);
        dialog.setLayout(new BorderLayout());
        dialog.add(new JScrollPane(table), BorderLayout.CENTER);

        JPanel bottom = new JPanel(new FlowLayout(FlowLayout.LEFT));
        JLabel info = new JLabel("Pairs: " + pairs.size() + "   (Cache max: " + PREVIEW_CACHE_MAX + ")");
        JButton clearCache = new JButton("Clear Preview Cache");
        clearCache.addActionListener(e -> {
            neighborPreviewCache.clear();
            table.repaint();
        });
        bottom.add(info);
        bottom.add(clearCache);
        dialog.add(bottom, BorderLayout.SOUTH);

        dialog.setSize(900, 600);
        dialog.setLocationRelativeTo(frame);
        dialog.setVisible(true);
    }

    // ---- Table model ----

    private class NeighborPairPreviewTableModel extends AbstractTableModel {

        private final String[] cols = {
                "Polygon 1", "Polygon 2", "Preview", "Union Preview", "Contact Preview"
        };

        private final ArrayList<int[]> pairs;

        NeighborPairPreviewTableModel(List<int[]> pairs){
            this.pairs = new ArrayList<>(pairs);
            // Sort like Qt (by idA, idB)
            Collections.sort(this.pairs, (p1, p2) -> {
                int a1 = Math.min(p1[0], p1[1]);
                int b1 = Math.max(p1[0], p1[1]);
                int a2 = Math.min(p2[0], p2[1]);
                int b2 = Math.max(p2[0], p2[1]);
                if(a1 != a2) return Integer.compare(a1, a2);
                return Integer.compare(b1, b2);
            });
        }

        @Override public int getRowCount(){ return pairs.size(); }
        @Override public int getColumnCount(){ return cols.length; }
        @Override public String getColumnName(int c){ return cols[c]; }

        @Override
        public Class<?> getColumnClass(int columnIndex){
            if(columnIndex >= 2) return Icon.class;
            return Integer.class;
        }

        @Override
        public Object getValueAt(int rowIndex, int columnIndex){
            int[] pr = pairs.get(rowIndex);
            int a = pr[0];
            int b = pr[1];

            if(columnIndex == 0) return a;
            if(columnIndex == 1) return b;

            // 2=preview, 3=union, 4=contact
            return getPreviewIcon(a, b, columnIndex);
        }
    }

    // ---- Preview icon retrieval (cached) ----

    private Icon getPreviewIcon(int polyA, int polyB, int tableCol){
        long key = pairKey(polyA, polyB);

        ImageIcon[] arr = neighborPreviewCache.get(key);
        if(arr == null){
            arr = new ImageIcon[3];
            neighborPreviewCache.put(key, arr);
        }

        int slot = tableCol - 2; // 0 preview, 1 union, 2 contact
        if(slot < 0 || slot > 2) return null;

        if(arr[slot] != null) return arr[slot];

        // generate lazily (keeps UI simple + avoids huge memory)
        if(slot == 0) arr[slot] = buildPreviewIcon(polyA, polyB);
        else if(slot == 1) arr[slot] = buildUnionPreviewIcon(polyA, polyB);
        else arr[slot] = buildContactPreviewIcon(polyA, polyB);

        return arr[slot];
    }

    // ---- Neighbor detection fallback: >=2 shared vertices (Qt NeighborPair::isNeighbor) ----
    private static List<int[]> detectNeighborPairsFromPolygons(List<List<Integer>> polys){

        HashMap<Integer, ArrayList<Integer>> vToPolys = new HashMap<>();
        for(int pid=0; pid<polys.size(); pid++){
            List<Integer> poly = polys.get(pid);
            if(poly == null) continue;
            for(Integer vid : poly){
                if(vid == null) continue;
                vToPolys.computeIfAbsent(vid, k -> new ArrayList<>()).add(pid);
            }
        }

        HashMap<Long, Integer> counts = new HashMap<>();
        for(Map.Entry<Integer, ArrayList<Integer>> e : vToPolys.entrySet()){
            ArrayList<Integer> ps = e.getValue();
            if(ps.size() < 2) continue;
            for(int i=0; i<ps.size(); i++){
                for(int j=i+1; j<ps.size(); j++){
                    int a = ps.get(i), b = ps.get(j);
                    long k = pairKey(a, b);
                    counts.put(k, counts.getOrDefault(k, 0) + 1);
                }
            }
        }

        ArrayList<int[]> out = new ArrayList<>();
        for(Map.Entry<Long, Integer> e : counts.entrySet()){
            if(e.getValue() >= 2){
                long k = e.getKey();
                int a = (int)(k >> 32);
                int b = (int)(k & 0xffffffffL);
                out.add(new int[]{a, b});
            }
        }
        return out;
    }

    // ---- Geometry helpers ----

    private Path2D.Double polygonPath(int polygonId){
        if(polygons == null || polygonId < 0 || polygonId >= polygons.size()) return null;
        List<Integer> ids = polygons.get(polygonId);
        if(ids == null || ids.size() < 3) return null;

        Path2D.Double path = new Path2D.Double();
        Point p0 = vertexPoints.get(ids.get(0));
        path.moveTo(p0.x, p0.y);
        for(int i=1; i<ids.size(); i++){
            Point p = vertexPoints.get(ids.get(i));
            path.lineTo(p.x, p.y);
        }
        path.closePath();
        return path;
    }

    private static AffineTransform worldToPreview(Rectangle2D bounds, int size, double margin){

        if(bounds == null || bounds.getWidth() <= 0 || bounds.getHeight() <= 0){
            bounds = new Rectangle2D.Double(-1, -1, 2, 2);
        }

        double scaleX = (size - 2.0 * margin) / bounds.getWidth();
        double scaleY = (size - 2.0 * margin) / bounds.getHeight();
        double scale = Math.min(scaleX, scaleY);

        AffineTransform at = new AffineTransform();
        at.translate(size / 2.0, size / 2.0);
        at.scale(scale, scale);
        at.translate(-bounds.getCenterX(), -bounds.getCenterY());
        return at;
    }

    private static BufferedImage blankPreview(){
        BufferedImage img = new BufferedImage(PREVIEW_SIZE, PREVIEW_SIZE, BufferedImage.TYPE_INT_ARGB);
        Graphics2D g = img.createGraphics();
        g.setColor(Color.WHITE);
        g.fillRect(0, 0, PREVIEW_SIZE, PREVIEW_SIZE);
        g.dispose();
        return img;
    }

    private ImageIcon buildPreviewIcon(int polyA, int polyB){

        Path2D.Double a = polygonPath(polyA);
        Path2D.Double b = polygonPath(polyB);

        BufferedImage img = blankPreview();
        if(a == null && b == null) return new ImageIcon(img);

        Rectangle2D bounds = null;
        if(a != null) bounds = a.getBounds2D();
        if(b != null) bounds = (bounds == null) ? b.getBounds2D() : bounds.createUnion(b.getBounds2D());

        AffineTransform at = worldToPreview(bounds, PREVIEW_SIZE, PREVIEW_MARGIN);

        Graphics2D g = img.createGraphics();
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);

        // Draw filled polygons in world coords under transform
        g.transform(at);

        if(a != null){
            g.setColor(new Color(76, 132, 255, 120));
            g.fill(a);
            g.setColor(new Color(76, 132, 255, 180));
            g.draw(a);
        }
        if(b != null){
            g.setColor(new Color(255, 112, 67, 120));
            g.fill(b);
            g.setColor(new Color(255, 112, 67, 180));
            g.draw(b);
        }

        // Labels (draw in image coords, not world coords)
        g.setTransform(new AffineTransform());
        g.setColor(Color.BLACK);
        g.setFont(g.getFont().deriveFont(Font.BOLD, 11f));

        if(a != null){
            Point2D c = new Point2D.Double(a.getBounds2D().getCenterX(), a.getBounds2D().getCenterY());
            Point2D ci = at.transform(c, null);
            drawCenteredString(g, String.valueOf(polyA), (int)ci.getX(), (int)ci.getY());
        }
        if(b != null){
            Point2D c = new Point2D.Double(b.getBounds2D().getCenterX(), b.getBounds2D().getCenterY());
            Point2D ci = at.transform(c, null);
            drawCenteredString(g, String.valueOf(polyB), (int)ci.getX(), (int)ci.getY());
        }

        g.dispose();
        return new ImageIcon(img);
    }

    private ImageIcon buildUnionPreviewIcon(int polyA, int polyB){

        Path2D.Double a = polygonPath(polyA);
        Path2D.Double b = polygonPath(polyB);

        BufferedImage img = blankPreview();
        if(a == null && b == null) return new ImageIcon(img);

        Area union = new Area();
        if(a != null) union.add(new Area(a));
        if(b != null) union.add(new Area(b));
        Rectangle2D bounds = union.getBounds2D();

        AffineTransform at = worldToPreview(bounds, PREVIEW_SIZE, PREVIEW_MARGIN);
        double scale = at.getScaleX(); // uniform scale

        // shared edge keys
        HashSet<Long> edgesA = polygonEdgeKeys(polyA);
        HashSet<Long> edgesB = polygonEdgeKeys(polyB);

        Graphics2D g = img.createGraphics();
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        g.transform(at);

        // Union fill
        g.setColor(new Color(76, 175, 80, 160));
        g.fill(union);

        // Draw outer edges of A and B but skip shared edges (avoid internal boundary)
        g.setColor(new Color(46, 125, 50));
        g.setStroke(new BasicStroke((float)(1.0 / Math.max(scale, 1e-6))));

        drawPolygonEdgesSkippingShared(g, polyA, edgesB);
        drawPolygonEdgesSkippingShared(g, polyB, edgesA);

        g.dispose();
        return new ImageIcon(img);
    }

    private ImageIcon buildContactPreviewIcon(int polyA, int polyB){

        Path2D.Double a = polygonPath(polyA);
        Path2D.Double b = polygonPath(polyB);

        BufferedImage img = blankPreview();
        if(a == null && b == null) return new ImageIcon(img);

        // bounds = union of both polygons
        Rectangle2D bounds = null;
        if(a != null) bounds = a.getBounds2D();
        if(b != null) bounds = (bounds == null) ? b.getBounds2D() : bounds.createUnion(b.getBounds2D());

        AffineTransform at = worldToPreview(bounds, PREVIEW_SIZE, PREVIEW_MARGIN);
        double scale = at.getScaleX();

        // Shared vertices: intersection of vertex indices
        HashSet<Integer> sharedVerts = polygonSharedVertices(polyA, polyB);

        // Shared edges: present in both polygons AND both endpoints are shared
        HashSet<Long> edgesA = polygonEdgeKeys(polyA);
        HashSet<Long> edgesB = polygonEdgeKeys(polyB);
        HashSet<Long> sharedEdges = new HashSet<>();
        for(long ek : edgesA){
            if(edgesB.contains(ek)){
                int v1 = (int)(ek >> 32);
                int v2 = (int)(ek & 0xffffffffL);
                if(sharedVerts.contains(v1) && sharedVerts.contains(v2)){
                    sharedEdges.add(ek);
                }
            }
        }

        // Connecting edges: edges with exactly one shared vertex
        ArrayList<long[]> connectingEdgesA = polygonConnectingEdges(polyA, sharedVerts);
        ArrayList<long[]> connectingEdgesB = polygonConnectingEdges(polyB, sharedVerts);

        HashSet<Integer> connectingVertsA = connectingVerticesFromConnectingEdges(connectingEdgesA, sharedVerts);
        HashSet<Integer> connectingVertsB = connectingVerticesFromConnectingEdges(connectingEdgesB, sharedVerts);

        Graphics2D g = img.createGraphics();
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        g.transform(at);

        // translucent fills
        if(a != null){
            g.setColor(new Color(76, 132, 255, 80));
            g.fill(a);
        }
        if(b != null){
            g.setColor(new Color(255, 112, 67, 80));
            g.fill(b);
        }

        g.setStroke(new BasicStroke((float)(1.0 / Math.max(scale, 1e-6))));

        // connecting edges (orange)
        g.setColor(new Color(255, 152, 0));
        drawEdgeListByVertexIds(g, connectingEdgesA);
        drawEdgeListByVertexIds(g, connectingEdgesB);

        // shared edges (dark red)
        g.setColor(new Color(183, 28, 28));
        drawEdgeKeys(g, sharedEdges);

        // vertices (draw as fixed-size dots ~ 3px)
        double rWorld = 3.0 / Math.max(scale, 1e-6);

        // connecting vertices (yellow)
        g.setColor(new Color(255, 235, 59));
        drawVertexDots(g, connectingVertsA, rWorld);
        drawVertexDots(g, connectingVertsB, rWorld);

        // shared vertices (red)
        g.setColor(new Color(244, 67, 54));
        drawVertexDots(g, sharedVerts, rWorld);

        g.dispose();
        return new ImageIcon(img);
    }

    // ---------- low-level helpers ----------

    private static void drawCenteredString(Graphics2D g, String s, int cx, int cy){
        FontMetrics fm = g.getFontMetrics();
        int w = fm.stringWidth(s);
        int h = fm.getAscent();
        g.drawString(s, cx - w/2, cy + h/2 - 2);
    }

    private HashSet<Long> polygonEdgeKeys(int polyId){
        HashSet<Long> out = new HashSet<>();
        if(polygons == null || polyId < 0 || polyId >= polygons.size()) return out;
        List<Integer> ids = polygons.get(polyId);
        if(ids == null || ids.size() < 2) return out;

        for(int i=0; i<ids.size(); i++){
            int v1 = ids.get(i);
            int v2 = ids.get((i+1) % ids.size());
            out.add(edgeKey(v1, v2));
        }
        return out;
    }

    private void drawPolygonEdgesSkippingShared(Graphics2D g, int polyId, HashSet<Long> otherEdges){
        if(polygons == null || polyId < 0 || polyId >= polygons.size()) return;
        List<Integer> ids = polygons.get(polyId);
        if(ids == null || ids.size() < 2) return;

        for(int i=0; i<ids.size(); i++){
            int v1 = ids.get(i);
            int v2 = ids.get((i+1) % ids.size());
            long ek = edgeKey(v1, v2);
            if(otherEdges != null && otherEdges.contains(ek)) continue;

            Point p1 = vertexPoints.get(v1);
            Point p2 = vertexPoints.get(v2);
            g.drawLine(p1.x, p1.y, p2.x, p2.y);
        }
    }

    private HashSet<Integer> polygonSharedVertices(int polyA, int polyB){
        HashSet<Integer> a = new HashSet<>();
        HashSet<Integer> b = new HashSet<>();
        if(polygons == null) return new HashSet<>();
        if(polyA < 0 || polyA >= polygons.size()) return new HashSet<>();
        if(polyB < 0 || polyB >= polygons.size()) return new HashSet<>();

        List<Integer> pa = polygons.get(polyA);
        List<Integer> pb = polygons.get(polyB);
        if(pa != null) a.addAll(pa);
        if(pb != null) b.addAll(pb);

        a.retainAll(b);
        return a;
    }

    private ArrayList<long[]> polygonConnectingEdges(int polyId, HashSet<Integer> sharedVerts){
        ArrayList<long[]> out = new ArrayList<>();
        if(polygons == null || polyId < 0 || polyId >= polygons.size()) return out;
        List<Integer> ids = polygons.get(polyId);
        if(ids == null || ids.size() < 2) return out;

        for(int i=0; i<ids.size(); i++){
            int v1 = ids.get(i);
            int v2 = ids.get((i+1) % ids.size());
            boolean s1 = sharedVerts.contains(v1);
            boolean s2 = sharedVerts.contains(v2);
            if(s1 ^ s2){
                out.add(new long[]{v1, v2});
            }
        }
        return out;
    }

    private static HashSet<Integer> connectingVerticesFromConnectingEdges(ArrayList<long[]> edges, HashSet<Integer> sharedVerts){
        HashSet<Integer> out = new HashSet<>();
        for(long[] e : edges){
            int v1 = (int)e[0];
            int v2 = (int)e[1];
            if(!sharedVerts.contains(v1)) out.add(v1);
            if(!sharedVerts.contains(v2)) out.add(v2);
        }
        return out;
    }

    private void drawEdgeListByVertexIds(Graphics2D g, ArrayList<long[]> edges){
        for(long[] e : edges){
            int v1 = (int)e[0];
            int v2 = (int)e[1];
            Point p1 = vertexPoints.get(v1);
            Point p2 = vertexPoints.get(v2);
            g.drawLine(p1.x, p1.y, p2.x, p2.y);
        }
    }

    private void drawEdgeKeys(Graphics2D g, HashSet<Long> edgeKeys){
        for(long ek : edgeKeys){
            int v1 = (int)(ek >> 32);
            int v2 = (int)(ek & 0xffffffffL);
            Point p1 = vertexPoints.get(v1);
            Point p2 = vertexPoints.get(v2);
            g.drawLine(p1.x, p1.y, p2.x, p2.y);
        }
    }

    private void drawVertexDots(Graphics2D g, Collection<Integer> vids, double rWorld){
        for(Integer vid : vids){
            if(vid == null) continue;
            Point p = vertexPoints.get(vid);
            double x = p.x - rWorld;
            double y = p.y - rWorld;
            g.fill(new java.awt.geom.Ellipse2D.Double(x, y, 2*rWorld, 2*rWorld));
        }
    }

    // =====================================================
    // Neighbor Pair Geometry (49 features) - Qt-equivalent
    // Ported from qtproj/neighborgeometrycalculator.cpp
    // =====================================================

    static class NeighborPairGeometryResult {
        int firstPolygonId;
        int secondPolygonId;

        // 1) Area
        double areaRatio = -1.0;
        double areaMean = Double.NaN, areaMin = Double.NaN, areaMax = Double.NaN, areaDiff = Double.NaN;

        // 2) Perimeter
        double perimeterRatio = -1.0;
        double perimeterMean = Double.NaN, perimeterMin = Double.NaN, perimeterMax = Double.NaN, perimeterDiff = Double.NaN;

        // 3) Aspect ratio
        double aspectRatio = -1.0;
        double aspectMean = Double.NaN, aspectMin = Double.NaN, aspectMax = Double.NaN, aspectDiff = Double.NaN;

        // 4) Circularity
        double circularityRatio = -1.0;
        double circularityMean = Double.NaN, circularityMin = Double.NaN, circularityMax = Double.NaN, circularityDiff = Double.NaN;

        // 5) Solidity
        double solidityRatio = -1.0;
        double solidityMean = Double.NaN, solidityMin = Double.NaN, solidityMax = Double.NaN, solidityDiff = Double.NaN;

        // 6) Vertex count
        double vertexCountRatio = -1.0;
        double vertexCountMean = Double.NaN, vertexCountMin = Double.NaN, vertexCountMax = Double.NaN, vertexCountDiff = Double.NaN;

        // 7) Centroid distance
        double centroidDistance = -1.0;
        double centroidDistanceNormalized = Double.NaN;

        // 8) Union geometry
        double unionAspectRatio = Double.NaN;
        double unionCircularity = Double.NaN;
        double unionConvexDeficiency = Double.NaN;

        // 9) Shared edge metrics
        double normalizedSharedEdgeLength = -1.0;
        double sharedEdgeLength = Double.NaN; // total shared edge length (Qt records only if hasSharedEdge && longestEdgeLen>0)

        double sharedEdgeUnsharedVerticesDistance = Double.NaN;
        double sharedEdgeUnsharedVerticesDistanceNormalized = Double.NaN;

        double centroidSharedEdgeDistance = Double.NaN;
        double centroidSharedEdgeDistanceNormalized = Double.NaN;

        // 10) Junction angles
        double junctionAngleAverageDegrees = Double.NaN;
        double junctionAngleMaxDegrees = Double.NaN;
        double junctionAngleMinDegrees = Double.NaN;
        double junctionAngleDifferenceDegrees = Double.NaN;
        double junctionAngleRatio = Double.NaN;

        // 11) Shared edge vs union geometry
        double sharedEdgeUnionCentroidDistance = Double.NaN;
        double sharedEdgeUnionCentroidDistanceNormalized = Double.NaN;
        double sharedEdgeUnionAxisAngleDegrees = Double.NaN;
    }

    // ---- Main entry (compute all 49 features, Qt logic) ----
    private NeighborPairGeometryResult calculateGeometryForPair(int polyA, int polyB){

        NeighborPairGeometryResult res = new NeighborPairGeometryResult();
        res.firstPolygonId = polyA;
        res.secondPolygonId = polyB;

        if(polygons == null || vertexPoints == null) return res;
        if(polyA < 0 || polyA >= polygons.size()) return res;
        if(polyB < 0 || polyB >= polygons.size()) return res;

        List<Integer> idsA = polygons.get(polyA);
        List<Integer> idsB = polygons.get(polyB);
        if(idsA == null || idsB == null || idsA.size() < 3 || idsB.size() < 3) return res;

        // --- polygon scalar geometry (Qt PolygonItem::area/perimeter/centroid etc.) ---
        double areaA = polygonAreaAbs(idsA);
        double areaB = polygonAreaAbs(idsB);

        double perimeterA = polygonPerimeter(idsA);
        double perimeterB = polygonPerimeter(idsB);

        double aspectA = aspectRatioForPolygon(idsA);          // Qt: bounds.width/height, >=1
        double aspectB = aspectRatioForPolygon(idsB);

        double circA = circularityForPolygon(areaA, perimeterA); // Qt: 4*pi*area/per^2, invalid -> -1
        double circB = circularityForPolygon(areaB, perimeterB);

        double solA = solidityForPolygon(idsA);                // Qt: area / convexHullArea, invalid -> -1
        double solB = solidityForPolygon(idsB);

        double vCountA = (double) idsA.size();
        double vCountB = (double) idsB.size();

        // Qt avgSqrtArea = 0.5*(sqrt(areaA)+sqrt(areaB))
        double avgSqrtArea = 0.5 * (Math.sqrt(areaA) + Math.sqrt(areaB));

        // --- ratio + stats (Qt safeRatio + computeStats) ---
        res.areaRatio = safeRatio(areaA, areaB);
        computeStats(areaA, areaB, v -> res.areaMean = v, v -> res.areaMin = v, v -> res.areaMax = v, v -> res.areaDiff = v);

        res.perimeterRatio = safeRatio(perimeterA, perimeterB);
        computeStats(perimeterA, perimeterB, v -> res.perimeterMean = v, v -> res.perimeterMin = v, v -> res.perimeterMax = v, v -> res.perimeterDiff = v);

        res.aspectRatio = safeRatio(aspectA, aspectB);
        computeStats(aspectA, aspectB, v -> res.aspectMean = v, v -> res.aspectMin = v, v -> res.aspectMax = v, v -> res.aspectDiff = v);

        res.circularityRatio = safeRatio(circA, circB);
        computeStats(circA, circB, v -> res.circularityMean = v, v -> res.circularityMin = v, v -> res.circularityMax = v, v -> res.circularityDiff = v);

        res.solidityRatio = safeRatio(solA, solB);
        computeStats(solA, solB, v -> res.solidityMean = v, v -> res.solidityMin = v, v -> res.solidityMax = v, v -> res.solidityDiff = v);

        res.vertexCountRatio = safeRatio(vCountA, vCountB);
        computeStats(vCountA, vCountB, v -> res.vertexCountMean = v, v -> res.vertexCountMin = v, v -> res.vertexCountMax = v, v -> res.vertexCountDiff = v);

        // --- centroid distance (Qt PolygonItem::centroid has fallback avg if near-zero area) ---
        Point2D.Double centA = polygonCentroid_PolygonItemLike(idsA);
        Point2D.Double centB = polygonCentroid_PolygonItemLike(idsB);
        res.centroidDistance = dist(centA, centB);
        if(Double.isFinite(res.centroidDistance) && avgSqrtArea > 0.0){
            res.centroidDistanceNormalized = res.centroidDistance / avgSqrtArea;
        }

        // --- union geometry (Qt uses QPainterPath unionPath.united + toFillPolygons) ---
        // We do the same conceptually with java.awt.geom.Area union and then extract fill polygons from its path.
        Area union = new Area(polygonPath(polyA));
        union.add(new Area(polygonPath(polyB)));

        Rectangle2D unionBounds = union.getBounds2D();
        if(unionBounds != null && unionBounds.getWidth() > 0.0 && unionBounds.getHeight() > 0.0){
            double ratio = unionBounds.getWidth() / unionBounds.getHeight();
            res.unionAspectRatio = (ratio >= 1.0) ? ratio : (1.0 / ratio);
        }

        // Extract union fill polygons (Qt: unionPath.simplified().toFillPolygons())
        List<List<Point2D.Double>> unionPolys = areaToPolygons(union);

        double unionArea = 0.0;
        double unionPerimeter = 0.0;
        Point2D.Double centroidAccum = new Point2D.Double(0,0);
        ArrayList<Point2D.Double> unionPoints = new ArrayList<>();

        for(List<Point2D.Double> up : unionPolys){
            double a = polygonAreaAbsPts(up); // Qt polygonArea(abs)
            unionArea += a;
            if(a > 0.0){
                Point2D.Double c = polygonCentroid_NoFallback(up); // Qt polygonCentroid() used for union centroid
                centroidAccum.x += c.x * a;
                centroidAccum.y += c.y * a;
            }
            unionPerimeter += polygonPerimeterPts(up);
            unionPoints.addAll(up);
        }

        Point2D.Double unionCentroid = new Point2D.Double(0,0);
        boolean hasUnionCentroid = false;
        if(unionArea > 0.0){
            unionCentroid.x = centroidAccum.x / unionArea;
            unionCentroid.y = centroidAccum.y / unionArea;
            hasUnionCentroid = true;
        }

        if(unionArea > 0.0 && unionPerimeter > 0.0){
            res.unionCircularity = (4.0 * Math.PI * unionArea) / (unionPerimeter * unionPerimeter);
        }

        // unionConvexDeficiency: (hullArea - unionArea)/unionArea
        if(unionArea > 0.0 && !unionPoints.isEmpty()){
            List<Point2D.Double> hull = convexHull(unionPoints); // Qt monotonic chain w/ cross<=0
            double hullArea = polygonAreaAbsPts(hull);
            if(hullArea > 0.0){
                res.unionConvexDeficiency = (hullArea - unionArea) / unionArea;
            }
        }

        // union principal axis (Qt PCA on unionPoints)
        Point2D.Double unionAxis = principalAxisForPoints(unionPoints);
        boolean hasUnionAxis = (unionAxis != null && (Math.abs(unionAxis.x) > 1e-12 || Math.abs(unionAxis.y) > 1e-12));

        // --- NeighborPair shared relationships (Qt NeighborPair) ---
        // shared vertices = intersection
        HashSet<Integer> setA = new HashSet<>(idsA);
        HashSet<Integer> setB = new HashSet<>(idsB);
        HashSet<Integer> sharedVerts = new HashSet<>(setA);
        sharedVerts.retainAll(setB);

        // edgesB keys
        HashSet<Long> edgeKeysB = new HashSet<>();
        for(int i=0; i<idsB.size(); i++){
            int v1 = idsB.get(i);
            int v2 = idsB.get((i+1) % idsB.size());
            edgeKeysB.add(edgeKey(v1, v2));
        }

        // shared edges: iterate edgesA in order (Qt uses edgesA then checks membership in edgeKeysB)
        double sharedEdgeTotalLength = 0.0;
        double recordedSharedEdgeLength = Double.NaN;

        boolean hasSharedEdge = false;
        int longestV1 = -1, longestV2 = -1;
        Point2D.Double longestP1 = null, longestP2 = null;
        double longestLen = 0.0;

        for(int i=0; i<idsA.size(); i++){
            int v1 = idsA.get(i);
            int v2 = idsA.get((i+1) % idsA.size());

            boolean v1Shared = sharedVerts.contains(v1);
            boolean v2Shared = sharedVerts.contains(v2);
            if(!v1Shared || !v2Shared) continue;

            long ek = edgeKey(v1, v2);
            if(!edgeKeysB.contains(ek)) continue;

            // this is a shared edge
            Point p1 = vertexPoints.get(v1);
            Point p2 = vertexPoints.get(v2);
            double len = Math.hypot(p2.x - p1.x, p2.y - p1.y);

            sharedEdgeTotalLength += len;

            if(len > longestLen){
                longestLen = len;
                longestV1 = v1;
                longestV2 = v2;
                longestP1 = new Point2D.Double(p1.x, p1.y);
                longestP2 = new Point2D.Double(p2.x, p2.y);
                hasSharedEdge = true;
            }
        }

        // sharedEdgeUnsharedVerticesDistance + centroidSharedEdgeDistance are based on LONGEST shared edge (Qt)
        double sharedEdgeUnsharedVerticesDistance = Double.NaN;
        double centroidSharedEdgeDistance = Double.NaN;

        if(hasSharedEdge && longestLen > 0.0){
            recordedSharedEdgeLength = sharedEdgeTotalLength;

            ArrayList<Double> dists = new ArrayList<>(idsA.size() + idsB.size());

            // accumulateDistances(poly): all vertices except the 2 endpoints of longest shared edge
            for(int vid : idsA){
                if(vid == longestV1 || vid == longestV2) continue;
                Point p = vertexPoints.get(vid);
                double d = pointToSegmentDistance(new Point2D.Double(p.x, p.y), longestP1, longestP2);
                if(Double.isFinite(d)) dists.add(d);
            }
            for(int vid : idsB){
                if(vid == longestV1 || vid == longestV2) continue;
                Point p = vertexPoints.get(vid);
                double d = pointToSegmentDistance(new Point2D.Double(p.x, p.y), longestP1, longestP2);
                if(Double.isFinite(d)) dists.add(d);
            }

            if(!dists.isEmpty()){
                double sum = 0.0;
                for(double d : dists) sum += d;
                sharedEdgeUnsharedVerticesDistance = sum / dists.size();
            }

            // centroidSharedEdgeDistance: average of distances from each polygon centroid to the segment
            double distA = pointToSegmentDistance(centA, longestP1, longestP2);
            double distB = pointToSegmentDistance(centB, longestP1, longestP2);
            if(Double.isFinite(distA) && Double.isFinite(distB)){
                centroidSharedEdgeDistance = 0.5 * (distA + distB);
            }
        }

        // Normalized shared edge length (Qt)
        if(avgSqrtArea > 0.0 && sharedEdgeTotalLength > 0.0){
            res.normalizedSharedEdgeLength = sharedEdgeTotalLength / avgSqrtArea;
        }
        res.sharedEdgeLength = recordedSharedEdgeLength;

        // sharedEdgeUnsharedVerticesDistance (+ normalized)
        if(Double.isFinite(sharedEdgeUnsharedVerticesDistance)){
            res.sharedEdgeUnsharedVerticesDistance = sharedEdgeUnsharedVerticesDistance;
            if(avgSqrtArea > 0.0){
                res.sharedEdgeUnsharedVerticesDistanceNormalized = sharedEdgeUnsharedVerticesDistance / avgSqrtArea;
            }
        }

        // centroidSharedEdgeDistance (+ normalized)
        if(Double.isFinite(centroidSharedEdgeDistance)){
            res.centroidSharedEdgeDistance = centroidSharedEdgeDistance;
            if(avgSqrtArea > 0.0){
                res.centroidSharedEdgeDistanceNormalized = centroidSharedEdgeDistance / avgSqrtArea;
            }
        }

        // sharedEdgeUnionCentroidDistance uses DISTANCE TO INFINITE LINE (Qt formula), not segment distance!
        double sharedEdgeUnionCentroidDistance = Double.NaN;
        if(hasUnionCentroid && hasSharedEdge && longestLen > 0.0){
            double x0 = unionCentroid.x, y0 = unionCentroid.y;
            double x1 = longestP1.x, y1 = longestP1.y;
            double x2 = longestP2.x, y2 = longestP2.y;

            double numerator = Math.abs((y2 - y1) * x0 - (x2 - x1) * y0 + x2 * y1 - y2 * x1);
            sharedEdgeUnionCentroidDistance = numerator / longestLen;
        }

        if(Double.isFinite(sharedEdgeUnionCentroidDistance)){
            res.sharedEdgeUnionCentroidDistance = sharedEdgeUnionCentroidDistance;
            if(avgSqrtArea > 0.0){
                res.sharedEdgeUnionCentroidDistanceNormalized = sharedEdgeUnionCentroidDistance / avgSqrtArea;
            }
        }

        // sharedEdgeUnionAxisAngleDegrees (Qt: acos(abs(dot)) in degrees)
        if(hasSharedEdge && longestLen > 0.0 && hasUnionAxis){
            double ex = (longestP2.x - longestP1.x);
            double ey = (longestP2.y - longestP1.y);
            double edgeMag = Math.hypot(ex, ey);
            double axisMag = Math.hypot(unionAxis.x, unionAxis.y);

            if(edgeMag > 0.0 && axisMag > 0.0){
                double dot = (ex * unionAxis.x + ey * unionAxis.y) / (edgeMag * axisMag);
                dot = clamp(dot, -1.0, 1.0);
                double angle = Math.acos(Math.abs(dot));
                res.sharedEdgeUnionAxisAngleDegrees = Math.toDegrees(angle);
            }
        }

        // --- Junction angle features (Qt connectingVertexFor + atan2(|cross|, dot)) ---
        // Qt iterates shared vertex list; we just iterate the set (order irrelevant to stats).
        ArrayList<Double> anglesDeg = new ArrayList<>();
        if(sharedVerts.size() >= 2){ // neighbor definition
            for(int shared : sharedVerts){
                int connA = connectingVertexFor(idsA, shared, sharedVerts);
                int connB = connectingVertexFor(idsB, shared, sharedVerts);
                if(connA < 0 || connB < 0) continue;

                Point ps = vertexPoints.get(shared);
                Point pa = vertexPoints.get(connA);
                Point pb = vertexPoints.get(connB);

                double ax = pa.x - ps.x;
                double ay = pa.y - ps.y;
                double bx = pb.x - ps.x;
                double by = pb.y - ps.y;

                double cross = ax * by - ay * bx;
                double dot = ax * bx + ay * by;
                double angleRad = Math.atan2(cross, dot);
                double magnitude = Math.abs(angleRad);

                anglesDeg.add(Math.toDegrees(magnitude));
            }
        }

        if(!anglesDeg.isEmpty()){
            double sum = 0.0;
            double maxA = Double.NEGATIVE_INFINITY;
            double minA = Double.POSITIVE_INFINITY;
            for(double a : anglesDeg){
                sum += a;
                maxA = Math.max(maxA, a);
                minA = Math.min(minA, a);
            }

            res.junctionAngleAverageDegrees = sum / anglesDeg.size();
            res.junctionAngleMaxDegrees = maxA;
            res.junctionAngleMinDegrees = minA;
            res.junctionAngleDifferenceDegrees = maxA - minA;
            if(maxA > 0.0){
                res.junctionAngleRatio = minA / maxA;
            }
        }

        return res;
    }

    // =====================================================
    // Helpers (Qt-equivalent)
    // =====================================================

    private static double safeRatio(double a, double b){
        if(!Double.isFinite(a) || !Double.isFinite(b) || a <= 0.0 || b <= 0.0) return -1.0;
        double maxVal = Math.max(a, b);
        if(maxVal <= 0.0) return -1.0;
        return Math.min(a, b) / maxVal;
    }

    @FunctionalInterface private interface DoubleSetter { void set(double v); }

    private static void computeStats(double a, double b, DoubleSetter mean, DoubleSetter minV, DoubleSetter maxV, DoubleSetter diff){
        if(!Double.isFinite(a) || !Double.isFinite(b)) return;
        double m = 0.5 * (a + b);
        double mn = Math.min(a, b);
        double mx = Math.max(a, b);
        mean.set(m);
        minV.set(mn);
        maxV.set(mx);
        diff.set(mx - mn);
    }

    private static double clamp(double v, double lo, double hi){
        return Math.max(lo, Math.min(hi, v));
    }

    private static double dist(Point2D.Double a, Point2D.Double b){
        return Math.hypot(a.x - b.x, a.y - b.y);
    }

    // ---- Polygon area/perimeter/centroid (matching Qt PolygonItem + calculator) ----

    private double polygonAreaAbs(List<Integer> ids){
        return Math.abs(polygonAreaSigned(ids));
    }

    private double polygonAreaSigned(List<Integer> ids){
        int n = ids.size();
        if(n < 3) return 0.0;
        double s = 0.0;
        for(int i=0; i<n; i++){
            Point p = vertexPoints.get(ids.get(i));
            Point q = vertexPoints.get(ids.get((i+1)%n));
            s += p.x * (double)q.y - q.x * (double)p.y;
        }
        return 0.5 * s;
    }

    private double polygonPerimeter(List<Integer> ids){
        int n = ids.size();
        if(n < 2) return 0.0;
        double per = 0.0;
        for(int i=0; i<n; i++){
            Point p = vertexPoints.get(ids.get(i));
            Point q = vertexPoints.get(ids.get((i+1)%n));
            per += Math.hypot(q.x - p.x, q.y - p.y);
        }
        return per;
    }

    // Qt PolygonItem::centroid (fallback average if |A| < 1e-12)
    private Point2D.Double polygonCentroid_PolygonItemLike(List<Integer> ids){
        int n = ids.size();
        if(n < 3) return new Point2D.Double(0,0);

        double A = polygonAreaSigned(ids);
        if(Math.abs(A) < 1e-12){
            double sx = 0.0, sy = 0.0;
            for(int vid : ids){
                Point p = vertexPoints.get(vid);
                sx += p.x;
                sy += p.y;
            }
            return new Point2D.Double(sx / n, sy / n);
        }

        double cx = 0.0, cy = 0.0;
        for(int i=0; i<n; i++){
            Point p = vertexPoints.get(ids.get(i));
            Point q = vertexPoints.get(ids.get((i+1)%n));
            double cross = p.x * (double)q.y - q.x * (double)p.y;
            cx += (p.x + q.x) * cross;
            cy += (p.y + q.y) * cross;
        }
        cx /= (6.0 * A);
        cy /= (6.0 * A);
        return new Point2D.Double(cx, cy);
    }

    // neighborgeometrycalculator.cpp polygonCentroid() (no fallback; returns (0,0) if |signedArea|<1e-9)
    private static Point2D.Double polygonCentroid_NoFallback(List<Point2D.Double> pts){
        int n = pts.size();
        if(n < 3) return new Point2D.Double(0,0);

        double signedArea = 0.0;
        double cx = 0.0;
        double cy = 0.0;
        for(int i=0; i<n; i++){
            Point2D.Double a = pts.get(i);
            Point2D.Double b = pts.get((i+1)%n);
            double cross = a.x * b.y - b.x * a.y;
            signedArea += cross;
            cx += (a.x + b.x) * cross;
            cy += (a.y + b.y) * cross;
        }
        signedArea *= 0.5;
        if(Math.abs(signedArea) < 1e-9) return new Point2D.Double(0,0);

        cx /= (6.0 * signedArea);
        cy /= (6.0 * signedArea);
        return new Point2D.Double(cx, cy);
    }

    private static double polygonAreaAbsPts(List<Point2D.Double> pts){
        int n = pts.size();
        if(n < 3) return 0.0;
        double s = 0.0;
        for(int i=0; i<n; i++){
            Point2D.Double a = pts.get(i);
            Point2D.Double b = pts.get((i+1)%n);
            s += a.x*b.y - b.x*a.y;
        }
        return Math.abs(s) * 0.5;
    }

    private static double polygonPerimeterPts(List<Point2D.Double> pts){
        int n = pts.size();
        if(n < 2) return 0.0;
        double per = 0.0;
        for(int i=0; i<n; i++){
            Point2D.Double a = pts.get(i);
            Point2D.Double b = pts.get((i+1)%n);
            per += Math.hypot(b.x - a.x, b.y - a.y);
        }
        return per;
    }

    // ---- Aspect, circularity, solidity (Qt-equivalent) ----

    private double aspectRatioForPolygon(List<Integer> ids){
        if(ids == null || ids.size() < 3) return -1.0;

        double minX = Double.POSITIVE_INFINITY, minY = Double.POSITIVE_INFINITY;
        double maxX = Double.NEGATIVE_INFINITY, maxY = Double.NEGATIVE_INFINITY;

        for(int vid : ids){
            Point p = vertexPoints.get(vid);
            minX = Math.min(minX, p.x);
            minY = Math.min(minY, p.y);
            maxX = Math.max(maxX, p.x);
            maxY = Math.max(maxY, p.y);
        }

        double w = maxX - minX;
        double h = maxY - minY;
        if(w <= 0.0 || h <= 0.0) return -1.0;

        double r = w / h;
        return (r >= 1.0) ? r : (1.0 / r);
    }

    private static double circularityForPolygon(double area, double per){
        if(area <= 0.0 || per <= 0.0) return -1.0;
        return (4.0 * Math.PI * area) / (per * per);
    }

    private double solidityForPolygon(List<Integer> ids){
        if(ids == null || ids.size() < 3) return -1.0;

        ArrayList<Point2D.Double> pts = new ArrayList<>(ids.size());
        for(int vid : ids){
            Point p = vertexPoints.get(vid);
            pts.add(new Point2D.Double(p.x, p.y));
        }

        double area = polygonAreaAbs(ids);
        if(area <= 0.0) return -1.0;

        List<Point2D.Double> hull = convexHull(pts);
        double hullArea = polygonAreaAbsPts(hull);
        if(hullArea <= 0.0) return -1.0;

        return area / hullArea;
    }

    // ---- Convex hull (Qt monotonic chain; cross<=0 pop) ----

    private static double cross(Point2D.Double a, Point2D.Double b, Point2D.Double c){
        return (b.x - a.x)*(c.y - a.y) - (b.y - a.y)*(c.x - a.x);
    }

    private static List<Point2D.Double> convexHull(List<Point2D.Double> points){
        if(points == null) return new ArrayList<>();
        ArrayList<Point2D.Double> pts = new ArrayList<>(points);

        if(pts.size() < 3) return pts;

        pts.sort((p1, p2) -> {
            if(p1.x == p2.x) return Double.compare(p1.y, p2.y);
            return Double.compare(p1.x, p2.x);
        });

        ArrayList<Point2D.Double> lower = new ArrayList<>();
        for(Point2D.Double p : pts){
            while(lower.size() >= 2 &&
                    cross(lower.get(lower.size()-2), lower.get(lower.size()-1), p) <= 0){
                lower.remove(lower.size()-1);
            }
            lower.add(p);
        }

        ArrayList<Point2D.Double> upper = new ArrayList<>();
        for(int i=pts.size()-1; i>=0; i--){
            Point2D.Double p = pts.get(i);
            while(upper.size() >= 2 &&
                    cross(upper.get(upper.size()-2), upper.get(upper.size()-1), p) <= 0){
                upper.remove(upper.size()-1);
            }
            upper.add(p);
        }

        // pop last of each (Qt lower.pop_back(); upper.pop_back();)
        if(!lower.isEmpty()) lower.remove(lower.size()-1);
        if(!upper.isEmpty()) upper.remove(upper.size()-1);

        lower.addAll(upper);
        return lower;
    }

    // ---- Union extraction: Area -> list of polygons (like Qt toFillPolygons) ----
    // Note: for your neighbor cell polygons (no overlap), this matches Qt well.

    private static List<List<Point2D.Double>> areaToPolygons(Area area){

        ArrayList<List<Point2D.Double>> out = new ArrayList<>();
        if(area == null) return out;

        // Flattening is a safety net; most unions of straight polygons remain straight.
        PathIterator it = new FlatteningPathIterator(area.getPathIterator(null), 0.25);

        double[] c = new double[6];
        ArrayList<Point2D.Double> cur = null;

        while(!it.isDone()){
            int type = it.currentSegment(c);
            switch(type){
                case PathIterator.SEG_MOVETO:
                    if(cur != null && cur.size() >= 3){
                        normalizeClosedPolygon(cur);
                        if(cur.size() >= 3) out.add(cur);
                    }
                    cur = new ArrayList<>();
                    cur.add(new Point2D.Double(c[0], c[1]));
                    break;

                case PathIterator.SEG_LINETO:
                    if(cur != null){
                        cur.add(new Point2D.Double(c[0], c[1]));
                    }
                    break;

                case PathIterator.SEG_CLOSE:
                    if(cur != null && cur.size() >= 3){
                        normalizeClosedPolygon(cur);
                        if(cur.size() >= 3) out.add(cur);
                    }
                    cur = null;
                    break;

                // FlatteningPathIterator should remove curves, but keep endpoints just in case:
                case PathIterator.SEG_QUADTO:
                    if(cur != null){
                        cur.add(new Point2D.Double(c[2], c[3]));
                    }
                    break;
                case PathIterator.SEG_CUBICTO:
                    if(cur != null){
                        cur.add(new Point2D.Double(c[4], c[5]));
                    }
                    break;
            }
            it.next();
        }

        if(cur != null && cur.size() >= 3){
            normalizeClosedPolygon(cur);
            if(cur.size() >= 3) out.add(cur);
        }

        return out;
    }

    // remove consecutive duplicates, and drop last point if equals first (Qt polygons are implicitly closed)
    private static void normalizeClosedPolygon(ArrayList<Point2D.Double> pts){
        for(int i=pts.size()-1; i>0; i--){
            Point2D.Double a = pts.get(i);
            Point2D.Double b = pts.get(i-1);
            if(Math.abs(a.x - b.x) < 1e-12 && Math.abs(a.y - b.y) < 1e-12){
                pts.remove(i);
            }
        }
        if(pts.size() >= 2){
            Point2D.Double first = pts.get(0);
            Point2D.Double last  = pts.get(pts.size()-1);
            if(Math.abs(first.x - last.x) < 1e-12 && Math.abs(first.y - last.y) < 1e-12){
                pts.remove(pts.size()-1);
            }
        }
    }

    // ---- PCA principal axis (Qt principalAxisForPoints) ----
    private static Point2D.Double principalAxisForPoints(List<Point2D.Double> pts){
        if(pts == null || pts.size() < 2) return new Point2D.Double(0,0);

        double meanX = 0.0, meanY = 0.0;
        for(Point2D.Double p : pts){
            meanX += p.x;
            meanY += p.y;
        }
        meanX /= pts.size();
        meanY /= pts.size();

        double covXX = 0.0, covXY = 0.0, covYY = 0.0;
        for(Point2D.Double p : pts){
            double dx = p.x - meanX;
            double dy = p.y - meanY;
            covXX += dx * dx;
            covXY += dx * dy;
            covYY += dy * dy;
        }

        double a = covXX, b = covXY, c = covYY;
        double trace = a + c;
        double detTerm = Math.sqrt((a - c)*(a - c) + 4.0*b*b);
        double lambdaMax = 0.5 * (trace + detTerm);

        Point2D.Double eigen = new Point2D.Double(lambdaMax - c, b);
        if(Math.abs(eigen.x) < 1e-9 && Math.abs(eigen.y) < 1e-9){
            eigen = new Point2D.Double(b, lambdaMax - a);
        }

        double mag = Math.hypot(eigen.x, eigen.y);
        if(mag <= 0.0) return new Point2D.Double(0,0);

        return new Point2D.Double(eigen.x / mag, eigen.y / mag);
    }

    // ---- Point to segment distance (Qt pointToSegmentDistance) ----
    private static double pointToSegmentDistance(Point2D.Double p, Point2D.Double a, Point2D.Double b){
        double dx = b.x - a.x;
        double dy = b.y - a.y;
        double denom = dx*dx + dy*dy;
        if(denom <= 0.0) return Double.NaN;

        double t = ((p.x - a.x)*dx + (p.y - a.y)*dy) / denom;
        double clamped = clamp(t, 0.0, 1.0);
        double px = a.x + clamped * dx;
        double py = a.y + clamped * dy;

        return Math.hypot(p.x - px, p.y - py);
    }

    // ---- Junction angle helper: Qt connectingVertexFor() ----
    // returns the single "unshared neighbor" of a shared vertex, only when exactly one side is shared.
    private int connectingVertexFor(List<Integer> poly, int shared, HashSet<Integer> sharedIds){
        if(poly == null || poly.size() < 3) return -1;

        int idx = -1;
        for(int i=0; i<poly.size(); i++){
            if(poly.get(i) == shared){
                idx = i;
                break;
            }
        }
        if(idx < 0) return -1;

        int n = poly.size();
        int prev = poly.get((idx - 1 + n) % n);
        int next = poly.get((idx + 1) % n);

        boolean prevShared = sharedIds.contains(prev);
        boolean nextShared = sharedIds.contains(next);

        if(prevShared && !nextShared) return next;
        if(nextShared && !prevShared) return prev;

        return -1;
    }

    private void calculateAllNeighborPairGeometries(JFrame frame){

        if(polygons == null || polygons.size() < 2){
            JOptionPane.showMessageDialog(frame, "Need at least 2 polygons.", "Neighbor Pair Geometrical Calculation",
                    JOptionPane.WARNING_MESSAGE);
            return;
        }

        List<int[]> pairs = (imagePanel == null) ? null : imagePanel.getNeighborPairs();

        // same fallback you already use in preview (Qt rule: >=2 shared vertices)
        if(pairs == null || pairs.isEmpty()){
            pairs = detectNeighborPairsFromPolygons(polygons);
        }

        if(pairs == null || pairs.isEmpty()){
            JOptionPane.showMessageDialog(frame,
                    "No neighboring polygon pairs found.\n(Need pairs sharing >=2 vertices.)",
                    "Neighbor Pair Geometrical Calculation",
                    JOptionPane.INFORMATION_MESSAGE);
            return;
        }

        // normalize + unique + stable sort (same as preview model)
        final ArrayList<int[]> sortedPairs = new ArrayList<>();
        final HashSet<Long> seen = new HashSet<>();
        for(int[] pr : pairs){
            if(pr == null || pr.length < 2) continue;
            int a = Math.min(pr[0], pr[1]);
            int b = Math.max(pr[0], pr[1]);
            long k = pairKey(a, b);
            if(seen.add(k)) sortedPairs.add(new int[]{a, b});
        }
        Collections.sort(sortedPairs, (p1, p2) -> {
            if(p1[0] != p2[0]) return Integer.compare(p1[0], p2[0]);
            return Integer.compare(p1[1], p2[1]);
        });

        if(imagePanel != null){
            imagePanel.setNeighborPairs(new ArrayList<>(sortedPairs));
        }
        updateInfoPanel();

        // progress dialog
        final JDialog prog = new JDialog(frame, "Computing neighbor pair geometries...", true);
        prog.setLayout(new BorderLayout());
        final JProgressBar bar = new JProgressBar(0, sortedPairs.size());
        bar.setStringPainted(true);
        prog.add(bar, BorderLayout.CENTER);

        final JButton cancelBtn = new JButton("Cancel");
        JPanel south = new JPanel(new FlowLayout(FlowLayout.RIGHT));
        south.add(cancelBtn);
        prog.add(south, BorderLayout.SOUTH);

        final HashMap<Long, NeighborPairGeometryResult> computed = new HashMap<>();

        SwingWorker<Void, Integer> worker = new SwingWorker<Void, Integer>(){
            @Override
            protected Void doInBackground(){
                for(int i=0; i<sortedPairs.size(); i++){
                    if(isCancelled()) break;

                    int a = sortedPairs.get(i)[0];
                    int b = sortedPairs.get(i)[1];

                    NeighborPairGeometryResult res = calculateGeometryForPair(a, b); // already exists :contentReference[oaicite:7]{index=7}
                    computed.put(pairKey(a, b), res);

                    publish(i+1);
                }
                return null;
            }

            @Override
            protected void process(List<Integer> chunks){
                int v = chunks.get(chunks.size()-1);
                bar.setValue(v);
                bar.setString(v + " / " + sortedPairs.size());
            }

            @Override
            protected void done(){
                prog.dispose();

                if(isCancelled()){
                    IJ.showStatus("Neighbor pair geometry calculation cancelled.");
                    return;
                }

                // commit to global cache on EDT
                neighborPairGeometryCache.clear();
                neighborPairGeometryCache.putAll(computed);

                IJ.showStatus("Neighbor pair geometries computed: " + computed.size());
                updateInfoPanel();

                // show preview again; it will now include geometry columns
                showNeighborPairPreviewDialog(frame);
            }
        };

        cancelBtn.addActionListener(e -> worker.cancel(true));
        worker.execute();

        prog.setSize(420, 120);
        prog.setLocationRelativeTo(frame);
        prog.setVisible(true);
    }

    // =========================
    // divided pair estimation (threshold + matching)
    // =========================

    private void showNeighborPairEstimationDialog(JFrame frame){

        // Need geometry already computed (same requirement as Qt estimation stage)
        if(polygons == null || polygons.size() < 2){
            JOptionPane.showMessageDialog(frame, "Need at least 2 polygons.", "divided pair estimation",
                    JOptionPane.WARNING_MESSAGE);
            return;
        }

        if(lastEstimationCriterion == null){
            lastEstimationCriterion = new Criterion(); // Qt defaults
        }
        final Criterion criterion = new Criterion();
        criterion.featureKey = lastEstimationCriterion.featureKey;
        criterion.requireAbove = lastEstimationCriterion.requireAbove;
        criterion.threshold = lastEstimationCriterion.threshold;
        criterion.matchingMode = lastEstimationCriterion.matchingMode;

        JDialog dialog = new JDialog(frame, "divided pair estimation (Single Feature)", true);
        dialog.setLayout(new BorderLayout());

        JPanel form = new JPanel(new GridBagLayout());
        GridBagConstraints gc = new GridBagConstraints();
        gc.insets = new Insets(6, 8, 6, 8);
        gc.anchor = GridBagConstraints.WEST;
        gc.fill = GridBagConstraints.HORIZONTAL;
        gc.gridy = 0;

        // Feature combo
        gc.gridx = 0; gc.weightx = 0;
        form.add(new JLabel("Geometry feature:"), gc);
        JComboBox<String> featureCombo = new JComboBox<>(GEOM_FEATURE_COLS);
        featureCombo.setEditable(false);
        featureCombo.setSelectedItem(criterion.featureKey);
        gc.gridx = 1; gc.weightx = 1;
        form.add(featureCombo, gc);

        // Direction combo
        gc.gridy++;
        gc.gridx = 0; gc.weightx = 0;
        form.add(new JLabel("Comparison:"), gc);
        JComboBox<String> dirCombo = new JComboBox<>(new String[]{
                "Above or equal to threshold (>=)",
                "Below or equal to threshold (<=)"
        });
        dirCombo.setSelectedIndex(criterion.requireAbove ? 0 : 1);
        gc.gridx = 1; gc.weightx = 1;
        form.add(dirCombo, gc);

        // Threshold spinner
        gc.gridy++;
        gc.gridx = 0; gc.weightx = 0;
        form.add(new JLabel("Threshold:"), gc);
        SpinnerNumberModel thModel = new SpinnerNumberModel(criterion.threshold, -1e9, 1e9, 0.1);
        JSpinner thresholdSpin = new JSpinner(thModel);
        JSpinner.NumberEditor editor = new JSpinner.NumberEditor(thresholdSpin, "0.####");
        thresholdSpin.setEditor(editor);
        gc.gridx = 1; gc.weightx = 1;
        form.add(thresholdSpin, gc);

        // Matching mode combo
        gc.gridy++;
        gc.gridx = 0; gc.weightx = 0;
        form.add(new JLabel("Matching:"), gc);
        JComboBox<Criterion.MatchingMode> modeCombo = new JComboBox<>(Criterion.MatchingMode.values());
        modeCombo.setSelectedItem(criterion.matchingMode);
        gc.gridx = 1; gc.weightx = 1;
        form.add(modeCombo, gc);

        dialog.add(form, BorderLayout.CENTER);

        JPanel buttons = new JPanel(new FlowLayout(FlowLayout.RIGHT));
        JButton ok = new JButton("OK");
        JButton cancel = new JButton("Cancel");
        buttons.add(ok);
        buttons.add(cancel);
        dialog.add(buttons, BorderLayout.SOUTH);

        ok.addActionListener(e -> {
            criterion.featureKey = (String)featureCombo.getSelectedItem();
            criterion.requireAbove = (dirCombo.getSelectedIndex() == 0);
            criterion.threshold = ((Number)thresholdSpin.getValue()).doubleValue();
            criterion.matchingMode = (Criterion.MatchingMode)modeCombo.getSelectedItem();

            dialog.dispose();

            // run estimation
            runNeighborPairEstimation(frame, criterion);
        });
        cancel.addActionListener(e -> dialog.dispose());

        dialog.setSize(520, 240);
        dialog.setLocationRelativeTo(frame);
        dialog.setVisible(true);
    }

    private void runNeighborPairEstimation(JFrame frame, Criterion criterion){

        // Must have geometry
        List<int[]> pairs = (imagePanel == null) ? null : imagePanel.getNeighborPairs();
        if(pairs == null || pairs.isEmpty()){
            pairs = detectNeighborPairsFromPolygons(polygons);
        }

        if(pairs == null || pairs.isEmpty()){
            JOptionPane.showMessageDialog(frame,
                    "No neighboring polygon pairs found.\n(Need pairs sharing >=2 vertices.)",
                    "divided pair estimation",
                    JOptionPane.INFORMATION_MESSAGE);
            return;
        }

        if(!hasGeometryForPairs(pairs)){
            JOptionPane.showMessageDialog(frame,
                    "Neighbor-pair geometries are not available.\nRun: Process → Neighbor Pair Geometrical Calculation... first.",
                    "divided pair estimation",
                    JOptionPane.WARNING_MESSAGE);
            return;
        }

        int featIdx = featureIndexFromKey(criterion.featureKey);
        if(featIdx < 0){
            JOptionPane.showMessageDialog(frame,
                    "Unknown geometry feature key: " + criterion.featureKey,
                    "divided pair estimation",
                    JOptionPane.ERROR_MESSAGE);
            return;
        }

        // normalize + unique + stable sort (same as geometry calculation)
        final ArrayList<int[]> sortedPairs = new ArrayList<>();
        final HashSet<Long> seen = new HashSet<>();
        for(int[] pr : pairs){
            if(pr == null || pr.length < 2) continue;
            int a = Math.min(pr[0], pr[1]);
            int b = Math.max(pr[0], pr[1]);
            long k = pairKey(a, b);
            if(seen.add(k)) sortedPairs.add(new int[]{a, b});
        }
        Collections.sort(sortedPairs, (p1, p2) -> {
            if(p1[0] != p2[0]) return Integer.compare(p1[0], p2[0]);
            return Integer.compare(p1[1], p2[1]);
        });

        // clear previous estimation + store criterion
        clearNeighborPairEstimationCache();
        lastEstimationCriterion = criterion;

        ArrayList<CandidateMatch> candidates = new ArrayList<>();

        for(int[] pr : sortedPairs){
            int a = pr[0], b = pr[1];
            long key = pairKey(a, b);

            NeighborPairGeometryResult r = neighborPairGeometryCache.get(key);
            Double valObj = geomValueByIndex(r, featIdx);

            EstimationEntry entry = new EstimationEntry();
            if(valObj != null){
                entry.featureValue = valObj.doubleValue();
            }

            boolean okVal = (valObj != null) && Double.isFinite(valObj.doubleValue());
            if(okVal){
                double v = valObj.doubleValue();
                boolean pass = criterion.requireAbove ? (v >= criterion.threshold) : (v <= criterion.threshold);
                entry.passed = pass;
                if(pass){
                    entry.score = criterion.requireAbove ? (v - criterion.threshold) : (criterion.threshold - v);
                    // candidate edge for matching
                    if(Double.isFinite(entry.score)){
                        candidates.add(new CandidateMatch(a, b, entry.score, v));
                    }
                }
            }

            neighborPairEstimationCache.put(key, entry);
        }

        int polygonCount = (polygons == null) ? 0 : polygons.size();
        ArrayList<CandidateMatch> chosen = selectMatches(polygonCount, candidates, criterion.matchingMode);

        int selectedCount = 0;
        for(CandidateMatch m : chosen){
            long k = pairKey(m.first, m.second);
            EstimationEntry e = neighborPairEstimationCache.get(k);
            if(e != null){
                e.selected = true;
                selectedCount++;
            }
        }

        if(imagePanel != null) imagePanel.repaint();
        updateInfoPanel();

        IJ.showStatus("divided pair estimation done. Selected pairs: " + selectedCount);

        // refresh preview to show estimation columns
        showNeighborPairPreviewDialog(frame);
    }

    private static int featureIndexFromKey(String key){
        if(key == null) return -1;
        for(int i=0; i<GEOM_FEATURE_COLS.length; i++){
            if(key.equals(GEOM_FEATURE_COLS[i])) return i;
        }
        return -1;
    }

    // ---- Matching algorithms (Qt: greedyMatching / maximumWeightMatching / selectMatches) ----

    private static ArrayList<CandidateMatch> selectMatches(int polygonCount, ArrayList<CandidateMatch> candidates,
                                                        Criterion.MatchingMode mode){

        if(candidates == null) candidates = new ArrayList<>();

        if(mode == Criterion.MatchingMode.Unconstrained){
            candidates.sort((a, b) -> Double.compare(b.score, a.score));
            return candidates;
        }

        if(mode == Criterion.MatchingMode.GlobalMaximumWeight){
            return maximumWeightMatching(polygonCount, candidates);
        }

        // LocalOptimal (greedy)
        return greedyMatching(polygonCount, candidates);
    }

    private static ArrayList<CandidateMatch> greedyMatching(int polygonCount, ArrayList<CandidateMatch> candidates){

        ArrayList<CandidateMatch> all = new ArrayList<>(candidates);
        all.sort((a, b) -> Double.compare(b.score, a.score));

        boolean[] used = new boolean[Math.max(0, polygonCount)];
        ArrayList<CandidateMatch> chosen = new ArrayList<>();

        for(CandidateMatch cand : all){
            if(cand == null) continue;
            if(cand.first < 0 || cand.second < 0) continue;
            if(cand.first >= polygonCount || cand.second >= polygonCount) continue;
            if(used[cand.first] || used[cand.second]) continue;
            used[cand.first] = true;
            used[cand.second] = true;
            chosen.add(cand);
        }
        return chosen;
    }

    private static ArrayList<CandidateMatch> maximumWeightMatching(int polygonCount, ArrayList<CandidateMatch> candidates){

        if(polygonCount <= 1 || candidates == null || candidates.isEmpty()){
            return new ArrayList<>();
        }

        // Qt: supports up to 63 polygons using a 64-bit mask; fallback to greedy otherwise
        if(polygonCount >= 64){
            //IJ.log("Global maximum weight matching supports up to 63 polygons. Falling back to greedy matching.");
            return greedyMatching(polygonCount, candidates);
        }

        ArrayList<ArrayList<CandidateMatch>> adjacency = new ArrayList<>(polygonCount);
        for(int i=0; i<polygonCount; i++){
            adjacency.add(new ArrayList<>());
        }

        for(CandidateMatch cand : candidates){
            if(cand == null) continue;
            if(cand.first < 0 || cand.second < 0) continue;
            if(cand.first >= polygonCount || cand.second >= polygonCount) continue;

            adjacency.get(cand.first).add(cand);
            adjacency.get(cand.second).add(new CandidateMatch(cand.second, cand.first, cand.score, cand.featureValue));
        }

        class Solver {
            HashMap<Long, Solution> cache = new HashMap<>();

            Solution solve(long usedMask){

                Solution cached = cache.get(usedMask);
                if(cached != null) return cached;

                int firstFree = -1;
                for(int idx=0; idx<polygonCount; idx++){
                    if((usedMask & (1L << idx)) == 0){
                        firstFree = idx;
                        break;
                    }
                }

                if(firstFree == -1){
                    Solution empty = new Solution();
                    cache.put(usedMask, empty);
                    return empty;
                }

                long baseMask = usedMask | (1L << firstFree);

                // option: leave firstFree unmatched
                Solution best = solve(baseMask);

                // try pairing firstFree with each free neighbor
                for(CandidateMatch edge : adjacency.get(firstFree)){
                    int other = edge.second;
                    if(other == firstFree) continue;
                    if((baseMask & (1L << other)) != 0) continue;

                    long newMask = baseMask | (1L << other);
                    Solution candSol = solve(newMask);
                    double total = candSol.weight + edge.score;

                    if(total > best.weight){
                        ArrayList<CandidateMatch> newMatches = new ArrayList<>(candSol.matches);
                        newMatches.add(edge);
                        best = new Solution(total, newMatches);
                    }
                }

                cache.put(usedMask, best);
                return best;
            }
        }

        Solution finalSol = new Solver().solve(0L);
        return finalSol.matches;
    }


    private boolean hasGeometryForPairs(List<int[]> pairs){
        if(pairs == null || pairs.isEmpty()) return false;
        for(int[] pr : pairs){
            if(pr == null || pr.length < 2) return false;
            long k = pairKey(pr[0], pr[1]);
            if(!neighborPairGeometryCache.containsKey(k)) return false;
        }
        return true;
    }

    private boolean hasEstimationForPairs(List<int[]> pairs){
        if(pairs == null || pairs.isEmpty()) return false;
        for(int[] pr : pairs){
            if(pr == null || pr.length < 2) return false;
            long k = pairKey(pr[0], pr[1]);
            if(!neighborPairEstimationCache.containsKey(k)) return false;
        }
        return true;
    }

    private static final String[] GEOM_FEATURE_COLS = new String[]{
            "areaRatio","areaMean","areaMin","areaMax","areaDiff",
            "perimeterRatio","perimeterMean","perimeterMin","perimeterMax","perimeterDiff",
            "aspectRatio","aspectMean","aspectMin","aspectMax","aspectDiff",
            "circularityRatio","circularityMean","circularityMin","circularityMax","circularityDiff",
            "solidityRatio","solidityMean","solidityMin","solidityMax","solidityDiff",
            "vertexCountRatio","vertexCountMean","vertexCountMin","vertexCountMax","vertexCountDiff",
            "centroidDistance","centroidDistanceNormalized",
            "unionAspectRatio","unionCircularity","unionConvexDeficiency",
            "normalizedSharedEdgeLength","sharedEdgeLength",
            "sharedEdgeUnsharedVerticesDistance","sharedEdgeUnsharedVerticesDistanceNormalized",
            "centroidSharedEdgeDistance","centroidSharedEdgeDistanceNormalized",
            "junctionAngleAverageDegrees","junctionAngleMaxDegrees","junctionAngleMinDegrees","junctionAngleDifferenceDegrees","junctionAngleRatio",
            "sharedEdgeUnionCentroidDistance","sharedEdgeUnionCentroidDistanceNormalized","sharedEdgeUnionAxisAngleDegrees"
    };

    private static Double geomValueByIndex(NeighborPairGeometryResult r, int idx){
        if(r == null) return null;
        switch(idx){
            case 0: return r.areaRatio; case 1: return r.areaMean; case 2: return r.areaMin; case 3: return r.areaMax; case 4: return r.areaDiff;
            case 5: return r.perimeterRatio; case 6: return r.perimeterMean; case 7: return r.perimeterMin; case 8: return r.perimeterMax; case 9: return r.perimeterDiff;
            case 10: return r.aspectRatio; case 11: return r.aspectMean; case 12: return r.aspectMin; case 13: return r.aspectMax; case 14: return r.aspectDiff;
            case 15: return r.circularityRatio; case 16: return r.circularityMean; case 17: return r.circularityMin; case 18: return r.circularityMax; case 19: return r.circularityDiff;
            case 20: return r.solidityRatio; case 21: return r.solidityMean; case 22: return r.solidityMin; case 23: return r.solidityMax; case 24: return r.solidityDiff;
            case 25: return r.vertexCountRatio; case 26: return r.vertexCountMean; case 27: return r.vertexCountMin; case 28: return r.vertexCountMax; case 29: return r.vertexCountDiff;
            case 30: return r.centroidDistance; case 31: return r.centroidDistanceNormalized;
            case 32: return r.unionAspectRatio; case 33: return r.unionCircularity; case 34: return r.unionConvexDeficiency;
            case 35: return r.normalizedSharedEdgeLength; case 36: return r.sharedEdgeLength;
            case 37: return r.sharedEdgeUnsharedVerticesDistance; case 38: return r.sharedEdgeUnsharedVerticesDistanceNormalized;
            case 39: return r.centroidSharedEdgeDistance; case 40: return r.centroidSharedEdgeDistanceNormalized;
            case 41: return r.junctionAngleAverageDegrees; case 42: return r.junctionAngleMaxDegrees; case 43: return r.junctionAngleMinDegrees;
            case 44: return r.junctionAngleDifferenceDegrees; case 45: return r.junctionAngleRatio;
            case 46: return r.sharedEdgeUnionCentroidDistance; case 47: return r.sharedEdgeUnionCentroidDistanceNormalized; case 48: return r.sharedEdgeUnionAxisAngleDegrees;
            default: return null;
        }
    }

    private class NeighborPairGeometryTableModel extends AbstractTableModel {

        private final String[] cols;
        private final ArrayList<int[]> pairs;

        NeighborPairGeometryTableModel(List<int[]> pairs){
            this.pairs = new ArrayList<>(pairs);
            Collections.sort(this.pairs, (p1, p2) -> {
                int a1 = Math.min(p1[0], p1[1]);
                int b1 = Math.max(p1[0], p1[1]);
                int a2 = Math.min(p2[0], p2[1]);
                int b2 = Math.max(p2[0], p2[1]);
                if(a1 != a2) return Integer.compare(a1, a2);
                return Integer.compare(b1, b2);
            });

            cols = new String[5 + GEOM_FEATURE_COLS.length];
            cols[0] = "Polygon 1";
            cols[1] = "Polygon 2";
            cols[2] = "Preview";
            cols[3] = "Union Preview";
            cols[4] = "Contact Preview";
            for(int i=0; i<GEOM_FEATURE_COLS.length; i++){
                cols[5+i] = GEOM_FEATURE_COLS[i];
            }
        }

        @Override public int getRowCount(){ return pairs.size(); }
        @Override public int getColumnCount(){ return cols.length; }
        @Override public String getColumnName(int c){ return cols[c]; }

        @Override
        public Class<?> getColumnClass(int columnIndex){
            if(columnIndex >= 2 && columnIndex <= 4) return Icon.class;
            if(columnIndex == 0 || columnIndex == 1) return Integer.class;
            return Double.class;
        }

        @Override
        public Object getValueAt(int rowIndex, int columnIndex){
            int[] pr = pairs.get(rowIndex);
            int a = pr[0], b = pr[1];

            if(columnIndex == 0) return a;
            if(columnIndex == 1) return b;
            if(columnIndex >= 2 && columnIndex <= 4){
                return getPreviewIcon(a, b, columnIndex);
            }

            NeighborPairGeometryResult r = neighborPairGeometryCache.get(pairKey(a, b));
            int featIdx = columnIndex - 5;
            return geomValueByIndex(r, featIdx);
        }
    }

    private class NeighborPairGeometryEstimationTableModel extends AbstractTableModel {

        private final String[] cols;
        private final ArrayList<int[]> pairs;

        NeighborPairGeometryEstimationTableModel(List<int[]> pairs){
            this.pairs = new ArrayList<>(pairs);
            Collections.sort(this.pairs, (p1, p2) -> {
                int a1 = Math.min(p1[0], p1[1]);
                int b1 = Math.max(p1[0], p1[1]);
                int a2 = Math.min(p2[0], p2[1]);
                int b2 = Math.max(p2[0], p2[1]);
                if(a1 != a2) return Integer.compare(a1, a2);
                return Integer.compare(b1, b2);
            });

            int base = 5 + GEOM_FEATURE_COLS.length;
            cols = new String[base + 3];
            cols[0] = "Polygon 1";
            cols[1] = "Polygon 2";
            cols[2] = "Preview";
            cols[3] = "Union Preview";
            cols[4] = "Contact Preview";
            for(int i=0; i<GEOM_FEATURE_COLS.length; i++){
                cols[5+i] = GEOM_FEATURE_COLS[i];
            }

            cols[base + 0] = "EstPassed";
            cols[base + 1] = "EstScore";
            cols[base + 2] = "EstSelected";
        }

        @Override public int getRowCount(){ return pairs.size(); }
        @Override public int getColumnCount(){ return cols.length; }
        @Override public String getColumnName(int c){ return cols[c]; }

        @Override
        public Class<?> getColumnClass(int columnIndex){
            if(columnIndex >= 2 && columnIndex <= 4) return Icon.class;
            if(columnIndex == 0 || columnIndex == 1) return Integer.class;

            int geomStart = 5;
            int geomEnd = geomStart + GEOM_FEATURE_COLS.length - 1;
            if(columnIndex >= geomStart && columnIndex <= geomEnd) return Double.class;

            int base = 5 + GEOM_FEATURE_COLS.length;
            if(columnIndex == base + 0) return Boolean.class;
            if(columnIndex == base + 1) return Double.class;
            if(columnIndex == base + 2) return Boolean.class;

            return Object.class;
        }

        @Override
        public Object getValueAt(int rowIndex, int columnIndex){
            int[] pr = pairs.get(rowIndex);
            int a = pr[0], b = pr[1];

            if(columnIndex == 0) return a;
            if(columnIndex == 1) return b;
            if(columnIndex >= 2 && columnIndex <= 4){
                return getPreviewIcon(a, b, columnIndex);
            }

            int geomStart = 5;
            int geomEnd = geomStart + GEOM_FEATURE_COLS.length - 1;

            if(columnIndex >= geomStart && columnIndex <= geomEnd){
                NeighborPairGeometryResult r = neighborPairGeometryCache.get(pairKey(a, b));
                int featIdx = columnIndex - geomStart;
                return geomValueByIndex(r, featIdx);
            }

            EstimationEntry e = neighborPairEstimationCache.get(pairKey(a, b));
            int base = 5 + GEOM_FEATURE_COLS.length;
            if(e == null){
                if(columnIndex == base + 0) return false;
                if(columnIndex == base + 1) return Double.NaN;
                if(columnIndex == base + 2) return false;
                return null;
            }

            if(columnIndex == base + 0) return e.passed;
            if(columnIndex == base + 1) return e.score;
            if(columnIndex == base + 2) return e.selected;

            return null;
        }
    }

    private static class DoubleCellRenderer extends DefaultTableCellRenderer {
        private final DecimalFormat df = new DecimalFormat("0.######");
        DoubleCellRenderer(){
            setHorizontalAlignment(SwingConstants.RIGHT);
        }
        @Override
        public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected,
                                                    boolean hasFocus, int row, int column){
            super.getTableCellRendererComponent(table, value, isSelected, hasFocus, row, column);
            if(value == null){
                setText("");
            }else if(value instanceof Double){
                double d = (Double)value;
                if(Double.isNaN(d)){
                    setText("");
                }else{
                    setText(df.format(d));
                }
            }else{
                setText(value.toString());
            }
            return this;
        }
    }

    // =========================
    // Compare estimated vs real division (Qt-like metrics)
    // =========================
    private DivisionMetrics lastDivisionMetrics = null;

    private static class DivisionMetrics {
        int estimatedPairs = 0;
        int realPairs = 0;
        int truePositives = 0;
        int falsePositives = 0;
        int falseNegatives = 0;
        int trueNegatives = 0;
        int neighborPairs = 0;

        double precision = -1.0;
        double recall = -1.0;
        double f1 = -1.0;
        double specificity = -1.0;
        double accuracy = -1.0;
    }

    private static double safeRatio(int num, int den){
        if(den <= 0) return -1.0;
        return ((double)num) / ((double)den);
    }

    private static HashSet<Long> normalizePairListToSet(List<int[]> pairs){
        HashSet<Long> set = new HashSet<>();
        if(pairs == null) return set;
        for(int[] pr : pairs){
            if(pr == null || pr.length < 2) continue;
            int a = pr[0], b = pr[1];
            if(a == b) continue;
            int lo = Math.min(a, b);
            int hi = Math.max(a, b);
            long k = (((long)lo) << 32) | (hi & 0xffffffffL);
            set.add(k);
        }
        return set;
    }

    private static HashSet<Long> selectedEstimatedSet(HashMap<Long, EstimationEntry> estCache){
        HashSet<Long> set = new HashSet<>();
        if(estCache == null) return set;
        for(Map.Entry<Long, EstimationEntry> ent : estCache.entrySet()){
            EstimationEntry e = ent.getValue();
            if(e != null && e.selected){
                set.add(ent.getKey());
            }
        }
        return set;
    }

    private void compareEstimatedAndRealDivision(JFrame frame){

        if(polygons == null || polygons.isEmpty()){
            JOptionPane.showMessageDialog(frame,
                    "No polygons available.\nLoad / detect polygons first.",
                    "Compare Estimated and Real Division",
                    JOptionPane.WARNING_MESSAGE);
            return;
        }

        // Universe = all neighbor pairs (same as Qt evaluation)
        List<int[]> pairs = (imagePanel == null) ? null : imagePanel.getNeighborPairs();
        if(pairs == null || pairs.isEmpty()){
            pairs = detectNeighborPairsFromPolygons(polygons);
        }
        if(pairs == null || pairs.isEmpty()){
            JOptionPane.showMessageDialog(frame,
                    "No neighbor pairs found.\nRun Neighbor Detection first.",
                    "Compare Estimated and Real Division",
                    JOptionPane.WARNING_MESSAGE);
            return;
        }

        HashSet<Long> neighborSet = normalizePairListToSet(pairs);

        if(realDivisionPairCache.isEmpty()){
            JOptionPane.showMessageDialog(frame,
                    "No real division pairs loaded.\nImport them first: Import & Export → Import Real Division Pairs...",
                    "Compare Estimated and Real Division",
                    JOptionPane.WARNING_MESSAGE);
            return;
        }

        // Estimated set = selected estimation pairs
        HashSet<Long> estSetAll = selectedEstimatedSet(neighborPairEstimationCache);
        if(estSetAll.isEmpty()){
            JOptionPane.showMessageDialog(frame,
                    "No estimated division pairs selected.\nRun: Process → divided pair estimation... first.",
                    "Compare Estimated and Real Division",
                    JOptionPane.WARNING_MESSAGE);
            return;
        }

        // Filter real/estimated to the neighbor universe, warn about out-of-universe
        int realOutside = 0;
        HashSet<Long> realSet = new HashSet<>();
        for(Map.Entry<Long, RealDivisionEntry> ent : realDivisionPairCache.entrySet()){
            long k = ent.getKey();
            if(neighborSet.contains(k)){
                realSet.add(k);
            }else{
                realOutside++;
            }
        }

        int estOutside = 0;
        HashSet<Long> estSet = new HashSet<>();
        for(long k : estSetAll){
            if(neighborSet.contains(k)){
                estSet.add(k);
            }else{
                estOutside++;
            }
        }

        DivisionMetrics m = new DivisionMetrics();
        m.neighborPairs = neighborSet.size();
        m.realPairs = realSet.size();
        m.estimatedPairs = estSet.size();

        // Compute TP/FP/FN (set operations)
        HashSet<Long> tp = new HashSet<>(estSet);
        tp.retainAll(realSet);

        HashSet<Long> fp = new HashSet<>(estSet);
        fp.removeAll(realSet);

        HashSet<Long> fn = new HashSet<>(realSet);
        fn.removeAll(estSet);

        m.truePositives = tp.size();
        m.falsePositives = fp.size();
        m.falseNegatives = fn.size();

        // TN is within neighbor universe only (Qt style)
        m.trueNegatives = m.neighborPairs - m.truePositives - m.falsePositives - m.falseNegatives;
        if(m.trueNegatives < 0) m.trueNegatives = 0;

        // Metrics
        m.precision = safeRatio(m.truePositives, m.truePositives + m.falsePositives);
        m.recall    = safeRatio(m.truePositives, m.truePositives + m.falseNegatives);
        if(m.precision >= 0.0 && m.recall >= 0.0 && (m.precision + m.recall) > 0.0){
            m.f1 = 2.0 * m.precision * m.recall / (m.precision + m.recall);
        }else{
            m.f1 = -1.0;
        }
        m.specificity = safeRatio(m.trueNegatives, m.trueNegatives + m.falsePositives);
        m.accuracy    = safeRatio(m.truePositives + m.trueNegatives, m.neighborPairs);

        // Build comparison arrow caches for visualization (Qt categories)
        clearComparisonPairCaches();

        for(long k : tp){
            // timing comes from real cache
            RealDivisionEntry src = realDivisionPairCache.get(k);
            RealDivisionEntry dst = new RealDivisionEntry();
            dst.timing = (src == null) ? Double.NaN : src.timing;
            truePositivePairCache.put(k, dst);
        }
        for(long k : fp){
            RealDivisionEntry dst = new RealDivisionEntry();
            dst.timing = Double.NaN;
            falsePositivePairCache.put(k, dst);
        }
        for(long k : fn){
            RealDivisionEntry src = realDivisionPairCache.get(k);
            RealDivisionEntry dst = new RealDivisionEntry();
            dst.timing = (src == null) ? Double.NaN : src.timing;
            falseNegativePairCache.put(k, dst);
        }

        lastDivisionMetrics = m;
        updateInfoPanel();
        if(imagePanel != null) imagePanel.repaint();

        // Report
        String msg =
                "Universe (neighbor pairs): " + m.neighborPairs + "\n" +
                "Real pairs (in-universe): " + m.realPairs + (realOutside>0 ? "   (ignored outside: "+realOutside+")" : "") + "\n" +
                "Estimated pairs (in-universe): " + m.estimatedPairs + (estOutside>0 ? "   (ignored outside: "+estOutside+")" : "") + "\n\n" +
                "TP: " + m.truePositives + "\n" +
                "FP: " + m.falsePositives + "\n" +
                "FN: " + m.falseNegatives + "\n" +
                "TN: " + m.trueNegatives + "\n\n" +
                "Precision: " + (m.precision<0 ? "NA" : String.format(Locale.ROOT,"%.4f", m.precision)) + "\n" +
                "Recall: "    + (m.recall<0 ? "NA" : String.format(Locale.ROOT,"%.4f", m.recall)) + "\n" +
                "F1: "        + (m.f1<0 ? "NA" : String.format(Locale.ROOT,"%.4f", m.f1)) + "\n" +
                "Specificity: "+ (m.specificity<0 ? "NA" : String.format(Locale.ROOT,"%.4f", m.specificity)) + "\n" +
                "Accuracy: "  + (m.accuracy<0 ? "NA" : String.format(Locale.ROOT,"%.4f", m.accuracy)) + "\n\n" +
                "Visualization caches updated:\n" +
                "- TruePositive / FalsePositive / FalseNegative arrows";

        JOptionPane.showMessageDialog(frame, msg,
                "Compare Estimated and Real Division",
                JOptionPane.INFORMATION_MESSAGE);

        IJ.showStatus("Compare done: TP=" + m.truePositives + " FP=" + m.falsePositives + " FN=" + m.falseNegatives
                + " Precision=" + (m.precision<0 ? "NA" : String.format(Locale.ROOT,"%.3f", m.precision))
                + " Recall=" + (m.recall<0 ? "NA" : String.format(Locale.ROOT,"%.3f", m.recall)));
    }


    // =========================
    // Vertex Display Setting
    // =========================
    private void showVertexSettingsDialog(JFrame parent) {

        // Radius control
        SpinnerNumberModel radiusModel = new SpinnerNumberModel(
                vertexRadius,   // initial
                1,              // min
                50,             // max
                1               // step
        );
        JSpinner radiusSpinner = new JSpinner(radiusModel);

        // Color control (button opens JColorChooser)
        JButton colorButton = new JButton("Choose...");
        colorButton.setFocusPainted(false);
        colorButton.setOpaque(true);
        colorButton.setBackground(vertexColor);

        final Color[] chosenColor = new Color[] { vertexColor };

        colorButton.addActionListener(e -> {
            Color c = JColorChooser.showDialog(parent, "Vertex Color", chosenColor[0]);
            if (c != null) {
                chosenColor[0] = c;
                colorButton.setBackground(c);
            }
        });

        JPanel panel = new JPanel(new GridBagLayout());
        GridBagConstraints gc = new GridBagConstraints();
        gc.insets = new Insets(6, 6, 6, 6);
        gc.anchor = GridBagConstraints.WEST;

        gc.gridx = 0; gc.gridy = 0;
        panel.add(new JLabel("Vertex radius (px):"), gc);

        gc.gridx = 1; gc.gridy = 0;
        panel.add(radiusSpinner, gc);

        gc.gridx = 0; gc.gridy = 1;
        panel.add(new JLabel("Vertex color:"), gc);

        gc.gridx = 1; gc.gridy = 1;
        panel.add(colorButton, gc);

        int result = JOptionPane.showConfirmDialog(
                parent,
                panel,
                "Vertex Settings",
                JOptionPane.OK_CANCEL_OPTION,
                JOptionPane.PLAIN_MESSAGE
        );

        if (result == JOptionPane.OK_OPTION) {
            vertexRadius = ((Number) radiusSpinner.getValue()).intValue();
            vertexColor = chosenColor[0];

            if (imagePanel != null) {
                imagePanel.repaint();     // redraw with new radius/color
            }
        }
    }

    // =========================
    // Line Display Setting
    // =========================
    private void showLineSettingsDialog(JFrame parent) {

        // Width control
        SpinnerNumberModel widthModel = new SpinnerNumberModel(
                (double) lineWidth,   // initial
                0.1,                  // min
                50.0,                 // max
                0.1                   // step
        );
        JSpinner widthSpinner = new JSpinner(widthModel);

        // Color control
        JButton colorButton = new JButton("Choose...");
        colorButton.setFocusPainted(false);
        colorButton.setOpaque(true);
        colorButton.setBackground(lineColor);

        final Color[] chosenColor = new Color[] { lineColor };

        colorButton.addActionListener(e -> {
            Color c = JColorChooser.showDialog(parent, "Line Color", chosenColor[0]);
            if (c != null) {
                chosenColor[0] = c;
                colorButton.setBackground(c);
            }
        });

        JPanel panel = new JPanel(new GridBagLayout());
        GridBagConstraints gc = new GridBagConstraints();
        gc.insets = new Insets(6, 6, 6, 6);
        gc.anchor = GridBagConstraints.WEST;

        gc.gridx = 0; gc.gridy = 0;
        panel.add(new JLabel("Line width (px):"), gc);

        gc.gridx = 1; gc.gridy = 0;
        panel.add(widthSpinner, gc);

        gc.gridx = 0; gc.gridy = 1;
        panel.add(new JLabel("Line color:"), gc);

        gc.gridx = 1; gc.gridy = 1;
        panel.add(colorButton, gc);

        int result = JOptionPane.showConfirmDialog(
                parent,
                panel,
                "Line Settings",
                JOptionPane.OK_CANCEL_OPTION,
                JOptionPane.PLAIN_MESSAGE
        );

        if (result == JOptionPane.OK_OPTION) {
            lineWidth = ((Double) widthSpinner.getValue()).floatValue();
            lineColor = chosenColor[0];

            if (imagePanel != null) {
                imagePanel.repaint();
            }
        }
    }

    // =========================
    // Polygon Display Setting
    // =========================
    private void showPolygonSettingsDialog(JFrame parent) {

        JRadioButton uniformBtn = new JRadioButton("Uniform color", !polygonRandomColors);
        JRadioButton randomBtn  = new JRadioButton("Random colors (neighbor-aware)", polygonRandomColors);

        ButtonGroup bg = new ButtonGroup();
        bg.add(uniformBtn);
        bg.add(randomBtn);

        JButton polyColorButton = new JButton("Choose...");
        polyColorButton.setFocusPainted(false);
        polyColorButton.setOpaque(true);
        polyColorButton.setBackground(polygonUniformColor);

        final Color[] chosenPolyColor = new Color[]{ polygonUniformColor };

        polyColorButton.addActionListener(e -> {
            Color c = JColorChooser.showDialog(parent, "Polygon Fill Color", chosenPolyColor[0]);
            if(c != null){
                chosenPolyColor[0] = c;
                polyColorButton.setBackground(c);
            }
        });

        SpinnerNumberModel alphaModel = new SpinnerNumberModel(
                polygonFillAlpha, 0, 255, 5
        );
        JSpinner alphaSpinner = new JSpinner(alphaModel);

        JCheckBox showIdBox = new JCheckBox("Show cell ID (0..)", showCellId);

        SpinnerNumberModel fontModel = new SpinnerNumberModel(
                cellIdFontSize, 6, 200, 1
        );
        JSpinner fontSpinner = new JSpinner(fontModel);

        JButton idColorButton = new JButton("Choose...");
        idColorButton.setFocusPainted(false);
        idColorButton.setOpaque(true);
        idColorButton.setBackground(cellIdColor);

        final Color[] chosenIdColor = new Color[]{ cellIdColor };

        idColorButton.addActionListener(e -> {
            Color c = JColorChooser.showDialog(parent, "Cell ID Color", chosenIdColor[0]);
            if(c != null){
                chosenIdColor[0] = c;
                idColorButton.setBackground(c);
            }
        });

        Runnable updateEnabled = () -> {
            boolean uniform = uniformBtn.isSelected();
            polyColorButton.setEnabled(uniform);

            boolean show = showIdBox.isSelected();
            fontSpinner.setEnabled(show);
            idColorButton.setEnabled(show);
        };

        uniformBtn.addActionListener(e -> updateEnabled.run());
        randomBtn.addActionListener(e -> updateEnabled.run());
        showIdBox.addActionListener(e -> updateEnabled.run());

        updateEnabled.run();

        JPanel panel = new JPanel(new GridBagLayout());
        GridBagConstraints gc = new GridBagConstraints();
        gc.insets = new Insets(6,6,6,6);
        gc.anchor = GridBagConstraints.WEST;

        int row = 0;

        gc.gridx=0; gc.gridy=row; panel.add(new JLabel("Fill mode:"), gc);
        gc.gridx=1; gc.gridy=row; panel.add(uniformBtn, gc);
        gc.gridx=2; gc.gridy=row; panel.add(randomBtn, gc);
        row++;

        gc.gridx=0; gc.gridy=row; panel.add(new JLabel("Uniform fill color:"), gc);
        gc.gridx=1; gc.gridy=row; gc.gridwidth=2; panel.add(polyColorButton, gc);
        gc.gridwidth=1;
        row++;

        gc.gridx=0; gc.gridy=row; panel.add(new JLabel("Fill opacity (0-255):"), gc);
        gc.gridx=1; gc.gridy=row; panel.add(alphaSpinner, gc);
        row++;

        gc.gridx=0; gc.gridy=row; panel.add(new JLabel("Cell ID:"), gc);
        gc.gridx=1; gc.gridy=row; gc.gridwidth=2; panel.add(showIdBox, gc);
        gc.gridwidth=1;
        row++;

        gc.gridx=0; gc.gridy=row; panel.add(new JLabel("Cell ID font size:"), gc);
        gc.gridx=1; gc.gridy=row; panel.add(fontSpinner, gc);
        row++;

        gc.gridx=0; gc.gridy=row; panel.add(new JLabel("Cell ID color:"), gc);
        gc.gridx=1; gc.gridy=row; panel.add(idColorButton, gc);
        row++;

        int result = JOptionPane.showConfirmDialog(
                parent,
                panel,
                "Polygon Settings",
                JOptionPane.OK_CANCEL_OPTION,
                JOptionPane.PLAIN_MESSAGE
        );

        if(result == JOptionPane.OK_OPTION){

            polygonRandomColors = randomBtn.isSelected();
            polygonUniformColor = chosenPolyColor[0];
            polygonFillAlpha = ((Number)alphaSpinner.getValue()).intValue();

            showCellId = showIdBox.isSelected();
            cellIdFontSize = ((Number)fontSpinner.getValue()).intValue();
            cellIdColor = chosenIdColor[0];

            if(imagePanel != null){
                imagePanel.refreshPolygonColors();
                imagePanel.repaint();
            }
        }
    }

    // ========================
    // Polygon neighbor pairs link display settings
    // ========================
    private void showNeighborLinkSettings(JFrame frame){

        final JDialog dialog = new JDialog(frame, "Neighbor Link Settings", true);
        dialog.setLayout(new BorderLayout());

        // current values
        final Color[] chosenColor = new Color[]{ imagePanel.getNeighborLinkColor() };

        JCheckBox cbShow = new JCheckBox("Show neighbor links", imagePanel.isShowNeighborLinks());

        JSpinner spWidth = new JSpinner(new SpinnerNumberModel(
                (double) imagePanel.getNeighborLinkWidth(),
                0.1,   // min
                30.0,  // max
                0.1    // step
        ));

        JButton btnColor = new JButton("Choose Color...");
        JPanel colorPreview = new JPanel();
        colorPreview.setPreferredSize(new Dimension(40, 18));
        colorPreview.setBackground(chosenColor[0]);

        btnColor.addActionListener(new ActionListener(){
            @Override public void actionPerformed(ActionEvent e){
                Color c = JColorChooser.showDialog(dialog, "Neighbor Link Color", chosenColor[0]);
                if(c != null){
                    chosenColor[0] = c;
                    colorPreview.setBackground(c);
                }
            }
        });

        JPanel center = new JPanel(new GridBagLayout());
        GridBagConstraints gc = new GridBagConstraints();
        gc.gridx = 0; gc.gridy = 0;
        gc.insets = new Insets(6, 8, 6, 8);
        gc.anchor = GridBagConstraints.WEST;

        center.add(cbShow, gc);

        gc.gridy++;
        center.add(new JLabel("Line width:"), gc);
        gc.gridx = 1;
        center.add(spWidth, gc);

        gc.gridx = 0; gc.gridy++;
        center.add(new JLabel("Color:"), gc);
        gc.gridx = 1;
        JPanel colorRow = new JPanel(new FlowLayout(FlowLayout.LEFT, 6, 0));
        colorRow.add(btnColor);
        colorRow.add(colorPreview);
        center.add(colorRow, gc);

        dialog.add(center, BorderLayout.CENTER);

        JButton ok = new JButton("OK");
        JButton cancel = new JButton("Cancel");

        ok.addActionListener(new ActionListener(){
            @Override public void actionPerformed(ActionEvent e){
                imagePanel.setShowNeighborLinks(cbShow.isSelected());

                double w = (Double) spWidth.getValue();
                imagePanel.setNeighborLinkWidth((float) w);

                imagePanel.setNeighborLinkColor(chosenColor[0]);

                dialog.dispose();
            }
        });

        cancel.addActionListener(new ActionListener(){
            @Override public void actionPerformed(ActionEvent e){
                dialog.dispose();
            }
        });

        JPanel bottom = new JPanel(new FlowLayout(FlowLayout.RIGHT));
        bottom.add(ok);
        bottom.add(cancel);
        dialog.add(bottom, BorderLayout.SOUTH);

        dialog.pack();
        dialog.setLocationRelativeTo(frame);
        dialog.setVisible(true);
    }

    // ========================
    // Estimated division arrow display settings
    // ========================
    private void showDivisionArrowSettings(JFrame frame){

        final JDialog dialog = new JDialog(frame, "Division Arrow Settings", true);
        dialog.setLayout(new BorderLayout());

        // Local copies (Cancel won't change anything)
        final Color[] estColor  = new Color[]{ divisionArrowColor };
        final Color[] realColor = new Color[]{ realDivisionArrowColor };
        final Color[] tpColor   = new Color[]{ truePositiveArrowColor };
        final Color[] fpColor   = new Color[]{ falsePositiveArrowColor };
        final Color[] fnColor   = new Color[]{ falseNegativeArrowColor };

        // ====== Helper: build one section panel ======
        class SectionWidgets {
            JCheckBox cbShow;
            JComboBox<DivisionArrowType> typeCombo;
            JSpinner spWidth, spHeadLen, spHeadWid;
            JPanel colorPreview;
        }

        java.util.function.Function<String, JPanel> titledPanel = (title) -> {
            JPanel p = new JPanel(new GridBagLayout());
            p.setBorder(BorderFactory.createTitledBorder(
                    BorderFactory.createEtchedBorder(), title,
                    TitledBorder.LEFT, TitledBorder.TOP
            ));
            return p;
        };

        java.util.function.BiFunction<JPanel, GridBagConstraints, SectionWidgets> buildSection =
                (panel, g) -> {
                    SectionWidgets w = new SectionWidgets();

                    g.gridx = 0; g.gridy = 0; g.gridwidth = 2;
                    // placeholder checkbox will be attached by caller
                    g.gridwidth = 1;
                    return w;
                };

        // ====== Main container (scrollable, because 5 sections) ======
        JPanel main = new JPanel();
        main.setLayout(new BoxLayout(main, BoxLayout.Y_AXIS));

        Insets in = new Insets(6, 8, 6, 8);

        // ---------- 1) Estimated ----------
        JPanel estPanel = titledPanel.apply("Estimated division arrows (prediction)");
        GridBagConstraints ge = new GridBagConstraints();
        ge.insets = in;
        ge.anchor = GridBagConstraints.WEST;
        ge.fill = GridBagConstraints.HORIZONTAL;
        ge.gridx = 0; ge.gridy = 0;

        JCheckBox cbShowEst = new JCheckBox("Show estimated arrows", showDivisionArrows);
        ge.gridwidth = 2;
        estPanel.add(cbShowEst, ge);
        ge.gridwidth = 1;

        ge.gridy++;
        estPanel.add(new JLabel("Arrow type:"), ge);
        JComboBox<DivisionArrowType> estTypeCombo = new JComboBox<>(DivisionArrowType.values());
        estTypeCombo.setSelectedItem(divisionArrowType);
        ge.gridx = 1;
        estPanel.add(estTypeCombo, ge);

        ge.gridx = 0; ge.gridy++;
        estPanel.add(new JLabel("Line width:"), ge);
        JSpinner estWidth = new JSpinner(new SpinnerNumberModel((double)divisionArrowWidth, 0.1, 30.0, 0.1));
        ge.gridx = 1;
        estPanel.add(estWidth, ge);

        ge.gridx = 0; ge.gridy++;
        estPanel.add(new JLabel("Head length:"), ge);
        JSpinner estHeadLen = new JSpinner(new SpinnerNumberModel((double)divisionArrowHeadLength, 0.0, 200.0, 0.5));
        ge.gridx = 1;
        estPanel.add(estHeadLen, ge);

        ge.gridx = 0; ge.gridy++;
        estPanel.add(new JLabel("Head width:"), ge);
        JSpinner estHeadWid = new JSpinner(new SpinnerNumberModel((double)divisionArrowHeadWidth, 0.0, 200.0, 0.5));
        ge.gridx = 1;
        estPanel.add(estHeadWid, ge);

        ge.gridx = 0; ge.gridy++;
        estPanel.add(new JLabel("Color:"), ge);
        ge.gridx = 1;
        JButton estColorBtn = new JButton("Choose...");
        JPanel estColorPreview = new JPanel();
        estColorPreview.setPreferredSize(new Dimension(40, 18));
        estColorPreview.setBackground(estColor[0]);
        JPanel estColorRow = new JPanel(new FlowLayout(FlowLayout.LEFT, 6, 0));
        estColorRow.add(estColorBtn);
        estColorRow.add(estColorPreview);
        estPanel.add(estColorRow, ge);

        estColorBtn.addActionListener(e -> {
            Color c = JColorChooser.showDialog(dialog, "Estimated Arrow Color", estColor[0]);
            if(c != null){
                estColor[0] = c;
                estColorPreview.setBackground(c);
            }
        });

        // ---------- 2) Real ----------
        JPanel realPanel = titledPanel.apply("Real division arrows (ground truth)");
        GridBagConstraints gr = new GridBagConstraints();
        gr.insets = in;
        gr.anchor = GridBagConstraints.WEST;
        gr.fill = GridBagConstraints.HORIZONTAL;
        gr.gridx = 0; gr.gridy = 0;

        JCheckBox cbShowReal = new JCheckBox("Show real arrows", showRealDivisionArrows);
        gr.gridwidth = 2;
        realPanel.add(cbShowReal, gr);
        gr.gridwidth = 1;

        gr.gridy++;
        realPanel.add(new JLabel("Arrow type:"), gr);
        JComboBox<DivisionArrowType> realTypeCombo = new JComboBox<>(DivisionArrowType.values());
        realTypeCombo.setSelectedItem(realDivisionArrowType);
        gr.gridx = 1;
        realPanel.add(realTypeCombo, gr);

        gr.gridx = 0; gr.gridy++;
        realPanel.add(new JLabel("Line width:"), gr);
        JSpinner realWidth = new JSpinner(new SpinnerNumberModel((double)realDivisionArrowWidth, 0.1, 30.0, 0.1));
        gr.gridx = 1;
        realPanel.add(realWidth, gr);

        gr.gridx = 0; gr.gridy++;
        realPanel.add(new JLabel("Head length:"), gr);
        JSpinner realHeadLen = new JSpinner(new SpinnerNumberModel((double)realDivisionArrowHeadLength, 0.0, 200.0, 0.5));
        gr.gridx = 1;
        realPanel.add(realHeadLen, gr);

        gr.gridx = 0; gr.gridy++;
        realPanel.add(new JLabel("Head width:"), gr);
        JSpinner realHeadWid = new JSpinner(new SpinnerNumberModel((double)realDivisionArrowHeadWidth, 0.0, 200.0, 0.5));
        gr.gridx = 1;
        realPanel.add(realHeadWid, gr);

        gr.gridx = 0; gr.gridy++;
        realPanel.add(new JLabel("Color:"), gr);
        gr.gridx = 1;
        JButton realColorBtn = new JButton("Choose...");
        JPanel realColorPreview = new JPanel();
        realColorPreview.setPreferredSize(new Dimension(40, 18));
        realColorPreview.setBackground(realColor[0]);
        JPanel realColorRow = new JPanel(new FlowLayout(FlowLayout.LEFT, 6, 0));
        realColorRow.add(realColorBtn);
        realColorRow.add(realColorPreview);
        realPanel.add(realColorRow, gr);

        realColorBtn.addActionListener(e -> {
            Color c = JColorChooser.showDialog(dialog, "Real Arrow Color", realColor[0]);
            if(c != null){
                realColor[0] = c;
                realColorPreview.setBackground(c);
            }
        });

        // ---------- 3) True Positive ----------
        JPanel tpPanel = titledPanel.apply("True Positive (TP) arrows");
        GridBagConstraints gt = new GridBagConstraints();
        gt.insets = in;
        gt.anchor = GridBagConstraints.WEST;
        gt.fill = GridBagConstraints.HORIZONTAL;
        gt.gridx = 0; gt.gridy = 0;

        JCheckBox cbShowTP = new JCheckBox("Show TP arrows", showTruePositiveDivisionArrows);
        gt.gridwidth = 2;
        tpPanel.add(cbShowTP, gt);
        gt.gridwidth = 1;

        gt.gridy++;
        tpPanel.add(new JLabel("Arrow type:"), gt);
        JComboBox<DivisionArrowType> tpTypeCombo = new JComboBox<>(DivisionArrowType.values());
        tpTypeCombo.setSelectedItem(truePositiveArrowType);
        gt.gridx = 1;
        tpPanel.add(tpTypeCombo, gt);

        gt.gridx = 0; gt.gridy++;
        tpPanel.add(new JLabel("Line width:"), gt);
        JSpinner tpWidth = new JSpinner(new SpinnerNumberModel((double)truePositiveArrowWidth, 0.1, 30.0, 0.1));
        gt.gridx = 1;
        tpPanel.add(tpWidth, gt);

        gt.gridx = 0; gt.gridy++;
        tpPanel.add(new JLabel("Head length:"), gt);
        JSpinner tpHeadLen = new JSpinner(new SpinnerNumberModel((double)truePositiveArrowHeadLength, 0.0, 200.0, 0.5));
        gt.gridx = 1;
        tpPanel.add(tpHeadLen, gt);

        gt.gridx = 0; gt.gridy++;
        tpPanel.add(new JLabel("Head width:"), gt);
        JSpinner tpHeadWid = new JSpinner(new SpinnerNumberModel((double)truePositiveArrowHeadWidth, 0.0, 200.0, 0.5));
        gt.gridx = 1;
        tpPanel.add(tpHeadWid, gt);

        gt.gridx = 0; gt.gridy++;
        tpPanel.add(new JLabel("Color:"), gt);
        gt.gridx = 1;
        JButton tpColorBtn = new JButton("Choose...");
        JPanel tpColorPreview = new JPanel();
        tpColorPreview.setPreferredSize(new Dimension(40, 18));
        tpColorPreview.setBackground(tpColor[0]);
        JPanel tpColorRow = new JPanel(new FlowLayout(FlowLayout.LEFT, 6, 0));
        tpColorRow.add(tpColorBtn);
        tpColorRow.add(tpColorPreview);
        tpPanel.add(tpColorRow, gt);

        tpColorBtn.addActionListener(e -> {
            Color c = JColorChooser.showDialog(dialog, "TP Arrow Color", tpColor[0]);
            if(c != null){
                tpColor[0] = c;
                tpColorPreview.setBackground(c);
            }
        });

        // ---------- 4) False Positive ----------
        JPanel fpPanel = titledPanel.apply("False Positive (FP) arrows");
        GridBagConstraints gf = new GridBagConstraints();
        gf.insets = in;
        gf.anchor = GridBagConstraints.WEST;
        gf.fill = GridBagConstraints.HORIZONTAL;
        gf.gridx = 0; gf.gridy = 0;

        JCheckBox cbShowFP = new JCheckBox("Show FP arrows", showFalsePositiveDivisionArrows);
        gf.gridwidth = 2;
        fpPanel.add(cbShowFP, gf);
        gf.gridwidth = 1;

        gf.gridy++;
        fpPanel.add(new JLabel("Arrow type:"), gf);
        JComboBox<DivisionArrowType> fpTypeCombo = new JComboBox<>(DivisionArrowType.values());
        fpTypeCombo.setSelectedItem(falsePositiveArrowType);
        gf.gridx = 1;
        fpPanel.add(fpTypeCombo, gf);

        gf.gridx = 0; gf.gridy++;
        fpPanel.add(new JLabel("Line width:"), gf);
        JSpinner fpWidth = new JSpinner(new SpinnerNumberModel((double)falsePositiveArrowWidth, 0.1, 30.0, 0.1));
        gf.gridx = 1;
        fpPanel.add(fpWidth, gf);

        gf.gridx = 0; gf.gridy++;
        fpPanel.add(new JLabel("Head length:"), gf);
        JSpinner fpHeadLen = new JSpinner(new SpinnerNumberModel((double)falsePositiveArrowHeadLength, 0.0, 200.0, 0.5));
        gf.gridx = 1;
        fpPanel.add(fpHeadLen, gf);

        gf.gridx = 0; gf.gridy++;
        fpPanel.add(new JLabel("Head width:"), gf);
        JSpinner fpHeadWid = new JSpinner(new SpinnerNumberModel((double)falsePositiveArrowHeadWidth, 0.0, 200.0, 0.5));
        gf.gridx = 1;
        fpPanel.add(fpHeadWid, gf);

        gf.gridx = 0; gf.gridy++;
        fpPanel.add(new JLabel("Color:"), gf);
        gf.gridx = 1;
        JButton fpColorBtn = new JButton("Choose...");
        JPanel fpColorPreview = new JPanel();
        fpColorPreview.setPreferredSize(new Dimension(40, 18));
        fpColorPreview.setBackground(fpColor[0]);
        JPanel fpColorRow = new JPanel(new FlowLayout(FlowLayout.LEFT, 6, 0));
        fpColorRow.add(fpColorBtn);
        fpColorRow.add(fpColorPreview);
        fpPanel.add(fpColorRow, gf);

        fpColorBtn.addActionListener(e -> {
            Color c = JColorChooser.showDialog(dialog, "FP Arrow Color", fpColor[0]);
            if(c != null){
                fpColor[0] = c;
                fpColorPreview.setBackground(c);
            }
        });

        // ---------- 5) False Negative ----------
        JPanel fnPanel = titledPanel.apply("False Negative (FN) arrows");
        GridBagConstraints gn = new GridBagConstraints();
        gn.insets = in;
        gn.anchor = GridBagConstraints.WEST;
        gn.fill = GridBagConstraints.HORIZONTAL;
        gn.gridx = 0; gn.gridy = 0;

        JCheckBox cbShowFN = new JCheckBox("Show FN arrows", showFalseNegativeDivisionArrows);
        gn.gridwidth = 2;
        fnPanel.add(cbShowFN, gn);
        gn.gridwidth = 1;

        gn.gridy++;
        fnPanel.add(new JLabel("Arrow type:"), gn);
        JComboBox<DivisionArrowType> fnTypeCombo = new JComboBox<>(DivisionArrowType.values());
        fnTypeCombo.setSelectedItem(falseNegativeArrowType);
        gn.gridx = 1;
        fnPanel.add(fnTypeCombo, gn);

        gn.gridx = 0; gn.gridy++;
        fnPanel.add(new JLabel("Line width:"), gn);
        JSpinner fnWidth = new JSpinner(new SpinnerNumberModel((double)falseNegativeArrowWidth, 0.1, 30.0, 0.1));
        gn.gridx = 1;
        fnPanel.add(fnWidth, gn);

        gn.gridx = 0; gn.gridy++;
        fnPanel.add(new JLabel("Head length:"), gn);
        JSpinner fnHeadLen = new JSpinner(new SpinnerNumberModel((double)falseNegativeArrowHeadLength, 0.0, 200.0, 0.5));
        gn.gridx = 1;
        fnPanel.add(fnHeadLen, gn);

        gn.gridx = 0; gn.gridy++;
        fnPanel.add(new JLabel("Head width:"), gn);
        JSpinner fnHeadWid = new JSpinner(new SpinnerNumberModel((double)falseNegativeArrowHeadWidth, 0.0, 200.0, 0.5));
        gn.gridx = 1;
        fnPanel.add(fnHeadWid, gn);

        gn.gridx = 0; gn.gridy++;
        fnPanel.add(new JLabel("Color:"), gn);
        gn.gridx = 1;
        JButton fnColorBtn = new JButton("Choose...");
        JPanel fnColorPreview = new JPanel();
        fnColorPreview.setPreferredSize(new Dimension(40, 18));
        fnColorPreview.setBackground(fnColor[0]);
        JPanel fnColorRow = new JPanel(new FlowLayout(FlowLayout.LEFT, 6, 0));
        fnColorRow.add(fnColorBtn);
        fnColorRow.add(fnColorPreview);
        fnPanel.add(fnColorRow, gn);

        fnColorBtn.addActionListener(e -> {
            Color c = JColorChooser.showDialog(dialog, "FN Arrow Color", fnColor[0]);
            if(c != null){
                fnColor[0] = c;
                fnColorPreview.setBackground(c);
            }
        });

        // ===== Enable/disable head controls based on arrow type =====
        Runnable refreshEnable = () -> {
            boolean estHeads  = ((DivisionArrowType) estTypeCombo.getSelectedItem())  != DivisionArrowType.LineOnly;
            boolean realHeads = ((DivisionArrowType) realTypeCombo.getSelectedItem()) != DivisionArrowType.LineOnly;
            boolean tpHeads   = ((DivisionArrowType) tpTypeCombo.getSelectedItem())   != DivisionArrowType.LineOnly;
            boolean fpHeads   = ((DivisionArrowType) fpTypeCombo.getSelectedItem())   != DivisionArrowType.LineOnly;
            boolean fnHeads   = ((DivisionArrowType) fnTypeCombo.getSelectedItem())   != DivisionArrowType.LineOnly;

            estHeadLen.setEnabled(estHeads);   estHeadWid.setEnabled(estHeads);
            realHeadLen.setEnabled(realHeads); realHeadWid.setEnabled(realHeads);
            tpHeadLen.setEnabled(tpHeads);     tpHeadWid.setEnabled(tpHeads);
            fpHeadLen.setEnabled(fpHeads);     fpHeadWid.setEnabled(fpHeads);
            fnHeadLen.setEnabled(fnHeads);     fnHeadWid.setEnabled(fnHeads);
        };
        refreshEnable.run();

        estTypeCombo.addActionListener(e -> refreshEnable.run());
        realTypeCombo.addActionListener(e -> refreshEnable.run());
        tpTypeCombo.addActionListener(e -> refreshEnable.run());
        fpTypeCombo.addActionListener(e -> refreshEnable.run());
        fnTypeCombo.addActionListener(e -> refreshEnable.run());

        // Add all sections
        main.add(estPanel);
        main.add(realPanel);
        main.add(tpPanel);
        main.add(fpPanel);
        main.add(fnPanel);

        JScrollPane scroll = new JScrollPane(main);
        scroll.setBorder(null);
        dialog.add(scroll, BorderLayout.CENTER);

        // ===== Buttons =====
        JPanel bottom = new JPanel(new FlowLayout(FlowLayout.RIGHT));
        JButton ok = new JButton("OK");
        JButton cancel = new JButton("Cancel");
        bottom.add(ok);
        bottom.add(cancel);
        dialog.add(bottom, BorderLayout.SOUTH);

        ok.addActionListener(e -> {
            // Estimated
            showDivisionArrows = cbShowEst.isSelected();
            divisionArrowType = (DivisionArrowType) estTypeCombo.getSelectedItem();
            divisionArrowWidth = ((Number) estWidth.getValue()).floatValue();
            divisionArrowHeadLength = ((Number) estHeadLen.getValue()).doubleValue();
            divisionArrowHeadWidth  = ((Number) estHeadWid.getValue()).doubleValue();
            divisionArrowColor = estColor[0];

            // Real
            showRealDivisionArrows = cbShowReal.isSelected();
            realDivisionArrowType = (DivisionArrowType) realTypeCombo.getSelectedItem();
            realDivisionArrowWidth = ((Number) realWidth.getValue()).floatValue();
            realDivisionArrowHeadLength = ((Number) realHeadLen.getValue()).doubleValue();
            realDivisionArrowHeadWidth  = ((Number) realHeadWid.getValue()).doubleValue();
            realDivisionArrowColor = realColor[0];

            // TP
            showTruePositiveDivisionArrows = cbShowTP.isSelected();
            truePositiveArrowType = (DivisionArrowType) tpTypeCombo.getSelectedItem();
            truePositiveArrowWidth = ((Number) tpWidth.getValue()).floatValue();
            truePositiveArrowHeadLength = ((Number) tpHeadLen.getValue()).doubleValue();
            truePositiveArrowHeadWidth  = ((Number) tpHeadWid.getValue()).doubleValue();
            truePositiveArrowColor = tpColor[0];

            // FP
            showFalsePositiveDivisionArrows = cbShowFP.isSelected();
            falsePositiveArrowType = (DivisionArrowType) fpTypeCombo.getSelectedItem();
            falsePositiveArrowWidth = ((Number) fpWidth.getValue()).floatValue();
            falsePositiveArrowHeadLength = ((Number) fpHeadLen.getValue()).doubleValue();
            falsePositiveArrowHeadWidth  = ((Number) fpHeadWid.getValue()).doubleValue();
            falsePositiveArrowColor = fpColor[0];

            // FN
            showFalseNegativeDivisionArrows = cbShowFN.isSelected();
            falseNegativeArrowType = (DivisionArrowType) fnTypeCombo.getSelectedItem();
            falseNegativeArrowWidth = ((Number) fnWidth.getValue()).floatValue();
            falseNegativeArrowHeadLength = ((Number) fnHeadLen.getValue()).doubleValue();
            falseNegativeArrowHeadWidth  = ((Number) fnHeadWid.getValue()).doubleValue();
            falseNegativeArrowColor = fnColor[0];

            if(imagePanel != null) imagePanel.repaint();
            dialog.dispose();
        });

        cancel.addActionListener(e -> dialog.dispose());

        dialog.setSize(580, 680);
        dialog.setLocationRelativeTo(frame);
        dialog.setVisible(true);
    }



    // =========================
    // Viewer
    // =========================

    class ImagePanel extends JComponent{

        private BufferedImage image;
        private BufferedImage backgroundImage;
        private double zoom=1.0;

        private List<Point> overlayPoints=new ArrayList<>();
        private List<LineEdge> lines=new ArrayList<>();
        private List<List<Integer>> polygons=new ArrayList<>();
        private List<Integer> polygonDisplayIds = new ArrayList<>();
        private ArrayList<int[]> neighborPairs = new ArrayList<>();

        // ===== Selection ====

        private int selectedVertex=-1;
        private boolean draggingVertex=false;
        private int selectedLine=-1;
        private int selectedPolygon=-1;

        // --- multi-vertex picking for manual line/polygon creation
        private final ArrayList<Integer> pickedVertices = new ArrayList<>();
        private final LinkedHashSet<Integer> debugHighlightedVertices = new LinkedHashSet<>();
        private final LinkedHashSet<Integer> debugHighlightedLines = new LinkedHashSet<>();

        // --- to distinguish click (select) vs drag (move) on a vertex ---
        private int pressedVertex = -1;
        private Point pressPoint = null;

        private boolean draggingImage=false;
        private Point dragStartScreen=null;
        private Point dragStartView=null;

        private final JPopupMenu popup;

        private int clickX,clickY;
        private int mouseX, mouseY;

        // helper for manually adding line and polygon
        private boolean isPicked(int idx){
            return pickedVertices.contains(idx);
        }

        private void togglePickVertex(int idx){
            if(idx < 0) return;
            int pos = pickedVertices.indexOf(idx);
            if(pos >= 0){
                pickedVertices.remove(pos);
            }else{
                pickedVertices.add(idx);
            }
        }

        private void clearPickedVertices(){
            pickedVertices.clear();
        }

        private void clearDebugHighlights(){
            debugHighlightedVertices.clear();
            debugHighlightedLines.clear();
        }

        void setDebugHighlights(Collection<Integer> vertexIndices, Collection<Integer> lineIndices){
            clearDebugHighlights();

            if(vertexIndices != null){
                for(Integer idx : vertexIndices){
                    if(idx != null && idx >= 0 && idx < overlayPoints.size()){
                        debugHighlightedVertices.add(idx);
                    }
                }
            }

            if(lineIndices != null){
                for(Integer idx : lineIndices){
                    if(idx != null && idx >= 0 && idx < lines.size()){
                        debugHighlightedLines.add(idx);
                    }
                }
            }

            repaint();
            updateSelectionInfoLabels();
        }

        private void addVertexAtScreen(int sx, int sy){
            Point p = toImagePixel(sx, sy);
            overlayPoints.add(p);
            repaint();
            updateInfoPanel();
        }

        private void addLineFromPickedVertices(){
            if(pickedVertices.size() != 2) return;

            int a = pickedVertices.get(0);
            int b = pickedVertices.get(1);
            LineEdge ne = new LineEdge(a, b);
            if(!lines.contains(ne)){
                lines.add(ne);
            }

            refreshPolygonColors();
            clearPickedVertices();
            repaint();
            updateInfoPanel();
        }

        private void addPolygonFromPickedVertices(){
            if(pickedVertices.size() < 3) return;

            ArrayList<Integer> poly = buildPolygonOrderFromPicked();
            if(poly.size() >= 3){
                String key = polygonKey(poly);
                boolean exists = false;
                for(List<Integer> p : polygons){
                    if(p != null && p.size() >= 3 && polygonKey(p).equals(key)){
                        exists = true;
                        break;
                    }
                }

                if(!exists){
                    polygons.add(poly);
                    sortPolygonsByTopVertices();
                    refreshPolygonColors();
                }
            }

            clearPickedVertices();
            repaint();
            updateInfoPanel();
        }

        private void deleteAllSelectedItems(){
            TreeSet<Integer> verticesToDelete = new TreeSet<>(Collections.reverseOrder());

            if(selectedVertex >= 0 && selectedVertex < overlayPoints.size()){
                verticesToDelete.add(selectedVertex);
            }
            for(int v : pickedVertices){
                if(v >= 0 && v < overlayPoints.size()){
                    verticesToDelete.add(v);
                }
            }

            for(int v : verticesToDelete){
                deleteVertexAndUpdateTopology(v);
            }

            if(verticesToDelete.isEmpty()){
                if(selectedLine >= 0 && selectedLine < lines.size()){
                    LineEdge removed = lines.remove(selectedLine);
                    selectedLine = -1;
                    if(removed != null){
                        deletePolygonsContainingEdge(removed.v1, removed.v2);
                    }
                }

                if(selectedPolygon >= 0 && selectedPolygon < polygons.size()){
                    polygons.remove(selectedPolygon);
                }
            }

            selectedVertex = -1;
            selectedLine = -1;
            selectedPolygon = -1;
            clearPickedVertices();
            sortPolygonsByTopVertices();
            refreshPolygonColors();
            repaint();
            updateInfoPanel();
        }

        // Keep picks consistent if a vertex is deleted
        private void shiftPickedAfterVertexDeletion(int delIdx){
            for(int i = pickedVertices.size()-1; i >= 0; i--){
                int v = pickedVertices.get(i);
                if(v == delIdx){
                    pickedVertices.remove(i);
                }else if(v > delIdx){
                    pickedVertices.set(i, v-1);
                }
            }
        }

        // Build polygon order from picked vertices:
        // 1) if they form a cycle in current lines -> follow that cycle
        // 2) else -> sort by angle around centroid
        private ArrayList<Integer> buildPolygonOrderFromPicked(){

            // unique + valid
            LinkedHashSet<Integer> uniq = new LinkedHashSet<>();
            for(int v : pickedVertices){
                if(v >= 0 && v < overlayPoints.size()) uniq.add(v);
            }
            ArrayList<Integer> verts = new ArrayList<>(uniq);
            if(verts.size() < 3) return new ArrayList<>();

            // Try cycle-following using existing edges
            HashSet<Integer> S = new HashSet<>(verts);

            HashMap<Integer, ArrayList<Integer>> nb = new HashMap<>();
            for(int v : verts) nb.put(v, new ArrayList<>());

            for(LineEdge e : lines){
                if(S.contains(e.v1) && S.contains(e.v2)){
                    nb.get(e.v1).add(e.v2);
                    nb.get(e.v2).add(e.v1);
                }
            }

            boolean allDeg2 = true;
            for(int v : verts){
                if(nb.get(v).size() != 2){
                    allDeg2 = false;
                    break;
                }
            }

            if(allDeg2){
                int start = verts.get(0);
                for(int v : verts) start = Math.min(start, v);

                ArrayList<Integer> order = new ArrayList<>();
                order.add(start);

                int prev = start;
                int cur  = nb.get(start).get(0); // choose one direction

                while(cur != start){
                    order.add(cur);
                    ArrayList<Integer> nbs = nb.get(cur);
                    int next = (nbs.get(0) != prev) ? nbs.get(0) : nbs.get(1);
                    prev = cur;
                    cur = next;

                    // safety against infinite loop
                    if(order.size() > verts.size() + 2) break;
                }

                if(order.size() == verts.size()){
                    if(signedAreaIds(order) < 0) Collections.reverse(order); // CCW
                    return order;
                }
                // if failed, fall through to angle sort
            }

            // Angle sort fallback
            double cx=0, cy=0;
            for(int v : verts){
                Point p = overlayPoints.get(v);
                cx += (p.x + 0.5);
                cy += (p.y + 0.5);
            }
            cx /= verts.size();
            cy /= verts.size();

            final double cx0 = cx;
            final double cy0 = cy;

            verts.sort((a,b) -> {
                Point pa = overlayPoints.get(a);
                Point pb = overlayPoints.get(b);
                double aa = Math.atan2((pa.y+0.5)-cy0, (pa.x+0.5)-cx0);
                double ab = Math.atan2((pb.y+0.5)-cy0, (pb.x+0.5)-cx0);
                return Double.compare(aa, ab);
            });

            if(signedAreaIds(verts) < 0) Collections.reverse(verts); // CCW
            return verts;
        }

        // Signed area using overlayPoints (positive => CCW)
        private double signedAreaIds(List<Integer> poly){
            double s = 0.0;
            int m = poly.size();
            for(int i=0; i<m; i++){
                Point p = overlayPoints.get(poly.get(i));
                Point q = overlayPoints.get(poly.get((i+1) % m));
                double x1 = p.x + 0.5, y1 = p.y + 0.5;
                double x2 = q.x + 0.5, y2 = q.y + 0.5;
                s += x1*y2 - x2*y1;
            }
            return 0.5 * s;
        }

        // Optional: avoid adding duplicate polygons (same vertex set / cycle)
        private String polygonKey(List<Integer> poly){
            // canonical by rotation on smallest id, and direction (forward/reverse)
            int m = poly.size();
            int min = Integer.MAX_VALUE;
            for(int v : poly) min = Math.min(min, v);

            ArrayList<Integer> best = null;

            // forward rotations
            for(int i=0; i<m; i++){
                if(poly.get(i) != min) continue;
                ArrayList<Integer> rot = new ArrayList<>(m);
                for(int k=0; k<m; k++) rot.add(poly.get((i+k)%m));
                if(best == null || lexLess(rot, best)) best = rot;
            }

            // reverse rotations
            ArrayList<Integer> rev = new ArrayList<>(poly);
            Collections.reverse(rev);
            for(int i=0; i<m; i++){
                if(rev.get(i) != min) continue;
                ArrayList<Integer> rot = new ArrayList<>(m);
                for(int k=0; k<m; k++) rot.add(rev.get((i+k)%m));
                if(best == null || lexLess(rot, best)) best = rot;
            }

            StringBuilder sb = new StringBuilder(m*4);
            for(int v : best) sb.append(v).append(':');
            return sb.toString();
        }

        private boolean lexLess(List<Integer> a, List<Integer> b){
            int m = Math.min(a.size(), b.size());
            for(int i=0; i<m; i++){
                int da=a.get(i), db=b.get(i);
                if(da != db) return da < db;
            }
            return a.size() < b.size();
        }

        //helper for PolygonID recognition
           // Sort polygons by their top-most vertex position (y then x).
           // If the top vertex is identical, compare the 2nd top-most, then 3rd, ...
        private void sortPolygonsByTopVertices(){

            if(polygons == null || polygons.size() < 2) return;

            class PolyKey {
                final List<Integer> poly;
                final long[] key; // sorted vertex positions: (y,x) packed into a long
                PolyKey(List<Integer> p, long[] k){ poly=p; key=k; }
            }

            ArrayList<PolyKey> list = new ArrayList<>(polygons.size());

            for(List<Integer> poly : polygons){
                list.add(new PolyKey(poly, buildTopVertexKey(poly)));
            }

            Collections.sort(list, new Comparator<PolyKey>() {
                @Override
                public int compare(PolyKey a, PolyKey b) {

                    int n = Math.min(a.key.length, b.key.length);

                    for(int i=0; i<n; i++){
                        long va = a.key[i];
                        long vb = b.key[i];
                        if(va < vb) return -1;
                        if(va > vb) return 1;
                    }

                    // If all compared vertices are identical, shorter polygon first
                    int cmpLen = Integer.compare(a.key.length, b.key.length);
                    if(cmpLen != 0) return cmpLen;

                    return 0;
                }
            });

            ArrayList<List<Integer>> sorted = new ArrayList<>(polygons.size());
            for(PolyKey pk : list) sorted.add(pk.poly);

            polygons.clear();
            polygons.addAll(sorted);
            // selection index becomes invalid after reorder
            selectedPolygon = -1;
        }

        // Build a lexicographic key: vertices sorted by (y, x), top to bottom.
        // Each vertex is packed as: (y << 32) | x
        private long[] buildTopVertexKey(List<Integer> poly){

            if(poly == null || poly.isEmpty()) return new long[0];

            // collect valid vertices (allow duplicates? usually none; we keep as-is)
            long[] tmp = new long[poly.size()];
            int cnt = 0;

            for(int vid : poly){
                if(vid < 0 || vid >= overlayPoints.size()) continue;

                Point p = overlayPoints.get(vid);
                // top-most means smaller y; if y ties, smaller x
                long key = (((long)p.y) << 32) | ((long)p.x & 0xffffffffL);

                tmp[cnt++] = key;
            }

            if(cnt == 0) return new long[0];

            long[] keyArr = Arrays.copyOf(tmp, cnt);
            Arrays.sort(keyArr);
            return keyArr;
        }

        //helper: Remove all polygons whose boundary contains the undirected edge (a,b).
        private void deletePolygonsContainingEdge(int a, int b){

            if(polygons == null || polygons.isEmpty()) return;

            long target = undirectedEdgeKey(a, b);

            for(int i = polygons.size() - 1; i >= 0; i--){

                List<Integer> poly = polygons.get(i);
                if(poly == null || poly.size() < 2) continue;

                if(polygonContainsEdgeRendered(poly, target)){
                    polygons.remove(i);
                }
            }
        }

        // Same undirected key convention used everywhere: (min << 32) | max
        private long undirectedEdgeKey(int u, int v){
            int a = Math.min(u, v);
            int b = Math.max(u, v);
            return (((long)a) << 32) | (b & 0xffffffffL);
        }

        // Check edge membership using the "effective" polygon vertex cycle after filtering invalid indices
        // (so this matches what you actually draw).
        private boolean polygonContainsEdgeRendered(List<Integer> poly, long target){

            int n = overlayPoints.size();
            if(n <= 0) return false;

            ArrayList<Integer> vs = new ArrayList<>(poly.size());

            // filter invalid vertices like paintComponent does, and also remove immediate duplicates
            for(int vid : poly){
                if(vid < 0 || vid >= n) continue;
                if(vs.isEmpty() || vs.get(vs.size()-1) != vid){
                    vs.add(vid);
                }
            }

            // if closed by repeating first vertex, drop last
            if(vs.size() >= 2 && vs.get(0).equals(vs.get(vs.size()-1))){
                vs.remove(vs.size()-1);
            }

            if(vs.size() < 2) return false;

            int m = vs.size();
            for(int k = 0; k < m; k++){
                int u = vs.get(k);
                int v = vs.get((k + 1) % m);

                if(undirectedEdgeKey(u, v) == target){
                    return true;
                }
            }

            return false;
        }

        //helper: delete vertex
        // Delete a vertex safely: remove incident lines, remove polygons containing it,
        // and reindex all remaining line/poly vertex ids.
        private void deleteVertexAndUpdateTopology(int delIdx){

            if(delIdx < 0 || delIdx >= overlayPoints.size()) return;

            final int oldN = overlayPoints.size(); // size BEFORE deletion

            // 1) remove vertex once
            overlayPoints.remove(delIdx);

            // keep picks consistent
            shiftPickedAfterVertexDeletion(delIdx);

            // 2) rebuild lines: delete incident, reindex others (keep stable order)
            if(lines != null){

                LinkedHashSet<LineEdge> newSet = new LinkedHashSet<>();

                for(LineEdge e : lines){

                    int a = e.v1;
                    int b = e.v2;

                    // delete lines touching deleted vertex
                    if(a == delIdx || b == delIdx) continue;

                    // reindex
                    if(a > delIdx) a--;
                    if(b > delIdx) b--;

                    if(a == b) continue;
                    newSet.add(new LineEdge(a, b));
                }

                lines.clear();
                lines.addAll(newSet);
            }

            // 3) rebuild polygons: delete polygons containing deleted vertex, reindex others
            if(polygons != null){

                ArrayList<List<Integer>> kept = new ArrayList<>(polygons.size());

                for(List<Integer> poly : polygons){

                    if(poly == null || poly.size() < 3) continue;

                    boolean drop = false;

                    // drop if contains deleted vertex OR already stale indices (from earlier edits)
                    for(int vid : poly){
                        if(vid == delIdx){
                            drop = true;
                            break;
                        }
                        if(vid < 0 || vid >= oldN){
                            drop = true;
                            break;
                        }
                    }
                    if(drop) continue;

                    ArrayList<Integer> np = new ArrayList<>(poly.size());

                    for(int vid : poly){

                        int nv = (vid > delIdx) ? (vid - 1) : vid;

                        if(nv < 0 || nv >= overlayPoints.size()){
                            drop = true;
                            break;
                        }

                        // remove immediate duplicates
                        if(np.isEmpty() || np.get(np.size()-1) != nv){
                            np.add(nv);
                        }
                    }
                    if(drop) continue;

                    // drop closing duplicate
                    if(np.size() >= 2 && np.get(0).equals(np.get(np.size()-1))){
                        np.remove(np.size()-1);
                    }

                    if(np.size() < 3) continue;
                    if(new HashSet<>(np).size() < 3) continue;

                    kept.add(np);
                }

                polygons.clear();
                polygons.addAll(kept);
            }

            // 4) maintain your ID ordering + color cache (ORDER MATTERS)
            sortPolygonsByTopVertices();
            refreshPolygonColors();
        }


        ImagePanel(){         

            // ====== Menu setting =====
            popup=new JPopupMenu();

            JMenuItem addV=new JMenuItem("Add vertex");
            JMenuItem addL=new JMenuItem("Add Line from selected vertices");
            JMenuItem addP=new JMenuItem("Add Polygon from selected vertices");
            JMenuItem clearPicked = new JMenuItem("Clear selected vertices");
            JMenuItem delV=new JMenuItem("Delete vertex");
            JMenuItem delL=new JMenuItem("Delete Line");
            JMenuItem delP=new JMenuItem("Delete Polygon");

            popup.add(addV);
            popup.add(addL);
            popup.add(addP);
            popup.add(delV);
            popup.add(delL);
            popup.add(delP);

            addV.addActionListener(e->{

                Point p=toImagePixel(clickX,clickY);
                overlayPoints.add(p);
                repaint();
                updateInfoPanel();
            });

            addV.addActionListener(e -> addVertexAtScreen(clickX, clickY));

            addL.addActionListener(e->{
                if(pickedVertices.size() ==2){
                    int a = pickedVertices.get(0);
                    int b = pickedVertices.get(1);
                    LineEdge ne = new LineEdge(a, b);
                    if(!lines.contains(ne)){
                        lines.add(ne);
                    }

                    refreshPolygonColors();
                    clearPickedVertices();
                    repaint();
                    updateInfoPanel();
                }
            });

            addL.addActionListener(e -> addLineFromPickedVertices());

            addP.addActionListener(e->{
                if(pickedVertices.size() >=3){
                    ArrayList<Integer> poly = buildPolygonOrderFromPicked();
                    if(poly.size() >= 3){
                        String key = polygonKey(poly);
                        boolean exists = false;
                        for(List<Integer> p : polygons){
                            if(p != null && p.size() >= 3&& polygonKey(p).equals(key)){
                                exists = true;
                                break;
                            }
                        }

                        if(!exists){
                            polygons.add(poly);
                            sortPolygonsByTopVertices();
                            refreshPolygonColors();
                        }
                    }
                }

                clearPickedVertices();
                repaint();
                updateInfoPanel();
            });

            addP.addActionListener(e -> addPolygonFromPickedVertices());

            clearPicked.addActionListener(e -> {
                clearPickedVertices();
                repaint();
                updateInfoPanel();
            });

            delV.addActionListener(e->{

                if(selectedVertex>=0){
                    deleteVertexAndUpdateTopology(selectedVertex);

                    selectedVertex = -1;
                    selectedLine = -1;
                    selectedPolygon = -1;
                    repaint();
                    updateInfoPanel();
                }                
            });

            delL.addActionListener(e -> {

                if(selectedLine >= 0 && selectedLine < lines.size()){

                    // remove the line and get its endpoints
                    LineEdge removed = lines.remove(selectedLine);

                    selectedLine = -1;
                    selectedPolygon = -1;

                    if(removed != null){

                        // delete polygons that contain this edge
                        deletePolygonsContainingEdge(removed.v1, removed.v2);

                        // keep ID ordering + colors consistent
                        sortPolygonsByTopVertices();
                        refreshPolygonColors();
                    }

                    repaint();
                    updateInfoPanel();
                }
            });

            delP.addActionListener(e-> {
                if(selectedPolygon >= 0 && selectedPolygon < polygons.size()){
                    polygons.remove(selectedPolygon);
                    selectedPolygon = -1;
                    sortPolygonsByTopVertices();
                    refreshPolygonColors();
                    repaint();
                }
            });

            InputMap inputMap = getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW);
            ActionMap actionMap = getActionMap();

            inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_V, InputEvent.CTRL_DOWN_MASK), "addVertexShortcut");
            inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_L, InputEvent.CTRL_DOWN_MASK), "addLineShortcut");
            inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_P, InputEvent.CTRL_DOWN_MASK), "addPolygonShortcut");
            inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_D, InputEvent.CTRL_DOWN_MASK), "deleteSelectedShortcut");

            actionMap.put("addVertexShortcut", new AbstractAction() {
                @Override
                public void actionPerformed(ActionEvent e) {
                    addVertexAtScreen(mouseX, mouseY);
                }
            });
            actionMap.put("addLineShortcut", new AbstractAction() {
                @Override
                public void actionPerformed(ActionEvent e) {
                    addLineFromPickedVertices();
                }
            });
            actionMap.put("addPolygonShortcut", new AbstractAction() {
                @Override
                public void actionPerformed(ActionEvent e) {
                    addPolygonFromPickedVertices();
                }
            });
            actionMap.put("deleteSelectedShortcut", new AbstractAction() {
                @Override
                public void actionPerformed(ActionEvent e) {
                    deleteAllSelectedItems();
                }
            });

            MouseAdapter ma=new MouseAdapter(){

                public void mousePressed(MouseEvent e){

                    mouseX = e.getX();
                    mouseY = e.getY();

                    if(SwingUtilities.isLeftMouseButton(e)){

                        int idx = findVertexAt(e.getX(), e.getY());

                        if(idx >= 0){
                            pressedVertex = idx;
                            pressPoint = e.getPoint();
                            draggingVertex = false;
                            return;
                        }

                        // otherwise pan image
                        JScrollPane sp=(JScrollPane)
                                SwingUtilities.getAncestorOfClass(JScrollPane.class,ImagePanel.this);

                        if(sp!=null){

                            draggingImage=true;

                            dragStartScreen=e.getLocationOnScreen();
                            dragStartView=sp.getViewport().getViewPosition();

                            setCursor(Cursor.getPredefinedCursor(Cursor.MOVE_CURSOR));
                        }
                    }
                }

                public void mouseDragged(MouseEvent e){

                    mouseX = e.getX();
                    mouseY = e.getY();
                    
                    updateMousePositionLabel(e.getX(), e.getY());

                    // vertex drag (start only if moved beyond threshold)
                    if(pressedVertex >= 0){

                        if(!draggingVertex){
                            int dx = e.getX() - pressPoint.x;
                            int dy = e.getY() - pressPoint.y;
                            if(dx*dx + dy*dy < 9) return; // 3px threshold
                            draggingVertex = true;
                        }

                        Point p = toImagePixel(e.getX(), e.getY());
                        overlayPoints.get(pressedVertex).x = p.x;
                        overlayPoints.get(pressedVertex).y = p.y;

                        repaint();
                        updateInfoPanel();
                        return;
                    }

                    // pan
                    if(draggingImage){

                        JScrollPane sp=(JScrollPane)
                                SwingUtilities.getAncestorOfClass(JScrollPane.class,ImagePanel.this);

                        if(sp==null) return;

                        Point cur=e.getLocationOnScreen();

                        int dx=cur.x-dragStartScreen.x;
                        int dy=cur.y-dragStartScreen.y;

                        Point newPos=new Point(
                                dragStartView.x-dx,
                                dragStartView.y-dy);

                        sp.getViewport().setViewPosition(newPos);
                    }
                }

                public void mouseReleased(MouseEvent e){

                    mouseX = e.getX();
                    mouseY = e.getY();

                    // click on vertex without drag => toggle pick
                    if(SwingUtilities.isLeftMouseButton(e)){
                        if(pressedVertex >= 0 && !draggingVertex){
                            togglePickVertex(pressedVertex);
                            repaint();
                            updateInfoPanel();
                        }
                    }

                    pressedVertex = -1;
                    pressPoint = null;
                    draggingVertex = false;

                    draggingImage=false;

                    // do NOT clear pickedVertices here
                    selectedVertex=-1;
                    selectedLine = -1;
                    selectedPolygon = -1;
                    updateSelectionInfoLabels();

                    setCursor(Cursor.getDefaultCursor());
                }

                @Override
                public void mouseExited(MouseEvent e) {
                    infoMousePositionValue.setText("-");
                }

                public void mouseClicked(MouseEvent e){

                    mouseX = e.getX();
                    mouseY = e.getY();

                    clickX=e.getX();
                    clickY=e.getY();

                    selectedVertex=findVertexAt(e.getX(),e.getY());
                    selectedLine = (selectedVertex < 0) ? findLineAt(e.getX(), e.getY()) : -1;
                    selectedPolygon = (selectedVertex < 0 && selectedLine < 0) ? findPolygonAt(e.getX(), e.getY()) : -1;
                    if(selectedVertex < 0){
                        clearPickedVertices();
                    }
                    updateSelectionInfoLabels();

                    if(SwingUtilities.isRightMouseButton(e)){
                        // Enable/disable menu items depending on what was clicked
                        // (Assumes you used variable names add, delV, delL from section B)
                        boolean onPickedVertex = (selectedVertex >= 0 && isPicked(selectedVertex));
                        addL.setEnabled(onPickedVertex && pickedVertices.size() == 2);
                        addP.setEnabled(onPickedVertex && pickedVertices.size() >= 3);
                        clearPicked.setEnabled(!pickedVertices.isEmpty());

                        delV.setEnabled(selectedVertex >= 0);
                        delL.setEnabled(selectedLine >= 0);
                        delP.setEnabled(selectedPolygon >=0);

                        popup.show(ImagePanel.this, e.getX(), e.getY());
                        repaint(); // optional: to show highlight
                    }
                }
            };

            addMouseListener(ma);
            addMouseMotionListener(ma);

            addMouseMotionListener(new MouseMotionAdapter() {
                @Override
                public void mouseMoved(MouseEvent e) {
                    mouseX = e.getX();
                    mouseY = e.getY();
                    updateMousePositionLabel(e.getX(), e.getY());
                }
            });

            addMouseWheelListener(e->{

                double factor=e.getWheelRotation()<0?1.25:0.8;

                zoom*=factor;

                revalidate();
                repaint();
                adjustWindowToContent();
            });
        }

        void setImage(BufferedImage img){

            image=img;

            revalidate();
            repaint();
            adjustWindowToContent();
        }

        void setBackgroundImage(BufferedImage img){
            backgroundImage = img;
            revalidate();
            repaint();
            adjustWindowToContent();
        }

        boolean hasBackgroundImage(){
            return backgroundImage != null;
        }

        Dimension getBackgroundImageSize(){
            if(backgroundImage == null) return null;
            return new Dimension(backgroundImage.getWidth(), backgroundImage.getHeight());
        }


        void setOverlayPoints(List<Point> pts){
            overlayPoints=pts;
            clearDebugHighlights();
            repaint();
            updateInfoPanel();
        }

        List<Point> getOverlayPoints(){
            return overlayPoints;
        }

        void setLines(List<LineEdge> l){

            lines=l;
            clearDebugHighlights();
            clearNeighborPairs();
            CellDivisionInference.this.clearNeighborPairGeometryCache();
            CellDivisionInference.this.clearNeighborPairEstimationCache();
            CellDivisionInference.this.clearRealDivisionPairCache();
            CellDivisionInference.this.clearComparisonPairCaches();
            repaint();
            updateInfoPanel();
        }

        void setPolygons(List<List<Integer>> p){

            polygons=p;
            clearDebugHighlights();
            polygonDisplayIds = new ArrayList<>();
            clearNeighborPairs();
            CellDivisionInference.this.clearNeighborPairGeometryCache();
            CellDivisionInference.this.clearNeighborPairEstimationCache();
            CellDivisionInference.this.clearRealDivisionPairCache();
            CellDivisionInference.this.clearComparisonPairCaches();
            sortPolygonsByTopVertices();
            refreshPolygonColors();
            repaint();
            updateInfoPanel();
        }

        void setPolygonsKeepOrder(List<List<Integer>> p){
            polygons = p;
            clearDebugHighlights();
            polygonDisplayIds = new ArrayList<>();
            clearNeighborPairs();
            CellDivisionInference.this.clearNeighborPairGeometryCache();
            CellDivisionInference.this.clearNeighborPairEstimationCache();
            CellDivisionInference.this.clearRealDivisionPairCache();
            CellDivisionInference.this.clearComparisonPairCaches();

            // IMPORTANT:
            // Do NOT sort here.
            // Keep imported polygon order exactly as given by the JSON.
            refreshPolygonColors();
            repaint();
            updateInfoPanel();
        }

        void setPolygonDisplayIds(List<Integer> ids){
            if(ids == null) polygonDisplayIds = new ArrayList<>();
            else polygonDisplayIds = new ArrayList<>(ids);
            repaint();
            updateInfoPanel();
        }

        void selectVertex(int vertexIdx){
            if(vertexIdx < 0 || vertexIdx >= overlayPoints.size()) return;
            selectedVertex = vertexIdx;
            selectedLine = -1;
            selectedPolygon = -1;
            clearPickedVertices();
            repaint();
            updateSelectionInfoLabels();
        }

        void selectLine(int lineIdx){
            if(lineIdx < 0 || lineIdx >= lines.size()) return;
            selectedVertex = -1;
            selectedLine = lineIdx;
            selectedPolygon = -1;
            clearPickedVertices();
            repaint();
            updateSelectionInfoLabels();
        }

        void selectPolygon(int polygonIdx){
            if(polygonIdx < 0 || polygonIdx >= polygons.size()) return;
            selectedVertex = -1;
            selectedLine = -1;
            selectedPolygon = polygonIdx;
            clearPickedVertices();
            repaint();
            updateSelectionInfoLabels();
        }

        void resetSelectionsAndPicks(){
            selectedVertex = -1;
            selectedLine = -1;
            selectedPolygon = -1;
            draggingVertex = false;
            draggingImage = false;
            pressedVertex = -1;
            pressPoint = null;
            clearPickedVertices();
        }

        List<int[]> getNeighborPairs(){
            return neighborPairs;
        }

        private List<Color> polygonFillColors = new ArrayList<>();

        void refreshPolygonColors(){

            polygonFillColors = new ArrayList<>(polygons.size());

            if(polygons == null || polygons.isEmpty()){
                return;
            }

            // Uniform mode
            if(!polygonRandomColors){
                Color c = withAlpha(polygonUniformColor, polygonFillAlpha);
                for(int i=0; i<polygons.size(); i++){
                    polygonFillColors.add(c);
                }
                return;
            }

            // Random neighbor-aware mode:
            // Build adjacency by shared undirected edges between polygons.
            int P = polygons.size();
            ArrayList<HashSet<Integer>> adj = new ArrayList<>(P);
            for(int i=0; i<P; i++) adj.add(new HashSet<>());

            HashMap<Long, Integer> edgeOwner = new HashMap<>(P * 8);

            for(int i=0; i<P; i++){
                List<Integer> poly = polygons.get(i);
                if(poly == null || poly.size() < 3) continue;

                int m = poly.size();
                for(int k=0; k<m; k++){
                    int a = poly.get(k);
                    int b = poly.get((k+1) % m);
                    if(a == b) continue;

                    int u = Math.min(a,b);
                    int v = Math.max(a,b);

                    long key = (((long)u) << 32) | (v & 0xffffffffL);

                    Integer prev = edgeOwner.putIfAbsent(key, i);
                    if(prev != null && prev != i){
                        adj.get(i).add(prev);
                        adj.get(prev).add(i);
                    }
                }
            }

            // Order polygons by degree (high degree first) for better greedy coloring
            Integer[] order = new Integer[P];
            for(int i=0; i<P; i++) order[i] = i;
            Arrays.sort(order, (i,j) -> Integer.compare(adj.get(j).size(), adj.get(i).size()));

            double[] hue = new double[P];
            Arrays.fill(hue, Double.NaN);

            // Candidate hues: 72 evenly-spaced values (good separation)
            double[] candidates = new double[72];
            for(int i=0; i<candidates.length; i++){
                candidates[i] = i / (double)candidates.length; // 0..1
            }

            for(int idx : order){

                double bestH = 0.0;
                double bestScore = -1.0;

                for(double hCand : candidates){

                    double minDist = 1.0;

                    for(int nb : adj.get(idx)){
                        if(Double.isNaN(hue[nb])) continue;
                        double d = Math.abs(hCand - hue[nb]);
                        d = Math.min(d, 1.0 - d); // circular distance
                        if(d < minDist) minDist = d;
                    }

                    if(minDist > bestScore){
                        bestScore = minDist;
                        bestH = hCand;
                    }
                }

                // Fallback if isolated or all neighbors uncolored
                if(bestScore < 0){
                    bestH = (idx * 0.6180339887498949) % 1.0; // golden ratio
                }

                hue[idx] = bestH;
            }

            // Convert hues to Colors
            for(int i=0; i<P; i++){
                double h = Double.isNaN(hue[i]) ? (i * 0.6180339887498949) % 1.0 : hue[i];
                Color base = Color.getHSBColor((float)h, 0.55f, 1.0f);
                polygonFillColors.add(new Color(base.getRed(), base.getGreen(), base.getBlue(), polygonFillAlpha));
            }
        }

        private Color withAlpha(Color c, int a){
            return new Color(c.getRed(), c.getGreen(), c.getBlue(), clamp255(a));
        }

        private int clamp255(int v){
            return Math.max(0, Math.min(255, v));
        }

        void setNeighborPairs(List<int[]> pairs){
            if(pairs == null) neighborPairs = new ArrayList<>();
            else neighborPairs = new ArrayList<>(pairs);
            CellDivisionInference.this.clearNeighborPairGeometryCache();
            CellDivisionInference.this.clearNeighborPairEstimationCache();
            CellDivisionInference.this.clearRealDivisionPairCache();
            CellDivisionInference.this.clearComparisonPairCaches();
            repaint();
        }

        void clearNeighborPairs(){
            neighborPairs = new ArrayList<>();
            CellDivisionInference.this.clearNeighborPairGeometryCache();
            CellDivisionInference.this.clearNeighborPairEstimationCache();
            CellDivisionInference.this.clearRealDivisionPairCache();
            CellDivisionInference.this.clearComparisonPairCaches();
        }

        boolean isShowNeighborLinks(){
            return showNeighborLinks;
        }
        void setShowNeighborLinks(boolean b){
            showNeighborLinks = b;
            repaint();
        }

        Color getNeighborLinkColor(){
            return neighborLinkColor;
        }
        void setNeighborLinkColor(Color c){
            if(c != null) neighborLinkColor = c;
            repaint();
        }

        float getNeighborLinkWidth(){
            return neighborLinkWidth;
        }
        void setNeighborLinkWidth(float w){
            if(w < 0.1f) w = 0.1f;
            neighborLinkWidth = w;
            repaint();
        }

        int findVertexAt(int mx,int my){

            double r=vertexRadius+2;
            double r2=r*r;

            for(int i=0;i<overlayPoints.size();i++){

                Point p=overlayPoints.get(i);

                double cx=(p.x+0.5)*zoom;
                double cy=(p.y+0.5)*zoom;

                double dx=mx-cx;
                double dy=my-cy;

                if(dx*dx+dy*dy<=r2)
                    return i;
            }

            return -1;
        }

        int findLineAt(int mx, int my){

            if(lines == null || lines.isEmpty()) return -1;

            // click tolerance in screen pixels
            double tol = Math.max(6.0, (double)lineWidth + 4.0);
            double tol2 = tol * tol;

            int bestIdx = -1;
            double bestD2 = Double.MAX_VALUE;

            for(int i=0; i<lines.size(); i++){

                LineEdge e = lines.get(i);

                // safety
                if(e.v1 < 0 || e.v2 < 0) continue;
                if(e.v1 >= overlayPoints.size() || e.v2 >= overlayPoints.size()) continue;

                Point p1 = overlayPoints.get(e.v1);
                Point p2 = overlayPoints.get(e.v2);

                double x1 = (p1.x + 0.5) * zoom;
                double y1 = (p1.y + 0.5) * zoom;
                double x2 = (p2.x + 0.5) * zoom;
                double y2 = (p2.y + 0.5) * zoom;

                double d2 = pointToSegmentDistanceSq(mx, my, x1, y1, x2, y2);

                if(d2 <= tol2 && d2 < bestD2){
                    bestD2 = d2;
                    bestIdx = i;
                }
            }

            return bestIdx;
        }

        private double pointToSegmentDistanceSq(double px, double py,
                                                double x1, double y1,
                                                double x2, double y2){

            double vx = x2 - x1;
            double vy = y2 - y1;

            double wx = px - x1;
            double wy = py - y1;

            double c1 = vx*wx + vy*wy;
            if(c1 <= 0) return (px-x1)*(px-x1) + (py-y1)*(py-y1);

            double c2 = vx*vx + vy*vy;
            if(c2 <= c1) return (px-x2)*(px-x2) + (py-y2)*(py-y2);

            double t = c1 / c2;
            double projx = x1 + t*vx;
            double projy = y1 + t*vy;

            double dx = px - projx;
            double dy = py - projy;

            return dx*dx + dy*dy;
        }

        int findPolygonAt(int mx, int my){

            if(polygons == null || polygons.isEmpty()) return -1;

            // use precise image coordinates (not floor)
            double px = mx / zoom;
            double py = my / zoom;

            int bestIdx = -1;
            double bestArea = Double.MAX_VALUE;

            for(int i=0; i<polygons.size(); i++){

                List<Integer> poly = polygons.get(i);
                if(poly == null || poly.size() < 3) continue;

                // Build arrays of polygon vertices in image coordinates
                int m = poly.size();
                double[] xs = new double[m];
                double[] ys = new double[m];

                int count = 0;
                for(int k=0; k<m; k++){
                    int vid = poly.get(k);
                    if(vid < 0 || vid >= overlayPoints.size()) continue;

                    Point p = overlayPoints.get(vid);
                    xs[count] = p.x + 0.5;
                    ys[count] = p.y + 0.5;
                    count++;
                }

                if(count < 3) continue;

                if(pointInPolygon(px, py, xs, ys, count)){
                    double area = Math.abs(polygonArea(xs, ys, count));
                    // pick smallest containing polygon (usually the intended cell)
                    if(area < bestArea){
                        bestArea = area;
                        bestIdx = i;
                    }
                }
            }

            return bestIdx;
        }

        // Ray casting point-in-polygon
        private boolean pointInPolygon(double x, double y, double[] xs, double[] ys, int n){

            boolean inside = false;

            for(int i=0, j=n-1; i<n; j=i++){
                double xi = xs[i], yi = ys[i];
                double xj = xs[j], yj = ys[j];

                boolean intersect = ((yi > y) != (yj > y)) &&
                        (x < (xj - xi) * (y - yi) / (yj - yi + 1e-12) + xi);

                if(intersect) inside = !inside;
            }

            return inside;
        }

        // Area-weighted centroid in image coordinates (x+0.5, y+0.5).
        // Returns {cx, cy} in image coordinate system.
        private double[] polygonCentroid(List<Integer> poly){

            int m = poly.size();

            double A2 = 0.0; // 2*Area
            double Cx6A = 0.0;
            double Cy6A = 0.0;

            for(int i=0; i<m; i++){

                int ia = poly.get(i);
                int ib = poly.get((i+1) % m);

                if(ia < 0 || ib < 0 || ia >= overlayPoints.size() || ib >= overlayPoints.size())
                    continue;

                Point a = overlayPoints.get(ia);
                Point b = overlayPoints.get(ib);

                double x1 = a.x + 0.5;
                double y1 = a.y + 0.5;
                double x2 = b.x + 0.5;
                double y2 = b.y + 0.5;

                double cross = x1*y2 - x2*y1;
                A2 += cross;

                Cx6A += (x1 + x2) * cross;
                Cy6A += (y1 + y2) * cross;
            }

            if(Math.abs(A2) < 1e-9){
                // fallback: average of vertices
                double sx = 0, sy = 0;
                int cnt = 0;
                for(int vid : poly){
                    if(vid < 0 || vid >= overlayPoints.size()) continue;
                    Point p = overlayPoints.get(vid);
                    sx += p.x + 0.5;
                    sy += p.y + 0.5;
                    cnt++;
                }
                if(cnt == 0) return new double[]{0,0};
                return new double[]{sx/cnt, sy/cnt};
            }

            double cx = Cx6A / (3.0 * A2);
            double cy = Cy6A / (3.0 * A2);

            return new double[]{cx, cy};
        }

        private double polygonArea(double[] xs, double[] ys, int n){
            double s = 0.0;
            for(int i=0; i<n; i++){
                int j = (i + 1) % n;
                s += xs[i] * ys[j] - xs[j] * ys[i];
            }
            return 0.5 * s;
        }

        private Point toImagePixel(int x,int y){

            return new Point(
                    (int)Math.floor(x/zoom),
                    (int)Math.floor(y/zoom));
        }

        private void updateMousePositionLabel(int x, int y){
            if(infoMousePositionValue == null){
                return;
            }
            if(!isInsideCanvas(x, y)){
                clearMousePositionLabel();
                return;
            }
            infoMousePositionValue.setText(x + ", " + y);
        }

        private void clearMousePositionLabel(){
            if(infoMousePositionValue != null){
                infoMousePositionValue.setText("-");
            }
        }

        private boolean isInsideCanvas(int x, int y){
            if(image == null) return false;
            int maxX = (int)Math.ceil(image.getWidth() * zoom);
            int maxY = (int)Math.ceil(image.getHeight() * zoom);
            return x >= 0 && y >= 0 && x < maxX && y < maxY;
        }

        void updateSelectionInfoLabels(){
            if(infoSelectedItemValue == null) return;

            String itemName = "-";
            String itemId = "-";
            String itemPos = "-";

            if(selectedVertex >= 0 && selectedVertex < overlayPoints.size()){
                Point p = overlayPoints.get(selectedVertex);
                itemName = "Vertex";
                itemId = Integer.toString(selectedVertex);
                itemPos = "(" + p.x + ", " + p.y + ")";
            }else if(selectedLine >= 0 && selectedLine < lines.size()){
                LineEdge e = lines.get(selectedLine);
                itemName = "Line";
                itemId = Integer.toString(selectedLine);
                itemPos = e.v1 + " - " + e.v2;
            }else if(selectedPolygon >= 0 && selectedPolygon < polygons.size()){
                List<Integer> poly = polygons.get(selectedPolygon);
                itemName = "Polygon";
                itemId = Integer.toString(selectedPolygon);
                itemPos = (poly == null) ? "-" : ("vertices=" + poly.size());
            }

            infoSelectedItemValue.setText(itemName);
            infoSelectedItemIdValue.setText(itemId);
            infoSelectedItemPosValue.setText(itemPos);
        }

        public Dimension getPreferredSize(){
            int canvasW = getCanvasWidth();
            int canvasH = getCanvasHeight();

            if(image==null)
            if(canvasW <= 0 || canvasH <= 0)
                return new Dimension(800,600);

            return new Dimension(
                    (int)(canvasW*zoom),
                    (int)(canvasH*zoom));
        }

        private int getCanvasWidth(){
            int w = 0;
            if(backgroundImage != null) w = Math.max(w, backgroundImage.getWidth());
            if(image != null) w = Math.max(w, image.getWidth());
            return w;
        }

        private int getCanvasHeight(){
            int h = 0;
            if(backgroundImage != null) h = Math.max(h, backgroundImage.getHeight());
            if(image != null) h = Math.max(h, image.getHeight());
            return h;
        }

        private void drawDivisionArrow(Graphics2D g2, double x1, double y1, double x2, double y2,
                                    DivisionArrowType type, double headLen, double headWid){

            double dx = x2 - x1;
            double dy = y2 - y1;
            double len = Math.hypot(dx, dy);
            if(len < 1e-9) return;

            double ux = dx / len;
            double uy = dy / len;

            // perpendicular
            double px = -uy;
            double py = ux;

            // main line
            g2.drawLine((int)Math.round(x1), (int)Math.round(y1),
                        (int)Math.round(x2), (int)Math.round(y2));

            if(type == DivisionArrowType.LineOnly) return;

            // helper to draw head (same as Qt createArrowPath: two lines at each tip)
            java.util.function.BiConsumer<double[], double[]> addHead = (tip, dir) -> {
                double tipX = tip[0], tipY = tip[1];
                double dirX = dir[0], dirY = dir[1];

                double backX = tipX - dirX * headLen;
                double backY = tipY - dirY * headLen;

                double leftX  = backX + px * headWid;
                double leftY  = backY + py * headWid;
                double rightX = backX - px * headWid;
                double rightY = backY - py * headWid;

                g2.drawLine((int)Math.round(tipX), (int)Math.round(tipY),
                            (int)Math.round(leftX), (int)Math.round(leftY));
                g2.drawLine((int)Math.round(tipX), (int)Math.round(tipY),
                            (int)Math.round(rightX), (int)Math.round(rightY));
            };

            if(type == DivisionArrowType.DoubleHeaded){
                addHead.accept(new double[]{x2, y2}, new double[]{ ux,  uy});
                addHead.accept(new double[]{x1, y1}, new double[]{-ux, -uy});
            }else if(type == DivisionArrowType.SingleHeaded){
                addHead.accept(new double[]{x2, y2}, new double[]{ ux,  uy});
            }
        }

        protected void paintComponent(Graphics g){

            super.paintComponent(g);

            if(image==null && backgroundImage == null) return;

            Graphics2D g2=(Graphics2D)g;

            if(backgroundImage != null){
                int bgW = (int)(backgroundImage.getWidth() * zoom);
                int bgH = (int)(backgroundImage.getHeight() * zoom);
                g2.drawImage(backgroundImage, 0, 0, bgW, bgH, null);
            }

            if(image != null){
                int w=(int)(image.getWidth()*zoom);
                int h=(int)(image.getHeight()*zoom);
                g2.drawImage(image,0,0,w,h,null);
            }

            // polygons
            if(polygons != null && !polygons.isEmpty()){

                // font size scales with zoom (clamped)
                int fs = (int)Math.round(cellIdFontSize * zoom);
                fs = Math.max(6, Math.min(200, fs));

                Font idFont = g2.getFont().deriveFont((float)fs);

                for(int i=0; i<polygons.size(); i++){

                    List<Integer> poly = polygons.get(i);
                    if(poly == null || poly.size() < 3) continue;

                    Polygon shape = new Polygon();
                    for(int vid : poly){
                        if(vid >= overlayPoints.size()) continue;
                        Point p = overlayPoints.get(vid);
                        shape.addPoint((int)((p.x+0.5)*zoom),
                                (int)((p.y+0.5)*zoom));
                    }

                    if(shape.npoints < 3) continue;

                    // fill color
                    Color fill = (polygonFillColors != null && polygonFillColors.size() == polygons.size())
                            ? polygonFillColors.get(i)
                            : new Color(0,200,255, polygonFillAlpha);

                    g2.setColor(fill);
                    g2.fillPolygon(shape);

                    // highlight selected polygon
                    if(i == selectedPolygon){
                        g2.setColor(Color.YELLOW);
                        g2.setStroke(new BasicStroke(2.0f));
                        g2.drawPolygon(shape);
                    }

                    // draw cell ID at centroid
                    if(showCellId){
                        double[] c = polygonCentroid(poly);

                        double sx = c[0] * zoom;
                        double sy = c[1] * zoom;

                        String label;
                        if(polygonDisplayIds != null && polygonDisplayIds.size() == polygons.size()){
                            label = String.valueOf(polygonDisplayIds.get(i));
                        }else{
                            label = String.valueOf(i);
                        }
                        
                        g2.setFont(idFont);
                        FontMetrics fm = g2.getFontMetrics();

                        int tw = fm.stringWidth(label);
                        int th = fm.getAscent();

                        int tx = (int)Math.round(sx - tw / 2.0);
                        int ty = (int)Math.round(sy + th / 2.0);

                        // small shadow for readability
                        g2.setColor(new Color(0,0,0,160));
                        g2.drawString(label, tx+1, ty+1);

                        g2.setColor(cellIdColor);
                        g2.drawString(label, tx, ty);
                    }
                }
            }

            // ===== neighbor centroid links =====
            if(showNeighborLinks && neighborPairs != null && !neighborPairs.isEmpty() && polygons != null && polygons.size() > 0){

                Stroke oldStroke = g2.getStroke();
                Color oldColor = g2.getColor();

                float[] dash = new float[]{6.0f, 6.0f}; // dashed line
                g2.setColor(neighborLinkColor); // magenta w/ alpha
                g2.setStroke(new BasicStroke(
                        neighborLinkWidth,
                        BasicStroke.CAP_ROUND,
                        BasicStroke.JOIN_ROUND,
                        10.0f,
                        dash,
                        0.0f
                ));

                for(int[] pr : neighborPairs){
                    if(pr == null || pr.length < 2) continue;
                    int i = pr[0], j = pr[1];
                    if(i < 0 || j < 0 || i >= polygons.size() || j >= polygons.size()) continue;

                    List<Integer> pA = polygons.get(i);
                    List<Integer> pB = polygons.get(j);
                    if(pA == null || pB == null || pA.size() < 3 || pB.size() < 3) continue;

                    double[] cA = polygonCentroid(pA);
                    double[] cB = polygonCentroid(pB);

                    int x1 = (int)Math.round(cA[0] * zoom);
                    int y1 = (int)Math.round(cA[1] * zoom);
                    int x2 = (int)Math.round(cB[0] * zoom);
                    int y2 = (int)Math.round(cB[1] * zoom);

                    g2.drawLine(x1, y1, x2, y2);
                }

                g2.setStroke(oldStroke);
                g2.setColor(oldColor);
            }

            // lines
            g2.setColor(lineColor);
            g2.setStroke(new BasicStroke(lineWidth, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));

            for(int i=0; i<lines.size(); i++){

                LineEdge e = lines.get(i);
                if(e.v1 >= overlayPoints.size() || e.v2 >= overlayPoints.size()) continue;

                Point p1 = overlayPoints.get(e.v1);
                Point p2 = overlayPoints.get(e.v2);

                if(i == selectedLine){
                    g2.setColor(Color.YELLOW);
                    g2.setStroke(new BasicStroke(lineWidth + 2.0f, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
                }else if(debugHighlightedLines.contains(i)){
                    g2.setColor(new Color(255, 0, 255));
                    g2.setStroke(new BasicStroke(lineWidth + 2.0f, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
                }else{
                    g2.setColor(lineColor);
                    g2.setStroke(new BasicStroke(lineWidth, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
                }

                g2.drawLine(
                        (int)((p1.x+0.5)*zoom),
                        (int)((p1.y+0.5)*zoom),
                        (int)((p2.x+0.5)*zoom),
                        (int)((p2.y+0.5)*zoom));
            }

            // ===== estimated division arrows (daughter-cell pairs) =====
            if(showDivisionArrows
                    && polygons != null && polygons.size() > 0
                    && CellDivisionInference.this.neighborPairEstimationCache != null
                    && !CellDivisionInference.this.neighborPairEstimationCache.isEmpty()){

                Stroke oldStroke = g2.getStroke();
                Color oldColor = g2.getColor();

                // Qt: width scales with view; in Swing we keep it readable and mildly zoom-aware
                float strokeW = Math.max(1.0f, divisionArrowWidth);
                g2.setStroke(new BasicStroke(strokeW, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
                g2.setColor(divisionArrowColor);

                // Qt head sizes are in scene units; here we scale with zoom a bit but clamp for usability
                double headLen = divisionArrowHeadLength * zoom;
                double headWid = divisionArrowHeadWidth  * zoom;
                headLen = Math.max(3.0, Math.min(60.0, headLen));
                headWid = Math.max(2.0, Math.min(40.0, headWid));

                for(Map.Entry<Long, EstimationEntry> ent : CellDivisionInference.this.neighborPairEstimationCache.entrySet()){
                    EstimationEntry ee = ent.getValue();
                    if(ee == null || !ee.selected) continue;

                    long k = ent.getKey();
                    int a = (int)(k >> 32);
                    int b = (int)(k & 0xffffffffL);

                    if(a < 0 || b < 0 || a >= polygons.size() || b >= polygons.size()) continue;
                    List<Integer> pA = polygons.get(a);
                    List<Integer> pB = polygons.get(b);
                    if(pA == null || pB == null || pA.size() < 3 || pB.size() < 3) continue;

                    double[] cA = polygonCentroid(pA);
                    double[] cB = polygonCentroid(pB);

                    double x1 = cA[0] * zoom;
                    double y1 = cA[1] * zoom;
                    double x2 = cB[0] * zoom;
                    double y2 = cB[1] * zoom;

                    drawDivisionArrow(g2, x1, y1, x2, y2, divisionArrowType, headLen, headWid);
                }

                g2.setStroke(oldStroke);
                g2.setColor(oldColor);
            }

            // ===== real (groud-truth) division arrows =====
            if(showRealDivisionArrows
                    && polygons != null && polygons.size() > 0
                    && CellDivisionInference.this.realDivisionPairCache != null
                    && !CellDivisionInference.this.realDivisionPairCache.isEmpty()){

                Stroke oldStroke = g2.getStroke();
                Color oldColor = g2.getColor();

                float strokeW = Math.max(1.0f, realDivisionArrowWidth);
                g2.setStroke(new BasicStroke(strokeW, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
                g2.setColor(realDivisionArrowColor);

                double headLen = realDivisionArrowHeadLength * zoom;
                double headWid = realDivisionArrowHeadWidth  * zoom;
                headLen = Math.max(3.0, Math.min(60.0, headLen));
                headWid = Math.max(2.0, Math.min(40.0, headWid));

                for(Map.Entry<Long, RealDivisionEntry> ent : CellDivisionInference.this.realDivisionPairCache.entrySet()){
                    long k = ent.getKey();
                    int a = (int)(k >> 32);
                    int b = (int)(k & 0xffffffffL);

                    if(a < 0 || b < 0 || a >= polygons.size() || b >= polygons.size()) continue;

                    List<Integer> pA = polygons.get(a);
                    List<Integer> pB = polygons.get(b);
                    if(pA == null || pB == null || pA.size() < 3 || pB.size() < 3) continue;

                    double[] cA = polygonCentroid(pA);
                    double[] cB = polygonCentroid(pB);

                    double x1 = cA[0] * zoom;
                    double y1 = cA[1] * zoom;
                    double x2 = cB[0] * zoom;
                    double y2 = cB[1] * zoom;

                    drawDivisionArrow(g2, x1, y1, x2, y2, realDivisionArrowType, headLen, headWid);
                }

                g2.setStroke(oldStroke);
                g2.setColor(oldColor);
            }

            // ===== True positive arrows =====
            if(showTruePositiveDivisionArrows
                && polygons != null && !CellDivisionInference.this.truePositivePairCache.isEmpty()){

                Stroke oldStroke = g2.getStroke();
                Color oldColor = g2.getColor();

                g2.setStroke(new BasicStroke(Math.max(1.0f, truePositiveArrowWidth),
                        BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
                g2.setColor(truePositiveArrowColor);

                double headLen = Math.max(3.0, Math.min(60.0, truePositiveArrowHeadLength * zoom));
                double headWid = Math.max(2.0, Math.min(40.0, truePositiveArrowHeadWidth  * zoom));

                for(long k : CellDivisionInference.this.truePositivePairCache.keySet()){
                    int a = (int)(k >> 32);
                    int b = (int)(k & 0xffffffffL);
                    if(a < 0 || b < 0 || a >= polygons.size() || b >= polygons.size()) continue;

                    List<Integer> pA = polygons.get(a);
                    List<Integer> pB = polygons.get(b);
                    if(pA == null || pB == null || pA.size() < 3 || pB.size() < 3) continue;

                    double[] cA = polygonCentroid(pA);
                    double[] cB = polygonCentroid(pB);

                    drawDivisionArrow(g2, cA[0]*zoom, cA[1]*zoom, cB[0]*zoom, cB[1]*zoom,
                            truePositiveArrowType, headLen, headWid);
                }

                g2.setStroke(oldStroke);
                g2.setColor(oldColor);
            }

            // ===== False positive arrows =====
            if(showFalsePositiveDivisionArrows
                    && polygons != null && !CellDivisionInference.this.falsePositivePairCache.isEmpty()){

                Stroke oldStroke = g2.getStroke();
                Color oldColor = g2.getColor();

                g2.setStroke(new BasicStroke(Math.max(1.0f, falsePositiveArrowWidth),
                        BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
                g2.setColor(falsePositiveArrowColor);

                double headLen = Math.max(3.0, Math.min(60.0, falsePositiveArrowHeadLength * zoom));
                double headWid = Math.max(2.0, Math.min(40.0, falsePositiveArrowHeadWidth  * zoom));

                for(long k : CellDivisionInference.this.falsePositivePairCache.keySet()){
                    int a = (int)(k >> 32);
                    int b = (int)(k & 0xffffffffL);
                    if(a < 0 || b < 0 || a >= polygons.size() || b >= polygons.size()) continue;

                    List<Integer> pA = polygons.get(a);
                    List<Integer> pB = polygons.get(b);
                    if(pA == null || pB == null || pA.size() < 3 || pB.size() < 3) continue;

                    double[] cA = polygonCentroid(pA);
                    double[] cB = polygonCentroid(pB);

                    drawDivisionArrow(g2, cA[0]*zoom, cA[1]*zoom, cB[0]*zoom, cB[1]*zoom,
                            falsePositiveArrowType, headLen, headWid);
                }

                g2.setStroke(oldStroke);
                g2.setColor(oldColor);
            }

            // ===== false negative arrows =====
            if(showFalseNegativeDivisionArrows
        && polygons != null && !CellDivisionInference.this.falseNegativePairCache.isEmpty()){

                Stroke oldStroke = g2.getStroke();
                Color oldColor = g2.getColor();

                g2.setStroke(new BasicStroke(Math.max(1.0f, falseNegativeArrowWidth),
                        BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
                g2.setColor(falseNegativeArrowColor);

                double headLen = Math.max(3.0, Math.min(60.0, falseNegativeArrowHeadLength * zoom));
                double headWid = Math.max(2.0, Math.min(40.0, falseNegativeArrowHeadWidth  * zoom));

                for(long k : CellDivisionInference.this.falseNegativePairCache.keySet()){
                    int a = (int)(k >> 32);
                    int b = (int)(k & 0xffffffffL);
                    if(a < 0 || b < 0 || a >= polygons.size() || b >= polygons.size()) continue;

                    List<Integer> pA = polygons.get(a);
                    List<Integer> pB = polygons.get(b);
                    if(pA == null || pB == null || pA.size() < 3 || pB.size() < 3) continue;

                    double[] cA = polygonCentroid(pA);
                    double[] cB = polygonCentroid(pB);

                    drawDivisionArrow(g2, cA[0]*zoom, cA[1]*zoom, cB[0]*zoom, cB[1]*zoom,
                            falseNegativeArrowType, headLen, headWid);
                }

                g2.setStroke(oldStroke);
                g2.setColor(oldColor);
            }

            

            // vertices
            for(int i=0;i<overlayPoints.size();i++){

                Point p=overlayPoints.get(i);

                double cx=(p.x+0.5)*zoom;
                double cy=(p.y+0.5)*zoom;

                int r=vertexRadius;

                if(isPicked(i)){
                    g2.setColor(Color.ORANGE);
                }else if(debugHighlightedVertices.contains(i)){
                    g2.setColor(new Color(255, 0, 255));
                }else{
                    g2.setColor(i==selectedVertex ? Color.YELLOW : vertexColor);
                }
                g2.fillOval((int)(cx-r),(int)(cy-r),2*r,2*r);
            }
        }
    }
}