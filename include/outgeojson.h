#ifndef OUTGEOJSON_H
#define OUTGEOJSON_H

#include "common.h"

#include <vector>
#include <fstream>
#include <iostream>
#include <string.h>

class OutGeomJson : public std::ofstream{
public:
    static std::string properties_to_geojson(const std::vector<std::pair<std::string, std::string>> & properties = {}){
        std::string geojson = "";
        if(properties.size()){
            geojson += "\"properties\": {";
            for(int32_t index = 0; index < properties.size(); ++index){
                if(index){
                    geojson += ",";
                }
                const auto & kv = properties.at(index);
                geojson += "\""+kv.first+"\":\"" + kv.second +"\"";
            }
            geojson += "},";
        }
        return geojson;
    }

    static std::string line_to_geojson(const std::vector<Point2D> & line, const std::vector<std::pair<std::string, std::string>> & properties = {}){
        std::string geojson = "{\"type\": \"Feature\"," + properties_to_geojson(properties) + "\"geometry\": {\"coordinates\": [";

        bool is_first = false;
        for(const auto & pt : line){
            if(is_first){
                geojson+=",";
            }else{
                is_first = true;
            }
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "[%.6f,%.6f]", pt.toFloatLon(), pt.toFloatLat());
            geojson += buffer;
        }

        geojson += "],\"type\": \"LineString\"}}";
        return geojson;
    }

    static std::string point_to_geojson(const Point2D & point, const std::vector<std::pair<std::string, std::string>> & properties = {}){
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "{\"type\": \"Feature\",%s\"geometry\": {\"coordinates\": [%.6f,%.6f],\"type\": \"Point\"}}", properties_to_geojson(properties).c_str(), point.toFloatLon(), point.toFloatLat());
        return buffer;
    }

    OutGeomJson(const char *file) : std::ofstream(file), feature_number(0){
        if(is_open()){
            *this << "{\"type\": \"FeatureCollection\",\"features\": [";
        }
    }

    ~OutGeomJson(){
        if(is_open()){
            *this << "]}";
            close();
        }
    }

    void add_point(const Point2D &pt, const std::vector<std::pair<std::string, std::string>> & properties = {}){
        const std::string & geom_json = point_to_geojson(pt, properties);
        add(geom_json.c_str());
    }

    void add_line(const std::vector<Point2D> & line, const std::vector<std::pair<std::string, std::string>> & properties = {}){
        const std::string & geom_json = line_to_geojson(line, properties);
        add(geom_json.c_str());
    }

    void add(const char* f){
        if(is_open() && strlen(f) != 0){
            if(feature_number++){
                *this << ',';
            }
            *this << f;
        }
    }
private:
    int32_t feature_number;
};
#endif
