///////////////////////////
// interface to EmonCMS
//
// apikey {String}: key for access to relevant EmonCMS account
// nodeid_offset {Number}: amount to add to node ids to generate emoncms node ids

var request = require('request');


// constructor
var constructor = function(config) {
	this.server = config.server || "emoncms.org";
	this.apikey = config.apikey;
	this.nodeid_offset = config.nodeid_offset || 0;

	// create a partially filled template as server and key won't change
	this.urlTemplate = serverUrlTemplate
				.replace("$server", this.server)
				.replace("$key", this.apikey);
}

// names and ranges for sensor types in numerial order from zero.
// min and max = 0 means allow all values
var sensorTypes = {
	0: { name: "test",        min: 0, max: 0 },
	1: { name: "temperature", min: -20, max: 50 },
	2: { name: "humidity",    min: 0, max: 100 },
	3: { name: "light",       min: 0, max: 255 },
	4: { name: "movement",    min: 0, max: 0 },
	5: { name: "pressure",    min: 0, max: 0 },
	6: { name: "sound",       min: 0, max: 0 },
	7: { name: "lowbatt",     min: 0, max: 0 }
};

var	serverUrlTemplate = "http://$server/input/post.json?node=$node&json=$json&apikey=$key";
	
var publish = function(nodeid, readings) {

	var json = formatJson(readings);

	var url = this.urlTemplate
				.replace("$node", this.nodeid_offset + nodeid)
				.replace("$json", JSON.stringify(json));
				
	request(url, function (error, response, body) {
	  if (response.statusCode != 200) {
		console.log(response.statusCode + " response: " + body);
	  }
	});
};
	
var formatJson = function(readings) {
	
	var json = {};

	for (i in readings) {
		var reading = readings[i];
	
		var id = reading.id;
		var type = parseInt(reading.type);
		var value = parseFloat(reading.value);
		
		// sanity check the readings and ignore if out of range
		if (type > 0 && type <= 7) {
			if (( sensorTypes[type].min == 0 && sensorTypes[type].max == 0 ) ||
				(value >= sensorTypes[type].min && value <= sensorTypes[type].max)) {

				// combine readings into JSON-formatted data for posting to emoncms.
				// format is {type}{id}:{reading}, e.g. temperature1:23.0
				json["" + sensorTypes[type].name + id] = value;
			}
		}
	}
	
	return json;
};


exports.Publisher = constructor;
constructor.prototype.publish = publish;

