#include <v8.h>
#include <string>
#include <vector>
#include "macros.h"
#include "gc.h"
#include "binary-b.h"
#include "bytestring.h"
#include "bytestorage-b.h"

#define WRONG_CTOR JS_TYPE_ERROR("ByteString called with wrong arguments.")

namespace {

v8::Global<v8::FunctionTemplate, v8::CopyablePersistentTraits<v8::FunctionTemplate> > _byteStringTemplate;
v8::Global<v8::Function, v8::CopyablePersistentTraits<v8::Function> > _byteString;

/**
 * ByteString constructor
 */
JS_METHOD(_ByteString) {
	if (!args.IsConstructCall()) { RETURN_CONSTRUCT_CALL; }

	try {
		int arglen = args.Length();
		switch (arglen) {
			case 0: /* empty */
				SAVE_PTR(0, new ByteStorageB());
			break;
			case 1: {
				if (args[0]->IsExternal()) { /* from a bytestorage */
					SAVE_VALUE(0, args[0]);
				} else if (args[0]->IsArray()) { /* array of numbers */
					v8::Local<v8::Array> arr = v8::Local<v8::Array>::Cast(args[0]);
					SAVE_PTR(0, new ByteStorageB(arr));
				} else if (args[0]->IsObject()) { /* copy constructor */
					v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(args[0]);
					if (IS_BINARY(obj)) {
						SAVE_PTR(0, new ByteStorageB(BS_OTHER(obj)));
					} else {
						WRONG_CTOR;
						return;
					}
				} else {
					WRONG_CTOR;
					return;
				}
			} break;
			case 2: {
				/* string, charset */
				v8::String::Utf8Value str(args[0]);
				v8::String::Utf8Value charset(args[1]);
				ByteStorageB bs_tmp((unsigned char *) (*str), str.length());
				ByteStorageB * bs = bs_tmp.transcode("utf-8", *charset);
				SAVE_PTR(0, bs);
			} break;
			default:
				WRONG_CTOR;
				return;
			break;
		}
	} catch (std::string e) {
		JS_ERROR(e.c_str());
		return;
	}

	GC * gc = GC_PTR;
	gc->add(args.This(), Binary_destroy);

	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_slice) {
	ByteStorageB * bs = BS_THIS;
	size_t len = args.Length();
	
	size_t start = (len > 0 ? args[0]->IntegerValue() : 0);
	int end_ = (len > 1 ? args[1]->IntegerValue() : bs->getLength());
	if (end_ < 0) { end_ += bs->getLength(); }
	size_t end = (end_ < 0 ? 0 : end_);
	
	ByteStorageB * bs2 = new ByteStorageB(bs, start, end);
	v8::Local<v8::Value> newargs[] = { v8::External::New(JS_ISOLATE, (void*)bs2) };

	v8::Local<v8::Function> byteString = v8::Local<v8::Function>::New(JS_ISOLATE, _byteString);
	args.GetReturnValue().Set(byteString->NewInstance(1, newargs));
}

JS_METHOD(_concat) {
	v8::Local<v8::Value> newargs[]= { args.This() };
	v8::Local<v8::Function> byteString = v8::Local<v8::Function>::New(JS_ISOLATE, _byteString);
	v8::Local<v8::Object> result = byteString->NewInstance(1, newargs);
	Binary_concat(result, args, true);
}

void _get(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
	ByteStorageB * bs = BS_OTHER(info.This());
	size_t len = bs->getLength();
	if (index < 0 || index >= len) { info.GetReturnValue().SetUndefined(); return; }
	
	ByteStorageB * bs2 = new ByteStorageB(bs, index, index+1);
	v8::Local<v8::Value> newargs[] = { v8::External::New(JS_ISOLATE, (void*)bs2) };
	v8::Local<v8::Function> byteString = v8::Local<v8::Function>::New(JS_ISOLATE, _byteString);
	info.GetReturnValue().Set(byteString->NewInstance(1, newargs));
}


} /* end namespace */

void ByteString_init(v8::Local<v8::FunctionTemplate> binaryTemplate) {
	v8::Local<v8::FunctionTemplate> byteStringTemplate = v8::FunctionTemplate::New(JS_ISOLATE, _ByteString);
	byteStringTemplate->Inherit(binaryTemplate);
	byteStringTemplate->SetClassName(JS_STR("ByteString"));
	_byteStringTemplate.Reset(JS_ISOLATE, byteStringTemplate);
	
	v8::Local<v8::ObjectTemplate> byteStringObject = byteStringTemplate->InstanceTemplate();
	byteStringObject->SetInternalFieldCount(1);	
	byteStringObject->SetAccessor(JS_STR("length"), Binary_length, 0, v8::Local<v8::Value>(), static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));
	byteStringObject->SetIndexedPropertyHandler(_get);

	v8::Local<v8::ObjectTemplate> byteStringPrototype = byteStringTemplate->PrototypeTemplate();
	byteStringPrototype->Set(JS_CONTEXT,JS_STR("slice"), v8::FunctionTemplate::New(JS_ISOLATE, _slice));
	byteStringPrototype->Set(JS_CONTEXT,JS_STR("concat"), v8::FunctionTemplate::New(JS_ISOLATE, _concat));

	_byteString.Reset(JS_ISOLATE, byteStringTemplate->GetFunction());
}

v8::Global<v8::Function, v8::CopyablePersistentTraits<v8::Function> > ByteString_function() {
	return _byteString;
}

v8::Global<v8::FunctionTemplate, v8::CopyablePersistentTraits<v8::FunctionTemplate> > ByteString_template() {
	return _byteStringTemplate;
}
