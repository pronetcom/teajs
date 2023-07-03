/**
 * Socket library. Provides a simple OO abstraction atop several socket-related functions.
 */

#include <v8.h>
#include "macros.h"
#include "common.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
//#include <in.h>

#ifdef windows
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  define close(s) closesocket(s)
#  define sock_errno WSAGetLastError()
#  define WOULD_BLOCK (sock_errno == WSAEWOULDBLOCK || sock_errno == WSAEINPROGRESS)
#else
#  include <unistd.h>
#  include <sys/socket.h>
#  include <sys/un.h>
#  include <sys/param.h>
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <netdb.h>
#  include <arpa/inet.h>
#  define sock_errno errno
#  define WOULD_BLOCK (sock_errno == EWOULDBLOCK || sock_errno == EAGAIN)
#endif 

#ifndef EWOULDBLOCK
#  define EWOULDBLOCK EAGAIN
#endif

#ifndef EINPROGRESS
#  define EINPROGRESS WSAEINPROGRESS
#endif

#ifndef MAXHOSTNAMELEN
#  define MAXHOSTNAMELEN 64
#endif

#ifndef INVALID_SOCKET
#  define INVALID_SOCKET -1
#endif

#ifndef SOCKET_ERROR
#  define SOCKET_ERROR -1
#endif

void FormatError() {
#ifdef windows
	int size = 0xFF;
	char buf[size];
	buf[size-1] = '\0';
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, WSAGetLastError(), 0, buf, size-1, NULL);
	JS_ERROR(buf);
#else
	JS_ERROR(strerror(errno));
#endif
}

namespace {

v8::Global<v8::Function> _socketFunc;
v8::Global<v8::FunctionTemplate> _socketTemplate;

inline bool isSocket(v8::Local<v8::Value> value) {
	v8::Local<v8::FunctionTemplate> socketTemplate = v8::Local<v8::FunctionTemplate>::New(JS_ISOLATE, _socketTemplate);
	if (INSTANCEOF(value, socketTemplate)) { return true; }

	v8::Local<v8::Value> getSocket = value->ToObject(JS_CONTEXT).ToLocalChecked()->Get(JS_CONTEXT,JS_STR("getSocket")).ToLocalChecked();
	if (!getSocket->IsFunction()) { return false; }
	
	v8::Local<v8::Function> getSocketFunc = v8::Local<v8::Function>::Cast(getSocket);
	v8::Local<v8::Value> socket = getSocketFunc->Call(JS_CONTEXT,value->ToObject(JS_CONTEXT).ToLocalChecked(), 0, NULL).ToLocalChecked();
	return INSTANCEOF(socket, socketTemplate);
}

inline int jsToSocket(v8::Local<v8::Value> value) {
	v8::Local<v8::FunctionTemplate> socketTemplate = v8::Local<v8::FunctionTemplate>::New(JS_ISOLATE, _socketTemplate);
	if (INSTANCEOF(value, socketTemplate)) { return value->ToObject(JS_CONTEXT).ToLocalChecked()->GetInternalField(0)->Int32Value(JS_CONTEXT).ToChecked(); }
	
	v8::Local<v8::Value> getSocket = value->ToObject(JS_CONTEXT).ToLocalChecked()->Get(JS_CONTEXT,JS_STR("getSocket")).ToLocalChecked();

	v8::Local<v8::Function> getSocketFunc = v8::Local<v8::Function>::Cast(getSocket);
	v8::Local<v8::Value> socket = getSocketFunc->Call(JS_CONTEXT,value->ToObject(JS_CONTEXT).ToLocalChecked(), 0, NULL).ToLocalChecked();

	return socket->ToObject(JS_CONTEXT).ToLocalChecked()->GetInternalField(0)->Int32Value(JS_CONTEXT).ToChecked();
}

#ifndef HAVE_PTON
int inet_pton(int family, const char *src, void *dst) {
	struct addrinfo hints, *res, *ressave;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = family;
	if (getaddrinfo(src, NULL, &hints, &res) != 0) { return 0; }
	
	if (family == PF_INET) {
		memcpy(dst, &((sockaddr_in *) res->ai_addr)->sin_addr.s_addr, sizeof(struct in_addr));
	} else {
		memcpy(dst, &((sockaddr_in6 *) res->ai_addr)->sin6_addr.s6_addr, sizeof(struct in6_addr));
	}

	freeaddrinfo(ressave);
	return 1;
}
#endif

#ifndef HAVE_NTOP
const char * inet_ntop(int family, const void *src, char *dst, socklen_t cnt) {
    switch (family) {
		case PF_INET: {
			struct sockaddr_in in;
			memset(&in, 0, sizeof(in));
			in.sin_family = PF_INET;
			memcpy(&in.sin_addr, src, sizeof(struct in_addr));
			getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in), dst, cnt, NULL, 0, NI_NUMERICHOST);
			return dst;
		}
		case PF_INET6: {
			struct sockaddr_in6 in;
			memset(&in, 0, sizeof(in));
			in.sin6_family = PF_INET6;
			memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
			getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in6), dst, cnt, NULL, 0, NI_NUMERICHOST);
			return dst;
		}
	}
	return NULL;
}
#endif

