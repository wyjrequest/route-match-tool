#include <exception>
#include <iostream>
#include <utility>
#include <fstream>
#include <memory>
#include <algorithm>
#include <regex>

#include "databuild.h"
#include "tinyxml2.h"
#include "common.h"

bool readtxt(const char* in_file,
          std::map<uint64_t, RelWayId> &point_hash,
          std::vector<WayInfo> &way_info){
    // 打开文件
    std::ifstream file(in_file);
    if (!file.is_open()) {
        std::cerr << "无法打开文件!" << std::endl;
        return false;
    }

    uint32_t point_id = 1U;
    uint32_t way_id = 1U;
    std::vector<std::string> result;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        result.clear();
        result.reserve(12);

        // 将每行按分隔符分割
        splitstring(line, ';', result);

        if(result.size() == 12){
            // 输出拆分后的结果w
            std::cout << "<< " << way_id << std::endl;

            WayInfo wi;
            wi.way_id = way_id;

            wi.k_v.reserve(6);

            std::string highway = kind_strs.find(result.at(0))->second;
            if(highway.compare("motorway") == 0){
                if(result.at(1).compare("10") == 0 || result.at(3).compare("11") == 0 || result.at(1).compare("15") == 0){
                    highway = "motorway_link";
                }
            }
            else if(highway.compare("trunk") == 0){
                if(result.at(1).compare("10") == 0 || result.at(3).compare("11") == 0 || result.at(1).compare("15") == 0){
                    highway = "trunk_link";
                }
            }
            wi.k_v.emplace_back("highway", highway);

            if(result.at(1).compare("30") == 0){
                wi.k_v.emplace_back("bridge", "true");
            }

            if(result.at(1).compare("31") == 0){
                wi.k_v.emplace_back("tunnel", "true");
            }

            wi.oneway_type = Options_Oneway(atoi(result.at(3).c_str()) - 1);
            wi.k_v.emplace_back("oneway", oneway_type_strs.find(wi.oneway_type)->second);
            wi.k_v.emplace_back("name", result.at(8));
            bool is_toll = (result.at(6).compare("2") != 0);
            wi.k_v.emplace_back("toll", is_toll ? "1" : "0");
            wi.k_v.emplace_back("price_type", (!is_toll || result.at(7).compare("None") == 0 || result.at(7).empty())? "0" : result.at(7));
            wi.k_v.emplace_back("slope", result.at(9));
            wi.k_v.emplace_back("lanes", result.at(11));

            if(result.at(4).size() > 12){
                std::string geom;
                geom.assign(result.at(4).begin() + 11, result.at(4).end() - 1);

                std::vector<std::string> coordlist;
                splitstring(geom, ',', coordlist);

                if(coordlist.size() > 1){
                    for(size_t index = 0; index < coordlist.size(); ++index){
                        std::vector<std::string> coord;
                        splitstring(coordlist.at(index), ' ', coord);
                        if(coord.size() == 2){
                            const double lon = atof(coord.at(0).c_str());
                            const double lat = atof(coord.at(1).c_str());
                            const uint64_t phv = CALCULATE_POINT_HASH(lon, lat);

                            if(point_hash.find(phv) == point_hash.end()){
                                point_hash[phv].id = point_id++;//   = RelWayId(std::make_shared<PointInfo>(point_id++, lon, lat, phv));
                            }
                            wi.rel_node_id.emplace_back(phv);
                        }
                    }
                }
            }

            wi.highway_type = highway_types.find(highway.c_str())->second;
            wi.slope_type = slope_types.find(result.at(9).c_str())->second;

            if(wi.oneway_type == Options_reverse){
                wi.oneway_type = Options_yes;
                if(wi.rel_node_id.size() > 1){
                    std::reverse(wi.rel_node_id.begin(), wi.rel_node_id.end());
                }
                if(wi.slope_type) {
                    wi.slope_type = static_cast<Options_Slope>(static_cast<uint8_t>(wi.slope_type) ^ 3);
                }
            }

            if(wi.rel_node_id.size() > 1){
                auto sph = point_hash.find(wi.rel_node_id.front());
                auto eph = point_hash.find(wi.rel_node_id.back());

                uint32_t swid = wi.way_id << 1;
                sph->second.out_way_id.insert(swid);
                eph->second.in_way_id.insert(swid);

                if(wi.oneway_type == Options_no){
                    sph->second.in_way_id.insert(swid + 1);
                    eph->second.out_way_id.insert(swid + 1);
                }
            }

            way_info.emplace_back(wi);
            // way_info[way_id] = wi;
            ++way_id;
        }
    }

    file.close();
    return true;
    // // 打开文件
    // std::ifstream file(in_file);
    // if (!file.is_open()) {
    //     std::cerr << "无法打开文件!" << std::endl;
    //     return false;
    // }

    // uint32_t point_id = 1U;
    // uint32_t way_id = 1U;
    // std::vector<std::string> result;
    // std::string line;
    // while (std::getline(file, line)) {
    //     if (!line.empty() && line.back() == '\r') {
    //         line.pop_back();
    //     }

    //     result.clear();
    //     result.reserve(15);

    //     // 将每行按分隔符分割
    //     splitstring(line, ';', result);

    //     if(result.size() == 15){
    //         // 输出拆分后的结果
    //         std::cout << "<< " << result.at(0).c_str() << std::endl;

    //         WayInfo wi;
    //         wi.way_id = way_id;

    //         wi.k_v.reserve(6);

    //         std::string highway = kind_strs.find(result.at(14))->second;
    //         if(highway.compare("unclassified") == 0){
    //             if(result.at(3).compare("10") == 0 || result.at(3).compare("11") == 0){
    //                 highway = "motorway_link";
    //             }
    //         }

    //         wi.k_v.emplace_back("highway", highway);
    //         if(result.at(3).compare("30") == 0){
    //             wi.k_v.emplace_back("bridge", "true");
    //         }

    //         if(result.at(3).compare("31") == 0){
    //             wi.k_v.emplace_back("tunnel", "true");
    //         }

    //         wi.oneway_type = Options_Oneway(atoi(result.at(6).c_str()) - 1);
    //         wi.k_v.emplace_back("oneway", oneway_type_strs.find(wi.oneway_type)->second);
    //         wi.k_v.emplace_back("name", result.at(10));
    //         wi.k_v.emplace_back("price_type", result.at(13).compare("None") == 0 ? "0" : result.at(13));
    //         // wi.k_v.emplace_back("slope", result.at(15));
    //         wi.k_v.emplace_back("toll", (result.at(7).compare("1") == 0 ? "1" : "0"));
    //         wi.k_v.emplace_back("lanes", result.at(8));

    //         if(result.at(11).size() > 3){
    //             std::string geom;
    //             geom.assign(result.at(11).begin() + 1, result.at(11).end() - 1);

    //             std::vector<std::string> coordlist;
    //             splitstring(geom, ',', coordlist);

    //             if(coordlist.size() > 1){
    //                 for(size_t index = 0; index < coordlist.size(); ++index){
    //                     std::vector<std::string> coord;
    //                     splitstring(coordlist.at(index), ' ', coord);
    //                     if(coord.size() == 2){
    //                         const double lon = atof(coord.at(0).c_str());
    //                         const double lat = atof(coord.at(1).c_str());
    //                         const uint64_t phv = CALCULATE_POINT_HASH(lon, lat);

    //                         if(point_hash.find(phv) == point_hash.end()){
    //                             point_hash[phv].id = point_id++;//   = RelWayId(std::make_shared<PointInfo>(point_id++, lon, lat, phv));
    //                         }
    //                         wi.rel_node_id.emplace_back(phv);
    //                     }
    //                 }
    //             }
    //         }

    //         if(wi.oneway_type == Options_reverse){
    //             wi.oneway_type = Options_yes;
    //             if(wi.rel_node_id.size() > 1){
    //                 std::reverse(wi.rel_node_id.begin(), wi.rel_node_id.end());
    //             }
    //             if(wi.slope_type) {
    //                 wi.slope_type = static_cast<Options_Slope>(static_cast<uint8_t>(wi.slope_type) ^ 3);
    //             }
    //         }

    //         if(wi.rel_node_id.size() > 1){
    //             auto sph = point_hash.find(wi.rel_node_id.front());
    //             auto eph = point_hash.find(wi.rel_node_id.back());

    //             uint32_t swid = wi.way_id << 1;
    //             sph->second.out_way_id.insert(swid);
    //             eph->second.in_way_id.insert(swid);

    //             if(wi.oneway_type == Options_no){
    //                 sph->second.in_way_id.insert(swid + 1);
    //                 eph->second.out_way_id.insert(swid + 1);
    //             }
    //         }

    //         way_info.emplace_back(wi);
    //         // way_info[way_id] = wi;
    //         ++way_id;
    //     }
    // }

    // file.close();
    // return true;
}

