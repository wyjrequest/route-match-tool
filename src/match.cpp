#include "match.h"

#include <iostream>
#include <fstream>

bool PointToLineDistance(const std::vector<Point2D> & line,
                         const Point2D &p,
                         NearestInfo & nearest_info){
    int32_t lx = p.x -100, rx = p.x + 100, ty = p.y + 100, by = p.y -100;
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
    return nearest_info.project_point_segment != std::numeric_limits<short>::max() && nearest_info.distance < 100;
}

#define INCLUDE_ANGLE(obj_a, angle_a, angle_b) ((obj_a = abs(angle_a - angle_b)) > 180.0 ? (360.0 - obj_a) : obj_a)


MatchService::MatchService(std::shared_ptr<DataManger> datamanager):dm(datamanager){}
MatchService::MatchService(const std::string & data_path){initialization(data_path);}

void MatchService::initialization(const std::string & data_path){
    dm = std::make_shared<DataManger>(data_path);
    // dm_slope = std::make_shared<DataManger>("zto_nav_v3_gc_motorway.bin");
}

uint32_t MatchService::RunQuery(const std::vector<Point2D> & coordlist, std::vector<TrackInfo > & track_list, double max_angle_diff){
    // std::cout << "tie the road nearby, point size:" << coordlist.size() << std::endl;

    size_t size = coordlist.size();
    track_list.resize(coordlist.size());

    for(size_t index = 0; index < size; ++index){
        track_list[index].pt = coordlist[index];
        track_list[index].distance_pre =  (index == 0 ? 0.0 : haversine_distance(coordlist.at(index - 1), coordlist.at(index)));
    }

    // std::vector<std::vector<NearestInfo > > route_nearest_info;
    // route_nearest_info.resize(size);

    for(size_t index = 0; index < size; ++index){
        if(index == 3079){
            index = index;
        }

        const Point2D &p = coordlist.at(index);
        std::vector<std::pair<RBox, std::shared_ptr<SegmentInfo>>> results;
        dm->GetSegmentInfoByCoord(p.x, p.y, results);

        // const size_t & size = results.size();
        std::vector<NearestInfo> & nearest_infos = track_list[index].nis;
        nearest_infos.reserve(size);

        double brearing = 0.0;
        if(index == (size - 1)){
            int32_t s = index;
            for(double len = 0.0; (len < 10.0 && (s > 0)); --s){
                len += track_list[s].distance_pre;
            }
            brearing = CalculateAngle(coordlist.at(s), coordlist.at(index));
        }else {
            int32_t e = index;
            for(double len = 0.0; len < 10.0 && e < (size - 1); ++e){
                len += track_list[e + 1].distance_pre;
            }
            brearing = CalculateAngle(coordlist.at(index), coordlist.at(e));
        }

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

                if(ni.angle < max_angle_diff){
                    nearest_infos.emplace_back(std::move(ni));
                }
            }
        }

        std::sort(nearest_infos.begin(), nearest_infos.end(), [&](const NearestInfo & a, const NearestInfo &b){
            return ((a.distance + a.angle * 4.0) < (b.distance + b.angle * 4.0));
        });

        // route_nearest_info[index] = nearest_infos;
    }

    return coordlist.size();
}

#define CALCUL_WAY_BEARING(isreverse, positions)                                      \
    (positions.size() < 2 ? 0 :                                                       \
        (isreverse ? CalculateAngle(*positions.rbegin(), *(positions.rbegin() - 1)) : \
            CalculateAngle(*positions.begin(), *(positions.begin() + 1))))

