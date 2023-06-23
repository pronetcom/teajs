var Process = require("process").Process;
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
zipIn.open("w");
while (line = f1.read()) {
    if (system.env.PRINT_DEBUGS == 1) {
        system.stdout.writeLine(line);
    }
    zipIn.write(line);
}
zipIn.close();
var zipOut = new fs.File(zip.out);
zipOut.open("r");
while (line = zipOut.read()) {
    if (system.env.PRINT_DEBUGS == 1) {
        system.stdout.writeLine(line);
    }
    f2.write(line);
}
zipOut.close();
var zipErr = new fs.File(zip.err);
zipErr.open("r");
while (line = zipErr.read()) {
    system.stdout.writeLine(line);
}
zipErr.close();

if (system.env.PRINT_DEBUGS == 1) {
    system.stdout.writeLine("open3 right after exec3");
}
/*
function read_data()
{
        var sockets=[proc.out,proc.err];
        var ready_sockets;
        while (ready_sockets=socket.select(sockets,[],[],1)) {
                ready_sockets.forEach(function(s) {
                        f2.write(s.read());
                });
        }
}
while (line=f1.readLine()) {
        read_data();
        zip.in.write(line);
}
zip.in.close();
//while (!proc.waitpid(zip.pid)) {
        //read_data();
//}
zip.close();
*/

f2.close();
f1.close();