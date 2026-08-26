#include "singlefeaturemixtureanalysis.h"

#include <QBoxLayout>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTextStream>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>

namespace {
constexpr double Pi = 3.1415926535897932384626433832795;

QStringList csvFields(const QString &line)
{
    QStringList out; QString field; bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (quoted && c == '"' && i + 1 < line.size() && line[i + 1] == '"') { field += '"'; ++i; }
        else if (c == '"') quoted = !quoted;
        else if (c == ',' && !quoted) { out << field; field.clear(); }
        else field += c;
    }
    out << field;
    return out;
}

QString csvQuote(QString value)
{
    if (value.contains('"')) value.replace("\"", "\"\"");
    return value.contains(',') || value.contains('"') || value.contains('\n')
            ? QStringLiteral("\"") + value + QStringLiteral("\"") : value;
}

double logI0(double x)
{
    x = std::abs(x);
    if (x < 50.0) return std::log(std::cyl_bessel_i(0.0, x));
    const double inv = 1.0 / x;
    return x - 0.5 * std::log(2.0 * Pi * x)
            + std::log(1.0 + inv / 8.0 + 9.0 * inv * inv / 128.0
                       + 225.0 * inv * inv * inv / 3072.0);
}

double a1(double k)
{
    if (k < 50.0) return std::cyl_bessel_i(1.0, k) / std::cyl_bessel_i(0.0, k);
    const double inv = 1.0 / k;
    return 1.0 - inv / 2.0 - inv * inv / 8.0 - inv * inv * inv / 8.0;
}

double kappaFromR(double r)
{
    r = std::clamp(r, 0.0, 1.0 - 1e-12);
    if (r < 1e-12) return 0.0;
    double k = r < .53 ? 2*r + r*r*r + 5*std::pow(r,5)/6
                       : (r < .85 ? -.4 + 1.39*r + .43/(1-r)
                                  : 1/(r*r*r - 4*r*r + 3*r));
    for (int i=0; i<30; ++i) {
        const double A=a1(k), derivative=1-A*A-A/k;
        const double next=std::max(1e-10, k-(A-r)/derivative);
        if (std::abs(next-k)<1e-12*(1+k)) break;
        k=next;
    }
    return std::min(k, 1e8);
}

struct Model { std::array<double,2> weight{{.5,.5}}, mu{{0,Pi}}, kappa{{1,1}}; double ll=-INFINITY; };

std::array<double,2> responsibility(double theta, const Model &m)
{
    std::array<double,2> z;
    for (int j=0;j<2;++j) z[j]=std::log(std::max(m.weight[j],1e-300))
            +m.kappa[j]*std::cos(theta-m.mu[j])-std::log(2*Pi)-logI0(m.kappa[j]);
    const double mx=std::max(z[0],z[1]);
    const double den=std::exp(z[0]-mx)+std::exp(z[1]-mx);
    return {{std::exp(z[0]-mx)/den,std::exp(z[1]-mx)/den}};
}

void sortModel(Model &m)
{
    auto angle=[](double mu){ double a=mu*90/Pi; if(a<0)a+=180; return a; };
    if (angle(m.mu[0]) > angle(m.mu[1])) { std::swap(m.weight[0],m.weight[1]); std::swap(m.mu[0],m.mu[1]); std::swap(m.kappa[0],m.kappa[1]); }
}

Model fit(const QVector<double> &angles, const Model &initial, int maxIter)
{
    Model m=initial;
    QVector<std::array<double,2>> resp(angles.size());
    double previous=-INFINITY;
    for(int it=0;it<maxIter;++it) {
        double ll=0;
        for(int i=0;i<angles.size();++i) {
            const double t=2*angles[i]*Pi/180;
            resp[i]=responsibility(t,m);
            std::array<double,2> z;
            for(int j=0;j<2;++j) z[j]=std::log(std::max(m.weight[j],1e-300))+m.kappa[j]*std::cos(t-m.mu[j])-std::log(2*Pi)-logI0(m.kappa[j]);
            const double mx=std::max(z[0],z[1]); ll += mx+std::log(std::exp(z[0]-mx)+std::exp(z[1]-mx));
        }
        m.ll=ll;
        if (std::isfinite(previous) && std::abs(ll-previous)<1e-6) break;
        previous=ll;
        for(int j=0;j<2;++j) {
            double n=0,c=0,s=0;
            for(int i=0;i<angles.size();++i) { const double w=resp[i][j],t=2*angles[i]*Pi/180; n+=w;c+=w*std::cos(t);s+=w*std::sin(t); }
            m.weight[j]=n/angles.size(); m.mu[j]=std::atan2(s,c); m.kappa[j]=kappaFromR(std::hypot(c,s)/n);
        }
    }
    sortModel(m); return m;
}

