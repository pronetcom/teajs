/**
 * TeaJS app file. This class represents generic V8 embedding; cgi binary and apache module inherit from it.
 */

//#define DEBUG_LOAD_JS

#include <sstream>
#include <vector>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <v8.h>
#include <libplatform/libplatform.h>


#if defined(FASTCGI) || defined (FASTCGI_JS)
#	include <fcgi_stdio.h>
#endif

#include "app.h"
#include "system.h"
#include "macros.h"
#include "cache.h"
#include "common.h"
#include "path.h"

#ifndef windows
#	include <dlfcn.h>
#else
#	include <windows.h>
#	define dlsym(x, y) GetProcAddress((HMODULE)x, y)
#endif

int fcgi_pre_accepted=0;
void write_debug(const char *text)
{
	struct tm *ptr;
	time_t lt;
	lt = time(NULL);
	ptr = localtime(&lt);
	FILE *out=fopen("/tmp/teajs-debug.txt","a");
	if (out) {
		fprintf(out,"PID=%d %04d-%02d-%02d %02d:%02d:%02d - %s\n",getpid(),ptr->tm_year+1900,ptr->tm_mon+1,ptr->tm_mday,ptr->tm_hour,ptr->tm_min,ptr->tm_sec,text);
		fclose(out);
	} else {
		fprintf(stderr,"write_debug() - cannot open debug.txt for writting\n");
	}
}

v8::Local<v8::Value> JS_ERROR(const char*reason) {
	//fprintf(stderr,"JS_ERROR() should throw - %s\n",reason);
	return JS_THROW(Error, reason);
}

/**
 * global.require = load module and return its (cached) exports
 */
JS_METHOD(_require) {
	TeaJS_App * app = APP_PTR;
	v8::String::Utf8Value file(JS_ISOLATE,args[0]);
	std::string root = *(v8::String::Utf8Value(JS_ISOLATE,args.Data()));
	
	//v8::TryCatch try_catch(JS_ISOLATE);
	try {
		v8::Persistent<v8::Value> required;
		required.Reset(JS_ISOLATE, app->require(*file, root));
		args.GetReturnValue().Set(required.Get(JS_ISOLATE));
	} catch (std::string e) {
		//JS_ERROR(e.c_str());
		JS_ERROR(e.c_str());
	}
}

/**
 * global.onexit = add a function to be executed when context ends
 */
JS_METHOD(_onexit) {
	TeaJS_App * app = APP_PTR;
	if (!args[0]->IsFunction()) { JS_TYPE_ERROR("Non-function passed to onexit()"); return; }
	v8::Persistent<v8::Function, v8::CopyablePersistentTraits<v8::Function> > fun;
	fun.Reset(JS_ISOLATE, v8::Local<v8::Function>::Cast(args[0]));
	app->onexit.push_back(fun);
	args.GetReturnValue().SetUndefined();
}

/**
 * global.exit - terminate execution
 */
JS_METHOD(_exit) {
	TeaJS_App * app = APP_PTR;
	if (args.Length() > 0) {
		app->exit_code = (int)args[0]->IntegerValue(JS_CONTEXT).ToChecked();
	} else {
		app->exit_code = 1;
	}

	fprintf(stderr,"TODO v8::V8::TerminateExecution(JS_ISOLATE);\n");// TODO vahvarh
	//v8::V8::TerminateExecution(JS_ISOLATE);
	/* do something at least a bit complex so the stack guard can throw the termination exception */
	(void)v8::Script::Compile(JS_CONTEXT,JS_STR("(function(){})()")).ToLocalChecked()->Run(JS_CONTEXT);
	args.GetReturnValue().SetUndefined();
}

TeaJS_App::~TeaJS_App()
{

}
/**
 * To be executed only once - initialize stuff
 */
void TeaJS_App::init(int argc, char ** argv) {
	this->cfgfile = config_path();
	this->show_errors = false;
	this->exit_code = 0;

	/*v8::V8::InitializeICU();

	this->platform = v8::platform::CreateDefaultPlatform();
	v8::V8::InitializePlatform(this->platform);

	v8::V8::Initialize();*/

	const char *path=blob_path();

	v8::V8::InitializeICUDefaultLocation(path);
	v8::V8::InitializeExternalStartupData(path);
	platform = v8::platform::NewDefaultPlatform();
	v8::V8::InitializePlatform(platform.get());
	v8::V8::Initialize();


	v8::Isolate::CreateParams create_params;
	//create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
	create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
	this->isolate = v8::Isolate::New(create_params);
	this->isolate->Enter();
}

