/*
 *	PostgreSQL database module for TeaJS
 *
 *	Based on the corresponding MySQL and SQLite modules
 *	"js_mysql.cc" and "js_sqlite.cc", respectively, from
 *	v8cgi release 0.6.0; see also:
 *	1. http://www.postgresql.org/docs/8.4/static/libpq-exec.html
 *	2. http://www.postgresql.org/docs/8.4/static/libpq-async.html
 *
 *	Initial version: 2009-08-17, Ryan RAFFERTY
 * 	Minor segfault fixes Ondrej ZARA
 */

#include <stdlib.h>
#include <unistd.h>
#include <map>
#include <memory>
#include <stdio.h>
#include <string.h>
#include <sstream>

#include <v8.h>
#include "macros.h"
#include "common.h"
#include "gc.h"

#ifdef windows
#	include <windows.h>
#	define sleep(num) { Sleep(num * 1000); }
#endif

//namespace pgsql {

	// Establish lock to prevent violation of V8 threading
	// restrictions:


	//v8::Locker global_lock(JS_ISOLATE);

	#include <pthread.h>

	extern "C" {
//	namespace pq {
//#ifndef windows
//		#include <postgres_fe.h>
//#endif
		#include <libpq-fe.h>
		#include <libpq/libpq-fs.h>
//	}
	}
	#ifdef __linux__
	#	ifndef _REENTRANT
	#		define _REENTRANT
	#	endif
	#endif

	#define PGSQL_ERROR PQerrorMessage(conn)
	#define ASSERT_CONNECTED if (!conn) { JS_ERROR("No connection established yet."); return; }
	#define ASSERT_RESULT if (!res) { JS_ERROR("No result returned yet."); return; }
	#define PGSQL_PTR_CON PGconn * conn = LOAD_PTR(0, PGconn *)
	#define PGSQL_RES_LOAD(item) PGresult * item = LOAD_PTR(0, PGresult *)
	#define PGSQL_RES_SAVE(item) SAVE_VALUE(0, item)
	#define PGSQL_RES_SETPOS(val) SAVE_VALUE(1, JS_INT(val))
	#define PGSQL_RES_POS LOAD_VALUE(1)->Int32Value(JS_CONTEXT).ToChecked()
	#define PGSQL_RES_CLEAR SAVE_PTR(0, (void *)NULL)
	//
	// If a non-blocking connection is in use,
	// then ensure that transmission of any pending
	// SQL commands and/or data is complete before
	// proceeding (generally, this pertains to async usage)
	//
	#define PGSQL_FLUSH(conn) if (PQisnonblocking(conn)) { int stat = PQflush(conn);			while (stat > 0) {	int sock = PQsocket(conn);	if (sock < 0)		{ JS_ERROR("SOCKET ERROR"); return; }	fd_set input_mask;	FD_ZERO(&input_mask);	FD_SET(sock, &input_mask);	if ( select(sock + 1, &input_mask, NULL, NULL, NULL) ) {	}	stat = PQflush(conn);			}		}



void destroy_result(v8::Local<v8::Object> obj) {
	fprintf(stderr,"pg::destroy_result() - TODO\n");exit(1);
	v8::Local<v8::Function> fun = v8::Local<v8::Function>::Cast(obj->Get(JS_CONTEXT,JS_STR("clear")).ToLocalChecked());
	(void)fun->Call(JS_CONTEXT,obj, 0, NULL);
}

void destroy_pgsql(v8::Local<v8::Object> obj) {
	fprintf(stderr,"pg::destroy_pgsql() - TODO\n");exit(1);
	v8::Local<v8::Function> fun = v8::Local<v8::Function>::Cast(obj->Get(JS_CONTEXT,JS_STR("close")).ToLocalChecked());
	(void)fun->Call(JS_CONTEXT,obj, 0, NULL);
}

void destroy_result2(void*tmp) {
	/*v8::Local<v8::Function> fun = v8::Local<v8::Function>::Cast(obj->Get(JS_CONTEXT,JS_STR("clear")).ToLocalChecked());
	(void)fun->Call(JS_CONTEXT,obj, 0, NULL);*/
	PGresult *res=(PGresult*)tmp;
	if (res) PQclear(res);
}

void destroy_pgsql2(void*tmp) {
	/*v8::Local<v8::Function> fun = v8::Local<v8::Function>::Cast(obj->Get(JS_CONTEXT,JS_STR("close")).ToLocalChecked());
	(void)fun->Call(JS_CONTEXT,obj, 0, NULL);*/
	PGconn * conn=(PGconn *)tmp;
	if (conn) PQfinish(conn);
}

/**
 * Parameters:
 *		lineno	= -1 fetch all, -2 fetch next line, >=0 - fetch lineno
 */
