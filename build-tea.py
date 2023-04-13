#!/usr/bin/env python3

import sys
import os
import re
import pprint
import subprocess
import shutil
import platform

def get_dependencies_list(params):
    return [
        {
            "name":"libpq",
            "name_pkg_config":"libpq",
            "optional":0,
            "includes_copy":[],
            "libraries":[]
        },
        {
            "name":"gd",
            "name_pkg_config":"gdlib",
            "optional":0,
            "includes_copy":["gdfx.h","gd.h","gd_io.h"],
            "libraries":[]
        },
        {
            "name":"memcached",
            "name_pkg_config":"libmemcached",
            "optional":0,
            "includes_copy_recursive":["libmemcached","libmemcached-1.0","libhashkit-1.0","sasl"],
            "libraries":[]
        }
    ]

# Copies params to $ENV
def params_to_env(params):
    for item in params:
        os.environ[item]=params[item]
    f=open("d8_objects.txt","w")
    f.write(params['D8_OBJECTS'])
    f.close()

# Replaces paths from d8.ninja with actual ones
# d8.ninja has paths relative to V8_COMPILEDIR, like ../../include/ or ../../../../../../Applications/Xcode.app/ or something
def patch_paths(params,s):

    def repl_path(m):
        return m.group(1)+params['V8_COMPILEDIR']+'/'+m.group(2)

    s=re.sub('(\.\.\/)+Applications\/','/Applications/',s)
    s=re.sub('\.\./\.\.',params['V8_BASEDIR'],s)
    s=re.sub('(-I)(gen)',repl_path,s)
    return s

# Removing unwanted params fetched from d8.ninja
def remove_unwanted(params,s):
    s=re.sub(' -Werror','',s)
    s=re.sub(' -fno-exceptions','',s)
    s=re.sub(' -Wunused-variable','',s)
    #s=re.sub(' -Wno-unused-parameter','',s)
    s=re.sub(' -DV8_DEPRECATION_WARNINGS','',s)
    s=re.sub(' -Wl,-z,defs','',s)
    #s=re.sub(' -fvisibility=default',' -fvisibility=public',s)
    return s

# Leaving only unique items in array
def array_unique(arr):
    ret=[]
    used={}
    for item in arr:
        if not (item in used):
            used[item]=1
            ret.append(item)
    return ret

# Parsing d8.ninja and taking out arguments, files etc
def parse_d8_compile_args(params):
    f=open(params['V8_COMPILEDIR']+"/obj/d8.ninja")
    build_section=""
    for item in f.readlines():
        tmp=re.search('^( *)(\w+) = (.*)',item)
        if tmp:
            if tmp.group(1)=="" or (tmp.group(1)=="  " and build_section=="./d8"):
                params['D8_'+tmp.group(2).upper()]=remove_unwanted(params,patch_paths(params,tmp.group(3)))
        tmp=re.search('^build ([^ ]+): link (.*) \|\|',item)
        if tmp:
            build_section=tmp.group(1)
            if build_section=="./d8":
                files_objects =[]
                files_archives=[]
                files_libpaths=[]
                for file in tmp.group(2).split(" "):
                    if file.find("/d8/")==-1:
                        if file.endswith(".o"):
                            files_objects .append(params['V8_COMPILEDIR']+"/"+file)
                        elif file.endswith(".a"):
                            tmp=re.search('^(.*)/lib(\w+)\.a$',file)
                            if tmp:
                                files_libpaths.append("-L"+params['V8_COMPILEDIR']+"/"+tmp.group(1))
                                files_archives.append("-l"+tmp.group(2))
                            else:
                                print("Could not parse library name - "+file)
                                exit(1)
                        else:
                            print("Unknown file extension - "+file)
                            exit(1)
                            #files_archives.append(params['V8_COMPILEDIR']+"/"+file)
                files_objects .append(params['V8_COMPILEDIR']+"/obj/v8_libbase/stack_trace.o") # ???
                files_objects .append(params['V8_COMPILEDIR']+"/obj/v8_libbase/stack_trace_posix.o") # ???
                files_archives.append(params['V8_COMPILEDIR']+"/obj/libwee8.a") # ???
                #files_objects .append(params['V8_COMPILEDIR']+"/obj/v8_libbase/sys-info.o") # ???
                params['D8_OBJECTS' ]=" ".join(array_unique(files_objects))
                params['D8_ARCHIVES']=" ".join(array_unique(files_archives))
                params['D8_LIBPATHS']=" ".join(array_unique(files_libpaths))
    f.close()

