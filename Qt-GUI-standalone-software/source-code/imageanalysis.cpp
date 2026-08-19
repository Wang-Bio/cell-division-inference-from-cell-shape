#include "imageanalysis.h"

#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <set>
#include <map>
#include <tuple>
#include <cmath>
#include <limits>
#include <climits>
#include <functional>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <deque>

namespace {
// Compute the number of 0->1 transitions in the ordered neighborhood.
int computeConnectivity(const cv::Mat &img, int r, int c) {
    uchar p2 = img.at<uchar>(r - 1, c);
    uchar p3 = img.at<uchar>(r - 1, c + 1);
    uchar p4 = img.at<uchar>(r, c + 1);
    uchar p5 = img.at<uchar>(r + 1, c + 1);
    uchar p6 = img.at<uchar>(r + 1, c);
    uchar p7 = img.at<uchar>(r + 1, c - 1);
    uchar p8 = img.at<uchar>(r, c - 1);
    uchar p9 = img.at<uchar>(r - 1, c - 1);

    int transitions = (!p2 && p3) + (!p3 && p4) + (!p4 && p5) + (!p5 && p6) +
                      (!p6 && p7) + (!p7 && p8) + (!p8 && p9) + (!p9 && p2);
    return transitions;
}

// Count the number of white neighbors (value 1) around the pixel.
int countNeighbors(const cv::Mat &img, int r, int c) {
    uchar p2 = img.at<uchar>(r - 1, c);
    uchar p3 = img.at<uchar>(r - 1, c + 1);
    uchar p4 = img.at<uchar>(r, c + 1);
    uchar p5 = img.at<uchar>(r + 1, c + 1);
    uchar p6 = img.at<uchar>(r + 1, c);
    uchar p7 = img.at<uchar>(r + 1, c - 1);
    uchar p8 = img.at<uchar>(r, c - 1);
    uchar p9 = img.at<uchar>(r - 1, c - 1);

    int n1 = (p2 | p3) + (p4 | p5) + (p6 | p7) + (p8 | p9);
    int n2 = (p3 | p4) + (p5 | p6) + (p7 | p8) + (p9 | p2);
    return std::min(n1, n2);
}

// Count branches after removing the immediate 3x3 junction neighbourhood.
// Looking only at the eight neighbours can merge two genuine arms into one
// foreground run when a diagonal/stair-stepped arm leaves a junction beside a
// horizontal or vertical arm.
int countLocalBranches(const cv::Mat &img, int centerY, int centerX) {
    constexpr int radius = 3;
    constexpr int coreRadius = 1;
    constexpr int side = radius * 2 + 1;
    bool foreground[side][side] = {};
    bool visited[side][side] = {};

    for (int localY = 0; localY < side; ++localY) {
        for (int localX = 0; localX < side; ++localX) {
            const int dx = localX - radius;
            const int dy = localY - radius;
            if (std::max(std::abs(dx), std::abs(dy)) <= coreRadius)
                continue;
            foreground[localY][localX] =
                img.at<uchar>(centerY + dy, centerX + dx) != 0;
        }
    }

    int branches = 0;
    const int offsets[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                               {0, 1},   {1, -1},  {1, 0},  {1, 1}};
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            if (!foreground[y][x] || visited[y][x])
                continue;

            ++branches;
            std::vector<cv::Point> stack{cv::Point(x, y)};
            visited[y][x] = true;
            while (!stack.empty()) {
                const cv::Point point = stack.back();
                stack.pop_back();
                for (const auto &offset : offsets) {
                    const int nextY = point.y + offset[0];
                    const int nextX = point.x + offset[1];
                    if (nextX < 0 || nextY < 0 || nextX >= side || nextY >= side
                        || !foreground[nextY][nextX] || visited[nextY][nextX])
                        continue;
                    visited[nextY][nextX] = true;
                    stack.emplace_back(nextX, nextY);
                }
            }
        }
    }
    return branches;
}

// One of the two Guo-Hall sub-iterations.
void guoHallIteration(cv::Mat &img, int iter) {
    cv::Mat marker = cv::Mat::zeros(img.size(), CV_8UC1);

    for (int r = 1; r < img.rows - 1; ++r) {
        for (int c = 1; c < img.cols - 1; ++c) {
            uchar p1 = img.at<uchar>(r, c);
            if (p1 != 1)
                continue;

            int connectivity = computeConnectivity(img, r, c);
            int neighbors = countNeighbors(img, r, c);

            uchar p2 = img.at<uchar>(r - 1, c);
            uchar p4 = img.at<uchar>(r, c + 1);
            uchar p5 = img.at<uchar>(r + 1, c + 1);
            uchar p6 = img.at<uchar>(r + 1, c);
            uchar p8 = img.at<uchar>(r, c - 1);
            uchar p9 = img.at<uchar>(r - 1, c - 1);

            int m = (iter == 0)
                        ? ((p2 | p4 | !p6) & p8)
                        : ((p4 | p6 | !p8) & p2);

            if (connectivity == 1 && (neighbors >= 2 && neighbors <= 3) && m == 0) {
                marker.at<uchar>(r, c) = 1;
            }
        }
    }

    img &= ~marker;
}
} // namespace

namespace ImageAnalysis {

cv::Mat toGrayscale(const cv::Mat &input) {
    if (input.channels() == 1) {
        return input.clone();
    }

    cv::Mat gray;
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

cv::Mat binarize(const cv::Mat &grayscale, double threshold, bool invert) {
    CV_Assert(grayscale.channels() == 1);

    cv::Mat binary;
    int threshType = invert ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY;

    // When the caller does not provide a threshold, fall back to Otsu so the
    // whole image is not forced to white (threshold < 0 would otherwise make
    // every pixel pass the binary test).
    if (threshold < 0) {
        threshType |= cv::THRESH_OTSU;
        threshold = 0.0;
    }

    cv::threshold(grayscale, binary, threshold, 255, threshType);
    return binary;
}

cv::Mat guoHallThinning(const cv::Mat &binaryInput) {
    CV_Assert(binaryInput.type() == CV_8UC1);

    // Normalize to 0/1 so the logical operations match the algorithm description.
    cv::Mat img;
    cv::threshold(binaryInput, img, 0, 1, cv::THRESH_BINARY);

    cv::Mat prev;
    cv::Mat diff;
    do {
        img.copyTo(prev);
        guoHallIteration(img, 0);
        guoHallIteration(img, 1);
        cv::absdiff(img, prev, diff);
    } while (cv::countNonZero(diff) > 0);

    img *= 255;
    return img;
}

cv::Mat destairSkeleton(const cv::Mat &skeletonImage) {
    CV_Assert(skeletonImage.type() == CV_8UC1);

    const int width = skeletonImage.cols;
    const int height = skeletonImage.rows;

    if (width < 3 || height < 3)
        return skeletonImage.clone();

    std::vector<uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0U);
    for (int y = 0; y < height; ++y) {
        const uchar *line = skeletonImage.ptr<uchar>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x);
            pixels[index] = line[x] > 0 ? 1U : 0U;
        }
    }

    auto at = [&pixels, width](int y, int x) -> uint8_t & {
        return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
    };

    for (int pass = 0; pass < 2; ++pass) {
        std::vector<std::size_t> remove;
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                if (at(y, x) == 0U)
                    continue;

                const uint8_t e = at(y, x + 1);
                const uint8_t ne = at(y - 1, x + 1);
                const uint8_t n = at(y - 1, x);
                const uint8_t nw = at(y - 1, x - 1);
                const uint8_t w = at(y, x - 1);
                const uint8_t sw = at(y + 1, x - 1);
                const uint8_t s0 = at(y + 1, x);
                const uint8_t se = at(y + 1, x + 1);

                if (pass == 0) {
                    const bool case1 = n && (e && !ne && !sw && (!w || !s0));
                    const bool case2 = n && (w && !nw && !se && (!e || !s0));
                    if (case1 || case2)
                        remove.push_back(static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                                         + static_cast<std::size_t>(x));
                } else {
                    const bool case1 = s0 && (e && !se && !nw && (!w || !n));
                    const bool case2 = s0 && (w && !sw && !ne && (!e || !n));
                    if (case1 || case2)
                        remove.push_back(static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                                         + static_cast<std::size_t>(x));
                }
            }
        }

        if (remove.empty())
            break;

        for (std::size_t index : remove)
            pixels[index] = 0U;
    }

    cv::Mat destaired(height, width, CV_8UC1, cv::Scalar(0));
    for (int y = 0; y < height; ++y) {
        uchar *line = destaired.ptr<uchar>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x);
            line[x] = pixels[index] ? 255 : 0;
        }
    }

    return destaired;
}

