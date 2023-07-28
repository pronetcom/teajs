var fs = require("fs");

var f1 = new fs.File("unit/tests/texts/very-long-file.txt");
f1.open("r");

if (system.env.PRINT_DEBUGS == 1) {
    system.stdout.writeLine("fs_tell_seek opened file");
}
system.stdout.writeLine("f1 tell: " + f1.tell());
system.stdout.writeLine("f1 seek end: " + f1.seek(0, 2));
system.stdout.writeLine("f1 tell: " + f1.tell());
system.stdout.writeLine("f1 seek start: " + f1.seek(0, 0));

f1.close();
//