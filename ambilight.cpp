#include <Arduino.h>
#include <EEPROM.h>
#include <Tlc5940.h>
#include <stdio.h>

#define NANO

#if defined NANO
    #include <SoftwareSerial.h>
    SoftwareSerial Software_serial(2, 4); // RX, TX
    #define bt_serial Software_serial
#else
    #define bt_serial Serial1
#endif

// functions
void strip_write_color_linear(int strip, int max_level, int red, int green, int blue);
void strip_write_color_with_correction(int strip, int max_level, int red, int green, int blue);
int get_fade_out_value(int current_color, int new_color, long last_modified, long current_time);
void fluorescent_lamp_animation();
void init_gamma_correction_table();
void save_settings(struct configuration config);

void (*strip_write_color)(int strip, int max_level, int red, int green, int blue);

// number of LED strips
const int LEFT     = 6;
const int TOP      = 8;
const int RIGHT    = 6;

const int CHANNELS = LEFT + TOP + RIGHT;

float GAMMA = 2.0;

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


struct configuration {
    int mode;
    int light_level; // = 1023;  // max intensity of LED stripes
    byte light_level_percent;
    byte red_color;
    byte green_color;
    byte blue_color;
};

// global variables
struct configuration config;

struct rgb {
    int red;
    int green;
    int blue;
};

struct channel_data {
    struct rgb value;
    struct rgb direction;
    long last_modified;
};

struct channel_data channels_data[CHANNELS];

int gamma_correction[256];


// FUNCTIONS

void save_settings(struct configuration config) {
    EEPROM.write(MODE_ADDR, config.mode);
    EEPROM.write(LIGHT_LEVEL_ADDR, config.light_level_percent);
    EEPROM.write(RED_COLOR_ADDR, config.red_color);
    EEPROM.write(GREEN_COLOR_ADDR, config.green_color);
    EEPROM.write(BLUE_COLOR_ADDR, config.blue_color);
}

void upload_settings(struct configuration config) {
    bt_serial.write(MODE);
    bt_serial.write(config.mode);
    bt_serial.write(LIGHT_LEVEL);
    bt_serial.write(config.light_level_percent);
    bt_serial.write(RED_COLOR);
    bt_serial.write(config.red_color);
    bt_serial.write(GREEN_COLOR);
    bt_serial.write(config.green_color);
    bt_serial.write(BLUE_COLOR);
    bt_serial.write(config.blue_color);
}

void strip_write_color_linear(int strip, int max_level, int red, int green, int blue) {
    Tlc.set(strip * 3, map(green, 0, 255, 0, max_level));
    Tlc.set(strip * 3 + 1, map(blue, 0, 255, 0, max_level));
    Tlc.set(strip * 3 + 2, map(red, 0, 255, 0, max_level));
}


void strip_write_color_with_correction(int strip, int max_level, int red, int green, int blue) {
    Tlc.set(strip * 3, map(gamma_correction[green], 0, 4096, 0, max_level));
    Tlc.set(strip * 3 + 1, map(gamma_correction[blue], 0, 4096, 0, max_level));
    Tlc.set(strip * 3 + 2, map(gamma_correction[red], 0, 4096, 0, max_level));
}


int get_fade_out_value(int current_color, int new_color, long last_modified, long current_time) {
    int tmp_color;

    if(new_color < current_color) {
        tmp_color = ((current_color - new_color) * (FADE_OUT_DELAY + last_modified - current_time)) / FADE_OUT_DELAY;
        new_color = (tmp_color < new_color) ? new_color : tmp_color;
    }
    return new_color;
}


