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

function readableTime(t) {
	var tSecs = Math.floor(t/1000);
	if (tSecs < 60) return tSecs + "s";
	if (tSecs < 120) return Math.floor(tSecs / 60) + "m " + (tSecs % 60) + "s";
	if (tSecs < 3600) return Math.floor(tSecs / 60) + "m";
	if (tSecs < 7200) "1h " + Math.floor(tSecs / 60) + "m";
	return (tSecs / 3600) + "h";
}

function agedColour(t, maxT) {
	// fade from green to red over maxT ts
	var colFrac;
	if ( t > maxT) colFrac = 255;
	else colFrac = Math.floor(t/maxT * 255);
	
	return "rgb(" + colFrac + "," + (255 - colFrac) + ",0)";
}

function updateCurrent(data) {

	var dom = $("#current")
	dom.empty();

	$.each(data.nodes, function(nodeid, node) {
	
		var time = Date.now() - node.lastReadingTime;
		
		var nodeDiv = $("<div/>", {
			"class": "node-current"
		});

		$("<span/>", {
			"class": "nodeid",
			"style": "background-color: " + agedColour(time, 300000),
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

		$("<span/>", {
			"class": "readingtime",
			"html" : readableTime(time) + " ago"
		}).appendTo(nodeDiv);

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

			function renderReading(reading, index) {
				var height = Math.floor(reading / divisor);
				var bgColour = agedColour(index, data.maxReadings);
				
				$("<div/>", {
					"class": "reading",
					"title": reading + " " + unit,
					"style": "height: " + height + "px" + "; background-color: " + bgColour,
					"html":  "<span>" + reading + "</span>"
				}).appendTo(sensorDiv);
			}

			var count = 0;
			for (var index= sensor.lastReading; sensor.readings[index]; index--) {
				renderReading(sensor.readings[index], count++);
			}

			for (var index= sensor.readings.length - 1; index > sensor.lastReading; index--) {
				renderReading(sensor.readings[index], count++);
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
