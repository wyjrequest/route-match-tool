#ifndef MATCH_H
#define MATCH_H

#include "common.h"
#include "databuild.h"
#include "data_manager.h"
#include "dijkstras.h"
#include "outgeojson.h"

struct AccidentInfo{
    Point2D pt;
    int32_t bearing;
    double  offset;
    AccidentInfo() : pt(), bearing(0), offset(0.0){}
    AccidentInfo(const Point2D p, const int32_t b, const double f) : pt(p), bearing(b), offset(f){}

    void set_str(const std::string & str){
        std::vector<std::string> result;
        splitstring(str, ';', result);
        if(result.size() == 4){
            pt.x    = atoi(result.at(0).c_str());
            pt.y    = atoi(result.at(1).c_str());
            bearing = atoi(result.at(2).c_str());
            offset  = atof(result.at(3).c_str());
        }
    }

    std::string to_str(){
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%d;%d;%d;%.2f", pt.x, pt.y, bearing, offset);
        return buffer;
    }
};

class MatchService
{
public:
    MatchService() = default;
    MatchService(const std::string & data_path);
    MatchService(std::shared_ptr<DataManger> data_path);
    uint32_t RunQuery(const std::vector<Point2D> & coordlist, MatchPathResult &result, double max_angle_diff = 30);
    uint32_t RunQuerySlope(const std::vector<Point2D> & coordlist, MatchPathResult &result);
    uint32_t RunQueryConstruction(const std::vector<Point2D> & coordlist, double max_andgle_diff);

    void initialization(const std::string & data_path);
    void setAccidentInfo(const AccidentInfo & ai, OutGeomJson * outgeomjson = NULL);
private:
    uint32_t RunQuery(const std::vector<Point2D> & coordlist, std::vector<TrackInfo > & track_list, double max_angle_diff = 30);
    uint32_t RunQuerySlope(const std::vector<Point2D> & coordlist, std::vector<TrackInfo > & track_list);
    std::shared_ptr<DataManger> dm;
    std::shared_ptr<DataManger> dm_slope;
};
#endif
