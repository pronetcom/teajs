#include <v8.h>
#include <string>
#include "macros.h"
#include "gc.h"
#include "binary-b.h"
#include "bytestring.h"
#include "bytearray.h"
#include "bytestorage-b.h"

void commonIndexOf(const v8::FunctionCallbackInfo<v8::Value>& args, int direction) {
	ByteStorageB * bs = BS_THIS;
	int len = args.Length();

	unsigned char value = (unsigned char) args[0]->IntegerValue();
	size_t start = (len > 1 ? args[1]->IntegerValue() : 0);
	size_t end = (len > 2 ? args[2]->IntegerValue() : bs->getLength()-1);
	size_t index1 = MIN(start, end);
	size_t index2 = MAX(start, end);

	args.GetReturnValue().Set(JS_INT(bs->indexOf(value, index1, index2, direction)));
}

void Binary_length(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
	ByteStorageB * bs = BS_OTHER(info.This());
	info.GetReturnValue().Set(JS_INT(bs->getLength()));
}

void Binary_convertTo(const v8::FunctionCallbackInfo<v8::Value> &args, v8::Local<v8::Function> ctor) {
	if (args.Length() == 0) {
		/* no copying for ByteString->ByteString conversions */
		v8::Local<v8::FunctionTemplate> byteString_template = v8::Local<v8::FunctionTemplate>::New(JS_ISOLATE, ByteString_template());
		if (INSTANCEOF(args.This(), byteString_template) && ctor == ByteString_function()) {
			args.GetReturnValue().Set(args.This());
			return;
		}
		
		v8::Local<v8::Value> newargs[]= { args.This() };
		args.GetReturnValue().Set(ctor->NewInstance(1, newargs));
	} else {
		ByteStorageB * bs = BS_THIS;
		v8::String::Utf8Value from(args[0]);
		v8::String::Utf8Value to(args[1]);
		try {		
			ByteStorageB * bs2 = bs->transcode(*from, *to);
			v8::Local<v8::Value> newargs[]= { v8::External::New(JS_ISOLATE, (void*) bs2) };
			args.GetReturnValue().Set(ctor->NewInstance(1, newargs));
		} catch (std::string e) {
			JS_ERROR(e.c_str());
		}
	}
	
}

JS_METHOD(Binary_indexOf) {
	commonIndexOf(args, 1);
}

JS_METHOD(Binary_lastIndexOf) {
	commonIndexOf(args, -1);
}

JS_METHOD(Binary_codeAt) {
	ByteStorageB * bs = BS_THIS;
	size_t len = bs->getLength();
	size_t index = args[0]->IntegerValue();
	if (index < 0 || index >= len) { args.GetReturnValue().SetUndefined(); return; }
	
	args.GetReturnValue().Set(JS_INT(bs->getByte(index)));
}

JS_METHOD(Binary_decodeToString) {
	ByteStorageB * bs = BS_THIS;
	v8::String::Utf8Value charset(args[0]);
	try {
		ByteStorageB bs_tmp(bs->transcode(*charset, "utf-8"));
		args.GetReturnValue().Set(bs_tmp.toString());
	} catch (std::string e) {
		JS_ERROR(e.c_str());
	}
}

JS_METHOD(Binary_toByteString) {
	v8::Local<v8::Function> byteString = v8::Local<v8::Function>::New(JS_ISOLATE, ByteString_function());
	Binary_convertTo(args, byteString);
}

JS_METHOD(Binary_toByteArray) {
	v8::Local<v8::Function> byteArray = v8::Local<v8::Function>::New(JS_ISOLATE, ByteArray_function());
	Binary_convertTo(args, byteArray);
}

JS_METHOD(Binary_concat) {
	args.GetReturnValue().Set(args.This());
}

void Binary_destroy(v8::Local<v8::Object> instance) {
	ByteStorageB * bs = BS_OTHER(instance);
	delete bs;
}

void Binary_concat(v8::Local<v8::Object> obj, const v8::FunctionCallbackInfo<v8::Value>& args, bool right) {
	ByteStorageB * bs = BS_OTHER(obj);
	
	v8::Local<v8::Value> arg;
	for (int i=0;i<args.Length();i++) {
		arg = args[i];
		
		if (arg->IsObject() && IS_BINARY(arg->ToObject())) {
			ByteStorageB * bs2 = BS_OTHER(arg->ToObject());
			(right ? bs->push(bs2) : bs->unshift(bs2));
		} else if (arg->IsArray()) {
			v8::Local<v8::Array> arr = v8::Local<v8::Array>::Cast(arg);
			ByteStorageB bs_tmp(arr);
			(right ? bs->push(&bs_tmp) : bs->unshift(&bs_tmp));
		} else {
			unsigned char byte = (unsigned char) arg->IntegerValue();
			(right ? bs->push(byte) : bs->unshift(byte));
		}
	}
	
	args.GetReturnValue().Set(JS_INT(bs->getLength()));
}

JS_METHOD(_Binary) {
	JS_ERROR("Binary function should never be called.");
}

SHARED_INIT() {
	v8::LocalScope handle_scope(JS_ISOLATE);
	
	v8::Local<v8::FunctionTemplate> binaryTemplate = v8::FunctionTemplate::New(JS_ISOLATE, _Binary);
	binaryTemplate->SetClassName(JS_STR("Binary"));
	
	v8::Local<v8::ObjectTemplate> binaryPrototype = binaryTemplate->PrototypeTemplate();
	binaryPrototype->Set(JS_CONTEXT,JS_STR("codeAt"), v8::FunctionTemplate::New(JS_ISOLATE, Binary_codeAt));
	binaryPrototype->Set(JS_CONTEXT,JS_STR("indexOf"), v8::FunctionTemplate::New(JS_ISOLATE, Binary_indexOf));
	binaryPrototype->Set(JS_CONTEXT,JS_STR("lastIndexOf"), v8::FunctionTemplate::New(JS_ISOLATE, Binary_lastIndexOf));
	binaryPrototype->Set(JS_CONTEXT,JS_STR("toByteString"), v8::FunctionTemplate::New(JS_ISOLATE, Binary_toByteString));
	binaryPrototype->Set(JS_CONTEXT,JS_STR("toByteArray"), v8::FunctionTemplate::New(JS_ISOLATE, Binary_toByteArray));
	binaryPrototype->Set(JS_CONTEXT,JS_STR("decodeToString"), v8::FunctionTemplate::New(JS_ISOLATE, Binary_decodeToString));
	binaryPrototype->Set(JS_CONTEXT,JS_STR("concat"), v8::FunctionTemplate::New(JS_ISOLATE, Binary_concat));
	exports->Set(JS_CONTEXT,JS_STR("Binary"), binaryTemplate->GetFunction());

	ByteString_init(binaryTemplate);
	v8::Local<v8::Function> byteString = v8::Local<v8::Function>::New(JS_ISOLATE, ByteString_function());
	exports->Set(JS_CONTEXT,JS_STR("ByteString"), byteString);

	ByteArray_init(binaryTemplate);
	v8::Local<v8::Function> byteArray = v8::Local<v8::Function>::New(JS_ISOLATE, ByteArray_function());
	exports->Set(JS_CONTEXT,JS_STR("ByteArray"), byteArray);
}
