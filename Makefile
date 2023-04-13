CPPS=$(wildcard src/*.cc src/lib/*/*.cc)
OBJS=$(CPPS:.cc=.o) $(CPPS:.cc=.os)
DWOBJS=$(CPPS:.cc=.dwo)

## not needed LIBS=-levent -L${V8_BASEDIR}/out/x64.debug/ -lv8 -lv8_libplatform -lv8_libbase -licui18n -licuuc -lchrome_zlib -lc++
CPP=${V8_CPP}

#FLAGS= -DDSO_EXT=so -DHAVE_SLEEP -DHAVE_PTON -DHAVE_NTOP -DHAVE_MMAN_H -DFASTCGI_JS -DVERSION=${TEAJS_VERSION} ${FCGI_INCLUDE}
FLAGS= -DDSO_EXT=so -DHAVE_SLEEP -DHAVE_PTON -DHAVE_NTOP -DHAVE_MMAN_H -DFASTCGI_JS -DVERSION=${TEAJS_VERSION} \
	${D8_DEFINES} ${D8_INCLUDE_DIRS} ${D8_CFLAGS} ${D8_CFLAGS_CC} \
	-I${TEAJS_BASEDIR}/src ${LIBPQ_INCLUDE} -isystem${TEAJS_BASEDIR}/3rd-party #-isystem/usr/include/



#V8_LIBSPATH=out.gn/x64.release.sample/obj/
#V8_LIBS=-lv8_libplatform -lv8_monolith
V8_LIBSPATH=out/x64.release/
#V8_LIBS=-lv8_libplatform -lv8
V8_LIBS=@d8_objects.txt ${D8_ARCHIVES} ${D8_LIBPATHS}
#V8_LIBS=${D8_ARCHIVES} -L/home/vahvarh/try_teajs/v8_things/v8/out/x64.release/obj/third_party/zlib/ -L/home/vahvarh/try_teajs/v8_things/v8/out/x64.release/obj/third_party/zlib/google

LIBS_ELF=-L/usr/lib/ -L${V8_BASEDIR}/$(V8_LIBSPATH)/obj -L${V8_BASEDIR}/$(V8_LIBSPATH) $(V8_LIBS) ${D8_LDFLAGS__} ${D8_LIBS} -pthread -ggdb -L. -L/usr/lib/x86_64-linux-gnu/ ${FCGI_LIBRARY} -ltea
LIBS_SO =-L/usr/lib/ -L${V8_BASEDIR}/$(V8_LIBSPATH)/obj -L${V8_BASEDIR}/$(V8_LIBSPATH) $(V8_LIBS) ${D8_LDFLAGS__} ${D8_LIBS} -pthread -ggdb -L. -L/usr/lib/x86_64-linux-gnu/ ${FCGI_LIBRARY}

#LIBS_ELF=-L. -L/usr/lib/x86_64-linux-gnu/ -L/usr/lib/x86_64-linux-gnu -L${V8_BASEDIR}/out/x64.debug -lpthread -lv8 -lv8_libplatform -ldl -ltea -pthread -std=${STD_CPP} -ggdb -lfcgi
#LIBS_SO=-L. -L/usr/lib/x86_64-linux-gnu/ -L/usr/lib/x86_64-linux-gnu -L${V8_BASEDIR}/out/x64.debug -lpthread -lv8 -lv8_libplatform -ldl -ltea -pthread -std=${STD_CPP} -ggdb -lfcgi




#LIBS_PG=-lpgcommon_shlib -lpgport_shlib -lpthread -lselinux -lxslt -lxml2 -lpam -lssl -lcrypto -lgssapi_krb5 -lpq -lz -lrt -ldl -lm -L/usr/lib/postgresql/13/lib -L${V8_BASEDIR}/build/linux/debian_sid_amd64-sysroot/usr/lib/x86_64-linux-gnu/ -L/usr/lib/x86_64-linux-gnu/


LIBS_PG=-L/usr/lib/x86_64-linux-gnu/ ${LIBPQ_LIBRARY}
LIBS_MEMCACHED=-L/usr/lib/x86_64-linux-gnu/ ${MEMCACHED_LIBRARY}
#LIBS_PG=-L/usr/lib/x86_64-linux-gnu/ -L/usr/lib/postgresql/13/lib -lpgport_shlib -lpgcommon_shlib ${LIBPQ_LIBRARY}
LIBS_Z=-L/usr/lib/x86_64-linux-gnu/ -lz
LIBS_TLS=-L/usr/lib/x86_64-linux-gnu/ -lssl -lcrypto
LIBS_GD=-L/usr/lib/x86_64-linux-gnu/ -lgd


