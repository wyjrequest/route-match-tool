#ifndef DIJKSTRAS_H
#define DIJKSTRAS_H

#include "common.h"
#include "databuild.h"
#include "data_manager.h"

struct NearestInfo{
    bool                     is_reverse;
    short                    project_point_segment;
    double                   angle;
    double                   distance;
    Point2D                  project_point;
    std::shared_ptr<SegmentInfo> way_info;
    NearestInfo() : is_reverse(false),
                    project_point_segment(std::numeric_limits<short>::max()),
                    angle(std::numeric_limits<double>::max()),
                    distance(std::numeric_limits<double>::max()), project_point(), way_info(nullptr) {}
    NearestInfo(bool         is_reverse_,
                short        project_point_segment_,
                double       angle_,
                double       distance_,
                Point2D      project_point_,
                std::shared_ptr<SegmentInfo> way_info_) :
                    is_reverse(is_reverse_),
                    project_point_segment(project_point_segment_),
                    angle(angle_),
                    distance(distance_),
                    project_point(project_point_),
                    way_info(way_info_) {}
};

struct Node {
    uint32_t dsegment_id;
    float f;

    bool is_neart_segment;
    double current_len;
    double total_len;

    std::shared_ptr<Node> parent;

    Node(uint32_t dsegment_id, float f, bool is_neart_segment = true, double current_len = 0.0, double total_len = 0.0, std::shared_ptr<Node> parent = nullptr)
        : dsegment_id(dsegment_id), f(f), is_neart_segment(is_neart_segment), current_len(current_len), total_len(total_len), parent(parent) {}

    bool operator>(const std::shared_ptr<Node>& other) const {
        return f > other->f; // priority queue orders nodes by f
    }
};

typedef std::priority_queue<std::shared_ptr<Node>, std::vector<std::shared_ptr<Node>>, std::greater<std::shared_ptr<Node>>> NodeQueue;

struct TrackInfo{
    Point2D pt;
    double distance_pre;

    std::vector<NearestInfo> nis;
};

struct SegmentNeartCost{
    uint32_t segment_id;
    double factor; // f = (square(d) + (angle * 4)), factor = (f1 + f2 ..fn)/ square(n) / 400.0;
    std::vector<uint32_t> neart_index;
    SegmentNeartCost(): segment_id(0), factor(0.0), neart_index(){}
};

struct SectionInfo{
    int32_t start_index;
    int32_t end_index;
    double len;
    SectionInfo():start_index(-1), end_index(-1), len(0.0){}
    SectionInfo(int32_t start_index, int32_t end_index, double len):start_index(start_index), end_index(end_index), len(len){}
};

inline std::string pointsToWkt(const std::vector<Point2D> & poistions){
    if(poistions.empty()){
        return "LINESTRING(0.0 0.0,0.0 0.0)";
    }

    std::string linestring = "";
    bool is_start = true;
    for(const auto & pt : poistions){

        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%.6f %.6f", pt.toFloatLon(), pt.toFloatLat());

        if(is_start){
            is_start = false;
        } else {
            linestring+=", ";
        }
        linestring += buffer;
    }

    linestring = "LINESTRING("+ linestring+")";
    return linestring;
}

#define SECTION_INFO_TO_STR(si_list, key, str) { \
        str = "\""#key"\":["; \
        bool is_start = true; \
        for(const auto & item : si_list){ \
            if(is_start){ \
                is_start = false; \
            }else{ \
                str+=","; \
            } \
            str+="{\"s\":" + std::to_string(item.start_index) + ", \"e\":" + std::to_string(item.end_index) +", \"l\":" + std::to_string(item.len) +"}"; \
        } \
        str+="]"; \
}

struct MatchPathResult{
    std::vector<uint32_t> dsegment_ids;
    std::vector<Point2D>  poistions;

    std::vector<SectionInfo> ramp_list;
    std::vector<SectionInfo> slop_list;
    std::vector<SectionInfo> dangers_list;

    std::string toWkt(){
        return pointsToWkt(poistions);
    }

    std::string toSectionInfoJson(){
        std::string d_list_str;
        SECTION_INFO_TO_STR(dangers_list, d_list, d_list_str);

        std::string r_list_str;
        SECTION_INFO_TO_STR(ramp_list, r_list, r_list_str);

        std::string s_list_str;
        SECTION_INFO_TO_STR(slop_list, s_list, s_list_str);

        return "{" + d_list_str +", " + r_list_str +", " + s_list_str + "}";
    }
};

class BidirectionalDijkstras {
public:
    BidirectionalDijkstras(std::shared_ptr<DataManger> dm,
                           const std::vector<TrackInfo > & tracklist
                       /* const std::vector<Point2D> & coordlist,
                       const std::vector<std::vector<NearestInfo>> & route_nearest_info */);
    void findPath(MatchPathResult & path);
private:
    void initialization();
    void initForward();
    void initRevered();

    double totalExtendLen(int32_t index, bool is_reverse);
    double currentExtendLen(const NearestInfo & ni, bool is_reverse);

    std::shared_ptr<Node> dsegmentIdToNode(uint32_t dsegment_id, bool is_reverse, std::shared_ptr<Node> parent = nullptr);
    bool expandNode(bool is_start);

    void gendsegmentPoints(const uint32_t dsegment_id, const int32_t start_index, const int32_t end_index, std::vector<Point2D> & poistions);
    void constructPath(MatchPathResult &path);
    void constructPoints(const std::vector<uint32_t> & dsegment_ids, std::vector<Point2D> & poistions);
    void constructSectionInfo(MatchPathResult &path);
private:
    std::shared_ptr<DataManger> datamanager;
    std::vector<TrackInfo > track_list;

    std::map<uint32_t, SegmentNeartCost> pre_segment_cost;

    NodeQueue forward_open_list, reverse_open_list;
    std::map<uint32_t, std::shared_ptr<Node>> forward_close_list, reverse_close_list;

    std::pair<int32_t, std::shared_ptr<Node>> forward_longest_index;
    std::pair<int32_t, std::shared_ptr<Node>> reverse_longest_index;

    std::vector<std::shared_ptr<Node> > forward_result;
    std::vector<std::shared_ptr<Node> > reverse_result;
};
#endif
