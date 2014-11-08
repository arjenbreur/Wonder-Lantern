/*
Wonder Lantern for Arduino and Adafruit NeoPixel LEDs.

This script is based off of Adafruit's Ampli-tie project:
https://github.com/adafruit/LED-Ampli-Tie-with-Flora

Hardware requirements:
 - Most Arduino or Arduino-compatible boards (ATmega 328P or better).
 - Adafruit Electret Microphone Amplifier (ID: 1063)
 - Adafruit Flora RGB Smart Pixels (ID: 1260)
   OR
 - Adafruit NeoPixel Digital LED strip (ID: 1138)
 - Optional: battery for portable use (else power through USB or adapter)
Software requirements:
 - Adafruit NeoPixel library

Connections:
 - 3.3V to mic amp +
 - GND to mic amp -
 - Analog pin to microphone output (configurable below)
 - Digital pin to LED data input (configurable below)
 See notes in setup() regarding 5V vs. 3.3V boards - there may be an
 extra connection to make and one line of code to enable or disable.

Written by Adafruit Industries.  Distributed under the BSD license.
Rewritten by Eriestuff, to add twinkle effect.
This paragraph must be included in any redistribution.
*/

#include <Adafruit_NeoPixel.h>

#define N_PIXELS   4  // Number of pixels in strand
#define MIC_PIN   A9  // Microphone is attached to this analog pin
#define LED_PIN    6  // NeoPixel LED strand is connected to this pin
#define DC_OFFSET  0  // DC offset in mic signal - if unusure, leave 0
#define NOISE      9  // Noise/hum/interference in mic signal
#define SAMPLES   40  // Length of buffer for dynamic level adjustment

byte peak      = 0;   // Used for falling dot
byte dotCount  = 0;   // Frame counter for delaying dot-falling speed
byte volCount  = 0;   // Frame counter for storing past volume data
int vol[SAMPLES];     // Collection of prior volume samples, used to take averages
int lvl       = 10;   // Current "dampened" audio level
int minLvlAvg = 0;    // For dynamic adjustment of graph low & high
int maxLvlAvg = 512;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Setup neo pixel twinkles
float fadeMillis = 3000;      // duration of twinkle fade
int   fadeLockMillis = 2000;  // don't overwrite a fading led whitin this amount of milis after is was lit. This preserves the twinkle effect even when there is continuous noise.
float redStates[N_PIXELS];    // these arrays keep track of the color value of each neopixel
float blueStates[N_PIXELS];   //
float greenStates[N_PIXELS];  //
long  timestamps[N_PIXELS];   // this array keeps track of the twinkle fading period of each neo pixel

// twinkle on idle mode
int twinkleWhileIdleBootThresholdLevel = 150;  // when the average microphone reading during the boot cycle is above this value, twinkle_while_idle mode is turned on
int twinkleRate = 400;                         // higher values yield more twinkles
boolean twinkleWhileIdle = false;              // set to true when twinkle_while_idle mode should ALWAYS be on, regarless of the boot cycle microphone input

void setup() {
  Serial.begin(9600);
  // This is only needed on 5V Arduinos (Uno, Leonardo, etc.).
  // Connect 3.3V to mic AND TO AREF ON ARDUINO and enable this
  // line.  Audio samples are 'cleaner' ats 3.3V.
  // COMMENT OUT THIS LINE FOR 3.3V ARDUINOS (FLORA, ETC.):
  // analogReference(EXTERNAL);

  memset(vol, 0, sizeof(vol));
  strip.begin();

  // boot delay loop
  // if the avegate sound level during this loop is high, 
  // the 'twinkle_while_idle' mode is turned on.
  // So, to turn on this mode, you should generate noise 
  // during boot, by tapping the microphone for example.
  int total = 0;
  int bootcycles = 200;
  for(int bootcycle = 0; bootcycle < bootcycles; bootcycle++){
     total += getMicReading();
     delay(10);
  }
  twinkleWhileIdle = (total/bootcycles > twinkleWhileIdleBootThresholdLevel)? true: false;

  // Initialize all pixels to 'green' as a signal that boot is complete
  // set all color values to 0 to remove this flash at startup
  for(uint16_t l = 0; l < strip.numPixels(); l++) {
    redStates[l]   = 0;
    greenStates[l] = 100;
    blueStates[l]  = 0;
    timestamps[l]  = millis();
  }

}


