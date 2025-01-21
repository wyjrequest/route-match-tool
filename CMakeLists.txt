cmake_minimum_required(VERSION 3.14)
project(route-match-tool C CXX)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyxml2/include
)

if(DIS_CENTOS)
    add_definitions(-DIS_CENTOS)
endif()

#set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)  # 强制要求 C++17 标准
#set(CMAKE_CXX_EXTENSIONS OFF)        # 禁用编译器特定扩展

# find_package(PostgreSQL REQUIRED)

# # 包含 PostgreSQL 的头文件
# include_directories(${PostgreSQL_INCLUDE_DIRS})

file(GLOB src
    #${CMAKE_CURRENT_SOURCE_DIR}/src/common.cpp
    #${CMAKE_CURRENT_SOURCE_DIR}/src/databuild.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinyxml2/src/*)

message("CMAKE_BUILD_TYPE:" ${CMAKE_BUILD_TYPE})
add_executable(route-match-tool main.cpp ${src})

target_link_libraries(route-match-tool curl pq -pthread) #${PostgreSQL_LIBRARIES}

## 查找Boost库
#find_package(Boost 1.70.0 COMPONENTS system filesystem REQUIRED)
#
## 如果Boost找到了
#if(Boost_FOUND)
#    include_directories(${Boost_INCLUDE_DIRS})
#    add_executable(osrmpare main.cpp ${src})
#    message("Boost_INCLUDE_DIRS ${Boost_INCLUDE_DIRS}")
#    # 将Boost库链接到目标
#    target_link_libraries(osrmpare  ${Boost_LIBRARIES})
#endif()

