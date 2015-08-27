#!/bin/bash
# speak-ip
# usage: speak-ip <interface name, eg wlan0>
# 

AUDIO_DIR=voxeo-audio
#AUDIO_DIR=mstts-audio

if [ -z "$1" ]; then
	echo "usage: $0 <interface name, eg wlan0>"
	exit 1
fi

IP=`ifconfig $1 | grep "inet addr" | awk -F: '{print $2}' | awk '{print $1}'`

if [ -z "$IP" ]; then
	# No address found
	exit 0
fi

DIR=`dirname $0`

# repeat 10 times
for (( loop=10; loop>0; loop-- )); do
	for (( i=0; i<${#IP}; i++ )); do
	  CH=${IP:$i:1}
	  case $CH in
	  0|1|2|3|4|5|6|7|8|9)
		aplay -q $DIR/$AUDIO_DIR/$CH.wav
		;;
	  .)
		aplay -q $DIR/$AUDIO_DIR/point.wav
		;;
	  esac
	done

	# delay between repetitions
	sleep 5
done
