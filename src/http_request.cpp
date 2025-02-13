#include "http_request.h"

// 回调函数，用于处理服务器响应
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    ((std::string*)userp)->append((char*)contents, totalSize);
    return totalSize;
}

int http_request(const char * url, std::string & readBuffer)
{
    // 初始化 libcurl
    CURL* curl;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
        // 设置 URL
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // 创建一个结构体用于设置 HTTP 请求头
        struct curl_slist *headers = NULL;

        // 添加自定义 HTTP 请求头
        headers = curl_slist_append(headers, "x-dubbo-tag:ztdtsjfbptv136fat");

        // 设置请求头
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // 设置回调函数处理响应内容
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // 发送 GET 请求
        res = curl_easy_perform(curl);

        // 检查请求是否成功
        if(res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        } else {
            // // 打印响应内容
            // std::cout << "Response: " << std::endl;
            // std::cout << readBuffer << std::endl;
        }
        // 清理
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return 0;
}

bool request_get_line(const char * url, std::vector<std::string> &line_ids){
    std::string readbuff;
    http_request(url, readbuff);

    // 使用 Boost.PropertyTree 解析 JSON 数据
    std::istringstream json_stream(readbuff);
    boost::property_tree::ptree root;

    try {
        // 解析 JSON 数据
        boost::property_tree::read_json(json_stream, root);

        // 使用 count() 判断键是否存在
        if (root.count("statusCode") <= 0) {
            std::cout << "Key 'statusCode' does not exist" << std::endl;
            return false;
        }

        // 提取数据
        std::string statusCode = root.get<std::string>("statusCode");
        if(statusCode.compare("SYS000") != 0){
            std::cout << "Error: request fail, " << url << std::endl;
            return false;
        }

        // 提取 phone_numbers 数组
        boost::property_tree::ptree lines = root.get_child("result");
        std::cout << "lines: ";
        for (const auto& entry : lines) {
            line_ids.emplace_back(entry.second.get<std::string>(""));
            std::cout << line_ids.back() << " ";
        }
        std::cout << std::endl;
    } catch (const boost::property_tree::json_parser_error& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        return false;
    }
    return true;
}


bool request_get_track(const char * url, std::string & track){
    std::string readbuff;
    http_request(url, readbuff);

    // 使用 Boost.PropertyTree 解析 JSON 数据
    std::istringstream json_stream(readbuff);
    boost::property_tree::ptree root;

    try {
        // 解析 JSON 数据
        boost::property_tree::read_json(json_stream, root);

        // 使用 count() 判断键是否存在
        if (root.count("statusCode") <= 0) {
            std::cout << "Key 'statusCode' does not exist" << std::endl;
            return false;
        }

        // 提取数据
        std::string statusCode = root.get<std::string>("statusCode");
        if(statusCode.compare("SYS000") != 0){
            std::cout << "Error: request fail, " << url << std::endl;
            return false;
        }

        track = root.get<std::string>("result");
        // std::cout << track.c_str() << std::endl;
    } catch (const boost::property_tree::json_parser_error& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        return false;
    }
    return true;
}
