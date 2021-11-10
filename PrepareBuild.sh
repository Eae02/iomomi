#!/bin/bash
make -C Protobuf -j4
(cd Deps/egame && ./PrepareBuild.sh)
