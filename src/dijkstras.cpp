#include "dijkstras.h"

#include <iostream>
#include <fstream>

BidirectionalDijkstras::BidirectionalDijkstras(std::shared_ptr<DataManger> dm,
                                               const std::vector<TrackInfo > & tracklist
                                               /* const std::vector<Point2D> & coordlist,
                                                * const std::vector<std::vector<NearestInfo>> & rni */)
                                                : datamanager(dm), track_list(std::move(tracklist)){
    uint32_t dsegmentid = 0;
    // track_list.resize(coordlist.size());

    for(int32_t i = 0; i < track_list.size(); ++i){
        const auto & nis = track_list[i].nis ;
        // track_list[i].pt = coordlist[i];
        // track_list[i].distance_pre =  (i == 0 ? 0.0 : haversine_distance(coordlist.at(i - 1), coordlist.at(i)));
        // track_list[i].nis = nis;
        for(const auto & ni : nis){
            dsegmentid = (ni.way_info->id << 1) + ni.is_reverse;
            if(pre_segment_cost.find(dsegmentid) == pre_segment_cost.end()){
                pre_segment_cost[dsegmentid].dsegment_id = dsegmentid;
            }
            pre_segment_cost[dsegmentid].factor += (ni.distance + ni.angle * 4);
            pre_segment_cost[dsegmentid].neart_index.emplace_back(i);
        }
    }

    for(auto & pec : pre_segment_cost){
        pec.second.factor = (1.0 + pec.second.factor / (800.0 * (pec.second.neart_index.size()))) / 2.0 ;
    }

    forward_longest_index = {-1, nullptr};
    reverse_longest_index = {track_list.size(), nullptr};

    initialization();
}

double BidirectionalDijkstras::totalExtendLen(int32_t index, bool is_reverse){
    double len = is_reverse ? track_list[index].distance_pre : (((track_list.size() - 1) > index) ? track_list[index + 1].distance_pre : 0.0);
    return len * 1.5;
}

double BidirectionalDijkstras::currentExtendLen(const NearestInfo & ni, bool is_reverse){
    double len = 0.0;
    if(is_reverse == ni.is_reverse){
        len = sqrt(squaredDistance(ni.project_point, ni.way_info->positions.at(ni.project_point_segment)));
        for(uint32_t pindex = ni.project_point_segment + 1; pindex < ni.way_info->positions.size(); ++pindex){
            len+= sqrt(squaredDistance(ni.way_info->positions.at(pindex - 1), ni.way_info->positions.at(pindex)));
        }
    }
    else
    {
        len = sqrt(squaredDistance(ni.way_info->positions.at(ni.project_point_segment - 1), ni.project_point));
        for(uint32_t pindex = ni.project_point_segment - 1; pindex > 0; --pindex){
            len+= sqrt(squaredDistance(ni.way_info->positions.at(pindex - 1), ni.way_info->positions.at(pindex)));
        }
    }
    return len;
}

std::shared_ptr<Node> BidirectionalDijkstras::dsegmentIdToNode(uint32_t dsegment_id, bool is_reverse, std::shared_ptr<Node> parent){
    auto pec = pre_segment_cost.find(dsegment_id);
    bool not_neart_segment = (pec == pre_segment_cost.end());

    if(not_neart_segment){
        const auto & si = datamanager->GetSegmentInfoById(dsegment_id >> 1);
        double len = si->length;
        double f = len;
        double total = 0.0;
        if(parent){
            f += parent->f;
            len += parent->current_len;
            total = parent->total_len;
        }
        return std::make_shared<Node>(dsegment_id, f, not_neart_segment, len, total, parent);
    }
    else{
        const SegmentNeartCost & snc = pec->second;
        int32_t pindex = is_reverse ? snc.neart_index.front() : snc.neart_index.back();

        double len = 0.0, f = 0.0;
        for(const NearestInfo & ni : track_list[pindex].nis){
            if(ni.way_info->id == (dsegment_id >> 1)){
                len = currentExtendLen(ni, is_reverse);
                f = len * snc.factor;
                break;
            }
        }

        if(parent){
            f += parent->f;
        }

        std::shared_ptr<Node> node = std::make_shared<Node>(dsegment_id, f, not_neart_segment, len, totalExtendLen(pindex, is_reverse), parent);

        if(is_reverse){
            if(reverse_longest_index.first > pindex){
                reverse_longest_index = {pindex, node};
            }
        }else{
            if(forward_longest_index.first < pindex){
                forward_longest_index = {pindex, node};
            }
        }
        return node;
    }
}

