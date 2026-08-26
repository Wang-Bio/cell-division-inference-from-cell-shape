#include "overlapindexanalysis.h"

#include <QBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHash>
#include <QImage>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
constexpr int Bins = 40;
struct Result { QString feature, label; double overlap; int daughter, nonDaughter; int order; };

QVector<QStringList> parseCsv(const QString &text)
{
    QVector<QStringList> rows; QStringList row; QString field; bool quoted = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (quoted) {
            if (c == '"' && i + 1 < text.size() && text[i + 1] == '"') { field += '"'; ++i; }
            else if (c == '"') quoted = false;
            else field += c;
        } else if (c == '"' && field.isEmpty()) quoted = true;
        else if (c == ',') { row << field; field.clear(); }
        else if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            row << field; field.clear(); rows << row; row.clear();
        } else field += c;
    }
    if (!field.isEmpty() || !row.isEmpty()) { row << field; rows << row; }
    return rows;
}

QString quoteCsv(QString s)
{
    if (s.contains('"')) s.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return (s.contains(',') || s.contains('"') || s.contains('\n') || s.contains('\r'))
            ? QStringLiteral("\"") + s + QStringLiteral("\"") : s;
}

int columnIndex(const QStringList &headers, const QString &wanted)
{
    const int exact = headers.indexOf(wanted); if (exact >= 0) return exact;
    for (int i = 0; i < headers.size(); ++i)
        if (headers[i].compare(wanted, Qt::CaseInsensitive) == 0) return i;
    return -1;
}

bool number(const QString &s, double *value)
{
    bool ok = false; const double v = s.trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(v)) return false;
    *value = v; return true;
}

QHash<QString, QString> featureNames()
{
    return {
        {"areaRatio","area_ratio"},{"areaMean","area_mean"},{"areaMin","area_min"},{"areaMax","area_max"},{"areaDiff","area_difference"},
        {"perimeterRatio","perimeter_ratio"},{"perimeterMean","perimeter_mean"},{"perimeterMin","perimeter_min"},{"perimeterMax","perimeter_max"},{"perimeterDiff","perimeter_difference"},
        {"aspectRatio","aspect_ratio"},{"aspectMean","aspect_mean"},{"aspectMin","aspect_min"},{"aspectMax","aspect_max"},{"aspectDiff","aspect_difference"},
        {"circularityRatio","circularity_ratio"},{"circularityMean","circularity_mean"},{"circularityMin","circularity_min"},{"circularityMax","circularity_max"},{"circularityDiff","circularity_difference"},
        {"solidityRatio","solidity_ratio"},{"solidityMean","solidity_mean"},{"solidityMin","solidity_min"},{"solidityMax","solidity_max"},{"solidityDiff","solidity_difference"},
        {"vertexCountRatio","vertex_count_ratio"},{"vertexCountMean","vertex_count_mean"},{"vertexCountMin","vertex_count_min"},{"vertexCountMax","vertex_count_max"},{"vertexCountDiff","vertex_count_difference"},
        {"centroidDistance","centroid_distance"},{"centroidDistanceNormalized","centroid_distance_normalized"},
        {"unionAspectRatio","union_aspect_ratio"},{"unionCircularity","union_circularity"},{"unionConvexDeficiency","union_convex_deficiency"},
        {"normalizedSharedEdgeLength","normalized_shared_edge_length"},{"sharedEdgeLength","shared_edge_length"},
        {"sharedEdgeUnsharedVerticesDistance","shared_edge_unshared_vertices_distance"},{"sharedEdgeUnsharedVerticesDistanceNormalized","shared_edge_unshared_vertices_distance_normalized"},
        {"centroidSharedEdgeDistance","centroid_shared_edge_distance"},{"centroidSharedEdgeDistanceNormalized","centroid_shared_edge_distance_normalized"},
        {"sharedEdgeUnionCentroidDistance","shared_edge_union_centroid_distance"},{"sharedEdgeUnionCentroidDistanceNormalized","shared_edge_union_centroid_distance_normalized"},
        {"sharedEdgeUnionAxisAngleDegrees","shared_edge_union_axis_angle"},{"junctionAngleAverageDegrees","junction_angle_mean"},
        {"junctionAngleMaxDegrees","junction_angle_max"},{"junctionAngleMinDegrees","junction_angle_min"},
        {"junctionAngleDifferenceDegrees","junction_angle_difference"},{"junctionAngleRatio","junction_angle_ratio"}
    };
}

