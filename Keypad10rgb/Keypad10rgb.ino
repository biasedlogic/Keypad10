#include <FastLED.h>
#include "Keyboard.h"

//#define DEBUG

#define OBS_PAUSE_SPECIAL
#define TOGGLE_HOLDABLE

#ifdef DEBUG
#define BASE_SCANCODE 'a'
#define TOGGLEMOD KEY_LEFT_SHIFT
#else
#define BASE_SCANCODE KEY_F13
#define TOGGLEMOD KEY_LEFT_CTRL
#endif

// This is the time in ms between for a toggle key between 'pressing' and 'releasing' the relevant keycode. Should be enough to register the key, but not enough to trigger autorepeat or such
#define REG_DELAY 200

// Keys are multiplexed. This is how many columns and rows are there. Decoupling diode anodes (+) go to columns, cathodes (-) to rows.
#define KEY_COL_N 3
#define KEY_ROW_N 4

#define KEY_ACTIVE LOW
#define KEY_INACTIVE HIGH

#define NO_LED 0xFF
#define NO_SWITCH 0x00

//LEDs are a chain of WS2812B-MINIs, first four are signal lights, then all the keys in sequence
#define DATA_PIN    4
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    14
CRGB leds[NUM_LEDS];

const CRGB led_onstate[NUM_LEDS] =  
        { CRGB::Red,    CRGB::Yellow, CRGB::Purple, CRGB::Green,    
                                      CRGB::Red,    
          CRGB::Cyan,   CRGB::Cyan,   CRGB::Yellow,    
          CRGB::Purple, CRGB::Purple, CRGB::CadetBlue,    
          CRGB::Red,    CRGB::Coral,  CRGB::Green};
          
const CRGB led_offstate[NUM_LEDS] = 
        { CRGB::Black,  CRGB::Black,  CRGB::Black,  CRGB::Black, 
                                      CRGB::Black, 
          CRGB::Black,  CRGB::Black,  CRGB::Black, 
          CRGB::Black,  CRGB::Black,  CRGB::Black, 
          CRGB::Black,  CRGB::Black,  CRGB::Black };

// This many buttons can be connected, not all need to be populated
#define NBUTTONS (KEY_COL_N*KEY_ROW_N)

// These are the pins that LED cathodes are connected to:
const uint8_t key_cols[KEY_COL_N] = {2, 14, 8};

// These are the pins that LED anodes are connected to:
const uint8_t key_rows[KEY_ROW_N] = {9, 16, 15, 7};

// LEDS are counted 0..(NUM_LEDS - 1). How they are physically aranged is a different story, so here's mapping from button number to LED number. 
// NO_LED means there's no LED associated (usually because there's no physical key in the matrix, i.e. reference design has keys 0 and 1 missing)
const uint8_t led_map[NBUTTONS] = {NO_LED,NO_LED,4,5,6,7,8,9,10,11,12,13};

// There are four more LEDs in my design than I have keys, so for future use I saved their numbers here:
const uint8_t led_A = 0;
const uint8_t led_B = 1;
const uint8_t led_C = 2;
const uint8_t led_D = 3;

// For each key in sequence: 
//  1 means the key is a toggle, so on first press it will send e.g. F13 and its LED will light up, the following keypress will send CTRL-F13 and extinguish the LED:
//  0 means it's an ordinary key behaving like a normal function key on the keyboard, i.e. pressing it each time will send a single keypress, if you hold it, your system will start autorepeating.
const uint8_t toggles[NBUTTONS] = {NO_SWITCH, NO_SWITCH, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0};