void BidirectionalDijkstras::initForward(){
    forward_longest_index.second = nullptr;
    for(int32_t index = forward_longest_index.first + 1; index < reverse_longest_index.first; ++index){
        if(track_list.at(index).nis.empty()){
            continue;
        }

        const auto & nis = track_list.at(index).nis;
        const auto & ni = nis.front();

        uint32_t dsegmentid = ((ni.way_info->id << 1) + ni.is_reverse);

        std::shared_ptr<Node> node = dsegmentIdToNode(dsegmentid, false, nullptr);
        forward_open_list.push(node);
        break;
    }
}

void BidirectionalDijkstras::initRevered(){
    reverse_longest_index.second = nullptr;
    for(int32_t index = reverse_longest_index.first - 1; index > forward_longest_index.first; --index){
        if(track_list.at(index).nis.empty()){
            continue;
        }

        const auto & nis = track_list.at(index).nis;
        const auto & ni = nis.front();

        uint32_t dsegmentid = ((ni.way_info->id << 1) + ni.is_reverse);

        std::shared_ptr<Node> node = dsegmentIdToNode(dsegmentid, true, nullptr);
        reverse_open_list.push(node);
        break;
    }
}

void BidirectionalDijkstras::initialization(){
    initForward();
    initRevered();
}

void BidirectionalDijkstras::findPath()
{
    while (!forward_open_list.empty() || !reverse_open_list.empty()) {
        // Expand from the start side
        if(!forward_open_list.empty()){

            auto node = forward_open_list.top();
            forward_open_list.pop();

            const uint32_t & dsegment_id = node->dsegment_id;
            const auto & reverse_close_item = reverse_close_list.find(dsegment_id);

            if(reverse_close_item != reverse_close_list.end()){
                forward_result.push_back(node);
                reverse_result.push_back(reverse_close_item->second);
                break;
            }

            forward_close_list[dsegment_id] = node;

            if((dsegment_id >> 1) == 2059757){
                node = node;
            }

            if(node->current_len < node->total_len){
                std::shared_ptr<SegmentInfo> segment_info = datamanager->GetSegmentInfoById(node->dsegment_id >> 1);
                const auto next_backward_ids = (dsegment_id & 0x1) ? segment_info->reverse_backward_ids : segment_info->backward_ids;

                for(const auto next_dsgment_id : next_backward_ids){
                    if(forward_close_list.find(next_dsgment_id) != forward_close_list.end()){
                        continue;
                    }
                    std::shared_ptr<Node> next_node = dsegmentIdToNode(next_dsgment_id, false, node);
                    forward_open_list.push(next_node);
                }
            }

            if(forward_open_list.empty()){
                forward_result.push_back(forward_longest_index.second);

                initForward();

                if(forward_open_list.empty()){
                    if(reverse_longest_index.second){
                        reverse_result.push_back(reverse_longest_index.second);
                    }
                    break;
                }
            }
        }

        if(!reverse_open_list.empty()){

            auto node = reverse_open_list.top();
            reverse_open_list.pop();

            const uint32_t & dsegment_id = node->dsegment_id;
            const auto & forward_close_item = forward_close_list.find(dsegment_id);

            if(forward_close_item != forward_close_list.end()){
                reverse_result.push_back(node);
                forward_result.push_back(forward_close_item->second);
                break;
            }

            reverse_close_list[dsegment_id] = node;

            if((dsegment_id >> 1) == 2059757){
                node = node;
            }

            if(node->current_len < node->total_len){
                std::shared_ptr<SegmentInfo> segment_info = datamanager->GetSegmentInfoById(node->dsegment_id >> 1);
                const auto next_forward_ids = (dsegment_id & 0x1) ? segment_info->reverse_forward_ids : segment_info->forward_ids;

                for(const auto next_dsgment_id : next_forward_ids){
                    if(reverse_close_list.find(next_dsgment_id) != reverse_close_list.end()){
                        continue;
                    }

                    std::shared_ptr<Node> next_node = dsegmentIdToNode(next_dsgment_id, true, node);
                    reverse_open_list.push(next_node);
                }
            }

            if(reverse_open_list.empty()){
                reverse_result.push_back(reverse_longest_index.second);

                initRevered();

                if(reverse_open_list.empty()){
                    if(forward_longest_index.second){
                        forward_result.push_back(forward_longest_index.second);
                    }
                    break;
                }
            }
        }
    }
}