/**
 * Initialize and setup the context. Executed during every request, prior to executing main request file.
 */
void TeaJS_App::prepare(char ** envp) {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	v8::Local<v8::Object> g = JS_GLOBAL;

	std::string root = path_getcwd();
	/* FIXME it might be better NOT to expose this to global
	g->Set(JS_STR("require"), v8::FunctionTemplate::New(_require, JS_STR(root.c_str()))->GetFunction());
	*/
	(void)g->Set(JS_CONTEXT,JS_STR("onexit"), v8::FunctionTemplate::New(JS_ISOLATE, _onexit)->GetFunction(JS_CONTEXT).ToLocalChecked());
	(void)g->Set(JS_CONTEXT,JS_STR("exit"), v8::FunctionTemplate::New(JS_ISOLATE, _exit)->GetFunction(JS_CONTEXT).ToLocalChecked());
	(void)g->Set(JS_CONTEXT,JS_STR("global"), g);

	this->paths.Reset(JS_ISOLATE, v8::Array::New(JS_ISOLATE));
	v8::Local<v8::Array> paths = v8::Local<v8::Array>::New(JS_ISOLATE, this->paths);

	/* config file */
	v8::Local<v8::Object> config =
			v8::Local<v8::Object>::New(JS_ISOLATE, this->require(path_normalize(this->cfgfile), path_getcwd()));

	if (!paths->Length()) {
		std::string error = "require.paths is empty, have you forgotten to push some data there?";
		// vahvarh throw
		throw error;
	}
	
	(void)g->Set(JS_CONTEXT,JS_STR("Config"), config->Get(JS_CONTEXT,JS_STR("Config")).ToLocalChecked());

	setup_teajs(g);
	setup_system(g, envp, this->mainfile, this->mainfile_args);
}

/**
 * Process a request.
 * @param {char**} envp Environment
 */
void TeaJS_App::execute(char ** envp) {

	v8::Isolate::Scope isolate_scope(isolate);
	v8::HandleScope handle_scope(isolate);

	// Create a new context.
	v8::Local<v8::Context> root_context = v8::Context::New(isolate);

	v8::Context::Scope context_scope(root_context);


	//v8::Locker locker(JS_ISOLATE);
	//v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);

	std::string caught;
	this->create_context();
	this->mainModule.Reset(JS_ISOLATE, v8::Object::New(JS_ISOLATE));

	// vahvarh throw
	try {
		v8::TryCatch tc(JS_ISOLATE);

		this->prepare(envp);
		if (tc.HasCaught()) {
			//JS_ERROR(this->format_exception(&tc).c_str());
			throw this->format_exception(&tc);
		} /* uncaught exception when loading config file */
		
		if (this->mainfile == "") {
			//JS_ERROR("Nothing to do :)");
			throw std::string("Nothing to do :)");
		}

		this->require(this->mainfile, path_getcwd()); 
		
		if (tc.HasCaught() && tc.CanContinue()) {
			//JS_ERROR(this->format_exception(&tc).c_str());
			throw this->format_exception(&tc);
		} /* uncaught exception when executing main file */

	} catch (std::string e) {
		this->exit_code = 1;
		caught = e;
	}
	
	this->finish();
	
	if (caught.length()) {
		throw caught;
	} // rethrow
}

/**
 * End request
 */
void TeaJS_App::finish() {
	v8::Local<v8::Value> show = this->get_config("showErrors");
	this->show_errors = show->ToBoolean(JS_ISOLATE)->IsTrue();

	/* user callbacks */
	for (unsigned int i=0; i<this->onexit.size(); i++) {
		v8::Local<v8::Function> onexit = v8::Local<v8::Function>::New(JS_ISOLATE, this->onexit[i]);
		(void)onexit->Call(JS_CONTEXT,JS_GLOBAL, 0, NULL);
		this->onexit[i].Reset();
	}
	this->onexit.clear();

	/* export cache */
	this->cache.clearExports();
	
	this->delete_context();
}

/**
 * Require a module.
 * @param {std::string} name
 * @param {std::string} relativeRoot module root for relative includes
 */