void fluorescent_lamp_animation() {

    // black
    for(int i = 0; i < CHANNELS; i++) {
        strip_write_color(i, config.light_level, 0, 0, 0);
    }
    Tlc.update();

    // top
    for(int j = 0; j < 3; j++) {
        for(int i = LEFT; i < LEFT + TOP; i++) {
            strip_write_color(i, 2095, config.red_color, config.green_color, config.blue_color);
        }
        Tlc.update();
        delay(20);

        for(int i = LEFT; i < LEFT + TOP; i++) {
            strip_write_color(i, 4095, 0, 0, 0);
        }
        Tlc.update();
        delay(80);
    }

    strip_write_color(LEFT, config.light_level, config.red_color, config.green_color, config.blue_color);
    strip_write_color(LEFT + TOP - 1, config.light_level, config.red_color, config.green_color, config.blue_color);
    Tlc.update();
    delay(800);

    for(int i = LEFT; i < LEFT + TOP; i ++) {
        strip_write_color(i, config.light_level, config.red_color, config.green_color, config.blue_color);
        strip_write_color(LEFT + TOP + RIGHT - i, config.light_level, config.red_color, config.green_color, config.blue_color);
        Tlc.update();
        delay(10);
    }
    Tlc.update();

    //left + right
    for(int j = 0; j < 3; j++) {
        for(int i = 0; i < LEFT; i++) {
            strip_write_color(i, 2095, config.red_color, config.green_color, config.blue_color);
            strip_write_color(i + LEFT + TOP, 2095, config.red_color, config.green_color, config.blue_color);
        }
        Tlc.update();
        delay(20);

        for(int i = 0; i < LEFT; i++) {
            strip_write_color(i, 4095, 0, 0, 0);
            strip_write_color(i + LEFT + TOP, 4095, 0, 0, 0);
        }
        Tlc.update();
        delay(80);
    }

    strip_write_color(0, config.light_level, config.red_color, config.green_color, config.blue_color);
    strip_write_color(LEFT + TOP, config.light_level, config.red_color, config.green_color, config.blue_color);
    Tlc.update();
    delay(500);

    for(int i = 0; i < LEFT; i ++) {
        strip_write_color(i, config.light_level, config.red_color, config.green_color, config.blue_color);
        Tlc.update();
        delay(10);
    }
    delay(400);

    for(int i = 0; i < LEFT; i ++) {
        strip_write_color(i + LEFT + TOP, config.light_level, config.red_color, config.green_color, config.blue_color);
        Tlc.update();
        delay(10);
    }
}

void init_gamma_correction_table() {
    float base;

    for(int i = 0; i < 256; i++) {
        base = ((float)4095 / 256 * (i + 1)) / 4095;
        gamma_correction[i] = int(4096 * pow(base, GAMMA));
    }
}

unsigned long boblight(channel_data channels_data[]) {
    int red, green, blue;
    long current_time;
    static unsigned long last_received = millis();

    if(Serial.available() >= 3 * CHANNELS + 1) {
        if(Serial.read() == 0xFF) {
            current_time = millis();
            for(int i = 0; i < CHANNELS; i++) {
                red = Serial.read();
                green = Serial.read();
                blue = Serial.read();
        
/*                if(red < FADE_OUT_THRESHOLD && green < FADE_OUT_THRESHOLD && blue < FADE_OUT_THRESHOLD) {
                    red = get_fade_out_value(channels_data[i].value.red, red, channels_data[i].last_modified, current_time);
                    green = get_fade_out_value(channels_data[i].value.green, green, channels_data[i].last_modified, current_time);
                    blue = get_fade_out_value(channels_data[i].value.blue, blue, channels_data[i].last_modified, current_time);
                }

                channels_data[i].last_modified = current_time;

                channels_data[i].value.red = red;
                channels_data[i].value.green = green;
                channels_data[i].value.blue = blue; */
                
                strip_write_color(i, config.light_level, red, green, blue);
            }
            Tlc.update();
            last_received = millis();
        }
    }
    return last_received;
}

void one_color(int light_level, byte red_color, byte green_color, byte blue_color) {
    for(int i = 0; i < CHANNELS; i++) {
        strip_write_color(i, light_level, red_color, green_color, blue_color);
    }
    Tlc.update();
    delay(40);
}

