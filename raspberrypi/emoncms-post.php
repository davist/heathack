<?php

$fp =fopen("/dev/ttyUSB0", "r");

if( !$fp) {
        echo "Error";die();
}

while (1) {
	$line = fgets($fp);

	$args = explode(" ", $line);

	if ($args[0] == "heathack") {
	
		$node = $args[1];
		
		$temp = $args[4];
		$humidity = $args[7];
		
//		echo "node $node: temp $temp, hum $humidity\n";
		
		$emon = fopen("http://emoncms.org/input/post.json?node=$node&json={temperature:$temp,humidity:$humidity}&apikey=ee64225c5bb56a053222f8d33f153084", "r");
		stream_set_timeout($emon, 10);
//		if (!feof($emon)) {
//			echo fgets($emon, 50);
//			echo "\n";
//		}
		fclose($emon);
	}
}

?>
