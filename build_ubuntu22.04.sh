#!/bin/bash
script_path="$(dirname "$(readlink -f "$0")")"

build_dir=${script_path}/build
if [ ! -d ${build_dir} ]; then
    mkdir ${build_dir}
fi

cd ${build_dir}

cmake -DCMAKE_BUILD_TYPE=Release ${script_path}
    
make