QFont fontAt(double points)
{
    QFontDatabase db; QString family = QStringLiteral("DejaVu Sans");
    if (db.families().contains(QStringLiteral("Arial"))) family = QStringLiteral("Arial");
    QFont f(family); f.setPointSizeF(points); f.setWeight(QFont::Normal); return f;
}

QImage plot(const QVector<Result> &rows, const QString &title, bool allPlot)
{
    const int n = rows.size();
    const int rawW = 3600, rawH = qRound(std::max(6.0, .38 * n + 2.2) * 300.0);
    QImage image(rawW, rawH, QImage::Format_RGB32); image.fill(Qt::white);
    image.setDotsPerMeterX(11811); image.setDotsPerMeterY(11811);
    QPainter p(&image); p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    const QRectF axes(.40 * rawW, .12 * rawH, .54 * rawW, .84 * rawH);
    const QColor text("#222222"), grid(217,217,217,204), highlight("#2E6F9E"), main("#6EA6CD");
    const std::array<int,5> ticks{{0,25,50,75,100}};
    auto xPos = [&](double x){ return axes.left() + x / 105.0 * axes.width(); };
    const double lower = -.368 - .05*n, upper = 1.05*n - .632;
    auto yPos = [&](double y){ return axes.top() + (y-lower)/(upper-lower)*axes.height(); };
    p.setPen(QPen(grid, 1)); for (int t : ticks) p.drawLine(QPointF(xPos(t),axes.top()),QPointF(xPos(t),axes.bottom()));
    for (int i=0;i<n;++i) {
        const double top=yPos(i-.38), bottom=yPos(i+.38); const QRectF bar(xPos(0),top,xPos(rows[i].overlap*100)-xPos(0),bottom-top);
        p.fillRect(bar, (!allPlot || i<10) ? highlight : main);
    }
    p.setPen(QPen(QColor("#333333"),1)); p.drawLine(axes.topLeft(),axes.topRight());
    p.setPen(text); p.setFont(fontAt(13));
    for (int t:ticks) { const double x=xPos(t); p.drawLine(QPointF(x,axes.top()),QPointF(x,axes.top()-10)); p.drawText(QRectF(x-110,axes.top()-75,220,55),Qt::AlignHCenter|Qt::AlignBottom,QString::number(t)+"%"); }
    p.setFont(fontAt(15)); p.drawText(QRectF(axes.left(),axes.top()-145,axes.width(),60),Qt::AlignHCenter|Qt::AlignVCenter,QStringLiteral("Overlap index (%)"));
    p.setFont(fontAt(16)); p.drawText(QRectF(0,axes.top()-250,rawW,70),Qt::AlignCenter,title);
    p.setFont(fontAt(13));
    for(int i=0;i<n;++i) {
        const double y=yPos(i); p.drawText(QRectF(0,y-35,axes.left()-30,70),Qt::AlignRight|Qt::AlignVCenter,rows[i].label);
        p.setFont(fontAt(12)); p.drawText(QRectF(xPos(rows[i].overlap*100+1.1),y-32,300,64),Qt::AlignLeft|Qt::AlignVCenter,QString::number(rows[i].overlap*100,'f',1)+"%"); p.setFont(fontAt(13));
    }
    p.end();
    int left=rawW,top=rawH,right=-1,bottom=-1;
    for(int y=0;y<rawH;++y) { const QRgb *line=reinterpret_cast<const QRgb*>(image.constScanLine(y)); for(int x=0;x<rawW;++x) if(line[x] != qRgb(255,255,255)){left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);} }
    if(right<left) return image;
    const QRect crop(std::max(0,left-30),std::max(0,top-30),std::min(rawW-1,right+30)-std::max(0,left-30)+1,std::min(rawH-1,bottom+30)-std::max(0,top-30)+1);
    QImage result=image.copy(crop); result.setDotsPerMeterX(11811); result.setDotsPerMeterY(11811); return result;
}
}

OverlapIndexWorker::OverlapIndexWorker(const OverlapIndexOptions &o,QObject *parent):QObject(parent),m_options(o){}
void OverlapIndexWorker::cancel(){m_cancelled=true;}

