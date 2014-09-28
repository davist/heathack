var serialport = require("serialport");
var http = require("http");
var express = require("express");

var SP = serialport.SerialPort;

var serial = new SP("/dev/ttyUSB0", {
	baudrate: 57600,
	parser: serialport.parsers.readline("\r\n")
});


var nodes = {};


///////////////////////////
// web server

var app = express();

app.use(express.static("./static"));

app.get("/data", function(req, res) {

	res.writeHead(200, {"Content-Type": "application/json" });
	res.end( JSON.stringify(nodes) );
});


app.listen(80);


///////////////////////////
// serial listener

serial.on("open", function() {

	console.log("serial port open");

	serial.on("data", function(data) {

		var tokens = data.toString().split(" ");

		if (tokens[0] != "heathack") return;

		var time = new Date().getSeconds();

		var nodeid = tokens[1];

		var node = nodes[nodeid];

		if (!node) {
			node = {};
			node.id = nodeid;
			node.sensors = {};
			nodes[nodeid] = node;
		}

		for (var i=2; i<tokens.length - 2; i+=3) {

			var sensorid = tokens[i];
			var type = tokens[i+1];
			var value = tokens[i+2];

			var sensor = node.sensors[sensorid];

			if (!sensor) {
				sensor = {};
				sensor.id = sensorid;
				sensor.type = type;
				sensor.lastTime = -1;
				sensor.readings = [];
				node.sensors[sensorid] = sensor;
			}

			var prev = sensor.lastTime;
			if (prev > time) prev = -1;

			var cur = time;
			while (cur > prev) {
				sensor.readings[cur] = value;
				cur --;
			}

			if (sensor.lastTime > time) {

				prev = sensor.lastTime;
				cur = 59;
				while (cur > prev) {
					sensor.readings[cur] = value;
					cur --;
				}
			}

			sensor.lastTime = time;
		}


	});

});