const SerialPort = require("serialport");
const Readline = SerialPort.parsers.Readline;
const express = require("express");
const config = require("./config");

// load configured publisher
const publishAPI = require('./' + config.publisher);

const publisher = new publishAPI.Publisher(config.publish_settings);


///////////////////////////
// data structure to hold readings which the web server publishes

const nodeData = {
	nodes: {},

	// number of previous readings to store
	maxReadings: 20,

    // current time to allow client to calculate time since each reading
    currentTime: null
};


///////////////////////////
// web server

const app = express();

app.use(express.static("./static"));

app.get("/data", function(req, res) {

    nodeData.currentTime = Date.now();
	res.writeHead(200, {"Content-Type": "application/json" });
	res.end( JSON.stringify(nodeData) );
});


app.listen(80);


///////////////////////////
// serial listener

// JeeLink on USB0 at 9600 baud default
const serial = new SerialPort(config.serialport || "/dev/ttyUSB0", {baudRate: config.baudrate || 9600});

const parser = serial.pipe(new Readline());

serial.on("error", function(e) {
	console.log("Serial port: " + e);
	process.exit(1);
});

// event handler for an input string from the serial port
parser.on("data", function(data) {

	// split string into tokens
	const tokens = data.toString().split(" ");

	// if first token (word) isn't "heathack", it's not valid data
	if (tokens[0] !== "heathack") return;

	// next token is node id
	const nodeid = tokens[1];

	// get the relevant node object
	let node = nodeData.nodes[nodeid];

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
	const curReadings = [];

	for (let i=2; i<tokens.length - 2; i+=3) {

		const sensorid = tokens[i];
		const type = tokens[i+1];
		const value = tokens[i+2];

		curReadings.push( {id: sensorid, type: type, value: value} );

		// find sensor object on node
		let sensor = node.sensors[sensorid];

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
