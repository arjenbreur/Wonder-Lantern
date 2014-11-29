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

#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Adafruit_NeoPixel.h>

#define N_PIXELS   4  // Number of pixels in strand
#define MIC_PIN   A9  // Microphone is attached to this analog pin
#define LED_PIN    6  // NeoPixel LED strand is connected to this pin
#define DC_OFFSET  0  // DC offset in mic signal - if unusure, leave 0
#define NOISE      9  // Noise/hum/interference in mic signal
#define SAMPLES   40  // Length of buffer for dynamic level adjustment

// Twinkle settings 
float   fadeMillis       =  1000; // duration of twinkle fade
int     fadeLockMillis   =   500; // withing this amount of millis, started twinkles will not be overwritten by new twinkles
int     twinkleRate      = 60000; // random twinkle will happen once every this many millis (when in twinkle_while_idle mode), so higher values yield twinkles LESS often
boolean twinkleWhileIdle = false; // true: twinkle_while_idle mode is always on. false: mode depends on mic input during boot cycle
int     twinkleWhileIdleBootThresholdLevel = 150;  // when the average microphone reading during the boot cycle is above this value, twinkle_while_idle mode is turned on


// init variables
byte peak      = 0;   // Used for falling dot
byte dotCount  = 0;   // Frame counter for delaying dot-falling speed
byte volCount  = 0;   // Frame counter for storing past volume data
int  vol[SAMPLES];    // Collection of prior volume samples, used to take averages
int  lvl       = 10;  // Current "dampened" audio level
int  minLvlAvg = 0;   // For dynamic adjustment of graph low & high
int  maxLvlAvg = 512;

// init pixels
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
float redStates[N_PIXELS];      // these arrays keep track of the color value of each neopixel, while fading
float blueStates[N_PIXELS];     //
float greenStates[N_PIXELS];    //
float redOriginal[N_PIXELS];    // these arrays keep track of the original color each neopixel, before fading
float blueOriginal[N_PIXELS];   //
float greenOriginal[N_PIXELS];  //
unsigned long timestamps[N_PIXELS]; // this array keeps track of the start time of a twinkle fade for each pixel

int num_pix_to_light = 0;
int timestamp_next_pix_to_light = 0;
int next_pix_to_light = -1;
int r,g,b;

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);


void setup() {
  Serial.begin(9600);
  // This is only needed on 5V Arduinos (Uno, Leonardo, etc.).
  // Connect 3.3V to mic AND TO AREF ON ARDUINO and enable this
  // line.  Audio samples are 'cleaner' ats 3.3V.
  // COMMENT OUT THIS LINE FOR 3.3V ARDUINOS (FLORA, ETC.):
  // analogReference(EXTERNAL);

  if (tcs.begin()) {
    tcs.setInterrupt(true);  // turn off LED
    Serial.println("Found sensor");
  } else {
    Serial.println("No TCS34725 found ... check your connections");
  }


  memset(vol, 0, sizeof(vol));
  memset(timestamps, 0, sizeof(timestamps));
  pixels.begin();

  // boot delay loop
  // if the avegate sound level during this loop is high, 
  // the 'twinkle_while_idle' mode is turned on.
  // So, to turn on this mode, you should generate noise 
  // during boot, by tapping the microphone for example.
  int average = 0;
  long bootEnd = millis() + 2000;
  int bootCycle = 0;
  while( millis() < bootEnd ){
     average = ((average * bootCycle) + getMicReading() ) / ++bootCycle ;
  }
  twinkleWhileIdle = (average > twinkleWhileIdleBootThresholdLevel)? true: twinkleWhileIdle;

  // Initialize all pixels to 'green' as a signal that boot is complete
  // note: to remove this "flash" at startup, set all color values to 0
  for(uint8_t px = 0; px < pixels.numPixels(); px++) {
    twinkle_init(px, 0, 100, 0);
  }

}


