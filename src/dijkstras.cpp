#include "dijkstras.h"

#include <iostream>
#include <fstream>

BidirectionalDijkstras::BidirectionalDijkstras(std::shared_ptr<DataManger> dm,
                                               const std::vector<Point2D> & coordlist,
                                               const std::vector<std::vector<NearestInfo>> & rni): datamanager(dm){
    uint32_t dsegmentid = 0;
    track_list.resize(coordlist.size());

    for(int32_t i = 0; i < coordlist.size(); ++i){
        const auto & nis = rni.at(i);
        track_list[i].pt = coordlist[i];
        track_list[i].distance_pre =  (i == 0 ? 0.0 : haversine_distance(coordlist.at(i - 1), coordlist.at(i)));
        track_list[i].nis = nis;
        for(const auto & ni : nis){
            dsegmentid = (ni.way_info->id << 1) + ni.is_reverse;
            if(pre_segment_cost.find(dsegmentid) == pre_segment_cost.end()){
                pre_segment_cost[dsegmentid].segment_id = dsegmentid;
            }
            pre_segment_cost[dsegmentid].factor += (ni.distance + (ni.angle * 4));
            pre_segment_cost[dsegmentid].neart_index.emplace_back(i);
        }
    }

    for(auto & pec : pre_segment_cost){
        pec.second.factor /= (pec.second.neart_index.size() * pec.second.neart_index.size()) / 400.0;
    }

    forward_longest_index = {-1, nullptr};
    reverse_longest_index = {coordlist.size(), nullptr};

    initialization();
}

