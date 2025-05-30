#include "data_manager.h"

#include <iostream>
#include <fstream>

DataManger::DataManger(const std::string & path){

    std::cout << "load :" << path.c_str() << std::endl;
    std::string base = path;
    std::array<std::string, 7> known_extensions{
        {".osm.bz2", ".osm.pbf", ".osm.xml", ".pbf", ".osm", ".osrm", ".bin"}};
    for (const auto &ext : known_extensions)
    {
        const auto pos = base.find(ext);
        if (pos != std::string::npos)
        {
            base.replace(pos, ext.size(), "");
            break;
        }
    }

    base += ".bin";

    std::cout << "load :" << base.c_str() << std::endl;
    std::ifstream infile(base, std::ios::binary);
    if (!infile) {
        std::cerr << "Error opening file for reading!" << std::endl;
        return;
    }

    int32_t slope_number = 0;

    double length = 0.0;
    size_t size = 0;
    infile.read(reinterpret_cast<char*>(&size), sizeof(size));
    waylist.resize(size + 8);
    uint8_t character_length = 0;

    for(size_t r = 0; r < size; ++r){
        std::shared_ptr<SegmentInfo> wi = std::make_shared<SegmentInfo>();
        {
            character_length = 0;
            infile.read(reinterpret_cast<char*>(&character_length), sizeof(uint8_t));
            if(character_length != 0){
                char buffer[character_length];
                // wi->name.resize(character_length);
                infile.read(buffer, character_length);
                wi->name = buffer;
            }
        }
        infile.read(reinterpret_cast<char*>(&wi->id), sizeof(uint32_t));
        infile.read(reinterpret_cast<char*>(&wi->highway_type), sizeof(Options_Highway));
        infile.read(reinterpret_cast<char*>(&wi->oneway_type), sizeof(Options_Oneway));
        infile.read(reinterpret_cast<char*>(&wi->slope_type), sizeof(Options_Slope));

        size_t forward_size = 0;
        infile.read(reinterpret_cast<char*>(&forward_size), sizeof(forward_size));
        if(forward_size){
            wi->forward_ids.resize(forward_size);
            infile.read(reinterpret_cast<char*>(wi->forward_ids.data()), forward_size * sizeof(uint32_t));
        }

        size_t backward_size = 0;
        infile.read(reinterpret_cast<char*>(&backward_size), sizeof(backward_size));
        if(backward_size){
            wi->backward_ids.resize(backward_size);
            infile.read(reinterpret_cast<char*>(wi->backward_ids.data()), backward_size * sizeof(uint32_t));
        }

        if(wi->oneway_type == Options_Oneway::Options_no){
            size_t reverse_backward_size = 0;
            infile.read(reinterpret_cast<char*>(&reverse_backward_size), sizeof(reverse_backward_size));
            if(reverse_backward_size){
                wi->reverse_backward_ids.resize(reverse_backward_size);
                infile.read(reinterpret_cast<char*>(wi->reverse_backward_ids.data()), reverse_backward_size * sizeof(uint32_t));
            }

            size_t reverse_forward_size = 0;
            infile.read(reinterpret_cast<char*>(&reverse_forward_size), sizeof(reverse_forward_size));
            if(reverse_forward_size){
                wi->reverse_forward_ids.resize(reverse_forward_size);
                infile.read(reinterpret_cast<char*>(wi->reverse_forward_ids.data()), reverse_forward_size * sizeof(uint32_t));
            }
        }

        wi->is_standard_trajectory = false;
        wi->is_construction = false;
        wi->has_accident = false;
        wi->accident_section = {std::numeric_limits<int32_t>::max(), -1};
        wi->max_turn_angle = 0.0;
        length = 0.0;
        size_t position_size = 0;
        infile.read(reinterpret_cast<char*>(&position_size), sizeof(position_size));
        if(position_size){
            wi->positions.resize(position_size);
            double a1 = 0.0;
            double a_len = 0.0;
            int32_t p_index = 0;

            for(size_t ps = 0; ps < position_size; ++ps){
                infile.read(reinterpret_cast<char*>(&wi->positions[ps].x), sizeof(wi->positions[ps].x));
                infile.read(reinterpret_cast<char*>(&wi->positions[ps].y), sizeof(wi->positions[ps].y));
                if(ps > 0){
                    double l = haversine_distance(wi->positions[ps - 1], wi->positions[ps]);
                    a_len += l;
                    if(a_len > 10.0){
                        if(p_index == 0){
                            a1 = CalculateAngle(wi->positions[p_index], wi->positions[ps]);
                            p_index = ps;
                        }
                        else{
                            double a2 = CalculateAngle(wi->positions[p_index], wi->positions[ps]);
                            double a = abs(a1 - a2);
                            if(a > 180){
                                a = 360 - a;
                            }

                            if(a > 100){
                                a = a;
                            }
                            wi->max_turn_angle = std::max(wi->max_turn_angle, a);
                            a1 = a2;
                            p_index = ps;
                        }
                        a_len = 0.0;
                    }

                    length += l;
                }
            }
        }
        wi->length = length;
        // std::cout << "<< ID: " << wi->id << ", r:" << r << std::endl;
        waylist[wi->id] = wi;
        if(wi->slope_type == Options_Slope::Options_downhill){
            std::cout<<" downhill slope id:" << wi->id << ", number:" << ++slope_number << std::endl;
        }
    }
    infile.close();

    std::cout << "build grid:" << path.c_str() << std::endl;
    cachelist.resize(((EAST - WEST)/GRID_RANGE + 1) * ((NORTH - SOUTH) / GRID_RANGE + 1));

    for(auto way : waylist){
        if(!way){
            continue;
        }

        Point2D ld(180000000, 90000000);
        Point2D rt(0, 0);
        for(auto point : way->positions){
            ld.x = std::min(point.x, ld.x);
            ld.y = std::min(point.y, ld.y);
            rt.x = std::max(point.x, rt.x);
            rt.y = std::max(point.y, rt.y);
        }

        ld.x -= WEST; rt.x -= WEST; ld.y -= SOUTH; rt.y -= SOUTH;

        const uint32_t rs = ld.x / GRID_RANGE;
        const uint32_t re = rt.x / GRID_RANGE;
        const uint32_t cs = ld.y / GRID_RANGE;
        const uint32_t ce = rt.y / GRID_RANGE;
        for(uint32_t r = rs; r <= re; ++r){
            for(uint32_t c = cs; c <= ce; ++c){
                cachelist.at(C0LUMNS_NUMBER * c + r).insert(std::make_pair(RBox(RPoint(ld.x, ld.y), RPoint(rt.x, rt.y)), way));
            }
        }
    }
    std::cout << "map data load success!" << std::endl;
}

void DataManger::GetSegmentInfoByCoord(const int32_t x, const int32_t y,
                                       std::vector<std::pair<RBox, std::shared_ptr<SegmentInfo>>> & results)
{
    const Point2D ld((x - WEST - 100), y - SOUTH - 100);
    const Point2D rt((x - WEST + 100), y - SOUTH + 100);

    const uint32_t rs = ld.x / GRID_RANGE;
    const uint32_t re = rt.x / GRID_RANGE;
    const uint32_t cs = ld.y / GRID_RANGE;
    const uint32_t ce = rt.y / GRID_RANGE;

    RBox query_box(RPoint(ld.x, ld.y), RPoint(rt.x, rt.y));
    for(uint32_t r = rs; r <= re; ++r){
        for(uint32_t c = cs; c <= ce; ++c){
            cachelist.at(C0LUMNS_NUMBER * c + r).query(bgi::intersects(query_box), std::back_inserter(results));
        }
    }
}