bool readxml(const char* in_file,
          std::map<uint64_t, RelWayId> &point_hash,
          std::vector<WayInfo> &way_info
          /*std::map<uint32_t, WayInfo> &way_info*/){
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError eResult = doc.LoadFile(in_file);
    if (eResult != tinyxml2::XML_SUCCESS) {
        std::cout << "Failed to load file\n";
        return false;
    }

    tinyxml2::XMLElement* root = doc.RootElement();
    if (root == nullptr) {
        std::cout << "Failed to get root element\n";
        return false;
    }
    std::map<uint32_t, std::shared_ptr<PointInfo> > point_info;

    std::cout << "<< load node" << std::endl;
    tinyxml2::XMLElement* node = root->FirstChildElement("node");
    for (; node != nullptr; node = node->NextSiblingElement()) {

        const tinyxml2::XMLAttribute* id_attr = node->FindAttribute("id");
        const tinyxml2::XMLAttribute* lon_attr = node->FindAttribute("lon");
        const tinyxml2::XMLAttribute* lat_attr = node->FindAttribute("lat");

        if (id_attr && lon_attr && lat_attr) {
            std::cout << "<< ID: " << id_attr->Value() << ", lon: " << lon_attr->Value() << ", lat: " << lat_attr->Value() << std::endl;

            const uint32_t id = id_attr->UnsignedValue();
            const double lon = lon_attr->DoubleValue();
            const double lat = lat_attr->DoubleValue();
            const uint64_t pointHash = CALCULATE_POINT_HASH(lon, lat);

            auto pi = std::make_shared<PointInfo>(id, lon, lat, pointHash);
            point_info[id] = pi;
            point_hash[pointHash].id = id; // = RelWayId(pi);
        }
    }

    std::cout << "<< load way" << std::endl;
    uint32_t id_index = 1U;
    tinyxml2::XMLElement* way = root->FirstChildElement("way");
    for (; way != nullptr; way = way->NextSiblingElement()) {
        const tinyxml2::XMLAttribute* id_attr = way->FindAttribute("id");
        if(!id_attr){
            continue;
        }

        std::cout << "<< way ID: " << id_attr->UnsignedValue() << std::endl;
        WayInfo wi;

        const tinyxml2::XMLElement * tag = way->FirstChildElement("tag");
        for (; tag != nullptr; tag = tag->NextSiblingElement()){
            const tinyxml2::XMLAttribute* k_attr = tag->FindAttribute("k");
            const tinyxml2::XMLAttribute* v_attr = tag->FindAttribute("v");
            if(k_attr){
                if(strcmp(k_attr->Value(), "highway") == 0){
                    if(v_attr){
                        auto type_it = highway_types.find(v_attr->Value());
                        wi.highway_type = (type_it != highway_types.cend()) ? type_it->second : Options_Highway_secondary;
                    }
                }
                else if(strcmp(k_attr->Value(), "oneway") == 0){
                    if(v_attr){
                        auto type_it = oneway_types.find(v_attr->Value());
                        wi.oneway_type = (type_it != oneway_types.cend()) ? type_it->second : Options_no;
                    }
                }
                else if(strcmp(k_attr->Value(), "slope") == 0){
                    if(v_attr){
                        auto type_it = slope_types.find(v_attr->Value());
                        wi.slope_type = (type_it != slope_types.cend()) ? type_it->second : Options_plain;
                    }
                }
            }

            wi.k_v.emplace_back(k_attr->Value(), v_attr->Value());
        }

        wi.way_id = id_index; // id_attr->UnsignedValue();
        const tinyxml2::XMLElement * nd = way->FirstChildElement("nd");
        for (; nd != nullptr; nd = nd->NextSiblingElement()){
            const tinyxml2::XMLAttribute* ref_attr = nd->FindAttribute("ref");
            if(ref_attr){
                auto pi = point_info.find(ref_attr->UnsignedValue());
                if(pi != point_info.end()){
                    wi.rel_node_id.emplace_back(pi->second->pointHash);
                }
            }
        }

        if(wi.rel_node_id.size() > 1){
            if(wi.oneway_type == Options_reverse){
                wi.oneway_type = Options_yes;
                std::reverse(wi.rel_node_id.begin(), wi.rel_node_id.end());
                if(wi.slope_type) wi.slope_type = static_cast<Options_Slope>(static_cast<uint8_t>(wi.slope_type) ^ 3);
            }

            auto sph = point_hash.find(wi.rel_node_id.front());
            auto eph = point_hash.find(wi.rel_node_id.back());

            uint32_t swid = wi.way_id << 1;
            sph->second.out_way_id.insert(swid);
            eph->second.in_way_id.insert(swid);

            if(wi.oneway_type == Options_no){
                sph->second.in_way_id.insert(swid + 1);
                eph->second.out_way_id.insert(swid + 1);
            }
        }
        way_info.emplace_back(wi);
        ++id_index;
        // way_info.insert(std::make_pair(wi.way_id, wi));
    }
    return true;
}