v8::Persistent<v8::Object, v8::CopyablePersistentTraits<v8::Object> > TeaJS_App::require(std::string name, std::string relativeRoot) {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
#ifdef VERBOSE
	printf("[require] looking for '%s'\n", name.c_str()); 
#endif	
	modulefiles files = this->resolve_module(name, relativeRoot);
	
	if (!files.size()) { 
		std::string error = "Cannot find module '";
		error += name;
		error += "'";
		//fprintf(stderr,"%s\n",error.c_str());
		throw error;
	}

#ifdef VERBOSE
	printf("[require] resolved as '%s' (%d files)\n", files[0].c_str(), (int)files.size()); 
#endif	

	/* module name is the first component => hybrid modules are indexed by their native part */
	std::string modulename = files[0];
	modulename = modulename.substr(0,	modulename.find_last_of('.'));

	v8::Persistent<v8::Object, v8::CopyablePersistentTraits<v8::Object> > exports = this->cache.getExports(modulename);
	/* check if exports are cached */
	if (!exports.IsEmpty()) { return exports; }
	
	/* create module-specific require */
	v8::Local<v8::Function> require = this->build_require(modulename, _require);

	/* add new blank exports to cache */
	v8::Local<v8::Object> _exports = v8::Object::New(JS_ISOLATE);
	this->cache.addExports(modulename, _exports);

	/* create/use the "module" variable" */
	v8::Local<v8::Object> module = (name == this->mainfile ? v8::Local<v8::Object>::New(JS_ISOLATE, this->mainModule) : v8::Object::New(JS_ISOLATE));
	(void)module->Set(JS_CONTEXT,JS_STR("id"), JS_STR(modulename.c_str()));

	int status = 0;
	for (unsigned int i=0; i<files.size(); i++) {
		std::string file = files[i];
		std::string ext = file.substr(file.find_last_of('.')+1, std::string::npos);
		if (ext == STRING(DSO_EXT)) {
			this->load_dso(file, require, _exports, module);
		} else {
			status = this->load_js(file, require, _exports, module);
			if (status != 0) {
				this->cache.removeExports(modulename);
				exports.Reset(JS_ISOLATE, _exports);
				return exports;
			}
		}
		
	}

	exports.Reset(JS_ISOLATE, _exports);
	return exports;
}

/**
 * Include a js module
 */
int TeaJS_App::load_js(std::string filename, v8::Local<v8::Function> require, v8::Local<v8::Object> exports, v8::Local<v8::Object> module) {
	char tmp[1024];
#ifdef DEBUG_LOAD_JS
	sprintf(tmp,"TeaJS_App::load_js(%s)",filename.c_str());
	write_debug(tmp);
#endif
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);

	/* compiled script wrapped in anonymous function */

	v8::Persistent<v8::Script, v8::CopyablePersistentTraits<v8::Script> > script_ml=this->cache.getScript(filename);
	if (script_ml.IsEmpty()) return 1;
	v8::Local<v8::Script> script = v8::Local<v8::Script>::New(JS_ISOLATE, script_ml);

	if (script.IsEmpty()) { return 1; } /* compilation error? */
	/* run the script, no error should happen here */
	v8::Local<v8::Value> wrapped = script->Run(JS_CONTEXT).ToLocalChecked();

	v8::Local<v8::Function> fun = v8::Local<v8::Function>::Cast(wrapped);
	v8::Local<v8::Value> params[3] = {require, exports, module}; 
	v8::MaybeLocal<v8::Value> result_ = fun->Call(JS_CONTEXT,exports, 3, params);
	if (result_.IsEmpty()) {
		#ifdef DEBUG_LOAD_JS
			sprintf(tmp,"TeaJS_App::load_js(%s) - is empty",filename.c_str());
			write_debug(tmp);
		#endif
		//fprintf(stderr,"TeaJS_App::load_js(%s) - IsEmpty()\tisolate=%ld, InContext()=%d, context=%ld\n",filename.c_str(),(void*)JS_ISOLATE,isolate->InContext(),(void*)(*JS_CONTEXT));
		return 1;
	}
	#ifdef DEBUG_LOAD_JS
		sprintf(tmp,"TeaJS_App::load_js(%s) - OK",filename.c_str());
		write_debug(tmp);
	#endif
	v8::Local<v8::Value> result = result_.ToLocalChecked();
	
	return (result.IsEmpty() ? 1 : 0);
}

/**
 * Include a DSO module
 */
