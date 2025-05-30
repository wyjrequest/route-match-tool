#include "postgres_database.h"

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

void download_data_iov(){
    // 数据库连接信息
    std::string conninfo = "host=10.7.176.83 port=5432 dbname=zto_map user=postgres password=zto@666_xt.%888";

    // 连接数据库
    PGconn* conn = PQconnectdb(conninfo.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        return;
    }

    std::cout << "Connected to database successfully!" << std::endl;

    // 构造查询语句
    std::string query = "SELECT count(x.*) FROM public.iov_track_road_info x where x.ds = '20250408'";

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

    std::ofstream file("iov_track_road_info.txt");
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing!" << std::endl;
        PQclear(res);
        PQfinish(conn);
        return;
    }

    int32_t index = 0;
    do
    {
        // 执行查询
        query = "SELECT x.road_section_code,x.road_info FROM public.iov_track_road_info x where x.ds = '20250408'";

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
            }
        }
        // 清理资源
        PQclear(res);
    }
    while(false);

    file.close();
    PQfinish(conn);
}

void download_data_road_all(const std::string & table_name, int32_t max_row_nuber){
    // 数据库连接信息
    std::string conninfo = "host=10.1.171.186 port=5432 dbname=v2_tenxun_line user=postgres password=zto";

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
            query = "SELECT x.id,x.\"name\",ST_AsText(x.line_geom),x.ad_code,x.form_way,x.road_class,x.level_layer,x.direction,x.width,x.toll,x.status,x.elevated,x.angle_differ FROM public." + table_name + " x order by x.id LIMIT "+std::to_string(max_row_nuber)+" OFFSET 0";
        }else{
            query = "SELECT x.id,x.\"name\",ST_AsText(x.line_geom),x.ad_code,x.form_way,x.road_class,x.level_layer,x.direction,x.width,x.toll,x.status,x.elevated,x.angle_differ FROM public." + table_name + " x where x.id > '"+ link_id_max +"' order by x.id LIMIT "+std::to_string(max_row_nuber);
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

void download_data_new(const std::string & table_name, int32_t max_row_nuber){
    // 数据库连接信息
    std::string conninfo = "host=10.1.171.186 port=5432 dbname=v2_tenxun_line user=postgres password=zto";

    // 连接数据库
    PGconn* conn = PQconnectdb(conninfo.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        return;
    }

    std::cout << "Connected to database successfully!" << std::endl;

    // 构造查询语句
    std::string query = "SELECT COUNT(*) FROM public."+table_name+" where road_class in ('高速公路','主要大街，城市快速路','国道','省道','县道') and (min_distance is null or min_distance <> 0);";

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
            query = "SELECT x.id,x.\"name\",ST_AsText(x.line_geom),x.ad_code,x.form_way,x.road_class,x.level_layer,x.direction,x.toll,x.status,x.elevated,x.angle_differ FROM public." + table_name + " x where x.road_class in ('高速公路','主要大街，城市快速路','国道','省道','县道') and (x.min_distance is null or x.min_distance <> 0) order by x.id LIMIT "+std::to_string(max_row_nuber)+" OFFSET 0";
        }else{
            query = "SELECT x.id,x.\"name\",ST_AsText(x.line_geom),x.ad_code,x.form_way,x.road_class,x.level_layer,x.direction,x.toll,x.status,x.elevated,x.angle_differ FROM public." + table_name + " x where x.road_class in ('高速公路','主要大街，城市快速路','国道','省道','县道') and (x.min_distance is null or x.min_distance <> 0) and x.id > '"+ link_id_max +"' order by x.id LIMIT "+std::to_string(max_row_nuber);
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

void download_data(const std::string & table_name, int32_t max_row_nuber){
    // 数据库连接信息
    std::string conninfo = "host=10.1.171.71 port=5434 dbname=zto_deal user=postgres password=zto";
    // std::string conninfo = "host=10.1.171.71 port=5434 dbname=zto_nav_v3_gc_motorway user=postgres password=zto";
    // std::string conninfo = "host=10.1.171.71 port=5434 dbname=zto_deal user=postgres password=zto";

    // std::string conninfo = "host=10.1.171.39 port=5432 dbname=tenxun_line user=postgres password=zto";

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

        // if(link_id_max.empty()){
        //     query = "SELECT x.gl_seg_id,x.direction,x.form_way,x.road_class,ST_AsText(x.line_geom),x.toll,x.\"name\" FROM public." + table_name + " x order by x.gl_seg_id LIMIT "+std::to_string(max_row_nuber)+" OFFSET 0";
        // }else{
        //     query = "SELECT x.gl_seg_id,x.direction,x.form_way,x.road_class,ST_AsText(x.line_geom),x.toll,x.\"name\" FROM public." + table_name + " x where x.gl_seg_id > "+ link_id_max +" order by x.gl_seg_id LIMIT "+std::to_string(max_row_nuber);
        // }

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

void download_shigong_shougong_data(){
    // 数据库连接信息
    std::string conninfo = "host=10.1.171.186 port=5432 dbname=gaode_guide user=postgres password=zto";

    // 连接数据库
    PGconn* conn = PQconnectdb(conninfo.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        return;
    }

    std::cout << "Connected to database successfully!" << std::endl;

    // 构造查询语句
    std::string query = "SELECT count(x.*) FROM public.v2_roads_china_gaosu_shigong x";

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

    std::ofstream file("v2_roads_china_gaosu_shigong.txt");
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing!" << std::endl;
        PQclear(res);
        PQfinish(conn);
        return;
    }

    int32_t index = 0;
    do
    {
        // 执行查询
        query = "SELECT ST_AsText(x.line_geom) from public.v2_roads_china_gaosu_shigong x";

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
            }
        }
        // 清理资源
        PQclear(res);
    }
    while(false);

    file.close();
    PQfinish(conn);
}

