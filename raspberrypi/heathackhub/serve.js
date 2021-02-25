var serialport = require("serialport");
var express = require("express");
var config = require("./config");

// load configured publisher
var publishAPI = require('./' + config.publisher);

var publisher = new publishAPI.Publisher(config.publish_settings);


var SP = serialport.SerialPort;

// JeeLink on USB0 at 9600 baud default
var serial = new SP(config.serialport || "/dev/ttyUSB0", {
	baudrate: config.baudrate || 9600,
	parser: serialport.parsers.readline("\r\n")
});

///////////////////////////
// data structure to hold readings which the web server publishes

var nodeData = {
	nodes: {},

	// number of previous readings to store
	maxReadings: 20,

    // current time to allow client to calculate time since each reading
    currentTime: null
};


///////////////////////////
// web server

var app = express();

app.use(express.static("./static"));

app.get("/data", function(req, res) {

    nodeData.currentTime = Date.now();
	res.writeHead(200, {"Content-Type": "application/json" });
	res.end( JSON.stringify(nodeData) );
});


app.listen(80);


///////////////////////////
// serial listener

serial.on("error", function(e) {
	console.log("Serial port: " + e);
	process.exit(1);
});

serial.on("open", function() {

	console.log("serial port open");

	// event handler for an input string from the serial port
	serial.on("data", function(data) {

		// split string into tokens
		var tokens = data.toString().split(" ");

		// if first token (word) isn't "heathack", it's not valid data
		if (tokens[0] != "heathack") return;

		// next token is node id
		var nodeid = tokens[1];

		// get the relevant node object
		var node = nodeData.nodes[nodeid];

		// if it doesn't exist, create a new one
		if (!node) {
			node = {};
			node.id = nodeid;
			node.sensors = {};
			nodeData.nodes[nodeid] = node;
		}

		node.lastReadingTime = Date.now();

		// rest of tokens are tuples of sensor id, type and value

		// data to publish to emoncms
		var curReadings = [];

		for (var i=2; i<tokens.length - 2; i+=3) {

			var sensorid = tokens[i];
			var type = tokens[i+1];
			var value = tokens[i+2];

			curReadings.push( {id: sensorid, type: type, value: value} );

			// find sensor object on node
			var sensor = node.sensors[sensorid];

			// if it doesn't exist, create a new one
			if (!sensor) {
				sensor = {};
				sensor.id = sensorid;
				sensor.type = type;
				sensor.lastReading = -1;
				sensor.readings = [];
				node.sensors[sensorid] = sensor;
			}

			// update pointer for next reading
			sensor.lastReading++;
			if (sensor.lastReading >= nodeData.maxReadings) sensor.lastReading = 0;

			// store nodes.maxReadings of readings
			sensor.readings[sensor.lastReading] = value;
		}

		if (config.verbose) {
			console.log("Node " + nodeid + ":");
			console.log(curReadings);
		}

		// publish to EmonCMS
		publisher.publish(nodeid, curReadings);
	});

});
