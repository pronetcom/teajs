#include <v8.h>
#include <string>
#include <vector>
#include "macros.h"
#include "gc.h"
#include "bytestorage.h"

#define BS_OTHER(object) LOAD_PTR_FROM(object, 0, ByteStorage *)
#define BS_THIS BS_OTHER(args.This())
#define WRONG_CTOR JS_TYPE_ERROR("Buffer() called with wrong arguments.")
#define WRONG_START_STOP JS_RANGE_ERROR("Buffer() Invalid start/stop numbers")
#define WRONG_SIZE JS_RANGE_ERROR("Buffer() Invalid size")

namespace {

v8::Global<v8::FunctionTemplate> _bufferTemplate;
v8::Global<v8::Function> _buffer;

size_t firstIndex(v8::Local<v8::Value> index, size_t length) {
	size_t i = 0;
	if (!index->IsUndefined()) { i = index->IntegerValue(JS_CONTEXT).ToChecked(); }
	if (i < 0) { i += length; }
	
	if (i < 0) { i = 0; }
	if (i > length) { i = length; }
	return i;
}

size_t lastIndex(v8::Local<v8::Value> index, size_t length) {
	size_t i = length;
	if (!index->IsUndefined()) { i = index->IntegerValue(JS_CONTEXT).ToChecked(); }
	if (i < 0) { i += length; }
	
	if (i < 0) { i = 0; }
	if ( i > length) { i = length; }
	return i;
}

/*
void Buffer_destroy(v8::Local<v8::Object> instance) {
	fprintf(stderr,"Buffer_destroy() - TODO\n");exit(1);
	fprintf(stderr,"Buffer_destroy()\n");
	ByteStorage * bs = BS_OTHER(instance);
	fprintf(stderr,"Buffer_destroy() bs=%ld\n",(void*)bs);
	delete bs;
}
*/

void Buffer_destroy2(void *tmp) {
	ByteStorage *bs=(ByteStorage *)tmp;
	//fprintf(stderr,"Buffer_destroy2() bs=%ld\n",(void*)bs);
	delete bs;
}



void Buffer_fromBuffer(const v8::FunctionCallbackInfo<v8::Value>& args, v8::Local<v8::Object> obj) {
	ByteStorage * bs2 = BS_OTHER(obj);

	int index1 = (int)firstIndex(args[1], bs2->getLength());
	int index2 = (int)lastIndex(args[2], bs2->getLength());
	if (index1>index2) { WRONG_START_STOP; return; }

	bool copy = true;
	if (!args[3]->IsUndefined()) { copy = args[3]->ToBoolean(JS_ISOLATE)->Value(); }
	
	ByteStorage * bs;
	if (copy) {
		size_t length = index2-index1;
		bs = new ByteStorage(length);
		bs->fill(bs2->getData() + index1, length);
	} else {
		bs = new ByteStorage(bs2, index1, index2);
	}
	
	SAVE_PTR(0, bs);
	args.GetReturnValue().Set(v8::Local<v8::Value>());
}

void Buffer_fromString(const v8::FunctionCallbackInfo<v8::Value>& args) {
	if (args.Length() < 2) { WRONG_CTOR; return; }
	v8::String::Utf8Value str(JS_ISOLATE,args[0]);
	v8::String::Utf8Value charset(JS_ISOLATE,args[1]);
	
	ByteStorage bs_tmp((char *) (*str), str.length());
	try
	{
		ByteStorage * bs = bs_tmp.transcode("utf-8", *charset);
		SAVE_PTR(0, bs);
		args.GetReturnValue().Set(v8::Local<v8::Value>());
	} catch (std::string e) {
		JS_ERROR(e);
	}
}

void Buffer_fromArray(const v8::FunctionCallbackInfo<v8::Value>& args) {
	v8::Local<v8::Array> arr = v8::Local<v8::Array>::Cast(args[0]);
	size_t index1 = firstIndex(args[1], arr->Length());
	size_t index2 = lastIndex(args[2], arr->Length());
	if (index1>index2) { WRONG_START_STOP; return; }
	ByteStorage * bs = new ByteStorage(index2 - index1);
	
	size_t index = 0;
	for (size_t i=index1; i<index2; i++) {
		char value = (char) arr->Get(JS_CONTEXT,(int)i).ToLocalChecked()->IntegerValue(JS_CONTEXT).ToChecked();
		bs->setByte(index++, value);
	}
	SAVE_PTR(0, bs);
	args.GetReturnValue().Set(v8::Local<v8::Value>());
}

JS_METHOD(_Buffer) {
	if (!args.IsConstructCall()) { RETURN_CONSTRUCT_CALL; }
	if (args.Length() == 0) { WRONG_CTOR; return; }

	ByteStorage *bs;
	// TODO vahvarh try
	try {
		if (args[0]->IsExternal()) { /* from a bytestorage */
			v8::Local<v8::External> ext = v8::Local<v8::External>::Cast(args[0]);
			SAVE_PTR(0, ext->Value());
		} else if (args[0]->IsNumber()) { /* length, [fill] */
			char fill = (char) (args.Length() > 1 ? (char) args[1]->IntegerValue(JS_CONTEXT).ToChecked() : 0);
			int len=(int)args[0]->IntegerValue(JS_CONTEXT).ToChecked();
			if(len<0) {
				WRONG_SIZE;
				return;
			}
			bs = new ByteStorage(len);
			bs->fill(fill);
			SAVE_PTR(0, bs);
		} else if (args[0]->IsArray()) { /* array of numbers */
			Buffer_fromArray(args);
			return;
		} else if (args[0]->IsObject()) { /* copy */
			v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(args[0]);
			v8::Local<v8::FunctionTemplate> bufferTemplate = v8::Local<v8::FunctionTemplate>::New(JS_ISOLATE, _bufferTemplate);
			if (INSTANCEOF(obj, bufferTemplate)) {
				Buffer_fromBuffer(args, obj);
				return;
			} else { WRONG_CTOR; return; }
		} else if (args[0]->IsString()) { /* string */
			Buffer_fromString(args);
			return;
		} else {
			WRONG_CTOR;
			return;
		}
	} catch (std::string e) {
		JS_ERROR(e);
		return;
	}
	
	bs=BS_OTHER(args.This());
	GC * gc = GC_PTR;
	//gc->add(args.This(), Buffer_destroy);
	gc->add(args.This(),Buffer_destroy2,0);
	/*fprintf(stderr,"JS_METHOD(_Buffer) bs=%ld\n",(void*)bs);
	args.This().SetWeak((void *)bs, &WeakCallback2,v8::WeakCallbackType::kParameter);*/

	args.GetReturnValue().Set(args.This());
}

JS_METHOD(Buffer_toString) {
	ByteStorage * bs = BS_THIS;

	if (args.Length() == 0) {
		size_t tmpSize = 100;
		std::string result = "[Buffer ";
		char * tmp = (char *) malloc(tmpSize);
		if (tmp) {
			size_t size = snprintf(tmp, tmpSize, "%lu", (unsigned long) bs->getLength());
			if (size < tmpSize) { result += tmp; }
			free(tmp);
		}
		result += "]";
		JS_STR(result.c_str());
		return;
	}
	
	v8::String::Utf8Value charset(JS_ISOLATE,args[0]);
	size_t index1 = firstIndex(args[1], bs->getLength());
	size_t index2 = lastIndex(args[2], bs->getLength());
	if (index1>index2) { WRONG_START_STOP; return; }
	ByteStorage view(bs, index1, index2);
	
	// TODO vahvarh try
	try {
		ByteStorage * bs2 = view.transcode(*charset, "utf-8");
		v8::Local<v8::Value> result = JS_STR_LEN((const char *) bs2->getData(),(int)bs2->getLength());
		delete bs2;
		args.GetReturnValue().Set(result);
	} catch (std::string e) {
		JS_ERROR(e);
	}
}

JS_METHOD(Buffer_range) {
	ByteStorage * bs = BS_THIS;
	size_t index1 = firstIndex(args[0], bs->getLength());
	size_t index2 = lastIndex(args[1], bs->getLength());
	if (index1>index2) { WRONG_START_STOP; return; }

	ByteStorage * bs2 = new ByteStorage(bs, index1, index2);
	v8::Local<v8::Value> newargs[] = { v8::External::New(JS_ISOLATE, (void*)bs2) };
	v8::Local<v8::Function> buffer = v8::Local<v8::Function>::New(JS_ISOLATE, _buffer);
	args.GetReturnValue().Set(buffer->NewInstance(JS_CONTEXT,1, newargs).ToLocalChecked());
}

JS_METHOD(Buffer_slice) {
	ByteStorage * bs = BS_THIS;
	size_t index1 = firstIndex(args[0], bs->getLength());
	size_t index2 = lastIndex(args[1], bs->getLength());
	if (index1>index2) { WRONG_START_STOP; return; }

	size_t length = index2-index1;
	ByteStorage * bs2 = new ByteStorage(bs->getData() + index1, length);
	
	v8::Local<v8::Value> newargs[] = { v8::External::New(JS_ISOLATE, (void*)bs2) };
	v8::Local<v8::Function> buffer = v8::Local<v8::Function>::New(JS_ISOLATE, _buffer);
	args.GetReturnValue().Set(buffer->NewInstance(JS_CONTEXT,1, newargs).ToLocalChecked());
}

JS_METHOD(Buffer_fill) {
	if (args.Length() == 0) { JS_TYPE_ERROR("Invalid value to fill"); return; }
	ByteStorage * bs = BS_THIS;
	size_t index1 = firstIndex(args[1], bs->getLength());
	size_t index2 = lastIndex(args[2], bs->getLength());
	if (index1>index2) { WRONG_START_STOP; return; }
	char fill = (char) args[0]->IntegerValue(JS_CONTEXT).ToChecked();

	for (size_t i = index1; i<index2; i++) {
		bs->setByte(i, fill);
	}

	args.GetReturnValue().Set(args.This());
}

/**
 * Generic copy implementation. Handler either Buffer->Array/Buffer, or Array/Buffer->Buffer copies.
 * @param {args} args
 * @param {bool} to We are copying *from* args.This()
 */
void Buffer_copy_impl(const v8::FunctionCallbackInfo<v8::Value>& args, bool source) {
	const char * errmsg = "First argument must be a Buffer or Array";
	ByteStorage * bs = BS_THIS;
	ByteStorage * bs2 = NULL;
	size_t length;
	v8::Local<v8::Array> arr;
	
	if (args.Length() == 0) { JS_TYPE_ERROR(errmsg); return; }
	if (args[0]->IsArray()) {
		arr = v8::Local<v8::Array>::Cast(args[0]);
		length = arr->Length();
	} else if (args[0]->IsObject()) {
		v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(args[0]);
		v8::Local<v8::FunctionTemplate> bufferTemplate = v8::Local<v8::FunctionTemplate>::New(JS_ISOLATE, _bufferTemplate);
		if (!INSTANCEOF(obj, bufferTemplate)) { JS_TYPE_ERROR(errmsg); return; }
		bs2 = BS_OTHER(obj);
		length = bs2->getLength();
	} else { JS_TYPE_ERROR(errmsg); return; }
	
	size_t offsetSource, offsetTarget, amount;
	if (source) {
		offsetSource = firstIndex(args[2], bs->getLength());
		offsetTarget = firstIndex(args[1], length);
		amount = MIN(length - offsetTarget, lastIndex(args[3], bs->getLength()) - offsetSource);
	} else {
		offsetSource = firstIndex(args[1], length);
		offsetTarget = firstIndex(args[2], bs->getLength());
		amount = MIN(length - offsetSource, lastIndex(args[3], bs->getLength()) - offsetTarget);
	}

	char byte;

	if (source) {
		for (size_t i=0; i<amount; i++) {
			byte = bs->getByte(i + offsetSource);
			if (bs2) {
				bs2->setByte(i + offsetTarget, byte);
			} else {
				(void)arr->Set(JS_CONTEXT,(int)(i + offsetTarget), JS_INT(byte));
			}
		}
	} else {
		for (size_t i=0; i<amount; i++) {
			if (bs2) {
				byte = bs2->getByte(i + offsetSource);
			} else {
				byte = arr->Get(JS_CONTEXT,(uint32_t)(i + offsetSource)).ToLocalChecked()->IntegerValue(JS_CONTEXT).ToChecked();
			}
			bs->setByte(i + offsetTarget, byte);
		}
	}

	args.GetReturnValue().Set(args.This());
}

JS_METHOD(Buffer_copy) {
	Buffer_copy_impl(args, true);
}

JS_METHOD(Buffer_copyFrom) {
	Buffer_copy_impl(args, false);
}

JS_METHOD(Buffer_read) {
	JS_ERROR("Buffer::read not yet implemented");
}

JS_METHOD(Buffer_write) {
	JS_ERROR("Buffer::write not yet implemented");
}

void Buffer_length(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
	ByteStorage * bs = BS_OTHER(info.This());
	info.GetReturnValue().Set((int)bs->getLength());
}

void Buffer_get(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
	ByteStorage * bs = BS_OTHER(info.This());
	size_t len = bs->getLength();
	if (index < 0 || index >= len) { JS_RANGE_ERROR("Non-existent index"); return; }

	info.GetReturnValue().Set(JS_INT((unsigned char) bs->getByte(index)));
}

void Buffer_set(uint32_t index, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info) {
	ByteStorage * bs = BS_OTHER(info.This());
	size_t len = bs->getLength();
	if (index < 0 || index >= len) { JS_RANGE_ERROR("Non-existent index"); return; }

	bs->setByte(index, (unsigned char) value->IntegerValue(JS_CONTEXT).ToChecked());
	info.GetReturnValue().Set(value);
}

} /* namespace */