// For momentary keys (where toggles[i] is zero) key groups can be defined. If a key has a non-zero group assigned, its LED will stay on after pressing (the key behaves like a normal keyboard key, it's just the LED that will stay on).
//  At the same time all LEDs of other keys in the same group will be extinguished. This is useful if you use the keys e.g. to switch scenes in OBS: the last selected scene is the active one, and the LED will tell you which one it was.
// If a key is assigned group 0 it is in no group at all, its LED will light as long as the key is held down and will go out as soon as you release it.
const uint8_t groups[NBUTTONS] = {NO_SWITCH, NO_SWITCH, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

//internal tables to save state of the keypad
uint8_t t_state[NBUTTONS];      // toggle states, i. e. whether each toggle key is on or off.
uint8_t key_old_state[NBUTTONS];    // key registered states, i.e. which key was pressed the last time we checked.
uint8_t keys[NBUTTONS];         // key realtime state from matrix scan.


#define KEY_VirtualCam  2
#define KEY_SCR_1       3
#define KEY_SCR_2       4
#define KEY_REC         5
#define KEY_SCR_1_VID   6
#define KEY_SCR_2_VID   7
#define KEY_PAUSE       8
#define KEY_CAM         9
#define KEY_WHITEBOARD  10
#define KEY_SAFESCREEN  11

#define LED_ACTIVE(X) (leds[led_map[X]]==led_onstate[led_map[X]])

void updateSpecialLEDs()
{
  //first LED (red) warns that a person might be visible in live feed
  if (LED_ACTIVE(KEY_SCR_1_VID) || LED_ACTIVE(KEY_SCR_2_VID) || LED_ACTIVE(KEY_CAM) || LED_ACTIVE(KEY_WHITEBOARD))
    leds[led_A] = led_onstate[led_A];
  else
    leds[led_A] = led_offstate[led_A];

  //second LED (yellow) warns that the screen or whiteboard is shared
  if (LED_ACTIVE(KEY_SCR_1) ||LED_ACTIVE(KEY_SCR_2) ||LED_ACTIVE(KEY_SCR_1_VID) ||LED_ACTIVE(KEY_SCR_2_VID) ||LED_ACTIVE(KEY_WHITEBOARD))
    leds[led_B] = led_onstate[led_B];
  else
    leds[led_B] = led_offstate[led_B];

  //third LED (purple) warns that a stream is either being recorded or is broadcast live
  if ((LED_ACTIVE(KEY_REC)&& !LED_ACTIVE(KEY_PAUSE)) || LED_ACTIVE(KEY_VirtualCam))
    leds[led_C] = led_onstate[led_C];
  else
    leds[led_C] = led_offstate[led_C];

  //fourth LED (green) tells that a "safe screen" is active (i.e. a test pattern or a "wait for picture" screen)
  if (LED_ACTIVE(KEY_SAFESCREEN))
    leds[led_D] = led_onstate[led_D];
  else
    leds[led_D] = led_offstate[led_D];
}


void setup() {  
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(100);
  //.setCorrection(TypicalLEDStrip);
  for (uint8_t i=0;i<KEY_COL_N;i++) // setup pins for keys, keypad is multiplexed with diodes to prevent any sort of ghosting
    {
      pinMode(key_cols[i], INPUT_PULLUP);      
    }
  for (uint8_t i=0;i<KEY_ROW_N;i++) // setup LED matrix control - column outputs
        {
          pinMode(key_rows[i],OUTPUT);
          digitalWrite(key_rows[i],HIGH);
        }

  digitalWrite(key_rows[0],LOW); //prepare the first row for reading
  
  for (uint8_t i=0;i<NUM_LEDS;i++) // setup LED matrix control - column outputs
        {
          leds[i]=led_offstate[i];
        }      
  FastLED.show();
  
  // master clock init sequence:
  CLKPR = 0x80;
  CLKPR = 0b00000000;

  //sync timers and reset their prescalers
  GTCCR = 0b10000011; 

  // timer mode initialization
  TCCR1A = 0b00000001;
  TCCR1B = 0b00001011;
    // 0101 F-PWM,   8-bit TOP=0x00FF Upd.OCR1x@BOT TOV1@:TOP
    // 00 Normal port operation, OC1A disconnected.
    // 00 Normal port operation, OC1B disconnected.
    // 011 0064 clkIO/64   (From prescaler)
    // 0 NC off
    // 0 count on falling edge
    // Timer 1 clock: 250000 Hz, resulting base frequency: 976.5625 Hz, resulting base period: 1.024 ms


  TIMSK1 = (1 << TOIE1);
  sei();
  
  //release prescaler reset
  GTCCR = 0x00; 

  // initialize control over the keyboard:
  Keyboard.begin();
  #ifdef DEBUG
  Serial.begin(9600);
  #endif
  
}

void loop() { 
  for (char i=0;i<NBUTTONS;i++) //scan through all keys
    {
      #ifdef OBS_PAUSE_SPECIAL
      if ( (i==KEY_PAUSE) && (t_state[KEY_REC]==0)) continue; //don't evaluate PAUSE key if not recording
      #endif
      
      if (keys[i]!=key_old_state[i]) // the button has either been pressed, or released, anyway the state changed
        {
          if (toggles[i]) 
            { // the key is a toggle key, i.e. it should send alternating keycodes on alternating keypresses...
              if (key_old_state[i]==KEY_ACTIVE)
                {  //the key was pressed, but isn't anymore - do nothing except register the fact...
                  key_old_state[i] = KEY_INACTIVE;

                  #ifdef TOGGLE_HOLDABLE
                  Keyboard.release(BASE_SCANCODE+i);
                  if (t_state[i]==0)
                    {                      
                      Keyboard.release(TOGGLEMOD);
                    }                    
                  #endif
                  
                  #ifdef DEBUG
                  Serial.print("Key release: ");
                  Serial.println(uint8_t(i));
                  #endif
                }
                else
                {  //the key wasn't pressed, but is now, i.e. we need to toggle the toggle...
                  key_old_state[i] = KEY_ACTIVE;                  
                  #ifdef DEBUG
                  Serial.print("Key press: ");
                  Serial.println(uint8_t(i));
                  #endif
                  if (t_state[i]) 
                    { //the state was on, so now it's off
                      t_state[i] = 0;                              
                      leds[led_map[i]] = led_offstate[led_map[i]]; //turn the key LED off
                      Keyboard.press(TOGGLEMOD);
                      Keyboard.press(BASE_SCANCODE+i);
                      #ifndef TOGGLE_HOLDABLE
                        delay(REG_DELAY);
                        Keyboard.release(BASE_SCANCODE+i);
                        Keyboard.release(TOGGLEMOD);
                      #endif

                      #ifdef OBS_PAUSE_SPECIAL
                      //release pause state as stopping recording will reset pause state
                      if (i==KEY_REC)
                      {
                        t_state[KEY_PAUSE]=0;
                        leds[led_map[KEY_PAUSE]]=led_offstate[led_map[KEY_PAUSE]];
                      }
                      #endif
                    }
                    else
                    { //the state was off, so now it's on
                      t_state[i] = 1;                      
                      leds[led_map[i]] = led_onstate[led_map[i]]; //turn the key LED on
                      Keyboard.press(BASE_SCANCODE+i);
                      #ifndef TOGGLE_HOLDABLE
                      delay(REG_DELAY);
                      Keyboard.release(BASE_SCANCODE+i);                      
                      #endif
                    }                  
                }              
              
            }
            else 
            { //the key is an ordinary key
              if (key_old_state[i]==KEY_ACTIVE)
                {  //the key was pressed, but isn't anymore
                  key_old_state[i] = KEY_INACTIVE;
                  Keyboard.release(BASE_SCANCODE+i);                  
                  if (groups[i]==0) leds[led_map[i]] = led_offstate[led_map[i]]; // if the key isn't in any key group, switch its LED off as soon as it's released
                  #ifdef DEBUG
                  Serial.print("Key release: ");
                  Serial.println(uint8_t(i));
                  #endif
                }
                else
                {  //the key wasn't pressed, but is now
                  key_old_state[i] = KEY_ACTIVE;
                  Keyboard.press(BASE_SCANCODE+i);
                  if (groups[i]) 
                    { // if the key is in a key group, switch all other group key's LEDs off first
                      for (uint8_t cancel = 0; cancel<NBUTTONS; cancel++)
                        if ((groups[cancel] == groups[i]) && (toggles[cancel]==0)) leds[led_map[cancel]] = led_offstate[led_map[cancel]]; //kill all other LEDs in the group, except if they are toggles
                    }                    
                    
                  leds[led_map[i]] = led_onstate[led_map[i]]; //then light up the LED of the key that was pressed - this regardless of whether it's in a group or not                  
                  #ifdef DEBUG
                  Serial.print("Key press: ");
                  Serial.println(uint8_t(i));
                  #endif
                }
            }
        }        
    }
    updateSpecialLEDs();
    FastLED.show();
    FastLED.delay(20);
}

ISR(TIMER1_OVF_vect)
{
    //do the key matrix scanning:
    static uint8_t row = 0;
    static uint8_t keycounter = 0;
    
    //before initializing the interrupt the first row was set active. At the end of each ISR call the row will get set for the next reading, so let's read the keys now:
    for (uint8_t col = 0; col<KEY_COL_N; col++) // set up column outputs for the current row
       {
          keys[keycounter++]=digitalRead(key_cols[col]);
       }
       
    digitalWrite(key_rows[row],HIGH); // release last active row
    row++;
    if (row == KEY_ROW_N) //if we scanned all rows already, restart the scan from the beginning
    {
      row = 0;
      keycounter = 0;
    }
    digitalWrite(key_rows[row],LOW); // activate current row for the next readout (allow inputs to stabilize)
}