cv::Mat removeSpurs(const cv::Mat &skeletonImage) {
    CV_Assert(skeletonImage.type() == CV_8UC1);

    const int width = skeletonImage.cols;
    const int height = skeletonImage.rows;

    if (width == 0 || height == 0)
        return skeletonImage.clone();

    std::vector<uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0U);
    for (int y = 0; y < height; ++y) {
        const uchar *line = skeletonImage.ptr<uchar>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x);
            pixels[index] = line[x] > 0 ? 1U : 0U;
        }
    }

    auto at = [&pixels, width](int y, int x) -> uint8_t & {
        return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
    };

    const int offsets[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                               {0, 1},  {1, -1}, {1, 0},  {1, 1}};

    while (true) {
        std::vector<std::size_t> remove;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (at(y, x) == 0U)
                    continue;

                int neighborCount = 0;
                for (const auto &offset : offsets) {
                    const int ny = y + offset[0];
                    const int nx = x + offset[1];
                    if (ny < 0 || ny >= height || nx < 0 || nx >= width)
                        continue;
                    neighborCount += at(ny, nx);
                }

                if (neighborCount == 1)
                    remove.push_back(static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                                     + static_cast<std::size_t>(x));
            }
        }

        if (remove.empty())
            break;

        for (std::size_t index : remove)
            pixels[index] = 0U;
    }

    cv::Mat cleaned(height, width, CV_8UC1, cv::Scalar(0));
    for (int y = 0; y < height; ++y) {
        uchar *line = cleaned.ptr<uchar>(y);
        for (int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x);
            line[x] = pixels[index] ? 255 : 0;
        }
    }

    return cleaned;
}


cv::Mat segmentWithGuoHall(const cv::Mat &input, double threshold, bool invertInput) {
    cv::Mat gray = toGrayscale(input);
    cv::Mat binary = binarize(gray, threshold, invertInput);
    cv::Mat skeleton = guoHallThinning(binary);
    cv::Mat destaired_skeleton = destairSkeleton(skeleton);
    cv::Mat despurred_skeleton = removeSpurs(destaired_skeleton);
    return despurred_skeleton;
}

std::vector<cv::Point2d> detectVertices(const cv::Mat &skeletonImage) {
    CV_Assert(skeletonImage.type() == CV_8UC1);

    const int rows = skeletonImage.rows;
    const int cols = skeletonImage.cols;
    if (rows < 3 || cols < 3)
        return {};

    std::vector<cv::Point> vertices;
    vertices.reserve(rows * cols / 16); // heuristic reserve

    constexpr int branchRadius = 3;
    for (int y = 1; y < rows - 1; ++y) {
        const uchar *row = skeletonImage.ptr<uchar>(y);
        for (int x = 1; x < cols - 1; ++x) {
            if (row[x] == 0)
                continue;

            // Crossing number: separate foreground runs in the clockwise
            // 8-neighbour ring, rather than raw occupied-neighbour count.
            static const int ring[8][2] = {{0,-1},{1,-1},{1,0},{1,1},
                                           {0,1},{-1,1},{-1,0},{-1,-1}};
            bool occupied[8];
            int occupiedNeighbors = 0;
            for (int k = 0; k < 8; ++k) {
                occupied[k] = skeletonImage.at<uchar>(y + ring[k][1], x + ring[k][0]) != 0;
                occupiedNeighbors += occupied[k] ? 1 : 0;
            }
            int runs = 0;
            for (int k = 0; k < 8; ++k)
                if (!occupied[(k + 7) % 8] && occupied[k]) ++runs;

            // Use the larger local topology wherever it fits: besides recovering
            // acute junctions, it rejects nearby unrelated lines and junctions
            // with more than four arms. Crossing number is only a frame fallback.
            const bool canCheckLocalTopology =
                x >= branchRadius && y >= branchRadius
                && x < cols - branchRadius && y < rows - branchRadius;
            const int localBranches = canCheckLocalTopology
                ? countLocalBranches(skeletonImage, y, x) : 0;
            const bool hasThreeOrFourBranches = localBranches == 3 || localBranches == 4;
            const bool boundaryFallback = !canCheckLocalTopology
                && (runs == 3 || runs == 4);
            if ((occupiedNeighbors >= 3 && hasThreeOrFourBranches) || boundaryFallback)
                vertices.emplace_back(x, y);
        }
    }

    // Cluster nearby vertices together to avoid redundancy. Any vertices within
    // a Euclidean distance of 2.0 pixels are merged, and the cluster centroid
    // is returned as the representative vertex.
    const double radius = 2.0;
    const double radiusSquared = radius * radius;
    std::vector<cv::Point2d> clustered;
    std::vector<bool> visited(vertices.size(), false);

    for (std::size_t i = 0; i < vertices.size(); ++i) {
        if (visited[i])
            continue;

        std::vector<std::size_t> stack = {i};
        std::vector<cv::Point2d> cluster;
        visited[i] = true;

        while (!stack.empty()) {
            const std::size_t idx = stack.back();
            stack.pop_back();

            cluster.push_back(vertices[idx]);

            for (std::size_t j = 0; j < vertices.size(); ++j) {
                if (visited[j])
                    continue;

                const double dx = static_cast<double>(vertices[j].x - vertices[idx].x);
                const double dy = static_cast<double>(vertices[j].y - vertices[idx].y);
                const double distSquared = dx * dx + dy * dy;

                if (distSquared <= radiusSquared) {
                    visited[j] = true;
                    stack.push_back(j);
                }
            }
        }

        double sumX = 0.0;
        double sumY = 0.0;
        for (const cv::Point &p : cluster) {
            sumX += static_cast<double>(p.x);
            sumY += static_cast<double>(p.y);
        }

        const double count = static_cast<double>(cluster.size());
        const cv::Point2d center(sumX / count, sumY / count);
        clustered.push_back(center);
    }

    return clustered;
}

