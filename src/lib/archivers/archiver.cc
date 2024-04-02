#include <v8.h>
#include "macros.h"
#include "archiverBase.h"
#include "archiverZip.h"

#include <string>
#include <cstring>
#include <ctime>
#include <cstdlib>

#define ARCHIVER_OTHER(object) LOAD_PTR_FROM(object, 0, ArchiverBase *)
#define ARCHIVER_THIS ARCHIVER_OTHER(args.This())
#define WRONG_CTOR JS_TYPE_ERROR("Archiver() called with wrong arguments.")
#define WRONG_START_STOP JS_RANGE_ERROR("Archiver() Invalid start/stop numbers")
#define WRONG_SIZE JS_RANGE_ERROR("Archiver() Invalid size")

namespace {

v8::Global<v8::FunctionTemplate> _archiverTemplate;
v8::Global<v8::Function> _archiver;

void destroyArchiver(void* temp) {
	ArchiverBase* archiver = (ArchiverBase*)temp;
	delete archiver;
}

JS_METHOD(_Archiver) {
	if (!args.IsConstructCall()) { RETURN_CONSTRUCT_CALL; }
	if (args.Length() != 1) { WRONG_CTOR; return; }
	if (!args[0]->IsNumber()) { WRONG_CTOR; return; }

	ArchiverBase* archiver;
	int archiverType = (int)args[0]->IntegerValue(JS_CONTEXT).ToChecked();

	switch (archiverType)
	{
	case ArchiverBase::ZIP:
		archiver = new ArchiverZip();
		break;
	
	default:
		JS_ERROR("Unknown type of archiver in constructor");
		return;
	}
	SAVE_PTR(0, archiver);
	
	GC * gc = GC_PTR;
	gc->add(args.This(), destroyArchiver, 0);
	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_open) {
	if (args.Length() != 2) { JS_ERROR("Wrong number of arguments"); return; }
	if (!args[0]->IsString()) { JS_ERROR("Wrong type of first argument"); return; }
	if (!args[1]->IsNumber()) { JS_ERROR("Wrong type of second argument"); return; }
	
	v8::String::Utf8Value str(JS_ISOLATE, args[0]);
	int flags = (int)args[1]->IntegerValue(JS_CONTEXT).ToChecked();
	ArchiverBase* archiver = ARCHIVER_THIS;

	if (!archiver->open(*str, flags)) {
		JS_ERROR(archiver->getError());
		return;
	}
}

JS_METHOD(_addFile) {
	if (args.Length() != 3) { JS_ERROR("Wrong number of arguments"); return; }
	if (!args[0]->IsString()) { JS_ERROR("Wrong type of first argument"); return; }
	if (!args[1]->IsString()) { JS_ERROR("Wrong type of first argument"); return; }
	if (!args[2]->IsBoolean()) { JS_ERROR("Wrong type of second argument"); return; }
	
	v8::String::Utf8Value name(JS_ISOLATE, args[0]);
	v8::String::Utf8Value source(JS_ISOLATE, args[1]);
	bool useAsBuffer = args[2]->ToBoolean(JS_ISOLATE)->BooleanValue(JS_ISOLATE);
	ArchiverBase* archiver = ARCHIVER_THIS;
	if (archiver->addFile(*name, *source, source.length(), useAsBuffer) < 0) {
		JS_ERROR(archiver->getError());
		return;
	}
}

JS_METHOD(_addDir) {
	if (args.Length() != 1) { JS_ERROR("Wrong number of arguments"); return; }
	if (!args[0]->IsString()) { JS_ERROR("Wrong type of first argument"); return; }
	
	v8::String::Utf8Value name(JS_ISOLATE, args[0]);
	ArchiverBase* archiver = ARCHIVER_THIS;
	if (archiver->addDir(*name) < 0) {
		JS_ERROR(archiver->getError());
		return;
	}
}

JS_METHOD(_readFile) {
	if (args.Length() != 1 && args.Length() != 2) { JS_ERROR("Wrong number of arguments"); return; }
	if ((!args[0]->IsString()) && (!args[0]->IsNumber())) { JS_ERROR("Wrong type of first argument"); return; }
	ArchiverBase* archiver = ARCHIVER_THIS;
	ByteStorage* result = nullptr;
	int64_t len = 0;
	if (args.Length() == 1) {
		if (args[0]->IsString()) {
			v8::String::Utf8Value name(JS_ISOLATE, args[0]);
			len = archiver->readFileByName(*name, 10000, result);
		}
		else {
			int64_t index = (int64_t)args[0]->IntegerValue(JS_CONTEXT).ToChecked();
			len = archiver->readFileByIndex(index, 10000, result);
		}
	}
	else {
		if (!args[1]->IsNumber()) { JS_ERROR("Wrong type of second argument"); return; }
		int64_t maxSize = (int64_t)args[1]->IntegerValue(JS_CONTEXT).ToChecked();
		if (maxSize <= 0) { JS_ERROR("Incorrect value of second argument"); return; }

		if (args[0]->IsString()) {
			v8::String::Utf8Value name(JS_ISOLATE, args[0]);
			len = archiver->readFileByName(*name, maxSize, result);
		}
		else {
			int64_t index = (int64_t)args[0]->IntegerValue(JS_CONTEXT).ToChecked();
			len = archiver->readFileByIndex(index, maxSize, result);
		}
	}

	if (len < 0) {
		JS_ERROR(archiver->getError());
		return;
	}
	args.GetReturnValue().Set(BYTESTORAGE_TO_JS(result));
	return;
}

JS_METHOD(_close) {
	if (args.Length() != 0) { JS_ERROR("Wrong number of arguments"); return; }
	ArchiverBase* archiver = ARCHIVER_THIS;
	if (archiver->close()) {
		JS_ERROR(archiver->getError());
	}
}

} /* namespace */