void MatchService::setAccidentInfo(const AccidentInfo & ai, OutGeomJson * outgeojson){
    const Point2D &p = ai.pt;
    std::vector<std::pair<RBox, std::shared_ptr<SegmentInfo>>> results;
    dm->GetSegmentInfoByCoord(p.x, p.y, results);

    std::vector<NearestInfo> nearest_infos;

    double brearing = ai.bearing;

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

            if(ni.angle < 20){
                nearest_infos.emplace_back(std::move(ni));
            }
        }
    }

    std::sort(nearest_infos.begin(), nearest_infos.end(), [&](const NearestInfo & a, const NearestInfo &b){
        return ((a.distance + a.angle * 4.0) < (b.distance + b.angle * 4.0));
    });

    if(nearest_infos.empty()){
        return;
    }

    const NearestInfo & ni = nearest_infos.front();

    double offset = (ni.way_info->highway_type == Options_Highway_motorway ||
                     ni.way_info->highway_type == Options_Highway_motorway_link ||
                     ni.way_info->highway_type == Options_Highway_trunk ||
                     ni.way_info->highway_type == Options_Highway_trunk_link)
            ? 500.0 : 200.0;

    if(ni.way_info->has_accident == false){
        ni.way_info->has_accident = true;
    }

    if(ni.is_reverse){
        int32_t e_seg_index = ni.project_point_segment;
        if(e_seg_index > ni.way_info->accident_section.e_seg_index){
            ni.way_info->accident_section.e_seg_index = e_seg_index;
        }

        int32_t s_seg_index = ni.project_point_segment;
        offset -= haversine_distance(ni.project_point, ni.way_info->positions.at(s_seg_index - 1));
        for(int32_t index = s_seg_index - 1; index > 0 && offset > 0.0; --index){
            offset -= haversine_distance(ni.way_info->positions.at(index), ni.way_info->positions.at(index - 1));
            s_seg_index = index;
        }

        if(ni.way_info->accident_section.s_seg_index > s_seg_index){
            ni.way_info->accident_section.s_seg_index = s_seg_index;
        }
    }
    else{
        int32_t s_seg_index = ni.project_point_segment;
        if(s_seg_index < ni.way_info->accident_section.s_seg_index){
            ni.way_info->accident_section.s_seg_index = s_seg_index;
        }

        int32_t e_seg_index = ni.project_point_segment;
        offset -= haversine_distance(ni.project_point, ni.way_info->positions.at(e_seg_index));
        for(int32_t index = e_seg_index + 1; index < ni.way_info->positions.size() && offset > 0.0; ++index){
            offset -= haversine_distance(ni.way_info->positions.at(index), ni.way_info->positions.at(index - 1));
            e_seg_index = index;
        }

        if(ni.way_info->accident_section.e_seg_index < e_seg_index){
            ni.way_info->accident_section.e_seg_index = e_seg_index;
        }
    }

    if(outgeojson){
        std::vector<Point2D> ps(ni.way_info->positions.begin() + ni.way_info->accident_section.s_seg_index - 1, ni.way_info->positions.begin() + ni.way_info->accident_section.e_seg_index + 1);
        outgeojson->add_line(ps, {{"ni", "true"},
                                  {"s_seg_index", std::to_string(ni.way_info->accident_section.s_seg_index)},
                                  {"e_seg_index", std::to_string(ni.way_info->accident_section.e_seg_index)},
                                  {"point_number", std::to_string(ni.way_info->positions.size())}});
    }

    auto dsegmentId = SegmentIdToDSegmentId(ni.way_info->id, ni.is_reverse);
    double b1 = CALCUL_WAY_BEARING(ni.is_reverse, ni.way_info->positions);

    while(offset > 0.0){
        std::vector<DSegmentId> dsegids_backward;
        dm->GetBackwardOfDSegmnetId(dsegmentId, dsegids_backward);

        if(dsegids_backward.empty()){
            break;
        }

        std::shared_ptr<SegmentInfo> si = nullptr;
        auto next_desid = 0;
        bool is_reverse = false;
        double b2 = 0;
        double angle = 0;

        for(int32_t index = 0; index < dsegids_backward.size(); ++index){
            auto next_desid_t = dsegids_backward.at(index);
            if(DSegmentIdToSegmentId(dsegmentId) == DSegmentIdToSegmentId(next_desid_t)){
                continue;
            }
            auto si_t = dm->GetSegmentInfoById(DSegmentIdToSegmentId(next_desid_t));
            if(!si_t){
               continue;
            }

            bool is_reverse_t = IsReverseOfDSegmentId(next_desid_t);
            double b_t = CALCUL_WAY_BEARING(is_reverse_t, si_t->positions);
            double angle_t = calculateIncludeAngle(b1, b_t);
            if(si == nullptr || angle_t < angle){
                next_desid = next_desid_t;
                si = si_t;
                b2 = b_t;
                angle = angle_t;
                is_reverse = is_reverse_t;
            }
        }

        if(!si){
            break;
        }

        si->has_accident = true;
        if(offset > si->length){
            si->accident_section = {1, (static_cast<int32_t>(si->positions.size()) - 1)};

            b1 = b2;
            dsegmentId = next_desid;
            offset -= si->length;

            if(outgeojson){
                std::vector<Point2D> ps(si->positions.begin() + si->accident_section.s_seg_index - 1, si->positions.begin() + si->accident_section.e_seg_index + 1);
                outgeojson->add_line(ps,{{"ni", "false_1"},
                                        {"s_seg_index", std::to_string(si->accident_section.s_seg_index)},
                                        {"e_seg_index", std::to_string(si->accident_section.e_seg_index)},
                                        {"point_number", std::to_string(si->positions.size())}});
            }
        }
        else{
            if(is_reverse){
                si->accident_section.e_seg_index = si->positions.size() - 1;

                int32_t s_seg_index = si->accident_section.e_seg_index;
                offset -= haversine_distance(si->positions.at(s_seg_index - 1), si->positions.at(s_seg_index));
                for(int32_t index = s_seg_index - 1; index > 0 && offset > 0.0; --index){
                    offset -= haversine_distance(si->positions.at(index - 1), si->positions.at(index));
                    s_seg_index = index;
                }

                if(si->accident_section.s_seg_index > s_seg_index){
                    si->accident_section.s_seg_index = s_seg_index;
                }
            }
            else{
                si->accident_section.s_seg_index = 1;

                int32_t e_seg_index = si->accident_section.s_seg_index;
                offset -= haversine_distance(si->positions.at(e_seg_index - 1), si->positions.at(e_seg_index));
                for(int32_t index = e_seg_index + 1; index < si->positions.size() && offset > 0.0; ++index){
                    offset -= haversine_distance(si->positions.at(index - 1), si->positions.at(index));
                    e_seg_index = index;
                }

                if(si->accident_section.e_seg_index < e_seg_index){
                    si->accident_section.e_seg_index = e_seg_index;
                }
            }

            if(outgeojson){
                std::vector<Point2D> ps(si->positions.begin() + si->accident_section.s_seg_index - 1, si->positions.begin() + si->accident_section.e_seg_index + 1);
                outgeojson->add_line(ps,{{"ni", "false_2"},
                                        {"s_seg_index", std::to_string(si->accident_section.s_seg_index)},
                                        {"e_seg_index", std::to_string(si->accident_section.e_seg_index)},
                                        {"point_number", std::to_string(si->positions.size())}});
            }
            break;
        }
    }
}

