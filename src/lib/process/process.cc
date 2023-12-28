/**
 * Process control - running other tasks and manipulating their input/output
 */

#include <v8.h>
#include <string>
#include <vector>
#include "macros.h"
#include "common.h"
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <fcntl.h>

#include <iostream>
#include <string>

#ifndef windows
#  include <sys/wait.h>
#  include <paths.h>  // for _PATH_BSHELL
#else
#  include <process.h>
#  define _PATH_BSHELL "sh"
#endif

namespace {

	JS_METHOD(_process) {
		ASSERT_CONSTRUCTOR;
		SAVE_PTR(0, NULL);
		args.GetReturnValue().Set(args.This());
	}

	JS_METHOD(_system) {
		if (args.Length() != 1) {
			JS_TYPE_ERROR("Wrong argument count. Use new Process().system(\"command\")");
			return;
		}

		v8::String::Utf8Value cmd(JS_ISOLATE, args[0]);
		int result = system(*cmd);
		args.GetReturnValue().Set(JS_INT(result));
	}

	/**
	 * The initial .exec method for stream buffering system commands
	 */
	JS_METHOD(_exec) {
		if (args.Length() != 1) {
			JS_TYPE_ERROR("Wrong argument count. Use new Process().exec(\"command\")");
			return;
		}

		std::string data;
		FILE* stream;
		const int MAX_BUFFER = 256;
		char buffer[MAX_BUFFER];

		v8::String::Utf8Value cmd(JS_ISOLATE, args[0]);
		stream = popen(*cmd, "r");
		while (fgets(buffer, MAX_BUFFER, stream) != NULL) {
			data.append(buffer);
		}
		pclose(stream);

		if (data.c_str() != NULL) {
			args.GetReturnValue().Set(JS_STR(data.c_str()));
		}
		else {
			args.GetReturnValue().SetNull();
		}
	}


