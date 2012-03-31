#include <Arduino.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <Tlc5940.h>
#include <stdio.h>


// functions
void strip_write_color_linear(int strip, int max_level, int red, int green, int blue);
void strip_write_color_with_correction(int strip, int max_level, int red, int green, int blue);
void display_status();
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


// INPUT/OUTPUT PINS

// lcd
const int LCD_RS        = 35;
const int LCD_CLK       = 37;
const int LCD_BACKLIGHT =  7;
const int LCD_DATA_1    = 45;
const int LCD_DATA_2    = 43;
const int LCD_DATA_3    = 41;
const int LCD_DATA_4    = 39;


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


LiquidCrystal lcd(LCD_RS, LCD_CLK, LCD_DATA_1, LCD_DATA_2, LCD_DATA_3, LCD_DATA_4, LCD_BACKLIGHT);

// FUNCTIONS

void save_settings(struct configuration config) {
    EEPROM.write(MODE_ADDR, config.mode);
    EEPROM.write(LIGHT_LEVEL_ADDR, config.light_level_percent);
    EEPROM.write(RED_COLOR_ADDR, config.red_color);
    EEPROM.write(GREEN_COLOR_ADDR, config.green_color);
    EEPROM.write(BLUE_COLOR_ADDR, config.blue_color);
}

void upload_settings(struct configuration config) {
    Serial1.write(MODE);
    Serial1.write(config.mode);
    Serial1.write(LIGHT_LEVEL);
    Serial1.write(config.light_level_percent);
    Serial1.write(RED_COLOR);
    Serial1.write(config.red_color);
    Serial1.write(GREEN_COLOR);
    Serial1.write(config.green_color);
    Serial1.write(BLUE_COLOR);
    Serial1.write(config.blue_color);
}

void strip_write_color_linear(int strip, int max_level, int red, int green, int blue) {
    
    if(strip & 1) {   // stupid feature, all odd stripes have RED <-> BLUE wires switched
        Tlc.set(strip * 3, map(red, 0, 255, 0, max_level));
        Tlc.set(strip * 3 + 2, map(blue, 0, 255, 0, max_level));
    } else {
        Tlc.set(strip * 3, map(blue, 0, 255, 0, max_level));
        Tlc.set(strip * 3 + 2, map(red, 0, 255, 0, max_level));
    }
    Tlc.set(strip * 3 + 1, map(green, 0, 255, 0, max_level));
}


void strip_write_color_with_correction(int strip, int max_level, int red, int green, int blue) {
    
    if(strip & 1) {   // stupid feature, all odd stripes have RED <-> BLUE wires switched
        Tlc.set(strip * 3, map(gamma_correction[red], 0, 4096, 0, max_level));
        Tlc.set(strip * 3 + 2, map(gamma_correction[blue], 0, 4096, 0, max_level));
    } else {
        Tlc.set(strip * 3, map(gamma_correction[blue], 0, 4096, 0, max_level));
        Tlc.set(strip * 3 + 2, map(gamma_correction[red], 0, 4096, 0, max_level));
    }
    Tlc.set(strip * 3 + 1, map(gamma_correction[green], 0, 4096, 0, max_level));
}


void display_status() {
    char status_message[17];  
    
    lcd.clear();
    switch(config.mode) {
        case BOBLIGHT:
            lcd.print("MODE: BOBLIGHT");
            break;
        case CONSTANT_COLOR:
            lcd.print("MODE: STATIC");
            break;
        case MODE_OFF:
            lcd.print("MODE: NO LIGHTS");
            break;      
    }
    lcd.setCursor(0, 1);
    sprintf(status_message, "LIGHT LEVEL: %d", config.light_level_percent);
    lcd.print(status_message);   
}

void display_number(unsigned long number) {
    char status_message[17];  
    
    lcd.setCursor(0, 1);
    sprintf(status_message, "%lu", number);
    lcd.print(status_message);   
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

void boblight(channel_data channels_data[]) {
    int red, green, blue;
    long current_time;

    if(Serial.available() >= 3 * CHANNELS + 1) {
        if(Serial.read() == 0xFF) {
            current_time = millis();
            for(int i = 0; i < CHANNELS; i++) {
                red = Serial.read();
                green = Serial.read();
                blue = Serial.read();
        
                if(red < FADE_OUT_THRESHOLD && green < FADE_OUT_THRESHOLD && blue < FADE_OUT_THRESHOLD) {
                    red = get_fade_out_value(channels_data[i].value.red, red, channels_data[i].last_modified, current_time);
                    green = get_fade_out_value(channels_data[i].value.green, green, channels_data[i].last_modified, current_time);
                    blue = get_fade_out_value(channels_data[i].value.blue, blue, channels_data[i].last_modified, current_time);
                }

                channels_data[i].last_modified = current_time;

                channels_data[i].value.red = red;
                channels_data[i].value.green = green;
                channels_data[i].value.blue = blue;
                
                strip_write_color(i, config.light_level, red, green, blue);
            }
            Tlc.update();
        }
    }
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
    
    // LCD initialization
    lcd.begin(16, 2);
    lcd.touchBacklight();

    Serial.begin(115200); 
    Serial1.begin(9600); 
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
    /*unsigned static long max_duration = 0;
    unsigned long start = 0;
    unsigned long end = 0;*/
    int command;

    lcd.updateBacklight();

    while(Serial1.available() >= 2) {
        
        command = Serial1.read();
        switch(command) {
        case MODE:
            config.mode = Serial1.read();
            break;
        case SAVE_SETTINGS:
            Serial1.read(); // ignore value
            save_settings(config);
            break;
        case LIGHT_LEVEL:
            config.light_level_percent = Serial1.read();
            config.light_level = map(config.light_level_percent, 0, 100, 0, 4095);
            break;
        case RED_COLOR:
            config.red_color = Serial1.read();
            break;
        case GREEN_COLOR:
            config.green_color = Serial1.read();
            break;
        case BLUE_COLOR:
            config.blue_color = Serial1.read();
            break;
        case UPLOAD_SETTINGS:
            Serial1.read(); // ignore value
            upload_settings(config);
            break;
        }
    }

    switch(config.mode) {
        case BOBLIGHT:
            //start=micros();
            boblight(channels_data);
            //end = micros();
            break;
        case CONSTANT_COLOR:
           one_color(config.light_level, config.red_color, config.green_color, config.blue_color);
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

    /*if(end - start > max_duration && config.mode == BOBLIGHT && millis() > 5000) {
        max_duration = end - start;
        display_number(max_duration);
    }*/
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