#include "outgeojson.h"
static OutGeomJson shigong_file("shigong.geojson");
uint32_t BidirectionalDijkstras::addConstructionInformation(){
    findPath();
    std::vector<uint32_t> result_dsegment_ids;
    constructPath(result_dsegment_ids);

    uint32_t find_size = result_dsegment_ids.size();
    if(find_size){
        for(auto dsegment_id : result_dsegment_ids){
            const auto si = datamanager->GetSegmentInfoById(dsegment_id >> 1);
            si->is_construction = true;
            shigong_file.add_line(si->positions, {{"osmid", std::to_string(dsegment_id >> 1)}});
        }
    }
    return find_size;
}

void BidirectionalDijkstras::constructMatchPathResult(MatchPathResult &path)
{
    findPath();
    if(path.is_slope){
        MatchPathResult tmp;
        constructPath(tmp);
        constructSectionInfoSlope(tmp);
        path.slop_list = std::move(tmp.slop_list);
    }else{
        constructPath(path);
        constructSectionInfo(path);
    }
}

void BidirectionalDijkstras::gendsegmentPoints(const uint32_t dsegment_id, const int32_t start_index, const int32_t end_index, std::vector<Point2D> & poistions){
    std::shared_ptr<SegmentInfo> si = datamanager->GetSegmentInfoById(dsegment_id >> 1);

    if(dsegment_id & 0x1){

        int32_t s_index = (start_index == -1 ? si->positions.size() : start_index);
        int32_t e_index = (end_index == -1 ? 0 : end_index);

        for(uint32_t pindex = s_index - 1; pindex > e_index; --pindex){ // The last point will not be added
            poistions.emplace_back(si->positions.at(pindex));
        }
    }
    else{

        int32_t s_index = (start_index == -1 ? 0 : start_index);
        int32_t e_index = (end_index == -1 ? si->positions.size() : end_index);

        for(uint32_t pindex = s_index; pindex < (e_index - 1); ++pindex){ // The last point will not be added
            poistions.emplace_back(si->positions.at(pindex));
        }
    }
}

void BidirectionalDijkstras::constructPoints(const std::vector<uint32_t> & dsegment_ids, std::vector<Point2D> & poistions){
    const size_t size= dsegment_ids.size() ;
    if(size == 0){
        return;
    }

    uint32_t s_segment_index = -1;

    const uint32_t &dsegment_id_front = dsegment_ids.front();
    const uint32_t &segment_id_front = dsegment_id_front >> 1;

    auto psc = pre_segment_cost.find(dsegment_id_front);
    if(psc != pre_segment_cost.end()){
        for(const auto & ni : track_list[psc->second.neart_index.front()].nis){
            if(ni.way_info->id == segment_id_front){
                poistions.emplace_back(ni.project_point);
                s_segment_index = ni.project_point_segment;
                break;
            }
        }
    }

    Point2D last_point;
    uint32_t e_segment_index = -1;

    if(size == 1){
        for(const auto & ni : track_list[psc->second.neart_index.back()].nis){
            if(ni.way_info->id == segment_id_front){
                e_segment_index = ni.project_point_segment;
                last_point = ni.project_point;
                break;
            }
        }
        gendsegmentPoints(dsegment_id_front, s_segment_index, e_segment_index, poistions);
    }
    else{
        gendsegmentPoints(dsegment_id_front, s_segment_index, -1, poistions);

        for(uint32_t index = 1; index < (dsegment_ids.size() - 1); ++index){
            gendsegmentPoints(dsegment_ids.at(index), -1, -1, poistions);
        }

        const uint32_t &dsegment_id_back = dsegment_ids.back();
        const uint32_t &segment_id_back = dsegment_id_back >> 1;

        psc = pre_segment_cost.find(dsegment_id_back);
        if(psc != pre_segment_cost.end()){
            for(const auto & ni : track_list[psc->second.neart_index.back()].nis){
                if(ni.way_info->id == segment_id_back){
                    e_segment_index = ni.project_point_segment;
                    last_point = ni.project_point;
                    break;
                }
            }
        }
        gendsegmentPoints(dsegment_id_back, -1, e_segment_index, poistions);
    }

    if(last_point.x != 0 && last_point.y != 0){
        poistions.emplace_back(last_point);
    }
}

