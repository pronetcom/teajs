CPPS=$(wildcard src/*.cc src/lib/*/*.cc)
OBJS=$(CPPS:.cc=.o) $(CPPS:.cc=.os)
DWOBJS=$(CPPS:.cc=.dwo)

uname_m := $(shell uname -m)
arch := $(patsubst aarch64,arm64,$(uname_m:x86_64=x64))
uname_s := $(shell uname -s)
ifeq ($(uname_s),Linux)
    LDFLAGS += -L/usr/lib/$(uname_m)-linux-gnu/ -Wl,-rpath -Wl,.
    LIB_SUFFIX := .so
    FCGI_LIBRARY=-L/usr/lib/$(uname_m)-linux-gnu/
    BS= \
	${V8_BASEDIR}/out/$(arch).release/obj/buildtools/third_party/libc++/libc++/*.o \
	${V8_BASEDIR}/out/$(arch).release/obj/buildtools/third_party/libc++abi/libc++abi/*.o \
	${V8_BASEDIR}/out/$(arch).release/obj/libwee8.a
endif
ifeq ($(uname_s),Darwin)
    LIB_SUFFIX := .dylib
    BS= \
	${V8_BASEDIR}/out/$(arch).release/obj/buildtools/third_party/libc++/libc++/*.o \
	${V8_BASEDIR}/out/$(arch).release/obj/buildtools/third_party/libc++abi/libc++abi/*.o
    ifeq ($(uname_m),arm64)
	CFLAGS += -I/opt/homebrew/opt/openssl/include -I/opt/homebrew/opt/gd/include -isystem/usr/include/ -Dexecvpe=execve
        LDFLAGS += -L/opt/homebrew/opt/openssl/lib -L/opt/homebrew/opt/gd/lib -L/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib/ -liconv
		export PATH="/opt/homebrew/opt/libiconv/bin:$PATH"
    else
	CFLAGS += -I/usr/local/opt/openssl/include -I/usr/local/opt/gd/include -isystem/usr/include/ -Dexecvpe=execve
        LDFLAGS += -L/usr/local/opt/openssl/lib -L/usr/local/opt/gd/lib -L/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib/ -liconv
    endif
endif

## not needed LIBS=-levent -L${V8_BASEDIR}/out/$(arch).debug/ -lv8 -lv8_libplatform -lv8_libbase -licui18n -licuuc -lchrome_zlib -lc++
CPP=${V8_CPP}

FLAGS= $(CFLAGS) -DDSO_EXT=$(subst .,,$(LIB_SUFFIX)) -DHAVE_SLEEP -DHAVE_PTON -DHAVE_NTOP -DHAVE_MMAN_H -DFASTCGI_JS -DVERSION=${TEAJS_VERSION} \
	${D8_DEFINES} ${D8_INCLUDE_DIRS} ${D8_CFLAGS} ${D8_CFLAGS_CC} -I./fcgi \
	-I${TEAJS_BASEDIR}/src ${LIBPQ_INCLUDE} -isystem${TEAJS_BASEDIR}/3rd-party

V8_LIBSPATH=out/$(arch).release/
V8_LIBS=@d8_objects.txt ${D8_ARCHIVES} ${D8_LIBPATHS}

LIBS_ELF=-L/usr/lib/ -L${V8_BASEDIR}/$(V8_LIBSPATH)/obj -L${V8_BASEDIR}/$(V8_LIBSPATH) ${D8_LDFLAGS__} ${D8_LIBS} ${BS} -pthread -ggdb -L. $(LDFLAGS) ${FCGI_LIBRARY} -ltea -lfcgi
LIBS_TEA=-L/usr/lib/ -L${V8_BASEDIR}/$(V8_LIBSPATH)/obj -L${V8_BASEDIR}/$(V8_LIBSPATH) $(V8_LIBS) ${D8_LDFLAGS__} ${D8_LIBS} -pthread -ggdb -L. $(LDFLAGS) ${FCGI_LIBRARY} -lfcgi
LIBS_SO =-L/usr/lib/ -L${V8_BASEDIR}/$(V8_LIBSPATH)/obj -L${V8_BASEDIR}/$(V8_LIBSPATH) ${D8_LDFLAGS__} ${D8_LIBS} ${BS} -pthread -ggdb -L. $(LDFLAGS) ${FCGI_LIBRARY} -ltea -lfcgi

LIBS_PG=$(LDFLAGS) ${LIBPQ_LIBRARY}
LIBS_MEMCACHED=$(LDFLAGS) ${MEMCACHED_LIBRARY}
LIBS_Z=$(LDFLAGS) -lz 
LIBS_TLS=$(LDFLAGS) -lssl -lcrypto
LIBS_GD=$(LDFLAGS) -lgd 
LIBS_CURSES=$(LDFLAGS) -lncurses

ifeq ($(MEMCACHED_LIBRARY),)
all: tea libtea$(LIB_SUFFIX) lib/binary$(LIB_SUFFIX) lib/fs$(LIB_SUFFIX) lib/gd$(LIB_SUFFIX) lib/process$(LIB_SUFFIX) lib/pgsql$(LIB_SUFFIX) lib/socket$(LIB_SUFFIX) lib/tls$(LIB_SUFFIX) lib/zlib$(LIB_SUFFIX) lib/curses$(LIB_SUFFIX) teajs.conf lib/snapshot_blob.bin
else
all: tea libtea$(LIB_SUFFIX) lib/binary$(LIB_SUFFIX) lib/fs$(LIB_SUFFIX) lib/gd$(LIB_SUFFIX) lib/process$(LIB_SUFFIX) lib/pgsql$(LIB_SUFFIX) lib/socket$(LIB_SUFFIX) lib/tls$(LIB_SUFFIX) lib/zlib$(LIB_SUFFIX) lib/curses$(LIB_SUFFIX) lib/memcached$(LIB_SUFFIX) teajs.conf lib/snapshot_blob.bin
endif

lib/snapshot_blob.bin: ${V8_COMPILEDIR}/snapshot_blob.bin
	cp ${V8_COMPILEDIR}/snapshot_blob.bin lib/snapshot_blob.bin

teajs.conf: teajs.conf.tmpl
	cat teajs.conf.tmpl | sed "s+LIBPATH+${TEAJS_LIBPATH}+" > teajs.conf

install:
	@if ! [ "$(shell id -u)" = 0 ];then echo "You are not root, run install as root please";exit 1;fi
	rm -f ${INSTALL_ROOT}/usr/local/bin/tea
	rm -f ${INSTALL_ROOT}/usr/local/bin/snapshot_blob.bin
	rm -f ${INSTALL_ROOT}/usr/local/lib/libtea$(LIB_SUFFIX)
	rm -rf ${INSTALL_ROOT}/usr/local/lib/teajs
	cp tea ${INSTALL_ROOT}/usr/local/bin/
	#cp snapshot_blob.bin /usr/local/bin/
	cp libtea$(LIB_SUFFIX) ${INSTALL_ROOT}/usr/local/lib
	mkdir ${INSTALL_ROOT}/usr/local/lib/teajs
	cp lib/* ${INSTALL_ROOT}/usr/local/lib/teajs/
	cat teajs.conf.tmpl | sed "s+LIBPATH+/usr/local/lib/teajs/+" > ${INSTALL_ROOT}/etc/teajs.conf
	ldconfig

remoteinstall:
	ssh root@${REMOTE} rm -f ${INSTALL_ROOT}/usr/local/bin/tea
	ssh root@${REMOTE} rm -f ${INSTALL_ROOT}/usr/local/lib/libtea$(LIB_SUFFIX)
	ssh root@${REMOTE} rm -rf ${INSTALL_ROOT}/usr/local/lib/teajs
	ssh root@${REMOTE} rm -rf ${INSTALL_ROOT}/usr/local/lib/snapshot_blob.bin
	scp tea root@${REMOTE}:${INSTALL_ROOT}/usr/local/bin/
	scp libtea$(LIB_SUFFIX) root@${REMOTE}:${INSTALL_ROOT}/usr/local/lib
	ssh root@${REMOTE} mkdir ${INSTALL_ROOT}/usr/local/lib/teajs
	scp lib/* root@${REMOTE}:${INSTALL_ROOT}/usr/local/lib/teajs/
	cat teajs.conf.tmpl | sed "s+LIBPATH+/usr/local/lib/teajs/+" > /tmp/teajs.conf
	scp /tmp/teajs.conf root@${REMOTE}:${INSTALL_ROOT}/etc/teajs.conf
	rm -rf /tmp/teajs.conf
	ssh root@${REMOTE} ldconfig

clean:
	rm -rf 3rd-party
	rm -f tea
	rm -f build-tea.log
	rm -f d8_objects.txt
	rm -f teajs.conf
	rm -f libtea$(LIB_SUFFIX)
	rm -f snapshot_blob.bin
	rm -f lib/snapshot_blob.bin
	rm -f $(OBJS)
	rm -f $(DWOBJS)
	rm -f lib/*$(LIB_SUFFIX)

tea: src/teajs.o libtea$(LIB_SUFFIX)
	$(CPP) -o $@ src/teajs.o $(LIBS_ELF)

libtea$(LIB_SUFFIX): src/common.o src/system.o src/cache.o src/gc.o src/app.o src/path.o src/lib/binary/bytestorage.o
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_TEA)

%.o: %.cc
	$(CPP) $(FLAGS) -c -o $@ $<

lib/binary$(LIB_SUFFIX): src/lib/binary/binary.o src/lib/binary/bytestorage.o libtea$(LIB_SUFFIX)
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO)

lib/fs$(LIB_SUFFIX): src/lib/fs/fs.o src/path.o src/lib/binary/bytestorage.o libtea$(LIB_SUFFIX)
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO)

lib/gd$(LIB_SUFFIX): src/lib/gd/gd.o libtea$(LIB_SUFFIX)
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO) $(LIBS_GD)

lib/process$(LIB_SUFFIX): src/lib/process/process.o libtea$(LIB_SUFFIX)
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO)

lib/memcached$(LIB_SUFFIX): src/lib/memcached/memcached.o libtea$(LIB_SUFFIX)
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO) $(LIBS_MEMCACHED)

lib/socket$(LIB_SUFFIX): src/lib/socket/socket.o libtea$(LIB_SUFFIX)
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO)

lib/pgsql$(LIB_SUFFIX): src/lib/pgsql/pgsql.o libtea$(LIB_SUFFIX)
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO) $(LIBS_PG)

lib/tls$(LIB_SUFFIX): src/lib/tls/tls.o libtea$(LIB_SUFFIX)
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO) $(LIBS_TLS)

lib/zlib$(LIB_SUFFIX): src/lib/zlib/zlib.o libtea$(LIB_SUFFIX)
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO) $(LIBS_Z)

lib/curses$(LIB_SUFFIX): src/lib/curses/curses.o libtea$(LIB_SUFFIX)
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO) $(LIBS_CURSES)
