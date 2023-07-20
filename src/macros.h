/**
 * Shorthands for various lengthy V8 syntax constructs.
 */
#ifndef _JS_MACROS_H
#define _JS_MACROS_H

#include <string.h>
#include <unistd.h>
#include "app.h"
#include "lib/binary/bytestorage.h"

#define _STRING(x) #x
#define STRING(x) _STRING(x)

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))

#define JS_ISOLATE v8::Isolate::GetCurrent()

#define SAVE_PTR_TO(obj, index, ptr) obj->SetInternalField(index, v8::External::New(JS_ISOLATE, ptr))
#define LOAD_PTR_FROM(obj, index, type) reinterpret_cast<type>(v8::Local<v8::External>::Cast(obj->GetInternalField(index))->Value())
#define SAVE_PTR(index, ptr) SAVE_PTR_TO(args.This(), index, ptr)
#define LOAD_PTR(index, type) LOAD_PTR_FROM(args.This(), index, type)
#define SAVE_VALUE(index, val) args.This()->SetInternalField(index, val)
#define LOAD_VALUE(index) args.This()->GetInternalField(index)
#define JS_STR(str) v8::String::NewFromUtf8(JS_ISOLATE, str).ToLocalChecked()
#define JS_STR_LEN(str, len) v8::String::NewFromUtf8(JS_ISOLATE, str, v8::NewStringType::kNormal,len).ToLocalChecked()
#define JS_INT(val) v8::Integer::New(JS_ISOLATE, val)
#define JS_BIGINT(val) v8::BigInt::New(JS_ISOLATE, val)
#define JS_FLOAT(val) v8::Number::New(JS_ISOLATE, val)
#define JS_BOOL(val) v8::Boolean::New(JS_ISOLATE, val)
#define JS_NULL v8::Null(JS_ISOLATE)
#define JS_UNDEFINED v8::Undefined(JS_ISOLATE)
#define JS_METHOD(name) void name(const v8::FunctionCallbackInfo<v8::Value>& args)
#define INSTANCEOF(obj, func) func->HasInstance(obj)

#define JS_THROW(type, reason) JS_ISOLATE->ThrowException(v8::Exception::type(JS_STR(reason)))
v8::Local<v8::Value> JS_ERROR(const char*data);
//#define JS_ERROR(reason) fprintf(stderr,"JS_ERROR() - %s\n",reason); JS_THROW(Error, reason)
#define JS_TYPE_ERROR(reason) JS_THROW(TypeError, reason)
#define JS_RANGE_ERROR(reason) JS_THROW(RangeError, reason)
#define JS_SYNTAX_ERROR(reason) JS_THROW(SyntaxError, reason)
#define JS_REFERENCE_ERROR(reason) JS_THROW(ReferenceError, reason)
#define JS_RETHROW(tc) v8::Local<v8::Value>::New(tc.Exception());

#define JS_GLOBAL JS_ISOLATE->GetCurrentContext()->Global()
#define JS_CONTEXT JS_ISOLATE->GetCurrentContext()
#define GLOBAL_PROTO v8::Local<v8::Object>::Cast(JS_GLOBAL->GetPrototype())
#define APP_PTR reinterpret_cast<TeaJS_App *>(v8::Local<v8::External>::Cast(GLOBAL_PROTO->GetInternalField(0))->Value())
#define GC_PTR reinterpret_cast<GC *>(v8::Local<v8::External>::Cast(GLOBAL_PROTO->GetInternalField(1))->Value())

#define ASSERT_CONSTRUCTOR if (!args.IsConstructCall()) { JS_ERROR("Invalid call format. Please use the 'new' operator."); return; }
#define ASSERT_NOT_CONSTRUCTOR if (args.IsConstructCall()) { return JS_ERROR("Invalid call format. Please do not use the 'new' operator."); }
#define RETURN_CONSTRUCT_CALL JS_ERROR("TODO RETURN_CONSTRUCT_CALL()")
//#define RETURN_CONSTRUCT_CALL \
//	std::vector< v8::Local<v8::Value> > params(args.Length()); \
//	for (size_t i=0; i<params.size(); i++) { params[i] = args[(int)i]; } \
//	args.GetReturnValue().Set(args.Callee()->NewInstance(args.Length(), &params[0])); \
//	return;

#ifdef _WIN32
#   define SHARED_INIT() extern "C" __declspec(dllexport) void init(v8::Local<v8::Function> require, v8::Local<v8::Object> exports, v8::Local<v8::Object> module)
#else
#   define SHARED_INIT() extern "C" void init(v8::Local<v8::Function> require, v8::Local<v8::Object> exports, v8::Local<v8::Object> module)
#endif

inline v8::Local<v8::Value> BYTESTORAGE_TO_JS(ByteStorage * bs) {
	v8::Local<v8::Object> binary = v8::Local<v8::Object>::New(JS_ISOLATE, (APP_PTR)->require("binary", ""));
	v8::Local<v8::Function> buffer = v8::Local<v8::Function>::Cast(binary->Get(JS_CONTEXT,JS_STR("Buffer")).ToLocalChecked());
	v8::Local<v8::Value> newargs[] = { v8::External::New(JS_ISOLATE, (void*)bs) };
	return v8::Local<v8::Function>::Cast(buffer)->NewInstance(JS_CONTEXT,1, newargs).ToLocalChecked();
}

