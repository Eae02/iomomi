#!/bin/bash
PROJECT_ROOT=$(realpath .)
g++ -std=gnu++17 -g -pedantic -isystem "$PROJECT_ROOT/Ext/glm/glm" -isystem "$PROJECT_ROOT/Inc" \
-DGLM_FORCE_RADIANS -DGLM_FORCE_CTOR_INIT -DGLM_ENABLE_EXPERIMENTAL Inc/Common.hpp -o Inc/Common.hpp.gch