typedef union sock_addr {
    struct sockaddr_in in;
#ifndef windows
    struct sockaddr_un un;
#endif
    struct sockaddr_in6 in6;
} sock_addr_t;

/**
 * Universal address creator.
 * @param {char *} address Stringified addres
 * @param {int} port Port number
 * @param {int} family Address family
 * @param {sock_addr_t *} result Target data structure
 * @param {socklen_t *} len Result length
 */
inline int create_addr(char * address, int port, int family, sock_addr_t * result, socklen_t * len) {
   switch (family) {
#ifndef windows
		case PF_UNIX: {
			size_t length = strlen(address);
 			struct sockaddr_un *addr = (struct sockaddr_un*) result;
			memset(addr, 0, sizeof(sockaddr_un));

			if (length >= sizeof(addr->sun_path)) { return 1; }
			addr->sun_family = PF_UNIX;
			memcpy(addr->sun_path, address, length);
			addr->sun_path[length] = '\0';
			*len = (socklen_t)(length + (sizeof(*addr) - sizeof(addr->sun_path)));
		} break;
#endif
		case PF_INET: {
			struct sockaddr_in *addr = (struct sockaddr_in*) result;
			memset(addr, 0, sizeof(*addr)); 
			addr->sin_family = PF_INET;
			int pton_ret = inet_pton(PF_INET, address, & addr->sin_addr.s_addr);
			if (pton_ret == 0) { return 1; }
			addr->sin_port = htons((short)port);
			*len = sizeof(*addr);
		} break;
		case PF_INET6: {
			struct sockaddr_in6 *addr = (struct sockaddr_in6*) result;
			memset(addr, 0, sizeof(*addr));
			addr->sin6_family = PF_INET6;
			int pton_ret = inet_pton(PF_INET6, address, & addr->sin6_addr.s6_addr);
			if (pton_ret == 0) { return 1; }
			addr->sin6_port = htons((short)port);
			*len = sizeof(*addr);
		} break;
    }
	return 0;
}

/**
 * Returns JS array with values describing remote address.
 * For UNIX socket, only one item is present. For PF_INET and
 * PF_INET6, array contains [address, port].
 */
inline v8::Local<v8::Value> create_peer(sockaddr * addr) {
	v8::Local<v8::Array> result;
	sockaddr_un * addr_un;
	sockaddr_in6 * addr_in6;
	sockaddr_in * addr_in;
	char *buf;
    switch (addr->sa_family) {
#ifndef windows
		case PF_UNIX:
			result = v8::Array::New(JS_ISOLATE, 1);
			addr_un = (sockaddr_un *) addr;
			(void)result->Set(JS_CONTEXT,JS_INT(0), JS_STR(addr_un->sun_path));
			return result;
#endif

		case PF_INET6:
			result = v8::Array::New(JS_ISOLATE, 2);
			buf = new char[INET6_ADDRSTRLEN];
			addr_in6 = (sockaddr_in6 *) addr;
			inet_ntop(PF_INET6, addr_in6->sin6_addr.s6_addr, buf, INET6_ADDRSTRLEN);
			(void)result->Set(JS_CONTEXT,JS_INT(0), JS_STR(buf));
			(void)result->Set(JS_CONTEXT,JS_INT(1), JS_INT(ntohs(addr_in6->sin6_port)));
			delete[] buf;
			return result;

		case PF_INET:
			result = v8::Array::New(JS_ISOLATE, 2);
			buf = new char[INET_ADDRSTRLEN];
			addr_in = (sockaddr_in *) addr;
			inet_ntop(PF_INET, & addr_in->sin_addr.s_addr, buf, INET_ADDRSTRLEN);
			(void)result->Set(JS_CONTEXT,JS_INT(0), JS_STR(buf));
			(void)result->Set(JS_CONTEXT,JS_INT(1), JS_INT(ntohs(addr_in->sin_port)));
			delete[] buf;
			return result;

	}
    return v8::Undefined(JS_ISOLATE);
}

