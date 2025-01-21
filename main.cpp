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
    std::cout << "Connected to database successfully!" << std::endl;

    // 构造 SQL 插入语句
    std::string query = "INSERT INTO iov_track_road_info (road_section_code, road_info, gmt_create, bd_geom, standard_geom) VALUES ($1::varchar(200), $2::json, $3::timestamp(0), ST_GeomFromText($4, 4326), ST_GeomFromText($5, 4326));";

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
        std::cout << "Data inserted successfully!" << std::endl;
    }

    // 清理资源
    PQclear(res);
}

void downloadData(){
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
    std::string query = "SELECT COUNT(*) FROM public.zto_nav_v3_gc_motorway;";

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

    std::ofstream file("motorway.txt");
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing!" << std::endl;
        return;
    }

    for(int32_t index = 0; index < row_count; index+= 1000)
    {
        // 执行查询
        query = "SELECT x.kind,x.form,x.is_highway,x.dir,ST_AsText(x.geom),x.len,x.price_type,x.\"name\",x.slope,x.slope_angle FROM public.zto_nav_v3_gc_motorway x LIMIT 1000 OFFSET " + std::to_string(index) + ";";

        // 执行查询
        res = PQexec(conn, query.c_str());

        // 检查查询结果
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
            PQclear(res);
            PQfinish(conn);
            return;
        }

        // 获取行数和列数
        int rows = PQntuples(res);
        int cols = PQnfields(res);

        if (rows == 0) {
            std::cout << "No more records to fetch." << std::endl;
        } else {
            std::cout << ((index / 1000) + 1) << std::endl;

            // 输出每一行数据
            for (int i = 0; i < rows; ++i) {
                for (int j = 0; j < cols; ++j) {
                    if(j){
                        file << ";";
                    }
                    file << PQgetvalue(res, i, j);
                }
                file << std::endl;
            }
        }
        // 清理资源
        PQclear(res);
    }

    file.close();

    PQfinish(conn);
}

MatchService match_service;
std::vector<std::string> section_codes;
std::vector<std::string > pre_section_codes;

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
void processPoints(const char * conninfo){
    // 连接数据库
    PGconn* conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        return;
    }

    // PGresult * res = PQexec(conn, "BEGIN");
    // if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    //     std::cerr << "BEGIN command failed: " << PQerrorMessage(conn) << std::endl;
    //     PQfinish(conn);
    //     return;
    // }
    // PQclear(res);

    char buffer[128];
    // for(const auto & section_code : section_codes){
    int32_t current_index = 0;
    while((current_index = spinlock()) < section_codes.size()){
        const std::string &section_code = section_codes.at(current_index);
        if(std::find(pre_section_codes.begin(), pre_section_codes.end(), section_code) != pre_section_codes.end()){
            continue;
        }

        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "https://zmap-openapi.gw.zt-express.com/iov/getTrack?sectionCode=%s", section_code.c_str());

        std::cout << "request track:" << section_code.c_str() << std::endl;
        std::string track;
        request_get_track(buffer, track);

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
                insert_data(conn, section_code, result.toSectionInfoJson(), "2025-01-10", result.toWkt(), "LINESTRING(0.0 0.0,0.0 0.0)");
                continue;
            }

            // 提取数据
            const std::string & points = root.get<std::string>("points");

            std::cout << "coordinate transformation gcj02_to_bd09..." << std::endl;
            std::vector<Point2D> coordlist;
            {
                std::vector<std::string> pointlist;
                splitstring(points, ';', pointlist);
                coordlist.reserve(pointlist.size());

                for(const auto &str_point : pointlist){
                    std::vector<std::string> lon_lat;
                    splitstring(str_point, ',', lon_lat);
                    if(lon_lat.size() == 2){
                        double bd09_lon, bd09_lat;
                        gcj02_to_bd09(atof(lon_lat.at(0).c_str()), atof(lon_lat.at(1).c_str()), bd09_lon, bd09_lat);
                        coordlist.emplace_back(bd09_lon * 1000000.0, bd09_lat * 1000000.0);
                    }
                }
            }

            std::cout << "match track..." << std::endl;
            MatchPathResult result;
            match_service.RunQuery(coordlist, result);

            std::cout << "insert database track:" << section_code.c_str() << std::endl;
            insert_data(conn, section_code, std::move(result.toSectionInfoJson()), "2025-01-10", std::move(result.toWkt()), std::move(pointsToWkt(coordlist)));

        } catch (const boost::property_tree::json_parser_error& e) {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        }
    }

    // res = PQexec(conn, "COMMIT");
    // if(PQresultStatus(res) != PGRES_COMMAND_OK){
    //    std::cerr << "COMMIT commad failed:" << PQerrorMessage(conn) << std::endl;
    // }
    // PQclear(res);
    PQfinish(conn);
}



