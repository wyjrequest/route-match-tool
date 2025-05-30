#ifndef DATABUILD_H
#define DATABUILD_H

#include <unordered_map>
#include <string>
#include <vector>
#include <set>
#include <map>

constexpr size_t INVALID_VALUE = 0xFFFFFFFF;

enum Options_Highway : uint8_t {
    Options_Highway_motorway = 0,
    Options_Highway_motorway_link = 1,
    Options_Highway_trunk = 2,
    Options_Highway_trunk_link = 3,
    Options_Highway_primary = 4,
    Options_Highway_primary_link = 5,
    Options_Highway_secondary = 6,
    Options_Highway_secondary_link = 7,
    Options_Highway_residential = 8,
    Options_Highway_residential_link = 9,
    Options_Highway_service = 10,
    Options_Highway_tertiary = 11,
    Options_Highway_tertiary_link = 12,
    Options_Highway_road = 13,
    Options_Highway_track = 14,
    Options_Highway_unclassified = 15,
    Options_Highway_undefined = 16,
    Options_Highway_unknown = 17,
    Options_Highway_living_street = 18,
    Options_Highway_footway = 18,
    Options_Highway_pedestrian = 20,
    Options_Highway_steps = 21,
    Options_Highway_bridleway = 22,
    Options_Highway_construction = 23,
    Options_Highway_cycleway = 24,
    Options_Highway_path = 25,
    Options_Highway_bus_guideway = 26
};

static const std::unordered_map<std::string, Options_Highway> highway_types{
    {"motorway", Options_Highway_motorway},
    {"motorway_link", Options_Highway_motorway_link},
    {"trunk", Options_Highway_trunk},
    {"trunk_link", Options_Highway_trunk_link},
    {"primary", Options_Highway_primary},
    {"primary_link", Options_Highway_primary_link},
    {"secondary", Options_Highway_secondary},
    {"secondary_link", Options_Highway_secondary_link},
    {"residential", Options_Highway_residential},
    {"residential_link", Options_Highway_residential_link},
    {"service", Options_Highway_service},
    {"tertiary", Options_Highway_tertiary},
    {"tertiary_link", Options_Highway_tertiary_link},
    {"road", Options_Highway_road},
    {"track", Options_Highway_track},
    {"unclassified", Options_Highway_unclassified},
    {"undefined", Options_Highway_undefined},
    {"unknown", Options_Highway_unknown},
    {"living_street", Options_Highway_living_street},
    {"footway", Options_Highway_footway},
    {"pedestrian", Options_Highway_pedestrian},
    {"steps", Options_Highway_steps},
    {"bridleway", Options_Highway_bridleway},
    {"construction", Options_Highway_construction},
    {"cycleway", Options_Highway_cycleway},
    {"path", Options_Highway_path},
    {"bus_guideway", Options_Highway_bus_guideway}
};

enum Options_Oneway : uint8_t {
    Options_no = 0,
    Options_yes = 1,
    Options_reverse = 2
};

static const std::unordered_map<std::string, Options_Oneway> oneway_types{
    {"no", Options_no},
    {"yes", Options_yes},
    {"-1", Options_reverse}
};

static const std::unordered_map<Options_Oneway, std::string> oneway_type_strs{
    {Options_no, "no"},
    {Options_yes, "yes"},
    {Options_reverse, "-1"}
};

enum Options_Slope : uint8_t {
    Options_plain = 0,
    Options_upslope = 1,
    Options_downhill = 2
};

static const std::unordered_map<std::string, Options_Slope> slope_types{
    {"0", Options_plain},
    {"1", Options_upslope},
    {"-1", Options_downhill}
};

static const std::unordered_map<Options_Slope, std::string> slope_type_strs{
    {Options_plain, "0"},
    {Options_upslope, "1"},
    {Options_downhill, "-1"}
};

struct PointInfo{
    uint32_t id;
    double   lon;
    double   lat;
    uint64_t pointHash;
    PointInfo(const uint32_t id_, const double lon_, const double lat_, const uint64_t pointHash_): id(id_), lon(lon_), lat(lat_), pointHash(pointHash_){}
};

struct RelWayId{
    // std::shared_ptr<PointInfo> point_info;
    uint32_t id;
    std::set<uint32_t> in_way_id;
    std::set<uint32_t> out_way_id;
    RelWayId(){}
    // RelWayId() : point_info(nullptr){}
    // RelWayId(const std::shared_ptr<PointInfo> & point_info_): point_info(point_info_){}
};

static const std::unordered_map<std::string, Options_Highway> road_class_to_highway_types{
    {"高速公路", Options_Highway_motorway},
    {"主要大街，城市快速路", Options_Highway_trunk},
    {"国道", Options_Highway_trunk},
    {"省道", Options_Highway_primary},
    {"县道", Options_Highway_secondary},
    {"九级道路", Options_Highway_service},
    {"乡镇村道", Options_Highway_tertiary},
    {"其它道路", Options_Highway_unclassified},
    {"轮渡", Options_Highway_undefined}
};

struct WayInfo{
    uint32_t        way_id;
    int32_t         width;
    bool            is_intersection;
    Options_Highway highway_type;
    Options_Oneway  oneway_type;
    Options_Slope   slope_type;

    std::vector<uint64_t> rel_node_id;
    std::vector<std::pair<std::string, std::string>> k_v;

    WayInfo() : way_id(0), width(0), is_intersection(false),
        highway_type(Options_Highway_undefined),
        oneway_type(Options_Oneway::Options_no),
        slope_type(Options_Slope::Options_plain),
        rel_node_id(),
        k_v(){}

    void set_oneway_by_dir(const char * dir){

        const uint8_t val = atoi(dir) - 1;
        if(0 <= val && val <= 2){
            oneway_type = Options_Oneway(val);
        }
        else{
            oneway_type = Options_Oneway::Options_no;
        }
    }

    void set_highwaytype_by_roadclass_and_formway(const std::string & roadclass, const std::string & formway){

        auto type_it = road_class_to_highway_types.find(roadclass.c_str());
        highway_type = (type_it != road_class_to_highway_types.cend()) ? type_it->second : Options_Highway_undefined;

        if(formway.find("IC") != -1){
            highway_type = Options_Highway_motorway_link;
        }
        else if(formway.find("JCT") != -1){
            highway_type = Options_Highway_motorway_link;
        }
        else if(formway.find("匝道") != -1){
            highway_type = (Options_Highway)(highway_type + 1);
        }
    }
};

#define CALCULATE_POINT_HASH(lon, lat) ((static_cast<uint64_t>(lon * 5965232.35) << 32) + static_cast<uint64_t>(lat * 5965232.35))
#define POINT_HAST_TO_LON(hash) (static_cast<double>(hash >> 32) / 5965232.35)
#define POINT_HAST_TO_LAT(hash) (static_cast<double>(hash & 0xFFFFFFFF) / 5965232.35)

static const std::unordered_map<std::string, std::string> kind_strs{
    {"None", "unclassified"},
    {"0", "unclassified"},
    {"1", "motorway"},
    {"2", "trunk"},
    {"3", "trunk"},
    {"4", "primary"},
    {"5", "unclassified"},
    {"6", "secondary"},
    {"7", "tertiary"},
    {"8", "unclassified"},
    {"9", "service"},
    {"10", "footway"},
    {"11", "footway"},
    {"13", "unclassified"}
};

int data_processing(int argc, const char *argv[]);
#endif