double BidirectionalDijkstras::totalExtendLen(bool is_reverse, int32_t index){
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
    bool is_neart_segment = pec != pre_segment_cost.end();

    if(!is_neart_segment){
        const auto & si = datamanager->GetSegmentInfoById(dsegment_id >> 1);
        double len = si->length;
        double f = len;
        double total = 0.0;
        if(parent){
            f += parent->f;
            total = parent->total_len;
        }
        return std::make_shared<Node>(dsegment_id, f, is_neart_segment, len, total, parent);
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

        std::shared_ptr<Node> node = std::make_shared<Node>(dsegment_id, len, is_neart_segment, len, totalExtendLen(pindex, is_reverse), parent);

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
    for(int32_t index = reverse_longest_index.first - 1; index > reverse_longest_index.first; --index){
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

void BidirectionalDijkstras::findPath(MatchPathResult &path)
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

    constructPath(path);
    constructSectionInfo(path);
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
    for(const auto & ni : track_list[psc->second.neart_index.front()].nis){
        if(ni.way_info->id == segment_id_front){
            poistions.emplace_back(ni.project_point);
            s_segment_index = ni.project_point_segment;
            break;
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
        const uint32_t &segment_id_back = dsegment_id_back << 1;

        for(const auto & ni : track_list[psc->second.neart_index.back()].nis){
            if(ni.way_info->id == segment_id_back){
                e_segment_index = ni.project_point_segment;
                last_point = ni.project_point;
                break;
            }
        }
        gendsegmentPoints(dsegment_id_back, -1, e_segment_index, poistions);
    }
    poistions.emplace_back(last_point);

//    if(dsegment_ids.size() == 1){
//        std::shared_ptr<SegmentInfo> si_front = datamanager->GetSegmentInfoById(dsegment_ids.front());
//
//        uint32_t s_segment_index = 1;
//        uint32_t e_segment_index = si_front->positions.size() - 1;
//
//        const uint32_t &dsegment_id_front = dsegment_ids.front();
//        auto psc = pre_segment_cost.find(dsegment_id_front);
//        if(psc != pre_segment_cost.end()){
//            for(const auto & ni : track_list[psc->second.neart_index.front()].nis){
//                if(ni.way_info->id == dsegment_id_front>>1){
//                    s_segment_index = ni.project_point_segment;
//                    break;
//                }
//            }
//
//            for(const auto & ni : track_list[psc->second.neart_index.back()].nis){
//                if(ni.way_info->id == dsegment_id_front>>1){
//                    e_segment_index = ni.project_point_segment;
//                    break;
//                }
//            }
//        }
//
//        if(s_segment_index == e_segment_index){
//
//        }
//        else if(dsegment_id_front & 0x1){
//            if(s_segment_index >= e_segment_index){
//                for(uint32_t pindex = s_segment_index; pindex >= e_segment_index; --pindex){
//                    poistions.emplace_back(si_front->positions.at(pindex));
//                }
//            }
//        }
//        else{
//            for(uint32_t pindex = s_segment_index; pindex < e_segment_index; ++pindex){
//                poistions.emplace_back(si_front->positions.at(pindex));
//            }
//        }
//    }
//    else{
//        {
//            const uint32_t &dsegment_id_front = dsegment_ids.front();
//            std::shared_ptr<SegmentInfo> si_front = datamanager->GetSegmentInfoById(dsegment_id_front >> 1);
//
//            uint32_t s_segment_index = 1;
//            auto psc = pre_segment_cost.find(dsegment_id_front);
//            if(psc != pre_segment_cost.end()){
//                for(const auto & ni : track_list[psc->second.neart_index.front()].nis){
//                    if(ni.way_info->id == dsegment_id_front>>1){
//                        s_segment_index = ni.project_point_segment;
//                        break;
//                    }
//                }
//            }
//
//            if(dsegment_id_front & 0x1){
//                for(uint32_t pindex = s_segment_index - 1; pindex > 0; --pindex){
//                    poistions.emplace_back(si_front->positions.at(pindex));
//                }
//            }
//            else{
//                for(uint32_t pindex = s_segment_index; pindex < (si_front->positions.size() - 1); ++pindex){
//                    poistions.emplace_back(si_front->positions.at(pindex));
//                }
//            }
//        }
//
//        for(uint32_t s_index = 1; s_index < (dsegment_ids.size() - 1); ++s_index){
//            const uint32_t &dsegment_id = dsegment_ids.at(s_index);
//            std::shared_ptr<SegmentInfo> si = datamanager->GetSegmentInfoById(dsegment_id >> 1);
//            if(dsegment_id & 0x1){
//                for(int32_t pindex = si->positions.size() - 1; pindex > 0; --pindex){
//                    poistions.emplace_back(si->positions.at(pindex));
//                }
//            }
//            else{
//                for(int32_t pindex = 0; pindex < si->positions.size() - 1; ++pindex){
//                    poistions.emplace_back(si->positions.at(pindex));
//                }
//            }
//        }
//        {
//            const uint32_t &dsegment_id_back = dsegment_ids.back();
//            std::shared_ptr<SegmentInfo> si_back = datamanager->GetSegmentInfoById(dsegment_id_back >> 1);
//
//            uint32_t e_segment_index = si_back->positions.size();
//            auto psc = pre_segment_cost.find(dsegment_id_back);
//            if(psc != pre_segment_cost.end()){
//                for(const auto & ni : track_list[psc->second.neart_index.back()].nis){
//                    if(ni.way_info->id == dsegment_id_back>>1){
//                        e_segment_index = ni.project_point_segment;
//                        break;
//                    }
//                }
//            }
//
//            if(dsegment_id_back & 0x1){
//                for(uint32_t pindex = si_back->positions.size() - 1; pindex >= e_segment_index; --pindex){
//                    poistions.emplace_back(si_back->positions.at(pindex));
//                }
//            }
//            else{
//                for(uint32_t pindex = 0; pindex < e_segment_index; ++pindex){
//                    poistions.emplace_back(si_back->positions.at(pindex));
//                }
//            }
//        }
//    }
//    {
//        const uint32_t &dsegment_id_back = dsegment_ids.back();
//
//        auto psc = pre_segment_cost.find(dsegment_id_back);
//        if(psc != pre_segment_cost.end()){
//
//            for(const auto & ni : track_list[psc->second.neart_index.back()].nis){
//                if(ni.way_info->id == dsegment_id_back>>1){
//                    poistions.emplace_back(ni.project_point);
//                    break;
//                }
//            }
//        }
//    }
}

// Reconstruct the path from both directions
void BidirectionalDijkstras::constructPath(MatchPathResult &path) {

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

void BidirectionalDijkstras::constructSectionInfo(MatchPathResult &path){
    SectionInfo ramp_info;
    SectionInfo slep_info;
    for(uint32_t index = 0; index < track_list.size(); ++index){
        for(const auto & ni : track_list[index].nis){
            uint32_t dsegment_id = (ni.way_info->id << 1) + ni.is_reverse;
            if(std::find(path.dsegment_ids.begin(), path.dsegment_ids.end(), dsegment_id) == path.dsegment_ids.end()){
                continue;
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
                    ramp_info = {-1, -1, 0.0};
                }
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
            }
            break;
        }
    }

    if(ramp_info.start_index != ramp_info.end_index){
        path.ramp_list.push_back(ramp_info);
        ramp_info = {-1, -1, 0.0};
    }

    if(slep_info.start_index != slep_info.end_index){
        path.slop_list.push_back(slep_info);
        slep_info = {-1, -1, 0.0};
    }

    double len = 0.0;
    int32_t s_index = -1;
    int32_t e_index = -1;
    for(const auto & ramp_info : path.ramp_list){
        len = 0.0;
        s_index = ramp_info.start_index;
        e_index = ramp_info.start_index;
        for(; s_index > 0 && (len < 200.0); --s_index){
            len += track_list[s_index].distance_pre;
        }

        if(s_index != e_index){
            path.dangers_list.emplace_back(s_index, e_index, len);
        }

        s_index = ramp_info.end_index;
        e_index = ramp_info.end_index;
        len = 0.0;
        for(; (e_index < track_list.size() - 1) && (len < 200.0); ++e_index){
            len += track_list[e_index + 1].distance_pre;
        }

        if(s_index != e_index){
            path.dangers_list.emplace_back(s_index, e_index, len);
        }
    }
}
