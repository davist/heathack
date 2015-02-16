var serialport = require("serialport");
var express = require("express");
var request = require('request');

// EmonCMS key for relevant account
var API_KEY = "ee64225c5bb56a053222f8d33f153084";

var SP = serialport.SerialPort;

// JeeLink on USB0 at 9600 baud
var serial = new SP("/dev/ttyUSB0", {
	baudrate: 9600,
	parser: serialport.parsers.readline("\r\n")
});

///////////////////////////
// data structure to hold readings which the web server publishes

var nodeData = {
	nodes: {},
	
	// number of previous readings to store
	maxReadings: 20
};


///////////////////////////
// web server

var app = express();

app.use(express.static("./static"));

app.get("/data", function(req, res) {

	res.writeHead(200, {"Content-Type": "application/json" });
	res.end( JSON.stringify(nodeData) );
});


app.listen(80);

///////////////////////////
// interface to EmonCMS
//
// apikey {String}: key for access to relevant EmonCMS account
// nodeid_offset {Number}: amount to add to node ids to generate emoncms node ids
EmonCMS = function(apikey, request, nodeid_offset) {
	this.apikey = apikey;
	this.nodeid_offset = nodeid_offset || 0;
	this.request = request;
}

EmonCMS.prototype = {

	// names and ranges for sensor types in numerial order from zero.
	// min and max = 0 means allow all values
	sensorTypes: {
		0: { name: "test",        min: 0, max: 0 },
		1: { name: "temperature", min: -20, max: 50 },
		2: { name: "humidity",    min: 0, max: 100 },
		3: { name: "light",       min: 0, max: 255 },
		4: { name: "movement",    min: 0, max: 0 },
		5: { name: "pressure",    min: 0, max: 0 },
		6: { name: "sound",       min: 0, max: 0 },
		7: { name: "lowbatt",     min: 0, max: 0 }
	},

	serverUrl: "http://emoncms.org/input/post.json?node=$node&json=$json&apikey=$key",
	
	publish: function(nodeid, readings) {

		var json = this.formatJson(readings);
	
		var url = this.serverUrl
					.replace("$node", nodeid)
					.replace("$json", JSON.stringify(json))
					.replace("$key", this.apikey);
					
		this.request(url, function (error, response, body) {
		  if (response.statusCode != 200) {
			console.log(response.statusCode + " response: " + body);
		  }
		});
	},
	
	formatJson: function(readings) {
	
		var json = {};
	
		for (i in readings) {
			var reading = readings[i];
		
			var id = reading.id;
			var type = parseInt(reading.type);
			var value = parseFloat(reading.value);
			
			// sanity check the readings and ignore if out of range
			if (type > 0 && type <= 7) {
				if ( !(this.sensorTypes[type].min == 0 && this.sensorTypes[type].max == 0) ) {

					if (value >= this.sensorTypes[type].min &&
						value <= this.sensorTypes[type].max) {

						// combine readings into JSON-formatted data for posting to emoncms.
						// format is {type}{id}:{reading}, e.g. temperature1:23.0
						json["" + this.sensorTypes[type].name + id] = value;
					}
				}
			}
		}
		
		return json;
	}
};

///////////////////////////
// EmonCMS publisher. Needs an API key for the relevant account
var emon = new EmonCMS(API_KEY, request);


///////////////////////////
// serial listener

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

		// rest of tokens are tuples of sensor id, type and value
		
		// data to publish to emoncms
		var curReadings = new Array();
		
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

		// publish to EmonCMS
		emon.publish(nodeid, curReadings);
	});

});
