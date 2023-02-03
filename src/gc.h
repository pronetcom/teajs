/**
 * Garbage collection support. Every C++ class can subscribe to be notified,
 * when its JS representation gets GC'ed.
 */

#ifndef _JS_GC_H
#define _JS_GC_H

#include <list>
#include "v8.h"

typedef void (*GC_dtor_t) (v8::Local<v8::Object>);
typedef void (*GC_dtor_v) (void*);

class GCObject {
	public:
		GCObject();
		~GCObject();
	    GC_dtor_t dtor_t_ptr;
	    GC_dtor_v dtor_v1_ptr;
	    GC_dtor_v dtor_v2_ptr;
	    const char *dtor_name;
		void *ptr;
		int internal_index;
	
    	v8::Persistent<v8::Value> _handle;
};

class GC {
public:
	virtual ~GC();

	// vahvarh
	//static void WeakCallback(const v8::WeakCallbackData<v8::Value, void>& data);    
	static void WeakCallback(const v8::WeakCallbackInfo<GCObject>& data);    

	/*virtual void add(v8::Local<v8::Value> object, GC_dtor_t dtor);
	virtual void add(v8::Local<v8::Value> object, const char *name);
	virtual void add(v8::Local<v8::Value> object, GC_dtor_v dtor,void*ptr);*/
	virtual void add(v8::Local<v8::Value> object, GC_dtor_v dtor,int internal_index);
};

#endif
