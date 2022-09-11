#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <v8.h>
#include <string>

#if defined(FASTCGI) || defined(FASTCGI_JS)
#  include <fcgi_stdio.h>
#endif

#include "macros.h"
#include "common.h"
#include "app.h"
#include "system.h"
#include "path.h"
#include <unistd.h>
#include <sys/time.h>

#ifndef HAVE_SLEEP
#	include <windows.h>
#	include <process.h>
#	define sleep(num) { Sleep(num * 1000); }
#	define usleep(num) { Sleep(num / 1000); }
#endif

extern char ** environ;
namespace {

v8::Global<v8::Function> js_stdin;
v8::Global<v8::Function> js_stdout;
v8::Global<v8::Function> js_stderr;

/* extract environment variables and create JS object */
void extract_env(char **envp,v8::Local<v8::Object> env)
{
	std::string name, value;
	bool done;
	int i,j;
	char ch;
	for (i = 0; envp[i] != NULL; i++) {
		done = false;
		name = "";
		value = "";
		for (j = 0; envp[i][j] != '\0'; j++) {
			ch = envp[i][j];
			if (!done) {
				if (ch == '=') {
					done = true;
				} else {
					name += ch;
				}
			} else {
				value += ch;
			}
		}
		(void)env->Set(JS_CONTEXT,JS_STR(name.c_str()), JS_STR(value.c_str()));
	}
}


/**
 * Read characters from stdin
 * @param {int} count How many; 0 == all
 */
JS_METHOD(_read) {
	size_t count = 0;
	if (args.Length() && args[0]->IsNumber()) {
		count = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	}
	
	READ(stdin, count, args);
}

JS_METHOD(_readline) {
	int size = args[1]->Int32Value(JS_CONTEXT).ToChecked();
	if (size < 1) { size = 0xFFFF; }
	READ_LINE(stdin, size, args);
}

/**
 * Dump data to stdout
 * @param {string||Buffer} String or Buffer
 */
JS_METHOD(_write_stdout) {
	WRITE(stdout, args[0]);
	args.GetReturnValue().Set(v8::Local<v8::Function>::New(JS_ISOLATE, js_stdout));
}

JS_METHOD(_write_stderr) {
	WRITE(stderr, args[0]);
	args.GetReturnValue().Set(v8::Local<v8::Function>::New(JS_ISOLATE, js_stderr));
}

JS_METHOD(_writeline_stdout) {
	v8::Local<v8::Value> str = args[0];
	if (!args.Length()) { str = JS_STR(""); }
	WRITE_LINE(stdout, str);
	args.GetReturnValue().Set(v8::Local<v8::Function>::New(JS_ISOLATE, js_stdout));
}

JS_METHOD(_writeline_stderr) {
	v8::Local<v8::Value> str = args[0];
	if (!args.Length()) { str = JS_STR(""); }
	WRITE_LINE(stderr, str);
	args.GetReturnValue().Set(v8::Local<v8::Function>::New(JS_ISOLATE, js_stderr));
}

JS_METHOD(_getcwd) {
	args.GetReturnValue().Set(JS_STR(path_getcwd().c_str()));
}

JS_METHOD(_getpid) {
	args.GetReturnValue().Set(getppid());
}

#if defined(FASTCGI_JS)
JS_METHOD(_FCGI_Accept) {
	if (fcgi_pre_accepted) {
		args.GetReturnValue().Set(1);
		fcgi_pre_accepted=0;
	}
	args.GetReturnValue().Set(FCGI_Accept());
}
#endif

/**
 * Sleep for a given number of seconds
 */
JS_METHOD(_sleep) {
	int num = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	{
		v8::Unlocker unlocker(JS_ISOLATE);
		sleep(num);
	}
	args.GetReturnValue().SetUndefined();
}

/**
 * reextract env
 */
JS_METHOD(_reextract_env) {
	//v8::Local<v8::Object> system = global->Get(JS_CONTEXT,JS_STR("system"));
	//v8::Local<v8::Object> env = system->Get(JS_CONTEXT,JS_STR("env"));
	v8::Local<v8::Object> env = v8::Object::New(JS_ISOLATE);
	(void)args.This()->Set(JS_CONTEXT,JS_STR("env"), env);
	//system->Set(JS_STR("env"), env);
	extract_env(environ,env);
	args.GetReturnValue().SetUndefined();
}

/**
 * Sleep for a given number of microseconds
 */
JS_METHOD(_usleep) {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	int num = args[0]->Int32Value(JS_CONTEXT).ToChecked();
	usleep(num);
	args.GetReturnValue().SetUndefined();
}

/**
 * run GC and forzen for n ms, default is 1000
 */
JS_METHOD(_gc) {
	int idle_time = args.Length() ? args[0]->Int32Value(JS_CONTEXT).ToChecked() : 1000;
	JS_ISOLATE->IdleNotificationDeadline(idle_time);
	JS_ISOLATE->LowMemoryNotification();
	args.GetReturnValue().SetUndefined();
}

/**
 * run HEAP statistics
 */
JS_METHOD(_heap_statistics) {
	v8::HeapStatistics heap_statistics;
	v8::Local<v8::Object> result = v8::Object::New(JS_ISOLATE);

	JS_ISOLATE->GetHeapStatistics(&heap_statistics);

	(void)result->Set(JS_CONTEXT,JS_STR("total_heap_size"), JS_BIGINT(heap_statistics.total_heap_size()));
	(void)result->Set(JS_CONTEXT,JS_STR("total_heap_size_executable"), JS_BIGINT(heap_statistics.total_heap_size_executable()));
	(void)result->Set(JS_CONTEXT,JS_STR("total_physical_size"), JS_BIGINT(heap_statistics.total_physical_size()));
	(void)result->Set(JS_CONTEXT,JS_STR("used_heap_size"), JS_BIGINT(heap_statistics.used_heap_size()));
	(void)result->Set(JS_CONTEXT,JS_STR("heap_size_limit"), JS_BIGINT(heap_statistics.heap_size_limit()));

	args.GetReturnValue().Set(result);
}

/**
 * Return the number of microseconds that have elapsed since the epoch.
 */
JS_METHOD(_getTimeInMicroseconds) {
	struct timeval tv;
	gettimeofday(&tv, 0);
	char buffer[24];
	sprintf(buffer, "%lu%06lu", tv.tv_sec, tv.tv_usec);
	args.GetReturnValue().Set(JS_STR(buffer)->ToNumber(JS_CONTEXT).ToLocalChecked());
}

JS_METHOD(_flush_stdout) {
	if (fflush(stdout)) { JS_ERROR("Can not flush stdout"); return; }
	args.GetReturnValue().SetUndefined();
}

JS_METHOD(_flush_stderr) {
	if (fflush(stderr)) { JS_ERROR("Can not flush stderr"); return; }
	args.GetReturnValue().SetUndefined();
}

}

