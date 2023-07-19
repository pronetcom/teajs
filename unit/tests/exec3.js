var Process = require("process").Process;
var Socket = require("socket").Socket;

var proc = new Process();
var fs = require("fs");

var f1 = new fs.File("unit/tests/texts/short-file.txt");
f1.open("r");

var call = proc.exec3(["unit/tests/texts/call"]);

var callIn = new fs.File(call.in);
Socket.makeNonblock(call.in);
callIn.open("w");

var callOut = new fs.File(call.out);
Socket.makeNonblock(call.out);
callOut.open("r");

var callErr = new fs.File(call.err);
Socket.makeNonblock(call.err);
callErr.open("r");

if (system.env.PRINT_DEBUGS == 1) {
    system.stdout.writeLine("executing call")
}

while (line = f1.readLine()) {
    callIn.write(line);
    var obj = callOut.readNonblock();
    line = obj.result;
    if (line != null && obj.size > 0) {
        system.stdout.writeLine("call out: ");
        system.stdout.writeLine(line);
    }
    obj = callErr.readNonblock();
    line = obj.result;
    if (line != null && obj.size > 0) {
        system.stdout.writeLine("call err: ");
        system.stdout.writeLine(line);
    }
}

if (system.env.PRINT_DEBUGS == 1) {
    system.stdout.writeLine("checking stdout")
}

while (true) {
    var obj = callOut.readNonblock();
    line = obj.result;
    if (line != null && obj.size > 0) {
        system.stdout.writeLine("call out: ");
        system.stdout.writeLine(line);
    }
    if (obj.info == 0) {
        system.stdout.writeLine("out break");
        break;
    }
}
while (true) {
    var obj = callErr.readNonblock();
    line = obj.result;
    if (line != null && obj.size > 0) {
        system.stdout.writeLine("call err: ")
        system.stdout.writeLine(line);
    }
    if (obj.info == 0) {
        system.stdout.writeLine("err break");
        break;
    }
}
callIn.close();
callOut.close();
callErr.close();
f1.close();