inline ByteStorage * JS_TO_BYTESTORAGE(v8::Local<v8::Value> value) {
	v8::Local<v8::Object> object = value->ToObject(JS_CONTEXT).ToLocalChecked();
	return LOAD_PTR_FROM(object, 0, ByteStorage *);
}

inline v8::Local<v8::Value> JS_BUFFER(char * data, size_t length) {
	ByteStorage * bs = new ByteStorage(data, length);
	return BYTESTORAGE_TO_JS(bs);
}

inline char * JS_BUFFER_TO_CHAR(v8::Local<v8::Value> value, size_t * size) {
	ByteStorage * bs = JS_TO_BYTESTORAGE(value);
	*size = bs->getLength();
	return bs->getData();
}

inline bool IS_BUFFER(v8::Local<v8::Value> value) {
	if (!value->IsObject()) { return false; }
	v8::Local<v8::Value> proto = value->ToObject(JS_CONTEXT).ToLocalChecked()->GetPrototype();
	// TODO vahvarh
	try {
		v8::Local<v8::Object> binary = v8::Local<v8::Object>::New(JS_ISOLATE, (APP_PTR)->require("binary", ""));
		v8::Local<v8::Value> prototype = binary->Get(JS_CONTEXT,JS_STR("Buffer")).ToLocalChecked()->ToObject(JS_CONTEXT).ToLocalChecked()->Get(JS_CONTEXT,JS_STR("prototype")).ToLocalChecked();
		return proto->Equals(JS_CONTEXT,prototype).ToChecked();
	} catch (std::string e) { // for some reasons, the binary module is not available
		return false;
	}
}

inline void READ(FILE * stream, size_t amount, const v8::FunctionCallbackInfo<v8::Value>& args) {
	std::string data;
	size_t size = 0;

   if (amount == 0) { /* all */
		size_t tmp;
		char * buf = new char[1024];
		do {
			tmp = fread((void *) buf, sizeof(char), sizeof(buf), stream);
			size += tmp;
			data.insert(data.length(), buf, tmp);
		} while (tmp == sizeof(buf));
		delete[] buf;
	} else {
		char * tmp = new char[amount];
		size = fread((void *) tmp, sizeof(char), amount, stream);
		data.insert(0, tmp, size);
		delete[] tmp;
	}
	args.GetReturnValue().Set(JS_BUFFER((char *) data.data(), size));
	return;
}

inline void READ_NONBLOCK(int fd, size_t amount, const v8::FunctionCallbackInfo<v8::Value>& args) {
	v8::Local<v8::Object> ret = v8::Object::New(JS_ISOLATE);
	std::string data;
	size_t size = 0;

   if (amount == 0) { /* all */
		size_t tmp;
		char * buf = new char[1024];
		while (true) {
			tmp = read(fd, (void *) buf, sizeof(char) * sizeof(buf));
			if (tmp == -1) {
				(void)ret->Set(JS_CONTEXT, JS_STR("info"), JS_INT(-1)); // need to read again
				break;
			}
			else if (tmp == 0) {
				(void)ret->Set(JS_CONTEXT, JS_STR("info"), JS_INT(0)); // don't need to read again
				break;
			}
			size += tmp;
			data.insert(data.length(), buf, tmp);
		}
		delete[] buf;
	} else {
		char * tmp = new char[amount];
		size = read(fd, (void *) tmp, sizeof(char) * amount);
		if (size == -1) {
			(void)ret->Set(JS_CONTEXT, JS_STR("info"), JS_INT(-1)); // need to read again
		}
		else if (size == 0) {
			(void)ret->Set(JS_CONTEXT, JS_STR("info"), JS_INT(0)); // don't need to read again
		}
		else {
			data.insert(0, tmp, size);
		}
		delete[] tmp;
	}
	//if (size > 0) {
	(void)ret->Set(JS_CONTEXT, JS_STR("result"), JS_BUFFER((char *) data.data(), size));
	(void)ret->Set(JS_CONTEXT, JS_STR("size"), JS_INT(size));
	args.GetReturnValue().Set(ret);
	return;
	//}
	//args.GetReturnValue().Set(JS_NULL);
}

inline void READ_LINE(FILE * stream, int amount, const v8::FunctionCallbackInfo<v8::Value>& args) {
	char * buf = new char[amount];
	v8::Local<v8::Value> result;
	
	char * r = fgets(buf, amount, stream);
	if (r) {
		result = JS_BUFFER(buf, strlen(buf));
	} else {
		result = JS_NULL;
	}
	delete[] buf;

	args.GetReturnValue().Set(result);
}

inline size_t WRITE(FILE * stream, v8::Local<v8::Value> data) {
	if (IS_BUFFER(data)) {
		size_t size = 0;
		char * cdata = JS_BUFFER_TO_CHAR(data, &size);
		int result = fwrite(cdata, sizeof(char), size, stream);
		fflush(stream);
		return result;
	} else {
		v8::String::Utf8Value utfdata(JS_ISOLATE,data);
		int result = fwrite(*utfdata, sizeof(char), utfdata.length(), stream);
		fflush(stream);
		return result;
	}
}

inline size_t WRITE_LINE(FILE * stream, v8::Local<v8::Value> data) {
	size_t result = 0;
	result += WRITE(stream, data);
	char newline = '\n';
	result += fwrite(&newline, sizeof(char), 1, stream);
	return result;
}

void write_debug(const char *text);

#endif