# Resolving 3rd_party depenencies
def resolve_3rd_parties(params):
    if not os.path.exists("3rd-party"):
        os.mkdir("3rd-party")
    if os.path.exists("/usr/bin/pkg-config"):
        params["resolve"]="pkg-config"
    elif os.path.exists("/usr/local/bin/brew"):
        params["resolve"]="brew"
    elif os.path.exists("/opt/homebrew/bin/brew"):
        params["resolve"]="brewm"
    else:
        print("No pkg-config and no brew - TODO STOP")
        exit(1)

    for dep in get_dependencies_list(params):
        resolve_3rd_party_item(params,dep)

def get_libraries_by_path(params,dep,path):
    # TODO if "libraries" in dep
    ret=[]
    for item in os.listdir(path):
        tmp=re.search('^lib(\w+)\.(dylib|a|so|dll)',item)
        if tmp:
            #print("get_libraries_by_path() - tmp.group(1)=%(a)s",{"a":tmp.group(1)})
            ret.append("-l"+tmp.group(1))
    return ret

def resolve_3rd_party_item(params,dep):
    if params["resolve"]=="pkg-config":
        params[dep["name"].upper()+'_INCLUDE']=subprocess.check_output('pkg-config --cflags '+dep["name_pkg_config"], shell=True).decode("utf-8").split("\n")[0]
        params[dep["name"].upper()+'_LIBRARY']=subprocess.check_output('pkg-config --libs '  +dep["name_pkg_config"], shell=True).decode("utf-8").split("\n")[0]
    elif params["resolve"]=="brew":
        if not os.path.exists("/usr/local/opt/"+dep["name"]):
            print("Path does not exist - /usr/local/opt/"+dep["name"])
            exit(1)
        params[dep["name"].upper()+'_INCLUDE']="-I/usr/local/opt/"+dep["name"]+"/include -I/usr/local/include/"
        libpath="/usr/local/opt/"+dep["name"]+"/lib"
        params[dep["name"].upper()+'_LIBRARY']="-L"+libpath+" "+(" ".join(array_unique(get_libraries_by_path(params,dep,libpath))))
    elif params["resolve"]=="brewm":
        if not os.path.exists("/opt/homebrew/opt/"+dep["name"]):
            print("Path does not exist - /opt/homebrew/opt/"+dep["name"])
            exit(1)
        params[dep["name"].upper()+'_INCLUDE']="-I/opt/homebrew/opt/"+dep["name"]+"/include -I/opt/homebrew/include/"
        libpath="/opt/homebrew/opt/"+dep["name"]+"/lib"
        params[dep["name"].upper()+'_LIBRARY']="-L"+libpath+" "+(" ".join(array_unique(get_libraries_by_path(params,dep,libpath))))
    else:
        exit(1)

    # TODO
    copy_from=["/usr/include/"]
    if params[dep["name"].upper()+'_INCLUDE']=="":
        print("Dependency %(name)s INCLUDE path is empty, searching in /usr/include/"%dep)
        if "includes_copy" in dep:
            for file in dep["includes_copy"]:
                flag=0
                for path in copy_from:
                    if flag==0 and os.path.exists(path+file):
                        dstpath=os.path.dirname("3rd-party/"+file)
                        os.makedirs(dstpath, exist_ok=True)
                        shutil.copyfile(path+file,"3rd-party/"+file)
                        flag=1
                if flag==0:
                    print("File %(file)s not found in /usr/include/ - TODO" % {"file":file})
                    exit(1)
        if "includes_copy_recursive" in dep:
            for file in dep["includes_copy_recursive"]:
                flag=0
                for path in copy_from:
                    if flag==0 and os.path.exists(path+file):
                        dstpath="3rd-party/"+file
                        if os.path.exists(dstpath):
                            shutil.rmtree(dstpath)
                        shutil.copytree(path+file,dstpath)
                        flag=1
                if flag==0:
                    print("File %(file)s not found in /usr/include/ - TODO" % {"file":file})
                    exit(1)

    #arr=[]
    #for file in dep["libraries"]:
    #    arr.append("-l"+file)
    #params[dep["name"].upper()+'_LIBRARY']=" ".join(arr)

