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
#define LED_PIN    6  // NeoPixel LED strand is connected to this pin

// Twinkle settings 
float   fadeMillis       =  500; // duration of twinkle fade
//int     fadeLockMillis   =  200; // withing this amount of millis, started twinkles will not be overwritten by new twinkles
//int     twinkleRate      = 7500; // random twinkle will happen once every this many millis (when in twinkle_while_idle mode), so higher values yield twinkles LESS often

// init pixels
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
float redStates[N_PIXELS];      // these arrays keep track of the color value of each neopixel, while fading
float blueStates[N_PIXELS];     //
float greenStates[N_PIXELS];    //
float redOriginal[N_PIXELS];    // these arrays keep track of the original color each neopixel, before fading
float blueOriginal[N_PIXELS];   //
float greenOriginal[N_PIXELS];  //
unsigned long timestamps[N_PIXELS]; // this array keeps track of the start time of a twinkle fade for each pixel

unsigned long loop_until;




void setup() {
  Serial.begin(9600);
  // This is only needed on 5V Arduinos (Uno, Leonardo, etc.).
  // Connect 3.3V to mic AND TO AREF ON ARDUINO and enable this
  // line.  Audio samples are 'cleaner' ats 3.3V.
  // COMMENT OUT THIS LINE FOR 3.3V ARDUINOS (FLORA, ETC.):
  // analogReference(EXTERNAL);

  memset(timestamps, 0, sizeof(timestamps));
  pixels.begin();
  
//  pixels.setPixelColor(0, 255, 0, 0);
//  pixels.setPixelColor(1, 0, 255, 0);
//  pixels.setPixelColor(2, 0, 0, 255);
//  pixels.setPixelColor(3, 255, 255, 255);

//  pixels.setPixelColor(0, 0, 0, 0);
//  pixels.setPixelColor(1, 0, 0, 0);
//  pixels.setPixelColor(2, 0, 0, 0);
//  pixels.setPixelColor(3, 255, 255, 255);

  pixels.show();    // send the new pixel settings to the string of NeoPixels
  
  loop_until = millis() + 1000;
}

void loop() {

  if( (long)( millis() - loop_until ) >= 0){
    loop_until = millis() + random(100, 1000);
    throw_flame();
  }
 
  // update the pixels
  flame_tick();     // adjust all NeoPixels color settings to their new brightness
  pixels.show();    // send the new pixel settings to the string of NeoPixels
  
  delay(10); // give the processor some rest, we don't need higher clock rate
}

void throw_flame(){
  int flame_height = random(2,N_PIXELS+1);
  for(int i=0;i<flame_height;i++){
    int inv_i = constrain(flame_height - i,0,N_PIXELS);
    int r = random(200,255);
    int g = constrain(random((inv_i*55),100),0,r); // constrain to maximum of r value
    int b = random(0,0);
    flame_pixel_init(i,r,g,b);
  }
}

void flame_pixel_init(uint16_t pixel, int r, int g, int b){
  redOriginal[pixel]   = r;
  greenOriginal[pixel] = g;
  blueOriginal[pixel]  = b;
  redStates[pixel] = r;
  greenStates[pixel] = g;
  blueStates[pixel] = b;
  timestamps[pixel] = millis() + (random(50,150)*pixel); // set the start timestamp for this pixel
}

void flame_tick(){
  unsigned long now = millis();

  for(uint16_t px = 0; px < N_PIXELS; px++) {
    if( (long)( now - timestamps[px] ) >= 0){
      // now is later than timestamp for this pixel
      // time to light the pixel, and start fading it!

      // light the pixel  
      pixels.setPixelColor(px, redStates[px], greenStates[px], blueStates[px]);

      // do fading math
      unsigned long fadeStart = timestamps[px];
      unsigned long fadeEnd = fadeStart + fadeMillis;
      unsigned long fadeDone = now - fadeStart;         // time since pixel color was initiated
      float fractionDone = (float) fadeDone/fadeMillis; // fraction of the fade period that has passed
      fractionDone = constrain(fractionDone, 0, 1);     // loop can overstep the exact fadeMillis, causing an invalid fraction.
      float fadeRate = 1-fractionDone;                  // rate at which to fade this pixel in this cycle (tick)
      
      // set the new color for the next fade step
      redStates[px]   = twinkleFade(redOriginal[px],   redStates[px],   fadeRate);
      greenStates[px] = twinkleFade(greenOriginal[px], greenStates[px], fadeRate);
      blueStates[px]  = twinkleFade(blueOriginal[px],  blueStates[px],  fadeRate);
  
      //      // Debug code
      //      Serial.print("px=");Serial.print(px);Serial.print("; ");
      ////      Serial.print("r");Serial.print(redStates[px]);Serial.print("");
      ////      Serial.print("g");Serial.print(greenStates[px]);Serial.print("");
      ////      Serial.print("b");Serial.print(blueStates[px]);Serial.print("; ");
      ////      Serial.print("");Serial.print(now);Serial.print("-");
      //      Serial.print("fadeDone=");Serial.print(fadeDone);Serial.print("; ");
      //      Serial.print("fraction=");Serial.print(fractionDone);Serial.print("; ");
      //      Serial.print("rate=");Serial.print(fadeRate);Serial.print("; ");
      //      Serial.println(";");

    }else{
      // now is not yet the set timestamp
      // lets wait till it's time to light the pixel
    }
  }
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