v8::Local<v8::Array> fetch_any(PGresult *res,int lineno,bool fetch_objects)
{
//	int x = PQnfields(res);
	int y;
	int ypg;
	int offset=0;
	if (lineno==-1) {
		y = PQntuples(res);
	} else if (lineno<0) {
		fprintf(stderr,"TODO v8::Local<v8::Array> fetch_any(PGresult *res,int lineno,bool fetch_objects)\n");
		exit(1);//assert("TODO");
	} else {
		offset=lineno;
		y=1;
	}
	int cnt = PQnfields(res);
		
	v8::Local<v8::Array> fieldnames = v8::Array::New(JS_ISOLATE, cnt);
	int *types=(int*)malloc(sizeof(int)*cnt);
	for(int u = 0; u < cnt; u++) {
		if (fetch_objects) (void)fieldnames->Set(JS_CONTEXT,JS_INT(u), JS_STR(PQfname(res, u)));
		types[u]=PQftype(res,u);
	}
	v8::Local<v8::Array> result = v8::Array::New(JS_ISOLATE, y);
	for (int i = 0; i < y; i++) {
		ypg=i+offset;
		v8::Local<v8::Object> item = v8::Object::New(JS_ISOLATE);
		(void)result->Set(JS_CONTEXT,JS_INT(i), item);
		for (int j=0; j<cnt; j++) {
			if (PQgetisnull(res, ypg, j)) {
				(void)item->Set(JS_CONTEXT,fieldnames->Get(JS_CONTEXT,JS_INT(j)).ToLocalChecked(), v8::Null(JS_ISOLATE));
				continue;
			}
			switch (types[j]) {
				case 20:
				case 21:
				case 23:
					(void)item->Set(JS_CONTEXT,fieldnames->Get(JS_CONTEXT,JS_INT(j)).ToLocalChecked(), JS_STR(PQgetvalue(res, ypg, j))->ToInteger(JS_CONTEXT).ToLocalChecked());
					break;
				case 1700:
					(void)item->Set(JS_CONTEXT,fieldnames->Get(JS_CONTEXT,JS_INT(j)).ToLocalChecked(), JS_STR(PQgetvalue(res, ypg, j))->ToNumber(JS_CONTEXT).ToLocalChecked());
					break;
				default:
					(void)item->Set(JS_CONTEXT,fieldnames->Get(JS_CONTEXT,JS_INT(j)).ToLocalChecked(), JS_STR(PQgetvalue(res, ypg, j)));
					break;
			}
		}
	}
	free(types);
	return result;
}



	/**
	 *	"rslt" corresponds to database query result objects
	 */
	v8::Global<v8::FunctionTemplate> _rslt;

	/**
	 *	Result constructor:
	 * 	- Adds "this.clear()" method to global GC so that
	 *		the V8 garbage collector automatically handles 
	 *		clearing of result structs when the corresponding
	 *		JavaScript object is destroyed/garbage-collected
	 */
	JS_METHOD(_result) {
		ASSERT_CONSTRUCTOR;
		PGSQL_RES_SAVE(args[0]);
		PGSQL_RES_SETPOS(0);
		GC * gc = GC_PTR;
		//gc->add(args.This(), destroy_result);
		gc->add(args.This(), destroy_result2,0);
		args.GetReturnValue().Set(args.This());
	}

	/**
	 *	CLEAR method
	 *	- Clears result object returned from the database
	 *		- it is necessary to clear result objects when no
	 *		longer needed, in order to prevent memory leaks
	 */ 
	JS_METHOD(_clear) {
		PGSQL_RES_LOAD(res);
		if (res) {
			PQclear(res);
			PGSQL_RES_CLEAR;
		}
		args.GetReturnValue().Set(args.This());
	}

	/**
	 *	PostgreSQL constructor:
	 * 	- Adds "this.close()" method to global GC
	 */
	JS_METHOD(_pgsql_constructor) {
		ASSERT_CONSTRUCTOR;
		v8::TryCatch try_catch(JS_ISOLATE);
		SAVE_PTR(0, NULL);
		GC * gc = GC_PTR;
		//gc->add(args.This(), destroy_pgsql);
		gc->add(args.This(), destroy_pgsql2,0);
		uint32_t len = args.Length();
		if (len > 0) {
			v8::Local<v8::Value> * fargs = new v8::Local<v8::Value>[len];
			for (uint32_t i=0;i<len;i++) {
	fargs[i] = args[i];
			}
			v8::Local<v8::Value> f = args.This()->Get(JS_CONTEXT,JS_STR("connect")).ToLocalChecked();
			v8::Local<v8::Function> connect = v8::Local<v8::Function>::Cast(f);
			v8::MaybeLocal<v8::Value> ml_ret = connect->Call(JS_CONTEXT,args.This(), len, fargs);
			if (try_catch.HasCaught()) {
				JS_ISOLATE->ThrowException(try_catch.Exception());
				return;
			}
			if (ml_ret.IsEmpty()) {
				JS_ERROR("Connect error");
				return;
			}
			v8::Local<v8::Value> ret = ml_ret.ToLocalChecked();
			delete[] fargs;
			args.GetReturnValue().Set(ret);
		} else {
			args.GetReturnValue().Set(args.This());
		}
	}

	/**
	 *	CLOSE method
	 *	- Closes connection to database server
	 */ 
	JS_METHOD(_close) {
		PGSQL_PTR_CON;
		if (conn) {
			PQfinish(conn);
			SAVE_PTR(0, NULL);
		}
		args.GetReturnValue().Set(args.This());
	}

	/**
	 *	CONNECT method
	 *	- call format: new PostgreSQL().connect("host", "user", "pass", "db")
	 */ 
	JS_METHOD(_connect) {
		if (args.Length() < 1 and args.Length() != 5) {
			JS_TYPE_ERROR("Invalid call format. Use either 'pgsql.connect(\"hostaddr=host port=port dbname=dbname user=user password=pass\")' or 'pgsql.connect(host, port, db, user, password)'");
			return;
		}
		uint32_t max = 4096;
		PGconn * conn = NULL;
		char * tconnstr = new char[max];
		if (args.Length() == 1) {
			if (args[0]->IsString()) {
	v8::String::Utf8Value connstr(JS_ISOLATE,args[0]);
	conn = PQconnectdb(*connstr);
			}
			else if (args[0]->IsArray()) {
	v8::Local<v8::Array> arr = v8::Local<v8::Array>::Cast(args[0]);
	uint32_t len = arr->Length();
	char * buf = new char[max];
	const char * keys[9] = {
			"host",
			"port",
			"dbname",
			"user",
			"password"
		};
	for (unsigned int i=0;i<len;i++) {
		v8::String::Utf8Value key(JS_ISOLATE, JS_STR(keys[i]) );
		v8::String::Utf8Value val(JS_ISOLATE, arr->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked() );
		if (i==0)
			sprintf(buf,"%s=%s",*key,*val);
		else
			sprintf(buf,"%s %s=%s",buf,*key,*val);
	}
	v8::String::Utf8Value connstr(JS_ISOLATE,JS_STR(buf));
	conn = PQconnectdb(*connstr);
	sprintf(tconnstr,"%s",buf);
	delete[] buf;
			}
			else if (args[0]->IsObject()) {
	v8::Local<v8::Object> arr = v8::Local<v8::Object>::Cast(args[0]);
	v8::Local<v8::Array> keys = v8::Local<v8::Array>::Cast(arr->GetPropertyNames(JS_CONTEXT).ToLocalChecked());
	uint32_t len = keys->Length();
	char * buf = new char[max];
	for (unsigned int i=0;i<len;i++) {
		v8::String::Utf8Value key(JS_ISOLATE, keys->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked() );
		v8::String::Utf8Value val(JS_ISOLATE, arr->Get(JS_CONTEXT,JS_STR(*key)).ToLocalChecked() );
		if (i==0)
			sprintf(buf,"%s=%s",*key,*val);
		else
			sprintf(buf,"%s %s=%s",buf,*key,*val);
	}
	v8::String::Utf8Value connstr(JS_ISOLATE,JS_STR(buf));
	conn = PQconnectdb(*connstr);
	sprintf(tconnstr,"%s",buf);
	delete[] buf;
			}
			else {
	v8::String::Utf8Value err(JS_ISOLATE,JS_STR("[js_pgsql.cc] ERROR: incorrect number of input parameters (%d)"));
	delete[] tconnstr;
	JS_ERROR(*err);
	return;
			}
		}
		else {
			v8::String::Utf8Value pghost(JS_ISOLATE,args[0]);
			v8::String::Utf8Value pgport(JS_ISOLATE,args[1]);
			v8::String::Utf8Value pgdb(JS_ISOLATE,args[2]);
			v8::String::Utf8Value pguser(JS_ISOLATE,args[3]);
			v8::String::Utf8Value pgpass(JS_ISOLATE,args[4]);
			const char * tstr = "host=%s port=%s dbname=%s user=%s password=%s connect_timeout=5";
			uint32_t tlen = (uint32_t)(pghost.length() + pgport.length() + pgdb.length() + pguser.length() + pgpass.length() + strlen(tstr));
			char * tmpstr = new char[tlen];
			sprintf(tmpstr, tstr, *pghost, *pgport, *pgdb, *pguser, *pgpass);
			v8::String::Utf8Value connstr(JS_ISOLATE,JS_STR(tmpstr));
			conn = PQconnectdb(*connstr);
			delete[] tmpstr;
		}
		if (PQstatus(conn) != CONNECTION_OK) {
			std::string ex = PGSQL_ERROR;
			if (conn) {
	PQfinish(conn);
	SAVE_PTR(0, NULL);
			}
			char err[max];
			sprintf((char *)err,"%s (connstr: [%s])",ex.c_str(),tconnstr);
			delete[] tconnstr;
			JS_ERROR(err);
			return;
		}
		else {
			SAVE_PTR(0, conn);
			delete[] tconnstr;
			args.GetReturnValue().Set(args.This());
		}
	}

	/**
	 *	QUERY method
	 *	- accepts a string as its input argument,
	 *		and returns an instance of the Result class
	 *	- alternatively, accepts a string and an array
	 *		as its input argument, in which the string and
	 *		array members are used as a prepared SQL
	 *		statement (c.f. "pg_query_params()" in PHP)
	 */
