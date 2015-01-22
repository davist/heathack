#include "HeatHack.h"

char* HHSensorTypeNames[] = {
	"Test",
	"Temperature",
	"Humidity",
	"Light",
	"Movement",
	"Pressure",
	"Sound",
    "Low Battery"
};

// Table indicating which sensor types send their reading as a literal integer
// and which send a decimal (to 1 decimal place), multiplied by 10 to convert to an int
bool HHSensorTypeIsInt[] = {
	true,
	false,
	false,
	true,
	true,
	false,
	false,
	true
};
