/**
 * Garbage collection support. Every C++ class can subscribe to be notified 
 * when its JS representation gets GC'ed.
 */

#include "gc.h"
#include "macros.h"

void GC::WeakCallback(const v8::WeakCallbackInfo<GCObject>& data) {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	
	GCObject *gcObject = data.GetParameter();
	//fprintf(stderr,"GC::WeakCallback() gcObject=%ld\n",(void*)gcObject);

	/*v8::Local<v8::Value> _object = v8::Local<v8::Value>::New(JS_ISOLATE, gcObject->_handle);
	//v8::Local<v8::Value> _object = v8::Local<v8::Value>::New(JS_ISOLATE, data.GetValue());
	v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(_object);

	if (gcObject->dtor) {
		gcObject->dtor(object);
	}
	if (gcObject->dtor_name) {
		v8::Local<v8::Function> fun = v8::Local<v8::Function>::Cast(object->Get(JS_CONTEXT,JS_STR(gcObject->dtor_name)).ToLocalChecked());
		(void)fun->Call(JS_CONTEXT,object, 0, NULL);
	}*/

	if (gcObject->dtor_t_ptr) {
		fprintf(stderr,"GC::WeakCallback() TODO dtor_name\n");exit(1);
	} else if (gcObject->dtor_v1_ptr) {
		fprintf(stderr,"GC::WeakCallback() TODO dtor_v1_ptr\n");exit(1);
		//gcObject->dtor_v1_ptr(gcObject->ptr);
	} else if (gcObject->dtor_v2_ptr) {
		gcObject->dtor_v2_ptr(data.GetInternalField(gcObject->internal_index));
	} else if (gcObject->dtor_name) {
		fprintf(stderr,"GC::WeakCallback() TODO dtor_name\n");exit(1);
	}
	gcObject->_handle.Reset();
	delete gcObject;
}

/*void GC::add(v8::Local<v8::Value> object, GC_dtor_t dtor) {
	fprintf(stderr,"GC::add() - TODO\n");exit(1);
	GCObject * gcObject = new GCObject();
	fprintf(stderr,"GC::add(v8::Local<v8::Value> object, GC::dtor_t dtor) gcObject=%ld\n",(void*)gcObject);
	gcObject->dtor_t_ptr = dtor;
	gcObject->_handle.Reset(JS_ISOLATE, object);
	gcObject->_handle.SetWeak(gcObject, &WeakCallback,v8::WeakCallbackType::kParameter);
	// TODO TODO vahvarh
	// gcObject->_handle.MarkIndependent();	
}
void GC::add(v8::Local<v8::Value> object, GC_dtor_v dtor,void*ptr)
{
	GCObject * gcObject = new GCObject();
	//fprintf(stderr,"GC::add(v8::Local<v8::Value> object, dtor_v,ptr) gcObject=%ld\n",(void*)gcObject);
	gcObject->dtor_v1_ptr = dtor;
	gcObject->ptr=ptr;
	gcObject->_handle.Reset(JS_ISOLATE, object);
	gcObject->_handle.SetWeak(gcObject, &WeakCallback,v8::WeakCallbackType::kParameter);
}

void GC::add(v8::Local<v8::Value> object, const char *name) {
	fprintf(stderr,"GC::add() - TODO\n");exit(1);
	GCObject * gcObject = new GCObject();
	fprintf(stderr,"GC::add(v8::Local<v8::Value> object,\"%s\") gcObject=%ld\n",(void*)gcObject,name);
	gcObject->dtor_name = name;
	gcObject->_handle.Reset(JS_ISOLATE, object);
	gcObject->_handle.SetWeak(gcObject, &WeakCallback,v8::WeakCallbackType::kParameter);
	// TODO TODO vahvarh
	//gcObject->_handle.MarkIndependent();
}*/

void GC::add(v8::Local<v8::Value> object, GC_dtor_v dtor,int internal_index)
{
	GCObject * gcObject = new GCObject();
	//fprintf(stderr,"GC::add(v8::Local<v8::Value> object, dtor_v,ptr) gcObject=%ld\n",(void*)gcObject);
	gcObject->dtor_v2_ptr = dtor;
	gcObject->_handle.Reset(JS_ISOLATE, object);
	gcObject->_handle.SetWeak(gcObject, &WeakCallback,v8::WeakCallbackType::kParameter);
	gcObject->internal_index=internal_index;
}


GC::~GC() {
}

GCObject::GCObject()
{
	dtor_t_ptr=NULL;
	dtor_v1_ptr=NULL;
	dtor_v2_ptr=NULL;
	dtor_name=NULL;
	ptr=NULL;
}

GCObject::~GCObject()
{
}

