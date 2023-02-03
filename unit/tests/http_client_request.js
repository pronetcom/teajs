/**
 * This file tests http client request
 */
var assert = require("assert");
var http=require("http");


function do_download(url,length,body,headers)
{
	var req=new http.ClientRequest(url);

	var resp=req.send(false);
	if (length>=0) {
		assert.equal(resp.data.length,length, "Response must be "+length+" bytes");
	}
	if (body) {
		assert.equal(resp.data.toString("utf-8"),body, "Wrong response body");
	}
	if (headers) {
		for (var k in headers) {
			assert.equal(resp.header(k),headers[k],"Wrong response header "+k+" must be '"+headers[k]+"'");
		}
	}
}

exports.testHTTP_short = function() {
	do_download("http://teajs.org/dltest/short_file.txt",12,"hello world\n");

};
exports.testHTTPS_short = function() {
	do_download("https://teajs.org/dltest/short_file.txt",12,"hello world\n");
};

exports.testHTTP_long = function() {
	do_download("http://teajs.org/dltest/long_file.txt",10*1024*1024);

};
exports.testHTTPS_long = function() {
	do_download("https://teajs.org/dltest/long_file.txt",10*1024*1024);
};
exports.testHTTPS_reconn = function() {
	do_download("https://logincall.reconn.ru/api/v1/calls/make");
};
exports.testHTTPS_short2 = function() {
	do_download("https://easymerch.com/site.css",-1,undefined,{"CONTENT-TYPE": "text/css","CONNECTION": "close"});
};
