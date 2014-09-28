$(window).load(function() {

refreshData();
setInterval( refreshData, 1000 );

});


function refreshData() {

	$.getJSON("/data", function(data) {
	
		updateCurrent(data);
		updateHistory(data);	
	});

}


function updateCurrent(data) {

	var dom = $("#current")

	dom.empty();

	$.each(data, function(nodeid, node) {
	
		var nodeDiv = $("<div/>", {
			"class": "node-current"
		});

		$("<span/>", {
			"class": "nodeid",
			"html" : "Node " + nodeid
		}).appendTo(nodeDiv);

		$.each(node.sensors, function(sensorid, sensor) {

			$("<span/>", {
				"class": "sensortype",
				"html" : SensorType[sensor.type].name
			}).appendTo(nodeDiv);

			$("<span/>", {
				"class": "reading",
				"html" : sensor.readings[sensor.lastTime]
			}).appendTo(nodeDiv);

			$("<span/>", {
				"class": "unit",
				"html" : SensorType[sensor.type].unit
			}).appendTo(nodeDiv);

		});


		dom.append( nodeDiv );		
	});


}


function updateHistory(data) {

	var dom = $("#history")

	dom.empty();

	$.each(data, function(nodeid, node) {
	
		var nodeDiv = $("<div/>", {
			"class": "node-history"
		});

		$("<h3/>", {
			"class": "nodeid",
			"html" : "Node " + nodeid
		}).appendTo(nodeDiv);

		$.each(node.sensors, function(sensorid, sensor) {

			var sensorDiv = $("<div/>", {
				"class": "sensor-history"
			}).appendTo(nodeDiv);
	
			$("<span/>", {
				"class": "sensortype",
				"html" : SensorType[sensor.type].name
			}).appendTo(sensorDiv);

			var unit = SensorType[sensor.type].unit;
			var divisor = SensorType[sensor.type].maximum / 150;

			function renderReading(sec) {
				var height = Math.floor(sensor.readings[sec] / divisor);
	
				$("<div/>", {
					"class": "reading",
					"title": sensor.readings[sec] + " " + unit,
					"style": "height: " + height + "px"
				}).appendTo(sensorDiv);
			}

			for (var sec= sensor.lastTime+1; sec<60; sec++) {
				renderReading(sec);
			}

			for (var sec= 0; sec<=sensor.lastTime; sec++) {
				renderReading(sec);
			}
		});


		dom.append( nodeDiv );		
	});
}


SensorType = {

0: {name: "Undefined", unit: "", maximum: 0},
1: {name: "Temperature", unit: "C", maximum: 50},
2: {name: "Humidity", unit: "%", maximum: 100},
3: {name: "Light", unit: "", maximum: 255},
4: {name: "Movement", unit: "", maximum: 1},
5: {name: "Pressure", unit: "mb", maximum: 0},
6: {name: "Sound", unit: "dB", maximum: 0},
7: {name: "Low Battery", unit: "", maximum: 1},


};
