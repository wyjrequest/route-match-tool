#ifndef CACHE_GRID_H
#define CACHE_GRID_H

#include <vector>
#include "common.h"

#include <memory>

constexpr int32_t EAST = 140000000;  // 东
constexpr int32_t SOUTH = 0;         // 南
constexpr int32_t WEST = 70000000;   // 西
constexpr int32_t NORTH = 60000000;  // 北
constexpr int32_t GRID_RANGE = 80000;

template<typename T>
class CacheGrid{
public:
    CacheGrid(int32_t minx_ = WEST,
              int32_t miny_ = SOUTH,
              int32_t maxx_ = EAST,
              int32_t maxy_ = NORTH,
              int32_t grid_range_ = GRID_RANGE)
        : minx(minx_),
          miny(miny_),
          maxx(maxx_),
          maxy(maxy_),
          grid_range(grid_range_) {

        col_number = ((maxx - minx) / grid_range + 1);
        row_number = ((maxy - miny) / grid_range + 1);
        cachelist.resize(col_number * row_number);
    }

    std::vector<T*> sreach_by_line(const std::vector<Point2D> & line){
        Point2D ld(180000000, 90000000);
        Point2D rt(0, 0);

        for(auto point : line){
            ld.x = std::min(point.x, ld.x);
            ld.y = std::min(point.y, ld.y);
            rt.x = std::max(point.x, rt.x);
            rt.y = std::max(point.y, rt.y);
        }

        std::vector<T*> result;
        sreach(result, ld, rt);
        return result;
    }

    std::vector<T*> sreach_by_point(const Point2D & pt, const int32_t range = 15){
        std::vector<T*> result;
        sreach(result, {pt.x - range, pt.y - range}, {pt.x + range, pt.y + range});
        return result;
    }

    std::vector<T*> sreach_by_range(const Point2D & ld, const Point2D & rt){
        std::vector<T*> result;
        sreach(result, ld, rt);
        return result;
    }
private:
    void sreach(std::vector<T*> & result, const Point2D & ld, const Point2D & rt){
        const int32_t rs = (ld.x - minx) / grid_range;
        const int32_t re = (rt.x - minx) / grid_range;
        const int32_t cs = (ld.y - miny) / grid_range;
        const int32_t ce = (rt.y - miny) / grid_range;

        for(int32_t r = rs; r <= re; ++r){
            for(int32_t c = cs; c <= ce; ++c){
                int32_t temp_index = col_number * c + r;
                if(cachelist[temp_index] == nullptr){
                    cachelist[temp_index] = std::make_shared<T>();
                }
                result.emplace_back(cachelist[temp_index].get());
            }
        }
    }
private:
    int32_t minx;
    int32_t miny;
    int32_t maxx;
    int32_t maxy;
    int32_t grid_range;

    int32_t row_number;
    int32_t col_number;
    std::vector<std::shared_ptr<T>> cachelist;
};
#endif