Model fullFit(const QVector<double> &angles, quint32 seed)
{
    std::mt19937 rng(seed); std::uniform_real_distribution<double> phase(0,Pi);
    Model best;
    for(int i=0;i<15;++i) {
        Model init; init.mu={{phase(rng),phase(rng)}}; init.kappa={{1.0,1.0}};
        Model candidate=fit(angles,init,400);
        if(candidate.ll>best.ll) best=candidate;
    }
    return best;
}

double angleMean(double mu) { double a=mu*90/Pi; return a<0?a+180:a; }

double wasserstein(QVector<QPair<double,double>> a, QVector<QPair<double,double>> b)
{
    auto less=[](const auto &x,const auto &y){return x.first<y.first;};
    std::stable_sort(a.begin(),a.end(),less); std::stable_sort(b.begin(),b.end(),less);
    double sa=0,sb=0; for(auto x:a)sa+=x.second; for(auto x:b)sb+=x.second;
    int i=0,j=0; double ca=0,cb=0,last=std::min(a[0].first,b[0].first),distance=0;
    while(i<a.size()||j<b.size()) {
        const double x=std::min(i<a.size()?a[i].first:INFINITY,j<b.size()?b[j].first:INFINITY);
        distance += std::abs(ca/sa-cb/sb)*(x-last);
        while(i<a.size()&&a[i].first==x)ca+=a[i++].second;
        while(j<b.size()&&b[j].first==x)cb+=b[j++].second;
        last=x;
    }
    return distance;
}

QVector<double> ecdf(const QVector<QPair<double,double>> &values)
{
    QVector<QPair<double,double>> sorted=values;
    std::stable_sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a.first<b.first;});
    double total=0;for(auto v:sorted)total+=v.second;
    QVector<double> result(1801); int pos=0;double sum=0;
    for(int g=0;g<=1800;++g){const double x=g/10.0;while(pos<sorted.size()&&sorted[pos].first<=x){sum+=sorted[pos].second;++pos;}result[g]=sum/total;}
    return result;
}

QFont plotFont(double pt)
{
    QFontDatabase db; QString family=QStringLiteral("DejaVu Sans");
    for(const QString &f:{QStringLiteral("Arial"),QStringLiteral("Helvetica"),QStringLiteral("DejaVu Sans")}) if(db.families().contains(f)){family=f;break;}
    QFont font(family);font.setPointSizeF(pt);font.setWeight(QFont::Normal);return font;
}

