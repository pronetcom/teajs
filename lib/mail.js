var fs=require("fs");
exports.mail = function(to, subject, body, headers, auth) {
	var file=new fs.File("/tmp/last-email.log");
	try {
		file.open("w");
	} catch(e) {
		file=undefined;
	}
	var from = Config["smtpFrom"];
	var rcpt = [];
	var h = {
		"To":[],
		"Cc":[],
		"From":from
	};
	if (to) { 
		rcpt.push(to); 
		h["To"].push(to); 
	}
	if (subject) { h["Subject"] = subject; }
	
	for (var p in headers) {
		var val = headers[p];
		if (p.match(/^to$/i)) {
			rcpt.push(val);
			h["To"].push(val);
		} else if (p.match(/^cc$/i)) {
			rcpt.push(val);
			h["Cc"].push(val);
		} else if (p.match(/^bcc$/i)) {
			rcpt.push(val);
		} else if (p.match(/^subject$/i)) {
			h["Subject"] = val;
		} else {
			h[p] = val; 
		}
	}
	
	function send(text, nowait) {
		if (file) file.write("S: "+text+"\n");
		sock.send(text);
		sock.send("\r\n");
		if (!nowait) {
			var data = sock.receive(1024);
			if (file) {file.write("R: ");file.write(data);}
		}
	}

	var Socket = require("socket").Socket;
	var host = Socket.getHostName();
	var sock = new Socket(Socket.PF_INET, Socket.SOCK_STREAM, Socket.IPPROTO_TCP);
	sock.connect(Config["smtpHost"], Config["smtpPort"]);
	var data = sock.receive(1024);
	if (file) {file.write("R: ");file.write(data);}
	
	if (auth && auth.type.match(/^login$/i)) {
		var base64 = require("base64");
		send("EHLO "+host); 
		send("AUTH LOGIN"); 
		send(base64.encode(auth.user));
		send(base64.encode(auth.password)); 
	} else {
		send("HELO "+host);
	}
	system.usleep(200);
	if (h.From.match(/</)) send("MAIL FROM:"+h.From); else send("MAIL FROM:<"+h.From+">");
	system.usleep(200);
	for (var i=0;i<rcpt.length;i++) {
		if (rcpt[i].match(/</)) send("RCPT TO:"+rcpt[i]); else send("RCPT TO:<"+rcpt[i]+">");
	}
	send("DATA");
	
	for (var name in h) {
		var value = h[name];
		if (!(value instanceof Array)) { value = [value]; }
		for (var i=0;i<value.length;i++) {
			send(name+": "+value[i], true);
		}
	}
	send("", true);
	var b = body.split(/\n/).map(function(a) { var ret="";while (a.length>200) {ret+=a.substr(0,200)+"\n";a=a.substr(200);} ret+=a;return ret;}).join("\n").replace(/\n\./g,"\n..");
	send(b+"\r\n.");
	send("QUIT");
	sock.close();
	if (file) file.close();
}
