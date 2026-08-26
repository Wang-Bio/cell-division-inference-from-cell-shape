#include "networkdebugger.h"
#include "vertexitem.h"
#include "lineitem.h"
#include "polygonitem.h"

#include <QDialog>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLabel>
#include <QLineF>
#include <QHash>
#include <QSet>
#include <QPainterPath>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
constexpr int OverlayTag = 0x4e444247;
QVector<QPointF> path(LineItem *e) {
    if (e->hasValidPath()) return e->centerlinePath();
    return {e->line().p1(), e->line().p2()};
}
double cross(QPointF a,QPointF b,QPointF c){ return (b.x()-a.x())*(c.y()-a.y())-(b.y()-a.y())*(c.x()-a.x()); }
bool onSegment(QPointF p,QPointF a,QPointF b,double eps=NetworkDebugTolerances::coordinate){
    return std::abs(cross(a,b,p)) <= eps && p.x() >= std::min(a.x(),b.x())-eps && p.x() <= std::max(a.x(),b.x())+eps && p.y() >= std::min(a.y(),b.y())-eps && p.y() <= std::max(a.y(),b.y())+eps;
}
int orientation(QPointF a,QPointF b,QPointF c){ double v=cross(a,b,c); return v>NetworkDebugTolerances::coordinate?1:v<-NetworkDebugTolerances::coordinate?-1:0; }
bool intersects(QPointF a,QPointF b,QPointF c,QPointF d){
    int a1=orientation(a,b,c),a2=orientation(a,b,d),a3=orientation(c,d,a),a4=orientation(c,d,b);
    return (a1*a2<0&&a3*a4<0)||(a1==0&&onSegment(c,a,b))||(a2==0&&onSegment(d,a,b))||(a3==0&&onSegment(a,c,d))||(a4==0&&onSegment(b,c,d));
}
double pointSegment(QPointF p,QPointF a,QPointF b){ QPointF d=b-a; double q=d.x()*d.x()+d.y()*d.y(); if(q==0)return QLineF(p,a).length(); double t=((p-a).x()*d.x()+(p-a).y()*d.y())/q; t=std::max(0.,std::min(1.,t)); return QLineF(p,a+t*d).length(); }
QString objects(const NetworkDebugIssue&i){QStringList s;for(int x:i.vertexIds)s<<"V"+QString::number(x);for(int x:i.lineIds)s<<"E"+QString::number(x);for(int x:i.polygonIds)s<<"P"+QString::number(x);return s.join(", ");}
void add(QVector<NetworkDebugIssue>&r,NetworkDebugIssue::Severity s,QString c,QVector<int>v,QVector<int>e,QVector<int>p,QRectF where,QString d,QString fix){r.push_back({s,c,v,e,p,where,d,fix});}
}