/**
 * Socket constructor
 * @param {int} family
 * @param {int} type
 * @param {int} [proto]
 */
JS_METHOD(_socket) {
	ASSERT_CONSTRUCTOR;
	if (args.Length() < 2) {
		JS_TYPE_ERROR("Invalid call format. Use 'new Socket(family, type, [proto])'");
		return;
	}
	
	int offset = (args[0]->IsExternal() ? 1 : 0);
	int family = args[offset + 0]->Int32Value(JS_CONTEXT).ToChecked();
	int type = args[offset + 1]->Int32Value(JS_CONTEXT).ToChecked();
	int proto = args[offset + 2]->Int32Value(JS_CONTEXT).ToChecked();
	int s = -1;
	
	if (args[0]->IsExternal()) {
		v8::Local<v8::External> tmp = v8::Local<v8::External>::Cast(args[0]);
		s = *((int *) tmp->Value());
	} else {
		s = socket(family, type, proto);
	}
	SAVE_VALUE(0, JS_INT(s));
	SAVE_VALUE(1, JS_BOOL(false));
	(void)args.This()->Set(JS_CONTEXT,JS_STR("family"), JS_INT(family));
	(void)args.This()->Set(JS_CONTEXT,JS_STR("type"), JS_INT(type));
	(void)args.This()->Set(JS_CONTEXT,JS_STR("proto"), JS_INT(proto));
	
	if (s == INVALID_SOCKET) {
		FormatError();
		return;
	}

	args.GetReturnValue().Set(args.This());
}

JS_METHOD(_getprotobyname) {
	v8::String::Utf8Value name(JS_ISOLATE,args[0]);
	struct protoent * result = getprotobyname(*name);
	if (result) {
		args.GetReturnValue().Set(JS_INT(result->p_proto));
	} else {
		JS_ERROR("Cannot retrieve protocol number");
	}
}

JS_METHOD(_getaddrinfo) {
	v8::String::Utf8Value name(JS_ISOLATE,args[0]);
	int family = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	if (family == 0) { family = PF_INET; }
	
	struct addrinfo hints, * servinfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;

	int result = getaddrinfo(*name, NULL, &hints, &servinfo);
	if (result != 0) {
		JS_ERROR(gai_strerror(result));
		return;
	}

	v8::Local<v8::Object> item = create_peer(servinfo->ai_addr)->ToObject(JS_CONTEXT).ToLocalChecked();
	freeaddrinfo(servinfo);
	args.GetReturnValue().Set(item->Get(JS_CONTEXT,JS_INT(0)).ToLocalChecked());
}

JS_METHOD(_getnameinfo) {
	v8::String::Utf8Value name(JS_ISOLATE,args[0]);
	int family = (int) args[1]->IntegerValue(JS_CONTEXT).ToChecked();
	if (family == 0) { family = PF_INET; }

	char hostname[NI_MAXHOST];
    sock_addr_t addr;
    socklen_t len = 0;

	int result = create_addr(*name, 0, family, &addr, &len);
	if (result != 0) {
		JS_ERROR("Malformed address");
		return;
	}
	
	result = getnameinfo((sockaddr *) & addr, len, hostname, NI_MAXHOST, NULL, 0, 0);
	if (result != 0) {
		JS_ERROR(gai_strerror(result));
	} else {
		args.GetReturnValue().Set(JS_STR(hostname));
	}
}

JS_METHOD(_gethostname) {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
    char * buf = new char[MAXHOSTNAMELEN+1];
    gethostname(buf, MAXHOSTNAMELEN);
	v8::Local<v8::Value> result = JS_STR(buf);
	delete[] buf;
	args.GetReturnValue().Set(result);
}

