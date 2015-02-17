HeatHack webserver and publisher for the JeeNode receiver.

Expects a JeeNode receiver connected to a USB port (it assumes the port will be /dev/ttyUSB0).
Also needs a network connection for publishing to EmonCMS.

1. Copy default-config.js to config.js

2. Edit the file and add your API key to config.publish_settings.apikey.

3. If not using emoncms.org, change config.publish_settings.server to point to your server.

4. run run.sh to start the server.

5. In a web browser on another machine, enter the hostname or IP address for the Pi and you should see the HeatHack page.
