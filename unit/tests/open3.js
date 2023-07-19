var Process = require("process").Process;
var Socket = require("socket").Socket;
var proc = new Process();

if (system.env.PRINT_DEBUGS == 1) {
    system.stdout.writeLine("open3 process");
}

var fs=require("fs");

var f1 = new fs.File("unit/tests/texts/very-long-file.txt");
f1.open("r");
var f2 = new fs.File("unit/tests/texts/very-long-file.txt.gz");
f2.open("w");

if (system.env.PRINT_DEBUGS == 1) {
    system.stdout.writeLine("open3 opened files");
}
var line = '';

if (system.env.PRINT_DEBUGS == 1) {
    system.stdout.writeLine("open3 readed f1");
}

var zip = proc.exec3(["gzip", "-fc"]);

var zipIn = new fs.File(zip.in);
Socket.makeNonblock(zip.in);
zipIn.open("w");

var zipOut = new fs.File(zip.out);
Socket.makeNonblock(zip.out);
zipOut.open("r");

var zipErr = new fs.File(zip.err);
Socket.makeNonblock(zip.err);
zipErr.open("r");

while (line = f1.readLine()) {
    if (system.env.PRINT_DEBUGS == 1) {
        system.stdout.writeLine("readed from f1");
    }
    zipIn.write(line);
    var obj = zipOut.readNonblock();
    line = obj.result;
    if (line != null && obj.size > 0) {
        if (system.env.PRINT_DEBUGS == 1) {
            system.stdout.writeLine("!!!");
        }
        f2.write(line);
    }
    obj = zipErr.readNonblock();
    line = obj.result;
    if (line != null && obj.size > 0) {
        system.stdout.writeLine(line);
    }
}
zipIn.close();
while (true) {
    var obj = zipOut.readNonblock();
    line = obj.result;
    //line = zipOut.read();
    if (line != null && obj.size > 0) {
        system.stdout.writeLine("out read");
        if (system.env.PRINT_DEBUGS == 1) {
            system.stdout.writeLine(line);
        }
        f2.write(line);
    }
    if (obj.info == 0) {
        system.stdout.writeLine("out break");
        break;
    }
}
while (true) {
    var obj = zipErr.readNonblock();
    system.stdout.writeLine("err read");
    line = obj.result;
    //line = zipERR.read();
    if (line != null && obj.size > 0) {
        system.stdout.writeLine(line);
    }
    if (obj.info == 0) {
        system.stdout.writeLine("err break");
        break;
    }
}

zipOut.close();
zipErr.close();

if (system.env.PRINT_DEBUGS == 1) {
    system.stdout.writeLine("open3 right after exec3");
}

f2.close();
f1.close();