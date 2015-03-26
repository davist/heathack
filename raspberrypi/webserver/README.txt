HeatHack webserver and publisher for the JeeNode receiver.

Expects a JeeNode receiver connected to a USB port (it assumes the port will be /dev/ttyUSB0 and the baud rate 9600).
Also needs a network connection for publishing to EmonCMS.

1. Install NodeJS. The packaged version in the Raspbian repository is out of date, so download from http://node-arm.herokuapp.com/
Instructions for downloading and installing on the Pi are on the web page. Currently tested to work with version 0.12.0.

2. From within this directory run 'npm install' to install the required packages.

3. Copy default-config.js to config.js

2. Edit config.js and add your EmonCMS Write API key to config.publish_settings.apikey.

3. If not using the emoncms.org website, change config.publish_settings.server to point to your server.

4. run './run.sh' to start the server. "serial port open" indicates it's up and running. "Error: Cannot open /dev/ttyUSB0" means it can't connect to the JeeNode.

5. In a web browser on another machine, enter the hostname or IP address for the Pi and you should see the HeatHack page. It will automatically update as readings come in.