void BidirectionalDijkstras::constructPath(MatchPathResult &path)
{
    std::vector<uint32_t> dsegment_ids;
    for(int32_t forward_index = 0; forward_index < forward_result.size(); ++forward_index){
        dsegment_ids.clear();
        auto node = forward_result.at(forward_index);
        while(node){
            dsegment_ids.emplace_back(node->dsegment_id);
            node = node->parent;
        }

        std::reverse(dsegment_ids.begin(), dsegment_ids.end());
        constructPoints(dsegment_ids, path.poistions);
        path.dsegment_ids.insert(path.dsegment_ids.end(), dsegment_ids.begin(), dsegment_ids.end());
    }

    for(int32_t reverse_index = reverse_result.size() -1; reverse_index >= 0; --reverse_index){
        dsegment_ids.clear();
        auto node = reverse_result.at(reverse_index);
        while(node){
            dsegment_ids.emplace_back(node->dsegment_id);
            node = node->parent;
        }

        constructPoints(dsegment_ids, path.poistions);
        path.dsegment_ids.insert(path.dsegment_ids.end(), dsegment_ids.begin(), dsegment_ids.end());
    }
}

// Reconstruct the path from both directions
void BidirectionalDijkstras::constructPath(std::vector<uint32_t> &result_dsegment_ids) {
    std::vector<uint32_t> dsegment_ids;
    for(int32_t forward_index = 0; forward_index < forward_result.size(); ++forward_index){
        dsegment_ids.clear();
        auto node = forward_result.at(forward_index);
        while(node){
            dsegment_ids.emplace_back(node->dsegment_id);
            node = node->parent;
        }

        std::reverse(dsegment_ids.begin(), dsegment_ids.end());
        result_dsegment_ids.insert(result_dsegment_ids.end(), dsegment_ids.begin(), dsegment_ids.end());
    }

    for(int32_t reverse_index = reverse_result.size() -1; reverse_index >= 0; --reverse_index){
        dsegment_ids.clear();
        auto node = reverse_result.at(reverse_index);
        while(node){
            dsegment_ids.emplace_back(node->dsegment_id);
            node = node->parent;
        }

        result_dsegment_ids.insert(result_dsegment_ids.end(), dsegment_ids.begin(), dsegment_ids.end());
    }
}