JS_METHOD(_select) {
	if (args.Length() != 4) {
		JS_TYPE_ERROR("Bad argument count. Socket.select must be called with 4 arguments.");
		return;
	}
	
	int max = 0;
	fd_set fds[3];

	/* fill descriptors */
	for (int i=0;i<3;i++) {
		FD_ZERO(&fds[i]);
		
		if (!args[i]->IsArray()) { continue; }
		v8::Local<v8::Array> arr = v8::Local<v8::Array>::Cast(args[i]);
		int len = arr->Length();
		for (int j=0;j<len;j++) {
			v8::Local<v8::Value> member = arr->Get(JS_CONTEXT,JS_INT(j)).ToLocalChecked();
			if (!isSocket(member)) { JS_ERROR("Arguments must be arrays of Socket instances"); return; }
			int fd = jsToSocket(member);
			if (fd > max) { max = fd; }
			FD_SET(fd, &fds[i]);
		}
	}
	
	/* prepare time info */
	timeval * tv = NULL;
	if (!args[3]->IsNull()) {
		int time = args[3]->Int32Value(JS_CONTEXT).ToChecked();
		tv = new timeval;
		tv->tv_sec = time / 1000;
		tv->tv_usec = 1000 * (time % 1000);
	}
	
	int ret;
	
	{
		v8::Unlocker unlocker(JS_ISOLATE);
		while (1) {
			ret = select(max+1, &fds[0], &fds[1], &fds[2], tv);
			if (ret != SOCKET_ERROR || sock_errno != EINTR) { break; }
		}
	}

	if (tv != NULL) { delete tv; } /* clean up time */
	if (ret == SOCKET_ERROR) { return FormatError(); }
	
	/* delete unused sockets */
	for (int i=0;i<3;i++) {
		if (!args[i]->IsArray()) { continue; }
		v8::Local<v8::Array> arr = v8::Local<v8::Array>::Cast(args[i]);
		int len = arr->Length();
		for (int j=0;j<len;j++) {
			v8::Local<v8::Value> member = arr->Get(JS_CONTEXT,JS_INT(j)).ToLocalChecked();
			int fd = jsToSocket(member);
			if (!FD_ISSET(fd, &fds[i])) {

				(void)arr->Delete(JS_CONTEXT,JS_INT(j));
				// TODO vahvarh
				//arr->ForceDelete(JS_INT(j));
			}
		}
	}

	args.GetReturnValue().Set(JS_INT(ret));
}

