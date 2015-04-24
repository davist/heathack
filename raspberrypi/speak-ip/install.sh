#!/bin/bash

# install if-up script to announce IP address every time a network interface starts up

DEST_DIR=/etc/network/if-up.d
DEST_FILE=${DEST_DIR}/speak-ip
CUR_DIR=`pwd`

# check destination directory exists
if [[ ! -e $DEST_DIR || ! -d $DEST_DIR ]]; then
	echo "destination directory $DEST_DIR doesn't exist or is not a directory"
	exit 0
fi

# check script isn't already installed
OVERWRITE=y
if [[ -e $DEST_FILE ]]; then
	read -p "if-up script is already installed. Overwrite? (y/n) " OVERWRITE
fi

if [[ $OVERWRITE == "y" || $OVERWRITE == "Y" ]]; then
	echo "Installing if-up script as $DEST_FILE"
	sudo cp ./ifup-script $DEST_FILE
	sudo sed -i "s#{{INSTALL_DIR}}#$CUR_DIR#g" $DEST_FILE
	sudo chown root:root $DEST_FILE
	sudo chmod a+rx $DEST_FILE
fi
