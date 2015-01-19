<?php

// API key for EmonCMS
$API_KEY = "ee64225c5bb56a053222f8d33f153084";

// names and ranges for sensor types in numerial order from zero.
// min and max = 0 means allow all values
$sensorTypes = array(
    array("name" => "undefined",   "min" => 0,   "max" => 0),
    array("name" => "temperature", "min" => -20, "max" => 50),
    array("name" => "humidity",    "min" => 0,   "max" => 100),
    array("name" => "light",       "min" => 0,   "max" => 255),
    array("name" => "movement",    "min" => 0,   "max" => 0),
    array("name" => "pressure",    "min" => 0,   "max" => 0),
    array("name" => "sound",       "min" => 0,   "max" => 0),
    array("name" => "lowbatt",     "min" => 0,   "max" => 0),
);

// open serial port
$fp =fopen("/dev/ttyUSB0", "r");

// check it worked
if( !$fp) {
        echo "Error";die();
}

// loop forever
while (1) {
    // read line of text from serial port
	$line = fgets($fp);

// debugging - print data received from JeeLink    
//    echo "$line\n";
    
    // split into arguments on spaces
	$args = explode(" ", $line);

    // check first argument is "heathack" to ensure data isn't garbage
	if ($args[0] == "heathack") {
	
        // node id is next argument 
		$node = $args[1];

        $json = "";
		
        // remaining args are triplets of sensor id, sensor type, reading
        for ($i=2; $i < (count($args) - 2); $i+=3) {
        
			$sensorid = $args[$i];
			$type = $args[$i+1];
			$reading = $args[$i+2];

            // sanity check the reading and ignore if out of range
            if ( !($sensorTypes[$type]["min"] == 0 && $sensorTypes[$type]["max"] == 0) ) {

                if ($reading >= $sensorTypes[$type]["min"] &&
                    $reading <= $sensorTypes[$type]["max"]) {

                    // combine readings into JSON-formatted data for posting to emoncms.
                    // format is {type}{id}:{reading}, e.g. temperature1:23.0
                    if (strlen($json) > 0) $json .= ",";
                    $json .= $sensorTypes[$type]["name"] . $sensorid . ":" . $reading;
                }
            }
            
        }

// debugging - print url
//		echo "http://emoncms.org/input/post.json?node=$node&json={{$json}}&apikey=$API_KEY\n";

        // post the data to emoncms
		$emon = fopen("http://emoncms.org/input/post.json?node=$node&json={{$json}}&apikey=$API_KEY", "r");

// debugging - print response from server
//		if (!feof($emon)) {
//			echo fgets($emon, 50);
//			echo "\n";
//		}

        if ($emon) fclose($emon);
	}
}

?>
