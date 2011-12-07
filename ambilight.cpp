#include <Arduino.h>
#include <MenuBackend.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <Tlc5940.h>
#include <stdio.h>


// functions
void menuSetup();
void menuUseEvent(MenuUseEvent used);
void menuChangeEvent(MenuChangeEvent changed);
void strip_write_color(int strip, int max_level, int red, int green, int blue);
int read_button_state(int BUTTON_PIN);
int get_active_button();
int handle_buttons();
void display_status();
int get_fade_out_value(int current_color, int new_color, long last_modified, int *updated, long current_time);
void fluorescent_lamp_animation();


// number of LED strips
const int LEFT     = 6;
const int TOP      = 8;
const int RIGHT    = 6;

const int CHANNELS = LEFT + TOP + RIGHT;

// max arduino PIN number where any button is attached
const int MAX_BUTTON_PIN = 29;   // BUTTON_OK = 29  in this case

// how long to wait for stable button state
const long debounce_delay = 100;

const long FADE_OUT_DELAY     = 2000;  // 2s
const long FADE_OUT_THRESHOLD = 50;    // fade out only when all colors are < FADE_OUT_THRESHOLD


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

// buttons
const int BUTTON_UP     = 27;
const int BUTTON_DOWN   = 23;
const int BUTTON_OK     = 29;
const int BUTTON_BACK   = 25;

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

struct button_state {
  long last_debounce_time;
  int last_button_state;
};

// global variables
struct configuration config;
struct button_state buttons_state[MAX_BUTTON_PIN + 1];

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


LiquidCrystal lcd(LCD_RS, LCD_CLK, LCD_DATA_1, LCD_DATA_2, LCD_DATA_3, LCD_DATA_4, LCD_BACKLIGHT);

// menu
MenuBackend menu = MenuBackend(menuUseEvent,menuChangeEvent);
  MenuItem status_m = MenuItem("Status");
  MenuItem mode_m = MenuItem("Mode");
    MenuItem boblight_m = MenuItem("Boblight");
    MenuItem constant_color_m = MenuItem("Constant color");
    MenuItem mode_off_m = MenuItem("Turn off lights");
    MenuItem demo_m = MenuItem("Demo");
  MenuItem settings_m = MenuItem("Settings");
    MenuItem select_constant_color_m = MenuItem("Select constant color");
      MenuItem select_constant_color_red_m = MenuItem("Red");
        MenuItem select_constant_color_red_select_m = MenuItem("!!select_constant_color_red_select_m");
      MenuItem select_constant_color_green_m = MenuItem("Green");
        MenuItem select_constant_color_green_select_m = MenuItem("!!select_constant_color_gren_select_m");
      MenuItem select_constant_color_blue_m = MenuItem("Blue");
        MenuItem select_constant_color_blue_select_m = MenuItem("!!select_constant_color_blue_select_m");
    MenuItem light_level_m = MenuItem("Light level");
      MenuItem light_level_select_m = MenuItem("!!light_level_select_m");
      
  MenuItem save_m = MenuItem("Save settings");



// FUNCTIONS

