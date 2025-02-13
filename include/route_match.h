#ifndef ROUTE_MATCH_H
#define ROUTE_MATCH_H

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

void download_data(const std::string & table_name, int32_t max_row_nuber = 1000);
int  route_match_processing(int argc, const char *argv[]);

#endif