void setup_system(v8::Local<v8::Object> global, char ** envp, std::string mainfile, std::vector<std::string> args) {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	v8::Local<v8::Object> system = v8::Object::New(JS_ISOLATE);
	v8::Local<v8::Object> env = v8::Object::New(JS_ISOLATE);
	(void)global->Set(JS_CONTEXT,JS_STR("system"), system);
	
	/**
	 * Create system.args 
	 */
	v8::Local<v8::Array> arr = v8::Array::New(JS_ISOLATE);
	(void)arr->Set(JS_CONTEXT,JS_INT(0), JS_STR(mainfile.c_str()));
	for (size_t i = 0; i < args.size(); ++i) {
		(void)arr->Set(JS_CONTEXT,JS_INT((int)(i+1)), JS_STR(args.at(i).c_str()));
	}
	(void)system->Set(JS_CONTEXT,JS_STR("args"), arr);

	v8::Local<v8::Function> _js_stdin = v8::FunctionTemplate::New(JS_ISOLATE, _read)->GetFunction(JS_CONTEXT).ToLocalChecked();
	(void)system->Set(JS_CONTEXT,JS_STR("stdin"), _js_stdin);
	(void)_js_stdin->Set(JS_CONTEXT,JS_STR("read"), _js_stdin);
	(void)_js_stdin->Set(JS_CONTEXT,JS_STR("readLine"), v8::FunctionTemplate::New(JS_ISOLATE, _readline)->GetFunction(JS_CONTEXT).ToLocalChecked());
	js_stdin.Reset(JS_ISOLATE, _js_stdin);

	v8::Local<v8::Function> _js_stdout = v8::FunctionTemplate::New(JS_ISOLATE, _write_stdout)->GetFunction(JS_CONTEXT).ToLocalChecked();
	(void)system->Set(JS_CONTEXT,JS_STR("stdout"), _js_stdout);
	(void)_js_stdout->Set(JS_CONTEXT,JS_STR("write"), _js_stdout);
	(void)_js_stdout->Set(JS_CONTEXT,JS_STR("writeLine"), v8::FunctionTemplate::New(JS_ISOLATE, _writeline_stdout)->GetFunction(JS_CONTEXT).ToLocalChecked());
	(void)_js_stdout->Set(JS_CONTEXT,JS_STR("flush"), v8::FunctionTemplate::New(JS_ISOLATE, _flush_stdout)->GetFunction(JS_CONTEXT).ToLocalChecked());
	js_stdout.Reset(JS_ISOLATE, _js_stdout);

	v8::Local<v8::Function> _js_stderr = v8::FunctionTemplate::New(JS_ISOLATE, _write_stderr)->GetFunction(JS_CONTEXT).ToLocalChecked();
	(void)system->Set(JS_CONTEXT,JS_STR("stderr"), _js_stderr);
	(void)_js_stderr->Set(JS_CONTEXT,JS_STR("write"), _js_stderr);
	(void)_js_stderr->Set(JS_CONTEXT,JS_STR("writeLine"), v8::FunctionTemplate::New(JS_ISOLATE, _writeline_stderr)->GetFunction(JS_CONTEXT).ToLocalChecked());
	(void)_js_stderr->Set(JS_CONTEXT,JS_STR("flush"), v8::FunctionTemplate::New(JS_ISOLATE, _flush_stderr)->GetFunction(JS_CONTEXT).ToLocalChecked());
	js_stderr.Reset(JS_ISOLATE, _js_stderr);
	
	(void)system->Set(JS_CONTEXT,JS_STR("getcwd"), v8::FunctionTemplate::New(JS_ISOLATE, _getcwd)->GetFunction(JS_CONTEXT).ToLocalChecked());
	(void)system->Set(JS_CONTEXT,JS_STR("getpid"), v8::FunctionTemplate::New(JS_ISOLATE, _getpid)->GetFunction(JS_CONTEXT).ToLocalChecked());
	(void)system->Set(JS_CONTEXT,JS_STR("sleep"), v8::FunctionTemplate::New(JS_ISOLATE, _sleep)->GetFunction(JS_CONTEXT).ToLocalChecked());
	(void)system->Set(JS_CONTEXT,JS_STR("usleep"), v8::FunctionTemplate::New(JS_ISOLATE, _usleep)->GetFunction(JS_CONTEXT).ToLocalChecked());
	(void)system->Set(JS_CONTEXT,JS_STR("gc"), v8::FunctionTemplate::New(JS_ISOLATE, _gc)->GetFunction(JS_CONTEXT).ToLocalChecked());
	(void)system->Set(JS_CONTEXT,JS_STR("heap_statistics"), v8::FunctionTemplate::New(JS_ISOLATE, _heap_statistics)->GetFunction(JS_CONTEXT).ToLocalChecked());
	(void)system->Set(JS_CONTEXT,JS_STR("getTimeInMicroseconds"), v8::FunctionTemplate::New(JS_ISOLATE, _getTimeInMicroseconds)->GetFunction(JS_CONTEXT).ToLocalChecked());
	(void)system->Set(JS_CONTEXT,JS_STR("env"), env);
	(void)system->Set(JS_CONTEXT,JS_STR("version"), JS_STR(STRING(VERSION)));
	
#if defined(FASTCGI_JS)
	(void)system->Set(JS_CONTEXT,JS_STR("FCGI_Accept"), v8::FunctionTemplate::New(JS_ISOLATE, _FCGI_Accept)->GetFunction(JS_CONTEXT).ToLocalChecked());
	(void)system->Set(JS_CONTEXT,JS_STR("reextract_env"), v8::FunctionTemplate::New(JS_ISOLATE, _reextract_env)->GetFunction(JS_CONTEXT).ToLocalChecked());
#endif

	extract_env(envp,env);
}