void menuSetup() {
  menu.getRoot().add(status_m);
    status_m.addAfter(mode_m);
    status_m.addBefore(mode_m);
    status_m.addRight(mode_m);
    status_m.addLeft(mode_m);
    
    mode_m.addRight(boblight_m);
      boblight_m.addAfter(constant_color_m);
      constant_color_m.addBefore(boblight_m);
      constant_color_m.addAfter(mode_off_m);
      mode_off_m.addBefore(constant_color_m);
      mode_off_m.addAfter(demo_m);
      demo_m.addBefore(mode_off_m);

      boblight_m.addLeft(mode_m);
      constant_color_m.addLeft(mode_m);
      mode_off_m.addLeft(mode_m);
      demo_m.addLeft(mode_m);
      
    mode_m.addAfter(settings_m);
    settings_m.addBefore(mode_m);
    settings_m.addAfter(save_m);
    settings_m.addRight(select_constant_color_m);
      select_constant_color_m.addAfter(light_level_m);
      select_constant_color_m.addRight(select_constant_color_red_m);
        select_constant_color_red_m.addAfter(select_constant_color_green_m);
        select_constant_color_green_m.addBefore(select_constant_color_red_m);
        select_constant_color_green_m.addAfter(select_constant_color_blue_m);        
        select_constant_color_blue_m.addBefore(select_constant_color_green_m);

        select_constant_color_red_m.addLeft(select_constant_color_m);
        select_constant_color_green_m.addLeft(select_constant_color_m);
        select_constant_color_blue_m.addLeft(select_constant_color_m);
        
        select_constant_color_red_m.addRight(select_constant_color_red_select_m);
          select_constant_color_red_select_m.addBefore(select_constant_color_red_select_m);
          select_constant_color_red_select_m.addAfter(select_constant_color_red_select_m);
          select_constant_color_red_select_m.addLeft(select_constant_color_red_m);
        select_constant_color_green_m.addRight(select_constant_color_green_select_m);
          select_constant_color_green_select_m.addBefore(select_constant_color_green_select_m);
          select_constant_color_green_select_m.addAfter(select_constant_color_green_select_m);
          select_constant_color_green_select_m.addLeft(select_constant_color_green_m);          
        select_constant_color_blue_m.addRight(select_constant_color_blue_select_m);
          select_constant_color_blue_select_m.addBefore(select_constant_color_blue_select_m);
          select_constant_color_blue_select_m.addAfter(select_constant_color_blue_select_m);
          select_constant_color_blue_select_m.addLeft(select_constant_color_blue_m);          
        
      light_level_m.addBefore(select_constant_color_m);
      light_level_m.addRight(light_level_select_m);
        light_level_select_m.addBefore(light_level_select_m);
        light_level_select_m.addAfter(light_level_select_m);
        light_level_select_m.addLeft(light_level_m);
     
      select_constant_color_m.addLeft(settings_m);
      light_level_m.addLeft(settings_m);
    
    save_m.addBefore(settings_m);
}

void menuUseEvent(MenuUseEvent used) {
  if(used.item == boblight_m) {
    config.mode = BOBLIGHT;
    for(int i = 0; i < CHANNELS; i++) {
      strip_write_color(i, config.light_level, 0, 0, 0);
    }

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

    Serial.flush();
    Tlc.update();
  } else if(used.item == constant_color_m) {
    config.mode = CONSTANT_COLOR;
  } else if(used.item == mode_off_m) {
    config.mode = MODE_OFF;
  } else if(used.item == demo_m) {
    config.mode = DEMO;
  } else if(used.item == save_m) {
    EEPROM.write(MODE_ADDR, config.mode);
    EEPROM.write(LIGHT_LEVEL_ADDR, config.light_level_percent);
    EEPROM.write(RED_COLOR_ADDR, config.red_color);
    EEPROM.write(GREEN_COLOR_ADDR, config.green_color);
    EEPROM.write(BLUE_COLOR_ADDR, config.blue_color);
  }
}

void menuChangeEvent(MenuChangeEvent changed) {
  
  char lcd_message[17];
  byte color;
  MenuItem *current;
  
  if(changed.to == status_m) {
     display_status();
  } else if(changed.to == light_level_select_m) {
    switch(changed.move) {
      case MOVE_UP:
        config.light_level_percent += 10;
        if(config.light_level_percent > 100) { config.light_level_percent = 100; }
        break;
      case MOVE_DOWN:        
        if(config.light_level_percent < 10) { 
          config.light_level_percent = 0;
        } else {
          config.light_level_percent -= 10;
        }
        break;
    }
    sprintf(lcd_message, "%d", config.light_level_percent);
    lcd.clear();
    lcd.print(lcd_message);
    config.light_level = map(config.light_level_percent, 0, 100, 0, 4095);
  } else if(changed.to == select_constant_color_red_select_m ||
            changed.to == select_constant_color_green_select_m ||
            changed.to == select_constant_color_blue_select_m) {
    
    if(changed.to == select_constant_color_red_select_m) { color = config.red_color; }
    else if(changed.to == select_constant_color_green_select_m) { color = config.green_color; }
    else if(changed.to == select_constant_color_blue_select_m) { color = config.blue_color; }
    else { return; }
              
    switch(changed.move) {
      case MOVE_UP:
        if(color > 250) { color = 255; }
        else { color += 5; }
        break;
      case MOVE_DOWN:        
        if(color < 5) { color = 0; }
        else { color -= 5; }
        break;
      }
    sprintf(lcd_message, "%d", color);
    lcd.clear();
    lcd.print(lcd_message);
    
    if(changed.to == select_constant_color_red_select_m) { config.red_color = color; }
    else if(changed.to == select_constant_color_green_select_m) { config.green_color = color; }
    else if(changed.to == select_constant_color_blue_select_m) { config.blue_color = color; }
    else { return; }
    
  } else {
    lcd.clear();

    current = &(menu.getCurrent());
    
    if(changed.move == MOVE_UP || 
       ((changed.move == MOVE_RIGHT || changed.move == MOVE_LEFT) && 
       ((*current).getBefore() == 0 || (*current).getAfter())) ||
       (changed.move == MOVE_DOWN && (*current).getBefore() == 0)) {
        sprintf(lcd_message, "*%s", changed.to.getName());
        lcd.print(lcd_message);
        if((*current).getAfter()) {
          lcd.setCursor(0, 1);
          sprintf(lcd_message, " %s", (*(*current).getAfter()).getName());
          lcd.print(lcd_message);
        }
    } else { 
      if((*current).getBefore()) {          
          sprintf(lcd_message, " %s", (*(*current).getBefore()).getName());
          lcd.print(lcd_message);
        }
        sprintf(lcd_message, "*%s", changed.to.getName());
        lcd.setCursor(0, 1);
        lcd.print(lcd_message);
    }   
  }
}

