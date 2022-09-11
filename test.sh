#!/bin/bash

#set -e

export LD_LIBRARY_PATH=./:lib/
export TEAJS_CONF_PATH=./teajs.conf
export TEAJS_BLOB_PATH=lib/snapshot_blob.bin
#gdb ./tea
#exit

if [ "y$1" != "y" ]; then
	./tea unit/runner.js $1
	exit
fi
echo Running unit/hello_world.js
./tea unit/helloworld.js

echo Running unit/runner.js
./tea unit/runner.js unit/tests

echo "THIS MUST FAIL (uncatched throw)"
echo "Running unit/throw.js"
./tea unit/throw.js

echo "THIS MUST FAIL (uncatched syntax error)"
echo "Running unit/syntax_error.js"
./tea unit/syntax_error.js
