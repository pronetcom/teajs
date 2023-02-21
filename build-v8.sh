#!/bin/bash

set -e

unameOut="$(uname -s)"
case "${unameOut}" in
	Linux*)		machine=Linux;;
	Darwin*)	machine=Mac;;
	CYGWIN*)	machine=Cygwin;;
	MINGW*)		machine=MinGw;;
	*)		machine="UNKNOWN:${unameOut}"
esac
echo ${machine}

arch=`uname -m | sed 's/x86_64/x64/;s/aarch64/arm64/'`

if [ "y$1" == "y" ]; then
	V8_BASEDIR=`pwd`/../v8_things
else
	V8_BASEDIR=$1
fi

echo Compiling v8 in $V8_BASEDIR

export TEAJS_BASEDIR=`pwd`
export PATH=$V8_BASEDIR/depot_tools:$PATH

if [ -d $V8_BASEDIR ]; then
	echo Directory exists, skipping
	cd $V8_BASEDIR
	cd v8
else
	echo Empty directory, downloading
	mkdir $V8_BASEDIR
	cd $V8_BASEDIR

	# 1. TODO check git exists
	TMP=`git --version 2>&1 | grep version`
	if [ "y$TMP" == "y" ]; then
		echo "git not found"
		exit 1
	fi

	# 2. install depot_tools
	git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

	# 3. update depot_tools
	gclient

	# 4. create v8 directory
	fetch v8
	cd v8

	# 5. download build dependencies
	gclient sync

	# 6. linux only
	if [ "y$machine" == "yLinux" ]; then
		sed -i "s/i686|x86_64/`uname -m`/" ./build/install-build-deps.sh
		test "x`uname -m`" == "xaarch64" && sed -i '/lib32z1/d' ./build/install-build-deps.sh
		sed -i '/deprecated-non-prototype/d' ./build/config/compiler/BUILD.gn
		sed -i '/deprecated-builtins/d' ./third_party/zlib/BUILD.gn
		./build/install-build-deps.sh
	fi
fi

#autoninja -v -C out/$arch.release all
#exit
#tools/dev/gm.py $arch.release all
tools/dev/gm.py $arch.release all  --progress=verbose --verbose --debug
exit
#tools/dev/gm.py $arch.debug all --verbose --debug

if [ -f out.gn/$arch.release/args.gn ]; then
	echo out.gn/$arch.release/args.gn already exists, skipping
else
	tools/dev/v8gen.py $arch.release
	mv out.gn/$arch.release/args.gn out.gn/$arch.release/args.gn~
	cat out.gn/$arch.release/args.gn~ | grep -v v8_enable_sandbox > out.gn/$arch.release/args.gn
	echo "is_component_build=true" >> out.gn/$arch.release/args.gn
	echo "v8_static_library=true" >> out.gn/$arch.release/args.gn
	#echo "use_custom_libcxx=false" >> out.gn/$arch.release/args.gn
	echo "v8_monolithic=true" >> out.gn/$arch.release/args.gn
	#echo "" >> out.gn/$arch.release/args.gn
fi
autoninja -v -C out.gn/$arch.release

echo Getting compilation and linking arguments
rm out.gn/$arch.release/v8_shell
rm out.gn/$arch.release/obj/v8_shell/shell.o

autoninja -v -C out.gn/$arch.release >$TEAJS_BASEDIR/linker_info.txt


#gn gen "--args=is_clang=true is_component_build=false v8_static_library=true use_custom_libcxx=false target_cpu=\"$arch\"" out.gn/$arch.release
#ninja -C out.gn/$arch.release v8_monolith

tools/dev/v8gen.py $arch.release.sample
autoninja -C out.gn/$arch.release.sample #v8_monolith
