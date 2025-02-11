#include "common.h"
#include <math.h>

void splitstring(const std::string& str, const char separate, std::vector<std::string>& result) {
    size_t startPos = 0;
    size_t delimiterPos;

    while ((delimiterPos = str.find(separate, startPos)) != std::string::npos) {
        // 提取从 startPos 到 delimiterPos 之间的子字符串
        result.push_back(str.substr(startPos, delimiterPos - startPos));
        startPos = delimiterPos + 1; // 更新 startPos 为下一个分隔符后的位置
    }
    // 添加最后一个部分
    result.push_back(str.substr(startPos));
}

// 计算两点之间的平方距离(m)
double squaredDistance(const Point2D& p1, const Point2D& p2) {
    return  (((p1.x - p2.x) * (p1.x - p2.x)) + ((p1.y - p2.y) * (p1.y - p2.y)))/ 100;
}

// 计算两向量的叉乘
double crossProduct(const Point2D& a, const Point2D& b) {
    return a.x * b.y - a.y * b.x;
}

// 计算点到线段的最短距离（使用叉乘）
double pointToSegmentDistance(const Point2D& p, const Point2D& a, const Point2D& b) {
    // 向量 AB 和 AP
    Point2D ab = {b.x - a.x, b.y - a.y};
    Point2D ap = {p.x - a.x, p.y - a.y};

    // 线段长度的平方
    double abLenSquared = squaredDistance(a, b);

    // 计算投影点的位置参数 t
    double t = (ap.x * ab.x + ap.y * ab.y) / abLenSquared;

    // 判断投影点是否在线段上
    if (t < 0.0) {
        // 最近点是 A
        return std::sqrt(squaredDistance(p, a));
    } else if (t > 1.0) {
        // 最近点是 B
        return std::sqrt(squaredDistance(p, b));
    } else {
        // 最近点在线段上，计算垂直距离
        double cross = crossProduct(ap, ab);
        return std::abs(cross) / std::sqrt(abLenSquared);
    }
}

// 计算两个点之间的方向角
double CalculateAngle(const Point2D & p1, const Point2D & p2) {
    const double dy = p2.y - p1.y;
    const double dx = p2.x - p1.x;

    if(std::fabs(dy) < EPSILON){
        return dx < 0.0 ? 270.0 : 90.0;
    }

    if(std::fabs(dx) < EPSILON){
        return dy > 0? 0.0 : 180.0;
    }
    // 计算方向角，atan 返回的是弧度
    double angle_radians = atan(std::fabs(dy/dx));

    // 将弧度转换为度
    angle_radians = angle_radians * 180.0 * M_1_PI;

    // 返回方向角（以度为单位）
    return (dy > 0.0 ? (dx > 0.0 ? (90.0 - angle_radians) : (270.0 + angle_radians)) : (dx > 0.0 ? (90.0 + angle_radians) : (270.0 - angle_radians)));
}

// 计算点到线段的投影点
Point2D projection_point(const Point2D A, const Point2D B, const Point2D P) {
    // 计算向量 AB 和 AP
    Point2D AB = {B.x - A.x, B.y - A.y};
    Point2D AP = {P.x - A.x, P.y - A.y};

    // 计算 |AB|^2
    double ab_squared = AB.toFloatLon() * AB.toFloatLon() + AB.toFloatLat() * AB.toFloatLat();

    // 如果 AB 长度为 0（退化成点），直接返回 A
    if (ab_squared == 0.0) {
        return A;
    }

    // 计算 t
    double t = (AP.toFloatLon() * AB.toFloatLon() + AP.toFloatLat() * AB.toFloatLat()) / ab_squared;

    // 限制 t 在 [0, 1] 之间
    t = std::max(0.0, std::min(1.0, t));

    // 计算投影点 Q
    return Point2D(A.x + t * AB.x, A.y + t * AB.y);
}

// 将角度转换为弧度
double degrees_to_radians(double degrees) {
    return degrees * M_PI / 180.0;
}

double haversine_distance(double lat1, double lon1, double lat2, double lon2) {
    // 将纬度和经度从度转换为弧度
    lat1 = degrees_to_radians(lat1);
    lon1 = degrees_to_radians(lon1);
    lat2 = degrees_to_radians(lat2);
    lon2 = degrees_to_radians(lon2);

    // 计算纬度和经度的差值
    double delta_lat = lat2 - lat1;
    double delta_lon = lon2 - lon1;

    // 应用 Haversine 公式
    double a = std::sin(delta_lat / 2) * std::sin(delta_lat / 2) +
               std::cos(lat1) * std::cos(lat2) * std::sin(delta_lon / 2) * std::sin(delta_lon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    // 计算距离
    return EARTH_RADIUS * c;
}

double haversine_distance(const Point2D & p1, const Point2D &p2) {
    return haversine_distance(p1.toFloatLat(), p1.toFloatLon(), p2.toFloatLat(), p2.toFloatLon());
}

// BD-09 到 GCJ-02 转换
void bd09_to_gcj02(double bd_lon, double bd_lat, double &gcj_lon, double &gcj_lat) {
    // 偏移量
    double x = bd_lon - 0.0065;
    double y = bd_lat - 0.006;

    // 计算距离
    double z = sqrt(x * x + y * y) - 0.00002 * sin(y * X_PI);
    double theta = atan2(y, x) - 0.000003 * cos(x * X_PI);

    // 转换为 GCJ-02 坐标
    gcj_lon = z * cos(theta);
    gcj_lat = z * sin(theta);
}

// GCJ-02 到 BD-09 的转换函数
void gcj02_to_bd09(double gcj_lon, double gcj_lat, double &bd_lon, double &bd_lat) {
    // const double X_PI = 3.14159265358979324 * 3000.0 / 180.0;

    const double z = sqrt(gcj_lon * gcj_lon + gcj_lat * gcj_lat) + 0.00002 * sin(gcj_lat * X_PI);
    const double theta = atan2(gcj_lat, gcj_lon) + 0.000003 * cos(gcj_lon * X_PI);
    bd_lat = (z * sin(theta) + 0.006);
    bd_lon = (z * cos(theta) + 0.0065);
}
