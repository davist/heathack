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
			// only include low battery when it's reporting a low battery
			if (sensor.type == 7 && sensor.readings[sensor.lastReading] == 0) return;

			var sensorDiv = $("<div/>", {
				"class": "sensor"
			});
			
			if (sensor.type == 7) {
				$(sensorDiv).addClass("warning");
			}

			$("<span/>", {
				"class": "sensortype",
				"html" : SensorType[sensor.type].name
			}).appendTo(sensorDiv);

			if (sensor.type != 7) {
				$("<span/>", {
					"class": "reading",
					"html" : sensor.readings[sensor.lastReading]
				}).appendTo(sensorDiv);

				$("<span/>", {
					"class": "unit",
					"html" : SensorType[sensor.type].unit
				}).appendTo(sensorDiv);
			}

			nodeDiv.append( sensorDiv );
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
			// don't inclode low battery in history
			if (sensor.type == 7) return;
		
			var sensorDiv = $("<div/>", {
				"class": "sensor-history"
			}).appendTo(nodeDiv);
	
			$("<span/>", {
				"class": "sensortype",
				"html" : SensorType[sensor.type].name
			}).appendTo(sensorDiv);

			var unit = SensorType[sensor.type].unit;
			var divisor = SensorType[sensor.type].maximum / 150;

			function renderReading(reading) {
				var height = Math.floor(reading / divisor);
	
				$("<div/>", {
					"class": "reading",
					"title": reading + " " + unit,
					"style": "height: " + height + "px",
					"html":  "<span>" + reading + "</span>"
				}).appendTo(sensorDiv);
			}

			for (var index= sensor.lastReading+1; sensor.readings[index]; index++) {
				renderReading(sensor.readings[index]);
			}

			for (var index= 0; index<=sensor.lastReading; index++) {
				renderReading(sensor.readings[index]);
			}
		});


		dom.append( nodeDiv );		
	});
}


SensorType = {

0: {name: "Test", unit: "", maximum: 0},
1: {name: "Temp", unit: "C", maximum: 30},
2: {name: "Hum", unit: "%", maximum: 100},
3: {name: "Light", unit: "", maximum: 255},
4: {name: "Motion", unit: "", maximum: 10},
5: {name: "Pres", unit: "mb", maximum: 0},
6: {name: "Sound", unit: "dB", maximum: 0},
7: {name: "Low Battery", unit: "", maximum: 1},


};
