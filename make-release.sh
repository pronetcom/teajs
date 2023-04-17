#!/bin/sh

cp tea deploy/tea
cp libtea.so deploy/libtea.so
cp lib/binary.so deploy/binary.so
cp lib/fs.so deploy/fs.so
cp lib/gd.so deploy/gd.so
cp lib/process.so deploy/process.so
cp lib/pgsql.so deploy/pgsql.so
cp lib/socket.so deploy/socket.so
cp lib/tls.so deploy/tls.so
cp lib/zlib.so deploy/zlib.so
cp teajs.conf deploy/teajs.conf
cp lib/snapshot_blob.bin deploy/snapshot_blob.bin