JS_METHOD(_query) {
	PGSQL_PTR_CON;
	ASSERT_CONNECTED;
	uint32_t len = args.Length();
	if (len < 1) {
		JS_TYPE_ERROR("No query specified");
		return;
	}
	if (len > 1) {
		v8::Local<v8::Value> * fargs = new v8::Local<v8::Value>[len];
		for (uint32_t i=0;i<len;i++) {
			fargs[i] = args[i];
		}
		v8::Local<v8::Value> f = args.This()->Get(JS_CONTEXT,JS_STR("queryParams")).ToLocalChecked();
		v8::Local<v8::Function> queryParams = v8::Local<v8::Function>::Cast(f);
		v8::Local<v8::Value> ret = queryParams->Call(JS_CONTEXT,args.This(), len, fargs).ToLocalChecked();
		delete[] fargs;
		args.GetReturnValue().Set(ret);
	}
	v8::String::Utf8Value q(JS_ISOLATE,args[0]);
	PGresult *res;
	res = PQexec(conn, *q);
	
	int code = -1;
	if (!(!res)) {
		code = PQresultStatus(res);
	}
	if (code != PGRES_COMMAND_OK && code != PGRES_TUPLES_OK) {
		char errTmp[10];
		sprintf(errTmp, "%d", code);
		std::string error = "[js_pgsql.cc @ _query] ERROR: ";
		error += PGSQL_ERROR;
		error += " (";
		error += errTmp;
		error += ")";
		PQclear(res);
		JS_ERROR(error.c_str());
		return;
	}
	int qc = args.This()->Get(JS_CONTEXT,JS_STR("queryCount")).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
	(void)args.This()->Set(JS_CONTEXT,JS_STR("queryCount"), JS_INT(qc+1));
	if (res) {
		v8::Local<v8::Value> resargs[] = { v8::External::New(JS_ISOLATE, (void *) res) };
				v8::Local<v8::FunctionTemplate> rslt = v8::Local<v8::FunctionTemplate>::New(JS_ISOLATE, _rslt);
		args.GetReturnValue().Set(rslt->GetFunction(JS_CONTEXT).ToLocalChecked()->NewInstance(JS_CONTEXT,1, resargs).ToLocalChecked());
				return;
	} else {
		 if (PQntuples(res)) {
			// PQclear(res);
			std::string ex = PGSQL_ERROR;
			if (conn) {
				PQfinish(conn);
				SAVE_PTR(0, NULL);
			}
			JS_ERROR(ex.c_str());
			return;
		} else {
			args.GetReturnValue().Set(args.This());
		}
	}
}

	/**
	 *	QUERYPARAMS method
	 *	- accepts a string as its input argument,
	 *		and returns an instance of the Result class
	 *	- alternatively, accepts a string and an object
	 *		as its input argument, in which the string and
	 *		object members are used as a prepared SQL
	 *		statement (c.f. "pg_query_params()" in PHP)
	 */
	JS_METHOD(_queryparams) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		if (args.Length() < 2) {
			JS_TYPE_ERROR("Too few input parameters");
			return;
		}
		if (args.Length() > 2) {
			JS_TYPE_ERROR("Too many input parameters");
			return;
		}
		PGresult *res;
		v8::String::Utf8Value q(JS_ISOLATE,args[0]);
		v8::Local<v8::Array> tarray = v8::Local<v8::Array>::Cast(args[1]->ToObject(JS_CONTEXT).ToLocalChecked());
		//v8::Local<v8::Object> parray = args[1]->ToObject(JS_CONTEXT).ToLocalChecked();
		int nparams = tarray->Length();
		char ** params = (char **)malloc(nparams * sizeof(char*));
		for(int i = 0; i < nparams; i++) {
			v8::Local<v8::Value> val=tarray->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked();
			if (val->IsUndefined()) {
				params[i]=NULL;
			} else if (val->IsNull()) {
				params[i]=NULL;
			} else {
				v8::String::Utf8Value tval(JS_ISOLATE,val->ToString(JS_CONTEXT).ToLocalChecked());
				if (tval.length()) params[i] = strdup(*tval); else params[i]=NULL;
			}
		}
		res = PQexecParams(conn, *q, nparams, NULL, params, NULL, NULL, 0);
 
		for(int i = 0; i < nparams; i++) {
			if (params[i]) free(params[i]);
		}
		free(params);

		int code = -1;
		if (!(!res)) {
			code = PQresultStatus(res);
		}
		if (code != PGRES_COMMAND_OK && code != PGRES_TUPLES_OK) {
			std::string str(PQresultErrorMessage(res));
			if (code > -1)
				PQclear(res);
			std::string ex = PGSQL_ERROR;
			if (conn) {
				PQfinish(conn);
				SAVE_PTR(0, NULL);
			}
			JS_ERROR(ex.c_str());
			return;
		}
		int qc = args.This()->Get(JS_CONTEXT,JS_STR("queryCount")).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
		(void)args.This()->Set(JS_CONTEXT,JS_STR("queryCount"), JS_INT(qc+1));
		if (res) {
			v8::Local<v8::Value> resargs[] = { v8::External::New(JS_ISOLATE, (void *) res) };
			v8::Local<v8::FunctionTemplate> rslt = v8::Local<v8::FunctionTemplate>::New(JS_ISOLATE, _rslt);
			args.GetReturnValue().Set(rslt->GetFunction(JS_CONTEXT).ToLocalChecked()->NewInstance(JS_CONTEXT,1, resargs).ToLocalChecked());
			return;
		} else {
			if (PQntuples(res)) {
				std::string ex = PGSQL_ERROR;
				if (conn) {
					PQfinish(conn);
					SAVE_PTR(0, NULL);
				}
				JS_ERROR(ex.c_str());
				return;
			} else {
				args.GetReturnValue().Set(args.This());
			}
		}
	}

	/**
	 *	SENDQUERY method
	 *	- asynchronous
	 *	- accepts a string as its input argument,
	 *		which is sent to the SQL server for
	 *		processing as a SQL statement
	 */
	JS_METHOD(_sendquery) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		v8::String::Utf8Value q(JS_ISOLATE,args[0]);
		int sock = PQsocket(conn);
		if (sock < 0) {
			JS_ERROR("SOCKET ERROR");
			return;
		}
		fd_set write_mask;
		FD_ZERO(&write_mask);
		FD_SET(sock, &write_mask);
		int code = 0;
		int flush = PQflush(conn);
		while (code < 1) {
			if (flush < 0) {
				JS_ERROR("[js_pgsql.cc @ _sendquery()] ERROR: PQflush() returned an error code");
				return;
			} else {
				while (flush > 0)
					if (select(sock + 1, NULL, &write_mask, NULL, NULL) == -1) {
						JS_ERROR("SOCKET ERROR");
						return;
					} else {
						flush = PQflush(conn);
					}
			}
			if (flush < 0) {
				JS_ERROR("[js_pgsql.cc @ _sendquery()] ERROR: PQflush() returned an error code");
				return;
			}
			code = PQsendQuery(conn, *q);
			flush = PQflush(conn);
			if (code < 0) {
				std::stringstream msg("");
				msg << "[js_pgsql.cc @ _sendquery()] ERROR: (";
				msg << code;
				msg << ") ";
				msg << PGSQL_ERROR;
				std::string err(msg.str());
				JS_ERROR(err.c_str());
				return;
			}
		}
		int qc = args.This()->Get(JS_CONTEXT,JS_STR("queryCount")).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
		(void)args.This()->Set(JS_CONTEXT,JS_STR("queryCount"), JS_INT(qc+1));
		args.GetReturnValue().Set(code);
	}

	/**
	 *	SENDQUERYPARAMS method
	 *	- asynchronous
	 *	- accepts a string as its input argument,
	 *		which is sent to the SQL server for
	 *		processing as a SQL statement
	 */
	JS_METHOD(_sendqueryparams) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		if (args.Length() < 1) {
			JS_TYPE_ERROR("[js_pgsql.cc @ _sendqueryparams()] ERROR: No query specified");
			return;
		}
		if (args.Length() > 1) {
			JS_TYPE_ERROR("[js_pgsql.cc @ _sendqueryparams()] ERROR: Too many input parameters");
			return;
		}
		v8::String::Utf8Value q(JS_ISOLATE,args[0]);
		v8::Local<v8::Array> tarray = v8::Local<v8::Array>::Cast(args[1]->ToObject(JS_CONTEXT).ToLocalChecked());
		//v8::Local<v8::Object> parray = args[1]->ToObject(JS_CONTEXT).ToLocalChecked();
		int sock = PQsocket(conn);
		if (sock < 0) {
			JS_ERROR("SOCKET ERROR");
			return;
		}
		int nparams = tarray->Length();
		char ** params = (char **)malloc(nparams);
		//size_t n = 0;
		for(int i = 0; i < nparams; i++) {
			//n = tarray->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked()->Utf8Length(JS_ISOLATE);
			v8::String::Utf8Value tval(JS_ISOLATE,tarray->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked());
			params[i] = strdup(*tval);
		}
		fd_set write_mask;
		FD_ZERO(&write_mask);
		FD_SET(sock, &write_mask);
		int code = 0;
		int flush = PQflush(conn);
		bool has_error = false;
		while (code < 1) {
			if (flush < 0) {
				JS_ERROR("[js_pgsql.cc @ _sendqueryparams()] ERROR: PQflush() returned an error code");
				has_error = true;
				break;
			} else {
				while (flush > 0) {
					if (select(sock + 1, NULL, &write_mask, NULL, NULL) == -1) {
						JS_ERROR("SOCKET ERROR");
						has_error = true;
						break;
					} else {
						flush = PQflush(conn);
					}
				}
			}
			if (flush < 0) {
				JS_ERROR("[js_pgsql.cc @ _sendqueryparams()] ERROR: PQflush() returned an error code");
				has_error = true;
				break;
			}
			code = PQsendQueryParams(conn, *q, nparams, NULL, params, NULL, NULL, 0);
			flush = PQflush(conn);
			if (code < 0) {
				std::stringstream msg("");
				msg << "[js_pgsql.cc @ _sendqueryparams()] ERROR: (";
				msg << code;
				msg << ") ";
				msg << PGSQL_ERROR;
				std::string err(msg.str());
				JS_ERROR(err.c_str());
				has_error = true;
				break;
			}
		}
		
		if (!has_error) {
			int qc = args.This()->Get(JS_CONTEXT,JS_STR("queryCount")).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
			(void)args.This()->Set(JS_CONTEXT,JS_STR("queryCount"), JS_INT(qc+1));
			args.GetReturnValue().Set(code);
		}
		
		for(int i = 0; i < nparams; i++) {
			if (params[i]) free(params[i]);
		}
		free(params);
	}
	
	JS_METHOD(_isconnected) {
		PGSQL_PTR_CON;
 //	 ASSERT_CONNECTED;
		args.GetReturnValue().Set(JS_BOOL(conn));
	}

	/**
	 *	ISBUSY method
	 *	- asynchronous
	 */
	JS_METHOD(_isbusy) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		args.GetReturnValue().Set(PQisBusy(conn));
	}

	/**
	 *	SOCKET method
	 *	- returns an integer value corresponding
	 *		to the underlying SQL socket connection
	 */
	JS_METHOD(_socket) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		args.GetReturnValue().Set(PQsocket(conn));
	}

	/**
	 *	CANCEL method
	 *	- attempts to cancel an outstanding SQL command
	 *		on a non-blocking (asynchronous) connection
	 */
	JS_METHOD(_cancel) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		char ebuf[4096];
		PGcancel * pg_cancel = PQgetCancel(conn);
	int val=PQcancel(pg_cancel,ebuf,sizeof(ebuf));
		v8::Local<v8::Value> ret = JS_INT(val);
		PQfreeCancel(pg_cancel);
		if (val < 0) {
			JS_ERROR("ERROR: Cancel failed");
			return;
		}
		args.GetReturnValue().Set(ret);
	}

	JS_METHOD(_escape) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		if (args.Length() < 1) {
			JS_TYPE_ERROR("Nothing to escape");
			return;
		}
		v8::String::Utf8Value str(JS_ISOLATE,args[0]);
		int len = args[0]->ToString(JS_CONTEXT).ToLocalChecked()->Utf8Length(JS_ISOLATE);
		char * result = new char[2*len + 1];
		int length = (int)PQescapeStringConn(conn, result, *str, len, NULL);
		v8::Local<v8::Value> output = JS_STR_LEN(result, length);
		delete[] result;
		args.GetReturnValue().Set(output);
	}

	JS_METHOD(_escapebytea) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		if (args.Length() < 1) {
			JS_TYPE_ERROR("Nothing to escape");
			return;
		}
		v8::String::Utf8Value str(JS_ISOLATE,args[0]);
		int len = args[0]->ToString(JS_CONTEXT).ToLocalChecked()->Utf8Length(JS_ISOLATE);
		size_t * length = NULL;
		char * result = (char *)PQescapeByteaConn(conn, (const unsigned char *)*str, len, length);
		int length_copy=(int)(*length);
		v8::Local<v8::Value> output = JS_STR_LEN(result, length_copy);
		delete[] result;
		args.GetReturnValue().Set(output);
	}

	JS_METHOD(_unescapebytea) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		if (args.Length() < 1) {
			JS_TYPE_ERROR("Nothing to escape");
			return;
		}
		v8::String::Utf8Value str(JS_ISOLATE,args[0]);
		size_t * length = NULL;
		char * result = (char *)PQunescapeBytea((const unsigned char *)*str, length);
		int length_copy=(int)(*length);
		v8::Local<v8::Value> output = JS_STR_LEN(result, length_copy);
		delete[] result;
		args.GetReturnValue().Set(output);
	}

	JS_METHOD(_numrows) {
		// PGresult * res = LOAD_PTR(0, PGresult *);
		PGSQL_RES_LOAD(res);
		ASSERT_RESULT;
		args.GetReturnValue().Set(PQntuples(res));
	}

	JS_METHOD(_numfields) {
		PGSQL_RES_LOAD(res);
		ASSERT_RESULT;
		args.GetReturnValue().Set(PQnfields(res));
	}

	JS_METHOD(_numaffectedrows) {
		PGSQL_RES_LOAD(res);
		ASSERT_RESULT;
		args.GetReturnValue().Set(atoi(PQcmdTuples(res)));
	}

	JS_METHOD(_fetchnames) {
		PGSQL_RES_LOAD(res);
		ASSERT_RESULT;
		int cnt = PQnfields(res);
		v8::Local<v8::Array> result = v8::Array::New(JS_ISOLATE, cnt);
		for(int i = 0; i < cnt; i++)
			(void)result->Set(JS_CONTEXT,JS_INT(i), JS_STR(PQfname(res, i)));
		args.GetReturnValue().Set(result);
	}

	/**
	 * Return indicated result value
	 */
	JS_METHOD(_fetchresult) {
		PGSQL_RES_LOAD(res);
		ASSERT_RESULT;
		int x = PQnfields(res);
		int y = PQntuples(res);
		v8::String::Utf8Value a(JS_ISOLATE,args[0]);
		v8::String::Utf8Value b(JS_ISOLATE,args[1]);
		int i = atoi(*a);
		int j = atoi(*b);
		if (i > x || i < 0) {
			JS_TYPE_ERROR("Row number out of bounds");
			return;
		}
		if (j > y || j < 0) {
			JS_TYPE_ERROR("Column number out of bounds");
			return;
		}
		args.GetReturnValue().Set(JS_STR(PQgetvalue(res,i,j)));
	}

	/**
	 * Return indicated result value
	 */
	JS_METHOD(_fetchfield) {
		PGSQL_RES_LOAD(res);
		ASSERT_RESULT;
		int x = PQnfields(res);
		int y = PQntuples(res);
		v8::String::Utf8Value a(JS_ISOLATE,args[0]);
		v8::String::Utf8Value b(JS_ISOLATE,args[1]);
		int i = atoi(*a);
		int j = PQfnumber(res,*b);
		if (i > x || i < 0) {
			JS_TYPE_ERROR("Row number out of bounds");
			return;
		}
		if (j > y || j < 0) {
			JS_TYPE_ERROR("Column name does not exist");
			return;
		}
		args.GetReturnValue().Set(JS_STR(PQgetvalue(res,i,j)));
	}

	/**
	 * Return all rows of result data as a two-dimensional JS array
	 */
	JS_METHOD(_fetchall) {
		PGSQL_RES_LOAD(res);
		ASSERT_RESULT;
/*		int x = PQnfields(res);
		int y = PQntuples(res);
		v8::Local<v8::Array> result = v8::Array::New(y);
		for (int i = 0; i < y; i++) {
			v8::Local<v8::Array> item = v8::Array::New(x);
			result->Set(JS_INT(i), item);
			for (int j=0; j<x; j++)
	if (PQgetisnull(res, i, j))
		item->Set(JS_INT(j), v8::Null());
	else
		item->Set(JS_INT(j), JS_STR(PQgetvalue(res, i, j)));
		}
		return result;*/
		args.GetReturnValue().Set(fetch_any(res,-1,false));
	}

	/**
	 * Return one row of result data as a JS array
	 */
	JS_METHOD(_fetchrow) {
		PGSQL_RES_LOAD(res);
		ASSERT_RESULT;
	int i=-2;
		if (args.Length() > 0) {
		v8::String::Utf8Value a(JS_ISOLATE,args[0]);
		i = atoi(*a);
		}
	v8::Local<v8::Array> item = fetch_any(res,i,false);
		args.GetReturnValue().Set(item->Get(JS_CONTEXT,JS_INT(0)).ToLocalChecked());
	}

	/**
	 * Return all rows of result data as a two-dimensional JS hash object,
	 * indexed by column name
	 */ 
	JS_METHOD(_fetchallobjects) {
		PGSQL_RES_LOAD(res);
		ASSERT_RESULT;
		args.GetReturnValue().Set(fetch_any(res,-1,true));
	}

	/**
	 * Return one row of result data as a JS hash object,
	 * indexed by column name
	 */ 
	JS_METHOD(_fetchrowobject) {
		PGSQL_RES_LOAD(res);
		ASSERT_RESULT;
	int i = -2;
	if (args.Length() > 0) {
		v8::String::Utf8Value a(JS_ISOLATE,args[0]);
		i = atoi(*a);
	}
	v8::Local<v8::Array> item = fetch_any(res,i,true);
		args.GetReturnValue().Set(item->Get(JS_CONTEXT,JS_INT(0)).ToLocalChecked());
	}

	JS_METHOD(_reset) {
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	PGSQL_RES_SETPOS(0);
		args.GetReturnValue().Set(JS_BOOL(true));
	}

