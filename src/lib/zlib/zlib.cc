#include <v8.h>
#include <zlib.h>
#include "macros.h"
#include <cstring>

namespace {

JS_METHOD(_compress) {
	if (args.Length() < 1) { JS_TYPE_ERROR("Bad argument count. Use 'compress(buffer[, level])'"); return; }
	if (!IS_BUFFER(args[0])) { JS_TYPE_ERROR("First argument must be an instance of Buffer"); return; }
	
	size_t inputLength = 0;
	char * data = JS_BUFFER_TO_CHAR(args[0], &inputLength);
	unsigned long outputLength = compressBound(inputLength);

	char * output = (char*) malloc(outputLength);
	int level = 9;
	if (args.Length() > 1) {
		level = (int) args[1]->IntegerValue(JS_CONTEXT).ToChecked();
	}
	compress2((uint8_t*) output, &outputLength, (const uint8_t *) data, inputLength, level);
	
	v8::Local<v8::Value> buffer = JS_BUFFER(output, outputLength);
	free(output);
	
	args.GetReturnValue().Set(buffer);
}

JS_METHOD(_decompress) {
	if (args.Length() < 1) { JS_TYPE_ERROR("Bad argument count. Use 'decompress(buffer)'"); return; }
	if (!IS_BUFFER(args[0])) { JS_TYPE_ERROR("First argument must be an instance of Buffer"); return; }

	size_t inputLength = 0;
	char * data = JS_BUFFER_TO_CHAR(args[0], &inputLength);

	size_t chunkSize = 8192;
	char * chunk = (char *) malloc(chunkSize);
	char * output = (char *) malloc(chunkSize);
	size_t capacity = chunkSize;
	size_t used = 0;

	z_stream stream;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	stream.avail_in = (int)inputLength;
	stream.next_in = (uint8_t*) data;

	if (inflateInit(&stream) != Z_OK) {
		free(chunk);
		free(output);
		JS_ERROR("Failed to decompress");
		return;
	}

	int ret = Z_OK;

	do {
		if (stream.avail_in == 0) break;

		do {
			stream.next_out = (uint8_t*) chunk;
			stream.avail_out = (int)chunkSize;
			ret = inflate(&stream, Z_NO_FLUSH);
			switch (ret) {
				case Z_MEM_ERROR:
				case Z_NEED_DICT:
				case Z_DATA_ERROR:
					inflateEnd(&stream);
					free(chunk);
					free(output);
					JS_ERROR("Failed to decompress");
					return;
			}

			size_t dataLength = chunkSize - stream.avail_out;
			if (!dataLength) { continue; }

			while ((capacity - used) < dataLength) {
				capacity <<= 1;
				output = (char *) realloc(output, capacity);
			}
			memcpy(output + used, chunk, dataLength);
			used += dataLength;
		} while (stream.avail_out == 0);
	} while (ret != Z_STREAM_END);

	inflateEnd(&stream);
	free(chunk);
	v8::Local<v8::Value> buffer = JS_BUFFER(output, used);
	free(output);
	args.GetReturnValue().Set(buffer);
}

}

SHARED_INIT() {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	(void)exports->Set(JS_CONTEXT,JS_STR("compress"), v8::FunctionTemplate::New(JS_ISOLATE, _compress)->GetFunction(JS_CONTEXT).ToLocalChecked());
	(void)exports->Set(JS_CONTEXT,JS_STR("decompress"), v8::FunctionTemplate::New(JS_ISOLATE, _decompress)->GetFunction(JS_CONTEXT).ToLocalChecked());
}


