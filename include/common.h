#ifndef COMMON_H
#define COMMON_H

#include <cmath>
#include <string>
#include <vector>

// 地球半径，单位：米
const double EARTH_RADIUS = 6371000.0;
const double EPSILON = 1e-9;

// PI 常量
const double PI = 3.14159265358979323846;
const double X_PI = (PI * 3000.0 / 180.0);

struct Point2D{
    int32_t x;
    int32_t y;
    double toFloatLon() const {return static_cast<double>(x) / 1000000.0;}
    double toFloatLat() const {return static_cast<double>(y) / 1000000.0;}
    Point2D():x(0),y(0){}
    Point2D(int32_t x_, int32_t y_):x(x_),y(y_){}
};

// 计算两点之间的平方距离(m)
double squaredDistance(const Point2D& p1, const Point2D& p2);

// 计算两向量的叉乘
double crossProduct(const Point2D& a, const Point2D& b);

// 计算点到线段的最短距离（使用叉乘）
double pointToSegmentDistance(const Point2D& p, const Point2D& a, const Point2D& b);

// 计算两个点之间的方向角
double CalculateAngle(const Point2D & p1, const Point2D & p2);

// 计算点到线段的投影点
Point2D projection_point(const Point2D A, const Point2D B, const Point2D P);

// 将角度转换为弧度
double degrees_to_radians(double degrees);

double haversine_distance(double lat1, double lon1, double lat2, double lon2);

double haversine_distance(const Point2D & p1, const Point2D &p2);

// BD-09 到 GCJ-02 转换
void bd09_to_gcj02(double bd_lon, double bd_lat, double &gcj_lon, double &gcj_lat);

// GCJ-02 到 BD-09 的转换函数
void gcj02_to_bd09(double gcj_lon, double gcj_lat, double &bd_lon, double &bd_lat);

void splitstring(const std::string& str, const char separate, std::vector<std::string>& result);
#endif
