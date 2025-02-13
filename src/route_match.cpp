#include "route_match.h"
#include "http_request.h"
#include "match.h"
#include <string>
#include <vector>
#include <iostream>

#include <atomic>
#include <thread>

#ifdef IS_CENTOS
#include <libpq-fe.h>
#else
#include <postgresql/libpq-fe.h>
#endif

void download_data(const std::string & table_name, int32_t max_row_nuber){
    // 数据库连接信息
    std::string conninfo = "host=10.1.171.71 port=5434 dbname=zto_deal user=postgres password=zto";

    // 连接数据库
    PGconn* conn = PQconnectdb(conninfo.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        return;
    }

    std::cout << "Connected to database successfully!" << std::endl;

    // 构造查询语句
    std::string query = "SELECT COUNT(*) FROM public."+table_name+";";

    // 执行查询
    PGresult* res = PQexec(conn, query.c_str());

    // 检查查询结果
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        PQfinish(conn);
        return;
    }

    // 获取总行数
    std::string row_count_str = PQgetvalue(res, 0, 0);
    int32_t row_count = atoi(row_count_str.c_str());
    PQclear(res);

    std::cout << "总行数:" << row_count << std::endl;

    std::ofstream file(std::string(table_name + ".txt").c_str());
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing!" << std::endl;
        PQclear(res);
        PQfinish(conn);
        return;
    }

    std::string link_id_max = "";

    int32_t index = 0;
    do
    {
        // 执行查询
        if(link_id_max.empty()){
            query = "SELECT x.link_id,x.kind,x.form,x.is_highway,x.dir,ST_AsText(x.geom),x.len,x.toll,x.price_type,x.\"name\",x.slope,x.slope_angle,x.lane_n FROM public." + table_name + " x order by x.link_id LIMIT "+std::to_string(max_row_nuber)+" OFFSET 0";
        }else{
            query = "SELECT x.link_id,x.kind,x.form,x.is_highway,x.dir,ST_AsText(x.geom),x.len,x.toll,x.price_type,x.\"name\",x.slope,x.slope_angle,x.lane_n FROM public." + table_name + " x where x.link_id > '"+ link_id_max +"' order by x.link_id LIMIT "+std::to_string(max_row_nuber);
        }

        // 执行查询
        res = PQexec(conn, query.c_str());

        // 检查查询结果
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
            PQclear(res);
            break;
        }

        // 获取行数和列数
        int rows = PQntuples(res);

        if (rows == 0) {
            std::cout << "No more records to fetch." << std::endl;
        } else {
            std::cout << ((index / 1000) + 1) << std::endl;

            int cols = PQnfields(res);
            // 输出每一行数据
            for (int i = 0; i < rows; ++i) {
                for (int j = 1; j < cols; ++j) {
                    if(j > 1){
                        file << ";";
                    }
                    file << PQgetvalue(res, i, j);
                }
                file << std::endl;

                if(i == rows - 1){
                    link_id_max = PQgetvalue(res, i, 0);
                }
            }
        }
        // 清理资源
        PQclear(res);

        if(rows < max_row_nuber){
            break;
        }
        index += max_row_nuber;
    }
    while(true);

    file.close();
    PQfinish(conn);
}

void getTableLastSectionCode(PGconn* conn, std::vector<std::string > & section_code){
    std::string query = "SELECT road_section_code FROM iov_track_road_info"; // WHERE ctid = (SELECT max(ctid) FROM iov_track_road_info

    // 执行查询
    PGresult* res = PQexec(conn, query.c_str());

    // 检查查询是否成功
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "SELECT failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
    }

    // 获取查询结果
    int nrows = PQntuples(res);
    int ncols = PQnfields(res);
    if(ncols > 0){
        for (int i = 0; i < nrows; i++) {
            section_code.push_back(PQgetvalue(res, i, 0));
        }
    }

    // 清理
    PQclear(res);
}

void insert_data(PGconn* conn, const std::string & road_section_code, const std::string& road_info, const std::string& gmt_create, const std::string& bd_geom, const std::string& st_geom) {
    // std::cout << "Connected to database successfully!" << std::endl;

    // 构造 SQL 插入语句
    std::string query = "INSERT INTO iov_track_road_info (road_section_code, road_info, gmt_create, bd_geom, standard_geom) VALUES "
                        "($1::varchar(200), $2::json, TO_TIMESTAMP($3, 'YYYYMMDD'), ST_GeomFromText($4, 4326), ST_GeomFromText($5, 4326)) "
                        "ON CONFLICT (road_section_code) "
                        "DO UPDATE SET "
                        "road_info = EXCLUDED.road_info, "
                        "gmt_create = EXCLUDED.gmt_create, "
                        "bd_geom = EXCLUDED.bd_geom, "
                        "standard_geom = EXCLUDED.standard_geom;";

    // 设置参数
    const char* paramValues[5] = {road_section_code.c_str(), road_info.c_str(), gmt_create.c_str(), bd_geom.c_str(), st_geom.c_str()};

    // 执行插入
    PGresult* res = PQexecParams(
        conn,
        query.c_str(),   // SQL 查询
        5,               // 参数个数
        nullptr,         // 参数类型（可以为空，PostgreSQL 会自动推断）
        paramValues,     // 参数值
        nullptr,         // 参数值长度（可以为空，PostgreSQL 会自动计算）
        nullptr,         // 参数格式（文本格式）
        0                // 返回结果格式（0 表示文本格式）
    );

    // 检查结果
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Insert failed: " << PQerrorMessage(conn) << std::endl;
    } else {
        // std::cout << "Data inserted successfully!" << std::endl;
    }

    // 清理资源
    PQclear(res);
}

// 数据库连接信息
#ifdef IS_CENTOS
const char * conninfo = "host=10.7.176.83 port=5432 dbname=zto_map user=postgres password=zto@666_xt.%888";
const char * requesturl = "http://zmap-openapi.ztosys.com";
#else
const char * conninfo = "host=10.7.68.124 port=5432 dbname=zto_map user=postgres password=zto";
const char * requesturl = "https://zmap-openapi.gw.zt-express.com";
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

    std::cout << "request track:" << section_code.c_str() << std::endl;
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
            insert_data(conn, section_code, result.toSectionInfoJson(), ds, result.toWkt(), "LINESTRING(0.0 0.0,0.0 0.0)");
        }
        else{
            // 提取数据
            const std::string & points = root.get<std::string>("points");

            std::cout << "coordinate transformation gcj02_to_bd09..." << std::endl;
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
                        double bd09_lon, bd09_lat;
                        gcj02_to_bd09(atof(lon_lat.at(0).c_str()), atof(lon_lat.at(1).c_str()), bd09_lon, bd09_lat);
                        coordlist.emplace_back(bd09_lon * 1000000.0, bd09_lat * 1000000.0);
                    }
                }
            }

            MatchPathResult result;
            match_service.RunQuery(coordlist, result);
            insert_data(conn, section_code, std::move(result.toSectionInfoJson()), ds, std::move(result.toWkt()), std::move(pointsToWkt(coordlist)));
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

int route_match_processing(int argc, const char *argv[])
{
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

    if(0){
        section_codes = {"55700-57100"};
        processPoints(argv[2]);
        return 1;
    }

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s/iov/getAllSectionCode?ds=%s", requesturl, argv[2]);
    request_get_line(buffer, section_codes);
    std::cout <<"Total number:" << section_codes.size() << std::endl;


    int32_t thread_num = std::thread::hardware_concurrency() / 2 + 1;
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