void drawPlot(const QString &path,const QVector<double> &angles,const QVector<int> &labels,
              const QVector<std::array<double,2>> &r,const std::array<double,2> &point)
{
    QImage image(5520,2280,QImage::Format_RGBA8888);image.fill(Qt::white);image.setDotsPerMeterX(23622);image.setDotsPerMeterY(23622);
    QPainter p(&image);p.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);
    const QRectF rects[2]={{348.1667,182,2429.5417,1796.3333},{2955.0833,182,2429.5417,1796.3333}};
    const QColor colors[2]={QColor("#4C79A2"),QColor("#DA6752")};
    const QString titles[2]={QStringLiteral("Non-daughter vs low component"),QStringLiteral("Daughter vs high component")};
    auto map=[&](const QRectF &r,double x,double y){return QPointF(r.left()+(x-60)/120*r.width(),r.top()+(1.02-y)/1.02*r.height());};
    for(int panel=0;panel<2;++panel){const QRectF ar=rects[panel];p.save();p.setClipRect(ar);
        p.setPen(QPen(QColor("#E8E8E8"),5));for(int k=0;k<=5;++k)p.drawLine(map(ar,60,k*.2),map(ar,180,k*.2));
        QVector<QPair<double,double>> observed,component;for(int i=0;i<angles.size();++i){if(labels[i]==panel)observed.append({angles[i],1});component.append({angles[i],std::max(0.0,r[i][panel])});}
        const QVector<double> eo=ecdf(observed),ec=ecdf(component);QPainterPath po,pc;po.moveTo(map(ar,60,eo[600]));pc.moveTo(map(ar,60,ec[600]));for(int g=601;g<=1800;++g){po.lineTo(map(ar,g/10.,eo[g]));pc.lineTo(map(ar,g/10.,ec[g]));}
        QPen op(QColor("#222222"),16.6667,Qt::SolidLine,Qt::FlatCap);p.setPen(op);p.drawPath(po);
        QPen cp(colors[panel],16.6667,Qt::CustomDashLine,Qt::FlatCap);cp.setDashPattern({4,2});p.setPen(cp);p.drawPath(pc);p.restore();
        p.setPen(QPen(Qt::black,6.6667));p.drawLine(ar.bottomLeft(),ar.bottomRight());p.drawLine(ar.topLeft(),ar.bottomLeft());p.setFont(plotFont(8));
        for(int x=60;x<=180;x+=20){QPointF q=map(ar,x,0);p.drawLine(q,q+QPointF(0,29.1667));p.drawText(QRectF(q.x()-80,q.y()+35,160,65),Qt::AlignHCenter|Qt::AlignTop,QString::number(x));}
        for(int k=0;k<=5;++k){QPointF q=map(ar,60,k*.2);p.drawLine(q,q-QPointF(29.1667,0));if(panel==0)p.drawText(QRectF(q.x()-190,q.y()-35,145,70),Qt::AlignRight|Qt::AlignVCenter,QString::number(k*.2,'f',1));}
        p.setFont(plotFont(9));p.drawText(QRectF(ar.left(),2130,ar.width(),90),Qt::AlignHCenter|Qt::AlignTop,QStringLiteral("Mean junction angle (°)"));p.drawText(QRectF(ar.left(),72,ar.width(),78),Qt::AlignCenter,titles[panel]);
        if(panel==0){p.save();p.translate(111,1080.17);p.rotate(-90);p.drawText(QRectF(-400,-50,800,100),Qt::AlignCenter,QStringLiteral("Cumulative proportion"));p.restore();}
        p.setFont(plotFont(7.5));const QString annotation=QStringLiteral("W₁ point estimate = %1°").arg(point[panel],0,'f',2);
        const QPointF textOrigin(ar.left()+72.88,ar.top()+53.89);
        QRectF box(textOrigin.x()-29,textOrigin.y()-20,955,250);p.setPen(QPen(QColor("#D0D0D0"),5));p.setBrush(Qt::white);p.drawRoundedRect(box,25,25);p.setPen(Qt::black);p.drawText(QRectF(textOrigin,QSizeF(897,199.2)),Qt::AlignLeft|Qt::AlignTop,annotation);
        const QRectF legend=panel==0?QRectF(1573.96,1739.58,1166.25,201.25):QRectF(4180.87,1739.58,1166.25,201.25);const double y1=legend.top()+48,y2=legend.top()+148,x1=legend.left()+20,x2=x1+106;
        p.setPen(op);p.drawLine(QPointF(x1,y1),QPointF(x2,y1));p.setPen(Qt::black);p.drawText(QRectF(x2+35,y1-45,900,90),Qt::AlignVCenter,QStringLiteral("Observed label"));p.setPen(cp);p.drawLine(QPointF(x1,y2),QPointF(x2,y2));p.setPen(Qt::black);p.drawText(QRectF(x2+35,y2-45,900,90),Qt::AlignVCenter,QStringLiteral("Posterior-weighted component"));
    }p.end();image.save(path,"PNG");
}