JS_METHOD(_execute) {
	PGSQL_PTR_CON;
	ASSERT_CONNECTED;
	v8::String::Utf8Value n(JS_ISOLATE,args[0]);
	v8::Local<v8::Array> tmp ( v8::Local<v8::Array>::Cast(args[1]) );
	uint32_t len = tmp->Length();
	char ** q = (char **)malloc(len);
	for(uint32_t i = 0; i < len; i++) {
		//uint32_t n = tmp->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked()->Utf8Length(JS_ISOLATE);
		v8::String::Utf8Value tval(JS_ISOLATE,tmp->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked());
		q[i] = strdup(*tval);
	}
	PGresult * res = PQexecPrepared(conn, *n, len, (const char* const*)q, NULL, NULL, 0);
				
	for(uint32_t i = 0; i < len; i++) {
		if (q[i]) free(q[i]);
	}
	free(q);
	
	int code = -1;
	if (!(!res)) { code = PQresultStatus(res); }
	if (code != PGRES_COMMAND_OK && code != PGRES_TUPLES_OK) {
		std::stringstream msg;
		msg << "[js_pgsql.cc @ _execute()] ERROR: (";
		msg << code;
		msg << ") ";
		msg << PGSQL_ERROR;
		std::string err(msg.str());
		PQclear(res);
		JS_ERROR(err.c_str());
		return;
	}
	if (!res) {
		std::stringstream msg;
		msg << "[js_pgsql.cc @ _execute()] ERROR: null result";
		std::string err(msg.str());
		JS_ERROR(err.c_str());
		return;
	} else {
		v8::Local<v8::Value> resargs[] = { v8::External::New(JS_ISOLATE, (void *) res) };
				v8::Local<v8::FunctionTemplate> rslt = v8::Local<v8::FunctionTemplate>::New(JS_ISOLATE, _rslt);
				args.GetReturnValue().Set(rslt->GetFunction(JS_CONTEXT).ToLocalChecked()->NewInstance(JS_CONTEXT,1, resargs).ToLocalChecked());
	}
}

	JS_METHOD(_sendexecute) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		v8::String::Utf8Value n(JS_ISOLATE,args[0]);
		v8::Local<v8::Array> tmp ( v8::Local<v8::Array>::Cast(args[1]) );
		int sock = PQsocket(conn);
		if (sock < 0) {
			JS_ERROR("SOCKET ERROR");
			return;
		}
		uint32_t len = tmp->Length();
		char ** q = (char **)malloc(len);
		for(uint32_t i = 0; i < len; i++) {
			//uint32_t n = tmp->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked()->Utf8Length(JS_ISOLATE);
			v8::String::Utf8Value tval(JS_ISOLATE,tmp->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked());
			q[i] = strdup(*tval);
		}		
		fd_set write_mask;
		FD_ZERO(&write_mask);
		FD_SET(sock, &write_mask);
		int code = 0;
		int flush = PQflush(conn);
		bool has_error = false;
		while (code < 1) {
			if (flush < 0) {
				JS_ERROR("[js_pgsql.cc @ _sendexecute()] ERROR: PQflush() returned an error code");
				has_error = true;
				break;
			} else {
				while (flush > 0) {
					if (select(sock + 1, NULL, &write_mask, NULL, NULL) == -1) {
						JS_ERROR("SOCKET ERROR");
						has_error = true;
						break;
					} else {
						flush = PQflush(conn);
					}
				}
			}
			if (flush < 0) {
				JS_ERROR("[js_pgsql.cc @ _sendexecute()] ERROR: PQflush() returned an error code");
				has_error = true;
				break;
			}
			code = PQsendQueryPrepared(conn, *n, len, (const char* const*)q, NULL, NULL, 0);
			flush = PQflush(conn);
			if (code < 0) {
				std::stringstream msg("");
				msg << "[js_pgsql.cc @ _sendexecute()] ERROR: (";
				msg << code;
				msg << ") ";
				msg << PGSQL_ERROR;
				std::string err(msg.str());
				JS_ERROR(err.c_str());
				has_error = true;
				break;
			}
		}
		
		if (!has_error) {
			int qc = args.This()->Get(JS_CONTEXT,JS_STR("queryCount")).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
			(void)args.This()->Set(JS_CONTEXT,JS_STR("queryCount"), JS_INT(qc+1));
			args.GetReturnValue().Set(JS_BOOL(code));
		}
		
		for(uint32_t i = 0; i < len; i++) {
			if (q[i]) free(q[i]);
		}
		free(q);
		
	}

	JS_METHOD(_prepare) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		v8::String::Utf8Value n(JS_ISOLATE,args[0]);
		v8::String::Utf8Value q(JS_ISOLATE,args[1]);
		PGresult * res = PQprepare(conn, *n, *q, 0, NULL);
		int code = -1;
		if (!(!res))
			code = PQresultStatus(res);
		if (code != PGRES_COMMAND_OK && code != PGRES_TUPLES_OK) {
			std::stringstream msg;
			msg << "[js_pgsql.cc @ _prepare()] ERROR: (";
			msg << code;
			msg << ") ";
			msg << PGSQL_ERROR;
			std::string err(msg.str());
			PQclear(res);
			JS_ERROR(err.c_str());
			return;
		}
		if (!res) {
			std::stringstream msg;
			msg << "[js_pgsql.cc @ _prepare()] ERROR: null result";
			std::string err(msg.str());
			JS_ERROR(err.c_str());
			return;
		} else {
			v8::Local<v8::Value> resargs[] = { v8::External::New(JS_ISOLATE, (void *) res) };
			v8::Local<v8::FunctionTemplate> rslt = v8::Local<v8::FunctionTemplate>::New(JS_ISOLATE, _rslt);
			args.GetReturnValue().Set(rslt->GetFunction(JS_CONTEXT).ToLocalChecked()->NewInstance(JS_CONTEXT,1, resargs).ToLocalChecked());
		}
	}

	JS_METHOD(_deallocate) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		v8::String::Utf8Value n(JS_ISOLATE,args[0]);
		std::stringstream q("DEALLOCATE ");
		q << *n;
		PGresult * res = PQexec(conn, q.str().c_str());
		int code = -1;
		if (!(!res))
			code = PQresultStatus(res);
		if (code != PGRES_COMMAND_OK && code != PGRES_TUPLES_OK) {
			std::stringstream msg;
			msg << "[js_pgsql.cc @ _deallocate()] ERROR: (";
			msg << code;
			msg << ") ";
			msg << PGSQL_ERROR;
			std::string err(msg.str());
			PQclear(res);
			JS_ERROR(err.c_str());
			return;
		}
		if (!res) {
			std::stringstream err("[js_pgsql.cc @ _deallocate()] ERROR: null result");
			JS_ERROR(err.str().c_str());
			return;
		} else {
			v8::Local<v8::Value> resargs[] = { v8::External::New(JS_ISOLATE, (void *) res) };
			v8::Local<v8::FunctionTemplate> rslt = v8::Local<v8::FunctionTemplate>::New(JS_ISOLATE, _rslt);
			args.GetReturnValue().Set(rslt->GetFunction(JS_CONTEXT).ToLocalChecked()->NewInstance(JS_CONTEXT,1, resargs).ToLocalChecked());
		}
	}

	JS_METHOD(_sendprepare) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		v8::String::Utf8Value n(JS_ISOLATE,args[0]);
		v8::String::Utf8Value q(JS_ISOLATE,args[1]);
		int sock = PQsocket(conn);
		if (sock < 0) {
			JS_ERROR("SOCKET ERROR");
			return;
		}
		fd_set write_mask;
		FD_ZERO(&write_mask);
		FD_SET(sock, &write_mask);
		int code = 0;
		int flush = PQflush(conn);
		while (code < 1) {
			if (flush < 0) {
				JS_ERROR("[js_pgsql.cc @ _sendprepare()] ERROR: PQflush() returned an error code");
				return;
			} else {
				while (flush > 0)
					if (select(sock + 1, NULL, &write_mask, NULL, NULL) == -1) {
						JS_ERROR("SOCKET ERROR");
						return;
					} else {
						flush = PQflush(conn);
					}
			}
			if (flush < 0) {
				JS_ERROR("[js_pgsql.cc @ _sendprepare()] ERROR: PQflush() returned an error code");
				return;
			}
			code = PQsendPrepare(conn, *n, *q, 0, NULL);
			flush = PQflush(conn);
			if (code < 0) {
				std::stringstream msg("");
				msg << "[js_pgsql.cc @ _sendprepare()] ERROR: (";
				msg << code;
				msg << ") ";
				msg << PGSQL_ERROR;
				std::string err(msg.str());
				JS_ERROR(err.c_str());
				return;
			}
		}
		int qc = args.This()->Get(JS_CONTEXT,JS_STR("queryCount")).ToLocalChecked()->Int32Value(JS_CONTEXT).ToChecked();
		(void)args.This()->Set(JS_CONTEXT,JS_STR("queryCount"), JS_INT(qc+1));
		args.GetReturnValue().Set(code);
	}

	JS_METHOD(_senddeallocate) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		v8::String::Utf8Value n(JS_ISOLATE,args[0]);
		std::stringstream q("DEALLOCATE ");
		int ret = PQsendQuery(conn, q.str().c_str());
		if (!ret) {
			if (PQstatus(conn) != CONNECTION_OK)
				PQreset(conn);
			ret = PQsendQuery(conn, q.str().c_str());
			if (!ret) {
				JS_ERROR("[js_pgsql.cc @ _senddeallocate()] ERROR: PQsendQuery failed");
			}
		}
		args.GetReturnValue().Set(ret);
	}

	// * * *
	// * * *	Miscellaneous
	// * * *

	void get_client_encoding_id(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value> &args) {
		v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		args.GetReturnValue().Set(PQclientEncoding(conn));
	}

	void set_client_encoding_id(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &args) {
		v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
		PGSQL_PTR_CON;
		if (!conn) {
			JS_ERROR("[js_pgsql.cc @ set_client_encoding()] ERROR: null connection");
		} else {
			PQsetClientEncoding(conn,*v8::String::Utf8Value(JS_ISOLATE,value->ToString(JS_CONTEXT).ToLocalChecked()));
		}
	}

	void get_client_encoding(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value> &args) {
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		args.GetReturnValue().Set(JS_STR(pg_encoding_to_char(PQclientEncoding(conn))));
	}

	void set_error_verbosity(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &args) {
		v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
		PGSQL_PTR_CON;
		if (!conn) {
			JS_ERROR("[js_pgsql.cc @ set_error_verbosity()] ERROR: null connection");
			return;
		}
		PGVerbosity v = PQERRORS_DEFAULT;
		std::stringstream inv("");
		inv << *v8::String::Utf8Value(JS_ISOLATE,value->ToString(JS_CONTEXT).ToLocalChecked());
		if (inv.str().compare("terse") || inv.str().compare("TERSE")) { v = PQERRORS_TERSE; }
		else if (inv.str().compare("default") || inv.str().compare("DEFAULT")) { v = PQERRORS_DEFAULT; }
		else if (inv.str().compare("verbose") || inv.str().compare("VERBOSE")) { v = PQERRORS_VERBOSE; }
		PQsetErrorVerbosity(conn,v);
	}

	// * * *
	// * * *	types / methods for async SQL queries
	// * * *

	struct pt_arg_t {
		PGconn * conn;
		PGresult * res;
		v8::Local<v8::Value> callback;
		v8::Local<v8::Value> ret;
	};

	void * async_cb_routine(void * inv) {
		v8::Locker tlock(JS_ISOLATE);
		v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
		v8::Context::Scope context_scope(v8::Context::New(JS_ISOLATE));
		//tlock.StartPreemption(100);
		v8::TryCatch try_catch(JS_ISOLATE);
		v8::Local<v8::Object> global = JS_ISOLATE->GetCurrentContext()->Global();
		pt_arg_t * args = (pt_arg_t *)inv;
		PGconn * conn = args->conn;
		PGresult * res = args->res;
		//v8::Local<v8::Value> callback( args->callback );
		int sock = PQsocket(conn);
		if (sock < 0) {
			v8::TryCatch trycatch(JS_ISOLATE);
			args->ret = JS_ERROR("SOCKET ERROR");
		}
		fd_set input_mask;
		FD_ZERO(&input_mask);
		FD_SET(sock, &input_mask);
		while (!!res) {
			if ( select(sock + 1, &input_mask, NULL, NULL, NULL) ) {
				if (PQisBusy(conn))
					PQconsumeInput(conn);
				res = PQgetResult(conn);
			}
		}
		if (PQresultStatus(res)==PGRES_TUPLES_OK) {
			v8::Local<v8::Value> fargs[0];
			v8::Local<v8::Value> cbresult;
			if (args->callback->IsFunction()) {
				v8::TryCatch try_catch(JS_ISOLATE);
				cbresult = v8::Local<v8::Function>::Cast(args->callback)->Call(JS_CONTEXT,global, sizeof(fargs), fargs).ToLocalChecked();
			} else if (args->callback->IsString()) {
				v8::TryCatch try_catch(JS_ISOLATE);
				v8::Local<v8::Value> func = global->Get(JS_CONTEXT, args->callback ).ToLocalChecked();
				cbresult = v8::Local<v8::Function>::Cast(func)->Call(JS_CONTEXT,global, sizeof(fargs), fargs).ToLocalChecked();
			}
			args->ret = cbresult;
		} else {
			args->ret = JS_ERROR(std::string("[js_pgsql.cc @ _asyncquery()] ERROR: " + std::string(PGSQL_ERROR)).c_str());
		}
		return NULL;
	}

	void * reaper(void * inv) {
		pthread_t * target = (pthread_t *)inv;
		sleep(8);
		pthread_cancel(*target);
		return NULL;
	}

	JS_METHOD(_asyncquery) {
		v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
		v8::TryCatch try_catch(JS_ISOLATE);
		PGSQL_PTR_CON;
		ASSERT_CONNECTED;
		int code = -1;
		pthread_t cb_thread_main;
		pthread_t * cb_thread = &cb_thread_main;
		int pquery = 0;
		int32_t prepared = 0;
		prepared = args[0]->Int32Value(JS_CONTEXT).ToChecked();
		char ** q = (char **)malloc(2);
		v8::Local<v8::Value> callback;
		v8::Local<v8::Array> p;
		v8::Local<v8::Array> cbargs;
		if (args.Length() > 2) {
			if (args[1]->IsString()) {
				std::stringstream ttmp("");
				ttmp << *v8::String::Utf8Value(JS_ISOLATE, args[1]->ToString(JS_CONTEXT).ToLocalChecked() );
				*q = strdup(ttmp.str().c_str());
			}
			if (args[2]->IsArray()) {
				pquery = 1;
				p = v8::Local<v8::Array>::Cast( args[2] );
				if (args.Length() > 3) {
					callback = args[3];
					if (args.Length() > 4) 
						if (args[4]->IsArray())
							cbargs = v8::Local<v8::Array>::Cast(args[4]);
				}
			} else {
				callback =	args[2] ;
				if (args.Length()>3) if (args[3]->IsArray())
					cbargs = v8::Local<v8::Array>::Cast(args[3]);
			}
		} else if (args.Length()==2 && args[1]->IsObject()) {
			std::stringstream ttmp("");
			v8::Local<v8::Object> inv( args[1]->ToObject(JS_CONTEXT).ToLocalChecked() );
			if (inv->Has(JS_CONTEXT,JS_STR("query")).ToChecked()) {
				ttmp.flush();
				ttmp << *(v8::String::Utf8Value(JS_ISOLATE,inv->Get(JS_CONTEXT,JS_STR("query")).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked()));
				*q = strdup(ttmp.str().c_str());
			}
			if (inv->Has(JS_CONTEXT,JS_STR("queryParams")).ToChecked()) {
				pquery = 1;
				p = v8::Local<v8::Array>::Cast(inv->Get(JS_CONTEXT,JS_STR("queryParams")).ToLocalChecked());
			}
			if (inv->Has(JS_CONTEXT,JS_STR("callback")).ToChecked()) {
				ttmp.flush();
				ttmp << *(v8::String::Utf8Value(JS_ISOLATE,inv->Get(JS_CONTEXT,JS_STR("callback")).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked()));
				callback = JS_STR(ttmp.str().c_str());
			}
			if (inv->Has(JS_CONTEXT,JS_STR("callbackParams")).ToChecked())
				cbargs = v8::Local<v8::Array>::Cast(inv->Get(JS_CONTEXT,JS_STR("callbackParams")).ToLocalChecked());
		}
		v8::Local<v8::Value> ret;
		if (pquery==1) {
			int nparams = p->Length();
			char ** params = (char **)malloc(nparams);
			//size_t n = 0;
			for(int i = 0; i < nparams; i++) {
				//n = p->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT)->Utf8Length(JS_ISOLATE);
				v8::String::Utf8Value tval(JS_ISOLATE,p->Get(JS_CONTEXT,JS_INT(i)).ToLocalChecked()->ToString(JS_CONTEXT).ToLocalChecked());
				params[i] = strdup(*tval);
			}
			if (prepared==1)
				code = PQsendQueryPrepared(conn, *q, nparams, (const char* const*)params, NULL, NULL, 0);
			else
				code = PQsendQueryParams(conn, *q, nparams, NULL, params, NULL, NULL, 0);
			
			for (int i = 0; i < nparams; i++)
				if (params[i]) free(params[i]);
			free(params);
		} else {
			if (prepared==1)
				code = PQsendQueryPrepared(conn, *q, 0, NULL, NULL, NULL, 0);
			else
				code = PQsendQuery(conn, *q);
		}
		
		if (*q) free(*q);
		free(q);		
		
		if (code==1) {

	// v8::Locker locker;

	//	Create a worker thread to monitor the PGSQL socket
	//	for a response and correspondingly execute the JS
	//	callback method	upon completion of the async SQL query

	pt_arg_t cb_thread_args_main;
	pt_arg_t * cb_thread_args = &cb_thread_args_main;
	int status_main = 0;
	int * status = &status_main;
	cb_thread_args->conn = conn;
	cb_thread_args->res = NULL;
	cb_thread_args->callback = callback;
	cb_thread_args->ret = ret;
//	cb_thread_args->env = env;
//	cb_thread_args->func = rslt->GetFunction();
//	cb_thread_args->resobj = rslt->GetFunction()->NewInstance(1, &ret);
//	cb_thread_args->resobj = rslt->GetFunction()->NewInstance(0, &ret);
	pthread_t tmon_main;
	pthread_t * tmon = &tmon_main;
	// v8::TryCatch try_catch;
	{
		v8::Unlocker unlock(JS_ISOLATE);
		int pt_id = pthread_create(
			(pthread_t *)cb_thread,
			(const pthread_attr_t *)NULL,
			(void *(*)(void *))async_cb_routine,
			(void *)cb_thread_args
			);
		if (pt_id < 0) {
			JS_ERROR("[js_pgsql.cc @ _asyncquery()] ERROR 1: \"pthread_create()\" returned an error code");
			return;
		}
		pt_id = pthread_create(
			(pthread_t *)tmon,
			(const pthread_attr_t *)NULL,
			(void *(*)(void *))reaper,
			(void *)cb_thread
			);
		if (pt_id < 0) {
			JS_ERROR("[js_pgsql.cc @ _asyncquery()] ERROR 2: \"pthread_create()\" returned an error code");
			return;
		}
		int pt_code=pthread_join(cb_thread_main, (void **) (&status) );
		if (pt_code < 0) {
			JS_ERROR("[js_pgsql.cc @ _asyncquery()] ERROR 3: \"pthread_join()\" returned an error code");
			return;
		}
		unlock.~Unlocker();
	}
	// 	***	wait for thread		***
//	int pt_code=pthread_join(cb_thread_main, (void **) (&status) );
//	if (pt_code < 0)
//		return JS_ERROR("[js_pgsql.cc @ _asyncquery()] ERROR 2: \"pthread_create()\" returned an error code");
//	else
//		ret = cb_thread_args->ret;
		}
		args.GetReturnValue().Set(ret);
	}


	// ===============================================
	// ======	END			==========
	// ===============================================



	/***********************************************************
	 *
	 *	"handler" functions for virtual properties:
	 *
	 ***********************************************************/

	/*v8::Local<v8::String> v8_str(const char* x) {
		return v8::String::NewFromUtf8(JS_ISOLATE, x).ToLocalChecked();
	}*/

	void handle_busy(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value> &args) {
		v8::Local<v8::Value> fargs[] = { };
		v8::Local<v8::Value> f = args.This()->Get(JS_CONTEXT,JS_STR("isBusy")).ToLocalChecked();
		v8::Local<v8::Function> isBusy = v8::Local<v8::Function>::Cast(f);
		args.GetReturnValue().Set(isBusy->Call(JS_CONTEXT,args.This(), 0, fargs).ToLocalChecked());
	}

	void handle_connected(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value> &args) {
		int tmp = -1;
		{
			v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
			v8::Local<v8::Value> fargs[] = { };
			v8::Local<v8::Value> f = args.This()->Get(JS_CONTEXT,JS_STR("socket")).ToLocalChecked();
			v8::Local<v8::Function> socket = v8::Local<v8::Function>::Cast(f);
			v8::Local<v8::Value> ret = socket->Call(JS_CONTEXT,args.This(), sizeof(fargs), fargs).ToLocalChecked();
			tmp = ret->Int32Value(JS_CONTEXT).ToChecked();
		}
		if (tmp < 0)
			args.GetReturnValue().Set(false);
		else
			args.GetReturnValue().Set(true);
	}

	void handle_socket(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value> &args) {
		v8::Local<v8::Value> ret;
		{
			v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
			v8::Local<v8::Value> fargs[] = { };
			v8::Local<v8::Value> f = args.This()->Get(JS_CONTEXT,JS_STR("socket")).ToLocalChecked();
			v8::Local<v8::Function> socket = v8::Local<v8::Function>::Cast(f);
			ret = socket->Call(JS_CONTEXT,args.This(), 0, fargs).ToLocalChecked();
		}
		if (ret == JS_INT(-1))
			ret = JS_BOOL(false);
		args.GetReturnValue().Set(ret);
	}

	void get_nonblocking(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value> &args) {
		v8::Local<v8::Value> ret = JS_BOOL(false);
		{
			v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
			PGSQL_PTR_CON;
			ASSERT_CONNECTED;
			ret = JS_BOOL(PQisnonblocking(conn));
		}
		args.GetReturnValue().Set(ret);
	}

	void set_nonblocking(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &args) {
		v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
		PGSQL_PTR_CON;
		if (!conn)
			JS_ERROR("[js_pgsql.cc @ set_nonblocking()] ERROR: null connection");
		else
			PQsetnonblocking(conn,value->Int32Value(JS_CONTEXT).ToChecked());
	}

	void rslt_error(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value> &args) {
		std::string emsg("");
		{
			v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
			PGSQL_RES_LOAD(res);
			ASSERT_RESULT;
			emsg = PQresultErrorMessage(res);
			//scope.~HandleScope();
		}
		args.GetReturnValue().Set(JS_STR(emsg.c_str()));
	}

	void rslt_rows(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value> &args) {
		v8::Local<v8::Value> ret;
		{
			v8::Local<v8::Value> targs[] = {	};
			v8::Local<v8::Value> f = args.This()->Get(JS_CONTEXT,JS_STR("fetchAllObjects")).ToLocalChecked();
			v8::Local<v8::Function> fetchAllObjects = v8::Local<v8::Function>::Cast(f);
			ret = fetchAllObjects->Call(JS_CONTEXT,args.This(), 0, targs).ToLocalChecked();
		}
		args.GetReturnValue().Set(ret);
	}

	void rslt_max(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value> &args) {
		v8::Local<v8::Value> targs[] = {	};
		v8::Local<v8::Value> f = args.This()->Get(JS_CONTEXT,JS_STR("numRows")).ToLocalChecked();
		v8::Local<v8::Function> numrows = v8::Local<v8::Function>::Cast(f);
		v8::Local<v8::Value> tmp = numrows->Call(JS_CONTEXT,args.This(), 0, targs).ToLocalChecked();
		args.GetReturnValue().Set(tmp);
	}

	void rslt_pos(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value> &args) {
		int pos = PGSQL_RES_POS;
		args.GetReturnValue().Set(pos);
	}

	void rslt_next_row(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value> &args) {
		v8::Local<v8::Value> targs[] = { };
		v8::Local<v8::Value> f = args.This()->Get(JS_CONTEXT,JS_STR("numRows")).ToLocalChecked();
		v8::Local<v8::Function> numrows = v8::Local<v8::Function>::Cast(f);
		v8::Local<v8::Value> tmp = numrows->Call(JS_CONTEXT,args.This(), 0, targs).ToLocalChecked();
		int32_t max = tmp->Int32Value(JS_CONTEXT).ToChecked();
		int pos = PGSQL_RES_POS;
		if (pos < max) {
			v8::Local<v8::Value> fargs[] = { JS_INT(pos) };
			PGSQL_RES_SETPOS((pos + 1));
			f = args.This()->Get(JS_CONTEXT,JS_STR("fetchRowObject")).ToLocalChecked();
			v8::Local<v8::Function> fetchrow = v8::Local<v8::Function>::Cast(f);
			args.GetReturnValue().Set(fetchrow->Call(JS_CONTEXT,args.This(), 1, fargs).ToLocalChecked());
		} else {
			args.GetReturnValue().Set(false);
		}
	}
	
