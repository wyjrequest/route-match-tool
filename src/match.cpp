#include "match.h"

#include <iostream>
#include <fstream>

bool PointToLineDistance(const std::vector<Point2D> & line,
                         const Point2D &p,
                         NearestInfo & nearest_info){
    int32_t lx = p.x -200, rx = p.x + 200, ty = p.y + 200, by = p.y -200;
    int32_t minx, miny, maxx, maxy;
    Point2D pp;
    double  d;

    for(uint32_t index = 1; index < line.size(); ++index){
        minx = std::min(line[index -1].x, line[index].x);
        miny = std::min(line[index -1].y, line[index].y);
        maxx = std::max(line[index -1].x, line[index].x);
        maxy = std::max(line[index -1].y, line[index].y);
        if((maxx < lx) || (minx > rx) || (maxy < by) || (miny > ty)){
            continue;
        }
        pp = projection_point(line[index -1], line[index], p);
        d = squaredDistance(pp, p);
        if((nearest_info.distance > d)){
            nearest_info.distance = d;
            nearest_info.project_point_segment = index;
            nearest_info.project_point = std::move(pp);
        }
    }
    return nearest_info.project_point_segment != std::numeric_limits<short>::max();
}

#define INCLUDE_ANGLE(obj_a, angle_a, angle_b) ((obj_a = abs(angle_a - angle_b)) > 180.0 ? (360.0 - obj_a) : obj_a)

MatchService::MatchService(const std::string & data_path){initialization(data_path);}

void MatchService::initialization(const std::string & data_path){
    dm = std::make_shared<DataManger>(data_path);
}

uint32_t MatchService::RunQuery(const std::vector<Point2D> & coordlist, MatchPathResult &result){

    std::cout << "tie the road nearby, point size:" << coordlist.size() << std::endl;

    size_t size = coordlist.size();
    std::vector<std::vector<NearestInfo > > route_nearest_info;
    route_nearest_info.resize(size);

    for(size_t index = 0; index < size; ++index){
        const Point2D &p = coordlist.at(index);
        std::vector<std::pair<RBox, std::shared_ptr<SegmentInfo>>> results;
        dm->GetSegmentInfoByCoord(p.x, p.y, results);

        const size_t & size = results.size();
        std::vector<NearestInfo> nearest_infos;
        nearest_infos.reserve(size);
        const double brearing = size < 2 ? 0.0 :
                    ((index < (size - 1)) ?
                         CalculateAngle(coordlist.at(index), coordlist.at(index + 1)) :
                         CalculateAngle(coordlist.at(index - 1), coordlist.at(index)));

        auto result_begin = results.begin();
        auto result_end   = results.end();
        for(; result_begin != result_end; ++result_begin){
            const auto & rway = (*result_begin);
            if(std::find_if(results.begin(), result_begin,
                            [rway](const auto & v){return rway.second == v.second;}) != result_begin) {
                continue;
            }
            NearestInfo ni;
            if(PointToLineDistance(rway.second->positions, p, ni)){
                ni.way_info = rway.second;

                ni.angle = CalculateAngle(rway.second->positions[ni.project_point_segment - 1], rway.second->positions[ni.project_point_segment]);
                ni.angle = INCLUDE_ANGLE(ni.angle, ni.angle, brearing);

                if((ni.way_info->oneway_type == Options_no) && (ni.angle > 90)){
                    ni.is_reverse = true;
                    ni.angle = 180.0 - ni.angle;
                }
                nearest_infos.emplace_back(std::move(ni));
            }
        }

        std::sort(nearest_infos.begin(), nearest_infos.end(), [&](const NearestInfo & a, const NearestInfo &b){
            return ((a.distance + a.angle * 4.0) < (b.distance + b.angle * 4.0));
        });

        route_nearest_info[index] = nearest_infos;

        // if(nearest_infos.empty()){
        //     continue;
        // }
        //
        // const auto & ni = nearest_infos.front();
        // if(ni.way_info->highway_type == Options_Highway::Options_Highway_motorway_link){
        //     if(start_ramp == -1){
        //         start_ramp = index;
        //     }
        //     end_ramp = index;
        // } else if(start_ramp != -1){
        //     ramp_list.emplace_back(start_ramp, end_ramp);
        //     start_ramp = -1;
        //     end_ramp = -1;
        // }
        //
        // if(ni.way_info->slope_type != Options_Slope::Options_plain){
        //     if(slop_type == ni.way_info->slope_type){
        //         end_ramp = index;
        //     }else{
        //         if(slop_type != Options_Slope::Options_plain){
        //             slop_list.emplace_back(start_slop, end_slop, slop_type);
        //         }
        //
        //         start_slop = index;
        //         end_slop = index;
        //         slop_type = ni.way_info->slope_type;
        //     }
        // } else if(slop_type != Options_Slope::Options_plain){
        //     slop_list.emplace_back(start_slop, end_slop, slop_type);
        //     start_slop = -1;
        //     end_slop = -1;
        //     slop_type = Options_Slope::Options_plain;
        // }
    }

    std::cout << "bidirectional dijkstras..." << std::endl;
    BidirectionalDijkstras bd(dm, coordlist, route_nearest_info);
    bd.findPath(result);

    return coordlist.size();
}

