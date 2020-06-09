#!/bin/bash

find sdcard -type f ! -name '*.txt' -delete

make clean

rm -rf build-munt

pushd external/circle-stdlib
    make mrproper
popd