void OverlapIndexWorker::run()
{
    auto fail=[&](const QString &s){emit finished(false,s);};
    emit progress(0,100,QStringLiteral("Loading CSV…"));
    QFile file(m_options.csvPath); if(!file.open(QIODevice::ReadOnly)){fail(QStringLiteral("The CSV file cannot be read."));return;}
    QByteArray bytes=file.readAll(); if(bytes.isEmpty()){fail(QStringLiteral("The CSV file is empty."));return;}
    if(bytes.startsWith("\xEF\xBB\xBF")) bytes.remove(0,3);
    const QVector<QStringList> table=parseCsv(QString::fromUtf8(bytes));
    if(table.isEmpty() || table[0].isEmpty()){fail(QStringLiteral("The CSV file is empty."));return;}
    const QStringList headers=table[0]; const int observed=columnIndex(headers,"observed_division"), exception=columnIndex(headers,"exception_label");
    if(observed<0){fail(QStringLiteral("Missing required column: observed_division"));return;}
    if(exception<0){fail(QStringLiteral("Missing required column: exception_label"));return;}
    const int inputRows=int(table.size())-1; QVector<QStringList> rows; int excluded=0,daughters=0,nonDaughters=0;
    for(int r=1;r<table.size();++r) {
        QStringList row=table[r]; while(row.size()<headers.size())row<<QString();
        double ex=0; const QString e=row.value(exception).trimmed(); if((number(e,&ex)&&ex==1.0)||e.toCaseFolded()==QStringLiteral("1")){++excluded;continue;}
        rows<<row; double label=0;if(number(row.value(observed),&label)){if(label==1.0)++daughters;else ++nonDaughters;}
    }
    if(daughters==0 || nonDaughters==0){fail(QStringLiteral("Insufficient labeled values: daughter and non-daughter labels are both required."));return;}
    const QSet<QString> excludedColumns={"filename","pairindex","pair_index","firstpolygonid","first_polygon_id","cell_1_id","secondpolygonid","second_polygon_id","cell_2_id","observed_division","division_timing","estimated_division","exception_label"};
    QVector<Result> results; const auto names=featureNames();
    emit progress(10,100,QStringLiteral("Calculating overlap indices…"));
    for(int c=0;c<headers.size();++c) {
        if(m_cancelled){fail(QStringLiteral("Overlap-index analysis cancelled."));return;}
        if(excludedColumns.contains(headers[c].toCaseFolded()))continue;
        QVector<double> d,nd; bool numeric=false;
        for(const auto &row:rows){double label=0,v=0;if(!number(row.value(c),&v))continue;numeric=true;if(!number(row.value(observed),&label))continue;if(label==1.0)d<<v;else nd<<v;}
        if(!numeric)continue;
        double overlap=std::numeric_limits<double>::quiet_NaN();
        if(d.size()>=2&&nd.size()>=2){double lo=std::numeric_limits<double>::infinity(),hi=-lo;for(double v:d){lo=std::min(lo,v);hi=std::max(hi,v);}for(double v:nd){lo=std::min(lo,v);hi=std::max(hi,v);}
            if(std::abs(lo-hi)<=1e-8+1e-5*std::abs(hi))overlap=1.;else{std::array<int,Bins> a{},b{};std::array<double,Bins+1> edges{};for(int i=0;i<=Bins;++i)edges[i]=lo+(hi-lo)*(double(i)/Bins);edges.front()=lo;edges.back()=hi;auto bin=[&](double v){if(v==hi)return Bins-1;const auto it=std::upper_bound(edges.begin(),edges.end(),v);return std::clamp(int(it-edges.begin())-1,0,Bins-1);};for(double v:d)++a[bin(v)];for(double v:nd)++b[bin(v)];overlap=0;for(int i=0;i<Bins;++i)overlap+=std::min(double(a[i])/d.size(),double(b[i])/nd.size());}}
        results<<Result{headers[c],names.value(headers[c],headers[c]),overlap,int(d.size()),int(nd.size()),c};
        emit progress(10+60*c/std::max(1,int(headers.size())),100,QStringLiteral("Calculating %1").arg(headers[c]));
    }
    if(results.isEmpty()){fail(QStringLiteral("No numeric geometry features were found."));return;}
    std::stable_sort(results.begin(),results.end(),[](const Result&a,const Result&b){if(std::isnan(a.overlap)!=std::isnan(b.overlap))return !std::isnan(a.overlap);if(std::isnan(a.overlap))return false;return a.overlap<b.overlap;});
    QVector<Result> finite;for(const auto&r:results)if(std::isfinite(r.overlap))finite<<r;
    if(finite.isEmpty()){fail(QStringLiteral("Insufficient labeled values to calculate an overlap index."));return;}
    emit progress(75,100,QStringLiteral("Drawing plots…"));
    QVector<Result> top=finite.mid(0,std::min(10,int(finite.size())));
    const QString topTitle=QStringLiteral("Top %1 features by lowest overlap index").arg(top.size());
    const QImage topImage=plot(top,topTitle,false), allImage=plot(finite,QStringLiteral("All features by overlap index"),true);
    if(m_cancelled){fail(QStringLiteral("Overlap-index analysis cancelled."));return;}
    QDir out(m_options.outputDirectory);if(!out.exists()&&!QDir().mkpath(out.absolutePath())){fail(QStringLiteral("The output directory cannot be created."));return;}
    QFileInfo check(out.absoluteFilePath(".overlap_write_check"));QFile checkFile(check.filePath());if(!checkFile.open(QIODevice::WriteOnly)){fail(QStringLiteral("The output directory is not writable."));return;}checkFile.close();checkFile.remove();
    const QString csvPath=out.absoluteFilePath("overlap_indices.csv"),topPath=out.absoluteFilePath("top10_overlap_index.png"),allPath=out.absoluteFilePath("all_features_overlap_index.png");
    QSaveFile csv(csvPath);if(!csv.open(QIODevice::WriteOnly|QIODevice::Text)){fail(QStringLiteral("Failed to save overlap_indices.csv."));return;}QTextStream stream(&csv);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
    stream<<"feature,plot_label,overlap_index,overlap_percent,n_daughter,n_non_daughter,n_bins\n";
    for(const auto&r:results){stream<<quoteCsv(r.feature)<<','<<quoteCsv(r.label)<<',';if(std::isfinite(r.overlap))stream<<QString::number(r.overlap,'g',10);stream<<',';if(std::isfinite(r.overlap))stream<<QString::number(r.overlap*100,'g',10);stream<<','<<r.daughter<<','<<r.nonDaughter<<','<<Bins<<'\n';}
    stream.flush();if(stream.status()!=QTextStream::Ok||!csv.commit()){fail(QStringLiteral("Failed to save overlap_indices.csv."));return;}
    if(!topImage.save(topPath,"PNG")){QFile::remove(csvPath);fail(QStringLiteral("Failed to save top10_overlap_index.png."));return;}
    if(!allImage.save(allPath,"PNG")){QFile::remove(csvPath);QFile::remove(topPath);fail(QStringLiteral("Failed to save all_features_overlap_index.png."));return;}
    emit progress(100,100,QStringLiteral("Complete"));
    emit finished(true,QStringLiteral("Overlap-index analysis complete.\n\nInput rows: %1\nExcluded exception rows: %2\nAnalyzed rows: %3 (%4 daughter, %5 non-daughter)\nFeatures analyzed: %6\n\nOutputs:\n%7\n%8\n%9").arg(inputRows).arg(excluded).arg(rows.size()).arg(daughters).arg(nonDaughters).arg(results.size()).arg(csvPath,topPath,allPath));
}

