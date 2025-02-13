#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <iostream>
#include <string>
#include <curl/curl.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

int http_request(const char * url, std::string & readBuffer);

bool request_get_line(const char * url, std::vector<std::string> &line_ids);
bool request_get_track(const char * url, std::string & track);
#endif
