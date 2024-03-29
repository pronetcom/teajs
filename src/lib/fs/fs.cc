/**
 * IO library defines File and Directory classes.
 * Some compatibility macros are defined for Win32 environment.
 */

#include <v8.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <string>
#include <cstdio>
#include <cstdlib>
#include "macros.h"
#include "common.h"
#include "path.h"

#include <iostream>
#include <fstream>

#include <unistd.h>
#include <dirent.h>

#ifdef windows
#	define MKDIR(a, b) mkdir(a)
#else
#	define MKDIR mkdir
#endif

#define TYPE_FILE 0
#define TYPE_DIR 1

namespace {

v8::Global<v8::Function> file;

/**
 * Generic directory lister
 * @param {char *} name Directory name
 * @param {int} type Type constant - do we list files or directories?
 * @param {args} the function callback args
 */
void list_items(char * name, int type, const v8::FunctionCallbackInfo<v8::Value>& args) {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	v8::Local<v8::Array> result = v8::Array::New(JS_ISOLATE);
	int cnt = 0;

	DIR * dp;
	struct dirent * ep;
	struct stat st;
	std::string path;
	unsigned int cond = (type == TYPE_FILE ? 0 : S_IFDIR);
	
	dp = opendir(name);
	if (dp == NULL) { JS_ERROR("Directory cannot be opened"); return; }
	while ((ep = readdir(dp))) { 
		path = name;
		path += "/";
		path += ep->d_name;
		if (stat(path.c_str(), &st) != 0) { continue; } /* cannot access */
		
		if ((st.st_mode & S_IFDIR) == cond) {
			std::string name = ep->d_name;
			if (type == TYPE_FILE) {
				(void)result->Set(JS_CONTEXT,JS_INT(cnt++), JS_STR(ep->d_name));
			} else if (name != "." && name != "..") {
				(void)result->Set(JS_CONTEXT,JS_INT(cnt++), JS_STR(ep->d_name));
			}
		}
	}
	closedir(dp);
	args.GetReturnValue().Set(result);
}

JS_METHOD(_directory) {
	ASSERT_CONSTRUCTOR;
	SAVE_VALUE(0, args[0]);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_create) {
	v8::String::Utf8Value name(JS_ISOLATE,LOAD_VALUE(0));
	int mode;
	if (args.Length() == 0) { 
		mode = 0777; 
	} else {
		mode = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	}

	int result = MKDIR(*name, mode);
	if (result != 0) {
		JS_ERROR("Cannot create directory");
		return;
	}

	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_listfiles) {
	v8::String::Utf8Value name(JS_ISOLATE,LOAD_VALUE(0));
	list_items(*name, TYPE_FILE, args);
}

JS_METHOD(_listdirectories) {
	v8::String::Utf8Value name(JS_ISOLATE,LOAD_VALUE(0));
	list_items(*name, TYPE_DIR, args);
}

JS_METHOD(_isdirectory) {
	v8::String::Utf8Value name(JS_ISOLATE,LOAD_VALUE(0));
	args.GetReturnValue().Set(JS_BOOL(path_dir_exists(*name)));
}

JS_METHOD(_file) {
	ASSERT_CONSTRUCTOR;
	
	SAVE_VALUE(0, args[0]);
	SAVE_VALUE(1, JS_BOOL(false));
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_open) {
	bool debug = false;
	if (const char* env_d = std::getenv("PRINT_DEBUGS")) {
		if (strcmp(env_d, "1") == 0) {
			debug = true;
		}
	}
	if (args.Length() < 1) {
		JS_TYPE_ERROR("Bad argument count. Use 'file.open(mode)'");
		return;
	}
	v8::String::Utf8Value mode(JS_ISOLATE,args[0]);
	v8::String::Utf8Value name(JS_ISOLATE,LOAD_VALUE(0));
	v8::Local<v8::Value> file = LOAD_VALUE(1);
	if (!file->IsFalse()) {
		JS_ERROR("File already opened");
		return;
	}
	
	FILE * f;

	char* cName = *name;
	bool isFd = true;
	int fd = 0;
	for (int i = 0; cName[i] != 0; i++) {
		if (cName[i] < '0' || cName[i] > '9') {
			isFd = false;
			break;
		}
		fd = fd * 10 + (int)(cName[i] - '0');
	}
	if (isFd) {
		if (debug) {
			std::cout << "File descriptor!: " << atoi(cName) << " " << *mode << "\n";
			std::cout << "fs pid: " << getpid() << "\n";
		}
		if (fd == -1) {
			JS_ERROR("Cannot create fd for this process");
			return;
		}
		f = fdopen(fd, *mode);
	}
	else {
		if (debug) {
			std::cout << "Not file descriptor!\n";
		}
		f = fopen(cName, *mode);
	}

	
	
	if (!f) {
		JS_ERROR("Cannot open file");
		return;
	}
	
	SAVE_PTR(1, f);
	args.GetReturnValue().Set(args.This());
}
		
JS_METHOD(_close) {
	v8::Local<v8::Value> file = LOAD_VALUE(1);
	
	if (file->IsFalse()) {
		JS_ERROR("Cannot close non-opened file");
		return;
	}
	
	FILE * f = LOAD_PTR(1, FILE *);
	
	fclose(f);
	SAVE_VALUE(1, JS_BOOL(false));
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_flush) {
	v8::Local<v8::Value> file = LOAD_VALUE(1);

	if (file->IsFalse()) {
		JS_ERROR("Cannot flush non-opened file");
		return;
	}

	FILE * f = LOAD_PTR(1, FILE *);

	fflush(f);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_read) {
	v8::Local<v8::Value> file = LOAD_VALUE(1);
	
	if (file->IsFalse()) {
		JS_ERROR("File must be opened before reading");
		return;
	}
	FILE * f = LOAD_PTR(1, FILE *);
	
	size_t count = 0;
	if (args.Length() && args[0]->IsNumber()) {
		count = args[0]->IntegerValue(JS_CONTEXT).ToChecked();
	}
	
	READ(f, count, args);
}

JS_METHOD(_getBinaryContent) { // pos, length
	
	v8::String::Utf8Value name(JS_ISOLATE, LOAD_VALUE(0));

	
	
	long pos = 0, length = 0;
	if (args.Length() && args[0]->IsNumber()) {
		pos = args[0]->IntegerValue(JS_CONTEXT).ToChecked();
	}

	if (args.Length() > 1 && args[1]->IsNumber()) {
		length = args[1]->IntegerValue(JS_CONTEXT).ToChecked();
	}

	std::ifstream ifile(*name, std::ios::binary | std::ios::ate);
	std::streamsize size = ifile.tellg();
	if (!length) {
		length = size;
	}
	if (pos > size) {
		args.GetReturnValue().Set(JS_STR(""));
		return;
	}
	if (pos + length > size) {
		length = size - pos;
	}
	ifile.seekg(pos, std::ios::beg);

	char* buff = new char[length];
	ifile.read(buff, length);
	ifile.close();

	v8::Local<v8::Object> ret = v8::Object::New(JS_ISOLATE);

	(void)ret->Set(JS_CONTEXT, JS_STR("result"), JS_STR(buff));

	(void)ret->Set(JS_CONTEXT, JS_STR("length"), JS_INT((int)length));

	args.GetReturnValue().Set(ret);
}

JS_METHOD(_ftell) {
	v8::Local<v8::Value> file = LOAD_VALUE(1);

	if (file->IsFalse()) {
		JS_ERROR("File must be opened before using ftell");
		return;
	}
	FILE* f = LOAD_PTR(1, FILE*);

	args.GetReturnValue().Set(JS_INT((int)std::ftell(f)));
}

JS_METHOD(_fseek) {
	v8::Local<v8::Value> file = LOAD_VALUE(1);

	if (file->IsFalse()) {
		JS_ERROR("File must be opened before using fseek");
		return;
	}
	if (args.Length() < 1) {
		JS_ERROR("Too few arguments");
		return;
	}
	if (args.Length() > 2) {
		JS_ERROR("Too many arguments");
		return;
	}
	FILE* f = LOAD_PTR(1, FILE*);

	size_t pos = 0, origin = 0;
	if (args[0]->IsNumber()) {
		pos = args[0]->IntegerValue(JS_CONTEXT).ToChecked();
	}
	else {
		JS_ERROR("Non-integer first argument");
		return;
	}
	if (args.Length() > 1) {
		if (args[1]->IsNumber()) {
			origin = args[1]->IntegerValue(JS_CONTEXT).ToChecked();
		}
		else {
			JS_ERROR("Non-integer second argument");
			return;
		}
	}

	args.GetReturnValue().Set(JS_INT((int)std::fseek(f, pos, (int)origin)));
}

JS_METHOD(_readNonblock) {
	v8::Local<v8::Value> file = LOAD_VALUE(1);

	if (file->IsFalse()) {
		JS_ERROR("File must be opened before reading");
		return;
	}
	FILE* f = LOAD_PTR(1, FILE*);

	size_t count = 0;
	if (args.Length() && args[0]->IsNumber()) {
		count = args[0]->IntegerValue(JS_CONTEXT).ToChecked();
	}

	READ_NONBLOCK(fileno(f), count, args);
}

JS_METHOD(_readline) {
	v8::Local<v8::Value> file = LOAD_VALUE(1);
	
	if (file->IsFalse()) {
		JS_ERROR("File must be opened before reading");
		return;
	}
	FILE * f = LOAD_PTR(1, FILE *);
	
	int size = (int) args[1]->IntegerValue(JS_CONTEXT).ToChecked();
	if (size < 1) { size = 0xFFFF; }
	
	READ_LINE(f, size, args);
}

JS_METHOD(_rewind) {
	v8::Local<v8::Value> file = LOAD_VALUE(1);
	if (file->IsFalse()) {
		JS_ERROR("File must be opened before rewinding");
		return;
	}
	
	FILE * f = LOAD_PTR(1, FILE *);
	rewind(f);

	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_iseof) {
	v8::Local<v8::Value> file = LOAD_VALUE(1);
	if (file->IsFalse()) {
		JS_ERROR("File must be opened before an EOF check");
		return;
	}
	FILE * f = LOAD_PTR(1, FILE *);

	args.GetReturnValue().Set(JS_BOOL(feof(f) != 0));
}

JS_METHOD(_write) {
	v8::Local<v8::Value> file = LOAD_VALUE(1);
	
	if (file->IsFalse()) {
		JS_ERROR("File must be opened before writing");
		return;
	}
	
	FILE * f = LOAD_PTR(1, FILE *);
	
	WRITE(f, args[0]);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_writeline) {
	v8::Local<v8::Value> file = LOAD_VALUE(1);
	
	if (file->IsFalse()) {
		JS_ERROR("File must be opened before writing");
		return;
	}
	
	FILE * f = LOAD_PTR(1, FILE *);
	
	WRITE_LINE(f, args[0]);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_removefile) {
	v8::String::Utf8Value name(JS_ISOLATE,LOAD_VALUE(0));
	
	if (remove(*name) != 0) {
		JS_ERROR("Cannot remove file");
		return;
	}

	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_removedirectory) {
	v8::String::Utf8Value name(JS_ISOLATE,LOAD_VALUE(0));
	
	if (rmdir(*name) != 0) {
		JS_ERROR("Cannot remove directory");
		return;
	}

	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_stat) {
	v8::String::Utf8Value name(JS_ISOLATE,LOAD_VALUE(0));
	struct stat st;
	if (stat(*name, &st) == 0) {
		v8::Local<v8::Object> obj = v8::Object::New(JS_ISOLATE);
		(void)obj->Set(JS_CONTEXT,JS_STR("size"), JS_BIGINT(st.st_size));
		(void)obj->Set(JS_CONTEXT,JS_STR("mtime"), JS_BIGINT(st.st_mtime));
		(void)obj->Set(JS_CONTEXT,JS_STR("atime"), JS_BIGINT(st.st_atime));
		(void)obj->Set(JS_CONTEXT,JS_STR("ctime"), JS_BIGINT(st.st_ctime));
		(void)obj->Set(JS_CONTEXT,JS_STR("mode"), JS_INT(st.st_mode));
		(void)obj->Set(JS_CONTEXT,JS_STR("uid"), JS_INT(st.st_uid));
		(void)obj->Set(JS_CONTEXT,JS_STR("gid"), JS_INT(st.st_gid));
		args.GetReturnValue().Set(obj);
	} else {
		args.GetReturnValue().Set(JS_BOOL(false));
	}
}

bool _copy(char * name1, char * name2) {
	size_t size = 0;
	void * data = mmap_read(name1, &size);
	if (data == NULL) { JS_ERROR("Cannot open source file"); return false; }
	
	int result = mmap_write(name2, data, size);
	mmap_free((char *)data, size);
	
	if (result == -1) { JS_ERROR("Cannot open target file"); return false; }
	return true;
}

JS_METHOD(_movefile) {
	if (args.Length() < 1) {
		JS_TYPE_ERROR("Bad argument count. Use 'file.rename(newname)'");
		return;
	}
	
	v8::String::Utf8Value name(JS_ISOLATE,LOAD_VALUE(0));
	v8::String::Utf8Value newname(JS_ISOLATE,args[0]);

	int renres = rename(*name, *newname);

	if (renres != 0) {
		if (!_copy(*name, *newname)) {
			return;
		}
		remove(*name);
	}
	
	SAVE_VALUE(0, args[0]);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_copyfile) {
	if (args.Length() < 1) {
		JS_TYPE_ERROR("Bad argument count. Use 'file.copy(newname)'");
		return;
	}
	
	v8::String::Utf8Value name(JS_ISOLATE,LOAD_VALUE(0));
	v8::String::Utf8Value newname(JS_ISOLATE,args[0]);

	if (!_copy(*name, *newname)) {
		return;
	}
	v8::Local<v8::Value> fargs[] = { args[0] };
	v8::Local<v8::Function> _file = v8::Local<v8::Function>::New(JS_ISOLATE, file);
	args.GetReturnValue().Set(_file->NewInstance(JS_CONTEXT,1, fargs).ToLocalChecked());
}

JS_METHOD(_tostring) {
	args.GetReturnValue().Set(LOAD_VALUE(0));
}

JS_METHOD(_exists) {
	v8::String::Utf8Value name(JS_ISOLATE,LOAD_VALUE(0));
	int result = access(*name, F_OK);
	args.GetReturnValue().Set(JS_BOOL(result == 0));
}

JS_METHOD(_isfile) {
	v8::String::Utf8Value name(JS_ISOLATE,LOAD_VALUE(0));
	args.GetReturnValue().Set(JS_BOOL(path_file_exists(*name)));
}


}