void BidirectionalDijkstras::constructSectionInfo(MatchPathResult &path){
    SectionInfo ramp_info;
    SectionInfo slep_info;
    SectionInfo turn_info;
    SectionInfo construction_info;
    SectionInfo dangers_info;

    for(int32_t index = 0; index < track_list.size(); ++index){
        bool is_highway = false;
        for(const auto & ni : track_list[index].nis){
            uint32_t dsegment_id = (ni.way_info->id << 1) + ni.is_reverse;
            if(std::find(path.dsegment_ids.begin(), path.dsegment_ids.end(), dsegment_id) == path.dsegment_ids.end()){
                continue;
            }

            if(!is_highway &&
                    (ni.way_info->highway_type == Options_Highway_motorway ||
                     ni.way_info->highway_type == Options_Highway_motorway_link ||
                     ni.way_info->highway_type == Options_Highway_trunk ||
                     ni.way_info->highway_type == Options_Highway_trunk_link)){
                is_highway = true;
            }

            if(ni.way_info->highway_type == Options_Highway::Options_Highway_motorway_link || ni.way_info->highway_type == Options_Highway::Options_Highway_trunk_link){
                if(ramp_info.start_index == -1){
                    ramp_info.start_index = index;
                    ramp_info.end_index = index;
                }else{
                    ramp_info.end_index = index;
                    ramp_info.len += track_list[index].distance_pre;
                }
            }
            else{
                if(ramp_info.start_index != ramp_info.end_index){
                    path.ramp_list.push_back(ramp_info);
                }
                ramp_info = {-1, -1, 0.0};
            }

            if((ni.is_reverse && ni.way_info->slope_type == Options_Slope::Options_upslope) || (!ni.is_reverse && ni.way_info->slope_type == Options_Slope::Options_downhill)){
                if(slep_info.start_index == -1){
                    slep_info.start_index = index;
                    slep_info.end_index = index;
                }else{
                    slep_info.end_index = index;
                    slep_info.len += track_list[index].distance_pre;
                }
            }
            else{
                if(slep_info.start_index != slep_info.end_index){
                    path.slop_list.push_back(slep_info);
                    slep_info = {-1, -1, 0.0};
                }
                slep_info = {-1, -1, 0.0};
            }

            if(ni.way_info->is_construction){
                if(construction_info.start_index == -1){
                    construction_info.start_index = index;
                    construction_info.end_index = index;
                }else{
                    construction_info.end_index = index;
                    construction_info.len += track_list[index].distance_pre;
                }
            }
            else{
                if(construction_info.start_index != construction_info.end_index){
                    if(construction_info.len > 1000.0){
                        path.construction_list.push_back(construction_info);
                    }
                   //  construction_info = {-1, -1, 0.0};
                }
                construction_info = {-1, -1, 0.0};
            }

            if(ni.way_info->has_accident &&
                     ni.way_info->accident_section.s_seg_index <= ni.project_point_segment &&
                     ni.project_point_segment <= ni.way_info->accident_section.e_seg_index){
                if(dangers_info.start_index == -1){
                    dangers_info.start_index = index;
                    dangers_info.end_index = index;
                }else{
                    dangers_info.end_index = index;
                    dangers_info.len += track_list[index].distance_pre;
                }
            }
            else{
                if(dangers_info.start_index != dangers_info.end_index){
                    path.dangers_list.push_back(dangers_info);
                }
                dangers_info = {-1, -1, 0.0};
            }
            break;
        }

        // dangqiandianzhuanwanbanjing
        if(track_list[index].nis.empty() || !is_highway){
            continue;
        }

        int32_t next_index = index + 1;
        double next_len = 0;

        for(; next_index < track_list.size() ; ++next_index){
            bool is_next_highway = false;
            for(const auto & ni : track_list[next_index].nis){
                uint32_t dsegment_id = (ni.way_info->id << 1) + ni.is_reverse;
                if(std::find(path.dsegment_ids.begin(), path.dsegment_ids.end(), dsegment_id) == path.dsegment_ids.end()){
                    continue;
                }

                if(ni.way_info->highway_type == Options_Highway_motorway ||
                         ni.way_info->highway_type == Options_Highway_motorway_link ||
                         ni.way_info->highway_type == Options_Highway_trunk ||
                         ni.way_info->highway_type == Options_Highway_trunk_link){
                    is_next_highway = true;
                    break;
                }
            }

            if(is_next_highway == false){
                break;
            }

            next_len += track_list[next_index].distance_pre;
            if(next_len > 100.0 || track_list[next_index].nis.empty()){
                break;
            }
        }

        if(next_len < 100.0 || next_index <= turn_info.end_index){
            continue;
        }

        int32_t prev_index = index - 1;
        double prev_len = 0;
        for(; prev_index >= 0; --prev_index){
            bool is_prev_highway = false;
            for(const auto & ni : track_list[prev_index].nis){
                uint32_t dsegment_id = (ni.way_info->id << 1) + ni.is_reverse;
                if(std::find(path.dsegment_ids.begin(), path.dsegment_ids.end(), dsegment_id) == path.dsegment_ids.end()){
                    continue;
                }

                if(ni.way_info->highway_type == Options_Highway_motorway ||
                         ni.way_info->highway_type == Options_Highway_motorway_link ||
                         ni.way_info->highway_type == Options_Highway_trunk ||
                         ni.way_info->highway_type == Options_Highway_trunk_link){
                    is_prev_highway = true;
                    break;
                }
            }

            if(is_prev_highway == false){
                break;
            }

            prev_len += track_list[prev_index + 1].distance_pre;
            if(prev_len > 100.0 || track_list[prev_index].nis.empty()){
                break;
            }
        }

        if(prev_len < 100.0){
            continue;
        }

        double a1 = CalculateAngle(track_list[prev_index].pt, track_list[index].pt);
        double a2 = CalculateAngle(track_list[index].pt, track_list[next_index].pt);
        double a = abs(a1 - a2);
        if(a > 180){
            a = 360 - a;
        }

        double l = (prev_len + next_len);
        double r = 28.6478897 * (l/a);
        if(r < 300){
            if(path.turn_list.size() && path.turn_list.back().end_index >= prev_index){
                // path.turn_list[path.turn_list.size() - 1].end_index = next_index;
                // path.turn_list[path.turn_list.size() - 1].len += l;
                auto & back_info = path.turn_list[path.turn_list.size() - 1];
                back_info.end_index = next_index;
                back_info.len = 0.0;
                for(int32_t t_index = back_info.start_index + 1; t_index <= back_info.end_index; ++t_index){
                            back_info.len += track_list[t_index].distance_pre;
                }
            }else{
                turn_info.end_index = next_index;

                if(turn_info.start_index == -1){
                    turn_info.start_index = prev_index;
                    turn_info.len = l;
                    turn_info.turn_angle = 30.0;
                }
                else{
                    turn_info.len = 0.0;
                    for(int32_t t_index = turn_info.start_index + 1; t_index <=turn_info.end_index; ++t_index){
                        turn_info.len += track_list[t_index].distance_pre;
                    }
                }
                // turn_info.len += l;
                // turn_info.turn_angle = 30.0;
                // turn_info.end_index = next_index;
            }
        }
        else{
            if(turn_info.start_index != -1){
                path.turn_list.push_back(turn_info);
                turn_info = {-1, -1, 0.0};
            }
        }
    }

    if(dangers_info.start_index != dangers_info.end_index){
        path.dangers_list.push_back(dangers_info);
        dangers_info = {-1, -1, 0.0};
    }

    if(turn_info.start_index != -1){
        path.turn_list.push_back(turn_info);
        turn_info = {-1, -1, 0.0};
    }

    if(ramp_info.start_index != ramp_info.end_index){
        path.ramp_list.push_back(ramp_info);
        ramp_info = {-1, -1, 0.0};
    }

    if(slep_info.start_index != slep_info.end_index){
        path.slop_list.push_back(slep_info);
        slep_info = {-1, -1, 0.0};
    }

    if(construction_info.start_index != construction_info.end_index){
        if(construction_info.len > 1000.0){
            path.construction_list.push_back(construction_info);
        }
        construction_info = {-1, -1, 0.0};
    }

    // double len = 0.0;
    // int32_t s_index = -1;
    // int32_t e_index = -1;
    // for(const auto & ramp_info : path.ramp_list){
    //     len = 0.0;
    //     s_index = ramp_info.start_index;
    //     e_index = ramp_info.start_index;
    //     for(; s_index > 0 && (len < 200.0); --s_index){
    //         len += track_list[s_index].distance_pre;
    //     }
    //
    //     if(s_index != e_index){
    //         path.dangers_list.emplace_back(s_index, e_index, len);
    //     }
    //
    //     s_index = ramp_info.end_index;
    //     e_index = ramp_info.end_index;
    //     len = 0.0;
    //     for(; (e_index < track_list.size() - 1) && (len < 200.0); ++e_index){
    //         len += track_list[e_index + 1].distance_pre;
    //     }
    //
    //     if(s_index != e_index){
    //         path.dangers_list.emplace_back(s_index, e_index, len);
    //     }
    // }
}

