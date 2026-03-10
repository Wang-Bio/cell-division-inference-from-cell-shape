#include "canvasmanager.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QInputDialog>
#include <QColorDialog>
#include <QBrush>
#include <QPen>
#include <QDebug>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QFileInfo>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

CanvasManager::CanvasManager(QObject *parent)
    : QObject(parent)
{
}

CanvasInfo CanvasManager::createCanvasWithDialog(QGraphicsView *view, QWidget *dialogParent)
{
    CanvasInfo info;
    info.valid = false;

    if(!view) return info;

    //Create dialog
    QDialog dialog(dialogParent);
    dialog.setWindowTitle("Create Canvas");

    //UI elements
    QSpinBox *spinWidth = new QSpinBox(&dialog);
    QSpinBox *spinHeight = new QSpinBox(&dialog);
    QPushButton *btnColor = new QPushButton("Choose Color", &dialog);
    QLabel *colorPreview = new QLabel(&dialog);

    spinWidth->setRange(1,100000);
    spinHeight->setRange(1,100000);
    spinWidth->setValue(512);
    spinHeight->setValue(512);
    QColor bgColor = Qt::black;

    //Set up preview box
    colorPreview->setFixedSize(40,20);
    colorPreview->setAutoFillBackground(true);
    QPalette pal = colorPreview->palette();
    pal.setColor(QPalette::Window, bgColor);
    colorPreview->setPalette(pal);

    // Connect color button
    QObject::connect(btnColor, &QPushButton::clicked, [&]() {
        QColor c = QColorDialog::getColor(bgColor, &dialog, "Choose Background Color");
        if (c.isValid()) {
            bgColor = c;
            QPalette pal2 = colorPreview->palette();
            pal2.setColor(QPalette::Window, bgColor);
            colorPreview->setPalette(pal2);
        }
    });

    // --- Layout ---
    QFormLayout *form = new QFormLayout;
    form->addRow("Width:", spinWidth);
    form->addRow("Height:", spinHeight);

    // color horizontal layout
    QHBoxLayout *colorLayout = new QHBoxLayout;
    colorLayout->addWidget(colorPreview);
    colorLayout->addWidget(btnColor);
    form->addRow("Color:", colorLayout);

    // OK / Cancel buttons
    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addLayout(form);
    vbox->addWidget(buttons);

    dialog.setLayout(vbox);

    // Show the dialog
    if (dialog.exec() != QDialog::Accepted)
        return info;

    int width = spinWidth->value();
    int height = spinHeight->value();

    // Remove old scene completely
    if (QGraphicsScene *oldScene = view->scene()) {
        view->setScene(nullptr);
        delete oldScene;
    }

    // --- Create scene ---
    auto *scene = new QGraphicsScene(view);
    scene->setSceneRect(0, 0, width, height);
    scene->setBackgroundBrush(bgColor);

    // Optional border
    scene->addRect(0, 0, width, height, QPen(Qt::black));

    // Assign to view
    view->setScene(scene);
    view->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);

    //return
    info.width = width;
    info.height = height;
    info.color = bgColor;
    info.valid = true;

    return info;
}


static QImage matToQImage(const cv::Mat &mat)
{
    if(mat.empty()) return QImage();

    cv::Mat rgb;

    switch(mat.type())
    {
    case CV_8UC1:
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8).copy();

    case CV_8UC3:
    //BGR -> RGB
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();

    case CV_8UC4:
    //BGRA -> RGBA
        cv::cvtColor(mat, rgb, cv::COLOR_BGRA2RGBA);
        return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGBA8888).copy();

    default:
        qWarning()<<"matToQImage: unsupported cv::Mat type:" <<mat.type();
        return QImage();
    }
}

ImageInfo CanvasManager::openRawImage(QGraphicsView *view, QWidget *dialogParent)
{
    return openRawImage(view, dialogParent, QString());
}

ImageInfo CanvasManager::openRawImage(QGraphicsView *view, QWidget *dialogParent, const QString &filePath)
{
    ImageInfo info;
    info.valid = false;
    info.image = cv::Mat();
    info.filePath.clear();
    info.fileName.clear();

    if(!view) return info;

    QString resolvedPath = filePath;
    //1. File dialog
    if(resolvedPath.isEmpty()){
        QString filter = "Images (*.png *.jpg *.jpeg *.tif *.tiff *.bmp);;All Files (*.*)";
        resolvedPath = QFileDialog::getOpenFileName(dialogParent, "Open Raw Image", QString(), filter);
        if(resolvedPath.isEmpty()) return info;
    }

    //2. Load Image with OpenCV
    cv::Mat img = cv::imread(resolvedPath.toStdString(), cv::IMREAD_COLOR);
    if(img.empty())
    {
        qWarning() << "Failed to load image:" <<resolvedPath;
        return info;
    }
    //Fill into struct
    info.image = img;
    info.filePath = resolvedPath;
    info.fileName = QFileInfo(resolvedPath).fileName();
    info.valid = true;

    //3. Convert to QImage
    QImage qimg = matToQImage(img);
    if(qimg.isNull()){
        qWarning() << "Failed to convert cv::Mat to QImage: "<<filePath;
        info.valid = false;
        return info;
    }

    QPixmap pixmap = QPixmap::fromImage(qimg);

    //4. Prepare scene
    QGraphicsScene *scene = view->scene();
    if(!scene)
    {
        scene = new QGraphicsScene(view);
        view->setScene(scene);
    }

    scene->clear();
    scene->setSceneRect(0,0,qimg.width(),qimg.height());

    //5. Add pximap to scene
    QGraphicsPixmapItem *item = scene->addPixmap(pixmap);
    item->setPos(0,0);

    //6. Fit in view
    view->fitInView(scene->sceneRect(),Qt::KeepAspectRatio);

    qDebug()<<"Loaded image:" <<filePath<< "; size = "<<img.cols<<" x "<<img.rows<<"; channels = "<<img.channels();

    return info;
}

void CanvasManager::deleteImageAndCanvas(QGraphicsView *view, bool resetViewTransform)
{
    if(!view) return;

    if(QGraphicsScene *scene = view->scene()){
        view->setScene(nullptr);
        delete scene;
    }

    if(resetViewTransform){
        view->resetTransform();
    }
}