namespace {
struct BackgroundTopology {
    cv::Mat labels, acceptedMask;
    std::vector<bool> accepted;
};
BackgroundTopology backgroundTopology(const cv::Mat &s)
{
    cv::Mat background; cv::compare(s,0,background,cv::CMP_EQ);
    BackgroundTopology topology; cv::Mat stats,centroids;
    const int count=cv::connectedComponentsWithStats(background,topology.labels,stats,centroids,4);
    std::vector<bool> frame(count,false);
    auto mark=[&](int x,int y){int l=topology.labels.at<int>(y,x);if(l>0)frame[l]=true;};
    for(int x=0;x<s.cols;++x){mark(x,0);mark(x,s.rows-1);} for(int y=0;y<s.rows;++y){mark(0,y);mark(s.cols-1,y);}
    std::vector<int> areas; for(int l=1;l<count;++l)if(!frame[l])areas.push_back(stats.at<int>(l,cv::CC_STAT_AREA));
    double median=0; if(!areas.empty()){auto m=areas.begin()+areas.size()/2;std::nth_element(areas.begin(),m,areas.end());median=*m;}
    double voidArea=std::max(double(ImageAnalysis::kMinimumInternalVoidAreaPixels),ImageAnalysis::kInternalVoidMedianAreaFactor*median);
    topology.accepted.assign(count,false); topology.acceptedMask=cv::Mat(s.size(),CV_8UC1,cv::Scalar(0));
    for(int l=1;l<count;++l)topology.accepted[l]=frame[l]||(median>0&&stats.at<int>(l,cv::CC_STAT_AREA)>=voidArea);
    for(int y=0;y<s.rows;++y)for(int x=0;x<s.cols;++x){int l=topology.labels.at<int>(y,x);if(l>0&&topology.accepted[l])topology.acceptedMask.at<uchar>(y,x)=255;}
    return topology;
}
cv::Mat boundaryBackgroundMask(const cv::Mat&s){return backgroundTopology(s).acceptedMask;}
double distanceSquared(const cv::Point2d&a,const cv::Point2d&b){cv::Point2d d=a-b;return d.dot(d);}
cv::Point nearestSkeleton(const cv::Mat&s,const cv::Point2d&p,int radius,bool *ok=nullptr){
    cv::Point best; double bd=std::numeric_limits<double>::infinity(); bool found=false;
    for(int y=std::max(0,cvFloor(p.y)-radius);y<=std::min(s.rows-1,cvCeil(p.y)+radius);++y)
      for(int x=std::max(0,cvFloor(p.x)-radius);x<=std::min(s.cols-1,cvCeil(p.x)+radius);++x)if(s.at<uchar>(y,x)){
        double d=distanceSquared(p,cv::Point2d(x,y)); if(d<=radius*radius&&(d<bd||(d==bd&&std::tie(y,x)<std::tie(best.y,best.x)))){bd=d;best={x,y};found=true;}}
    if(ok)*ok=found; return best;
}
std::vector<cv::Point2d> circularSmooth(const std::vector<cv::Point2d>&p,double sigma){
    int n=p.size(),r=std::max(1,int(std::ceil(3*sigma)));std::vector<double>w(2*r+1);double sum=0;
    for(int k=-r;k<=r;++k){w[k+r]=std::exp(-.5*k*k/(sigma*sigma));sum+=w[k+r];}
    std::vector<cv::Point2d>q(n);for(int i=0;i<n;++i)for(int k=-r;k<=r;++k)q[i]+=p[(i+k+n)%n]*(w[k+r]/sum);return q;
}
std::vector<cv::Point2d> resampleClosed(const std::vector<cv::Point>&raw,double spacing){
    std::vector<cv::Point2d>p;for(auto&v:raw)p.emplace_back(v);if(p.size()<2)return p;
    // Canonical clockwise orientation and lexicographically smallest seam.
    double area=0;for(size_t i=0;i<p.size();++i){auto&a=p[i];auto&b=p[(i+1)%p.size()];area+=a.x*b.y-b.x*a.y;}if(area<0)std::reverse(p.begin(),p.end());
    auto seam=std::min_element(p.begin(),p.end(),[](auto&a,auto&b){return std::tie(a.y,a.x)<std::tie(b.y,b.x);});std::rotate(p.begin(),seam,p.end());
    std::vector<double>cum(1,0);for(size_t i=0;i<p.size();++i)cum.push_back(cum.back()+cv::norm(p[(i+1)%p.size()]-p[i]));double length=cum.back();int count=std::max(3,int(std::round(length/spacing)));
    std::vector<cv::Point2d>out;out.reserve(count);size_t edge=0;for(int i=0;i<count;++i){double d=length*i/count;while(edge+1<cum.size()&&cum[edge+1]<d)++edge;double len=cum[edge+1]-cum[edge],t=len? (d-cum[edge])/len:0;out.push_back(p[edge]*(1-t)+p[(edge+1)%p.size()]*t);}return out;
}
double segmentError(const std::vector<cv::Point2d>&p,int i,int j){double e=0;cv::Point2d d=p[j]-p[i];double l2=d.dot(d);for(int k=i+1;k<j;++k){double t=l2?std::max(0.,std::min(1.,(p[k]-p[i]).dot(d)/l2)):0;e=std::max(e,cv::norm(p[k]-(p[i]+t*d)));}return e;}
int skeletonDegree(const cv::Mat&s,const cv::Point&p){
    static const int ring[8][2]={{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
    bool occupied[8]={};for(int k=0;k<8;++k){int x=p.x+ring[k][0],y=p.y+ring[k][1];occupied[k]=x>=0&&y>=0&&x<s.cols&&y<s.rows&&s.at<uchar>(y,x);}
    int runs=0;for(int k=0;k<8;++k)if(occupied[k]&&!occupied[(k+7)%8])++runs;return runs;
}
}

bool isOuterBoundaryPoint(const cv::Mat&s,const cv::Point2d&p)
{
    auto t=backgroundTopology(s);int cx=cvRound(p.x),cy=cvRound(p.y);for(int y=std::max(0,cy-1);y<=std::min(s.rows-1,cy+1);++y)for(int x=std::max(0,cx-1);x<=std::min(s.cols-1,cx+1);++x){int l=t.labels.at<int>(y,x);if(l>0&&t.accepted[l])return true;}return false;
}

VertexDetectionResult detectCellArcVertices(const cv::Mat&s,const OuterDetectionParameters&par)
{
    CV_Assert(s.type()==CV_8UC1); VertexDetectionResult result; auto topology=backgroundTopology(s);
    const auto branches=detectVertices(s); std::vector<cv::Point2d> region;
    cv::Mat candidates(s.size(),CV_8UC1,cv::Scalar(0));int r=par.outerJunctionNeighborhood;
    for(int y=par.frameGuard+1;y<s.rows-par.frameGuard-1;++y)for(int x=par.frameGuard+1;x<s.cols-par.frameGuard-1;++x)if(s.at<uchar>(y,x)){
      bool exterior=false;std::set<int>cells;for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx)if(dx*dx+dy*dy<=r*r){int l=topology.labels.at<int>(y+dy,x+dx);if(l>0){if(topology.accepted[l])exterior=true;else cells.insert(l);}}
      if(exterior&&cells.size()>=2)candidates.at<uchar>(y,x)=255;
    }
    cv::Mat labs,stats,cents;int n=cv::connectedComponentsWithStats(candidates,labs,stats,cents,8);for(int l=1;l<n;++l){bool ok;cv::Point q=nearestSkeleton(s,{cents.at<double>(l,0),cents.at<double>(l,1)},r,&ok);if(ok)region.emplace_back(q);}
    std::sort(region.begin(),region.end(),[](auto&a,auto&b){return std::tie(a.y,a.x)<std::tie(b.y,b.x);});std::vector<cv::Point2d> anchors;
    for(auto&p:region){bool duplicate=false;for(auto&q:anchors)if(distanceSquared(p,q)<=par.junctionMergeRadius*par.junctionMergeRadius){duplicate=true;break;}if(!duplicate)anchors.push_back(p);}
    for(auto&p:branches){int nearest=-1;double best=par.junctionMergeRadius*par.junctionMergeRadius;for(int i=0;i<(int)anchors.size();++i){double d=distanceSquared(p,anchors[i]);if(d<=best){best=d;nearest=i;}}if(nearest<0){if(isOuterBoundaryPoint(s,p))anchors.push_back(p);else result.vertices.push_back({p,DetectedVertexKind::InteriorJunction});}}
    for(auto&p:anchors)result.vertices.push_back({p,DetectedVertexKind::BoundaryJunction});
    std::vector<std::vector<cv::Point>> rawContours;cv::findContours(topology.acceptedMask,rawContours,cv::RETR_LIST,cv::CHAIN_APPROX_NONE);int contourId=0;
    for(auto&raw:rawContours){bool frame=false;for(auto&p:raw)frame|=p.x==0||p.y==0||p.x==s.cols-1||p.y==s.rows-1;if(frame||raw.size()<3)continue;auto contour=resampleClosed(raw,par.contourSampleSpacing);if(contour.size()<3)continue;
      struct A{int index;cv::Point2d point;double distance;};std::map<int,A> mapped;for(auto&a:anchors){int bi=-1;double bd=std::numeric_limits<double>::infinity();for(int i=0;i<(int)contour.size();++i){double d=distanceSquared(a,contour[i]);if(d<bd){bd=d;bi=i;}}if(std::sqrt(bd)<=par.anchorContourTolerance){auto it=mapped.find(bi);if(it==mapped.end()||bd<it->second.distance)mapped[bi]={bi,a,bd};else result.vertices.push_back({a,DetectedVertexKind::Ambiguous,contourId,-1});}}
      std::vector<A> ma;for(auto&v:mapped)ma.push_back(v.second);if(ma.size()<2){result.diagnostics.push_back("outer contour "+std::to_string(contourId)+" has fewer than two reliable anchors");++contourId;continue;}
      auto curvSmooth=circularSmooth(contour,par.curvatureSigma),fitSmooth=circularSmooth(contour,par.fitSigma);int N=contour.size();
      for(int arc=0;arc<(int)ma.size();++arc){int start=ma[arc].index,end=ma[(arc+1)%ma.size()].index;int length=(end-start+N)%N;if(length<2)continue;std::vector<cv::Point2d> u(length+1),cs(length+1),fs(length+1);for(int k=0;k<=length;++k){int z=(start+k)%N;u[k]=contour[z];cs[k]=curvSmooth[z];fs[k]=fitSmooth[z];}
        struct C{int i;double angle;};std::vector<C> curvature;int w=par.curvatureWindow;for(int i=w;i+w<=length;++i){auto a=cs[i]-cs[i-w],b=cs[i+w]-cs[i];double den=cv::norm(a)*cv::norm(b);if(!den)continue;double angle=std::acos(std::max(-1.,std::min(1.,a.dot(b)/den)))*180/CV_PI;if(angle>=par.curvatureThresholdDegrees&&cv::norm(u[i]-u.front())>=par.junctionExclusion&&cv::norm(u[i]-u.back())>=par.junctionExclusion)curvature.push_back({i,angle});}
        std::stable_sort(curvature.begin(),curvature.end(),[](auto&a,auto&b){return a.angle!=b.angle?a.angle>b.angle:a.i<b.i;});std::vector<C> peaks;for(auto&c:curvature){bool close=false;for(auto&p:peaks)if(std::abs(c.i-p.i)<=par.curvatureNmsRadius)close=true;if(!close)peaks.push_back(c);}
        int m=length+1;std::vector<int>links(m,INT_MAX),prev(m,-1);std::vector<double>cost(m,std::numeric_limits<double>::infinity());links[0]=0;cost[0]=0;for(int j=1;j<m;++j)for(int i=0;i<j;++i){double e=segmentError(fs,i,j);if(e>par.maximumFitError)continue;int nl=links[i]+1;double nc=cost[i]+e*e;if(nl<links[j]||(nl==links[j]&&(nc<cost[j]-1e-12||(std::abs(nc-cost[j])<=1e-12&&i<prev[j])))){links[j]=nl;cost[j]=nc;prev[j]=i;}}
        std::vector<int>imai;for(int at=prev[m-1];at>0;at=prev[at])imai.push_back(at);std::sort(imai.begin(),imai.end());struct Pair{double d;int p,q;};std::vector<Pair>pairs;for(int pi=0;pi<(int)peaks.size();++pi)for(int qi=0;qi<(int)imai.size();++qi){double d=cv::norm(u[peaks[pi].i]-u[imai[qi]]);if(d<=par.consensusRadius)pairs.push_back({d,pi,qi});}std::sort(pairs.begin(),pairs.end(),[](auto&a,auto&b){return std::tie(a.d,a.p,a.q)<std::tie(b.d,b.p,b.q);});std::set<int>up,uq;for(auto&pair:pairs)if(!up.count(pair.p)&&!uq.count(pair.q)){auto pos=u[peaks[pair.p].i];bool ok;cv::Point snap=nearestSkeleton(s,pos,par.skeletonSnapRadius,&ok);if(!ok||skeletonDegree(s,snap)!=2)continue;bool excluded=false;for(auto&a:anchors)if(cv::norm(cv::Point2d(snap)-a)<par.junctionExclusion)excluded=true;for(auto&v:result.vertices)if(v.kind==DetectedVertexKind::ContourSupport&&distanceSquared(v.position,snap)<=1.0)excluded=true;if(!excluded){result.vertices.push_back({cv::Point2d(snap),DetectedVertexKind::ContourSupport,contourId,arc});up.insert(pair.p);uq.insert(pair.q);}}
      }++contourId;
    }
    // Secondary outer-cell area screen.  It does not alter the CSS/Imai-Iri
    // consensus above: it may only use otherwise-valid CSS maxima, then prune
    // supports while retaining the original shortcut-error constraint.
    struct ScreenCandidate { int index; cv::Point snap; };
    struct ScreenArc {
        int contourId=-1, arcId=-1, cellLabel=-1;
        std::vector<cv::Point2d> raw, fitted;
        std::vector<ScreenCandidate> valid;
        std::set<int> selected;
    };
    std::vector<ScreenArc> screenArcs;
    rawContours.clear(); cv::findContours(topology.acceptedMask,rawContours,cv::RETR_LIST,cv::CHAIN_APPROX_NONE); contourId=0;
    for(auto&rawContour:rawContours){bool frame=false;for(auto&p:rawContour)frame|=p.x==0||p.y==0||p.x==s.cols-1||p.y==s.rows-1;if(frame||rawContour.size()<3)continue;
      auto contour=resampleClosed(rawContour,par.contourSampleSpacing);if(contour.size()<3)continue;auto smooth=circularSmooth(contour,par.curvatureSigma),fit=circularSmooth(contour,par.fitSigma);int count=contour.size();
      std::map<int,cv::Point2d> mapped;for(auto&a:anchors){int best=-1;double bd=std::numeric_limits<double>::infinity();for(int i=0;i<count;++i){double d=distanceSquared(a,contour[i]);if(d<bd){bd=d;best=i;}}if(std::sqrt(bd)<=par.anchorContourTolerance&&(!mapped.count(best)||distanceSquared(a,contour[best])<distanceSquared(mapped[best],contour[best])))mapped[best]=a;}
      std::vector<int> indices;for(auto&m:mapped)indices.push_back(m.first);if(indices.size()<2){++contourId;continue;}
      for(int arc=0;arc<(int)indices.size();++arc){int start=indices[arc],end=indices[(arc+1)%indices.size()],length=(end-start+count)%count;if(length<2)continue;ScreenArc item;item.contourId=contourId;item.arcId=arc;
        for(int k=0;k<=length;++k){int z=(start+k)%count;item.raw.push_back(contour[z]);item.fitted.push_back(fit[z]);}
        std::map<int,int> cellVotes;for(auto&p:item.raw)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int x=cvRound(p.x)+dx,y=cvRound(p.y)+dy;if(x>=0&&y>=0&&x<s.cols&&y<s.rows){int label=topology.labels.at<int>(y,x);if(label>0&&!topology.accepted[label])++cellVotes[label];}}
        int votes=0;for(auto&v:cellVotes)if(v.second>votes){votes=v.second;item.cellLabel=v.first;}if(item.cellLabel<0)continue;
        int w=par.curvatureWindow;std::vector<std::pair<double,int>> peaks;for(int i=w;i+w<=length;++i){auto a=smooth[(start+i)%count]-smooth[(start+i-w)%count],b=smooth[(start+i+w)%count]-smooth[(start+i)%count];double den=cv::norm(a)*cv::norm(b);if(!den)continue;double angle=std::acos(std::max(-1.,std::min(1.,a.dot(b)/den)))*180/CV_PI;if(angle>=par.curvatureThresholdDegrees&&cv::norm(item.raw[i]-item.raw.front())>=par.junctionExclusion&&cv::norm(item.raw[i]-item.raw.back())>=par.junctionExclusion)peaks.push_back({angle,i});}
        std::stable_sort(peaks.begin(),peaks.end(),[](auto&a,auto&b){return a.first!=b.first?a.first>b.first:a.second<b.second;});std::vector<int> kept;for(auto&p:peaks){bool close=false;for(int i:kept)if(std::abs(i-p.second)<=par.curvatureNmsRadius)close=true;if(!close)kept.push_back(p.second);}
        for(int i:kept){bool ok;cv::Point snap=nearestSkeleton(s,item.raw[i],par.skeletonSnapRadius,&ok);if(!ok||skeletonDegree(s,snap)!=2)continue;bool excluded=false;for(auto&a:anchors)if(cv::norm(cv::Point2d(snap)-a)<par.junctionExclusion)excluded=true;if(!excluded)item.valid.push_back({i,snap});}
        for(auto&v:result.vertices)if(v.kind==DetectedVertexKind::ContourSupport&&v.contourId==contourId&&v.arcId==arc)for(auto&c:item.valid)if(distanceSquared(v.position,c.snap)<=1.0)item.selected.insert(c.index);
        screenArcs.push_back(std::move(item));
      }++contourId;
    }
    std::map<int,int> cellAreas;for(int y=0;y<s.rows;++y)for(int x=0;x<s.cols;++x){int label=topology.labels.at<int>(y,x);if(label>0&&!topology.accepted[label])++cellAreas[label];}
    auto areaError=[&](int label){cv::Mat reference(s.size(),CV_8UC1,cv::Scalar(0)),changed(s.size(),CV_8UC1,cv::Scalar(0));for(int y=0;y<s.rows;++y)for(int x=0;x<s.cols;++x)if(topology.labels.at<int>(y,x)==label)reference.at<uchar>(y,x)=255;for(auto&arc:screenArcs)if(arc.cellLabel==label){std::vector<cv::Point> polygon;for(auto&p:arc.raw)polygon.emplace_back(cvRound(p.x),cvRound(p.y));std::vector<int> chosen(arc.selected.begin(),arc.selected.end());std::sort(chosen.begin(),chosen.end(),std::greater<int>());polygon.emplace_back(cvRound(arc.raw.back().x),cvRound(arc.raw.back().y));for(int i:chosen)polygon.emplace_back(cvRound(arc.raw[i].x),cvRound(arc.raw[i].y));polygon.emplace_back(cvRound(arc.raw.front().x),cvRound(arc.raw.front().y));if(polygon.size()>=3)cv::fillPoly(changed,std::vector<std::vector<cv::Point>>{polygon},cv::Scalar(255));}cv::Mat approximate,difference;cv::bitwise_xor(reference,changed,approximate);cv::bitwise_xor(reference,approximate,difference);return cellAreas[label]?double(cv::countNonZero(difference))/cellAreas[label]:0.0;};
    std::set<int> outerCells;for(auto&a:screenArcs)outerCells.insert(a.cellLabel);for(int cell:outerCells){double before=areaError(cell),current=before;
      while(current>par.maxAreaErrorFraction){ScreenArc*bestArc=nullptr;int bestIndex=-1;double best=current;for(auto&arc:screenArcs)if(arc.cellLabel==cell)for(auto&candidate:arc.valid)if(!arc.selected.count(candidate.index)){arc.selected.insert(candidate.index);double trial=areaError(cell);arc.selected.erase(candidate.index);if(trial<best-1e-12){best=trial;bestArc=&arc;bestIndex=candidate.index;}}if(!bestArc)break;bestArc->selected.insert(bestIndex);current=best;}
      bool changed=true;while(changed){changed=false;for(auto&arc:screenArcs)if(arc.cellLabel==cell){std::vector<int> selected(arc.selected.begin(),arc.selected.end());for(int index:selected){auto it=std::lower_bound(selected.begin(),selected.end(),index);int pos=it-selected.begin(),previous=pos?selected[pos-1]:0,next=pos+1<(int)selected.size()?selected[pos+1]:int(arc.raw.size()-1);if(segmentError(arc.fitted,previous,next)>par.maximumFitError)continue;arc.selected.erase(index);double trial=areaError(cell);if(trial<=par.maxAreaErrorFraction){current=trial;changed=true;break;}arc.selected.insert(index);}if(changed)break;}}
      current=areaError(cell);result.diagnostics.push_back("outer cell "+std::to_string(cell)+" area error "+std::to_string(before)+" -> "+std::to_string(current)+(current>par.maxAreaErrorFraction?" (unresolved)":""));
    }
    // Final, conservative redundancy screen.  This intentionally runs after
    // the established CSS/Imai-Iri detection and area refinement above.  A
    // rejected or numerically ambiguous candidate is simply retained.
    auto cross=[](const cv::Point2d&a,const cv::Point2d&b,const cv::Point2d&c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);};
    auto samePoint=[&](const cv::Point2d&a,const cv::Point2d&b){return distanceSquared(a,b)<=1e-12;};
    auto intersects=[&](const cv::Point2d&a,const cv::Point2d&b,const cv::Point2d&c,const cv::Point2d&d){
      if(samePoint(a,c)||samePoint(a,d)||samePoint(b,c)||samePoint(b,d))return false;
      double ab1=cross(a,b,c),ab2=cross(a,b,d),cd1=cross(c,d,a),cd2=cross(c,d,b),eps=1e-9;
      // Collinearity/endpoint contact is uncertain and is therefore rejected.
      if(std::abs(ab1)<=eps||std::abs(ab2)<=eps||std::abs(cd1)<=eps||std::abs(cd2)<=eps)return true;
      return (ab1<0)!=(ab2<0)&&(cd1<0)!=(cd2<0);
    };
    auto angle=[](const cv::Point2d&a,const cv::Point2d&b){double den=cv::norm(a)*cv::norm(b);if(den<=1e-12)return std::numeric_limits<double>::infinity();return std::acos(std::max(-1.,std::min(1.,a.dot(b)/den)))*180/CV_PI;};
    std::map<std::pair<int,int>,int> deletedPerArc;
    for(int cell:outerCells){double pruneBefore=areaError(cell),current=pruneBefore;bool accepted=true;
      while(accepted){accepted=false;struct Redundant {double marginal;ScreenArc*arc;int index;};std::vector<Redundant> candidates;
        for(auto&arc:screenArcs)if(arc.cellLabel==cell&&deletedPerArc[{arc.contourId,arc.arcId}]<par.maxDeletedPerArc){std::vector<int> chosen(arc.selected.begin(),arc.selected.end());for(int pos=0;pos<(int)chosen.size();++pos){int q=chosen[pos],p=pos?chosen[pos-1]:0,r=pos+1<(int)chosen.size()?chosen[pos+1]:int(arc.raw.size()-1);double contribution=std::abs(cross(arc.raw[p],arc.raw[q],arc.raw[r]))*.5/std::max(1,cellAreas[cell]);candidates.push_back({contribution,&arc,q});}}
        std::stable_sort(candidates.begin(),candidates.end(),[](const Redundant&a,const Redundant&b){return a.marginal!=b.marginal?a.marginal<b.marginal:std::tie(a.arc->contourId,a.arc->arcId,a.index)<std::tie(b.arc->contourId,b.arc->arcId,b.index);});
        for(auto candidate:candidates){ScreenArc&arc=*candidate.arc;if(!arc.selected.count(candidate.index)||!(candidate.marginal<par.maxRedundantAreaContributionFraction))continue;std::vector<int> chosen(arc.selected.begin(),arc.selected.end());auto found=std::lower_bound(chosen.begin(),chosen.end(),candidate.index);int pos=found-chosen.begin(),p=pos?chosen[pos-1]:0,r=pos+1<(int)chosen.size()?chosen[pos+1]:int(arc.raw.size()-1);
          double deviation=segmentError(arc.raw,p,r);if(!std::isfinite(deviation)||deviation>par.maxRedundantContourDeviationPixels)continue;
          if(chosen.size()==1&&deviation>par.nearlyStraightContourDeviationPixels)continue;
          cv::Point2d pq=arc.raw[candidate.index]-arc.raw[p],qr=arc.raw[r]-arc.raw[candidate.index],pr=arc.raw[r]-arc.raw[p];double oldMismatch=0,newMismatch=0;bool certain=true;
          for(int k=p;k<=r;++k){int lo=std::max(p,k-1),hi=std::min(r,k+1);cv::Point2d tangent=arc.raw[hi]-arc.raw[lo];double oldAngle=angle(tangent,k<=candidate.index?pq:qr),newAngle=angle(tangent,pr);if(!std::isfinite(oldAngle)||!std::isfinite(newAngle)){certain=false;break;}oldMismatch=std::max(oldMismatch,oldAngle);newMismatch=std::max(newMismatch,newAngle);}if(!certain||newMismatch>oldMismatch+par.tangentComparisonEpsilonDegrees)continue;
          // P/R are junctions when they are arc endpoints; their raw tangent
          // mismatch must strictly improve rather than merely tie.
          bool junctionMismatchReduced=true;if(p==0){double oldJ=angle(arc.raw[1]-arc.raw[0],pq),newJ=angle(arc.raw[1]-arc.raw[0],pr);junctionMismatchReduced=std::isfinite(oldJ)&&std::isfinite(newJ)&&newJ+par.tangentComparisonEpsilonDegrees<oldJ;}if(junctionMismatchReduced&&r==(int)arc.raw.size()-1){double oldJ=angle(arc.raw[r]-arc.raw[r-1],qr),newJ=angle(arc.raw[r]-arc.raw[r-1],pr);junctionMismatchReduced=std::isfinite(oldJ)&&std::isfinite(newJ)&&newJ+par.tangentComparisonEpsilonDegrees<oldJ;}if(!junctionMismatchReduced)continue;
          bool crossing=false;for(auto&other:screenArcs){std::vector<int> points{0};points.insert(points.end(),other.selected.begin(),other.selected.end());points.push_back(int(other.raw.size()-1));for(int i=1;i<(int)points.size();++i){if(&other==&arc&&((points[i-1]==p&&points[i]==candidate.index)||(points[i-1]==candidate.index&&points[i]==r)))continue;if(intersects(arc.raw[p],arc.raw[r],other.raw[points[i-1]],other.raw[points[i]])){crossing=true;break;}}if(crossing)break;}if(crossing||samePoint(arc.raw[p],arc.raw[r]))continue;
          arc.selected.erase(candidate.index);double trial=areaError(cell);if(!std::isfinite(trial)||trial>=par.maxAreaErrorFraction||trial-current>par.maxRedundantAreaErrorIncreaseFraction+1e-12){arc.selected.insert(candidate.index);continue;}
          ++deletedPerArc[{arc.contourId,arc.arcId}];cv::Point report(cvRound(arc.raw[candidate.index].x),cvRound(arc.raw[candidate.index].y));for(auto&valid:arc.valid)if(valid.index==candidate.index){report=valid.snap;break;}result.diagnostics.push_back("redundancy prune outer cell "+std::to_string(cell)+" deleted CONTOUR_SUPPORT ("+std::to_string(report.x)+","+std::to_string(report.y)+") area error "+std::to_string(current)+" -> "+std::to_string(trial));current=trial;accepted=true;break;
        }
      }
      result.diagnostics.push_back("redundancy prune outer cell "+std::to_string(cell)+" summary "+std::to_string(pruneBefore)+" -> "+std::to_string(current));
    }
    result.vertices.erase(std::remove_if(result.vertices.begin(),result.vertices.end(),[](const VertexDetection&v){return v.kind==DetectedVertexKind::ContourSupport;}),result.vertices.end());for(auto&arc:screenArcs)for(auto&candidate:arc.valid)if(arc.selected.count(candidate.index)){bool duplicate=false;for(auto&vertex:result.vertices)if(vertex.kind==DetectedVertexKind::ContourSupport&&distanceSquared(vertex.position,candidate.snap)<=1.0)duplicate=true;if(!duplicate)result.vertices.push_back({cv::Point2d(candidate.snap),DetectedVertexKind::ContourSupport,arc.contourId,arc.arcId});}
    return result;
}