void loop() {
  uint16_t minLvl, maxLvl;
  int n, height;

  twinkle_tick();                    // adjust all NeoPixels to their new brightness
 
  n = getMicReading();
  storeSample(n);                    // store this sound level sample, to take averages later
  lvl = levelMicReading(n);          // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point)
  height = calcBarHeight(minLvlAvg, maxLvlAvg);

  if(height == 0){
    // twinkle on idle
    if(twinkleWhileIdle==true && random(twinkleRate) == 1) {
      int num_pixels_to_twinkle = random(1, N_PIXELS);
      for(uint8_t px = 0; px < num_pixels_to_twinkle; px++){
        int lockMillis = fadeMillis;
        twinkle_start(px, lockMillis); // twinkle in random color, and don't overwrite this twinkle before its completely done.
      }
    }
  }else{    
    // light up the pixels
    // set same random color for all pixels
    int r = random(255);
    int g = random(255);
    int b = random(255);
    for(uint8_t px=0; px<N_PIXELS; px++) {
      if(px < height){
        twinkle_start(px, r, g, b, fadeLockMillis);
      }
    }
  }
  // update the pixels
  strip.show(); // Update strip

  // Get volume range of prior frames
  // these min and max levels will be used in the next cycle, to dynamicaly adjust the threshold to the current average sound level.
  minLvl = maxLvl = vol[0];
  for(int s=1; s<SAMPLES; s++) {
    if(vol[s] < minLvl)      minLvl = vol[s];
    else if(vol[s] > maxLvl) maxLvl = vol[s];
  }
  // minLvl and maxLvl indicate the volume range over prior frames, used
  // for vertically scaling the output graph (so it looks interesting
  // regardless of volume level).  If they're too close together though
  // (e.g. at very low volume levels) the graph becomes super coarse
  // and 'jumpy'...so keep some minimum distance between them (this
  // also lets the graph go to zero when no sound is playing):
  if((maxLvl - minLvl) < N_PIXELS) maxLvl = minLvl + N_PIXELS;
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6; // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6; // (fake rolling average)

}


int getMicReading(){
  int n;
  n   = analogRead(MIC_PIN);              // Raw reading from mic 
  n   = abs(n - 512 - DC_OFFSET);         // Center on zero
  n   = (n <= NOISE) ? 0 : (n - NOISE);   // Remove noise/hum
  return n;
}

int levelMicReading(int n){
  lvl = ((lvl * 7) + n) >> 3;             // "Dampened" reading (else looks twitchy)
  return lvl;
}

int calcBarHeight(int minLvlAvg, int maxLvlAvg){
  int height = N_PIXELS * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);   
  if(height < 0L)            height = 0;      // Clip output
  else if(height > N_PIXELS) height = N_PIXELS;
  return height;
}

void storeSample(int n){
  vol[volCount] = n;                      // Save sample for dynamic leveling
  if(++volCount >= SAMPLES) volCount = 0; // Advance/rollover sample counter
}

void twinkle_start(uint16_t pixel){
  twinkle_start(pixel, random(256), random(256), random(256));
}
void twinkle_start(uint16_t pixel, int lockMillis){
  twinkle_start(pixel, random(256), random(256), random(256), lockMillis);
}
void twinkle_start(uint16_t pixel, int r, int g, int b, int lockMillis){
  // dont overwrite the neopixel if it is still in its locked fading stage
  if(timestamps[pixel] + lockMillis < millis() ) {
    twinkle_start(pixel, r, g, b);
  }
}
void twinkle_start(uint16_t pixel, int r, int g, int b){
  redStates[pixel] = r;
  greenStates[pixel] = g;
  blueStates[pixel] = b;
  timestamps[pixel] = millis(); // reset the timestamp for this pixel
}



void twinkle_tick(){
    long now = millis();
  
    for(uint16_t l = 0; l < N_PIXELS; l++) {
      if (redStates[l] > 1 || greenStates[l] > 1 || blueStates[l] > 1) {
        strip.setPixelColor(l, redStates[l], greenStates[l], blueStates[l]);

        long diff =  now - timestamps[l];
        float fraction = diff/fadeMillis;
        float fadeRate = 1-fraction;
        
        if (redStates[l] > 1) {
          redStates[l] = redStates[l] * fadeRate;
        } else {
          redStates[l] = 0;
        }
        
        if (greenStates[l] > 1) {
          greenStates[l] = greenStates[l] * fadeRate;
        } else {
          greenStates[l] = 0;
        }
        
        if (blueStates[l] > 1) {
          blueStates[l] = blueStates[l] * fadeRate;
        } else {
          blueStates[l] = 0;
        }
        
      } else {
        strip.setPixelColor(l, 0, 0, 0);
      }
    }
}