//} // end namespace

SHARED_INIT() {
	//fprintf(stderr,"pgsql.cc > SHARED_INIT						isolate=%ld, InContext()=%d, context=%ld\n",(void*)JS_ISOLATE,JS_ISOLATE->InContext(),(void*)(*JS_CONTEXT));
	v8::HandleScope handle_scope(JS_ISOLATE);//v8::LocalScope handle_scope(JS_ISOLATE);
	v8::Local<v8::FunctionTemplate> ft = v8::FunctionTemplate::New(JS_ISOLATE, _pgsql_constructor);
	ft->SetClassName(JS_STR("PostgreSQL"));

	v8::Local<v8::ObjectTemplate> ot = ft->InstanceTemplate();
	ot->SetInternalFieldCount(2); // connection, result

	// Set handler for virtual "busy" property:
	ot->SetAccessor(JS_STR("busy"),handle_busy);

	// Set handler for virtual "connected" property:
	ot->SetAccessor(JS_STR("connected"),handle_connected);

	// Set handler for virtual "socket" property:
	ot->SetAccessor(JS_STR("socket"),handle_socket);

	// Set handler for virtual "nonblocking" property:
	ot->SetAccessor(JS_STR("nonblocking"),get_nonblocking,set_nonblocking);

	// Set handler for virtual "clientEncodingId" property:
	ot->SetAccessor(JS_STR("clientEncodingId"),get_client_encoding_id,set_client_encoding_id);

	// Set handler for virtual "clientEncoding" property:
	ot->SetAccessor(JS_STR("clientEncoding"),get_client_encoding,set_client_encoding_id);

	// Static property, useful for stats gathering
	ot->Set(JS_ISOLATE,"queryCount"		, JS_INT(0));

	v8::Local<v8::ObjectTemplate> pt = ft->PrototypeTemplate();

	// PostgreSQL prototype methods (new PostgreSQL().*)
	pt->Set(JS_ISOLATE,"connect"			, v8::FunctionTemplate::New(JS_ISOLATE, _connect));
	pt->Set(JS_ISOLATE,"close"				, v8::FunctionTemplate::New(JS_ISOLATE, _close));
	pt->Set(JS_ISOLATE,"query"				, v8::FunctionTemplate::New(JS_ISOLATE, _query));
	pt->Set(JS_ISOLATE,"queryParams"		, v8::FunctionTemplate::New(JS_ISOLATE, _queryparams));
	pt->Set(JS_ISOLATE,"escape"				, v8::FunctionTemplate::New(JS_ISOLATE, _escape));
	pt->Set(JS_ISOLATE,"escapeBytea"		, v8::FunctionTemplate::New(JS_ISOLATE, _escapebytea));
	pt->Set(JS_ISOLATE,"unescapeBytea"		, v8::FunctionTemplate::New(JS_ISOLATE, _unescapebytea));
	pt->Set(JS_ISOLATE,"sendQuery"			, v8::FunctionTemplate::New(JS_ISOLATE, _sendquery));
	pt->Set(JS_ISOLATE,"sendQueryParams"	, v8::FunctionTemplate::New(JS_ISOLATE, _sendqueryparams));
	pt->Set(JS_ISOLATE,"isBusy"				, v8::FunctionTemplate::New(JS_ISOLATE, _isbusy));
	pt->Set(JS_ISOLATE,"isConnected"		, v8::FunctionTemplate::New(JS_ISOLATE, _isconnected));
	pt->Set(JS_ISOLATE,"socket"				, v8::FunctionTemplate::New(JS_ISOLATE, _socket));
	pt->Set(JS_ISOLATE,"cancel"				, v8::FunctionTemplate::New(JS_ISOLATE, _cancel));
	pt->Set(JS_ISOLATE,"prepare"			, v8::FunctionTemplate::New(JS_ISOLATE, _prepare));
	pt->Set(JS_ISOLATE,"sendPrepare"		, v8::FunctionTemplate::New(JS_ISOLATE, _sendprepare));
	pt->Set(JS_ISOLATE,"sendQuery"			, v8::FunctionTemplate::New(JS_ISOLATE, _sendquery));
	pt->Set(JS_ISOLATE,"sendQueryParams"	, v8::FunctionTemplate::New(JS_ISOLATE, _sendqueryparams));
	pt->Set(JS_ISOLATE,"execute"			, v8::FunctionTemplate::New(JS_ISOLATE, _execute));
	pt->Set(JS_ISOLATE,"sendExecute"		, v8::FunctionTemplate::New(JS_ISOLATE, _sendexecute));
	pt->Set(JS_ISOLATE,"asyncQuery"			, v8::FunctionTemplate::New(JS_ISOLATE, _asyncquery));

	v8::Local<v8::FunctionTemplate> rslt = v8::FunctionTemplate::New(JS_ISOLATE, _result);
	rslt->SetClassName(JS_STR("Result"));
	_rslt.Reset(JS_ISOLATE, rslt);

	v8::Local<v8::ObjectTemplate> resinst = rslt->InstanceTemplate();
	resinst->SetInternalFieldCount(2);

	// Set handler for virtual "error" property:
	resinst->SetAccessor(JS_STR("error"),rslt_error);

	// Set handler for virtual "length" property:
	resinst->SetAccessor(JS_STR("length"),rslt_max);

	// Set handler for virtual "index" property:
	resinst->SetAccessor(JS_STR("index"),rslt_pos);

	// Set handler for virtual "rows" property:
	resinst->SetAccessor(JS_STR("rows"),rslt_rows);

	// Set handler for virtual "nextRow" property:
	resinst->SetAccessor(JS_STR("nextRow"),rslt_next_row);

	v8::Local<v8::ObjectTemplate> resproto = rslt->PrototypeTemplate();

	// Result prototype methods (new PostgreSQL().query().*)
	resproto->Set(JS_ISOLATE,"numRows"			, v8::FunctionTemplate::New(JS_ISOLATE, _numrows));
	resproto->Set(JS_ISOLATE,"numFields"			, v8::FunctionTemplate::New(JS_ISOLATE, _numfields));
	resproto->Set(JS_ISOLATE,"numAffectedRows"		, v8::FunctionTemplate::New(JS_ISOLATE, _numaffectedrows));
	resproto->Set(JS_ISOLATE,"fetchNames"			, v8::FunctionTemplate::New(JS_ISOLATE, _fetchnames));
	resproto->Set(JS_ISOLATE,"fetchResult"			, v8::FunctionTemplate::New(JS_ISOLATE, _fetchresult));
	resproto->Set(JS_ISOLATE,"fetchField"			, v8::FunctionTemplate::New(JS_ISOLATE, _fetchfield));
	resproto->Set(JS_ISOLATE,"fetchRow"				, v8::FunctionTemplate::New(JS_ISOLATE, _fetchrow));
	resproto->Set(JS_ISOLATE,"fetchRowObject"		, v8::FunctionTemplate::New(JS_ISOLATE, _fetchrowobject));
	resproto->Set(JS_ISOLATE,"fetchAll"				, v8::FunctionTemplate::New(JS_ISOLATE, _fetchall));
	resproto->Set(JS_ISOLATE,"fetchAllObjects"		, v8::FunctionTemplate::New(JS_ISOLATE, _fetchallobjects));
	resproto->Set(JS_ISOLATE,"unescapeBytea"		, v8::FunctionTemplate::New(JS_ISOLATE, _unescapebytea));
	resproto->Set(JS_ISOLATE,"clear"				, v8::FunctionTemplate::New(JS_ISOLATE, _clear));
	resproto->Set(JS_ISOLATE,"reset"				, v8::FunctionTemplate::New(JS_ISOLATE, _reset));

	//fprintf(stderr,"pgsql.cc > SHARED_INIT geting context\n");
	v8::Local<v8::Context> tmp_context=JS_CONTEXT;
	
	//fprintf(stderr,"pgsql.cc > SHARED_INIT context=%ld\n",(void*)*(JS_CONTEXT));
	//fprintf(stderr,"pgsql.cc > SHARED_INIT store PostgreSQL function\n");
	v8::MaybeLocal<v8::Function> tmp_func=ft->GetFunction(JS_CONTEXT);
	//fprintf(stderr,"pgsql.cc > SHARED_INIT set PostgreSQL function\n");
	(void)exports->Set(JS_CONTEXT,JS_STR("PostgreSQL"), ft->GetFunction(JS_CONTEXT).ToLocalChecked());
	//fprintf(stderr,"pgsql.cc > SHARED_INIT end()\n");
}