	/**
	 * Executes the specified comment, setting the environment if requested. Like
	 * the exec* family this does not return.
	 *
	 * @param command the command to run
	 * @param env an object whose fields will be the environment variables or null
	 *		to use the current environment.
	 */
	void _executecommand(
		const char* command, v8::Object* env) {
		if (env != NULL) {

			std::vector<std::string> env_strings;
			std::vector<char const*> env_pointers;
			v8::Local<v8::Array> keys = env->GetPropertyNames(JS_CONTEXT).ToLocalChecked();
			for (unsigned int i = 0; i < keys->Length(); i++) {
				v8::String::Utf8Value key(JS_ISOLATE, keys->Get(JS_CONTEXT, JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked());
				v8::String::Utf8Value value(JS_ISOLATE, env->Get(JS_CONTEXT, JS_STR(*key)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked());
				env_strings.push_back(std::string(*key) + "=" + *value);
				env_pointers.push_back(env_strings.back().c_str());
			}
			env_pointers.push_back(NULL);

			// There is no execlep so on Windows we hope that the first version works.
			execle(
				_PATH_BSHELL, "sh", "-c", command, (char*)NULL, &env_pointers.front());
		}
		else {
			execl(_PATH_BSHELL, "sh", "-c", command, (char*)NULL);
		}
		exit(127);  // "command not found"
	}

	void _executeWithArgs(
		const char* argv[], v8::Object* env) {
		if (env != NULL) {

			std::vector<std::string> env_strings;
			std::vector<char const*> env_pointers;
			v8::Local<v8::Array> keys = env->GetPropertyNames(JS_CONTEXT).ToLocalChecked();
			for (unsigned int i = 0; i < keys->Length(); i++) {
				v8::String::Utf8Value key(JS_ISOLATE, keys->Get(JS_CONTEXT, JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked());
				v8::String::Utf8Value value(JS_ISOLATE, env->Get(JS_CONTEXT, JS_STR(*key)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked());
				env_strings.push_back(std::string(*key) + "=" + *value);
				env_pointers.push_back(env_strings.back().c_str());
			}
			env_pointers.push_back(NULL);

			// There is no execlep so on Windows we hope that the first version works.
			execvpe(
				argv[0], (char* const*)argv, (char* const*)(&env_pointers.front()));
		}
		else {
			execvp(argv[0], (char* const*)argv);
		}
		exit(127);  // "command not found"
	}

	/**
	 * Execute the given command, and return exit code and stdout/stderr data
	 *
	 * TODO: any reason to keep exec?  Either remove exec and rename exec2 to exec,
	 * or rename exec2 to something else.
	 *
	 * @param command    {String}  bash command
	 * @param input      {String}  optional; text to send to process as stdin
	 * @param env        {Object|null}  optional; an object whose fields will be the
	 *                             environment variables. If null or missing then
	 *                             the current environment will be used.
	 *
	 * @return a JavaScript object like: {
	 *   status: {Integer} the return code from the process
	 *   out:    {String}  contents of stdout
	 *   err:    {String}  contents of stderr
	 * }
	 */

#ifndef windows

	JS_METHOD(_exec2) {
		int arg_count = args.Length();
		if (arg_count < 1 || arg_count > 3) {
			JS_TYPE_ERROR("Wrong argument count. Use new Process().exec2(\"command\", [\"standard input\"], [\"env\"])");
			return;
		}

		const int MAX_BUFFER = 256;
		char buffer[MAX_BUFFER + 1];

		v8::String::Utf8Value command_arg(JS_ISOLATE, args[0]);
		v8::Object* env = NULL;
		if (arg_count >= 3 && !((*args[2])->IsNull())) {
			env = (*args[2]->ToObject(JS_CONTEXT).ToLocalChecked());
		}

		// File descriptors all named from perspective of child process.
		int input_fd[2];
		int out_fd[2];
		int err_fd[2];

		if (pipe(input_fd) == -1) { // Where the parent is going to write to
			JS_ERROR("Failed to pipe input (exec2)");
			return;
		}
		if (pipe(out_fd) == -1) { // From where parent is going to read
			JS_ERROR("Failed to pipe output (exec2)");
			return;
		}
		if (pipe(err_fd) == -1) {
			JS_ERROR("Failed to pipe error (exec2)");
			return;
		}

		int pid = fork();
		switch (pid) {

		case -1:  // Error case.
			JS_ERROR("Failed to fork process");
			return;

		case 0:  // Child process.

			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			close(STDIN_FILENO);

			dup2(input_fd[0], STDIN_FILENO);
			dup2(out_fd[1], STDOUT_FILENO);
			dup2(err_fd[1], STDERR_FILENO);

			close(input_fd[0]); // Not required for the child
			close(input_fd[1]);
			close(out_fd[0]);
			close(out_fd[1]);
			close(err_fd[0]);
			close(err_fd[1]);

			_executecommand(*command_arg, env);

			args.GetReturnValue().SetNull();  // unreachable
			return;

		default:  // Parent process.

			close(input_fd[0]); // These are being used by the child
			close(out_fd[1]);
			close(err_fd[1]);

			if (arg_count >= 2) {
				v8::String::Utf8Value input_arg(JS_ISOLATE, args[1]);
				if (write(input_fd[1], *input_arg, input_arg.length()) == -1) { // Write to child’s stdin
					JS_ERROR("Failed to write to child's stdin (exec2)");
					return;
				}
			}
			close(input_fd[1]);

			std::string ret_out;
			while (true) {
				int bytes_read = (int)read(out_fd[0], buffer, MAX_BUFFER);
				if (bytes_read == 0) {
					break;
				}
				else if (bytes_read < 0) {
					JS_ERROR("Error while trying read child's output");
					args.GetReturnValue().SetNull();
					return;
				}
				buffer[bytes_read] = 0;
				ret_out.append(buffer);
			}
			close(out_fd[0]);

			std::string ret_err;
			while (true) {
				int bytes_read = (int)read(err_fd[0], buffer, MAX_BUFFER);
				if (bytes_read == 0) {
					break;
				}
				else if (bytes_read < 0) {
					JS_ERROR("Error while trying read child's errput");
					args.GetReturnValue().SetNull();
					return;
				}
				buffer[bytes_read] = 0;
				ret_err.append(buffer);
			}
			close(err_fd[0]);

			int status;
			waitpid(pid, &status, 0);

			v8::Local<v8::Object> ret = v8::Object::New(JS_ISOLATE);
			if (WIFEXITED(status)) {
				(void)ret->Set(JS_CONTEXT, JS_STR("status"), JS_INT(WEXITSTATUS(status)));
			}
			else {
				// TODO: What should we do in this case? It's not clear how to return
				// other exit codes (for signals, for example).
				(void)ret->Set(JS_CONTEXT, JS_STR("status"), JS_INT(-1));
			}
			(void)ret->Set(JS_CONTEXT, JS_STR("out"), JS_STR(ret_out.c_str()));
			(void)ret->Set(JS_CONTEXT, JS_STR("err"), JS_STR(ret_err.c_str()));
			args.GetReturnValue().Set(ret);
		}
	}

	JS_METHOD(_exec3) {
		int arg_count = args.Length();
		if (arg_count < 1 || arg_count > 2) {
			JS_TYPE_ERROR("Wrong argument count. Use new Process().exec3([\"commands\"], [\"env\"])");
			return;
		}

		// const int MAX_BUFFER = 256;
		// char buffer[MAX_BUFFER + 1];

		v8::Local<v8::Array> command_args = v8::Local<v8::Array>::Cast(args[0]);
		const char** argv = new const char* [command_args->Length() + 1];
		for (unsigned int i = 0; i < command_args->Length(); i++) {
			v8::String::Utf8Value arg(JS_ISOLATE, command_args->Get(JS_CONTEXT, JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked());
			argv[i] = strdup(*arg);
		}
		argv[command_args->Length()] = nullptr;
		v8::Object* env = NULL;
		if (arg_count >= 2 && !((*args[1])->IsNull())) {
			env = (*args[1]->ToObject(JS_CONTEXT).ToLocalChecked());
		}

		// File descriptors all named from perspective of child process.
		int input_fd[2];
		int out_fd[2];
		int err_fd[2];

		if (pipe(input_fd) == -1) { // Where the parent is going to write to
			JS_ERROR("Failed to pipe input (exec3)");
			return;
		}
		if (pipe(out_fd) == -1) { // From where parent is going to read
			JS_ERROR("Failed to pipe output (exec3)");
			return;
		}
		if (pipe(err_fd) == -1) {
			JS_ERROR("Failed to pipe error (exec3)");
			return;
		}

		int pid = fork();
		switch (pid) {

		case -1:  // Error case.
			JS_ERROR("Failed to fork process");
			return;

		case 0:  // Child process.

			dup2(input_fd[0], STDIN_FILENO);
			dup2(out_fd[1], STDOUT_FILENO);
			dup2(err_fd[1], STDERR_FILENO);

			fcntl(input_fd[0], F_SETFL, fcntl(input_fd[0], F_GETFL) | O_NONBLOCK);
			fcntl(out_fd[1], F_SETFL, fcntl(out_fd[1], F_GETFL) | O_NONBLOCK);
			fcntl(err_fd[1], F_SETFL, fcntl(err_fd[1], F_GETFL) | O_NONBLOCK);

			close(input_fd[0]); // Not required for the child
			close(input_fd[1]);
			close(out_fd[0]);
			close(out_fd[1]);
			close(err_fd[0]);
			close(err_fd[1]);

			_executeWithArgs(argv, env);

			args.GetReturnValue().SetNull();  // unreachable
			return;

		default:  // Parent process.

			for (unsigned int i = 0; i < command_args->Length(); i++) {
				delete[] argv[i];
			}
			delete [] argv;
			delete env;
			

			close(input_fd[0]); // These are being used by the child
			close(out_fd[1]);
			close(err_fd[1]);

			v8::Local<v8::Object> ret = v8::Object::New(JS_ISOLATE);
			/*
			if (WIFEXITED(status)) {
				(void)ret->Set(JS_CONTEXT, JS_STR("status"), JS_INT(WEXITSTATUS(status)));
			}
			else {
				// TODO: What should we do in this case? It's not clear how to return
				// other exit codes (for signals, for example).
				(void)ret->Set(JS_CONTEXT, JS_STR("status"), JS_INT(-1));
			}
			(void)ret->Set(JS_CONTEXT, JS_STR("out"), JS_STR(ret_out.c_str()));
			(void)ret->Set(JS_CONTEXT, JS_STR("err"), JS_STR(ret_err.c_str()));
			*/
			std::string strIn = "", strOut = "", strErr = "";
			strIn += std::to_string(input_fd[1]);
			(void)ret->Set(JS_CONTEXT, JS_STR("in"), JS_STR(strIn.c_str()));
			
			strOut += std::to_string(out_fd[0]);
			(void)ret->Set(JS_CONTEXT, JS_STR("out"), JS_STR(strOut.c_str()));
			
			strErr += std::to_string(err_fd[0]);
			(void)ret->Set(JS_CONTEXT, JS_STR("err"), JS_STR(strErr.c_str()));
			
			args.GetReturnValue().Set(ret);
		}
	}

	JS_METHOD(_open3) {
		int arg_count = args.Length();
		if (arg_count < 1 || arg_count > 2) {
			JS_TYPE_ERROR("Wrong argument count. Use new Process().exec2(\"command\", [\"standard input\"], [\"env\"])");
			return;
		}

		const int MAX_BUFFER = 256;
		char buffer[MAX_BUFFER + 1];

		v8::String::Utf8Value command_arg(JS_ISOLATE, args[0]);
		v8::Object* env = NULL;
		if (arg_count >= 2 && !((*args[1])->IsNull())) {
			env = (*args[1]->ToObject(JS_CONTEXT).ToLocalChecked());
		}

		// File descriptors all named from perspective of child process.
		int input_fd[2];
		int out_fd[2];
		int err_fd[2];

		if (pipe(input_fd) == -1) { // Where the parent is going to write to
			JS_ERROR("Failed to pipe input (open3)");
			return;
		}
		if (pipe(out_fd) == -1) { // From where parent is going to read
			JS_ERROR("Failed to pipe output (open3)");
			return;
		}
		if (pipe(err_fd) == -1) {
			JS_ERROR("Failed to pipe error (open3)");
			return;
		}

		int pid = fork();
		switch (pid) {

		case -1:  // Error case.
			JS_ERROR("Failed to fork process");
			return;

		case 0:  // Child process.

			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			close(STDIN_FILENO);

			dup2(input_fd[0], STDIN_FILENO);
			dup2(out_fd[1], STDOUT_FILENO);
			dup2(err_fd[1], STDERR_FILENO);

			close(input_fd[0]); // Not required for the child
			close(input_fd[1]);
			close(out_fd[0]);
			close(out_fd[1]);
			close(err_fd[0]);
			close(err_fd[1]);

			_executecommand(*command_arg, env);

			args.GetReturnValue().SetNull();  // unreachable
			return;

		default:  // Parent process.

			close(input_fd[0]); // These are being used by the child
			close(out_fd[1]);
			close(err_fd[1]);

			if (arg_count >= 2) {
				v8::String::Utf8Value input_arg(JS_ISOLATE, args[1]);
				if (write(input_fd[1], *input_arg, input_arg.length()) == -1) { // Write to child’s stdin
					JS_ERROR("Failed to write to child's stdin (open3)");
					return;
				}
			}
			close(input_fd[1]);

			std::string ret_out;
			while (true) {
				int bytes_read = (int)read(out_fd[0], buffer, MAX_BUFFER);
				if (bytes_read == 0) {
					break;
				}
				else if (bytes_read < 0) {
					JS_ERROR("Error while trying read child's output");
					args.GetReturnValue().SetNull();
					return;
				}
				buffer[bytes_read] = 0;
				ret_out.append(buffer);
			}
			close(out_fd[0]);

			std::string ret_err;
			while (true) {
				int bytes_read = (int)read(err_fd[0], buffer, MAX_BUFFER);
				if (bytes_read == 0) {
					break;
				}
				else if (bytes_read < 0) {
					JS_ERROR("Error while trying read child's errput");
					args.GetReturnValue().SetNull();
					return;
				}
				buffer[bytes_read] = 0;
				ret_err.append(buffer);
			}
			close(err_fd[0]);

			int status;
			waitpid(pid, &status, 0);

			v8::Local<v8::Object> ret = v8::Object::New(JS_ISOLATE);
			if (WIFEXITED(status)) {
				(void)ret->Set(JS_CONTEXT, JS_STR("status"), JS_INT(WEXITSTATUS(status)));
			}
			else {
				// TODO: What should we do in this case? It's not clear how to return
				// other exit codes (for signals, for example).
				(void)ret->Set(JS_CONTEXT, JS_STR("status"), JS_INT(-1));
			}
			(void)ret->Set(JS_CONTEXT, JS_STR("out"), JS_STR(ret_out.c_str()));
			(void)ret->Set(JS_CONTEXT, JS_STR("err"), JS_STR(ret_err.c_str()));
			args.GetReturnValue().Set(ret);
		}
	}

	/**
	 * Execute the given command as a background process.
	 *
	 * @param command    {String}  bash command
	 * @param input      {String}  optional; text to send to process as stdin
	 * @param env        {Object|null}  optional; an object whose fields will be the
	 *                             environment variables. If null or missing then
	 *                             the current environment will be used.
	 * @return undefined
	 */
	JS_METHOD(_fork) {
		int arg_count = args.Length();
		if (arg_count < 1 || arg_count > 3) {
			JS_TYPE_ERROR("Wrong argument count. Use new Process().exec2(\"command\", [\"standard input\"], [\"env\"])");
			return;
		}

		v8::String::Utf8Value command_arg(JS_ISOLATE, args[0]);
		v8::Object* env = NULL;
		if (arg_count >= 3 && !((*args[2])->IsNull())) {
			env = (*args[2]->ToObject(JS_CONTEXT).ToLocalChecked());
		}

		int input_fd[2];
		if (pipe(input_fd) == -1) { // Where the parent is going to write to
			JS_ERROR("Failed to pipe input (fork)");
			return;
		}

		int pid = fork();
		switch (pid) {

		case -1:  // Error case.
			JS_ERROR("Failed to fork process");
			return;

		case 0: { // Child process.

			close(STDIN_FILENO);
			dup2(input_fd[0], STDIN_FILENO);
			close(input_fd[0]); // Not required for the child
			close(input_fd[1]);

			_executecommand(*command_arg, env);

			args.GetReturnValue().SetNull();  // unreachable
			return;
		}

		default:  // Parent process.

			close(input_fd[0]); // These are being used by the child
			if (arg_count >= 2) {
				v8::String::Utf8Value input_arg(JS_ISOLATE, args[1]);
				if (write(input_fd[1], *input_arg, input_arg.length()) == -1) { // Write to child’s stdin
					JS_ERROR("Failed to write to child's stdin (fork)");
					return;
				}
			}
			close(input_fd[1]);
			args.GetReturnValue().SetUndefined();
			return;
		}
	}

#endif

}

SHARED_INIT() {
	//fprintf(stderr,"process.cc > SHARED_INIT					isolate=%ld, InContext()=%d, context=%ld\n",(void*)JS_ISOLATE,JS_ISOLATE->InContext(),(void*)(*JS_CONTEXT));
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	v8::Local<v8::FunctionTemplate> funct = v8::FunctionTemplate::New(JS_ISOLATE, _process);
	funct->SetClassName(JS_STR("Process"));
	v8::Local<v8::ObjectTemplate> ot = funct->InstanceTemplate();
	ot->SetInternalFieldCount(1);
	v8::Local<v8::ObjectTemplate> process = funct->PrototypeTemplate();

	/* this module provides a set of (static) functions */
	process->Set(JS_ISOLATE, "system", v8::FunctionTemplate::New(JS_ISOLATE, _system));
	process->Set(JS_ISOLATE, "exec", v8::FunctionTemplate::New(JS_ISOLATE, _exec));

#ifndef windows
	process->Set(JS_ISOLATE, "exec2", v8::FunctionTemplate::New(JS_ISOLATE, _exec2));
	process->Set(JS_ISOLATE, "exec3", v8::FunctionTemplate::New(JS_ISOLATE, _exec3));
	process->Set(JS_ISOLATE, "open3", v8::FunctionTemplate::New(JS_ISOLATE, _exec2));
	process->Set(JS_ISOLATE, "fork", v8::FunctionTemplate::New(JS_ISOLATE, _fork));
#endif

	(void)exports->Set(JS_CONTEXT, JS_STR("Process"), funct->GetFunction(JS_CONTEXT).ToLocalChecked());
}