QVector<NetworkDebugIssue> NetworkDebugger::inspect(QGraphicsScene *scene){
    QVector<NetworkDebugIssue> out; if(!scene)return out;
    QVector<VertexItem*> vs; QVector<LineItem*> es; QVector<PolygonItem*> ps;
    for(auto*i:scene->items()){if(auto*v=qgraphicsitem_cast<VertexItem*>(i))vs<<v;else if(auto*e=qgraphicsitem_cast<LineItem*>(i))es<<e;else if(auto*p=qgraphicsitem_cast<PolygonItem*>(i))ps<<p;}
    std::sort(vs.begin(),vs.end(),[](auto*a,auto*b){return a->id()<b->id();}); std::sort(es.begin(),es.end(),[](auto*a,auto*b){return a->id()<b->id();}); std::sort(ps.begin(),ps.end(),[](auto*a,auto*b){return a->id()<b->id();});
    QHash<int,int> degree; for(auto*e:es){degree[e->v1Id()]++;degree[e->v2Id()]++;}
    for(int i=0;i<vs.size();++i){
        int deg=degree.value(vs[i]->id()); if(!deg)add(out,NetworkDebugIssue::Warning,"V004",{vs[i]->id()},{},{},QRectF(vs[i]->pos(),QSizeF()),"Orphan vertex has degree zero.","Delete it or connect it to the intended wall.");
        else if(deg==1||deg>=4) add(out,NetworkDebugIssue::Warning,"V005",{vs[i]->id()},{},{},QRectF(vs[i]->pos(),QSizeF()),"Unexpected vertex degree "+QString::number(deg)+".","Inspect the local topology; reconnect manually if incorrect.");
        for(int j=i+1;j<vs.size();++j){double d=QLineF(vs[i]->pos(),vs[j]->pos()).length();if(d<=NetworkDebugTolerances::coordinate)add(out,NetworkDebugIssue::Error,"V001",{vs[i]->id(),vs[j]->id()},{},{},QRectF(vs[i]->pos(),vs[j]->pos()).normalized(),"Distinct vertices occupy the same coordinate.","Merge or delete the duplicated vertex.");else if(d<=NetworkDebugTolerances::strictDistancePx)add(out,NetworkDebugIssue::Error,"V002",{vs[i]->id(),vs[j]->id()},{},{},QRectF(vs[i]->pos(),vs[j]->pos()).normalized(),"Vertices are no more than one pixel apart.","Inspect the wall; merge/delete only if it is not biological.");}
    }
    for(auto*e:es){QVector<QPointF> q=path(e); QRectF box;for(auto p:q)box=box.united(QRectF(p,QSizeF()));if(e->v1Id()==e->v2Id())add(out,NetworkDebugIssue::Error,"E001",{e->v1Id()},{e->id()},{},box,"Line connects a vertex to itself.","Delete and reconnect the incorrect line.");double len=0;for(int i=1;i<q.size();++i)len+=QLineF(q[i-1],q[i]).length();if(len<=NetworkDebugTolerances::shortLinePx)add(out,NetworkDebugIssue::Warning,"E010",{e->v1Id(),e->v2Id()},{e->id()},{},box,"Line is extremely short.","Verify image support before deleting or reconnecting it.");for(int i=0;i+1<q.size();++i)for(int j=i+2;j+1<q.size();++j)if(!(i==0&&j+1==q.size()-1)&&intersects(q[i],q[i+1],q[j],q[j+1]))add(out,NetworkDebugIssue::Error,"E004",{}, {e->id()},{},box,"Line path intersects itself.","Delete and reconnect the incorrect line.");}
    for(int i=0;i<es.size();++i)for(int j=i+1;j<es.size();++j){auto a=es[i],b=es[j];bool shared=a->v1Id()==b->v1Id()||a->v1Id()==b->v2Id()||a->v2Id()==b->v1Id()||a->v2Id()==b->v2Id();if(a->v1Id()==b->v1Id()&&a->v2Id()==b->v2Id())add(out,NetworkDebugIssue::Error,"E002",{a->v1Id(),a->v2Id()},{a->id(),b->id()},{},a->sceneBoundingRect()|b->sceneBoundingRect(),"Duplicate lines have the same endpoints.","Delete the duplicated line after inspecting both paths.");auto x=path(a),y=path(b);bool hit=false;for(int ai=0;ai+1<x.size()&&!hit;++ai)for(int bi=0;bi+1<y.size();++bi)if(intersects(x[ai],x[ai+1],y[bi],y[bi+1])){bool endpoint=shared&&(QLineF(x[ai],y[bi]).length()<1e-6||QLineF(x[ai],y[bi+1]).length()<1e-6||QLineF(x[ai+1],y[bi]).length()<1e-6||QLineF(x[ai+1],y[bi+1]).length()<1e-6);if(!endpoint)hit=true;}if(hit)add(out,NetworkDebugIssue::Error,"E005",{}, {a->id(),b->id()},{},a->sceneBoundingRect()|b->sceneBoundingRect(),"Line interiors cross without a valid shared vertex.","Inspect both lines and delete/reconnect the incorrect one.");}
    for(auto*v:vs)for(auto*e:es)if(v->id()!=e->v1Id()&&v->id()!=e->v2Id()){auto q=path(e);for(int k=0;k+1<q.size();++k)if(pointSegment(v->pos(),q[k],q[k+1])<=NetworkDebugTolerances::coordinate){add(out,NetworkDebugIssue::Error,"V003",{v->id()},{e->id()},{},QRectF(v->pos(),QSizeF()),"Vertex lies on the interior of an unrelated line.","Split the line at the vertex or delete and reconnect it.");break;}}
    for(auto*p:ps){auto ids=p->vertexIds();QSet<int> unique;for(int id:ids)unique.insert(id);if(ids.size()<3)add(out,NetworkDebugIssue::Error,"P001",{}, {},{p->id()},p->sceneBoundingRect(),"Polygon boundary is not a closed cycle.","Rebuild the polygon from a closed sequence of graph lines.");if(unique.size()<3||p->area()<=NetworkDebugTolerances::minimumAreaPx2)add(out,NetworkDebugIssue::Error,"P002",{}, {},{p->id()},p->sceneBoundingRect(),"Polygon is geometrically degenerate.","Delete it and reconnect its boundary before rebuilding polygons.");if(unique.size()!=ids.size())add(out,NetworkDebugIssue::Error,"P003",ids,{}, {p->id()},p->sceneBoundingRect(),"Polygon repeats a nonclosing element.","Rebuild the polygon with each boundary element once.");}
    for(int i=0;i<ps.size();++i)for(int j=i+1;j<ps.size();++j){QPainterPath a,b;a.addPolygon(ps[i]->polygon());b.addPolygon(ps[j]->polygon());QPainterPath z=a.intersected(b);double area=0;for(auto poly:z.toFillPolygons()){double s=0;for(int k=0;k<poly.size();++k)s+=poly[k].x()*poly[(k+1)%poly.size()].y()-poly[(k+1)%poly.size()].x()*poly[k].y();area+=std::abs(s)/2;}double limit=std::max(NetworkDebugTolerances::minimumAreaPx2,NetworkDebugTolerances::relativeOverlapArea*std::min(ps[i]->area(),ps[j]->area()));if(area>limit)add(out,NetworkDebugIssue::Error,"P006",{}, {},{ps[i]->id(),ps[j]->id()},z.boundingRect(),"Polygon interiors overlap with positive area.","Inspect shared boundary lines and delete/reconnect the incorrect line.");}
    std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){if(a.severity!=b.severity)return a.severity<b.severity;if(a.code!=b.code)return a.code<b.code;return objects(a)<objects(b);});return out;
}