void download_shigong_data(const std::string & table_name, int32_t max_row_nuber){
    // 数据库连接信息
    // std::string conninfo = "host=10.1.171.71 port=5434 dbname=tenxun_line_shoudong user=postgres password=zto";
    std::string conninfo = "host=10.7.80.34 port=5432 dbname=postgres user=postgres password=zto";

    // 连接数据库
    PGconn* conn = PQconnectdb(conninfo.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        return;
    }

    std::cout << "Connected to database successfully!" << std::endl;

    // 构造查询语句
    std::string query = "SELECT COUNT(x.*) FROM public."+table_name+" x where (x.collect_time = 1745822924 and x.description like '%车道限制%' and (x.description like '%高速%' or x.description like '%互通%' or x.description like '%环线%'))  or  x.is_gaosu=1;";

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
        if(link_id_max.empty()){
            query = "SELECT x.id,ST_AsText(x.point),ST_AsText(x.to_point) FROM public." + table_name + " x where (x.collect_time = 1745822924 and x.description like '%车道限制%' and (x.description like '%高速%' or x.description like '%互通%' or x.description like '%环线%'))  or  x.is_gaosu=1 order by x.id LIMIT "+std::to_string(max_row_nuber)+" OFFSET 0";
        }else{
            query = "SELECT x.id,ST_AsText(x.point),ST_AsText(x.to_point) FROM public." + table_name + " x where (x.collect_time = 1745822924 and x.description like '%车道限制%' and (x.description like '%高速%' or x.description like '%互通%' or x.description like '%环线%'))  or  x.is_gaosu=1 and x.id > "+ link_id_max +" order by x.id LIMIT "+std::to_string(max_row_nuber);
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

void insert_data(PGconn* conn, const std::string & road_section_code, const std::string& road_info, const std::string& gmt_create, const std::string& bd_geom, const std::string& st_geom, const std::string& st_geom_bd09) {
    // std::cout << "Connected to database successfully!" << std::endl;

    // track_to_road_test
    // 构造 SQL 插入语句
    std::string query = "INSERT INTO iov_track_road_info (road_section_code, road_info, gmt_create, bd_geom, standard_geom, standard_geom_bd09, ds) VALUES "
                        "($1::varchar(200), $2::json, TO_TIMESTAMP($3, 'YYYYMMDD'), ST_GeomFromText($4, 4326), ST_GeomFromText($5, 4326), ST_GeomFromText($6, 4326), $7::varchar(200)) "
                        "ON CONFLICT (road_section_code) "
                        "DO UPDATE SET "
                        "road_info = EXCLUDED.road_info, "
                        "gmt_create = EXCLUDED.gmt_create, "
                        "bd_geom = EXCLUDED.bd_geom, "
                        "standard_geom = EXCLUDED.standard_geom, "
                        "standard_geom_bd09 = EXCLUDED.standard_geom_bd09, "
                        "ds = EXCLUDED.ds;";

    // 设置参数
    const char* paramValues[7] = {road_section_code.c_str(), road_info.c_str(), gmt_create.c_str(), bd_geom.c_str(), st_geom.c_str(), st_geom_bd09.c_str(), gmt_create.c_str()};

    // 执行插入
    PGresult* res = PQexecParams(
        conn,
        query.c_str(),   // SQL 查询
        7,               // 参数个数
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

void write_data_dw_gis_fence_position_version_index_all2(const std::string & filename){
    std::ifstream file(filename);
    if (file.is_open()) {
        std::string conninfo = "host=10.1.171.186 port=5432 dbname=line_build user=postgres password=zto";
        PGconn* conn = PQconnectdb(conninfo.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
            PQfinish(conn);
            return;
        }

        const int32_t byte_size = 1024;
        char buffer[byte_size];
        memset(buffer, 0, byte_size);
        snprintf(buffer, sizeof(buffer), "COPY dw_gis_fence_position_version_index_all2 (areaid, areaname, center, layer, parentid, version, isdeleted, geom, geom_bd09) FROM STDIN WITH CSV DELIMITER ';';");

        PGresult* res = PQexec(conn, buffer);
        if (PQresultStatus(res) != PGRES_COPY_IN) {
           std::cerr << "Failed to start COPY operation: " << PQerrorMessage(conn) << std::endl;
           PQclear(res);
           PQfinish(conn);
           return;
        }
        PQclear(res);

        std::cout << "<< write_database: 0%" << std::endl;

        std::string line_str;
        while(std::getline(file, line_str)){
            if (!line_str.empty() && line_str.back() == '\r') {
                line_str.pop_back();
            }
            if (PQputCopyData(conn, line_str.c_str(), line_str.size()) != 1) { //
                std::cerr << "Failed to send data: " << PQerrorMessage(conn) << std::endl;
                PQputCopyEnd(conn, nullptr); // 结束COPY操作，即使有错误发生。
                PQfinish(conn);
                return;
            }
        }
        // 完成 COPY 操作
        PQputCopyEnd(conn, nullptr);

        // 检查 COPY 操作的结果
        res = PQgetResult(conn);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::cerr << "COPY failed: " << PQerrorMessage(conn) << std::endl;
        } else {
            std::cout << "Bulk insert successful using COPY." << std::endl;
        }
        PQclear(res);
        PQfinish(conn);
        file.close();
    }
}

void insert_data_dw_gis_fence_position_version_index_all2(PGconn* conn,
                 const char * areaid,
                 const char * areaname,
                 const char * center,
                 const char * layer,
                 const char * parentid,
                 const char * version,
                 const char * isdeleted,
                 const char * geom,
                 const char * geom_bd09) {

    // 构造 SQL 插入语句
    std::string query = "INSERT INTO dw_gis_fence_position_version_index_all2 (areaid, areaname, center, layer, parentid, geom, geom_bd09, version, isdeleted) VALUES "
                        "($1::varchar(50), $2::varchar(50), ST_GeomFromText($3, 4326), $4::int4, $5::varchar(50), ST_GeomFromText($6, 4326), ST_GeomFromText($7, 4326), $8::int4, $9::int4)";

    // 设置参数
    const char* paramValues[9] = {areaid,
                                  areaname,
                                  center,
                                  layer,
                                  parentid,
                                  geom,
                                  geom_bd09,
                                  version,
                                  isdeleted};

    // 执行插入
    PGresult* res = PQexecParams(
        conn,
        query.c_str(),   // SQL 查询
        9,               // 参数个数
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

void processdw_gis_fence_position_version_index_all(){
    // 数据库连接信息
    std::string conninfo = "host=10.1.171.186 port=5432 dbname=line_build user=postgres password=zto";

    // 连接数据库
    PGconn* conn = PQconnectdb(conninfo.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        return;
    }

    std::cout << "Connected to database successfully!" << std::endl;

    // 构造查询语句
    std::string query = "SELECT COUNT(*) FROM public.dw_gis_fence_position_version_index_all;";

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

    std::ofstream file("dw_gis_fence_position_version_index_all.txt");
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing!" << std::endl;
        PQclear(res);
        PQfinish(conn);
        return;
    }

    std::cout << "总行数:" << row_count << std::endl;

    std::string link_id_max = "";

    int32_t index = 0;
    do
    {
        if(link_id_max.empty()){
            query = "SELECT x.areaid,x.areaname,ST_AsText(x.center),x.layer,x.parentid,x.\"version\",x.isdeleted,ST_AsText(x.geom) FROM public.dw_gis_fence_position_version_index_all x order by x.areaid LIMIT 10000 OFFSET 0";
            // query = "SELECT x.id,ST_AsText(x.line_geom) FROM public." + table_name + " x order by x.id LIMIT "+std::to_string(max_row_nuber)+" OFFSET 0";
        }else{
            // query = "SELECT x.id,ST_AsText(x.line_geom) FROM public." + table_name + " x where x.id > "+ link_id_max +" order by x.id LIMIT "+std::to_string(max_row_nuber);
            query = "SELECT x.areaid,x.areaname,ST_AsText(x.center),x.layer,x.parentid,x.\"version\",x.isdeleted,ST_AsText(x.geom) FROM public.dw_gis_fence_position_version_index_all x where x.areaid > '"+ link_id_max +"' order by x.areaid LIMIT 10000";
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
            std::cout << ((index / 10000) + 1) << std::endl;

            // int cols = PQnfields(res);
            // 输出每一行数据
            for (int i = 0; i < rows; ++i) {
                const std::string geom = PQgetvalue(res, i, 7);

                std::vector<std::string> wkt_strs;
                get_wkt_str(geom, wkt_strs);

                for(auto wkt : wkt_strs){
                    std::vector<Point2D> line;
                    wkt_str_to_geometry(wkt, line);

                    if(line.size() > 3){
                        for(auto & pt : line){
                            double bd09_lon, bd09_lat;
                            gcj02_to_bd09(pt.toFloatLon(), pt.toFloatLat(), bd09_lon, bd09_lat);
                            pt.x = bd09_lon * 1000000.0;
                            pt.y = bd09_lat * 1000000.0;
                        }
                        wkt = "POLYGON((" + wkt + "))";
                        pointsToPolygonWkt(line).c_str();

                        file << PQgetvalue(res, i, 0) << ';'
                             << PQgetvalue(res, i, 1) << ';'
                             << PQgetvalue(res, i, 2) << ';'
                             << PQgetvalue(res, i, 3) << ';'
                             << PQgetvalue(res, i, 4) << ';'
                             << PQgetvalue(res, i, 5) << ';'
                             << PQgetvalue(res, i, 6) << ';'
                             << wkt.c_str() << ';'
                             << pointsToPolygonWkt(line).c_str() << std::endl;
                    }
                }

                if(i == rows - 1){
                    link_id_max = PQgetvalue(res, i, 0);
                }
            }
        }
        // 清理资源
        PQclear(res);

        if(rows < 10000){
            break;
        }
        index += 10000;
    }
    while(true);

    file.close();
    PQfinish(conn);

    write_data_dw_gis_fence_position_version_index_all2("dw_gis_fence_position_version_index_all.txt");
}

void load_high_incidence_of_accidents(const std::string &tn, std::vector<std::shared_ptr<AccidentInfo>> & ai_list){
    // 数据库连接信息
    std::string conninfo = "host=10.7.80.34 port=5432 dbname=road_info user=postgres password=zto";

    // 连接数据库
    PGconn* conn = PQconnectdb(conninfo.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
        PQfinish(conn);
        return;
    }

    std::cout << "Connected to database successfully!" << std::endl;

    std::string query = "SELECT COUNT(*) FROM public." + tn+ " where yuying_text like '%事故%';";
    PGresult* res = PQexec(conn, query.c_str());
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        PQfinish(conn);
        return;
    }

    const int32_t count = atoi(PQgetvalue(res, 0, 0));
    std::cout << "总行数:" << count << std::endl;
    PQclear(res);

    ai_list.reserve(count);

    const int32_t def_row_num = 1000;

    std::string id = "";
    int32_t index = 0;
    int32_t rows = 0;

    do
    {
        if(id.empty()){
            query = "SELECT x.id, x.coord, x.bearing, x.yuying_text from "+tn+" x where x.yuying_text like '%事故%' order by x.id LIMIT "+std::to_string(def_row_num)+" OFFSET 0";
        }else{
            query = "SELECT x.id, x.coord, x.bearing, x.yuying_text from "+tn+" x where x.yuying_text like '%事故%' and x.id > "+ id +" order by x.id LIMIT "+std::to_string(def_row_num);
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
        rows = PQntuples(res);

        if (rows == 0) {
            std::cout << "No records to fetch." << std::endl;
        } else {
            std::cout << index + 1 << std::endl;
            // const int32_t cols = PQnfields(res);

            // 输出每一行数据
            for (int32_t i = 0; i < rows; ++i) {
                const char * coord = PQgetvalue(res, i, 1);
                const char * bearing = PQgetvalue(res, i, 2);
                const std::string yuying_text = PQgetvalue(res, i, 3);

                std::vector<std::string> lon_lat;
                splitstring(coord, ',', lon_lat);

                if(lon_lat.size() == 2){
                    double lon = atof(lon_lat.at(0).c_str());
                    double lat = atof(lon_lat.at(1).c_str());
                    const Point2D pt(lon * 1000000.0, lat * 1000000.0);

                    const double b = atoi(bearing);
                    const double f = (yuying_text.find("前方经过") == std::string::npos) ? 500 : 200;
                    auto ai = std::make_shared<AccidentInfo>(std::move(pt), b, f);
                    ai_list.emplace_back(ai);
                }
            }

            id = PQgetvalue(res, rows - 1, 0);
        }

        index += rows;
        PQclear(res);
    }
    while(rows == def_row_num);
    PQfinish(conn);
}