void TeaJS_App::load_dso(std::string filename, v8::Local<v8::Function> require, v8::Local<v8::Object> exports, v8::Local<v8::Object> module) {

	v8::Isolate *isolate=JS_ISOLATE;
	//fprintf(stderr,"TeaJS_App::load_dso(%s)\tisolate=%ld, InContext()=%d, context=%ld\n",filename.c_str(),(void*)JS_ISOLATE,isolate->InContext(),(void*)(*JS_CONTEXT));

	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	void * handle = this->cache.getHandle(filename);
	
	typedef void (*init_t)(v8::Local<v8::Function>, v8::Local<v8::Object>, v8::Local<v8::Object>);
	init_t func = (init_t) dlsym(handle, "init");

	if (!func) {
		std::string error = "Cannot initialize shared library '";
		error += filename;
		error += "'";
		throw error;
	}
	
	func(require, exports, module);
}

/**
 * Fully expand/resolve module name
 */
TeaJS_App::modulefiles TeaJS_App::resolve_module(std::string name, std::string relativeRoot) {
	if (!name.length()) { return modulefiles(); }

	if (path_isabsolute(name)) {
		/* TeaJS non-standard extension - absolute path */
#ifdef VERBOSE
		printf("[resolve_module] expanded to '%s'\n", name.c_str()); 
#endif	
		return this->resolve_extension(name);
	} else if (name.at(0) == '.') {
		/* local module, relative to current path */
		std::string path = relativeRoot;
		path += "/";
		path += name;
#ifdef VERBOSE
		printf("[resolve_module] expanded to '%s'\n", path.c_str()); 
#endif	
		return this->resolve_extension(path);
	} else {
		/* convert to array of search paths */
		v8::Local<v8::Array> arr = v8::Local<v8::Array>::New(JS_ISOLATE, this->paths);
		int length = arr->Length();
		v8::Local<v8::Value> prefix;
		modulefiles result;
		
		for (int i=0;i<length;i++) {
			prefix = arr->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked();
			v8::String::Utf8Value pfx(JS_ISOLATE,prefix);
			std::string path(*pfx);
			path += "/";
			path += name;
#ifdef VERBOSE
		printf("[resolve_module] expanded to '%s'\n", path.c_str()); 
#endif	
			result = this->resolve_extension(path);
			if (result.size()) { return result; }
		}
		
		return modulefiles();
	}
}

/**
 * Try to adjust file's extension in order to locate an existing file
 */
TeaJS_App::modulefiles TeaJS_App::resolve_extension(std::string path) {
	/* remove /./, /../ etc */
	std::string fullPath = path_normalize(path); 
	modulefiles result;
	
	/* first, try suffixes */
	const char * suffixes[] = {STRING(DSO_EXT), "js"};
	std::string path2; 
	for (int j=0;j<2;j++) {
		path2 = fullPath;
		path2 += ".";
		path2 += suffixes[j];
		if (path_file_exists(path2)) { 
			result.push_back(path2); 
#ifdef VERBOSE
			printf("[resolve_extension] extension found '%s'\n", path2.c_str()); 
#endif	
		}
	}

	/* if the path already exists (extension to commonjs modules 1.1), use it */
	if (!result.size() && path_file_exists(fullPath)) { result.push_back(fullPath); }
	
	return result;
}

/** 
 * Convert JS exception to c string 
 */
std::string TeaJS_App::format_exception(v8::TryCatch* try_catch) {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	v8::String::Utf8Value exception(JS_ISOLATE,try_catch->Exception());
	v8::Local<v8::Message> message = try_catch->Message();
	std::string msgstring = "";
	std::stringstream ss;

	if (message.IsEmpty()) {
		msgstring += *exception;
	} else {
		v8::String::Utf8Value filename(JS_ISOLATE,message->GetScriptResourceName());
		int linenum = message->GetLineNumber(JS_CONTEXT).ToChecked();
		msgstring += *exception;
		msgstring += " (";
		msgstring += *filename;
		msgstring += ":";
		ss << linenum;
		msgstring += ss.str();
		msgstring += ")";
		
		v8::Local<v8::Value> stack = try_catch->StackTrace(JS_CONTEXT).ToLocalChecked();
		if (!stack.IsEmpty()) {
			v8::String::Utf8Value sstack(JS_ISOLATE,stack);
			msgstring += "\n";
			msgstring += *sstack;
		}
	}
	return msgstring;
}

/**
 * Creates a new context
 */
