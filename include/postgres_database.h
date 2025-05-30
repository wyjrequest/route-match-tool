#ifndef POSTGRES_DATABASE_H
#define POSTGRES_DATABASE_H
#include "match.h"

#include <string>
#include <vector>
#include <iostream>
#include <atomic>
#include <thread>

#if IS_CENTOS
#include <libpq-fe.h>
#else
#include <postgresql/libpq-fe.h>
#endif

void download_data_iov();
void download_shigong_shougong_data();
void download_data_new(const std::string & table_name, int32_t max_row_nuber);
void download_data(const std::string & table_name, int32_t max_row_nuber);
void download_shigong_data(const std::string & table_name, int32_t max_row_nuber);
void download_data_road_all(const std::string & table_name, int32_t max_row_nuber = 1000);
void getTableLastSectionCode(PGconn* conn, std::vector<std::string > & section_code);
void insert_data(PGconn* conn, const std::string & road_section_code, const std::string& road_info, const std::string& gmt_create, const std::string& bd_geom, const std::string& st_geom, const std::string& st_geom_bd09);
void write_data_dw_gis_fence_position_version_index_all2(const std::string & filename);
void insert_data_dw_gis_fence_position_version_index_all2(PGconn* conn,
                 const char * areaid,
                 const char * areaname,
                 const char * center,
                 const char * layer,
                 const char * parentid,
                 const char * version,
                 const char * isdeleted,
                 const char * geom,
                 const char * geom_bd09);

void processdw_gis_fence_position_version_index_all();
void load_high_incidence_of_accidents(const std::string &tn, std::vector<std::shared_ptr<AccidentInfo>> & ai_list);
#endif