// #define PUSH_GEOMETRY_TO_JSON(coords, pt) { util::json::Array array; array.values.resize(2); array.values[0] = pt.toFloatLon(); array.values[1] = pt.toFloatLat(); coords.values.emplace_back(array); }
// #define PUSH_SLOPE_INFO  slope_info.values["slopeLength"] = length;                 \
//                  slope_info.values["startIndex"] = start_index;                     \
//                  slope_info.values["endIndex"] = end_index;                         \
//                  slope_info.values["slopeType"] = static_cast<uint8_t>(slope_type); \
//                  slope_infos.values.push_back(std::move(slope_info));
//
// #define CALCUL_WAY_BEARING(isreverse, positions) (positions.size() < 2 ? 0 : (isreverse ? CalculateAngle(*positions.rbegin(), *(positions.rbegin() - 1)) : CalculateAngle(*positions.begin(), *(positions.begin() + 1))))
//
// void NearestServiceV2::MakeResponse(const NearestInfo &ni, util::json::Object &waypoint, const engine::api::NearestParameters & parameter)
// {
//     util::json::Array array;
//     array.values.push_back(ni.project_point.toFloatLon());
//     array.values.push_back(ni.project_point.toFloatLat());
//     waypoint.values["location"] = array;
//
//     waypoint.values["name"] = ni.way_info->name;
//     waypoint.values["hint"] = std::move("");
//     waypoint.values["distance"] = std::sqrt(ni.distance);
//
//     util::json::Array nodes;
//     waypoint.values["nodes"] = nodes;
//
//     if(parameter.is_slope_warning){
//
//         std::shared_ptr<SegmentInfo> temp = ni.way_info;
//         bool is_reverse = ni.is_reverse;
//
//         std::vector<Point2D> way_positions = ni.way_info->positions;
//         Options_Slope slope_type = ni.way_info->slope_type;
//
//         if(is_reverse){
//             if(slope_type != Options_plain){
//                 slope_type = (Options_Slope)(static_cast<uint8_t>(slope_type) ^3);
//             }
//             std::reverse(way_positions.begin(), way_positions.end());
//         }
//
//         util::json::Array coordinates;
//         PUSH_GEOMETRY_TO_JSON(coordinates, ni.project_point);
//
//         double length = 0.0;
//         uint32_t start_index = 0;
//         uint32_t end_index = 0;
//
//         if(way_positions.size()){
//             for(uint32_t n = ni.project_point_segment; n < way_positions.size(); ++n){
//                 if(static_cast<short>(n) == ni.project_point_segment){
//                     length = haversine_distance(ni.project_point, way_positions[n]);
//                 }
//                 else{
//                     length = haversine_distance(way_positions[n - 1], way_positions[n]);
//                 }
//             }
//             PUSH_GEOMETRY_TO_JSON(coordinates, way_positions.back());
//             ++end_index;
//         }
//
//         double maxlen = parameter.slope_warning_range + 1000.0 - length;
//         util::json::Array slope_infos;
//         util::json::Object slope_info;
//
//         double max_warning_len = (slope_type == Options_plain) ? parameter.slope_warning_range : 100;
//
//         std::list<std::shared_ptr<SegmentInfo>> topo_list;
//
//         while(temp && ((slope_type != Options_plain) || (length < max_warning_len)) && (maxlen > 0.0)){
//             const auto & backward_ids = (is_reverse ? temp->reverse_backward_ids : temp->backward_ids);
//             if(backward_ids.size() < ((temp->oneway_type == Options_no) ? 2 : 1)){
//                 break;
//             }
//
//             topo_list.emplace_back(temp);
//
//             // search backward way
//             uint32_t cur_id = temp->id;
//             double cur_way_bearing = CALCUL_WAY_BEARING(is_reverse, temp->positions);
//             double min_angle = 180.0;
//
//             for(const auto item : backward_ids){
//                 if(cur_id != (item >> 1)){
//                     const auto & way_t = dm->GetSegmentInfoById(item >> 1);
//                     if(way_t){
//                         bool is_reverse_t= (item) & 1;
//                         double angle_t = CALCUL_WAY_BEARING(is_reverse_t, way_t->positions);
//                         angle_t = INCLUDE_ANGLE(angle_t, angle_t, cur_way_bearing);
//                         if(angle_t < min_angle){
//                             temp = way_t;
//                             min_angle = angle_t;
//                             is_reverse = (item) & 1;
//                         }
//                     }
//                 }
//             }
//
//             if((temp == nullptr) || std::find(topo_list.begin(), topo_list.end(), temp) != topo_list.end()){
//                 break;
//             }
//
//             auto temp_slop_type = temp->slope_type;
//             if(is_reverse){
//                 if(temp_slop_type != Options_plain){
//                     temp_slop_type = (Options_Slope)(static_cast<uint8_t>(temp_slop_type) ^3);
//                 }
//             }
//
//             if(slope_type != temp_slop_type){
//                 PUSH_SLOPE_INFO;
//                 slope_info = util::json::Object();
//                 length = 0.0;
//                 start_index = end_index;
//                 slope_type = temp->slope_type;
//                 max_warning_len = 100;
//             }
//
//             if(temp->positions.size()){
//                 length += temp->length;
//                 maxlen -= temp->length;
//                 PUSH_GEOMETRY_TO_JSON(coordinates, (is_reverse ? temp->positions.front() : temp->positions.back()));
//                 ++end_index;
//             }
//         }
//
//         PUSH_SLOPE_INFO;
//         waypoint.values["slopeInfo"] = std::move(slope_infos);
//
//         util::json::Object geometry;
//         geometry.values["coordinates"]=std::move(coordinates);
//         geometry.values["type"] = "LineString";
//         waypoint.values["geometry"] = std::move(geometry);
//     }
// }