void strip_write_color(int strip, int max_level, int red, int green, int blue) {
  
  if(strip & 1) {   // stupid feature, all odd stripes have RED <-> BLUE wires switched
    Tlc.set(strip * 3, map(red, 0, 255, 0, max_level));
    Tlc.set(strip * 3 + 2, map(blue, 0, 255, 0, max_level));
  } else {
    Tlc.set(strip * 3, map(blue, 0, 255, 0, max_level));
    Tlc.set(strip * 3 + 2, map(red, 0, 255, 0, max_level));
  }
  Tlc.set(strip * 3 + 1, map(green, 0, 255, 0, max_level));
}

int read_button_state(int BUTTON_PIN) {

  int button_state;
  int reading = digitalRead(BUTTON_PIN);

  if (reading != buttons_state[BUTTON_PIN].last_button_state) {
    buttons_state[BUTTON_PIN].last_debounce_time = millis();
  }
  if ((millis() - buttons_state[BUTTON_PIN].last_debounce_time) > debounce_delay) {
    button_state = reading;
  } else {
    button_state = buttons_state[BUTTON_PIN].last_button_state;
  }
  buttons_state[BUTTON_PIN].last_button_state = reading;
  return(button_state);
}

int get_active_button() {

  int active_button;
  
  active_button = 0;
  active_button = read_button_state(BUTTON_UP)?BUTTON_UP:active_button;
  active_button = read_button_state(BUTTON_DOWN)?BUTTON_DOWN:active_button;
  active_button = read_button_state(BUTTON_OK)?BUTTON_OK:active_button;
  active_button = read_button_state(BUTTON_BACK)?BUTTON_BACK:active_button;  
  
  return(active_button);
}

int handle_buttons() {

  long hold_time;  
  int pressed = 0;
  int active_button;
  static int last_active_button = 0;
  static long hold_time_last = 0;

  hold_time = millis() - hold_time_last;

  active_button = get_active_button();   
  if(active_button != last_active_button) {
    pressed = active_button;
    hold_time_last = millis();
  } else if (active_button == last_active_button &&
             hold_time > 500) {
    pressed = active_button;
    hold_time_last = millis() - 400;
  }
  last_active_button = active_button;
  
  if(pressed != 0) {
    lcd.touchBacklight();
    if(!lcd.backlightIsOn()) {
      return 0;
    }
  }
  
  return pressed;
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
  sprintf(status_message, "%u", number);
  lcd.print(status_message);   
}