void demo(channel_data channels_data[]) {
    for(int i = 0; i < CHANNELS; i++) {
        channels_data[i].value.red += channels_data[i].direction.red;
        channels_data[i].value.green += channels_data[i].direction.green;
        channels_data[i].value.blue += channels_data[i].direction.blue;
        channels_data[i].direction.red = random(0,20) == 0?random(-1,2):channels_data[i].direction.red;
        channels_data[i].direction.green = random(0,20) == 0?random(-1,2):channels_data[i].direction.green;
        channels_data[i].direction.blue = random(0,20) == 0?random(-1,2):channels_data[i].direction.blue;
        if(channels_data[i].value.red + channels_data[i].direction.red > 255 ||
                      channels_data[i].value.red + channels_data[i].direction.red < 0) {
            channels_data[i].direction.red = 0;

         }
        if(channels_data[i].value.green + channels_data[i].direction.green > 255 ||
                      channels_data[i].value.green + channels_data[i].direction.green < 0) {
            channels_data[i].direction.green = 0;
         }
        if(channels_data[i].value.blue + channels_data[i].direction.blue > 255 ||
                      channels_data[i].value.blue + channels_data[i].direction.blue < 0) {
            channels_data[i].direction.blue = 0;
        }
        strip_write_color(i, config.light_level, channels_data[0].value.red, channels_data[0].value.green, channels_data[0].value.blue);
    }
    Tlc.update();
    delay(40);
}


void setup()  {
    
    Serial.begin(115200); 
    //bt_serial.begin(14400);
    bt_serial.begin(9600); 
    Tlc.init(0);
    
    // load settings from EEPROM
    config.mode = EEPROM.read(MODE_ADDR);
    config.light_level_percent = EEPROM.read(LIGHT_LEVEL_ADDR);
    config.light_level = map(config.light_level_percent, 0, 100, 0, 4095);
    config.red_color = EEPROM.read(RED_COLOR_ADDR);
    config.green_color = EEPROM.read(GREEN_COLOR_ADDR);
    config.blue_color = EEPROM.read(BLUE_COLOR_ADDR);
    
    // init auxiliary array
    for(int i = 0; i < CHANNELS; i++) {
        channels_data[i].value.red = 0;
        channels_data[i].value.green = 0;
        channels_data[i].value.blue = 0;
        channels_data[i].direction.red = 1;
        channels_data[i].direction.green = 1;
        channels_data[i].direction.blue = 1;
        channels_data[i].last_modified = millis();
    }

    init_gamma_correction_table();

    strip_write_color = strip_write_color_with_correction;

    // starting animation
    // fluorescent_lamp_animation();

} 

void loop()  { 
    int command;
    unsigned long last_received;

    while(bt_serial.available() >= 2) {
        
        command = bt_serial.read();
        switch(command) {
        case MODE:
            config.mode = bt_serial.read();
            break;
        case SAVE_SETTINGS:
            bt_serial.read(); // ignore value
            save_settings(config);
            break;
        case LIGHT_LEVEL:
            config.light_level_percent = bt_serial.read();
            config.light_level = map(config.light_level_percent, 0, 100, 0, 4095);
            break;
        case RED_COLOR:
            config.red_color = bt_serial.read();
            break;
        case GREEN_COLOR:
            config.green_color = bt_serial.read();
            break;
        case BLUE_COLOR:
            config.blue_color = bt_serial.read();
            break;
        case UPLOAD_SETTINGS:
            bt_serial.read(); // ignore value
            upload_settings(config);
            break;
        }
    }

    switch(config.mode) {
        case BOBLIGHT:
            last_received = boblight(channels_data);
            if(last_received < millis() - 5000) {   // 5 seconds without boblight data? switch to constant color
                config.mode = CONSTANT_COLOR;
            }
            break;
        case CONSTANT_COLOR:
            one_color(config.light_level, config.red_color, config.green_color, config.blue_color);
            if(Serial.available() >= 3 * CHANNELS + 1) {    // data from boblight available? switch to BOBLIGHT mode
                config.mode = BOBLIGHT;
            }
            break;
        case MODE_OFF:
            one_color(config.light_level, 0, 0, 0);
            break;
        case DEMO:
            demo(channels_data);
            break;
        default:  // unknown mode, probably not yet written in EEPROM - switch do BOBLIGHT
            config.mode = BOBLIGHT;
            break;
    } // case mode    
}



int main(void) {

    init();

#if defined(USBCON)
    USB.attach();
#endif

    setup();

    for (;;) {
        loop();
        if (serialEventRun) serialEventRun();
    }

    return 0;
}