void buildxml(const char* out_file,
              std::map<uint64_t, RelWayId> &point_hash,
              std::vector<WayInfo> &way_info
              /*std::map<uint32_t, WayInfo> &way_info*/){

    std::cout << "<< Declare the XML file to be created " << std::endl;


    std::ofstream file(out_file);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing!" << std::endl;
        return;
    }

    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
    file << "<osm version=\"0.6\" generator=\"ogr2osm 1.2.0\" upload=\"false\">" << std::endl;

    std::cout << "<< Create osm node " << std::endl;
    std::cout << "<< Write in node 0%" << std::endl;

    int32_t step_num = point_hash.size() / 20  + 1;
    uint32_t id_index = 1U;
    for(auto node_it = point_hash.begin();  node_it != point_hash.end(); ++node_it, ++id_index){
        // RelWayId node_item = node_it->second;
        node_it->second.id = id_index;

        char buffer[128];
        snprintf(buffer, sizeof(buffer), "    <node visible=\"true\" id=\"%d\" lat=\"%.6f\" lon=\"%.6f\"/>", id_index, POINT_HAST_TO_LAT(node_it->first), POINT_HAST_TO_LON(node_it->first));
        file<<buffer<<std::endl;

        if(step_num > 1 && id_index % step_num == 0){
            double progres = static_cast<double>(id_index) / static_cast<double>(point_hash.size()) * 100.0;
            snprintf(buffer, sizeof(buffer), "%.2f", progres);
            std::cout <<"<< "<< buffer << "%" << std::endl;
        }
    }
    std::cout << "<< 100%" <<std::endl;
    std::cout << "<< write in way  0%" << std::endl;

    std::string way_str;
    id_index = 0U;
    step_num = way_info.size() / 20;
    for(auto way_it = way_info.begin();  way_it != way_info.end(); ++way_it){
        const WayInfo & way_item = *way_it;
        way_str = "    <way visible=\"true\" id=\""+std::to_string(++id_index)+"\">\n";;

        for(const auto & node_id : way_item.rel_node_id){
            auto ph_it = point_hash.find(node_id);
            if(ph_it != point_hash.end()){
                way_str += "        <nd ref=\""+std::to_string(ph_it->second.id)+"\"/>\n";
            }
        }

        for(auto p : way_item.k_v){
            if(std::strcmp(p.first.c_str(), "oneway") == 0){
                way_str+= "        <tag k=\""+p.first+"\" v=\""+oneway_type_strs.find(way_item.oneway_type)->second+"\"/>\n";
            }else if(std::strcmp(p.first.c_str(), "slope") == 0){
                way_str+= "        <tag k=\""+p.first+"\" v=\""+slope_type_strs.find(way_item.slope_type)->second+"\"/>\n";
            }
            else{
                way_str+= "        <tag k=\""+p.first+"\" v=\""+p.second+"\"/>\n";
            }
        }

        way_str += "    </way>";
        file << way_str.c_str() << std::endl;

        if(step_num > 1 && id_index % step_num == 0){
            double progres = static_cast<double>(id_index) / static_cast<double>(way_info.size()) * 100.0;
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%.2f", progres);
            std::cout << "<< "<< buffer << "%" << std::endl;
        }
    }
    file << "</osm>" << std::endl;
    file.close();
    std::cout << "<< 100%" <<std::endl;

    // //声明要创建的xml文件
    // tinyxml2::XMLDocument xml;
    // tinyxml2::XMLDeclaration* declaration = xml.NewDeclaration();
    // xml.InsertFirstChild(declaration);

    // std::cout << "<< Create osm node " << std::endl;
    // //创建根节点
    // tinyxml2::XMLElement* osm = xml.NewElement("osm");
    // osm->SetAttribute("version", "0.6");
    // osm->SetAttribute("generator", "ogr2osm 1.2.0");
    // osm->SetAttribute("upload", "false");
    // xml.InsertEndChild(osm);

    // std::cout << "<< Write in node 0%";
    // uint32_t id_index = 1U;
    // for(auto node_it = point_hash.begin();  node_it != point_hash.end(); ++node_it, ++id_index){
    //     RelWayId node_item = node_it->second;
    //     node_it->second.id = id_index;

    //     tinyxml2::XMLElement* node = xml.NewElement("node");
    //     node->SetAttribute("visible", "true");
    //     node->SetAttribute("id", id_index);

    //     char buffer[64];
    //     snprintf(buffer, sizeof(buffer), "%.6f", POINT_HAST_TO_LAT(node_it->first));
    //     node->SetAttribute("lat", buffer);

    //     snprintf(buffer, sizeof(buffer), "%.6f", POINT_HAST_TO_LON(node_it->first));
    //     node->SetAttribute("lon", buffer);
    //     osm->InsertEndChild(node);

    //     if(id_index % 100000 == 0){
    //         double progres = static_cast<double>(id_index) / static_cast<double>(point_hash.size()) * 100.0;
    //         snprintf(buffer, sizeof(buffer), "%.2f", progres);
    //         std::cout <<"  "<< buffer << "%";
    //     }
    // }
    // std::cout << "  100%" <<std::endl;
    // id_index = 0U;

    // auto build_way_element=[&id_index, &xml](){
    //     tinyxml2::XMLElement* way = xml.NewElement("way");
    //     way->SetAttribute("visible", "true");
    //     way->SetAttribute("id", ++id_index);
    //     return way;
    // };

    // auto append_tag = [&xml](const WayInfo & way_item, tinyxml2::XMLElement* way){
    //     for(auto p : way_item.k_v){
    //         tinyxml2::XMLElement* tag = xml.NewElement("tag");
    //         tag->SetAttribute("k", p.first.c_str());
    //         if(std::strcmp(p.first.c_str(), "oneway") == 0){
    //             tag->SetAttribute("v", oneway_type_strs.find(way_item.oneway_type)->second.c_str());
    //         }else if(std::strcmp(p.first.c_str(), "slope") == 0){
    //             tag->SetAttribute("v", slope_type_strs.find(way_item.slope_type)->second.c_str());
    //         }
    //         else{
    //             tag->SetAttribute("v", p.second.c_str());
    //         }
    //         way->InsertEndChild(tag);
    //     }
    // };

    // std::cout << "<< write in way  0%";
    // for(auto way_it = way_info.begin();  way_it != way_info.end(); ++way_it){
    //     const WayInfo & way_item = way_it->second;
    //     tinyxml2::XMLElement* way = build_way_element();

    //     auto rel_node_step_start = way_item.rel_node_id.begin();
    //     for(auto rel_node_it = rel_node_step_start; rel_node_it != way_item.rel_node_id.end(); ++rel_node_it){
    //         auto point_hash_it = point_hash.find((*rel_node_it));
    //         if(point_hash_it != point_hash.end()){
    //             const RelWayId node_item = point_hash_it->second;

    //             tinyxml2::XMLElement* nd = xml.NewElement("nd");
    //             nd->SetAttribute("ref", node_item.id);
    //             way->InsertEndChild(nd);

    //             if((rel_node_step_start != rel_node_it) &&
    //                (rel_node_it != (way_item.rel_node_id.end() - 1))){

    //                 std::set<uint32_t> tempwayids;
    //                 for(auto & in : node_item.in_way_id){
    //                     tempwayids.insert((in >> 1));
    //                 }

    //                 for(auto & out : node_item.out_way_id){
    //                     tempwayids.insert((out >> 1));
    //                 }

    //                 bool is_slipt = true;
    //                 for(auto & value : tempwayids){
    //                     if(value == way_item.way_id){
    //                         continue;
    //                     }
    //                     // 拆分规则
    //                     if(way_item.highway_type == Options_Highway_motorway){
    //                         auto tempit =  way_info.find(value);
    //                         if(tempit != way_info.end()){
    //                             is_slipt = (way_info.find(value)->second.highway_type == Options_Highway_motorway_link);
    //                         }
    //                     }

    //                     if(is_slipt){
    //                         append_tag(way_item, way);
    //                         osm->InsertEndChild(way);

    //                         way = build_way_element();
    //                         tinyxml2::XMLElement* nd = xml.NewElement("nd");
    //                         nd->SetAttribute("ref", node_item.id);
    //                         way->InsertEndChild(nd);

    //                         rel_node_step_start=rel_node_it;
    //                         break;
    //                     }
    //                 }
    //             }
    //         }
    //     }

    //     append_tag(way_item, way);
    //     osm->InsertEndChild(way);

    //     if(id_index % 10000 == 0){
    //         double progres = static_cast<double>(id_index) / static_cast<double>(way_info.size()) * 100.0;
    //         char buffer[64];
    //         snprintf(buffer, sizeof(buffer), "%.2f", progres);
    //         std::cout << "  "<< buffer << "%";
    //     }
    // }
    // std::cout << "  100%" <<std::endl;
    // //将xml保存到当前项目中
    // xml.SaveFile(out_file);
}

