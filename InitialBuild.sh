#!/bin/bash
mkdir -p .build/Debug-Linux
(cd .build/Debug-Linux && cmake -DCMAKE_BUILD_TYPE=Debug -DEGAME_CMAKE_DIR=$EGPATH/CMake/Debug-Linux ../..)
cp Inc/Common.hpp .build/Debug-Linux/CMakeFiles/Gravity.pch.dir/Inc/Common.hpp
make -j6 -C .build/Debug-Linux
ln -s $(realpath ./Assets) Bin/Debug-Linux/assets
ln -s $(realpath ./Levels) Bin/Debug-Linux/levels
mkdir -p Bin/Debug-Linux/rt
ln -s $(realpath $EGPATH/Bin/Debug-Linux/libEGame.so) Bin/Debug-Linux/rt/libEGame.so
ln -s $(realpath $EGPATH/Bin/Debug-Linux/libEGameAssetGen.so) Bin/Debug-Linux/rt/libEGameAssetGen.so