double componentDensity(double angle, const Model &model, int component)
{
    const double theta=2*angle*Pi/180;
    return model.weight[component]*std::exp(model.kappa[component]*std::cos(theta-model.mu[component])
           -std::log(2*Pi)-logI0(model.kappa[component]));
}

double modelCutoff(const Model &model)
{
    const double low=angleMean(model.mu[0]), high=angleMean(model.mu[1]);
    double best=low, difference=INFINITY;
    for(int i=0;i<=10000;++i){const double x=low+(high-low)*i/10000.;const double d=std::abs(componentDensity(x,model,0)-componentDensity(x,model,1));if(d<difference){difference=d;best=x;}}
    return best;
}

void drawPooledPlot(const QString &path,const QVector<double> &angles,const Model &model,double cutoff)
{
    QImage image(2400,1600,QImage::Format_RGBA8888);image.fill(Qt::white);QPainter p(&image);p.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);
    const QRectF area(220,150,2000,1200);QVector<int> bins(36);for(double a:angles)++bins[std::min(35,int(a/5))];const int maximum=*std::max_element(bins.begin(),bins.end());
    auto map=[&](double x,double y){return QPointF(area.left()+x/180*area.width(),area.bottom()-y*area.height());};
    p.setPen(Qt::NoPen);p.setBrush(QColor("#C8CDD3"));for(int i=0;i<bins.size();++i){const QPointF a=map(i*5,double(bins[i])/maximum),b=map((i+1)*5,0);p.drawRect(QRectF(a,b).normalized());}
    double peak=0;for(int i=0;i<=1800;++i)peak=std::max(peak,componentDensity(i/10.,model,0)+componentDensity(i/10.,model,1));
    const QColor colors[2]={QColor("#4C79A2"),QColor("#DA6752")};for(int j=0;j<2;++j){QPainterPath curve;for(int i=0;i<=1800;++i){const double x=i/10.,y=componentDensity(x,model,j)/peak; if(i==0)curve.moveTo(map(x,y));else curve.lineTo(map(x,y));}p.setPen(QPen(colors[j],8));p.drawPath(curve);}
    p.setPen(QPen(Qt::black,5));p.drawLine(area.bottomLeft(),area.bottomRight());p.drawLine(area.topLeft(),area.bottomLeft());p.setPen(QPen(QColor("#333333"),6,Qt::DashLine));p.drawLine(map(cutoff,0),map(cutoff,1));p.setFont(plotFont(18));p.setPen(Qt::black);p.drawText(QRectF(0,30,image.width(),90),Qt::AlignCenter,QStringLiteral("Pooled angular distribution and fitted mixture"));p.drawText(QRectF(area.left(),1380,area.width(),80),Qt::AlignCenter,QStringLiteral("Angle (degrees); model cutoff = %1°").arg(cutoff,0,'f',2));p.end();image.save(path,"PNG");
}
}

SingleFeatureMixtureWorker::SingleFeatureMixtureWorker(const SingleFeatureMixtureOptions &o,QObject *p):QObject(p),m_options(o){}
void SingleFeatureMixtureWorker::cancel(){m_cancelled=true;}

