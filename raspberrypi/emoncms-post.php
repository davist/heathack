<?php

$sensorTypeNames = array( "undefined", "temperature", "humidity", "light", "movement", "pressure", "sound", "lowbatt" );


$fp =fopen("/dev/ttyUSB0", "r");

if( !$fp) {
        echo "Error";die();
}

while (1) {
	$line = fgets($fp);

	$args = explode(" ", $line);

	if ($args[0] == "heathack") {
	
		$node = $args[1];

        $json = "";
		
        for ($i=2; $i < (count($args) - 2); $i+=3) {
        
			$sensorid = $args[$i];
			$type = $args[$i+1];
			$value = $args[$i+2];
        
            if (strlen($json) > 0) $json .= ",";
            $json .= $sensorTypeNames[$type] . $sensorid . ":" . $value;
        }
        
//		echo "http://emoncms.org/input/post.json?node=$node&json={{$json}}&apikey=ee64225c5bb56a053222f8d33f153084\n";

		$emon = fopen("http://emoncms.org/input/post.json?node=$node&json={{$json}}&apikey=ee64225c5bb56a053222f8d33f153084", "r");

        //		stream_set_timeout($emon, 10);
//		if (!feof($emon)) {
//			echo fgets($emon, 50);
//			echo "\n";
//		}

//        if ($emon) fclose($emon);
	}
}

?>