void builddata(const char* out_file,
              std::map<uint64_t, RelWayId> &point_hash,
              std::vector<WayInfo> & way_info
              /*std::map<uint32_t, WayInfo> &way_info*/){

    std::cout << "<< Declare the data file to be created " << std::endl;
    std::ofstream outbin(out_file, std::ios::binary);
    if (!outbin) {
        std::cerr << "Error opening file for writing!" << std::endl;
        return;
    }

    size_t size = way_info.size();
    outbin.write(reinterpret_cast<const char*>(&size), sizeof(size_t));

    std::cout << "<< write in way  0%";
    for(auto & way_item : way_info){
        const WayInfo & way = way_item;

        // write name
        {
            uint8_t character_length = 0xFF;
            for(auto & k_v : way.k_v){
                if(std::strcmp(k_v.first.c_str(), "name") == 0){
                    character_length = k_v.second.length();
                    if(character_length > 0 && character_length < 0xFF){
                        outbin.write(reinterpret_cast<const char*>(&character_length), sizeof(uint8_t));
                        outbin.write(k_v.second.c_str(), character_length);
                    }
                    break;
                }
            }

            if(character_length == 0 || character_length == 0xFF){
                character_length = 0;
                outbin.write(reinterpret_cast<const char*>(&character_length), sizeof(uint8_t));
            }
        }

        if(way.highway_type == Options_Highway::Options_Highway_motorway_link){
            std::cout<< way.way_id << ":motoway_link" << std::endl;
        }

        if(way.slope_type != Options_Slope::Options_plain){
            std::cout << way.way_id << ":" << way.slope_type<<std::endl;
        }

        outbin.write(reinterpret_cast<const char*>(&way.way_id), sizeof(uint32_t));
        outbin.write(reinterpret_cast<const char*>(&way.highway_type), sizeof(Options_Highway));
        outbin.write(reinterpret_cast<const char*>(&way.oneway_type), sizeof(Options_Oneway));
        outbin.write(reinterpret_cast<const char*>(&way.slope_type), sizeof(Options_Slope));

        // write forward way
        {
            auto point_hash_item = point_hash.find(way.rel_node_id.front());
            auto ph = point_hash_item->second;

            size = ph.in_way_id.size();
            outbin.write(reinterpret_cast<const char*>(&size), sizeof(size_t));
            for(auto way_id : ph.in_way_id){
                outbin.write(reinterpret_cast<const char*>(&way_id), sizeof(uint32_t));
            }
        }
        // write backward way
        {
            auto point_hash_item = point_hash.find(way.rel_node_id.back());
            auto ph = point_hash_item->second;

            size = ph.out_way_id.size();
            outbin.write(reinterpret_cast<const char*>(&(size)), sizeof(size_t));
            for(auto way_id : ph.out_way_id){
                outbin.write(reinterpret_cast<const char*>(&way_id), sizeof(uint32_t));
            }
        }

        if(way.oneway_type == Options_Oneway::Options_no)
        {
            // write forward way
            {
                auto point_hash_item = point_hash.find(way.rel_node_id.front());
                auto ph = point_hash_item->second;

                size = ph.out_way_id.size();
                outbin.write(reinterpret_cast<const char*>(&size), sizeof(size_t));
                for(auto way_id : ph.out_way_id){
                    outbin.write(reinterpret_cast<const char*>(&way_id), sizeof(uint32_t));
                }
            }
            // write backward way
            {
                auto point_hash_item = point_hash.find(way.rel_node_id.back());
                auto ph = point_hash_item->second;

                size = ph.in_way_id.size();
                outbin.write(reinterpret_cast<const char*>(&(size)), sizeof(size_t));
                for(auto way_id : ph.in_way_id){
                    outbin.write(reinterpret_cast<const char*>(&way_id), sizeof(uint32_t));
                }
            }
        }

        // write node
        size = way.rel_node_id.size();
        outbin.write(reinterpret_cast<const char*>(&size), sizeof(size_t));
        for(auto node_id : way.rel_node_id){
            auto point_hash_item = point_hash.find(node_id);
            // auto ph = point_hash_item->second;
            uint32_t val = POINT_HAST_TO_LON(point_hash_item->first) * 1000000.0;
            outbin.write(reinterpret_cast<const char*>(&val), sizeof(uint32_t));

            val= POINT_HAST_TO_LAT(point_hash_item->first) * 1000000.0;
            outbin.write(reinterpret_cast<const char*>(&val), sizeof(uint32_t));
        }
    }
    outbin.close();
    std::cout << "  100%" <<std::endl;
}