JS_METHOD(_makeNonblock) {
	if (args.Length() != 1) {
		JS_TYPE_ERROR("Bad argument count. Socket.select must be called with 4 arguments.");
		return;
	}
	v8::String::Utf8Value str(JS_ISOLATE, args[0]);
	char* cName = *str;
	bool isFd = true;
	long fd = 0;
	for (int i = 0; cName[i] != 0; i++) {
		if (cName[i] < '0' || cName[i] > '9') {
			isFd = false;
			break;
		}
		fd = fd * 10 + (int)(cName[i] - '0');
	}
	if (!isFd) { JS_ERROR("Argument must be a descriptor"); return; }
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

JS_METHOD(_connect) {
	int family = args.This()->Get(JS_CONTEXT,JS_STR("family")).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
	int sock = LOAD_VALUE(0)->Int32Value(JS_CONTEXT).ToChecked();

	int argcount = 1;
	if (family != PF_UNIX) { argcount = 2; }
	if (args.Length() < argcount) {
		JS_TYPE_ERROR("Bad argument count. Use 'socket.connect(address, [port])'");
		return;
	}
	
	v8::String::Utf8Value address(JS_ISOLATE,args[0]);
	int port = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	sock_addr_t addr;
	socklen_t len = 0;

	int result = create_addr(*address, port, family, &addr, &len);
	if (result != 0) {
        JS_ERROR("Malformed address");
		return;
	}
	
	result = connect(sock, (sockaddr *) &addr, len);
	
	if (!result) { args.GetReturnValue().Set(JS_BOOL(true)); return; }
	if (WOULD_BLOCK) { args.GetReturnValue().Set(JS_BOOL(false)); return; }

	FormatError();
}

JS_METHOD(_bind) {
	int family = args.This()->Get(JS_CONTEXT,JS_STR("family")).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
	int sock = LOAD_VALUE(0)->Int32Value(JS_CONTEXT).ToChecked();

	if (args.Length() < 1) {
		JS_TYPE_ERROR("Bad argument count. Use 'socket.bind(address, [port])'");
		return;
	}
	
	v8::String::Utf8Value address(JS_ISOLATE,args[0]);
	int port = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	sock_addr_t addr;
	socklen_t len = 0;
	int result = create_addr(*address, port, family, &addr, &len);
	if (result != 0) {
		JS_ERROR("Malformed address");
		return;
	}

	result = bind(sock, (sockaddr *) &addr, len);
	if (result) {
		FormatError();
	} else {
		args.GetReturnValue().Set(args.This());
	}
}

JS_METHOD(_listen) {
	int sock = LOAD_VALUE(0)->Int32Value(JS_CONTEXT).ToChecked();

	int num = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	if (args.Length() == 0) { num = 5; }

	int result = listen(sock, num);
	if (result) {
		FormatError();
	} else {
		args.GetReturnValue().Set(args.This());
	}
}

JS_METHOD(_accept) {
	int sock = LOAD_VALUE(0)->Int32Value(JS_CONTEXT).ToChecked();
	int sock2 = accept(sock, NULL, NULL);
	if (sock2 != INVALID_SOCKET) { 
		v8::Local<v8::Value> argv[4];
		argv[0] = v8::External::New(JS_ISOLATE, &sock2); // dummy field
		argv[1] = args.This()->Get(JS_CONTEXT,JS_STR("family")).ToLocalChecked();
		argv[2] = args.This()->Get(JS_CONTEXT,JS_STR("type")).ToLocalChecked();
		argv[3] = args.This()->Get(JS_CONTEXT,JS_STR("proto")).ToLocalChecked();
		v8::Local<v8::Function> socketFunc = v8::Local<v8::Function>::New(JS_ISOLATE, _socketFunc);
		args.GetReturnValue().Set(socketFunc->NewInstance(JS_CONTEXT,4, argv).ToLocalChecked());
		return;
	}
	if (WOULD_BLOCK) { args.GetReturnValue().Set(JS_BOOL(false)); return; }
	FormatError();
}

JS_METHOD(_send) {
	int sock = LOAD_VALUE(0)->Int32Value(JS_CONTEXT).ToChecked();

	if (args.Length() < 1) {
		JS_TYPE_ERROR("Bad argument count. Use 'socket.send(data, [address], [port])'");
		return;
	}
	
	sock_addr_t taddr;
	sockaddr * target = NULL;
	socklen_t len = 0;
	ssize_t result;
	
	if (args.Length() > 1) {
		int family = args.This()->Get(JS_CONTEXT,JS_STR("family")).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
		v8::String::Utf8Value address(JS_ISOLATE,args[1]);
		int port = args[2]->Int32Value(JS_CONTEXT).ToChecked();
		int r = create_addr(*address, port, family, &taddr, &len);
		if (r != 0) { JS_ERROR("Malformed address"); return; }
		target = (sockaddr *) &taddr;
	}
	
	if (IS_BUFFER(args[0])) {
		size_t size = 0;
		char * data = JS_BUFFER_TO_CHAR(args[0], &size);
		result = sendto(sock, data, size, 0, target, len);
	} else {
		v8::String::Utf8Value data(JS_ISOLATE,args[0]);
		result = sendto(sock, *data, data.length(), 0, target, len);
	}
	
	if (result != SOCKET_ERROR) { args.GetReturnValue().Set(JS_INT((int)result)); return; }
	if (WOULD_BLOCK) { args.GetReturnValue().Set(JS_BOOL(false)); return; }
	FormatError();
}

JS_METHOD(_receive) {
	int sock = LOAD_VALUE(0)->Int32Value(JS_CONTEXT).ToChecked();
	int count = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int type = args.This()->Get(JS_CONTEXT,JS_STR("type")).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
	
	char * data = new char[count];
	sock_addr_t addr;
	socklen_t len = 0;

	ssize_t result = recvfrom(sock, data, count, 0, (sockaddr *) &addr, &len);
	if (result != SOCKET_ERROR) {
		v8::Local<v8::Value> buffer = JS_BUFFER(data, result);
		delete[] data;
		if (type == SOCK_DGRAM) { SAVE_VALUE(1, create_peer((sockaddr *) &addr)); }
		args.GetReturnValue().Set(buffer);
		return;
	}
	
	delete[] data;
	if (WOULD_BLOCK) { args.GetReturnValue().Set(JS_BOOL(false)); return; }

	FormatError();
}

JS_METHOD(_receive_strict) {
	int sock = LOAD_VALUE(0)->Int32Value(JS_CONTEXT).ToChecked();
	int count = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int type = args.This()->Get(JS_CONTEXT,JS_STR("type")).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
	
	const int TMP_BUFFER_MAX=10240;
	char * tmp = (char*)malloc(TMP_BUFFER_MAX);
	sock_addr_t addr;
	socklen_t addrlen = 0;

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
		//printf("recvfrom() recv_cap=%d\n",recv_cap);
		ssize_t result = recvfrom(sock, tmp, recv_cap, 0, (sockaddr *) &addr, &addrlen);
		//printf("	recvfrom() result=%d\n",result);
		if (result>0) {
			bsd->add(tmp,result);
			received+=result;
		} else if (result==0) {
			flag=0;// ???
		} else {
			if (WOULD_BLOCK) {
				flag=0;
			} else {
				free(tmp);
				delete bsd;
				FormatError();
				return;
			}
		}
	}
	free(tmp);
	v8::Local<v8::Value> buffer = BYTESTORAGE_TO_JS(new ByteStorage(bsd));
	args.GetReturnValue().Set(buffer);
}