void NetworkDebugger::clearHighlights(QGraphicsScene*s){if(!s)return;for(auto*i:s->items())if(i->data(0).toInt()==OverlayTag){s->removeItem(i);delete i;}}
void NetworkDebugger::show(QWidget*parent,QGraphicsScene*scene,QGraphicsView*view){
    auto*d=new QDialog(parent);d->setAttribute(Qt::WA_DeleteOnClose);d->setWindowTitle("Debug All — Network Issues");auto*l=new QVBoxLayout(d);auto*summary=new QLabel(d);auto*t=new QTableWidget(d);t->setColumnCount(6);t->setHorizontalHeaderLabels({"Severity","Code","Objects","Location","Description","Suggested correction"});t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);l->addWidget(summary);l->addWidget(t);auto*buttons=new QHBoxLayout;for(QString s:{"Previous","Next","Re-run","Clear Highlights","Close"}){auto*b=new QPushButton(s,d);buttons->addWidget(b);if(s=="Close")QObject::connect(b,&QPushButton::clicked,d,&QDialog::close);if(s=="Clear Highlights")QObject::connect(b,&QPushButton::clicked,[scene]{clearHighlights(scene);});if(s=="Previous")QObject::connect(b,&QPushButton::clicked,[t]{if(t->rowCount())t->selectRow((t->currentRow()-1+t->rowCount())%t->rowCount());});if(s=="Next")QObject::connect(b,&QPushButton::clicked,[t]{if(t->rowCount())t->selectRow((t->currentRow()+1)%t->rowCount());});if(s=="Re-run")QObject::connect(b,&QPushButton::clicked,[d,parent,scene,view]{d->close();show(parent,scene,view);});}l->addLayout(buttons);
    auto issues=inspect(scene);int ec=0,wc=0;for(auto&i:issues){ec+=i.severity==NetworkDebugIssue::Error;wc+=i.severity==NetworkDebugIssue::Warning;int r=t->rowCount();t->insertRow(r);QString sev=i.severity==NetworkDebugIssue::Error?"ERROR":i.severity==NetworkDebugIssue::Warning?"WARNING":"INFO";QString loc=QString("%1, %2 — %3 × %4").arg(i.location.x(),0,'f',1).arg(i.location.y(),0,'f',1).arg(i.location.width(),0,'f',1).arg(i.location.height(),0,'f',1);for(int c=0;c<6;++c)t->setItem(r,c,new QTableWidgetItem(QStringList{sev,i.code,objects(i),loc,i.description,i.correction}[c]));t->item(r,0)->setData(Qt::UserRole,r);}summary->setText(ec||wc?QString("%1 Errors, %2 Warnings").arg(ec).arg(wc):"No errors or warnings detected.");
    QObject::connect(t,&QTableWidget::cellClicked,[=](int row,int){clearHighlights(scene);if(row<0||row>=issues.size())return;auto i=issues[row];QPainterPath hp;for(int id:i.vertexIds)if(auto*v=VertexItem::findVertexById(scene,id))hp.addEllipse(v->pos(),6,6);for(int id:i.lineIds)if(auto*e=LineItem::findLineById(scene,id)){auto q=path(e);if(!q.isEmpty()){hp.moveTo(q[0]);for(int k=1;k<q.size();++k)hp.lineTo(q[k]);}}for(int id:i.polygonIds)if(auto*p=PolygonItem::findPolygonById(scene,id))hp.addPolygon(p->polygon());auto*o=scene->addPath(hp,QPen(QColor(255,0,255),4,Qt::DashLine));o->setData(0,OverlayTag);o->setZValue(100000);QRectF r=i.location.adjusted(-10,-10,10,10);if(view&&!r.isEmpty())view->fitInView(r,Qt::KeepAspectRatio);});d->resize(1200,500);d->show();
}
