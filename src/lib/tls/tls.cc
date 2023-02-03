/**
 * TLS library. TLS wraps a (connected) socket.
 */

#include <v8.h>
#include "macros.h"
#include "common.h"
#include <string>

#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef windows
#  define sock_errno WSAGetLastError()
#  define CONN_RESET (sock_errno == WSAECONNRESET)
#else
#  define sock_errno errno
#  define CONN_RESET (sock_errno == ECONNRESET || sock_errno == EPIPE)
#endif 



#define NOT_SOCKET JS_TYPE_ERROR("Invalid call format. Use 'new TLS(socket)'")
#define LOAD_SOCKET LOAD_VALUE(0)->ToObject(JS_CONTEXT).ToLocalChecked()->GetInternalField(0)->Int32Value(JS_CONTEXT).ToChecked()
#define LOAD_SSL LOAD_PTR(1, SSL *)
#define SSL_ERROR(ssl, ret) JS_ERROR(formatError(ssl, ret).c_str())

namespace {
	
SSL_CTX * ctx;

std::string formatError(SSL * ssl, int ret) {
	int code = SSL_get_error(ssl, ret);
	int errcode;
	std::string reason;

	switch (code) {
		case SSL_ERROR_NONE: reason = "SSL_ERROR_NONE"; break;
		case SSL_ERROR_ZERO_RETURN: reason = "SSL_ERROR_ZERO_RETURN"; break;
		case SSL_ERROR_WANT_READ: reason = "SSL_ERROR_WANT_READ"; break;
		case SSL_ERROR_WANT_WRITE: reason = "SSL_ERROR_WANT_WRITE"; break;
		case SSL_ERROR_WANT_ACCEPT: reason = "SSL_ERROR_WANT_ACCEPT"; break;
		case SSL_ERROR_WANT_CONNECT: reason = "SSL_ERROR_WANT_CONNECT"; break;
		case SSL_ERROR_WANT_X509_LOOKUP: reason = "SSL_ERROR_WANT_X509_LOOKUP"; break;
		case SSL_ERROR_SYSCALL: 
			errcode = (int)ERR_get_error();
			if (errcode) {
				reason = ERR_reason_error_string(errcode);
			} else {
				reason = "SSL_ERROR_SYSCALL";
			}
		break;
		case SSL_ERROR_SSL: 
			errcode = (int)ERR_get_error();
			if (errcode) {
				reason = ERR_reason_error_string(errcode);
			} else {
				reason = "SSL_ERROR_SSL";
			}
		break;
		
		
	}
	
	return reason;
}

void finalize(v8::Local<v8::Object> obj) {
	SSL * ssl = LOAD_PTR_FROM(obj, 1, SSL *);
	SSL_free(ssl);
}

void finalize2(void* ptr) {
	SSL * ssl = (SSL*)ptr;
	SSL_free(ssl);
}


/**
 * TLS constructor
 * @param {Socket} socket
 */
JS_METHOD(_tls) {
	ASSERT_CONSTRUCTOR;
	if (args.Length() < 1 || !args[0]->IsObject()) { NOT_SOCKET; return; }
	
	v8::Local<v8::Value> socket = args[0];

	// TODO vahvarh try catch throw
	try {
		v8::Local<v8::Value> socketproto = socket->ToObject(JS_CONTEXT).ToLocalChecked()->GetPrototype();
		v8::Persistent<v8::Object, v8::CopyablePersistentTraits<v8::Object> > required = APP_PTR->require("socket", "");
		v8::Local<v8::Object> socketmodule = v8::Local<v8::Object>::New(JS_ISOLATE, required);
		v8::Local<v8::Value> prototype = socketmodule->Get(JS_CONTEXT,JS_STR("Socket")).ToLocalChecked()->ToObject(JS_CONTEXT).ToLocalChecked()->Get(JS_CONTEXT,JS_STR("prototype")).ToLocalChecked();
		if (!prototype->Equals(JS_CONTEXT,socketproto).ToChecked()) { NOT_SOCKET; return; }
	} catch (std::string e) { // for some reasons, the socket module is not available
		JS_ERROR("Socket module not available");
		return;
	}

	SAVE_VALUE(0, socket);

	SSL * ssl = SSL_new(ctx);
	SSL_set_fd(ssl, LOAD_SOCKET);
	SAVE_PTR(1, ssl);

	GC * gc = GC_PTR;
	//gc->add(args.This(), finalize);
	gc->add(args.This(), finalize2,1);

	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_getSocket) {
	args.GetReturnValue().Set(LOAD_VALUE(0));
}

JS_METHOD(_verifyCertificate) {
	args.GetReturnValue().Set(JS_INT((int)SSL_get_verify_result(LOAD_SSL)));
}

JS_METHOD(_useCertificate) {
	v8::String::Utf8Value file(JS_ISOLATE,args[0]);
	args.GetReturnValue().Set(JS_INT(SSL_use_certificate_file(LOAD_SSL, *file, SSL_FILETYPE_PEM)));
}

JS_METHOD(_usePrivateKey) {
	v8::String::Utf8Value file(JS_ISOLATE,args[0]);
	args.GetReturnValue().Set(JS_INT(SSL_use_PrivateKey_file(LOAD_SSL, *file, SSL_FILETYPE_PEM)));
}

JS_METHOD(_accept) {
	SSL * ssl = LOAD_SSL;
	int result = SSL_accept(ssl);
	
	if (result == 1) {
		long verify_flag = (int)SSL_get_verify_result(ssl);
		if (verify_flag != X509_V_OK) {
			if (verify_flag == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) {
				JS_ERROR("ERROR CERT\n");
			}
			SSL_ERROR(ssl, verify_flag);
			std::string certError = "Certificate verification error " + std::to_string((int)verify_flag) +
				" " + std::to_string((int)X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) + "\n";
			JS_ERROR(certError.c_str());
		}
		args.GetReturnValue().Set(args.This());
	} else if (result == -1 && SSL_get_error(ssl, result) == SSL_ERROR_WANT_READ) { /* blocking socket */
		args.GetReturnValue().Set(JS_BOOL(false));
	} else {
		SSL_ERROR(ssl, result);
	}
}

JS_METHOD(_connect) {
	SSL * ssl = LOAD_SSL;
	int result = SSL_connect(ssl);
	
	if (result == 1) {
		long verify_flag = (int)SSL_get_verify_result(ssl);
		if (verify_flag != X509_V_OK) {
			if (verify_flag == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) {
				JS_ERROR("ERROR CERT\n");
			}
			SSL_ERROR(ssl, verify_flag);
			std::string certError = "Certificate verification error " + std::to_string((int)verify_flag) +
				" " + std::to_string((int)X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) + "\n";
			JS_ERROR(certError.c_str());
		}
		args.GetReturnValue().Set(args.This());
	} else {
		SSL_ERROR(ssl, result);
	}
}

JS_METHOD(_receive) {
	SSL * ssl = LOAD_SSL;
	int count = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	char * data = new char[count];

	ssize_t result = SSL_read(ssl, data, count);
	if (result >= 0) {
		if (result == 0) {
			int ret = SSL_get_error(ssl, (int)result);
			if (ret != SSL_ERROR_ZERO_RETURN)  {
				delete[] data;
				SSL_ERROR(ssl, (int)result);
				return;
			}
		}
		long verify_flag = (int)SSL_get_verify_result(ssl);
		if (verify_flag != X509_V_OK) {
			if (verify_flag == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) {
				JS_ERROR("ERROR CERT\n");
			}
			SSL_ERROR(ssl, verify_flag);
			std::string certError = "Certificate verification error" + std::to_string((int)verify_flag);
			JS_ERROR(certError.c_str());
		}
		v8::Local<v8::Value> buffer = JS_BUFFER(data, result);
		delete[] data;
		args.GetReturnValue().Set(buffer);
	} else {
		delete[] data;
		if (SSL_get_error(ssl, (int)result) == SSL_ERROR_WANT_READ) { // blocking socket
			args.GetReturnValue().Set(JS_BOOL(false));
		} else {
			SSL_ERROR(ssl, (int)result);
		}
	}
}

JS_METHOD(_receive_strict) {
	SSL * ssl = LOAD_SSL;
	int count = args[0]->Int32Value(JS_CONTEXT).ToChecked();

	const int TMP_BUFFER_MAX=10240;
	char * tmp = (char*)malloc(TMP_BUFFER_MAX);

	ssize_t len;
	int received=0;

	int flag=1;
	int recv_cap;
	ByteStorageData *bsd=new ByteStorageData(0,10240);
	while (flag) {
		if (count==0) {
			recv_cap=TMP_BUFFER_MAX;
		} else {
			recv_cap=count-received;
			if (recv_cap>TMP_BUFFER_MAX) recv_cap=TMP_BUFFER_MAX;
		}
		if (recv_cap==0) {
			flag=0;
			continue;
		}
		//printf("SSL_read() recv_cap=%d\n",recv_cap);
		len = SSL_read(ssl, tmp, recv_cap);

		long verify_flag = (int)SSL_get_verify_result(ssl);
		if (verify_flag != X509_V_OK) {
			if (verify_flag == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) {
				JS_ERROR("ERROR CERT\n");
			}
			SSL_ERROR(ssl, verify_flag);
			std::string certError = "Certificate verification error" + std::to_string((int)verify_flag);
			JS_ERROR(certError.c_str());
		}
		//printf("	SSL_read() len=%d\n",len);

		if (len==0) {
			flag=0;
		} else if (len>0) {
			bsd->add(tmp,len);
			received+=len;
		} else {
			free(tmp);
			delete bsd;
			SSL_ERROR(ssl, (int)len);
			return;
		}
	}
	free(tmp);
	v8::Local<v8::Value> buffer = BYTESTORAGE_TO_JS(new ByteStorage(bsd));
	args.GetReturnValue().Set(buffer);

}

JS_METHOD(_send) {
	if (args.Length() < 1) { JS_TYPE_ERROR("Bad argument count. Use 'tls.send(data)'"); return; }
	
	SSL * ssl = LOAD_SSL;
	ssize_t result;
	
	if (IS_BUFFER(args[0])) {
		size_t size = 0;
		char * data = JS_BUFFER_TO_CHAR(args[0], &size);	
		result = SSL_write(ssl, data, (int)size);
	} else {
		v8::String::Utf8Value data(JS_ISOLATE,args[0]);
		result = SSL_write(ssl, *data, data.length());
	}
	long verify_flag = (int)SSL_get_verify_result(ssl);
	if (verify_flag != X509_V_OK) {
		// X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY
		if (verify_flag == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) {
			JS_ERROR("ERROR CERT\n");
		}
		SSL_ERROR(ssl, verify_flag);
		std::string certError = "Certificate verification error" + std::to_string((int)verify_flag) + "\n";
		JS_ERROR(certError.c_str());
	}
	
	if (result > 0) {
		args.GetReturnValue().Set(args.This());
	} else if (SSL_get_error(ssl, (int)result) == SSL_ERROR_WANT_WRITE) { /* blocking socket */
		args.GetReturnValue().Set(JS_BOOL(false));
	} else {
		SSL_ERROR(ssl,(int) result);
	}
}

JS_METHOD(_close) {
	SSL * ssl = LOAD_SSL;
	int result = SSL_shutdown(ssl);
	if (result == 0) { _close(args); return; }

	if (result > 0) {
		args.GetReturnValue().Set(args.This());
	} else if (SSL_get_error(ssl, result) == SSL_ERROR_SYSCALL && result == -1 && CONN_RESET) { /* connection reset */
		args.GetReturnValue().Set(args.This());
	} else {
		SSL_ERROR(ssl, result);
	}
}

JS_METHOD(_setTLSMethod) {
	//SSL_CTX_free(ctx);
	if (args.Length() < 1) { SSL_CTX_set_min_proto_version(ctx, 3); return; }
	if (args[0]->IsUint32()) {
		v8::Local<v8::Context> context = v8::Context::New(JS_ISOLATE);
		v8::Maybe<uint32_t> maybe_uint = args[0]->Uint32Value(context);
		if (maybe_uint.IsJust()) {
			uint32_t method = maybe_uint.FromJust();
			SSL_CTX_set_min_proto_version(ctx, method);
		}
		else {
			JS_ERROR("setTLSMethod: failed conversion\n");
		}
	}
	else {
		JS_ERROR("setTLSMethod: wrong type\n");
	}
	//v8::Integer method = v8::Integer::New(JS_ISOLATE, );
	
	/*
	std::string m(*method);
	if (m == "TLSv1_method") {
		ctx = SSL_CTX_new(TLSv1_method());
	}
	else if (m == "TLSv1_server_method") {
		ctx = SSL_CTX_new(TLSv1_server_method());
	}
	else if (m == "TLSv1_client_method") {
		ctx = SSL_CTX_new(TLSv1_client_method());
	}
	else if (m == "TLSv1_1_method") {
		ctx = SSL_CTX_new(TLSv1_1_method());
	}
	else if (m == "TLSv1_1_server_method") {
		ctx = SSL_CTX_new(TLSv1_1_server_method());
	}
	else if (m == "TLSv1_1_client_method") {
		ctx = SSL_CTX_new(TLSv1_1_client_method());
	}
	else if (m == "TLSv1_2_method") {
		ctx = SSL_CTX_new(TLSv1_2_method());
	}
	else if (m == "TLSv1_2_server_method") {
		ctx = SSL_CTX_new(TLSv1_2_server_method());
	}
	else if (m == "TLSv1_2_client_method") {
		ctx = SSL_CTX_new(TLSv1_2_client_method());
	}
	else if (m == "TLSv1_3_method") {
		ctx = SSL_CTX_new(TLSv1_3_method());
	}
	else if (m == "TLSv1_3_server_method") {
		ctx = SSL_CTX_new(TLSv1_3_server_method());
	}
	else if (m == "TLSv1_3_client_method") {
		ctx = SSL_CTX_new(TLSv1_3_client_method());
	}
	else {
		ctx = SSL_CTX_new(TLSv1_3_method());
	}
	*/
}

}

