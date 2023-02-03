/*
 * glesbind.h
 *
 */

#ifndef GLESBIND_H_
#define GLESBIND_H_

#include <v8.h>

class GlesFactory {
public:
	static v8::Local<v8::ObjectTemplate> createGles(void);

	static v8::Global<v8::Object> self_;
	static v8::Global<v8::Context> gles_persistent_context;
	static char* root_path;
};

#endif /* GLESBIND_H_ */