bool OverlapIndexAnalysis::getOptions(QWidget *parent, OverlapIndexOptions *options)
{
    QDialog dialog(parent);dialog.setWindowTitle(QStringLiteral("Overlap Index for all features"));
    auto *layout=new QVBoxLayout(&dialog);auto *form=new QFormLayout;auto *csv=new QLineEdit;auto *out=new QLineEdit;
    auto row=[&](QLineEdit *edit,const QString &caption,auto browse){auto *w=new QWidget;auto *h=new QHBoxLayout(w);h->setContentsMargins(0,0,0,0);h->addWidget(edit);auto *button=new QPushButton(QStringLiteral("Browse…"));h->addWidget(button);QObject::connect(button,&QPushButton::clicked,&dialog,browse);form->addRow(caption,w);};
    row(csv,QStringLiteral("Neighbor-pair geometry CSV"),[&]{const QString p=QFileDialog::getOpenFileName(&dialog,QStringLiteral("Neighbor-pair geometry CSV"),csv->text(),QStringLiteral("CSV files (*.csv)"));if(!p.isEmpty())csv->setText(p);});
    row(out,QStringLiteral("Output directory"),[&]{const QString p=QFileDialog::getExistingDirectory(&dialog,QStringLiteral("Output directory"),out->text());if(!p.isEmpty())out->setText(p);});layout->addLayout(form);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        if(!QFileInfo(csv->text()).isFile()){QMessageBox::warning(&dialog,QStringLiteral("Overlap Index for all features"),QStringLiteral("Select a readable neighbor-pair geometry CSV."));return;}
        if(!QFileInfo(out->text()).isDir()){QMessageBox::warning(&dialog,QStringLiteral("Overlap Index for all features"),QStringLiteral("Select a valid output directory."));return;}
        dialog.accept();
    });QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return false;options->csvPath=QFileInfo(csv->text()).absoluteFilePath();options->outputDirectory=QDir(out->text()).absolutePath();return true;
}
