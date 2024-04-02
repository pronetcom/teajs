var assert = require("assert");
var Archiver = require("archive").Archiver;
var Buffer = require("binary").Buffer;

var plainText = "Testing message.";
var fileToZip = "/home/vismut-fo/teajs/unit/tests/texts/short-file.txt"

exports.testArchiveZip = function() {
    try {
        system.stdout.writeLine("archiver before constructor");
        var archiver = new Archiver(Archiver.ZIP);
        system.stdout.writeLine("archiver before opening archive");
        archiver.open("unit/tests/texts/tempArchive.zip", Archiver.ZIP_TRUNCATE | Archiver.ZIP_CREATE);
        system.stdout.writeLine("archiver before adding file");
        archiver.addFile("tempFile.txt", plainText, true);
        archiver.addFile("tempFile2.txt", fileToZip, false); // should be used only with absolute path
        system.stdout.writeLine("archiver before closing");
        archiver.close();

        system.stdout.writeLine("archiver before opening archive (reading)");
        
        /*
        archiver.open("unit/tests/texts/tempArchive.zip", Archiver.ZIP_RDONLY);
        var result = archiver.readFileByName();
        assert.equal(result.toString("utf-8", 0, result.length),
        "f9bb6c561e49e17a479bd5746411e43efef798e0", "sha1 with 'Testing message.'");
        */
        // var result = TLS.HashHmac(new Buffer("sha1", "utf-8"), new Buffer(plainText, "utf-8"), new Buffer(secret, "utf-8"));
        // assert.equal(result.toString("utf-8", 0, result.length),
        // "f9bb6c561e49e17a479bd5746411e43efef798e0", "sha1 with 'Testing message.'");
    } catch(e) {
        system.stdout.writeLine(e);
    }
}

exports.testArchiveZip();
