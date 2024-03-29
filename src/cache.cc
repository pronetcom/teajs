#include <string>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>

#include "macros.h"
#include "cache.h"
#include "common.h"

#ifndef windows
#   include <dlfcn.h>
#else
#   include <windows.h>
#   define dlopen(x,y) (void*)LoadLibrary(x)
#   define dlclose(x) FreeLibrary((HMODULE)x)
#endif

/**
 * Is this file already cached?
 */
bool Cache::isCached(std::string filename) {
	struct stat st;
	int result = stat(filename.c_str(), &st);
	if (result != 0) { return false; }

	TimeValue::iterator it = modified.find(filename);
	if (it == modified.end()) { return false; } /* not seen yet */
	
	if (it->second != st.st_mtime) { /* was modified */
		erase(filename);
		return false;
	}
	return true;
}

/**
 * Mark filename as "cached"
 * */
void Cache::mark(std::string filename) {
	struct stat st;
	stat(filename.c_str(), &st);
	modified[filename] = st.st_mtime;
}

/**
 * Remove file from all available caches
 */
void Cache::erase(std::string filename) {
	HandleValue::iterator it2 = handles.find(filename);
	if (it2 != handles.end()) { 
		dlclose(it2->second);
		handles.erase(it2); 
	}
	
	ScriptValue::iterator it3 = scripts.find(filename);
	if (it3 != scripts.end()) { 
		it3->second.Reset();
		scripts.erase(it3); 
	}
}

/**
 * Return source code for a given file
 */
std::string Cache::getSource(std::string filename) {
	FILE * file = fopen(filename.c_str(), "rb");
	if (file == NULL) { 
		std::string s = "Error reading '";
		s += filename;
		s += "'";
		
		throw s; 
	}
	
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	rewind(file);
	char* chars = new char[size + 1];
	chars[size] = '\0';
	for (unsigned int i = 0; i < size;) {
		size_t read = fread(&chars[i], 1, size - i, file);
		i += read;
	}
	fclose(file);
	std::string source = chars;
	delete[] chars;

	/* remove shebang line */
	if (source.find('#',0) == 0 && source.find('!',1) == 1 ) {
		long pfix = source.find('\n',0);
		source.erase(0,pfix);
	};
	
	source = this->wrapExports(source);
	return source;
}

/**
 * Return dlopen handle for a given file
 */
void * Cache::getHandle(std::string filename) {
#ifdef VERBOSE
	printf("[getHandle] cache try for '%s' .. ", filename.c_str()); 
#endif	
	if (this->isCached(filename)) {
#ifdef VERBOSE
		printf("cache hit\n"); 
#endif	
		HandleValue::iterator it = handles.find(filename);
		return it->second;
	} else {
#ifdef VERBOSE
		printf("cache miss\n"); 
#endif	

#ifdef windows
		SetErrorMode(SEM_FAILCRITICALERRORS);
#endif
		void * handle = dlopen(filename.c_str(), RTLD_LAZY);
		if (!handle) { 
			std::string error = "Error opening shared library '";
			error += filename;
			error += "' (";
			
#ifdef windows
			int size = 0xFF;
			char buf[size];
			buf[size-1] = '\0';
			int e = GetLastError();
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, e, 0, buf, size-1, NULL);
			char estr[100];
			snprintf(estr, sizeof(estr), "%i", e);

			error += "code ";
			error += estr;
			error += ", ";
			error += buf;	
#else
			error += dlerror();
#endif
			error += ")";
			
			throw error; // used only lrequire->load_dso, so it is in try/catch block in JS_METHOD _require, so using normal exception
		}
		this->mark(filename); /* mark as cached */
		handles[filename] = handle;
		return handle;
	}
}

/**
 * Return compiled script from a given file
 */
v8::Persistent<v8::Script, v8::CopyablePersistentTraits<v8::Script> > Cache::getScript(std::string filename) {
#ifdef VERBOSE
	printf("[getScript] cache try for '%s' .. ", filename.c_str()); 
#endif	
	if (this->isCached(filename)) {
#ifdef VERBOSE
		printf("[getScript] cache hit\n"); 
#endif	
		ScriptValue::iterator it = scripts.find(filename);
		return it->second;
	} else {
#ifdef VERBOSE
		printf("[getScript] cache miss\n"); 
#endif
		std::string source = this->getSource(filename);
		/* context-independent compiled script */
		v8::ScriptOrigin origin(JS_ISOLATE,JS_STR(filename.c_str()));
		//v8::Local<v8::Script> script = v8::Script::Compile(JS_CONTEXT,JS_STR(source.c_str()),&origin).ToLocalChecked();
		v8::MaybeLocal<v8::Script> script_ml = v8::Script::Compile(JS_CONTEXT,JS_STR(source.c_str()),&origin);
		v8::Persistent<v8::Script, v8::CopyablePersistentTraits<v8::Script> > _script;
		if (script_ml.IsEmpty()) {
			return _script;
		}
		v8::Local<v8::Script> script = script_ml.ToLocalChecked();
		_script.Reset(JS_ISOLATE, script);

		if (!script.IsEmpty()) {
			this->mark(filename); /* mark as cached */
			scripts[filename] = _script;
			return _script;
		}
		return _script;
	}
}

/**
 * Return exports object for a given file
 */
v8::Persistent<v8::Object, v8::CopyablePersistentTraits<v8::Object> > Cache::getExports(std::string filename) {
	ExportsValue::iterator it = exports.find(filename);
	if (it != exports.end()) { 
#ifdef VERBOSE
		printf("[getExports] using cached exports for '%s'\n", filename.c_str()); 
#endif	
		return it->second;
	} else {
#ifdef VERBOSE
		printf("[getExports] '%s' has no cached exports\n", filename.c_str()); 
#endif
		return v8::Persistent<v8::Object, v8::CopyablePersistentTraits<v8::Object> >();
		//return v8::Global<v8::Object, v8::CopyablePersistentTraits<v8::Object> >();
	}
}

/**
 * Add a single item to exports cache
 */
void Cache::addExports(std::string filename, v8::Local<v8::Object> obj) {
#ifdef VERBOSE
		printf("[addExports] caching exports for '%s'\n", filename.c_str()); 
#endif	
	exports[filename] = v8::Persistent<v8::Object, v8::CopyablePersistentTraits<v8::Object> >();
	//exports[filename] = v8::Global<v8::Object, v8::CopyablePersistentTraits<v8::Object> >();
	exports[filename].Reset(JS_ISOLATE, obj);
}

/**
 * Remove a cached exports object
 */
void Cache::removeExports(std::string filename) {
#ifdef VERBOSE
		printf("[removeExports] removing exports for '%s'\n", filename.c_str()); 
#endif	
	ExportsValue::iterator it = exports.find(filename);
	if (it != exports.end()) { 
		it->second.Reset();
		exports.erase(it);
	}
}

/**
 * Remove all cached exports
 */
void Cache::clearExports() {
	ExportsValue::iterator it;
	for (it=exports.begin(); it != exports.end(); it++) {
		it->second.Reset();
	}
	exports.clear();
}

/**
 * Wrap a string with exports envelope
 */
std::string Cache::wrapExports(std::string code) {
	std::string result = "";
	result += "(function(require,exports,module){";
	result += code;
	result += "\n})";
	return result;
}
