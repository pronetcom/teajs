var assert = require("assert");
var http = require("http");
var fs = require("fs");


function do_download(url, length, body, headers) {
	var req = new http.ClientRequest(url);

	var resp = req.download("unit/tests/texts/very-long-answer.xml", false);
	if (length >= 0) {
		assert.equal(resp.data.length, length, "Response must be " + length + " bytes");
	}
	//var f2 = new fs.File("unit/tests/texts/very-long-answer.xml");
	//f2.open("w");
	//f2.write(resp.data.toString("utf-8"));
	if (body) {
		assert.equal(resp.data.toString("utf-8"), body, "Wrong response body");
	}
	if (headers) {
		for (var k in headers) {
			assert.equal(resp.header(k), headers[k], "Wrong response header " + k + " must be '" + headers[k] + "'");
		}
	}
}

if (system.env.REQUIRE_LONGEST == 1) {
	exports.testHTTPS_longest = function () {
		system.gc();
		if (system.env.PRINT_DEBUGS == 1) {
			system.stdout.writeLine('HTTPS LONGEST START');
		}
		do_download("https://loreal-il-test.gd.easymerch.com/f-loreal-il-test/images/2561.xml");
		system.gc();
		if (system.env.PRINT_DEBUGS == 1) {
			system.stdout.writeLine('HTTPS LONGEST END');
		}
	};
}