uint32_t MatchService::RunQuerySlope(const std::vector<Point2D> & coordlist, std::vector<TrackInfo > & track_list){
    // std::cout << "tie the road nearby, point size:" << coordlist.size() << std::endl;

    size_t size = coordlist.size();
    track_list.resize(coordlist.size());

    for(size_t index = 0; index < size; ++index){
        track_list[index].pt = coordlist[index];
        track_list[index].distance_pre =  (index == 0 ? 0.0 : haversine_distance(coordlist.at(index - 1), coordlist.at(index)));
    }

    // std::vector<std::vector<NearestInfo > > route_nearest_info;
    // route_nearest_info.resize(size);

    for(size_t index = 0; index < size; ++index){
        if(index == 3079){
            index = index;
        }

        const Point2D &p = coordlist.at(index);
        std::vector<std::pair<RBox, std::shared_ptr<SegmentInfo>>> results;
        dm_slope->GetSegmentInfoByCoord(p.x, p.y, results);

        // const size_t & size = results.size();
        std::vector<NearestInfo> & nearest_infos = track_list[index].nis;
        nearest_infos.reserve(size);

        double brearing = 0.0;
        if(index == (size - 1)){
            int32_t s = index;
            for(double len = 0.0; len < 10.0 && s > 0; --s){
                len += track_list[s].distance_pre;
            }
            brearing = CalculateAngle(coordlist.at(s), coordlist.at(index));
        }else {
            int32_t e = index;
            for(double len = 0.0; len < 10.0 && e < (size - 1); ++e){
                len += track_list[e + 1].distance_pre;
            }
            brearing = CalculateAngle(coordlist.at(index), coordlist.at(e));
        }

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

                if(ni.angle < 30){
                    nearest_infos.emplace_back(std::move(ni));
                }
            }
        }

        std::sort(nearest_infos.begin(), nearest_infos.end(), [&](const NearestInfo & a, const NearestInfo &b){
            return ((a.distance + a.angle * 4.0) < (b.distance + b.angle * 4.0));
        });

        // route_nearest_info[index] = nearest_infos;
    }

    return coordlist.size();
}

uint32_t MatchService::RunQuery(const std::vector<Point2D> & coordlist, MatchPathResult &result, double max_angle_diff){
    std::vector<TrackInfo > track_list;
    RunQuery(coordlist, track_list);

    // std::cout << "bidirectional dijkstras..." << std::endl;
    BidirectionalDijkstras bd(dm, track_list /*coordlist, route_nearest_info*/);
    bd.constructMatchPathResult(result);
    return coordlist.size();
}

uint32_t MatchService::RunQuerySlope(const std::vector<Point2D> & coordlist, MatchPathResult &result){
    std::vector<TrackInfo > track_list;
    RunQuerySlope(coordlist, track_list);

    result.is_slope = true;

    // std::cout << "bidirectional dijkstras..." << std::endl;
    BidirectionalDijkstras bd(dm_slope, track_list /*coordlist, route_nearest_info*/);
    bd.constructMatchPathResult(result);
    return coordlist.size();
}

uint32_t MatchService::RunQueryConstruction(const std::vector<Point2D> & coordlist, double max_andgle_diff){
   std::vector<TrackInfo > track_list;
   RunQuery(coordlist, track_list, max_andgle_diff);

   // std::cout << "bidirectional dijkstras..." << std::endl;
   BidirectionalDijkstras bd(dm, track_list /*coordlist, route_nearest_info*/);
   return bd.addConstructionInformation();
}
