var assert = require("assert");
var TLS = require("tls").TLS;
var Buffer = require("binary").Buffer;

var plainText = "Testing message.";
var secret = "secret";

exports.testHMACSHA1 = function() {
    try {
        assert.equal(TLS.HashHmac(new Buffer("sha1", "utf-8"), new Buffer(plainText, "utf-8"), new Buffer(secret, "utf-8")).toString(),
        "f9bb6c561e49e17a479bd5746411e43efef798e0", "sha1 with 'Testing message.'");
    } catch(e) {
        system.stdout.writeLine(e);
    }
}