void slipway(std::map<uint64_t, RelWayId> &point_hash,
               std::vector<WayInfo> & way_info){

    std::cout << "<< write in way  0%" << std::endl;
    way_info.reserve(way_info.size() * 1.2);
    uint32_t step_num = way_info.size() / 20;
    for(size_t way_index = 0; way_index < way_info.size(); ++way_index){

        WayInfo & way_item = way_info[way_index];

        bool is_slipt = false;
        size_t index = 1;
        size_t end_index =  way_item.rel_node_id.size() - 1;
        for(; index < end_index; ++index){
            auto ph_it = point_hash.find(way_item.rel_node_id[index]);
            if(ph_it == point_hash.end()){
                continue;
            }

            RelWayId & node_item = ph_it->second;

            std::set<uint32_t> tempwayids;
            for(auto & in : node_item.in_way_id){
                tempwayids.insert((in >> 1) - 1);
            }
            for(auto & out : node_item.out_way_id){
                tempwayids.insert((out >> 1) - 1);
            }

            for(auto & v : tempwayids){
                if(v == way_item.way_id){
                    continue;
                }
                // 拆分规则
                if(way_item.highway_type == Options_Highway_motorway){
                    is_slipt = (way_info[v].highway_type == Options_Highway_motorway_link);
                }
                else{
                    is_slipt = true;
                }
                break;
            }

            if(is_slipt){
                WayInfo new_way_item;
                new_way_item.way_id = way_info.size() + 1;
                new_way_item.highway_type = way_item.highway_type;
                new_way_item.oneway_type = way_item.oneway_type;
                new_way_item.slope_type = way_item.slope_type;
                new_way_item.rel_node_id.assign(way_item.rel_node_id.begin() + index, way_item.rel_node_id.end());
                new_way_item.k_v = way_item.k_v;

                auto sph = point_hash.find(new_way_item.rel_node_id.front());
                auto eph = point_hash.find(new_way_item.rel_node_id.back());

                uint32_t nswid = new_way_item.way_id << 1;
                sph->second.out_way_id.insert(nswid);
                eph->second.in_way_id.insert(nswid);

                // remove old way rel
                uint32_t oswid = way_item.way_id << 1;
                eph->second.in_way_id.erase(eph->second.in_way_id.find(oswid));

                if(new_way_item.oneway_type == Options_no){
                    sph->second.in_way_id.insert(nswid + 1);
                    eph->second.out_way_id.insert(nswid + 1);

                    // remove old way rel
                    eph->second.out_way_id.erase(eph->second.out_way_id.find(oswid  + 1));
                }
                way_item.rel_node_id.erase(way_item.rel_node_id.begin() + index + 1, way_item.rel_node_id.end());
                way_info.push_back(new_way_item);
                break;
            }
        }

        if(step_num > 1 && way_index % step_num == 0){
            double progres = static_cast<double>(way_index) / static_cast<double>(way_info.size()) * 100.0;
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%.2f", progres);
            std::cout << "<< "<< buffer << "%" << std::endl;
        }
    }
    std::cout << "<< 100%" <<std::endl;
}

int data_processing(int argc, const char *argv[])
{
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " type in_file type out_file\n";
        return 1;
    }

    std::map<uint64_t, RelWayId> point_hash;
    // std::map<uint32_t, WayInfo> way_info;
    std::vector<WayInfo> way_info;

    bool is_completed = false;

    if(std::strcmp(argv[1], "1") == 0){
        std::cout << "read_xml----------" << std::endl;
        is_completed = readxml(argv[2], point_hash, way_info);
    }
    else{
        std::cout << "read_txt----------" << std::endl;
        is_completed = readtxt(argv[2], point_hash, way_info);
    }

    if(!is_completed){
        std::cout << "Error: read fail!" << std::endl;
        return 1;
    }

    std::cout << "slip_way----------" << std::endl;
    slipway(point_hash, way_info);

    if(std::strcmp(argv[3], "1") == 0){
        std::cout << "build_xml----------" << std::endl;
        buildxml(argv[4], point_hash, way_info);

    }
    else{
        std::cout << "build_data----------" << std::endl;
        builddata(argv[4], point_hash, way_info);
    }

    if(!is_completed){
        std::cout << "Error: build fail!" << std::endl;
        return 1;
    }

    std::cout << "execution completed" << std::endl;
    return 0;
}