#all: lib/binary.so
all: tea libtea.so lib/binary.so lib/fs.so lib/gd.so lib/process.so lib/pgsql.so lib/socket.so lib/tls.so lib/zlib.so lib/memcached.so teajs.conf lib/snapshot_blob.bin

lib/snapshot_blob.bin: ${V8_COMPILEDIR}/snapshot_blob.bin
	cp ${V8_COMPILEDIR}/snapshot_blob.bin lib/snapshot_blob.bin

teajs.conf: teajs.conf.tmpl
	cat teajs.conf.tmpl | sed "s+LIBPATH+${TEAJS_LIBPATH}+" > teajs.conf

install:
	@if ! [ "$(shell id -u)" = 0 ];then echo "You are not root, run install as root please";exit 1;fi
	rm -f ${INSTALL_ROOT}/usr/local/bin/tea
	rm -f ${INSTALL_ROOT}/usr/local/bin/snapshot_blob.bin
	rm -f ${INSTALL_ROOT}/usr/local/lib/libtea.so
	rm -rf ${INSTALL_ROOT}/usr/local/lib/teajs
	cp tea ${INSTALL_ROOT}/usr/local/bin/
	#cp snapshot_blob.bin /usr/local/bin/
	cp libtea.so ${INSTALL_ROOT}/usr/local/lib
	mkdir ${INSTALL_ROOT}/usr/local/lib/teajs
	cp lib/* ${INSTALL_ROOT}/usr/local/lib/teajs/
	cat teajs.conf.tmpl | sed "s+LIBPATH+/usr/local/lib/teajs/+" > ${INSTALL_ROOT}/etc/teajs.conf
	ldconfig

remoteinstall:
	ssh root@${REMOTE} rm -f ${INSTALL_ROOT}/usr/local/bin/tea
	ssh root@${REMOTE} rm -f ${INSTALL_ROOT}/usr/local/lib/libtea.so
	ssh root@${REMOTE} rm -rf ${INSTALL_ROOT}/usr/local/lib/teajs
	ssh root@${REMOTE} rm -rf ${INSTALL_ROOT}/usr/local/lib/snapshot_blob.bin
	scp tea root@${REMOTE}:${INSTALL_ROOT}/usr/local/bin/
	scp libtea.so root@${REMOTE}:${INSTALL_ROOT}/usr/local/lib
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
	rm -f libtea.so
	rm -f snapshot_blob.bin
	rm -f lib/snapshot_blob.bin
	rm -f $(OBJS)
	rm -f $(DWOBJS)
	rm -f lib/*.so

tea: src/common.o src/system.o src/cache.o src/gc.o src/app.o src/path.o src/lib/binary/bytestorage.o src/teajs.o libtea.so
	$(CPP) -fPIC -o $@ src/teajs.o $(LIBS_ELF)

libtea.so: src/common.o src/system.o src/cache.o src/gc.o src/app.o src/path.o src/lib/binary/bytestorage.o
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO)

%.o: %.cc
	$(CPP) $(FLAGS) -c -o $@ $<

lib/binary.so: src/lib/binary/binary.o src/lib/binary/bytestorage.o
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO)

lib/fs.so: src/lib/fs/fs.o src/path.o src/lib/binary/bytestorage.o
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO)

lib/gd.so: src/lib/gd/gd.o
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO) $(LIBS_GD)

lib/process.so: src/lib/process/process.o
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO)

lib/memcached.so: src/lib/memcached/memcached.o
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO) $(LIBS_MEMCACHED)

lib/socket.so: src/lib/socket/socket.o
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO)

lib/pgsql.so: src/lib/pgsql/pgsql.o
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO) $(LIBS_PG)

lib/tls.so: src/lib/tls/tls.o
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO) $(LIBS_TLS)

lib/zlib.so: src/lib/zlib/zlib.o
	$(CPP) -fPIC -o $@ -shared $^ $(LIBS_SO) $(LIBS_Z)

#clang++ -o lib/binary.so -Wl,-undefined -Wl,dynamic_lookup -shared src/lib/binary/binary.os src/lib/binary/bytestorage.os -lpthread -lrt -ldl