void SingleFeatureMixtureWorker::run()
{
    QFile file(m_options.csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { emit finished(false, "Cannot open input CSV."); return; }
    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif
    const QStringList headers=csvFields(in.readLine());QHash<QString,int> columns;for(int i=0;i<headers.size();++i)columns.insert(headers[i].trimmed().toLower(),i);
    auto index=[&](const QString &n){return columns.value(n.trimmed().toLower(),-1);};
    const int fi=index(m_options.feature),oi=index(m_options.observedColumn),ei=index(m_options.exceptionColumn);
    if(fi<0||ei<0){emit finished(false,QStringLiteral("Missing feature or exception-label CSV column. Column matching is case-insensitive."));return;}
    QVector<QStringList> pooledRaw,labeledRaw;QVector<double> pooledAngles,labeledAngles;QVector<int> labels;int inputRows=0,exceptions=0;
    while(!in.atEnd()){
        QString line=in.readLine();if(line.isEmpty())continue;++inputRows;QStringList f=csvFields(line);if(f.size()<headers.size())f.resize(headers.size());
        bool ok=false;const double ex=f[ei].trimmed().toDouble(&ok);if(ok&&ex!=0){++exceptions;continue;}
        const double angle=f[fi].trimmed().toDouble(&ok);if(!ok||!std::isfinite(angle))continue;
        pooledRaw<<f;pooledAngles<<angle;
        if(oi>=0){const double label=f[oi].trimmed().toDouble(&ok);if(ok&&(label==0||label==1)){labeledRaw<<f;labeledAngles<<angle;labels<<int(label);}}
    }
    const bool labeled=!labels.isEmpty();QVector<double> angles=labeled?labeledAngles:pooledAngles;QVector<QStringList> raw=labeled?labeledRaw:pooledRaw;
    if(angles.isEmpty()){emit finished(false,"No finite, non-exception observations are available.");return;}
    for(double a:angles)if(a<0||a>180){emit finished(false,"The axial von Mises feature must be angular degrees in [0,180].");return;}
    emit progress(0,1,QStringLiteral("Fitting %1 analyzed rows").arg(angles.size()));if(m_cancelled){emit finished(false,"Mixture analysis cancelled.");return;}
    const Model model=fullFit(angles,m_options.seed);const double cutoff=modelCutoff(model);QVector<std::array<double,2>> post(angles.size());for(int i=0;i<angles.size();++i)post[i]=responsibility(2*angles[i]*Pi/180,model);
    QDir().mkpath(m_options.outputDirectory);auto write=[&](QString name,const QStringList &lines){QFile f(QDir(m_options.outputDirectory).filePath(name));if(!f.open(QIODevice::WriteOnly|QIODevice::Text))return false;f.write("\xEF\xBB\xBF");f.write(lines.join('\n').toUtf8());f.write("\n");return true;};
    QStringList posterior;QStringList ph=headers;ph<<"posterior_component_0"<<"posterior_component_1";posterior<<ph.join(',');for(int i=0;i<raw.size();++i){QStringList q;for(QString value:raw[i])q<<csvQuote(value);q<<QString::number(post[i][0],'g',17)<<QString::number(post[i][1],'g',17);posterior<<q.join(',');}
    QStringList params;params<<"component,weight,mean_angle_degrees,kappa,log_likelihood,model_cutoff_degrees";for(int j=0;j<2;++j)params<<QStringLiteral("%1,%2,%3,%4,%5,%6").arg(j).arg(model.weight[j],0,'g',17).arg(angleMean(model.mu[j]),0,'g',17).arg(model.kappa[j],0,'g',17).arg(model.ll,0,'g',17).arg(cutoff,0,'g',17);
    QStringList summary;summary<<"analysis_mode,input_rows,excluded_exception_rows,analyzed_rows,model_cutoff_degrees"<<QStringLiteral("%1,%2,%3,%4,%5").arg(labeled?"labeled":"unlabeled pooled-distribution").arg(inputRows).arg(exceptions).arg(angles.size()).arg(cutoff,0,'g',17);
    if(!write("vonmises2_posteriors_realdata.csv",posterior)||!write("vonmises2_parameters_realdata.csv",params)||!write("mixture_analysis_summary.csv",summary)){emit finished(false,"Could not write one or more output CSV files.");return;}
    drawPooledPlot(QDir(m_options.outputDirectory).filePath("pooled_distribution_mixture.png"),angles,model,cutoff);
    QDir(m_options.outputDirectory).remove("component_label_distribution_closeness_bootstrap.csv");
    // Do not leave label-dependent artifacts in an unlabeled result directory.
    if(!labeled){
        QDir output(m_options.outputDirectory);
        output.remove("component_label_distribution_closeness_summary.csv");
        output.remove("component_label_distribution_closeness_ecdf.png");
    }
    if(labeled){
        std::array<double,2> point{{NAN,NAN}};for(int j=0;j<2;++j){QVector<QPair<double,double>> observed,component;for(int i=0;i<angles.size();++i){if(labels[i]==j)observed<<qMakePair(angles[i],1.);component<<qMakePair(angles[i],post[i][j]);}if(!observed.isEmpty())point[j]=wasserstein(observed,component);}
        QStringList closeness;closeness<<"comparison,point_estimate_degrees";for(int j=0;j<2;++j)if(std::isfinite(point[j]))closeness<<QStringLiteral("observed_%1_vs_component_%1,%2").arg(j).arg(point[j],0,'g',17);
        if(!write("component_label_distribution_closeness_summary.csv",closeness)){emit finished(false,"Could not write label-comparison summary CSV.");return;}
        if(labels.contains(0)&&labels.contains(1))drawPlot(QDir(m_options.outputDirectory).filePath("component_label_distribution_closeness_ecdf.png"),angles,labels,post,point);
    }
    emit progress(1,1,"Complete");emit finished(true,QStringLiteral("Mixture analysis complete in %1 mode. %2 input rows; %3 rows with nonzero exception labels excluded; %4 analyzed rows.").arg(labeled?"labeled":"unlabeled pooled-distribution").arg(inputRows).arg(exceptions).arg(angles.size()));
}

bool SingleFeatureMixtureAnalysis::getOptions(QWidget *parent,SingleFeatureMixtureOptions *o)
{
    QDialog d(parent);d.setWindowTitle("Mixture modeling for single geometry feature");auto *layout=new QVBoxLayout(&d);auto *form=new QFormLayout;layout->addLayout(form);
    auto pathRow = [&](const QString &title, QLineEdit **edit, bool directory) {
        auto *w = new QWidget;
        auto *h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        *edit = new QLineEdit;
        QLineEdit *const targetEdit = *edit;
        auto *b = new QPushButton("Browse…");
        h->addWidget(targetEdit);
        h->addWidget(b);
        form->addRow(title, w);
        QObject::connect(b, &QPushButton::clicked, &d,
                         [&d, directory, targetEdit] {
            const QString path = directory
                    ? QFileDialog::getExistingDirectory(&d,
                                                        "Select output directory")
                    : QFileDialog::getOpenFileName(&d,
                                                   "Select batch neighbor-pair geometry CSV",
                                                   QString(),
                                                   "CSV files (*.csv)");
            if (!path.isEmpty())
                targetEdit->setText(path);
        });
    };
    QLineEdit *csv,*out;pathRow("Batch geometry CSV",&csv,false);pathRow("Output directory",&out,true);auto *feature=new QComboBox;feature->setEditable(true);feature->addItems({"junctionAngleAverageDegrees","junctionAngleMaxDegrees","junctionAngleMinDegrees","junctionAngleDifferenceDegrees","sharedEdgeUnionAxisAngleDegrees"});form->addRow("Angular feature",feature);
    auto *observed=new QLineEdit(o->observedColumn),*exception=new QLineEdit(o->exceptionColumn);auto *seed=new QSpinBox;seed->setRange(0,INT_MAX);seed->setValue(int(o->seed));form->addRow("Observed-label column (optional)",observed);form->addRow("Exception column",exception);form->addRow("Seed",seed);auto *note=new QLabel("Only degree-valued angular features in [0,180] are accepted. Rows with nonzero exception labels are excluded. If no valid 0/1 division labels are available, a pooled unsupervised analysis is run.");note->setWordWrap(true);layout->addWidget(note);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);QObject::connect(buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);if(d.exec()!=QDialog::Accepted)return false;
    const QRegularExpression angularDegrees(QStringLiteral("angle.*degrees|degrees.*angle"),
                                             QRegularExpression::CaseInsensitiveOption);
    if(csv->text().isEmpty()||out->text().isEmpty()||!feature->currentText().contains(angularDegrees)){QMessageBox::critical(parent,"Mixture modeling","Choose an input CSV, output directory, and an angular degree feature.");return false;}o->csvPath=csv->text();o->outputDirectory=out->text();o->feature=feature->currentText();o->observedColumn=observed->text();o->exceptionColumn=exception->text();o->seed=quint32(seed->value());return true;
}
