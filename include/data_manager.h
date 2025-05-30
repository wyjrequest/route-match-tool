#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "common.h"
#include "databuild.h"
#include "cache_grid.h"

#include <iostream>
#include <queue>
#include <cmath>
#include <unordered_map>
#include <set>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <boost/geometry.hpp>
#include <boost/geometry/index/rtree.hpp>

// 使用 Boost.Geometry 命名空间
namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

typedef bg::model::point<uint32_t, 2, bg::cs::cartesian> RPoint;
typedef bg::model::box<RPoint> RBox;

// constexpr uint32_t EAST = 140000000;  // 东
// constexpr uint32_t SOUTH = 0;         // 南
// constexpr uint32_t WEST = 70000000;   // 西
// constexpr uint32_t NORTH = 60000000;  // 北
// constexpr uint32_t GRID_RANGE = 80000;

constexpr uint32_t C0LUMNS_NUMBER = ((EAST - WEST)/GRID_RANGE + 1);
constexpr uint32_t MAX_WARNING_RANGE = 3000.0;

typedef uint32_t SegmentId ;
typedef uint32_t DSegmentId;

#define DSegmentIdToSegmentId(id) (id >> 1)
#define IsReverseOfDSegmentId(id) (id & 0x1)

#define SegmentIdToDSegmentId(id, is_reverse) ((id << 1) + is_reverse)

struct AccidentSection{
    int32_t s_seg_index;
    int32_t e_seg_index;
};

struct SegmentInfo{
    bool                    is_standard_trajectory;
    bool                    has_accident;
    bool                    is_construction;
    Options_Highway         highway_type;
    Options_Oneway          oneway_type;
    Options_Slope           slope_type;
    SegmentId               id;

    double                  max_turn_angle;
    double                  length;
    std::string             name;

    AccidentSection         accident_section;
    std::vector<DSegmentId> forward_ids;
    std::vector<DSegmentId> backward_ids;
    std::vector<DSegmentId> reverse_backward_ids;
    std::vector<DSegmentId> reverse_forward_ids;
    std::vector<Point2D>    positions;
};

class DataManger{
public:
    DataManger(const std::string &path);
    void GetSegmentInfoByCoord(const int32_t x, const int32_t y,
                               std::vector<std::pair<RBox, std::shared_ptr<SegmentInfo>>> & results);
    std::shared_ptr<SegmentInfo> GetSegmentInfoById(uint32_t id){
        return waylist[id];
    }

    void GetForwardOfDSegmnetId(const DSegmentId dsegid, std::vector<DSegmentId> & forward_ids){
        std::shared_ptr<SegmentInfo> si = GetSegmentInfoById(DSegmentIdToSegmentId(dsegid));
        if(si){
            forward_ids = IsReverseOfDSegmentId(dsegid) ? si->reverse_forward_ids : si->forward_ids;
        }
    }

    void GetBackwardOfDSegmnetId(const DSegmentId dsegid, std::vector<DSegmentId> & backward_ids){
        std::shared_ptr<SegmentInfo> si = GetSegmentInfoById(DSegmentIdToSegmentId(dsegid));
        if(si){
            backward_ids = IsReverseOfDSegmentId(dsegid) ? si->reverse_backward_ids : si->backward_ids;
        }
    }

private:
    std::vector<std::shared_ptr<SegmentInfo> > waylist;
    std::vector<bgi::rtree<std::pair<RBox, std::shared_ptr<SegmentInfo>>, bgi::quadratic<16>> > cachelist;
};
#endif