def work():
    if len(sys.argv)==1:
        print("Usage:")
        myname=sys.argv[0]
        repl= {'myname':myname}
        print("    %(myname)s build [...]                         - builds teajs" % repl)
        print("    sudo %(myname)s install [...]                  - installs teajs" % repl)
        print("    %(myname)s remoteinstall --remote 172.168.32.1 - remote installs teajs (connects root@172.168.32.1)" % repl)
        print("    %(myname)s clean [...]                         - cleans project" % repl)
        print("")
        print("flags:")
        print("     --v8 some_v8_path/                            - use another v8 location")
        print("     --remote <servername>                         - copy software to remote machine (scp and ssh are used)")
        print("     --install_root /alter/path/                   - install software to altername root")
        print("     --jobs <number>                               - run makefile with -j <number>")
        return 1

    params={}
    params['JOBS']="8"
    for i in range(2,len(sys.argv),2):
        tmp=re.search('^--(.*)',sys.argv[i])
        if tmp:
            params[tmp.group(1).upper()]=sys.argv[i+1]
        else:
            print("Cannot parse argument '%(arg)s'",{"arg":sys.argv[i]})
            exit(1)

    params['CDIR']=os.path.dirname(os.path.realpath(__file__))
    params['PDIR']=re.sub('([\\\/])[^\\\/]+[\\\/]?$','',params['CDIR'])
    params['V8_BASEDIR']=params['PDIR']+"/v8_things/v8"
    params['V8_COMPILEDIR']=params['V8_BASEDIR']+"/out/"+platform.machine().replace("aarch", "arm").replace("x86_64","x64")+".release";

    params['TEAJS_BASEDIR']=params['CDIR']
    params['TEAJS_LIBPATH']=params['TEAJS_BASEDIR']+"/lib"
    f=open("VERSION")
    params['TEAJS_VERSION']=f.read().split("\n")[0]
    f.close()
    params['V8_CPP']=params['V8_BASEDIR']+"/third_party/llvm-build/Release+Asserts/bin/clang++"

    resolve_3rd_parties(params)

    parse_d8_compile_args(params)

    # Saving all params as to a log file
    f=open("build-tea.log","w")
    for item in params:
        print("    "+item+" = "+params[item][:70])
        f.write(item+" = "+params[item]+"\n")
    f.close()

    if not ('INSTALL_ROOT' in params):
       params['INSTALL_ROOT']='/'
    if sys.argv[1]=="build":
        params_to_env(params)
        subprocess.call(["make","-j",params["JOBS"]])
    elif sys.argv[1]=="install":
        params_to_env(params)
        subprocess.call(["make","-j",params["JOBS"],"install"])
    elif sys.argv[1]=="remoteinstall":
        params_to_env(params)
        subprocess.call(["make","-j",params["JOBS"],"remoteinstall"])
    elif sys.argv[1]=="clean":
        params_to_env(params)
        subprocess.call(["make","-j",params["JOBS"],"clean"])


if __name__ == "__main__":
    work()


