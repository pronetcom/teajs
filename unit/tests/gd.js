/**
 * This file tests GD module.
 */

var assert = require("assert");
var gd= require("gd");

exports.testOK = function() {
	var parts = module.id.split(/[\/\\]/);
	parts.pop();
	parts.push("sample.jpg");
	var path = parts.join("/");
	
	var img=new gd.Image(gd.Image.ANY,path);
	assert.equal(img!=null,true,"NULL returned");
}

exports.testFAIL1 = function() {
	var parts = module.id.split(/[\/\\]/);
	parts.pop();
	parts.push("sample.png");
	var path = parts.join("/");
	
	try {
		var img=new gd.Image(gd.Image.ANY,path);
		system.stdout.writeLine("no error thrown - this is bad");
		assert.equal(0,true,"Must throw error (this is a wrong file)");
	} catch(e) {
		system.stdout.writeLine("			error thrown (this is okay) - "+e);
		assert.equal(e,"Error: Cannot load file - possibly corrupted or wrong extension","Wrong error message");
	}
};

exports.testFAIL2 = function() {
	var parts = module.id.split(/[\/\\]/);
	parts.pop();
	parts.push("sample2.png");
	var path = parts.join("/");
	
	try {
		var img=new gd.Image(gd.Image.ANY,path);
		system.stdout.writeLine("no error thrown - this is bad");
		assert.equal(0,true,"Must throw error (this is a wrong file)");
	} catch(e) {
		system.stdout.writeLine("			error thrown (this is okay) - "+e);
		assert.equal(e,"Error: Cannot open file - No such file or directory","Wrong error message");
	}
};

