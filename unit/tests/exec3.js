var Process = require("process").Process;

var proc = new Process();

//proc.system("bash -c 'echo 1'");
system.stdout.writeLine(proc.exec3(["bash", "-c", "echo qqqq"]).out);
