#!/bin/bash

docker run --rm -u $(id -u ${USER}):$(id -g ${USER}) -v $(pwd):/chos -w /chos ipads/chcore_builder:v1.0 ./scripts/compile_user.sh 



