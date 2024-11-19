var assert = require("assert");
var Archiver = require("archive").Archiver;
var Buffer = require("binary").Buffer;

var plainText = "Testing message.";
var fileToZip = "/home/vismut-fo/teajs/unit/tests/texts/short-file.txt"

exports.testArchiveZip = function() {
    try {
        // system.stdout.writeLine("archiver before constructor");
        var archiver = new Archiver(Archiver.ZIP);
        // system.stdout.writeLine("archiver before opening archive");
        archiver.open("unit/tests/texts/tempArchive.zip", Archiver.ZIP_TRUNCATE | Archiver.ZIP_CREATE);
        // system.stdout.writeLine("archiver before adding file");
        archiver.addFile("tempFile.txt", plainText, true);
        archiver.addFile("tempFile2.txt", fileToZip, false); // should be used only with absolute path
        // system.stdout.writeLine("archiver before closing");
        archiver.close();

        // system.stdout.writeLine("archiver before opening archive (reading)");
        
        
        archiver.open("unit/tests/texts/tempArchive.zip", Archiver.ZIP_RDONLY);
        
        var result = archiver.readFile("tempFile.txt", 100);

        
        // system.stdout.writeLine("archiver after reading archive");
        // system.stdout.writeLine(result.toString("utf-8", 0, result.length));
        
        assert.equal(result.toString("utf-8", 0, result.length),
        plainText, "Not expected string in zip");
        
        
    } catch(e) {
        system.stdout.writeLine(e);
    }
}

// exports.testArchiveZip();
