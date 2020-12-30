#include "Keyboard.h"

//#define DEBUG

#ifdef DEBUG
#define BASE_SCANCODE 'a'
#define TOGGLEMOD KEY_LEFT_SHIFT
#else
#define BASE_SCANCODE KEY_F13
#define TOGGLEMOD KEY_LEFT_CTRL
#endif

// This is the time in ms between for a toggle key between 'pressing' and 'releasing' the relevant keycode. Should be enough to register the key, but not enough to trigger autorepeat or such
#define REG_DELAY 200

// Leds are multiplexed. This is how many columns and rows are there. Anodes (+) go to rows, cathodes (-) to columns.
#define LED_COL_N 3
#define LED_ROW_N 4

// This many buttons are connected
#define NBUTTONS 10

// The buttons numbered 0..NBUTTONS-1 are connected between these pins and GND (use only pins that have internal pullups or connect external pullups yourself!):
const uint8_t button_pin[NBUTTONS] = {9, 2, 10, 8, 4, 14, 7, 3, 5, 6};

// These are the pins that LED cathodes are connected to:
const uint8_t led_cols[LED_COL_N] = {A3, A1, A2};

// These are the pins that LED anodes are connected to:
const uint8_t led_rows[LED_ROW_N] = {16, 15, A0, 0};

// LEDS are counted 0..(rows x cols - 1) column by column then row by row. How they are physically aranged is a different story, so here's mapping from button number to LED number
const uint8_t led_map[NBUTTONS] = {2,3,4,5,6,7,8,9,10,11};

// There are two more LEDs in my design than I have keys, so for future use I saved their numbers here:
const uint8_t led_red = 0;
const uint8_t led_green = 1;

// For each key in sequence: 
//  1 means the key is a toggle, so on first press it will send e.g. F13 and its LED will light up, the following keypress will send CTRL-F13 and extinguish the LED:
//  0 means it's an ordinary key behaving like a normal function key on the keyboard, i.e. pressing it each time will send a single keypress, if you hold it, your system will start autorepeating.
const uint8_t toggles[NBUTTONS] = {1, 1, 1, 1, 0, 0, 0, 0, 0, 0};


// For momentary keys (where toggles[i] is zero) key groups can be defined. If a key has a non-zero group assigned, its LED will stay on after pressing (the key behaves like a normal keyboard key, it's just the LED that will stay on).
//  At the same time all LEDs of other keys in the same group will be extinguished. This is useful if you use the keys e.g. to switch scenes in OBS: the last selected scene is the active one, and the LED will tell you which one it was.
// If a key is assigned group 0 it is in no group at all, its LED will light as long as the key is held down and will go out as soon as you release it.
const uint8_t groups[NBUTTONS] = {0, 0, 0, 0, 1, 1, 1, 1, 1, 1};


//internal tables to save state of the keypad
uint8_t t_state[NBUTTONS];      // toggle states, i. e. whether each toggle key is on or off.
uint8_t key_state[NBUTTONS];    // key states, i.e. which key was pressed the last time we checked.
uint8_t led_state[LED_COL_N*LED_ROW_N]; // LED states, in sequence of LEDs in matrix, 1 is on, 0 is off.

void setup() {
  for (uint8_t i=0;i<NBUTTONS;i++) // setup pins for keys, keypad is non-multiplexed to make panel wiring easy and prevent any sort of ghosting
    {
      pinMode(button_pin[i], INPUT_PULLUP);      
    }
  for (uint8_t i=0;i<LED_COL_N;i++) // setup LED matrix control - column outputs
        {
          pinMode(led_cols[i],OUTPUT);
          digitalWrite(led_rows[i],HIGH);
        }
  for (uint8_t i=0;i<LED_ROW_N;i++)
        {
          pinMode(led_rows[i],OUTPUT); // setup LED matrix control - row outputs
          digitalWrite(led_rows[i],LOW);
        }

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
      if (digitalRead(button_pin[i])!=key_state[i]) // the button has either been pressed, or released, anyway the state changed
        {
          if (toggles[i]) 
            { // the key is a toggle key, i.e. it should send alternating keycodes on alternating keypresses...
              if (key_state[i]==LOW)
                {  //the key was pressed, but isn't anymore - do nothing except register the fact...
                  key_state[i] = HIGH;
                  #ifdef DEBUG
                  Serial.print("Key release: ");
                  Serial.println(uint8_t(i));
                  #endif
                }
                else
                {  //the key wasn't pressed, but is now, i.e. we need to toggle the toggle...
                  key_state[i] = LOW;                  
                  #ifdef DEBUG
                  Serial.print("Key press: ");
                  Serial.println(uint8_t(i));
                  #endif
                  if (t_state[i]) 
                    { //the state was on, so now it's off
                      t_state[i] = 0;                              
                      led_state[led_map[i]] = 0; //turn the key LED off
                      Keyboard.press(TOGGLEMOD);
                      Keyboard.press(BASE_SCANCODE+i);
                      delay(REG_DELAY);
                      Keyboard.release(BASE_SCANCODE+i);
                      Keyboard.release(TOGGLEMOD);
                    }
                    else
                    { //the state was off, so now it's on
                      t_state[i] = 1;                      
                      led_state[led_map[i]] = 1; //turn the key LED on
                      Keyboard.press(BASE_SCANCODE+i);
                      delay(REG_DELAY);
                      Keyboard.release(BASE_SCANCODE+i);                      
                    }                  
                }              
              
            }
            else 
            { //the key is an ordinary key
              if (key_state[i]==LOW)
                {  //the key was pressed, but isn't anymore
                  key_state[i] = HIGH;
                  Keyboard.release(BASE_SCANCODE+i);                  
                  if (groups[i]==0) led_state[led_map[i]] = 0; // if the key isn't in any key group, switch its LED off as soon as it's released
                  #ifdef DEBUG
                  Serial.print("Key release: ");
                  Serial.println(uint8_t(i));
                  #endif
                }
                else
                {  //the key wasn't pressed, but is now
                  key_state[i] = LOW;
                  Keyboard.press(BASE_SCANCODE+i);
                  if (groups[i]) 
                    { // if the key is in a key group, switch all other group key's LEDs off first
                      for (uint8_t cancel = 0; cancel<NBUTTONS; cancel++)
                        if ((groups[cancel] == groups[i]) && (toggles[cancel]==0)) led_state[led_map[cancel]] = 0; //kill all other LEDs in the group, except if they are toggles
                    }                    
                  led_state[led_map[i]] = 1; //then light up the LED of the key that was pressed - this regardless of whether it's in a group or not                  
                  #ifdef DEBUG
                  Serial.print("Key press: ");
                  Serial.println(uint8_t(i));
                  #endif
                }
            }
        }        
    }  
}

ISR(TIMER1_OVF_vect)
{
    //do the LED matrix scanning:
    static uint8_t row = 0;
    static uint8_t ledcounter = LED_COL_N;
    digitalWrite(led_rows[row],LOW); // release last active row
    row++;
    if (row == LED_ROW_N) //if we scanned all rows already, restart the scan from the beginning
    {
      row = 0;
      ledcounter = 0;
    }
    for (uint8_t col = 0; col<LED_COL_N; col++) // set up column outputs for the current row
       {
          if (led_state[ledcounter++])
            digitalWrite(led_cols[col],LOW);
            else
            digitalWrite(led_cols[col],HIGH);        
       }
    digitalWrite(led_rows[row],HIGH); // activate current row
}
