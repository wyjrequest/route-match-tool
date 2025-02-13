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
    // std::cout << "tie the road nearby, point size:" << coordlist.size() << std::endl;

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
    }

    // std::cout << "bidirectional dijkstras..." << std::endl;
    BidirectionalDijkstras bd(dm, coordlist, route_nearest_info);
    bd.findPath(result);

    return coordlist.size();
}