JS_METHOD(_socketclose) {
	int sock = LOAD_VALUE(0)->Int32Value(JS_CONTEXT).ToChecked();
	
	int result = close(sock);
	if (result == SOCKET_ERROR) {
		FormatError();
	} else {
		args.GetReturnValue().SetUndefined();
	}
}

JS_METHOD(_setoption) {
	if (args.Length() != 2) {
		JS_TYPE_ERROR("Bad argument count. Use 'socket.setOption(name, value)'");
		return;
	}
	int sock = LOAD_VALUE(0)->Int32Value(JS_CONTEXT).ToChecked();
	int name = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int value = args[1]->Int32Value(JS_CONTEXT).ToChecked();

	int level;
	switch (name) {
		case TCP_NODELAY:
			level = IPPROTO_TCP;
		break;
		default:
			level = SOL_SOCKET;
		break;
	}
	
	int result = setsockopt(sock, level, name, (char *) &value, sizeof(int));
	if (result) {
		FormatError();
	} else {
		args.GetReturnValue().Set(args.This());
	}
}

JS_METHOD(_getoption) {
	if (args.Length() < 1) {
		JS_TYPE_ERROR("Bad argument count. Use 'socket.getOption(name)'");
		return;
	}
	int sock = LOAD_VALUE(0)->Int32Value(JS_CONTEXT).ToChecked();
	int name = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	int level;
	switch (name) {
		case TCP_NODELAY:
			level = IPPROTO_TCP;
		break;
		default:
			level = SOL_SOCKET;
		break;
	}
	
	int value;
	socklen_t len = sizeof(value);
	int result = getsockopt(sock, level, name, (char *)&value, &len);

	if (result != 0) { FormatError(); return; }
	if (len != sizeof(value)) { JS_ERROR("getsockopt returned truncated value"); return; }
	args.GetReturnValue().Set(JS_INT(value));
}

JS_METHOD(_setblocking) {
	if (args.Length() < 1) {
		JS_TYPE_ERROR("Bad argument count. Use 'socket.setBlocking(true/false)'");
		return;
	}
	
	int sock = LOAD_VALUE(0)->Int32Value(JS_CONTEXT).ToChecked();
	
#ifdef windows
	unsigned long flag = !args[0]->ToBoolean(JS_ISOLATE)->Value(); /* with ioctlsocket, a non-zero sets nonblocking, a zero sets blocking */
	if (ioctlsocket(sock, FIONBIO, &flag) == SOCKET_ERROR) {
		FormatError();
		return;
	} else {
		args.GetReturnValue().SetUndefined();
		return;
	}

#else 
	int flags = fcntl(sock, F_GETFL);

	if (args[0]->ToBoolean(JS_ISOLATE)->Value()) {
		flags &= ~O_NONBLOCK;
	} else {
		flags |= O_NONBLOCK;
	}
	
	if (fcntl(sock, F_SETFL, flags) == -1) {
		FormatError();
		return;
	} else {
		args.GetReturnValue().SetUndefined();
		return;
	}
#endif

}