int get_fade_out_value(int current_color, int new_color, long last_modified, int *updated, long current_time) {
  
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


void setup()  {
  
  // LCD initialization
  lcd.begin(16, 2);
  lcd.touchBacklight();

  Serial.begin(115200); 
  Tlc.init(0);
 
  // buttons
  pinMode(BUTTON_UP, INPUT);
  pinMode(BUTTON_DOWN, INPUT);
  pinMode(BUTTON_OK, INPUT);
  pinMode(BUTTON_BACK, INPUT);
  
  // setup for buttons
  for(int i = 0; i < MAX_BUTTON_PIN + 1; i++){
    buttons_state[i].last_debounce_time = 0;
    buttons_state[i].last_button_state = LOW;
  }
  
  // load settings from EEPROM
  config.mode = EEPROM.read(MODE_ADDR);
  config.light_level_percent = EEPROM.read(LIGHT_LEVEL_ADDR);
  config.light_level = map(config.light_level_percent, 0, 100, 0, 4095);
  config.red_color = EEPROM.read(RED_COLOR_ADDR);
  config.green_color = EEPROM.read(GREEN_COLOR_ADDR);
  config.blue_color = EEPROM.read(BLUE_COLOR_ADDR);
  
  menuSetup(); 
  menu.setCurrent(&status_m);

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

  // starting animation
  fluorescent_lamp_animation();
} 




void loop()  { 
  
  int red, green, blue;
  int button_pressed;
  int channel_updated;
  long current_time;
  unsigned static long max_duration = 0;
  unsigned long start, end;
  static int first = 1;


  button_pressed = handle_buttons();
       
  switch(button_pressed) {
    case BUTTON_UP: 
      menu.moveUp();
      break;
    case BUTTON_DOWN:
      menu.moveDown();
      break;
    case BUTTON_OK:
      menu.use();
      menu.moveRight();
      break;
    case BUTTON_BACK:
      menu.moveLeft();
      break;
  }
  
  lcd.updateBacklight();
 
  switch(config.mode) {
    case BOBLIGHT:
  start=micros();
      if(Serial.available() >= 3 * CHANNELS + 1) {
        if(Serial.read() == 0xFF) {
	  current_time = millis();
          for(int i = 0; i < CHANNELS; i++) {
	    channel_updated = 0;
            red = Serial.read();
            green = Serial.read();
            blue = Serial.read();
	    
	    if(red < FADE_OUT_THRESHOLD && green < FADE_OUT_THRESHOLD && blue < FADE_OUT_THRESHOLD) {
              red = get_fade_out_value(channels_data[i].value.red, red, channels_data[i].last_modified, &channel_updated, current_time);
              green = get_fade_out_value(channels_data[i].value.green, green, channels_data[i].last_modified, &channel_updated, current_time);
              blue = get_fade_out_value(channels_data[i].value.blue, blue, channels_data[i].last_modified, &channel_updated, current_time);
	    }
    
	    //if(channel_updated) {
              channels_data[i].last_modified = current_time;
	    //}

	    channels_data[i].value.red = red;
	    channels_data[i].value.green = green;
	    channels_data[i].value.blue = blue;
            
	    strip_write_color(i, config.light_level, red, green, blue);
          }
  //start=micros();
          Tlc.update();
  //end = micros();
        }
      }
  end = micros();
      break;
    case CONSTANT_COLOR:
      for(int i = 0; i < CHANNELS; i++) {
        strip_write_color(i, config.light_level, config.red_color, config.green_color, config.blue_color);
      }
      Tlc.update();
      //delay(40);
      break;
    case MODE_OFF:
      for(int i = 0; i < CHANNELS; i++) {
        strip_write_color(i, config.light_level, 0, 0, 0);
      }
      Tlc.update();
      delay(40);
      break;
    case DEMO:
      for(int i = 0; i < CHANNELS; i++) {
        channels_data[i].value.red += channels_data[i].direction.red;
        channels_data[i].value.green += channels_data[i].direction.green;
        channels_data[i].value.blue += channels_data[i].direction.blue;
	channels_data[i].direction.red = random(0,20) == 0?random(-1,2):channels_data[i].direction.red;
	channels_data[i].direction.green = random(0,20) == 0?random(-1,2):channels_data[i].direction.green;
	channels_data[i].direction.blue = random(0,20) == 0?random(-1,2):channels_data[i].direction.blue;
	if(channels_data[i].value.red + channels_data[i].direction.red > 255 ||
           channels_data[i].value.red + channels_data[i].direction.red < 0) {
	   //channels_data[i].direction.red *= -1;
	   channels_data[i].direction.red = 0;
	 }
	if(channels_data[i].value.green + channels_data[i].direction.green > 255 ||
           channels_data[i].value.green + channels_data[i].direction.green < 0) {
	   //channels_data[i].direction.green *= -1;
	   channels_data[i].direction.green *= 0;
	 }
	if(channels_data[i].value.blue + channels_data[i].direction.blue > 255 ||
           channels_data[i].value.blue + channels_data[i].direction.blue < 0) {
	   //channels_data[i].direction.blue *= -1;
	   channels_data[i].direction.blue *= -0;
	 }
        strip_write_color(i, config.light_level, channels_data[0].value.red, channels_data[0].value.green, channels_data[0].value.blue);
      }
      Tlc.update();

      delay(40);
      break;
    default:  // unknown mode, probably not yet written in EEPROM - switch do BOBLIGHT
      config.mode = BOBLIGHT;
      break;
  } // case mode    


  if(end - start > max_duration && config.mode == BOBLIGHT && millis() > 5000) {
    max_duration = end - start;
    display_number(max_duration);
  }

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

