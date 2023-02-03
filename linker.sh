#!/bin/sh

/download/v8-build/v8/third_party/llvm-build/Release+Asserts/bin/clang++ -v -fPIC src/common.os src/system.os src/cache.os src/gc.os src/app.os src/path.os src/lib/binary/bytestorage.os src/teajs.o  -levent -L/download/v8-build/v8/out/x64.debug/ -lv8 -lv8_libplatform -lv8_libbase -licui18n -licuuc -lchrome_zlib -lc++ -ldl -o teajs
