var config = {};
config.publish_settings = {};

// set verbose to true to get each reading reported on the command line
// set to false for no output (e.g. if running in the background)
config.verbose = true;

// serial port the JeeNode receiver is connected to
config.serialport = "/dev/ttyUSB0";

// baud rate for connecting to JeeNode - default 9600
config.baudrate = 9600;

// name of publisher module to load for publishing readings to an external logging service
config.publisher = "emoncms";

// settings for the selected publisher module

// EmonCMS setttings

// server to publish to - default emoncms.org
config.publish_settings.server = "emoncms.org";

// API key for the account you're publishing to
config.publish_settings.apikey = "";

// Offset to add to node ids (e.g. to stop readings clashing with other data already on the server)
config.publish_settings.nodeid_offset = 0;


module.exports = config;
