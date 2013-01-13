#ifndef COMMON_H
#define COMMON_H

// number of LED strips
const int LEFT     = 6;
const int TOP      = 8;
const int RIGHT    = 6;

const int CHANNELS = LEFT + TOP + RIGHT;

const float GAMMA = 2.0;

const long FADE_OUT_DELAY     = 4000;  // 2s
const long FADE_OUT_THRESHOLD = 50;    // fade out only when all colors are < FADE_OUT_THRESHOLD


// android <-> arduino codes
const byte MODE             = 1;
const byte SAVE_SETTINGS    = 5;
const byte LIGHT_LEVEL      = 10;
const byte RED_COLOR        = 20;
const byte GREEN_COLOR      = 21;
const byte BLUE_COLOR       = 22;
const byte UPLOAD_SETTINGS  = 200;


// mode constants
const byte BOBLIGHT         = 10;
const byte CONSTANT_COLOR   = 20;
const byte MODE_OFF         = 30;
const byte DEMO             = 40;


// EEPROM addresses
const int MODE_ADDR          = 1;
const int LIGHT_LEVEL_ADDR   = 2;
const int RED_COLOR_ADDR     = 3;
const int GREEN_COLOR_ADDR   = 4;
const int BLUE_COLOR_ADDR    = 5;

#endif