std::vector<LineConnection> detectLines(const cv::Mat &skeletonImage,
                                        const std::vector<cv::Point> &vertices,
                                        int tolerance) {
    CV_Assert(skeletonImage.type() == CV_8UC1);
    CV_Assert(tolerance >= 0);

    const int rows = skeletonImage.rows;
    const int cols = skeletonImage.cols;
    if (rows == 0 || cols == 0 || vertices.size() < 2)
        return {};

    // Grow all vertex labels through the skeleton at the same time.  A vertex
    // is an absorbing source: a route cannot pass through it and continue to a
    // distant vertex.  Unlike connected-component filtering, wave fronts can
    // still distinguish several legitimate arms that remain 8-connected just
    // outside a small junction neighbourhood.
    cv::Mat owner(rows, cols, CV_32SC1, cv::Scalar(-1));
    cv::Mat distance(rows, cols, CV_32SC1, cv::Scalar(std::numeric_limits<int>::max()));
    cv::Mat parent(rows, cols, CV_32SC2, cv::Scalar(-1, -1));
    std::deque<cv::Point> queue;
    const int radius = std::max(1, tolerance);
    const int radiusSquared = radius * radius;
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const cv::Point &vertex = vertices[index];
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                const int x = vertex.x + dx;
                const int y = vertex.y + dy;
                const int d2 = dx * dx + dy * dy;
                if (d2 > radiusSquared || x < 0 || y < 0 || x >= cols || y >= rows
                    || skeletonImage.at<uchar>(y, x) == 0 || d2 >= distance.at<int>(y, x))
                    continue;
                owner.at<int>(y, x) = static_cast<int>(index);
                distance.at<int>(y, x) = 0;
            }
        }
    }
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
            if (owner.at<int>(y, x) >= 0)
                queue.emplace_back(x, y);

    struct Candidate {
        int length = std::numeric_limits<int>::max();
        cv::Point firstMeeting;
        cv::Point secondMeeting;
    };
    std::map<std::pair<int, int>, Candidate> candidates;
    const int offsets[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                               {0, 1},  {1, -1},  {1, 0},  {1, 1}};
    while (!queue.empty()) {
        const cv::Point current = queue.front();
        queue.pop_front();
        const int currentOwner = owner.at<int>(current);
        for (const auto &offset : offsets) {
            const cv::Point next(current.x + offset[1], current.y + offset[0]);
            if (next.x < 0 || next.y < 0 || next.x >= cols || next.y >= rows
                || skeletonImage.at<uchar>(next) == 0)
                continue;
            int &nextOwner = owner.at<int>(next);
            if (nextOwner < 0) {
                nextOwner = currentOwner;
                distance.at<int>(next) = distance.at<int>(current) + 1;
                parent.at<cv::Vec2i>(next) = cv::Vec2i(current.x, current.y);
                queue.push_back(next);
                continue;
            }
            if (nextOwner == currentOwner)
                continue;

            const auto key = std::minmax(currentOwner, nextOwner);
            const int length = distance.at<int>(current) + distance.at<int>(next) + 1;
            Candidate &candidate = candidates[key];
            if (length < candidate.length) {
                candidate.length = length;
                if (currentOwner == key.first) {
                    candidate.firstMeeting = current;
                    candidate.secondMeeting = next;
                } else {
                    candidate.firstMeeting = next;
                    candidate.secondMeeting = current;
                }
            }
        }
    }

    auto pathToSource = [&parent](cv::Point point) {
        std::vector<cv::Point> path;
        while (point.x >= 0 && point.y >= 0) {
            path.push_back(point);
            const cv::Vec2i previous = parent.at<cv::Vec2i>(point);
            point = cv::Point(previous[0], previous[1]);
        }
        return path;
    };

    std::vector<LineConnection> connections;
    connections.reserve(candidates.size());
    for (const auto &entry : candidates) {
        const int first = entry.first.first;
        const int second = entry.first.second;
        std::vector<cv::Point> firstHalf = pathToSource(entry.second.firstMeeting);
        std::reverse(firstHalf.begin(), firstHalf.end());
        std::vector<cv::Point> secondHalf = pathToSource(entry.second.secondMeeting);

        std::vector<cv::Point> path;
        path.reserve(firstHalf.size() + secondHalf.size() + 2);
        path.push_back(vertices[first]);
        path.insert(path.end(), firstHalf.begin(), firstHalf.end());
        path.insert(path.end(), secondHalf.begin(), secondHalf.end());
        path.push_back(vertices[second]);
        connections.push_back({first, second, std::move(path)});
    }
    return connections;
}

