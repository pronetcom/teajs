var pgsql = require("pgsql");
P = new pgsql.PostgreSQL();
P.connect("pronetcom.one", "5432", "teajs_test", "teajs_test", "puTh1Ju2");
Q = P.query("select true as col1, false as col2"); // , 'boolean'::regtype::integer as col3
system.stdout.writeLine("pgsql result:");
system.stdout.writeLine("x: " + Q.numFields() + "; y: " + Q.numRows());
Res = Q.fetchResult(0, 0);
system.stdout.writeLine(Res);
Res = Q.fetchResult(0, 1);
system.stdout.writeLine(Res);
Q.clear();