SHARED_INIT() {
	//fprintf(stderr,"fs.cc > SHARED_INIT InContext()=%d\n",JS_ISOLATE->InContext());
	//fprintf(stderr,"fs.cc > SHARED_INIT start()\n");
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	
	v8::Local<v8::FunctionTemplate> ft = v8::FunctionTemplate::New(JS_ISOLATE, _file);
	ft->SetClassName(JS_STR("File"));
	v8::Local<v8::ObjectTemplate> ot = ft->InstanceTemplate();
	/* filename, handle */
	ot->SetInternalFieldCount(2); 

	v8::Local<v8::ObjectTemplate> pt = ft->PrototypeTemplate();
	/**
	 * File prototype methods (new File().*)
	 */
	pt->Set(JS_ISOLATE,"open"		     , v8::FunctionTemplate::New(JS_ISOLATE, _open));
	pt->Set(JS_ISOLATE,"read"		     , v8::FunctionTemplate::New(JS_ISOLATE, _read));
	pt->Set(JS_ISOLATE,"getBinaryContent", v8::FunctionTemplate::New(JS_ISOLATE, _getBinaryContent));
	pt->Set(JS_ISOLATE,"tell"            , v8::FunctionTemplate::New(JS_ISOLATE, _ftell));
	pt->Set(JS_ISOLATE,"seek"            , v8::FunctionTemplate::New(JS_ISOLATE, _fseek));
	pt->Set(JS_ISOLATE,"readNonblock"    , v8::FunctionTemplate::New(JS_ISOLATE, _readNonblock));
	pt->Set(JS_ISOLATE,"readLine"	     , v8::FunctionTemplate::New(JS_ISOLATE, _readline));
	pt->Set(JS_ISOLATE,"rewind"		     , v8::FunctionTemplate::New(JS_ISOLATE, _rewind));
	pt->Set(JS_ISOLATE,"close"		     , v8::FunctionTemplate::New(JS_ISOLATE, _close));
	pt->Set(JS_ISOLATE,"flush"		     , v8::FunctionTemplate::New(JS_ISOLATE, _flush));
	pt->Set(JS_ISOLATE,"write"		     , v8::FunctionTemplate::New(JS_ISOLATE, _write));
	pt->Set(JS_ISOLATE,"writeLine"	     , v8::FunctionTemplate::New(JS_ISOLATE, _writeline));
	pt->Set(JS_ISOLATE,"remove"		     , v8::FunctionTemplate::New(JS_ISOLATE, _removefile));
	pt->Set(JS_ISOLATE,"toString"	     , v8::FunctionTemplate::New(JS_ISOLATE, _tostring));
	pt->Set(JS_ISOLATE,"exists"		     , v8::FunctionTemplate::New(JS_ISOLATE, _exists));
	pt->Set(JS_ISOLATE,"move"		     , v8::FunctionTemplate::New(JS_ISOLATE, _movefile));
	pt->Set(JS_ISOLATE,"copy"		     , v8::FunctionTemplate::New(JS_ISOLATE, _copyfile));
	pt->Set(JS_ISOLATE,"stat"		     , v8::FunctionTemplate::New(JS_ISOLATE, _stat));
	pt->Set(JS_ISOLATE,"isFile"		     , v8::FunctionTemplate::New(JS_ISOLATE, _isfile));
	pt->Set(JS_ISOLATE,"isEOF"		     , v8::FunctionTemplate::New(JS_ISOLATE, _iseof));


	(void)exports->Set(JS_CONTEXT,JS_STR("File"), ft->GetFunction(JS_CONTEXT).ToLocalChecked());			
	file.Reset(JS_ISOLATE, ft->GetFunction(JS_CONTEXT).ToLocalChecked());
	
	v8::Local<v8::FunctionTemplate> dt = v8::FunctionTemplate::New(JS_ISOLATE, _directory);
	dt->SetClassName(JS_STR("Directory"));
	ot = dt->InstanceTemplate();
	/* dirname */
	ot->SetInternalFieldCount(1); 

	pt = dt->PrototypeTemplate();

	/**
	 * Directory prototype methods (new File().*)
	 */
	pt->Set(JS_ISOLATE,"create"			, v8::FunctionTemplate::New(JS_ISOLATE, _create));
	pt->Set(JS_ISOLATE,"listFiles"		, v8::FunctionTemplate::New(JS_ISOLATE, _listfiles));
	pt->Set(JS_ISOLATE,"listDirectories", v8::FunctionTemplate::New(JS_ISOLATE, _listdirectories));
	pt->Set(JS_ISOLATE,"toString"		, v8::FunctionTemplate::New(JS_ISOLATE, _tostring));
	pt->Set(JS_ISOLATE,"exists"			, v8::FunctionTemplate::New(JS_ISOLATE, _exists));
	pt->Set(JS_ISOLATE,"remove"			, v8::FunctionTemplate::New(JS_ISOLATE, _removedirectory));
	pt->Set(JS_ISOLATE,"stat"			, v8::FunctionTemplate::New(JS_ISOLATE, _stat));
	pt->Set(JS_ISOLATE,"isDirectory"	, v8::FunctionTemplate::New(JS_ISOLATE, _isdirectory));

	//fprintf(stderr,"fs.cc > SHARED_INIT set Directory function\n");
	(void)exports->Set(JS_CONTEXT,JS_STR("Directory"), dt->GetFunction(JS_CONTEXT).ToLocalChecked());
	//fprintf(stderr,"fs.cc > SHARED_INIT end()\n");
}
