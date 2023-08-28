var assert = require("assert");
var http = require("http");


function do_send(url, length, body, headers) {
	var req = new http.ClientRequest(url);
	system.stdout.writeLine(req.post);
	req.addParams(new Map([
		['name', 'bobby hadz'],
		['country', 'Chile'],
	]));
	req.addFiles(new Map([
		['file', 'unit/tests/texts/very-long-file.txt']
	]));
	//req.post.set("name", "Artem");
	//req.postFiles.set("file", "unit/tests/texts/very-long-file.txt");
	system.stdout.writeLine("multipart after");
	var resp = req.sendFiles(false);
	if (length >= 0) {
		assert.equal(resp.data.length, length, "Response must be " + length + " bytes");
	}
	if (body) {
		assert.equal(resp.data.toString("utf-8"), body, "Wrong response body");
	}
	if (headers) {
		for (var k in headers) {
			assert.equal(resp.header(k), headers[k], "Wrong response header " + k + " must be '" + headers[k] + "'");
		}
	}
}

exports.testHTTPS_multipart = function () {
	system.stdout.writeLine("multipart begin");
	do_send("https://easymerch.com/site.css");
	system.stdout.writeLine("multipart end");
};
