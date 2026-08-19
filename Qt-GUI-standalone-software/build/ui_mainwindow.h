/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.4.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>
#include "interactivegraphicsview.h"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionDebug_All;
    QAction *actionOpen_Raw_Image;
    QAction *actionOpen_Background;
    QAction *actionCreate_Canvas;
    QAction *actionDelete_Image;
    QAction *actionReset_All;
    QAction *actionAdd_Vertex;
    QAction *actionDelete_Vertex;
    QAction *actionDelete_All_Vertices;
    QAction *actionVertex_Display_Setting;
    QAction *actionFind_Vertex;
    QAction *actionAdd_Line;
    QAction *actionDelete_Line;
    QAction *actionDelete_All_Lines;
    QAction *actionFind_Line;
    QAction *actionLine_Display_Setting;
    QAction *actionAdd_Polygon;
    QAction *actionDelete_Polygon;
    QAction *actionDelete_All_Polygons;
    QAction *actionPolygon_Display_Setting;
    QAction *actionFind_Polygon;
    QAction *actionGenerate_Random_Network;
    QAction *actionDetect_Neighbor_Pairs;
    QAction *actionNeighbor_Pair_Display_Setting;
    QAction *actionDivision_Pair_Display_Setting;
    QAction *actionSkeletonization;
    QAction *actionVertex_Detection;
    QAction *actionDevelopment_Vertex_Detection;
    QAction *actionLine_Detection;
    QAction *actionPolygon_Detection;
    QAction *actionEdit_Setting;
    QAction *actionGeometry_Calculation_Setting;
    QAction *actionNeighbor_Pair_Geometry_Calculation;
    QAction *actionEstimate_division_by_single_geometry;
    QAction *actionCompare_with_real_division;
    QAction *actionExport_Setting;
    QAction *actionImport;
    QAction *actionImport_All_Data;
    QAction *actionExport;
    QAction *actionExport_All_Data;
    QAction *actionBatch_Neighbor_Pair_Geometry_Calculation;
    QAction *actionBatch_Single_Cell_Geometry_Calculation;
    QAction *actionBatch_Estimate_Division_by_single_geometry;
    QAction *actionBatch_Estimate_Division_by_designated_geometry;
    QAction *actionCheck_Geometry_Calculation_Single_Pair;
    QAction *actionPrecision_and_Recall_Curve_Over_Single_Geometry;
    QAction *actionComparing_Batch_Estimation;
    QAction *actionExport_Geometric_Feature_Names;
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout;
    QWidget *widget;
    QFormLayout *formLayout_2;
    QLabel *label_input_file_name;
    QLabel *label_input_file_name_value;
    QLabel *label_input_directory;
    QLabel *label_input_directory_value;
    QLabel *label_canvas_size;
    QLabel *label_canvas_size_value;
    QLabel *label_mouse_position;
    QLabel *label_mouse_position_value;
    QLabel *label_mouse_pixel;
    QLabel *label_mouse_pixel_value;
    QLabel *label_selected_item;
    QLabel *label_selected_item_name;
    QLabel *label_selected_item_id;
    QLabel *label_selected_item_id_value;
    QLabel *label_vertex_number;
    QLabel *label_vertex_number_value;
    QLabel *label_selected_item_pos;
    QLabel *label_selected_item_pos_value;
    QLabel *label_line_number;
    QLabel *label_line_number_value;
    QLabel *label_polygon_number;
    QLabel *label_polygon_number_value;
    QLabel *label_neighbor_polygon_number;
    QLabel *label_neighbor_polygon_number_value;
    QLabel *label_estimated_division_number;
    QLabel *label_estimated_division_number_value;
    QLabel *label_real_division_number;
    QLabel *label_real_division_number_value;
    QLabel *label_true_positive_number;
    QLabel *label_true_positive_number_value;
    QLabel *label_false_positive_number;
    QLabel *label_false_positive_number_value;
    QLabel *label_false_negative_number;
    QLabel *label_false_negative_number_value;
    QLabel *label_true_negative_number;
    QLabel *label_true_negative_number_value;
    QLabel *label_precision;
    QLabel *label_precision_value;
    QLabel *label_recall;
    QLabel *label_recall_value;
    QLabel *label_F1_score;
    QLabel *label_F1_score_value;
    QLabel *label_specificity;
    QLabel *label_specificity_value;
    QLabel *label_accuracy;
    QLabel *label_accuracy_value;
    InteractiveGraphicsView *graphicsView;
    QMenuBar *menubar;
    QMenu *menuOpen;
    QMenu *menuEdit;
    QMenu *menuDetection;
    QMenu *menuUnderDevelopment;
    QMenu *menuGeometry;
    QMenu *menuEstimate;
    QMenu *menuDisplay;
    QMenu *menuDebug;
    QMenu *menuFind;
    QMenu *menuImport_Export;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(800, 600);
        actionDebug_All = new QAction(MainWindow);
        actionDebug_All->setObjectName("actionDebug_All");
        actionOpen_Raw_Image = new QAction(MainWindow);
        actionOpen_Raw_Image->setObjectName("actionOpen_Raw_Image");
        actionOpen_Background = new QAction(MainWindow);
        actionOpen_Background->setObjectName("actionOpen_Background");
        actionCreate_Canvas = new QAction(MainWindow);
        actionCreate_Canvas->setObjectName("actionCreate_Canvas");
        actionDelete_Image = new QAction(MainWindow);
        actionDelete_Image->setObjectName("actionDelete_Image");
        actionReset_All = new QAction(MainWindow);
        actionReset_All->setObjectName("actionReset_All");
        actionAdd_Vertex = new QAction(MainWindow);
        actionAdd_Vertex->setObjectName("actionAdd_Vertex");
        actionDelete_Vertex = new QAction(MainWindow);
        actionDelete_Vertex->setObjectName("actionDelete_Vertex");
        actionDelete_All_Vertices = new QAction(MainWindow);
        actionDelete_All_Vertices->setObjectName("actionDelete_All_Vertices");
        actionVertex_Display_Setting = new QAction(MainWindow);
        actionVertex_Display_Setting->setObjectName("actionVertex_Display_Setting");
        actionFind_Vertex = new QAction(MainWindow);
        actionFind_Vertex->setObjectName("actionFind_Vertex");
        actionAdd_Line = new QAction(MainWindow);
        actionAdd_Line->setObjectName("actionAdd_Line");
        actionDelete_Line = new QAction(MainWindow);
        actionDelete_Line->setObjectName("actionDelete_Line");
        actionDelete_All_Lines = new QAction(MainWindow);
        actionDelete_All_Lines->setObjectName("actionDelete_All_Lines");
        actionFind_Line = new QAction(MainWindow);
        actionFind_Line->setObjectName("actionFind_Line");
        actionLine_Display_Setting = new QAction(MainWindow);
        actionLine_Display_Setting->setObjectName("actionLine_Display_Setting");
        actionAdd_Polygon = new QAction(MainWindow);
        actionAdd_Polygon->setObjectName("actionAdd_Polygon");
        actionDelete_Polygon = new QAction(MainWindow);
        actionDelete_Polygon->setObjectName("actionDelete_Polygon");
        actionDelete_All_Polygons = new QAction(MainWindow);
        actionDelete_All_Polygons->setObjectName("actionDelete_All_Polygons");
        actionPolygon_Display_Setting = new QAction(MainWindow);
        actionPolygon_Display_Setting->setObjectName("actionPolygon_Display_Setting");
        actionFind_Polygon = new QAction(MainWindow);
        actionFind_Polygon->setObjectName("actionFind_Polygon");
        actionGenerate_Random_Network = new QAction(MainWindow);
        actionGenerate_Random_Network->setObjectName("actionGenerate_Random_Network");
        actionDetect_Neighbor_Pairs = new QAction(MainWindow);
        actionDetect_Neighbor_Pairs->setObjectName("actionDetect_Neighbor_Pairs");
        actionNeighbor_Pair_Display_Setting = new QAction(MainWindow);
        actionNeighbor_Pair_Display_Setting->setObjectName("actionNeighbor_Pair_Display_Setting");
        actionDivision_Pair_Display_Setting = new QAction(MainWindow);
        actionDivision_Pair_Display_Setting->setObjectName("actionDivision_Pair_Display_Setting");
        actionSkeletonization = new QAction(MainWindow);
        actionSkeletonization->setObjectName("actionSkeletonization");
        actionVertex_Detection = new QAction(MainWindow);
        actionVertex_Detection->setObjectName("actionVertex_Detection");
        actionDevelopment_Vertex_Detection = new QAction(MainWindow);
        actionDevelopment_Vertex_Detection->setObjectName("actionDevelopment_Vertex_Detection");
        actionLine_Detection = new QAction(MainWindow);
        actionLine_Detection->setObjectName("actionLine_Detection");
        actionPolygon_Detection = new QAction(MainWindow);
        actionPolygon_Detection->setObjectName("actionPolygon_Detection");
        actionEdit_Setting = new QAction(MainWindow);
        actionEdit_Setting->setObjectName("actionEdit_Setting");
        actionGeometry_Calculation_Setting = new QAction(MainWindow);
        actionGeometry_Calculation_Setting->setObjectName("actionGeometry_Calculation_Setting");
        actionNeighbor_Pair_Geometry_Calculation = new QAction(MainWindow);
        actionNeighbor_Pair_Geometry_Calculation->setObjectName("actionNeighbor_Pair_Geometry_Calculation");
        actionEstimate_division_by_single_geometry = new QAction(MainWindow);
        actionEstimate_division_by_single_geometry->setObjectName("actionEstimate_division_by_single_geometry");
        actionCompare_with_real_division = new QAction(MainWindow);
        actionCompare_with_real_division->setObjectName("actionCompare_with_real_division");
        actionExport_Setting = new QAction(MainWindow);
        actionExport_Setting->setObjectName("actionExport_Setting");
        actionImport = new QAction(MainWindow);
        actionImport->setObjectName("actionImport");
        actionImport_All_Data = new QAction(MainWindow);
        actionImport_All_Data->setObjectName("actionImport_All_Data");
        actionExport = new QAction(MainWindow);
        actionExport->setObjectName("actionExport");
        actionExport_All_Data = new QAction(MainWindow);
        actionExport_All_Data->setObjectName("actionExport_All_Data");
        actionBatch_Neighbor_Pair_Geometry_Calculation = new QAction(MainWindow);
        actionBatch_Neighbor_Pair_Geometry_Calculation->setObjectName("actionBatch_Neighbor_Pair_Geometry_Calculation");
        actionBatch_Single_Cell_Geometry_Calculation = new QAction(MainWindow);
        actionBatch_Single_Cell_Geometry_Calculation->setObjectName("actionBatch_Single_Cell_Geometry_Calculation");
        actionBatch_Estimate_Division_by_single_geometry = new QAction(MainWindow);
        actionBatch_Estimate_Division_by_single_geometry->setObjectName("actionBatch_Estimate_Division_by_single_geometry");
        actionBatch_Estimate_Division_by_designated_geometry = new QAction(MainWindow);
        actionBatch_Estimate_Division_by_designated_geometry->setObjectName("actionBatch_Estimate_Division_by_designated_geometry");
        actionCheck_Geometry_Calculation_Single_Pair = new QAction(MainWindow);
        actionCheck_Geometry_Calculation_Single_Pair->setObjectName("actionCheck_Geometry_Calculation_Single_Pair");
        actionPrecision_and_Recall_Curve_Over_Single_Geometry = new QAction(MainWindow);
        actionPrecision_and_Recall_Curve_Over_Single_Geometry->setObjectName("actionPrecision_and_Recall_Curve_Over_Single_Geometry");
        actionComparing_Batch_Estimation = new QAction(MainWindow);
        actionComparing_Batch_Estimation->setObjectName("actionComparing_Batch_Estimation");
        actionExport_Geometric_Feature_Names = new QAction(MainWindow);
        actionExport_Geometric_Feature_Names->setObjectName("actionExport_Geometric_Feature_Names");
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        horizontalLayout = new QHBoxLayout(centralwidget);
        horizontalLayout->setObjectName("horizontalLayout");
        widget = new QWidget(centralwidget);
        widget->setObjectName("widget");
        formLayout_2 = new QFormLayout(widget);
        formLayout_2->setObjectName("formLayout_2");
        label_input_file_name = new QLabel(widget);
        label_input_file_name->setObjectName("label_input_file_name");

        formLayout_2->setWidget(0, QFormLayout::LabelRole, label_input_file_name);

        label_input_file_name_value = new QLabel(widget);
        label_input_file_name_value->setObjectName("label_input_file_name_value");

        formLayout_2->setWidget(0, QFormLayout::FieldRole, label_input_file_name_value);

        label_input_directory = new QLabel(widget);
        label_input_directory->setObjectName("label_input_directory");

        formLayout_2->setWidget(1, QFormLayout::LabelRole, label_input_directory);

        label_input_directory_value = new QLabel(widget);
        label_input_directory_value->setObjectName("label_input_directory_value");
        label_input_directory_value->setWordWrap(true);

        formLayout_2->setWidget(1, QFormLayout::FieldRole, label_input_directory_value);

        label_canvas_size = new QLabel(widget);
        label_canvas_size->setObjectName("label_canvas_size");

        formLayout_2->setWidget(2, QFormLayout::LabelRole, label_canvas_size);

        label_canvas_size_value = new QLabel(widget);
        label_canvas_size_value->setObjectName("label_canvas_size_value");

        formLayout_2->setWidget(2, QFormLayout::FieldRole, label_canvas_size_value);

        label_mouse_position = new QLabel(widget);
        label_mouse_position->setObjectName("label_mouse_position");

        formLayout_2->setWidget(3, QFormLayout::LabelRole, label_mouse_position);

        label_mouse_position_value = new QLabel(widget);
        label_mouse_position_value->setObjectName("label_mouse_position_value");

        formLayout_2->setWidget(3, QFormLayout::FieldRole, label_mouse_position_value);

        label_mouse_pixel = new QLabel(widget);
        label_mouse_pixel->setObjectName("label_mouse_pixel");

        formLayout_2->setWidget(4, QFormLayout::LabelRole, label_mouse_pixel);

        label_mouse_pixel_value = new QLabel(widget);
        label_mouse_pixel_value->setObjectName("label_mouse_pixel_value");

        formLayout_2->setWidget(4, QFormLayout::FieldRole, label_mouse_pixel_value);

        label_selected_item = new QLabel(widget);
        label_selected_item->setObjectName("label_selected_item");

        formLayout_2->setWidget(5, QFormLayout::LabelRole, label_selected_item);

        label_selected_item_name = new QLabel(widget);
        label_selected_item_name->setObjectName("label_selected_item_name");

        formLayout_2->setWidget(5, QFormLayout::FieldRole, label_selected_item_name);

        label_selected_item_id = new QLabel(widget);
        label_selected_item_id->setObjectName("label_selected_item_id");

        formLayout_2->setWidget(6, QFormLayout::LabelRole, label_selected_item_id);

        label_selected_item_id_value = new QLabel(widget);
        label_selected_item_id_value->setObjectName("label_selected_item_id_value");

        formLayout_2->setWidget(6, QFormLayout::FieldRole, label_selected_item_id_value);

        label_vertex_number = new QLabel(widget);
        label_vertex_number->setObjectName("label_vertex_number");

        formLayout_2->setWidget(8, QFormLayout::LabelRole, label_vertex_number);

        label_vertex_number_value = new QLabel(widget);
        label_vertex_number_value->setObjectName("label_vertex_number_value");

        formLayout_2->setWidget(8, QFormLayout::FieldRole, label_vertex_number_value);

        label_selected_item_pos = new QLabel(widget);
        label_selected_item_pos->setObjectName("label_selected_item_pos");

        formLayout_2->setWidget(7, QFormLayout::LabelRole, label_selected_item_pos);

        label_selected_item_pos_value = new QLabel(widget);
        label_selected_item_pos_value->setObjectName("label_selected_item_pos_value");

        formLayout_2->setWidget(7, QFormLayout::FieldRole, label_selected_item_pos_value);

        label_line_number = new QLabel(widget);
        label_line_number->setObjectName("label_line_number");

        formLayout_2->setWidget(9, QFormLayout::LabelRole, label_line_number);

        label_line_number_value = new QLabel(widget);
        label_line_number_value->setObjectName("label_line_number_value");

        formLayout_2->setWidget(9, QFormLayout::FieldRole, label_line_number_value);

        label_polygon_number = new QLabel(widget);
        label_polygon_number->setObjectName("label_polygon_number");

        formLayout_2->setWidget(10, QFormLayout::LabelRole, label_polygon_number);

        label_polygon_number_value = new QLabel(widget);
        label_polygon_number_value->setObjectName("label_polygon_number_value");

        formLayout_2->setWidget(10, QFormLayout::FieldRole, label_polygon_number_value);

        label_neighbor_polygon_number = new QLabel(widget);
        label_neighbor_polygon_number->setObjectName("label_neighbor_polygon_number");

        formLayout_2->setWidget(11, QFormLayout::LabelRole, label_neighbor_polygon_number);

        label_neighbor_polygon_number_value = new QLabel(widget);
        label_neighbor_polygon_number_value->setObjectName("label_neighbor_polygon_number_value");

        formLayout_2->setWidget(11, QFormLayout::FieldRole, label_neighbor_polygon_number_value);

        label_estimated_division_number = new QLabel(widget);
        label_estimated_division_number->setObjectName("label_estimated_division_number");

        formLayout_2->setWidget(12, QFormLayout::LabelRole, label_estimated_division_number);

        label_estimated_division_number_value = new QLabel(widget);
        label_estimated_division_number_value->setObjectName("label_estimated_division_number_value");

        formLayout_2->setWidget(12, QFormLayout::FieldRole, label_estimated_division_number_value);

        label_real_division_number = new QLabel(widget);
        label_real_division_number->setObjectName("label_real_division_number");

        formLayout_2->setWidget(13, QFormLayout::LabelRole, label_real_division_number);

        label_real_division_number_value = new QLabel(widget);
        label_real_division_number_value->setObjectName("label_real_division_number_value");

        formLayout_2->setWidget(13, QFormLayout::FieldRole, label_real_division_number_value);

        label_true_positive_number = new QLabel(widget);
        label_true_positive_number->setObjectName("label_true_positive_number");

        formLayout_2->setWidget(14, QFormLayout::LabelRole, label_true_positive_number);

        label_true_positive_number_value = new QLabel(widget);
        label_true_positive_number_value->setObjectName("label_true_positive_number_value");

        formLayout_2->setWidget(14, QFormLayout::FieldRole, label_true_positive_number_value);

        label_false_positive_number = new QLabel(widget);
        label_false_positive_number->setObjectName("label_false_positive_number");

        formLayout_2->setWidget(15, QFormLayout::LabelRole, label_false_positive_number);

        label_false_positive_number_value = new QLabel(widget);
        label_false_positive_number_value->setObjectName("label_false_positive_number_value");

        formLayout_2->setWidget(15, QFormLayout::FieldRole, label_false_positive_number_value);

        label_false_negative_number = new QLabel(widget);
        label_false_negative_number->setObjectName("label_false_negative_number");

        formLayout_2->setWidget(16, QFormLayout::LabelRole, label_false_negative_number);

        label_false_negative_number_value = new QLabel(widget);
        label_false_negative_number_value->setObjectName("label_false_negative_number_value");

        formLayout_2->setWidget(16, QFormLayout::FieldRole, label_false_negative_number_value);

        label_true_negative_number = new QLabel(widget);
        label_true_negative_number->setObjectName("label_true_negative_number");

        formLayout_2->setWidget(17, QFormLayout::LabelRole, label_true_negative_number);

        label_true_negative_number_value = new QLabel(widget);
        label_true_negative_number_value->setObjectName("label_true_negative_number_value");

        formLayout_2->setWidget(17, QFormLayout::FieldRole, label_true_negative_number_value);

        label_precision = new QLabel(widget);
        label_precision->setObjectName("label_precision");

        formLayout_2->setWidget(18, QFormLayout::LabelRole, label_precision);

        label_precision_value = new QLabel(widget);
        label_precision_value->setObjectName("label_precision_value");

        formLayout_2->setWidget(18, QFormLayout::FieldRole, label_precision_value);

        label_recall = new QLabel(widget);
        label_recall->setObjectName("label_recall");

        formLayout_2->setWidget(19, QFormLayout::LabelRole, label_recall);

        label_recall_value = new QLabel(widget);
        label_recall_value->setObjectName("label_recall_value");

        formLayout_2->setWidget(19, QFormLayout::FieldRole, label_recall_value);

        label_F1_score = new QLabel(widget);
        label_F1_score->setObjectName("label_F1_score");

        formLayout_2->setWidget(20, QFormLayout::LabelRole, label_F1_score);

        label_F1_score_value = new QLabel(widget);
        label_F1_score_value->setObjectName("label_F1_score_value");

        formLayout_2->setWidget(20, QFormLayout::FieldRole, label_F1_score_value);

        label_specificity = new QLabel(widget);
        label_specificity->setObjectName("label_specificity");

        formLayout_2->setWidget(21, QFormLayout::LabelRole, label_specificity);

        label_specificity_value = new QLabel(widget);
        label_specificity_value->setObjectName("label_specificity_value");

        formLayout_2->setWidget(21, QFormLayout::FieldRole, label_specificity_value);

        label_accuracy = new QLabel(widget);
        label_accuracy->setObjectName("label_accuracy");

        formLayout_2->setWidget(22, QFormLayout::LabelRole, label_accuracy);

        label_accuracy_value = new QLabel(widget);
        label_accuracy_value->setObjectName("label_accuracy_value");

        formLayout_2->setWidget(22, QFormLayout::FieldRole, label_accuracy_value);


        horizontalLayout->addWidget(widget);

        graphicsView = new InteractiveGraphicsView(centralwidget);
        graphicsView->setObjectName("graphicsView");

        horizontalLayout->addWidget(graphicsView);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 800, 20));
        menuOpen = new QMenu(menubar);
        menuOpen->setObjectName("menuOpen");
        menuEdit = new QMenu(menubar);
        menuEdit->setObjectName("menuEdit");
        menuDetection = new QMenu(menubar);
        menuDetection->setObjectName("menuDetection");
        menuUnderDevelopment = new QMenu(menubar);
        menuUnderDevelopment->setObjectName("menuUnderDevelopment");
        menuGeometry = new QMenu(menubar);
        menuGeometry->setObjectName("menuGeometry");
        menuEstimate = new QMenu(menubar);
        menuEstimate->setObjectName("menuEstimate");
        menuDisplay = new QMenu(menubar);
        menuDisplay->setObjectName("menuDisplay");
        menuDebug = new QMenu(menubar);
        menuDebug->setObjectName("menuDebug");
        menuFind = new QMenu(menubar);
        menuFind->setObjectName("menuFind");
        menuImport_Export = new QMenu(menubar);
        menuImport_Export->setObjectName("menuImport_Export");
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        menubar->addAction(menuOpen->menuAction());
        menubar->addAction(menuDetection->menuAction());
        menubar->addAction(menuUnderDevelopment->menuAction());
        menubar->addAction(menuGeometry->menuAction());
        menubar->addAction(menuEstimate->menuAction());
        menubar->addAction(menuDisplay->menuAction());
        menubar->addAction(menuEdit->menuAction());
        menubar->addAction(menuFind->menuAction());
        menubar->addAction(menuImport_Export->menuAction());
        menubar->addAction(menuDebug->menuAction());
        menuOpen->addAction(actionOpen_Raw_Image);
        menuOpen->addAction(actionOpen_Background);
        menuOpen->addAction(actionCreate_Canvas);
        menuEdit->addAction(actionReset_All);
        menuEdit->addSeparator();
        menuEdit->addAction(actionEdit_Setting);
        menuEdit->addAction(actionDelete_Image);
        menuEdit->addAction(actionAdd_Vertex);
        menuEdit->addAction(actionDelete_Vertex);
        menuEdit->addAction(actionDelete_All_Vertices);
        menuEdit->addAction(actionAdd_Line);
        menuEdit->addAction(actionDelete_Line);
        menuEdit->addAction(actionDelete_All_Lines);
        menuEdit->addAction(actionAdd_Polygon);
        menuEdit->addAction(actionDelete_Polygon);
        menuEdit->addAction(actionDelete_All_Polygons);
        menuDetection->addAction(actionSkeletonization);
        menuDetection->addAction(actionVertex_Detection);
        menuDetection->addAction(actionLine_Detection);
        menuDetection->addAction(actionPolygon_Detection);
        menuDetection->addAction(actionDetect_Neighbor_Pairs);
        menuUnderDevelopment->addAction(actionDevelopment_Vertex_Detection);
        menuGeometry->addAction(actionGeometry_Calculation_Setting);
        menuGeometry->addAction(actionNeighbor_Pair_Geometry_Calculation);
        menuGeometry->addAction(actionBatch_Neighbor_Pair_Geometry_Calculation);
        menuGeometry->addAction(actionBatch_Single_Cell_Geometry_Calculation);
        menuEstimate->addAction(actionEstimate_division_by_single_geometry);
        menuEstimate->addAction(actionCompare_with_real_division);
        menuEstimate->addAction(actionBatch_Estimate_Division_by_single_geometry);
        menuEstimate->addAction(actionBatch_Estimate_Division_by_designated_geometry);
        menuEstimate->addAction(actionPrecision_and_Recall_Curve_Over_Single_Geometry);
        menuDisplay->addAction(actionVertex_Display_Setting);
        menuDisplay->addAction(actionLine_Display_Setting);
        menuDisplay->addAction(actionPolygon_Display_Setting);
        menuDisplay->addAction(actionNeighbor_Pair_Display_Setting);
        menuDisplay->addAction(actionDivision_Pair_Display_Setting);
        menuDebug->addAction(actionGenerate_Random_Network);
        menuDebug->addAction(actionDebug_All);
        menuDebug->addAction(actionCheck_Geometry_Calculation_Single_Pair);
        menuDebug->addAction(actionComparing_Batch_Estimation);
        menuDebug->addAction(actionExport_Geometric_Feature_Names);
        menuFind->addAction(actionFind_Vertex);
        menuFind->addAction(actionFind_Line);
        menuFind->addAction(actionFind_Polygon);
        menuImport_Export->addAction(actionImport);
        menuImport_Export->addAction(actionImport_All_Data);
        menuImport_Export->addAction(actionExport);
        menuImport_Export->addAction(actionExport_All_Data);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        actionDebug_All->setText(QCoreApplication::translate("MainWindow", "Debug All", nullptr));
        actionOpen_Raw_Image->setText(QCoreApplication::translate("MainWindow", "Open Raw Image", nullptr));
        actionOpen_Background->setText(QCoreApplication::translate("MainWindow", "Open Background", nullptr));
        actionCreate_Canvas->setText(QCoreApplication::translate("MainWindow", "Create Canvas", nullptr));
        actionDelete_Image->setText(QCoreApplication::translate("MainWindow", "Delete Image", nullptr));
        actionReset_All->setText(QCoreApplication::translate("MainWindow", "Reset All", nullptr));
        actionAdd_Vertex->setText(QCoreApplication::translate("MainWindow", "Add Vertex", nullptr));
        actionDelete_Vertex->setText(QCoreApplication::translate("MainWindow", "Delete Vertex", nullptr));
        actionDelete_All_Vertices->setText(QCoreApplication::translate("MainWindow", "Delete All Vertices", nullptr));
        actionVertex_Display_Setting->setText(QCoreApplication::translate("MainWindow", "Vertex Display Setting", nullptr));
        actionFind_Vertex->setText(QCoreApplication::translate("MainWindow", "Find Vertex", nullptr));
        actionAdd_Line->setText(QCoreApplication::translate("MainWindow", "Add Line", nullptr));
        actionDelete_Line->setText(QCoreApplication::translate("MainWindow", "Delete Line", nullptr));
        actionDelete_All_Lines->setText(QCoreApplication::translate("MainWindow", "Delete All Lines", nullptr));
        actionFind_Line->setText(QCoreApplication::translate("MainWindow", "Find Line", nullptr));
        actionLine_Display_Setting->setText(QCoreApplication::translate("MainWindow", "Line Display Setting", nullptr));
        actionAdd_Polygon->setText(QCoreApplication::translate("MainWindow", "Add Polygon", nullptr));
        actionDelete_Polygon->setText(QCoreApplication::translate("MainWindow", "Delete Polygon", nullptr));
        actionDelete_All_Polygons->setText(QCoreApplication::translate("MainWindow", "Delete All Polygons", nullptr));
        actionPolygon_Display_Setting->setText(QCoreApplication::translate("MainWindow", "Polygon Display Setting", nullptr));
        actionFind_Polygon->setText(QCoreApplication::translate("MainWindow", "Find Polygon", nullptr));
        actionGenerate_Random_Network->setText(QCoreApplication::translate("MainWindow", "Generate Random Network", nullptr));
        actionDetect_Neighbor_Pairs->setText(QCoreApplication::translate("MainWindow", "Detect Neighbor Pairs", nullptr));
        actionNeighbor_Pair_Display_Setting->setText(QCoreApplication::translate("MainWindow", "Neighbor Pair Display Setting", nullptr));
        actionDivision_Pair_Display_Setting->setText(QCoreApplication::translate("MainWindow", "Division Pair Display Setting", nullptr));
        actionSkeletonization->setText(QCoreApplication::translate("MainWindow", "Skeletonization", nullptr));
        actionVertex_Detection->setText(QCoreApplication::translate("MainWindow", "Vertex Detection", nullptr));
        actionDevelopment_Vertex_Detection->setText(QCoreApplication::translate("MainWindow", "Vertex Detection", nullptr));
        actionLine_Detection->setText(QCoreApplication::translate("MainWindow", "Line Detection", nullptr));
        actionPolygon_Detection->setText(QCoreApplication::translate("MainWindow", "Polygon Detection", nullptr));
        actionEdit_Setting->setText(QCoreApplication::translate("MainWindow", "Edit Setting", nullptr));
        actionGeometry_Calculation_Setting->setText(QCoreApplication::translate("MainWindow", "Geometry Calculation Setting", nullptr));
        actionNeighbor_Pair_Geometry_Calculation->setText(QCoreApplication::translate("MainWindow", "Neighbor Pair Geometry Calculation", nullptr));
        actionEstimate_division_by_single_geometry->setText(QCoreApplication::translate("MainWindow", "Estimate division by single geometry", nullptr));
        actionCompare_with_real_division->setText(QCoreApplication::translate("MainWindow", "Compare with real division", nullptr));
        actionExport_Setting->setText(QCoreApplication::translate("MainWindow", "Export Setting", nullptr));
        actionImport->setText(QCoreApplication::translate("MainWindow", "Import", nullptr));
        actionImport_All_Data->setText(QCoreApplication::translate("MainWindow", "Import All Data (JSON)", nullptr));
        actionExport->setText(QCoreApplication::translate("MainWindow", "Export", nullptr));
        actionExport_All_Data->setText(QCoreApplication::translate("MainWindow", "Export All Data (JSON)", nullptr));
        actionBatch_Neighbor_Pair_Geometry_Calculation->setText(QCoreApplication::translate("MainWindow", "Batch Neighbor Pair Geometry Calculation", nullptr));
        actionBatch_Single_Cell_Geometry_Calculation->setText(QCoreApplication::translate("MainWindow", "Batch Single Cell Geometry Calculation", nullptr));
        actionBatch_Estimate_Division_by_single_geometry->setText(QCoreApplication::translate("MainWindow", "Batch Estimate Division by single geometry", nullptr));
        actionBatch_Estimate_Division_by_designated_geometry->setText(QCoreApplication::translate("MainWindow", "Batch Estimate Division by designated geometry", nullptr));
        actionCheck_Geometry_Calculation_Single_Pair->setText(QCoreApplication::translate("MainWindow", "Check Geometry Calculation (Single Pair)", nullptr));
        actionPrecision_and_Recall_Curve_Over_Single_Geometry->setText(QCoreApplication::translate("MainWindow", "Batch Estimate Division by ranging single geometry", nullptr));
        actionComparing_Batch_Estimation->setText(QCoreApplication::translate("MainWindow", "Comparing Batch Estimation", nullptr));
        actionExport_Geometric_Feature_Names->setText(QCoreApplication::translate("MainWindow", "Export Geometric Feature Names", nullptr));
        label_input_file_name->setText(QCoreApplication::translate("MainWindow", "Input File Name", nullptr));
        label_input_file_name_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_input_directory->setText(QCoreApplication::translate("MainWindow", "Input Directory", nullptr));
        label_input_directory_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_canvas_size->setText(QCoreApplication::translate("MainWindow", "Canvas Size", nullptr));
        label_canvas_size_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_mouse_position->setText(QCoreApplication::translate("MainWindow", "Mouse Position", nullptr));
        label_mouse_position_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_mouse_pixel->setText(QCoreApplication::translate("MainWindow", "Mouse Pixel", nullptr));
        label_mouse_pixel_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_selected_item->setText(QCoreApplication::translate("MainWindow", "Selected Item", nullptr));
        label_selected_item_name->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_selected_item_id->setText(QCoreApplication::translate("MainWindow", "Selected Item Id", nullptr));
        label_selected_item_id_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_vertex_number->setText(QCoreApplication::translate("MainWindow", "Vertex Num", nullptr));
        label_vertex_number_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_selected_item_pos->setText(QCoreApplication::translate("MainWindow", "Selected Item Pos", nullptr));
        label_selected_item_pos_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_line_number->setText(QCoreApplication::translate("MainWindow", "Line Num", nullptr));
        label_line_number_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_polygon_number->setText(QCoreApplication::translate("MainWindow", "Polygon Num", nullptr));
        label_polygon_number_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_neighbor_polygon_number->setText(QCoreApplication::translate("MainWindow", "Neighbor Polygon Num", nullptr));
        label_neighbor_polygon_number_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_estimated_division_number->setText(QCoreApplication::translate("MainWindow", "Estimated Division Num", nullptr));
        label_estimated_division_number_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_real_division_number->setText(QCoreApplication::translate("MainWindow", "Real Division Num", nullptr));
        label_real_division_number_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_true_positive_number->setText(QCoreApplication::translate("MainWindow", "True Positive Num", nullptr));
        label_true_positive_number_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_false_positive_number->setText(QCoreApplication::translate("MainWindow", "False Postivie Num", nullptr));
        label_false_positive_number_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_false_negative_number->setText(QCoreApplication::translate("MainWindow", "False Negative Num", nullptr));
        label_false_negative_number_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_true_negative_number->setText(QCoreApplication::translate("MainWindow", "True Negative Num", nullptr));
        label_true_negative_number_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_precision->setText(QCoreApplication::translate("MainWindow", "Precision", nullptr));
        label_precision_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_recall->setText(QCoreApplication::translate("MainWindow", "Recall", nullptr));
        label_recall_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_F1_score->setText(QCoreApplication::translate("MainWindow", "F1 score", nullptr));
        label_F1_score_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_specificity->setText(QCoreApplication::translate("MainWindow", "Specificity", nullptr));
        label_specificity_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        label_accuracy->setText(QCoreApplication::translate("MainWindow", "Accuracy", nullptr));
        label_accuracy_value->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        menuOpen->setTitle(QCoreApplication::translate("MainWindow", "Open", nullptr));
        menuEdit->setTitle(QCoreApplication::translate("MainWindow", "Edit", nullptr));
        menuDetection->setTitle(QCoreApplication::translate("MainWindow", "Detect", nullptr));
        menuUnderDevelopment->setTitle(QCoreApplication::translate("MainWindow", "Under development", nullptr));
        menuGeometry->setTitle(QCoreApplication::translate("MainWindow", "Geometry", nullptr));
        menuEstimate->setTitle(QCoreApplication::translate("MainWindow", "Estimate", nullptr));
        menuDisplay->setTitle(QCoreApplication::translate("MainWindow", "Display", nullptr));
        menuDebug->setTitle(QCoreApplication::translate("MainWindow", "Debug", nullptr));
        menuFind->setTitle(QCoreApplication::translate("MainWindow", "Find", nullptr));
        menuImport_Export->setTitle(QCoreApplication::translate("MainWindow", "Import && Export", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
