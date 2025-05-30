#include "route_match.h"
#include "postgres_database.h"
#include "http_request.h"
#include "outgeojson.h"
#include "match.h"
#include <string>
#include <vector>
#include <iostream>

#include <atomic>
#include <thread>

// 数据库连接信息
#ifdef IS_CENTOS
const char * conninfo = "host=10.7.176.83 port=5432 dbname=zto_map user=postgres password=zto@666_xt.%888";
const char * requesturl = "http://zmap-openapi.ztosys.com";
#else
const char * conninfo = "host=10.7.68.124 port=5432 dbname=zto_map user=postgres password=zto";
// const char * conninfo = "host=10.7.68.124 port=5432 dbname=zto_map user=postgres password=zto";
const char * requesturl = "https://zmap-openapi.gw-test.ztosys.com";
#endif

MatchService match_service;
std::vector<std::string> section_codes;
std::vector<std::string > pre_section_codes;
std::vector<std::string> fail_section_codes;

std::atomic_flag lock_flag = ATOMIC_FLAG_INIT;
uint32_t spinlock(){
    int32_t index = 0;
    while(lock_flag.test_and_set(std::memory_order_acquire)){
    }

    static uint32_t global_index = 0;
    index = global_index++;

    lock_flag.clear(std::memory_order_release);
    return index;
}

bool processRouteSection(const std::string &section_code, PGconn* conn, const char * ds, bool is_append_fail = true){
    if(!pre_section_codes.empty() &&
       std::find(pre_section_codes.begin(), pre_section_codes.end(), section_code) != pre_section_codes.end()){
        return false;
    }

    char buffer[128];
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s/iov/getTrack?sectionCode=%s&ds=%s", requesturl, section_code.c_str(), ds);

    //std::cout << "request track:" << section_code.c_str() << std::endl;
    std::string track;

    if(!request_get_track(buffer, track)){
        if(is_append_fail){
            while(lock_flag.test_and_set(std::memory_order_acquire)){}
            fail_section_codes.emplace_back(section_code);
            lock_flag.clear(std::memory_order_release);
        }
        return false;
    }

    // 使用 Boost.PropertyTree 解析 JSON 数据
    std::istringstream json_stream(track);
    boost::property_tree::ptree root;

    try {
        // 解析 JSON 数据
        boost::property_tree::read_json(json_stream, root);

        // 使用 count() 判断键是否存在
        if (root.count("points") <= 0) {
            std::cout << "Key 'points' does not exist" << std::endl;
            MatchPathResult result;
            insert_data(conn, section_code, result.toSectionInfoJson(), ds, result.toWkt(), "LINESTRING(0.0 0.0,0.0 0.0)", "LINESTRING(0.0 0.0,0.0 0.0)");
        }
        else{
            // 提取数据
            const std::string & points = root.get<std::string>("points");

            // std::cout << "coordinate transformation gcj02_to_bd09..." << std::endl;
            std::vector<Point2D> coordlist;
            {
                std::vector<std::string> pointlist;
                splitstring(points, ';', pointlist);
                coordlist.reserve(pointlist.size());

                // int32_t s_index = 3070;
                for(const auto &str_point : pointlist){
                    // if(s_index != 0){
                    //     --s_index;
                    //     continue;
                    // }
                    std::vector<std::string> lon_lat;
                    splitstring(str_point, ',', lon_lat);
                    if(lon_lat.size() == 2){
                        coordlist.emplace_back(atof(lon_lat.at(0).c_str()) * 1000000.0, atof(lon_lat.at(1).c_str()) * 1000000.0);
                        // double bd09_lon, bd09_lat;
                        // gcj02_to_bd09(atof(lon_lat.at(0).c_str()), atof(lon_lat.at(1).c_str()), bd09_lon, bd09_lat);
                        // coordlist.emplace_back(bd09_lon * 1000000.0, bd09_lat * 1000000.0);
                    }
                }
            }

            MatchPathResult result;
            match_service.RunQuery(coordlist, result);

            std::vector<Point2D> bd09_coordlist;
            bd09_coordlist.reserve(coordlist.size());
            for(const auto & pt : coordlist){
                double bd09_lon, bd09_lat;
                gcj02_to_bd09(pt.toFloatLon(), pt.toFloatLat(), bd09_lon, bd09_lat);
                bd09_coordlist.emplace_back(bd09_lon * 1000000.0, bd09_lat * 1000000.0);
            }
            // match_service.RunQuerySlope(bd09_coordlist, result);
            insert_data(conn, section_code, std::move(result.toSectionInfoJson()), ds, std::move(result.toWkt()), std::move(pointsToWkt(coordlist)), std::move(pointsToWkt(bd09_coordlist)));
        }
    } catch (const boost::property_tree::json_parser_error& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    }

    return true;
}