SHARED_INIT() {
	//fprintf(stderr,"tls.cc > SHARED_INIT						isolate=%ld, InContext()=%d, context=%ld\n",(void*)JS_ISOLATE,JS_ISOLATE->InContext(),(void*)(*JS_CONTEXT));
	SSL_library_init();
	SSL_load_error_strings();
	ctx = SSL_CTX_new(TLS_method());

	if (!SSL_CTX_load_verify_locations(ctx, "/etc/ssl/certs/ca-certificates.crt", "/etc/ssl/certs/")) {
		JS_ERROR("CTX Certificate init failed\n");
	}

	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);

	v8::Local<v8::FunctionTemplate> ft = v8::FunctionTemplate::New(JS_ISOLATE, _tls);
	ft->SetClassName(JS_STR("TLS"));
	
	v8::Local<v8::ObjectTemplate> it = ft->InstanceTemplate();
	it->SetInternalFieldCount(2); /* socket, ssl */

	v8::Local<v8::ObjectTemplate> pt = ft->PrototypeTemplate();
	
	/**
	 * Prototype methods (new TLS().*)
	 */
	pt->Set(JS_ISOLATE,"getSocket"			, v8::FunctionTemplate::New(JS_ISOLATE, _getSocket));
	pt->Set(JS_ISOLATE,"verifyCertificate"	, v8::FunctionTemplate::New(JS_ISOLATE, _verifyCertificate));
	pt->Set(JS_ISOLATE,"useCertificate"		, v8::FunctionTemplate::New(JS_ISOLATE, _useCertificate));
	pt->Set(JS_ISOLATE,"usePrivateKey"		, v8::FunctionTemplate::New(JS_ISOLATE, _usePrivateKey));
	pt->Set(JS_ISOLATE,"accept"				, v8::FunctionTemplate::New(JS_ISOLATE, _accept));
	pt->Set(JS_ISOLATE,"connect"			, v8::FunctionTemplate::New(JS_ISOLATE, _connect));
	pt->Set(JS_ISOLATE,"receive"			, v8::FunctionTemplate::New(JS_ISOLATE, _receive));
	pt->Set(JS_ISOLATE,"receive_strict"		, v8::FunctionTemplate::New(JS_ISOLATE, _receive_strict));
	pt->Set(JS_ISOLATE,"send"				, v8::FunctionTemplate::New(JS_ISOLATE, _send));
	pt->Set(JS_ISOLATE,"close"				, v8::FunctionTemplate::New(JS_ISOLATE, _close));

	(void)exports->Set(JS_CONTEXT,JS_STR("TLS"), ft->GetFunction(JS_CONTEXT).ToLocalChecked());
}
