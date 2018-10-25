#!/bin/bash
make -C ${IDF_PATH}/tools/unit-test-app EXTRA_COMPONENT_DIRS="${PWD}/src ${PWD}/src/components" TEST_COMPONENTS=main
