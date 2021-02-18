#!/bin/bash
mkdir -p .build/Debug-Linux
(cd .build/Debug-Linux && cmake -DCMAKE_BUILD_TYPE=Debug -DEGAME_CMAKE_DIR=$EGPATH/CMake/Debug-Linux ../..)
make -j6 -C .build/Debug-Linux
mkdir -p Bin/Debug-Linux/rt
ln -s $(realpath $EGPATH/Bin/Debug-Linux/libEGame.so) Bin/Debug-Linux/rt/libEGame.so
ln -s $(realpath $EGPATH/Bin/Debug-Linux/libEGameAssetGen.so) Bin/Debug-Linux/rt/libEGameAssetGen.so