void processPoints(const char * ds){
    // 连接数据库
    PGconn* conn = PQconnectdb(conninfo);
    if (PQstatus(conn) == CONNECTION_OK) {
        // PGresult * res = PQexec(conn, "BEGIN");
        // if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        //     std::cerr << "BEGIN command failed: " << PQerrorMessage(conn) << std::endl;
        //     PQfinish(conn);
        //     return;
        // }
        // PQclear(res);

        int32_t current_index = 0;
        while((current_index = spinlock()) < section_codes.size()){
            std::cout << "request" << current_index <<" track:" << section_codes.at(current_index).c_str() << std::endl;
            processRouteSection(section_codes.at(current_index), conn, ds);
        }

        // res = PQexec(conn, "COMMIT");
        // if(PQresultStatus(res) != PGRES_COMMAND_OK){
        //    std::cerr << "COMMIT commad failed:" << PQerrorMessage(conn) << std::endl;
        // }
        // PQclear(res);
    }
    else{
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
    }
    PQfinish(conn);
}

typedef std::pair<RPoint, std::shared_ptr<AccidentInfo>> AcciddentInfoRPointValue;
void processd_high_incidence_of_accidents(){

    std::vector<std::shared_ptr<AccidentInfo> > ai_list;
    CacheGrid<bgi::rtree<AcciddentInfoRPointValue, bgi::quadratic<16>> > cache_rtree;

    const char * tn = "gaode_guide_info";
    load_high_incidence_of_accidents(tn, ai_list);

    for(const auto & ai : ai_list){
        const auto & cache_result = cache_rtree.sreach_by_point(ai->pt);

        if(cache_result.size()){
            RPoint query_point(ai->pt.x, ai->pt.y);
            cache_result.front()->insert(std::make_pair(query_point, ai));
        }
    }

    char fn[64];
    snprintf(fn, sizeof(fn), "%s.txt", tn);

    OutGeomJson ogj("gaode_guide_info.json");
    std::ofstream file(fn);
    if (file.is_open()) {
        for(int32_t i = 0; i< ai_list.size(); ++i){
            const auto & ai = ai_list.at(i);
            const int32_t offset = (ai->offset * 10);
            Point2D lb(ai->pt.x - offset, ai->pt.y - offset);
            Point2D rt(ai->pt.x + offset, ai->pt.y + offset);
            const auto cache_result = cache_rtree.sreach_by_range(lb, rt);

            RBox query_box(RPoint(lb.x, lb.y), RPoint(rt.x, rt.y));
            std::vector<AcciddentInfoRPointValue > rels;
            for(auto rtree : cache_result){
                rtree->query(bgi::within(query_box), std::back_inserter(rels));
            }

            bool is_filter = false;
            for(const auto & rel : rels){
                const auto & rel_ai = rel.second;
                if(ai == rel_ai){
                    continue;
                }

                const double b = CalculateAngle(ai->pt, rel_ai->pt);
                double a = calculateIncludeAngle(b, ai->bearing);
                if(a < 20){
                    a = calculateIncludeAngle(ai->bearing, rel_ai->bearing);
                    if(a < 20){
                        is_filter = true;
                        break;
                    }
                }
            }

            if(false == is_filter){
                file << ai->to_str() << std::endl;
                ogj.add_point(ai->pt, {{"bearing", std::to_string(ai->bearing)}, {"offset", std::to_string(ai->offset)}});
            }
        }

        file.close();
    }
    else{
        std::cerr << "opening file  failed:" << fn << std::endl;
    }
}

