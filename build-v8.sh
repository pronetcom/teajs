#!/bin/bash

set -e

unameOut="$(uname -s)"
case "${unameOut}" in
	Linux*)		machine=Linux;;
	Darwin*)	machine=Mac;;
	CYGWIN*)	machine=Cygwin;;
	MINGW*)		machine=MinGw;;
	*)			machine="UNKNOWN:${unameOut}"
esac
echo ${machine}

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
		./build/install-build-deps.sh
	fi
fi

#autoninja -v -C out/x64.release all
#exit
#tools/dev/gm.py x64.release all
tools/dev/gm.py x64.release all  --progress=verbose --verbose --debug
exit
#tools/dev/gm.py x64.debug all --verbose --debug

if [ -f out.gn/x64.release/args.gn ]; then
	echo out.gn/x64.release/args.gn already exists, skipping
else
	tools/dev/v8gen.py x64.release
	mv out.gn/x64.release/args.gn out.gn/x64.release/args.gn~
	cat out.gn/x64.release/args.gn~ | grep -v v8_enable_sandbox > out.gn/x64.release/args.gn
	echo "is_component_build=true" >> out.gn/x64.release/args.gn
	echo "v8_static_library=true" >> out.gn/x64.release/args.gn
	#echo "use_custom_libcxx=false" >> out.gn/x64.release/args.gn
	echo "v8_monolithic=true" >> out.gn/x64.release/args.gn
	#echo "" >> out.gn/x64.release/args.gn
fi
autoninja -v -C out.gn/x64.release

echo Getting compilation and linking arguments
rm out.gn/x64.release/v8_shell
rm out.gn/x64.release/obj/v8_shell/shell.o

autoninja -v -C out.gn/x64.release >$TEAJS_BASEDIR/linker_info.txt


#gn gen "--args=is_clang=true is_component_build=false v8_static_library=true use_custom_libcxx=false target_cpu=\"x64\"" out.gn/x64.release
#ninja -C out.gn/x64.release v8_monolith

tools/dev/v8gen.py x64.release.sample
autoninja -C out.gn/x64.release.sample #v8_monolith
