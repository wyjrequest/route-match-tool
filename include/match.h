#ifndef MATCH_H
#define MATCH_H

#include "common.h"
#include "databuild.h"
#include "data_manager.h"
#include "dijkstras.h"

class MatchService
{
public:
    MatchService() = default;
    MatchService(const std::string & data_path);
    uint32_t RunQuery(const std::vector<Point2D> & coordlist,
                      MatchPathResult &result);

    void initialization(const std::string & data_path);
private:
    std::shared_ptr<DataManger> dm;
};
#endif