void TeaJS_App::create_context() {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);

	if (this->global.IsEmpty()) { /* first time */
		v8::Local<v8::ObjectTemplate> globalt = v8::ObjectTemplate::New(JS_ISOLATE);
		globalt->SetInternalFieldCount(2);
		this->globalt.Reset(JS_ISOLATE, globalt);

		v8::Local<v8::Context> context = v8::Context::New(JS_ISOLATE, NULL, globalt);
		context->Enter();
		this->context.Reset(JS_ISOLATE, context);

		this->global.Reset(JS_ISOLATE, JS_GLOBAL);
	} else { /* Nth time */
#ifdef REUSE_CONTEXT
		v8::Local<v8::Context> context = v8::Local<v8::Context>::New(JS_ISOLATE, this->context);
		context->Enter();
		this->clear_global(); /* reuse - just clear */
#else
		v8::Local<v8::ObjectTemplate> globalt = v8::Local<v8::ObjectTemplate>::New(JS_ISOLATE, this->globalt);
		v8::Local<v8::Value> global = v8::Local<v8::Value>::New(JS_ISOLATE, this->global);
		v8::Local<v8::Context> context = v8::Context::New(JS_ISOLATE, NULL, globalt, global);
		context->Enter();
		this->context.Reset(JS_ISOLATE, context);
#endif
	}
	GLOBAL_PROTO->SetInternalField(0, v8::External::New(JS_ISOLATE, (void *) this));
	GLOBAL_PROTO->SetInternalField(1, v8::External::New(JS_ISOLATE, (void *) &(this->gc)));

}

/**
 * Deletes the existing context
 */
void TeaJS_App::delete_context() {
	v8::Local<v8::Context> context = v8::Local<v8::Context>::New(JS_ISOLATE, this->context);
	context->Exit();
#ifndef REUSE_CONTEXT
	this->context.Reset();
#endif
}

/**
 * Removes all "garbage" from the global object
 */
void TeaJS_App::clear_global() {
	v8::Local<v8::Array> keys = JS_GLOBAL->GetPropertyNames(JS_CONTEXT).ToLocalChecked();
	int length = keys->Length();
	for (int i=0;i<length;i++) {
		v8::Local<v8::String> key = keys->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked();
		// TODO vahvarh
		// JS_GLOBAL->ForceDelete(key);
		(void)JS_GLOBAL->Delete(JS_CONTEXT,key);
	}
}

/**
 * Retrieve a configuration value
 */
v8::Local<v8::Value> TeaJS_App::get_config(std::string name) {
	v8::Local<v8::Value> config = JS_GLOBAL->Get(JS_CONTEXT,JS_STR("Config")).ToLocalChecked();
	if (!config->IsObject()) { return JS_UNDEFINED; }
	return config->ToObject(JS_CONTEXT).ToLocalChecked()->Get(JS_CONTEXT,JS_STR(name.c_str())).ToLocalChecked();
}

/**
 * Build module-specific require
 */
v8::Local<v8::Function> TeaJS_App::build_require(std::string path, void (*func) (const v8::FunctionCallbackInfo<v8::Value>&)) {
	std::string root = path_dirname(path);
	v8::Local<v8::FunctionTemplate> requiretemplate = v8::FunctionTemplate::New(JS_ISOLATE, func, JS_STR(root.c_str()));
	v8::Local<v8::Function> require = requiretemplate->GetFunction(JS_CONTEXT).ToLocalChecked();
	v8::Local<v8::Array> paths = v8::Local<v8::Array>::New(JS_ISOLATE, this->paths);
	(void)require->Set(JS_CONTEXT,JS_STR("paths"), paths);
	v8::Local<v8::Object> mainModule = v8::Local<v8::Object>::New(JS_ISOLATE, this->mainModule);
	(void)require->Set(JS_CONTEXT,JS_STR("main"), mainModule);
	return require;
}

void TeaJS_App::setup_teajs(v8::Local<v8::Object> target) {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	
	v8::Local<v8::Object> teajs = v8::Object::New(JS_ISOLATE);
	(void)teajs->Set(JS_CONTEXT,JS_STR("version"), JS_STR(STRING(VERSION)));
	(void)teajs->Set(JS_CONTEXT,JS_STR("instanceType"), JS_STR(this->instanceType()));
	(void)teajs->Set(JS_CONTEXT,JS_STR("executableName"), JS_STR(this->executableName()));
	
	(void)target->Set(JS_CONTEXT,JS_STR("TeaJS"), teajs);
	(void)target->Set(JS_CONTEXT,JS_STR("v8cgi"), teajs);
}
