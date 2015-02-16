$(window).load(function() {

refreshData();
setInterval( refreshData, 5000 );

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

	$.each(data.nodes, function(nodeid, node) {
	
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
				"html" : sensor.readings[sensor.lastReading]
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

	$.each(data.nodes, function(nodeid, node) {
	
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

			function renderReading(index) {
				var height = Math.floor(sensor.readings[index] / divisor);
	
				$("<div/>", {
					"class": "reading",
					"title": sensor.readings[index] + " " + unit,
					"style": "height: " + height + "px"
				}).appendTo(sensorDiv);
			}

			for (var index= sensor.lastReading+1; index < node.maxReadings; sec++) {
				renderReading(index);
			}

			for (var index= 0; index<=sensor.lastReading; index++) {
				renderReading(index);
			}
		});


		dom.append( nodeDiv );		
	});
}


SensorType = {

0: {name: "Test", unit: "", maximum: 0},
1: {name: "Temperature", unit: "C", maximum: 30},
2: {name: "Humidity", unit: "%", maximum: 100},
3: {name: "Light", unit: "", maximum: 255},
4: {name: "Movement", unit: "", maximum: 1},
5: {name: "Pressure", unit: "mb", maximum: 0},
6: {name: "Sound", unit: "dB", maximum: 0},
7: {name: "Low Battery", unit: "", maximum: 1},


};