int route_match_processing(int argc, const char *argv[])
{
    if(0){
        processd_high_incidence_of_accidents();
        return 1;
    }
    if(0){
        processdw_gis_fence_position_version_index_all();
        return 1;
    }
    if(0){
        double a1 = CalculateAngle(Point2D(127294653, 42377856), Point2D(127294624, 42377745));
        double a2 = CalculateAngle(Point2D(127294624, 42377745), Point2D(127294124, 42377216));

        double a = abs(a1 - a2);
        if(a > 180){
            a = 360 - a;
        }
        return 0;
    }
    if(0){
        NodeQueue nq;
        nq.push(std::make_shared<Node>(1, 1));
        nq.push(std::make_shared<Node>(5, 5));
        nq.push(std::make_shared<Node>(4, 4));
        nq.push(std::make_shared<Node>(3, 3));
        nq.push(std::make_shared<Node>(41, 4));
        nq.push(std::make_shared<Node>(2, 2));
        nq.push(std::make_shared<Node>(6, 6));

        while(!nq.empty()){
            auto node = nq.top();
            nq.pop();
            std::cout << node->dsegment_id << std::endl;
        }
        return 1;
    }
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << "map_path time\n";
        return 1;
    }
    match_service.initialization(argv[1]);

    if(0){
        // 连接数据库
        PGconn* conn = PQconnectdb(conninfo);
        if (PQstatus(conn) != CONNECTION_OK) {
            std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
            PQfinish(conn);
            return 1;
        }
        getTableLastSectionCode(conn, pre_section_codes);
        PQfinish(conn);
        std::cout <<"Number of executed items:" << pre_section_codes.size() << std::endl;
    }
    if(1){
        // 打开文件
        std::ifstream file("shigu.txt");
        if (!file.is_open()) {
            std::cerr << "无法打开文件!" << std::endl;
            return false;
        }

        OutGeomJson outgeojson("shigu.json");

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            std::cout << line.c_str() << std::endl;

            AccidentInfo ai;
            ai.set_str(line);
            match_service.setAccidentInfo(ai, &outgeojson);
        }
        file.close();
        // return 1;
    }
    if(1)
    {
        {
            // 打开文件
            std::ifstream file("shigong.txt");
            if (!file.is_open()) {
                std::cerr << "无法打开文件!" << std::endl;
                return false;
            }

            std::string line;
            int32_t row = 0;
            while (std::getline(file, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                std::vector<std::string> wkt_strs;
                get_wkt_str(line, wkt_strs);

                std::vector<Point2D> coordlist;
                for(const auto & wkt_str : wkt_strs){
                    std::vector<std::string> pointlist;
                    splitstring(wkt_str, ',', pointlist);
                    coordlist.reserve(pointlist.size());

                    // int32_t s_index = 3070;
                    for(const auto &point_str : pointlist){
                        std::vector<std::string> lon_lat;
                        splitstring(point_str, ' ', lon_lat);
                        if(lon_lat.size() == 2){
                            coordlist.emplace_back(atof(lon_lat.at(0).c_str()) * 1000000.0, atof(lon_lat.at(1).c_str()) * 1000000.0);
                        }
                    }
                    std::cout<< row << ";" << wkt_str.c_str() << std::endl;
                }
                uint32_t size = match_service.RunQueryConstruction(coordlist, 180);
                if(0 == size){
                    std::reverse(coordlist.begin(), coordlist.end());
                    match_service.RunQueryConstruction(coordlist, 180);
                }
                ++row;
            }
            file.close();
        }

        {
            // 打开文件
            std::ifstream file("shigong_sg.txt");
            if (!file.is_open()) {
                std::cerr << "无法打开文件!" << std::endl;
                return false;
            }

            std::string line;
            int32_t row = 0;
            while (std::getline(file, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                std::vector<std::string> wkt_strs;
                get_wkt_str(line, wkt_strs);

                for(const auto & wkt_str : wkt_strs){
                    std::vector<Point2D> coordlist;
                    std::vector<std::string> pointlist;
                    splitstring(wkt_str, ',', pointlist);
                    coordlist.reserve(pointlist.size());

                    // int32_t s_index = 3070;
                    for(const auto &point_str : pointlist){
                        std::vector<std::string> lon_lat;
                        splitstring(point_str, ' ', lon_lat);
                        if(lon_lat.size() == 2){
                            coordlist.emplace_back(atof(lon_lat.at(0).c_str()) * 1000000.0, atof(lon_lat.at(1).c_str()) * 1000000.0);
                        }
                    }
                    std::cout<< row << ";" << wkt_str.c_str() << std::endl;
                    match_service.RunQueryConstruction(coordlist, 20);
                }
                ++row;
            }
            file.close();
        }

        // return 1;
    }

    if(1){
        // section_codes = {"55700-57100"};
        // section_codes = {"02828-79100"};
        // section_codes = {"57600-57100"};
        // section_codes = {"00007-21999"};
        // section_codes = {"02199-55100"};
        // section_codes = {"02199-02500", "02700-43100", "57700-55200"};
        // section_codes = {"02700-43100"};
        section_codes = {"02199-02500",
                         "57700-55200",
                         "871220-87100",
                         "02301-02828",
                         "87100-02301",
                         "02700-43100"};
        processPoints(argv[2]);
        return 1;
    }

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s/iov/getAllSectionCode?ds=%s", requesturl, argv[2]);
    request_get_line(buffer, section_codes);
    std::cout <<"Total number:" << section_codes.size() << std::endl;

    // int32_t thread_num = std::thread::hardware_concurrency() / 2 + 1;
    int32_t thread_num = 3;
    std::cout <<"thread number:" << thread_num << std::endl;

    std::vector<std::thread> thread_group;
    thread_group.reserve(thread_num);

    for(int32_t t = 0; t < thread_num; ++t){
        thread_group.emplace_back(processPoints, argv[2]);
    }

    for(int32_t t = 0; t < thread_num; ++t){
        thread_group.at(t).join();
    }

    if(fail_section_codes.size()){
        PGconn* conn = PQconnectdb(conninfo);
        if (PQstatus(conn) == CONNECTION_OK) {
            for(auto section_code : fail_section_codes){
                processRouteSection(section_code.c_str(), conn, argv[2], false);
            }
        }
        else {
            std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        }
        PQfinish(conn);
   }

   std::cout << "end!" << std::endl;
   return 1;
}