SHARED_INIT() {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	
	v8::Local<v8::FunctionTemplate> bufferTemplate = v8::FunctionTemplate::New(JS_ISOLATE, _Buffer);
	bufferTemplate->SetClassName(JS_STR("Buffer"));
	_bufferTemplate.Reset(JS_ISOLATE, bufferTemplate);
	
	v8::Local<v8::ObjectTemplate> bufferPrototype = bufferTemplate->PrototypeTemplate();
	bufferPrototype->Set(JS_ISOLATE,"toString"	, v8::FunctionTemplate::New(JS_ISOLATE, Buffer_toString));
	bufferPrototype->Set(JS_ISOLATE,"range"		, v8::FunctionTemplate::New(JS_ISOLATE, Buffer_range));
	bufferPrototype->Set(JS_ISOLATE,"slice"		, v8::FunctionTemplate::New(JS_ISOLATE, Buffer_slice));
	bufferPrototype->Set(JS_ISOLATE,"fill"		, v8::FunctionTemplate::New(JS_ISOLATE, Buffer_fill));
	bufferPrototype->Set(JS_ISOLATE,"copy"		, v8::FunctionTemplate::New(JS_ISOLATE, Buffer_copy));
	bufferPrototype->Set(JS_ISOLATE,"copyFrom"	, v8::FunctionTemplate::New(JS_ISOLATE, Buffer_copyFrom));
	bufferPrototype->Set(JS_ISOLATE,"read"		, v8::FunctionTemplate::New(JS_ISOLATE, Buffer_read));
	bufferPrototype->Set(JS_ISOLATE,"write"		, v8::FunctionTemplate::New(JS_ISOLATE, Buffer_write));

	v8::Local<v8::ObjectTemplate> bufferObject = bufferTemplate->InstanceTemplate();
	bufferObject->SetInternalFieldCount(1);	
	bufferObject->SetAccessor(JS_STR("length"), Buffer_length, 0, v8::Local<v8::Value>(), v8::DEFAULT, static_cast<v8::PropertyAttribute>(v8::DontDelete));
	bufferObject->SetIndexedPropertyHandler(Buffer_get, Buffer_set);

	(void)exports->Set(JS_CONTEXT,JS_STR("Buffer"), bufferTemplate->GetFunction(JS_CONTEXT).ToLocalChecked());
	_buffer.Reset(JS_ISOLATE, bufferTemplate->GetFunction(JS_CONTEXT).ToLocalChecked());
}