JS_METHOD(_getpeername) {
	int sock = LOAD_VALUE(0)->Int32Value(JS_CONTEXT).ToChecked();

	if (!LOAD_VALUE(1)->IsTrue()) {
	    sock_addr_t addr;
		socklen_t len = sizeof(sock_addr_t);
		int result = getpeername(sock, (sockaddr *) &addr, &len);
		if (result == 0) {
			SAVE_VALUE(1, create_peer((sockaddr *) &addr));
		} else {
			return FormatError();
		}
	}
	
	args.GetReturnValue().Set(LOAD_VALUE(1));
}

}

SHARED_INIT() {
	//fprintf(stderr,"socket.cc > SHARED_INIT						isolate=%ld, InContext()=%d, context=%ld\n",(void*)JS_ISOLATE,JS_ISOLATE->InContext(),(void*)(*JS_CONTEXT));

	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);

#ifdef windows
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 0);
	WSAStartup(wVersionRequested, &wsaData);
#endif

	v8::Local<v8::FunctionTemplate> socketTemplate = v8::FunctionTemplate::New(JS_ISOLATE, _socket);
	socketTemplate->SetClassName(JS_STR("Socket"));
	_socketTemplate.Reset(JS_ISOLATE, socketTemplate);

	/**
	 * Constants (Socket.*)
	 */
	socketTemplate->Set(JS_ISOLATE,"PF_INET"		, JS_INT(PF_INET)); 
	socketTemplate->Set(JS_ISOLATE,"PF_INET6"		, JS_INT(PF_INET6)); 
	socketTemplate->Set(JS_ISOLATE,"PF_UNIX"		, JS_INT(PF_UNIX)); 
	socketTemplate->Set(JS_ISOLATE,"IPPROTO_TCP"	, JS_INT(IPPROTO_TCP)); 
	socketTemplate->Set(JS_ISOLATE,"IPPROTO_UDP"	, JS_INT(IPPROTO_UDP)); 
	socketTemplate->Set(JS_ISOLATE,"SOCK_STREAM"	, JS_INT(SOCK_STREAM)); 
	socketTemplate->Set(JS_ISOLATE,"SOCK_DGRAM"		, JS_INT(SOCK_DGRAM)); 
	socketTemplate->Set(JS_ISOLATE,"SOCK_RAW"		, JS_INT(SOCK_RAW)); 
	socketTemplate->Set(JS_ISOLATE,"SO_REUSEADDR"	, JS_INT(SO_REUSEADDR)); 
	socketTemplate->Set(JS_ISOLATE,"SO_BROADCAST"	, JS_INT(SO_BROADCAST)); 
	socketTemplate->Set(JS_ISOLATE,"SO_KEEPALIVE"	, JS_INT(SO_KEEPALIVE)); 
	socketTemplate->Set(JS_ISOLATE,"SO_ERROR"		, JS_INT(SO_ERROR)); 
	socketTemplate->Set(JS_ISOLATE,"TCP_NODELAY"	, JS_INT(TCP_NODELAY)); 
	
	/*fprintf(stderr,"socket.cc > SHARED_INIT context=%ld\n",(void*)*(JS_CONTEXT));
	v8::MaybeLocal<v8::Function> tmp_func1=v8::FunctionTemplate::New(JS_ISOLATE, _getprotobyname)->GetFunction(JS_CONTEXT);
	v8::Local<v8::Function> tmp_func2=tmp_func1.ToLocalChecked();
	socketTemplate->Set(JS_ISOLATE,"getProtoByName"	, tmp_func2);*/
	socketTemplate->Set(JS_ISOLATE,"getProtoByName"	, v8::FunctionTemplate::New(JS_ISOLATE, _getprotobyname));
	socketTemplate->Set(JS_ISOLATE,"getAddrInfo"	, v8::FunctionTemplate::New(JS_ISOLATE, _getaddrinfo));
	socketTemplate->Set(JS_ISOLATE,"getNameInfo"	, v8::FunctionTemplate::New(JS_ISOLATE, _getnameinfo));
	socketTemplate->Set(JS_ISOLATE,"getHostName"	, v8::FunctionTemplate::New(JS_ISOLATE, _gethostname));
	socketTemplate->Set(JS_ISOLATE,"select"			, v8::FunctionTemplate::New(JS_ISOLATE, _select));
	socketTemplate->Set(JS_ISOLATE,"makeNonblock"       , v8::FunctionTemplate::New(JS_ISOLATE, _makeNonblock));
	/*socketTemplate->Set(JS_ISOLATE,"getProtoByName"	, v8::FunctionTemplate::New(JS_ISOLATE, _getprotobyname)->GetFunction(JS_CONTEXT).ToLocalChecked());
	socketTemplate->Set(JS_ISOLATE,"getProtoByName"	, v8::FunctionTemplate::New(JS_ISOLATE, _getprotobyname));
	socketTemplate->Set(JS_ISOLATE,"getAddrInfo"	, v8::FunctionTemplate::New(JS_ISOLATE, _getaddrinfo)->GetFunction(JS_CONTEXT).ToLocalChecked());
	socketTemplate->Set(JS_ISOLATE,"getNameInfo"	, v8::FunctionTemplate::New(JS_ISOLATE, _getnameinfo)->GetFunction(JS_CONTEXT).ToLocalChecked());
	socketTemplate->Set(JS_ISOLATE,"getHostName"	, v8::FunctionTemplate::New(JS_ISOLATE, _gethostname)->GetFunction(JS_CONTEXT).ToLocalChecked());
	socketTemplate->Set(JS_ISOLATE,"select"			, v8::FunctionTemplate::New(JS_ISOLATE, _select)->GetFunction(JS_CONTEXT).ToLocalChecked());*/

	v8::Local<v8::ObjectTemplate> it = socketTemplate->InstanceTemplate();
	it->SetInternalFieldCount(2); /* sock, peername */

	v8::Local<v8::ObjectTemplate> pt = socketTemplate->PrototypeTemplate();
	
	/**
	 * Prototype methods (new Socket().*)
	 */
	pt->Set(JS_ISOLATE,"connect"		, v8::FunctionTemplate::New(JS_ISOLATE, _connect));
	pt->Set(JS_ISOLATE,"send"			, v8::FunctionTemplate::New(JS_ISOLATE, _send));
	pt->Set(JS_ISOLATE,"receive"		, v8::FunctionTemplate::New(JS_ISOLATE, _receive));
	pt->Set(JS_ISOLATE,"receive_strict"	, v8::FunctionTemplate::New(JS_ISOLATE, _receive_strict));
	pt->Set(JS_ISOLATE,"bind"			, v8::FunctionTemplate::New(JS_ISOLATE, _bind));
	pt->Set(JS_ISOLATE,"listen"			, v8::FunctionTemplate::New(JS_ISOLATE, _listen));
	pt->Set(JS_ISOLATE,"accept"			, v8::FunctionTemplate::New(JS_ISOLATE, _accept));
	pt->Set(JS_ISOLATE,"close"			, v8::FunctionTemplate::New(JS_ISOLATE, _socketclose));
	pt->Set(JS_ISOLATE,"setOption"		, v8::FunctionTemplate::New(JS_ISOLATE, _setoption));
	pt->Set(JS_ISOLATE,"getOption"		, v8::FunctionTemplate::New(JS_ISOLATE, _getoption));
	pt->Set(JS_ISOLATE,"setBlocking"	, v8::FunctionTemplate::New(JS_ISOLATE, _setblocking));
	pt->Set(JS_ISOLATE,"getPeerName"	, v8::FunctionTemplate::New(JS_ISOLATE, _getpeername));

	(void)exports->Set(JS_CONTEXT,JS_STR("Socket"), socketTemplate->GetFunction(JS_CONTEXT).ToLocalChecked());
	_socketFunc.Reset(JS_ISOLATE, socketTemplate->GetFunction(JS_CONTEXT).ToLocalChecked());
	//fprintf(stderr,"socket.cc > SHARED_INIT end()\n");
}
