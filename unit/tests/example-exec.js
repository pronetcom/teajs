var http = require("http");
var req = new http.ClientRequest("https://system.easymerch.ru/112.jpg");
var d = new Date();
var resp = req.send(true);
system.stdout.writeLine('FINAL TIME = ' + (new Date() - d));


var req2 = new http.ClientRequest("https://system.easymerch.ru/api/i18n/download_files/?project=em-mobile");
d = new Date();
var resp2 = req2.send(true);
system.stdout.writeLine('FINAL TIME = ' + (new Date() - d));