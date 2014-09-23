#define MAX_TEMP_VALUE 500
#define MAX_HUM_VALUE 1000

#define GAUGE_TEMP_R 255
#define GAUGE_TEMP_G 0
#define GAUGE_TEMP_B 0

#define GAUGE_HUM_R 0
#define GAUGE_HUM_G 100
#define GAUGE_HUM_B 255

#define GLINT_TEMP_R 255
#define GLINT_TEMP_G 255
#define GLINT_TEMP_B 0

#define GLINT_HUM_R 255
#define GLINT_HUM_G 255
#define GLINT_HUM_B 255

class Gauge {
 
  static PrecipConfig config;
  Accumulation pile;

public:

  Gauge() {
    pile.config = &config;
  }

  void run(uint16_t value, bool isTemp) {
    
    pile.setTarget( (uint32_t)value * NUM_LEDS * HEIGHT_PER_LED / (isTemp ? MAX_TEMP_VALUE : MAX_HUM_VALUE) );
    
    if (isTemp) {
      config.pileColour.r = GAUGE_TEMP_R;
      config.pileColour.g = GAUGE_TEMP_G;
      config.pileColour.b = GAUGE_TEMP_B;
      config.glintColour.r = GLINT_TEMP_R;
      config.glintColour.g = GLINT_TEMP_G;
      config.glintColour.b = GLINT_TEMP_B;
    }
    else {
      config.pileColour.r = GAUGE_HUM_R;
      config.pileColour.g = GAUGE_HUM_G;
      config.pileColour.b = GAUGE_HUM_B;
      config.glintColour.r = GLINT_HUM_R;
      config.glintColour.g = GLINT_HUM_G;
      config.glintColour.b = GLINT_HUM_B;
    }
    
    pile.adjust();
    uint8_t pileEnd = pile.render();
  
    // render background
    for (uint8_t i=pileEnd + 1; i < NUM_LEDS; i++) {
        leds[i].setRGB(config.bgColour.r,
                       config.bgColour.g, 
                       config.bgColour.b);
    }
  }   
};

PrecipConfig Gauge::config = {
  {0, 0, 0},   // pileColour
  {0, 0, 0},       // bgColour
  {0, 0, 0}, // dropletColour
  {0, 0, 0}, // glintColour
  
  0, // nextDropletTrigger
  
  20, // glintStep;
  30, // glintChance;
  
  true, // pileGrowViaDroplets
  1, // pileMeltRate;
  1, // pileGrowRate;
  0, // pileDropletIncrement;
  0, // pileSplashSpeed;
  5, // pileWaveHeight;
  5, // pileWaveFreq;
  0, // pileWaveSplashHeight;
  0, // pileWaveSplashFreq;
  0, // pileWaveStep;

  
  0, // dropletInitSpeed;
  0, // dropletAcceleration;
  0, // dropletShininess;
  0, // dropletRotationFast;
  0, // dropletRotationSlow;
  0, // dropletValueRange (255 - dropletColour.v)
  0 // dropletMinBrightness
};

