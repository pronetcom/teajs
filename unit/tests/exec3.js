var Process = require("process").Process;

var proc = new Process();
var fs = require("fs");
//proc.system("bash -c 'echo 1'");
var ECHO = proc.exec3(["bash", "-c", "echo Hello world!"]);

var ECHOOut = new fs.File(ECHO.out);
ECHOOut.open("r");
while (line = ECHOOut.read()) {
    system.stdout.writeLine(line);
}
ECHOOut.close();
var ECHOErr = new fs.File(ECHO.err);
ECHOErr.open("r");
while (line = ECHOErr.read()) {
    system.stdout.writeLine(line);
}
ECHOErr.close();