void loop() {
  uint16_t minLvl, maxLvl;
  int n, height;

  n = getMicReading();               // normalized mic reading (removing noise)
  storeSample(n);                    // store this sound level sample, to take averages later
  lvl = levelMicReading(n);          // "Dampened" reading (else looks twitchy)

  // Calculate bar height based on dynamic min/max levels (fixed point)
  height = calcBarHeight(minLvlAvg, maxLvlAvg);
  
  //TODO: debug set to 0
  height = 0;

  if(height == 0){
    // twinkle a random pixel, when idle (sound level is too low to light up pixels).
//    if(twinkleWhileIdle==true && random(twinkleRate) == 1){
      if(random(500) == 1){
        r = random(255);
        g = random(5);
        b = random(5);
        num_pix_to_light = random(0, N_PIXELS);
        next_pix_to_light = 0;
        timestamp_next_pix_to_light = abs(millis() + 200);
      }


      Serial.print("millis=");Serial.print(millis());Serial.print("; ");
      Serial.print("num=");Serial.print(num_pix_to_light);Serial.print("; ");
      Serial.print("pix=");Serial.print(next_pix_to_light);Serial.print("; ");
      Serial.print("timestamp=");Serial.print(timestamp_next_pix_to_light);Serial.print("; ");
      Serial.println(";");      

      if(num_pix_to_light > -1 && timestamp_next_pix_to_light < millis()){
        twinkle_init(next_pix_to_light, r, g, b); // twinkle in random color, and don't overwrite this twinkle before its completely done.
        if(next_pix_to_light <= num_pix_to_light){
          next_pix_to_light = next_pix_to_light + 1;
        }else{
          num_pix_to_light = 0;
          timestamp_next_pix_to_light = 0;
          next_pix_to_light = -1;
        }
        delay(1000);
      }
//    }
  }else{
    // not idle, current mic level dictates pixels to be lit up.
    // set new color for the pixels, use same random color for all pixels, for this event
    int r = random(255);
    int g = random(5);
    int b = random(5);
    for(uint8_t px=0; px <= height; px++) {
      // only twinkle this pixel, if it is not still locked by a previous twinkle
      long twinkleEnd = timestamps[px] + fadeLockMillis;
      if( millis() > twinkleEnd ){ 
        twinkle_init(px, r, g, b);
      }
    }
  }

  // update the pixels
  twinkle_tick();  // adjust all NeoPixels color settings to their new brightness
  pixels.show();    // send the new pixel settings to the string of NeoPixels


  // Get volume range of prior frames
  // these min and max levels will be used in the next cycle,
  // to dynamicaly adjust the threshold to the current average sound level.
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

void twinkle_init(uint16_t pixel){
  twinkle_init(pixel, random(256), random(256), random(256));
}

void twinkle_init(uint16_t pixel, int r, int g, int b){
  redOriginal[pixel]   = r;
  greenOriginal[pixel] = g;
  blueOriginal[pixel]  = b;
  redStates[pixel] = r;
  greenStates[pixel] = g;
  blueStates[pixel] = b;
  timestamps[pixel] = millis(); // set the start timestamp for this pixel
}


/*
* Heartbeat method for twinkle effect
* This method should be called every loop cycle.
* It updates the LED color states, taking the fade duration setting into account,
* and the time that has passed since the previous cycle.
*/
void twinkle_tick(){
    unsigned long now = millis();

    for(uint16_t px = 0; px < N_PIXELS; px++) {
      if (redStates[px] > 1 || greenStates[px] > 1 || blueStates[px] > 1) {
        // set this pixel to the color that is stored in the color arrays
        pixels.setPixelColor(px, redStates[px], greenStates[px], blueStates[px]);

        // TODO: bugfix:
        // for some unclear reason, the timestamp is sometimes (much) greater than 'now', causing problematic values for diff, fraction and rate.
        // this quick fix makes sure the timestamp is alwasy in the past:
        if(timestamps[px] > now ){
          timestamps[px] = now - 222; // timestamp in array is corrupted, set to arbitrary time in the past
        }
        long diff = now - timestamps[px]; // time since pixel color was initiated
        float fraction = diff/fadeMillis; // fraction of the fade period that has passed
        float fadeRate = 1-fraction;      // rate at which to fade this pixel in this cycle (tick)
        
        // in case the fadeRate has an invalid value (due to corrupted values)
        // set the rate to a valid value
        if (fadeRate>=1){
          fadeRate = 0.5;
        }
        
        // fade the pixel       
        redStates[px]   = twinkleFade(redOriginal[px],   redStates[px],   fadeRate);
        greenStates[px] = twinkleFade(greenOriginal[px], greenStates[px], fadeRate);
        blueStates[px]  = twinkleFade(blueOriginal[px],  blueStates[px],  fadeRate);

//        // Debug code
//        Serial.print("px=");Serial.print(px);Serial.print("; ");
//        Serial.print("");Serial.print(redStates[px]);Serial.print("");
//        Serial.print("");Serial.print(greenStates[px]);Serial.print("");
//        Serial.print("");Serial.print(blueStates[px]);Serial.print("; ");
//        Serial.print("");Serial.print(now);Serial.print("-");
//        Serial.print("");Serial.print(timestamp);Serial.print("=");
//        Serial.print("");Serial.print(diff);Serial.print("; ");
//        Serial.print("fraction=");Serial.print(fraction);Serial.print("; ");
//        Serial.print("rate=");Serial.print(fadeRate);Serial.print("; ");
//        Serial.println(";");
        
      } else {
        // turn pixel off
        pixels.setPixelColor(px, 0, 0, 0);
      }
      
    }
}

int twinkleFade(int originalColor, int currentColor, float fadeRate){
  int newColor = 0;
  if (currentColor > 1) {
    newColor = originalColor * fadeRate;
  }
  return newColor;
}