SHARED_INIT() {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	
	v8::Local<v8::FunctionTemplate> archiverTemplate = v8::FunctionTemplate::New(JS_ISOLATE, _Archiver);
	archiverTemplate->SetClassName(JS_STR("Archiver"));
	_archiverTemplate.Reset(JS_ISOLATE, archiverTemplate);

	/**
	 * Constants (Archiver.*)
	 */
	// should be used in constructor
	archiverTemplate->Set(JS_ISOLATE, "ZIP"	, JS_INT(ArchiverBase::ZIP));

	// other constants
	archiverTemplate->Set(JS_ISOLATE, "ZIP_CHECKCONS"	, JS_INT(ZIP_CHECKCONS)); 
	archiverTemplate->Set(JS_ISOLATE, "ZIP_CREATE"		, JS_INT(ZIP_CREATE)); 
	archiverTemplate->Set(JS_ISOLATE, "ZIP_EXCL"		, JS_INT(ZIP_EXCL)); 
	archiverTemplate->Set(JS_ISOLATE, "ZIP_TRUNCATE"	, JS_INT(ZIP_TRUNCATE));
	archiverTemplate->Set(JS_ISOLATE, "ZIP_RDONLY"		, JS_INT(ZIP_RDONLY));

	v8::Local<v8::ObjectTemplate> archiverInstance = archiverTemplate->InstanceTemplate();
	archiverInstance->SetInternalFieldCount(1); /* archiver */
	
	v8::Local<v8::ObjectTemplate> archiverPrototype = archiverTemplate->PrototypeTemplate();
	archiverPrototype->Set(JS_ISOLATE, "open"			, v8::FunctionTemplate::New(JS_ISOLATE, _open));
	archiverPrototype->Set(JS_ISOLATE, "addFile"		, v8::FunctionTemplate::New(JS_ISOLATE, _addFile));  // should be used only with absolute path
	archiverPrototype->Set(JS_ISOLATE, "addDir"			, v8::FunctionTemplate::New(JS_ISOLATE, _addDir));
	archiverPrototype->Set(JS_ISOLATE, "readFile"		, v8::FunctionTemplate::New(JS_ISOLATE, _readFile));
	archiverPrototype->Set(JS_ISOLATE, "close"			, v8::FunctionTemplate::New(JS_ISOLATE, _close));

	/*
	v8::Local<v8::ObjectTemplate> bufferObject = archiverTemplate->InstanceTemplate();
	bufferObject->SetInternalFieldCount(1);	
	bufferObject->SetAccessor(JS_STR("length"), Buffer_length, 0, v8::Local<v8::Value>(), static_cast<v8::PropertyAttribute>(v8::DontDelete));
	bufferObject->SetIndexedPropertyHandler(Buffer_get, Buffer_set);
	*/

	(void)exports->Set(JS_CONTEXT,JS_STR("Archiver"), archiverTemplate->GetFunction(JS_CONTEXT).ToLocalChecked());
	_archiver.Reset(JS_ISOLATE, archiverTemplate->GetFunction(JS_CONTEXT).ToLocalChecked());
}
