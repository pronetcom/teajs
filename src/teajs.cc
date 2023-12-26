/**
 * TeaJS - (fast(cgi)) binary
 */

#include <v8.h>
#include <v8-debug.h>
#include <cstdlib>
#include <cstring>
#include "app.h"
#include "macros.h"
#include "path.h"
#include <csignal>

#if defined(FASTCGI) || defined(FASTCGI_JS)
#  include <fcgi_stdio.h>
#endif

#ifdef windows
#  include <fcntl.h> /*  _O_BINARY */
   unsigned int _CRT_fmode = _O_BINARY; 
#endif

/**
 * Format for command line arguments
 *
 * as you can see if you wish to pass any arguments to v8, you MUST
 * put a -- surrounded by whitespace after all the v8 arguments
 *
 * any arguments after the v8_args but before the program_file are
 * used by TeaJS.
 */
static const char * const teajs_usage = "tea [v8_args --] [-v] [-h] [-c path] [-d port] program_file [argument ...]";

class TeaJS_CGI : public TeaJS_App {
public:
	/**
	 * Initialize from command line
	 */
	virtual void init(int argc, char ** argv) {
		TeaJS_App::init(argc,argv);
		
		this->argv0 = (argc > 0 ? path_normalize(argv[0]) : std::string(""));

		if (argc == 1) {
			/* no command-line arguments, try looking for CGI env vars */
			this->fromEnvVars();
		}
		
		// TODO vahvarh try catch throw
		try {
			this->process_args(argc, argv);
		} catch (std::string e) {
			fwrite((void *) e.c_str(), sizeof(char), e.length(), stderr);
			fwrite((void *) "\n", sizeof(char), 1, stderr);
			this->exit_code = 1;
		}
	}

	void fromEnvVars() {
		char * env = getenv("PATH_TRANSLATED");
		if (!env) { env = getenv("SCRIPT_FILENAME"); }
		if (env) { this->mainfile = std::string(env); }
	}
	
private:
	std::string argv0;

	const char * instanceType() { 
		return "cli";
	}
	
	const char * executableName() {
		return this->argv0.c_str();
	}
	
	/**
	 * Process command line arguments.
	 */
	void process_args(int argc, char ** argv) {
		std::string err = "Invalid command line usage.\n";
		err += "Correct usage: ";
		err += teajs_usage; /* see the teajs_usage deftion for the format */
		
		int index = 0;

		/* see if we have v8 options */
		bool have_v8args = false;
		for (; index < argc; ++index) {
			/* FIXME: if there is a standalone "--" after the name of the script
			 then this breaks.  I can't figure out a solution to this, so
			 for now we don't support any standalone "--" after the script name.
			 One solution (and the way it currently works) is to require "--"
			 before all other args even if no v8 args are used, but that seems
			 I don't like this, but it is where we are right now. */
			if (std::string(argv[index]) == "--") {
				/* treat all previous arguments as v8 arguments */
				int v8argc = index;
				v8::V8::SetFlagsFromCommandLine(&v8argc, argv, true);
				index++; /* skip over the "--" */
				have_v8args = true;
				break;
			}
		}
		
		/* if there were no v8 args, then reset index to the first argument */
		if (!have_v8args) { index = 1; }
		
		/* scan for teajs-specific arguments now */
		while (index < argc) {
			std::string optname(argv[index]);
			if (optname[0] != '-') { break; } /* not starting with "-" => mainfile */
			if (optname.length() != 2) {
				JS_ERROR(err);
			} /* one-character options only */
			index++; /* skip the option name */
			
			switch (optname[1]) {
				case 'c':
					if (index >= argc) {
						JS_ERROR(err);
					} /* missing option value */
					this->cfgfile = argv[index];		
#ifdef VERBOSE
					printf("cfgfile: %s\n", argv[index]);
#endif
					index++; /* skip the option value */
				break;
				
				case 'h':
					printf(teajs_usage);
					printf("\n");
				break;
				
				case 'v':
					printf("TeaJS version %s", STRING(VERSION));
					printf("\n");
				break;
				
				
				default:
					JS_ERROR(err);
			}
			
		} 
		
		if (index < argc) {
			/* argv[index] is the program file */
			this->mainfile = argv[index];
			/* expand mainfile to absolute path */
			if (!path_isabsolute(this->mainfile)) {
				std::string tmp = path_getcwd();
				tmp += "/";
				tmp += this->mainfile;
				this->mainfile = path_normalize(this->mainfile);
			}
			index++; /* skip over the program_file */
		}
		
		/* all the rest of the arguments are arguments to the program_file */
		for (; index < argc; ++index) {
#ifdef VERBOSE
			printf("program_arg: %s\n", argv[index]);
#endif
			this->mainfile_args.push_back(std::string(argv[index]));
		}
	}
};

#if defined(FASTCGI) || defined(FASTCGI_JS)
	/**
	 * This is true only after we receive a signal to terminate
	 */
	void handle_sigterm(int param) {
		FCGI_SetExitStatus(0);
		exit(0); 
 	}

	void handle_sigusr1(int param) {
		FCGI_SetExitStatus(0);
		exit(0); 
	}
#endif

extern char ** environ;

void MAIN_DEBUG(const char*str)
{
	return;
	FILE *out=fopen("/tmp/tea.fastcgi","a");
	fprintf(out,"MAIN_DEBUG: %s\n",str);
	fclose(out);
}

int main(int argc, char ** argv) {
	MAIN_DEBUG("step 0");
	//sprintf(tmp,"FCGX_IsCGI=%d",FCGX_IsCGI());MAIN_DEBUG(tmp);
	TeaJS_CGI cgi;
	MAIN_DEBUG("step 1");
	cgi.init(argc, argv);
	if (cgi.exit_code) {
		fprintf(stderr,"main() - cgi.exit_code=%d\n",cgi.exit_code);
		exit(cgi.exit_code);
	}
	MAIN_DEBUG("step 2");

#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif

#if defined(FASTCGI) || defined(FASTCGI_JS)
	signal(SIGTERM, handle_sigterm);
#  ifdef SIGUSR1
	signal(SIGUSR1, handle_sigusr1);
#  endif
# endif

#ifdef FASTCGI_JS
	FCGI_Accept();
	fcgi_pre_accepted=1;
#endif

	MAIN_DEBUG("step 3");
#ifdef FASTCGI
	/**
	 * FastCGI main loop
	 */
	MAIN_DEBUG("step 4");
	while (FCGI_Accept() >= 0) {
		MAIN_DEBUG("step 5");
		cgi.fromEnvVars();
#endif

		// vahvarh TODO try catch throw
		try {
			cgi.execute(environ);
		} catch (std::string e) {
			FILE * target;
			target = (cgi.show_errors ? stdout : stderr);
			fwrite((void *) e.c_str(), sizeof(char), e.length(), target);
			fwrite((void *) "\n", sizeof(char), 1, target);
		}
		
#ifdef FASTCGI
		FCGI_SetExitStatus(cgi.exit_code);
	}
#endif
	MAIN_DEBUG("step 6");

	return cgi.exit_code;
}