int main(int argc, const char *argv[])
{
    if(0){
        std::vector<SectionInfo> list;
        list.emplace_back(0, 1, 10.0);
        list.emplace_back(1, 2, 30.0);
        list.emplace_back(3, 6, 100.0);
        std::string str;
        SECTION_INFO_TO_STR(list, d_list, str);

        std::cout << str.c_str() << std::endl;
    }

    if(0){
        // 数据库连接信息
        std::string conninfo = "host=10.7.68.124 port=5432 dbname=zto_map user=postgres password=zto";

        // 连接数据库
        PGconn* conn = PQconnectdb(conninfo.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
            PQfinish(conn);
            return 1;
        }

        std::vector<std::string> section_code;
        getTableLastSectionCode(conn, section_code);
        std::cout << section_code.size() << std::endl;

        PQfinish(conn);
    }
    if(0){
        downloadData();
        return 1;
    }
    if(0){
        return data_processing(argc, argv);
    }
    if(0){
        // 数据库连接信息
        std::string conninfo = "host=10.7.68.124 port=5432 dbname=zto_map user=postgres password=zto";

        // 连接数据库
        PGconn* conn = PQconnectdb(conninfo.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
            PQfinish(conn);
            return 1;
        }

        // 表名和数据
        std::string road_section_code = "test-test";
        std::string road_info = "{\"d_list\":[{\"s\":80, \"e\":85, \"l\":200.0},{\"s\":95, \"e\":105, \"l\":200.0},{\"s\":106, \"e\":107, \"l\":100.0}],\"r_list\":[{\"s\":85, \"e\":115, \"l\":150.0}],\"s_list\":[{\"s\":90, \"e\":110, \"l\":100.0}]}";
        std::string gmt_create = "2025-01-15";

        // 示例 WKT 格式的 LINESTRING 数据
        std::string bd_geom = "LINESTRING(30 10, 10 30, 40 40)";

        // 插入数据
        insert_data(conn, road_section_code, road_info, gmt_create, bd_geom, "");
        // 插入数据
        insert_data(conn, road_section_code, road_info, gmt_create, bd_geom, "");
        // 插入数据
        insert_data(conn, road_section_code, road_info, gmt_create, bd_geom, "");
        // 插入数据
        insert_data(conn, road_section_code, road_info, gmt_create, bd_geom, "");

        PQfinish(conn);
        return 1;
    }

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << "map_path time\n";
        return 1;
    }

    // 数据库连接信息
#ifdef IS_CENTOS
    const char * conninfo = "host=10.7.176.83 port=5432 dbname=zto_map user=postgres password=zto@666_xt.%888";
#else
    const char * conninfo = "host=10.7.68.124 port=5432 dbname=zto_map user=postgres password=zto";
#endif

    // 连接数据库
    PGconn* conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        return 1;
    }

    getTableLastSectionCode(conn, pre_section_codes);
    PQfinish(conn);

    match_service.initialization(argv[1]);

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "https://zmap-openapi.gw.zt-express.com/iov/getAllSectionCode?ds=20250110");

    request_get_line(buffer, section_codes);

    std::cout <<"Total number:" << section_codes.size() << std::endl;
    std::cout <<"Number of executed items:" << pre_section_codes.size() << std::endl;

    int32_t thread_num = std::thread::hardware_concurrency() / 2 + 1;
    std::cout <<"thread number:" << thread_num << std::endl;
    std::vector<std::thread> thread_group;
    thread_group.reserve(thread_num);
    for(int32_t t = 0; t < thread_num; ++t){
        thread_group.emplace_back(processPoints, conninfo);
    }

    for(int32_t t = 0; t < thread_num; ++t){
        thread_group.at(t).join();
    }
    return 1;
}