void BidirectionalDijkstras::constructSectionInfoSlope(MatchPathResult &path){
    SectionInfo slep_info;

    for(int32_t index = 0; index < track_list.size(); ++index){
        for(const auto & ni : track_list[index].nis){
            uint32_t dsegment_id = (ni.way_info->id << 1) + ni.is_reverse;
            if(std::find(path.dsegment_ids.begin(), path.dsegment_ids.end(), dsegment_id) == path.dsegment_ids.end()){
                continue;
            }

            if((ni.is_reverse && ni.way_info->slope_type == Options_Slope::Options_upslope) || (!ni.is_reverse && ni.way_info->slope_type == Options_Slope::Options_downhill)){
                if(slep_info.start_index == -1){
                    slep_info.start_index = index;
                    slep_info.end_index = index;
                }else{
                    slep_info.end_index = index;
                    slep_info.len += track_list[index].distance_pre;
                }
            }
            else{
                if(slep_info.start_index != slep_info.end_index){
                    if(slep_info.len > 500.0){
                        path.slop_list.push_back(slep_info);
                    }
                    slep_info = {-1, -1, 0.0};
                }
                slep_info = {-1, -1, 0.0};
            }
            break;
        }
    }

    if(slep_info.start_index != slep_info.end_index){
        if(slep_info.len > 500.0){
            path.slop_list.push_back(slep_info);
        }
        slep_info = {-1, -1, 0.0};
    }
}
