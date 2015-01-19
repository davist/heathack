#!/bin/bash

# Auto login for WiFi networks that redirect you to a login web page
# before allowing you to access the internet.

# Method:
# If connected to specified wifi network, check if internet is accessible
# and if not, perform a login to the wifi router.

# SSIDs (names) of relevant wifi networks
# Separate multiple names with commas
LOGIN_SSID="CEMC-WiFi,CMEC-PT"

# Network device. Typically wlan0 for wireless
NETWORK_DEV="wlan0"

# URL of a page on the internet to try downloading
TEST_URL="http://google.com"

# A word/phrase that can be found in the test page (but won't be found
# in the login page)
TEST_PHRASE="google"

# URL for the wifi network's login page
LOGIN_URL="http://aplogin.com"

# The data that needs POSTing to the login page to perform a login
# This will be the form fields from the login web page
LOGIN_POST_DATA="data1=heathack&data2=heathack&code=d&ip=xxIP_ADDRESSxx"


# Check required commands are available

IWGETID=/sbin/iwgetid
IP=/sbin/ip
WGET=/usr/bin/wget
SED=/bin/sed

if [[ ! -e $IWGETID ]]
then
	echo "$IWGETID not found" >&2
	exit -1;
fi

if [[ ! -e $IP ]]
then
	echo "$IP not found" >&2
	exit -1;
fi

if [[ ! -e $WGET ]]
then
	echo "$WGET not found" >&2
	exit -1;
fi

if [[ ! -e $SED ]]
then
	echo "$SED not found" >&2
	exit -1;
fi


# Get current network's SSID
CUR_SSID="$($IWGETID -r)"

if [[ -z $CUR_SSID ]]
then
	echo "Not on wifi" >&2
	exit 0
fi

# Is it one we need to login to? (does $LOGIN_SSID contain $CUR_SSID)
if [[ $LOGIN_SSID != *$CUR_SSID* ]]
then
	# not currently on the relevant wifi network
	echo "Not on relevant wifi network" >&2
	exit 0
fi

# grab the test page and see if it comes back correctly
TEST_PAGE_DATA=$($WGET -qO - $TEST_URL)

# check we got something
if [[ -z $TEST_PAGE_DATA ]]
then
	echo "failed to download anything for test page" >&2
	exit -1
fi

# test for the test phrase within the data
if [[ $TEST_PAGE_DATA == *$TEST_PHRASE* ]]
then
	# found the test phrase
	echo "Able to connect to the internet" >&2
	exit 0
fi

# So now we need to login to the wifi network

# is ip address required
if [[ $LOGIN_POST_DATA == *xxIP_ADDRESSxx* ]]
then
	IP_ADDRESS=$($IP addr show $NETWORK_DEV | awk '/inet / {print $2}' | cut -d/ -f1)
	
	# check we got ip address successfully
	if [[ -z $IP_ADDRESS ]]
	then
		echo "Failed to get ip address" >&2
		exit -1
	fi
	
	# replace placeholder with actual ip address
	SED_CMD="s/xxIP_ADDRESSxx/$IP_ADDRESS/g"
	LOGIN_POST_DATA=$(echo $LOGIN_POST_DATA | $SED $SED_CMD )
fi

# perform login
$WGET -qO /dev/null --post-data $LOGIN_POST_DATA $LOGIN_URL

# should now be able to access the internet!
echo "Logged in" >&2
exit 0