double normalizedAngle(const cv::Point2d &origin, const cv::Point2d &dest) {
    double angle = std::atan2(dest.y - origin.y, dest.x - origin.x);
    if (angle < 0)
        angle += 2.0 * CV_PI;
    return angle;
}

double signedArea(const std::vector<int> &cycle, const std::unordered_map<int, cv::Point2d> &positions) {
    if (cycle.size() < 3)
        return 0.0;

    double area = 0.0;
    for (std::size_t i = 0; i < cycle.size(); ++i) {
        const int idA = cycle[i];
        const int idB = cycle[(i + 1) % cycle.size()];

        const auto itA = positions.find(idA);
        const auto itB = positions.find(idB);
        if (itA == positions.end() || itB == positions.end())
            return 0.0;

        const cv::Point2d &a = itA->second;
        const cv::Point2d &b = itB->second;
        area += a.x * b.y - a.y * b.x;
    }

    return 0.5 * area;
}

std::vector<std::vector<int>> detectPolygonsCCW(const std::vector<VertexGeometry> &vertices,
                                                const std::vector<GraphEdge> &edges) {
    if (vertices.size() < 3 || edges.size() < 3)
        return {};

    std::unordered_map<int, cv::Point2d> positions;
    positions.reserve(vertices.size());
    for (const auto &v : vertices)
        positions.emplace(v.id, v.position);

    struct HalfEdge {
        int origin = -1;
        int dest = -1;
        int twin = -1;
        int next = -1;
    };

    std::vector<HalfEdge> halfEdges;
    halfEdges.reserve(edges.size() * 2);

    for (const auto &edge : edges) {
        if (edge.startId == edge.endId)
            continue;

        if (!positions.count(edge.startId) || !positions.count(edge.endId))
            continue;

        const int id1 = static_cast<int>(halfEdges.size());
        halfEdges.push_back({edge.startId, edge.endId, -1, -1});
        const int id2 = static_cast<int>(halfEdges.size());
        halfEdges.push_back({edge.endId, edge.startId, -1, -1});

        halfEdges[id1].twin = id2;
        halfEdges[id2].twin = id1;
    }

    if (halfEdges.size() < 6)
        return {};

    struct OutgoingEdge {
        int halfEdgeId = -1;
        double angle = 0.0;
    };

    std::unordered_map<int, std::vector<OutgoingEdge>> vertexStars;
    vertexStars.reserve(positions.size());

    for (int hid = 0; hid < static_cast<int>(halfEdges.size()); ++hid) {
        const HalfEdge &edge = halfEdges[hid];
        const auto originIt = positions.find(edge.origin);
        const auto destIt = positions.find(edge.dest);
        if (originIt == positions.end() || destIt == positions.end())
            continue;

        vertexStars[edge.origin].push_back({hid, normalizedAngle(originIt->second, destIt->second)});
    }

    for (auto &entry : vertexStars) {
        auto &outgoing = entry.second;
        std::sort(outgoing.begin(), outgoing.end(), [](const OutgoingEdge &lhs, const OutgoingEdge &rhs) {
            if (lhs.angle == rhs.angle)
                return lhs.halfEdgeId < rhs.halfEdgeId;
            return lhs.angle < rhs.angle;
        });
    }

    for (int hid = 0; hid < static_cast<int>(halfEdges.size()); ++hid) {
        HalfEdge &edge = halfEdges[hid];
        if (edge.twin < 0)
            continue;

        const auto starIt = vertexStars.find(edge.dest);
        if (starIt == vertexStars.end())
            continue;

        const auto &outgoing = starIt->second;
        const auto it = std::find_if(outgoing.begin(), outgoing.end(), [twinId = edge.twin](const OutgoingEdge &out) {
            return out.halfEdgeId == twinId;
        });
        if (it == outgoing.end())
            continue;

        const int index = static_cast<int>(std::distance(outgoing.begin(), it));
        const int prevIndex = (index - 1 + static_cast<int>(outgoing.size())) % static_cast<int>(outgoing.size());
        edge.next = outgoing[prevIndex].halfEdgeId;
    }

    std::vector<char> visited(halfEdges.size(), 0);
    std::vector<std::vector<int>> detectedPolygons;
    std::unordered_set<std::string> seenKeys;

    for (int startId = 0; startId < static_cast<int>(halfEdges.size()); ++startId) {
        if (visited[startId])
            continue;

        std::vector<int> traversal;
        traversal.reserve(8);
        int current = startId;
        bool closed = false;

        while (!visited[current]) {
            visited[current] = 1;
            traversal.push_back(current);

            const HalfEdge &edge = halfEdges[current];
            if (edge.next < 0) {
                current = -1;
                break;
            }

            current = edge.next;
            if (current == startId) {
                closed = true;
                break;
            }

            if (current < 0 || current >= static_cast<int>(halfEdges.size())) {
                current = -1;
                break;
            }
        }

        if (!closed)
            continue;

        std::vector<int> cycle;
        cycle.reserve(traversal.size());
        bool validCycle = true;
        for (int hid : traversal) {
            const HalfEdge &edge = halfEdges[hid];
            if (!positions.count(edge.origin)) {
                validCycle = false;
                break;
            }
            cycle.push_back(edge.origin);
        }

        if (!validCycle || cycle.size() < 3)
            continue;

        const double area = signedArea(cycle, positions);
        if (area <= 1e-6)
            continue;

        const std::string key = polygonKey(cycle);
        if (key.empty() || seenKeys.count(key))
            continue;

        seenKeys.insert(key);
        detectedPolygons.push_back(std::move(cycle));
    }

    const auto polygonMinY = [&positions](const std::vector<int> &polygon) {
        double minY = std::numeric_limits<double>::max();
        double minX = std::numeric_limits<double>::max();

        for (int vid : polygon) {
            const auto posIt = positions.find(vid);
            if (posIt == positions.end())
                continue;

            const cv::Point2d &pt = posIt->second;
            if (pt.y < minY || (pt.y == minY && pt.x < minX)) {
                minY = pt.y;
                minX = pt.x;
            }
        }

        return std::make_pair(minY, minX);
    };

    std::sort(detectedPolygons.begin(), detectedPolygons.end(), [&](const auto &lhs, const auto &rhs) {
        const auto lhsMin = polygonMinY(lhs);
        const auto rhsMin = polygonMinY(rhs);

        if (lhsMin.first == rhsMin.first)
            return lhsMin.second < rhsMin.second;
        return lhsMin.first < rhsMin.first;
    });

    return detectedPolygons;
}

std::vector<int> canonicalizePolygon(const std::vector<int> &ids) {
    if (ids.empty())
        return {};

    int minVal = ids.front();
    for (int id : ids)
        minVal = std::min(minVal, id);

    std::vector<int> best;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] != minVal)
            continue;

        std::vector<int> rotated;
        rotated.reserve(ids.size());
        for (std::size_t j = 0; j < ids.size(); ++j)
            rotated.push_back(ids[(i + j) % ids.size()]);

        if (best.empty() || rotated < best)
            best = std::move(rotated);
    }

    return best;
}

std::string polygonKey(const std::vector<int> &ids) {
    const std::vector<int> canonical = canonicalizePolygon(ids);
    if (canonical.empty())
        return {};

    std::string key;
    for (int id : canonical) {
        key += std::to_string(id);
        key.push_back(':');
    }

    return key;
}


} // namespace ImageAnalysis
