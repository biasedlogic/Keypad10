#include "Keyboard.h"

//#define DEBUG

#ifdef DEBUG
#define BASE_SCANCODE 'a'
#define TOGGLEMOD KEY_LEFT_SHIFT
#else
#define BASE_SCANCODE KEY_F13
#define TOGGLEMOD KEY_LEFT_CTRL
#endif


#define REG_DELAY 200
#define NOLED 0xFF

#define LED_COL_N 3
#define LED_ROW_N 4

#define NBUTTONS 10
const uint8_t button_pin[NBUTTONS] = {9, 2, 10, 8, 4, 14, 7, 3, 5, 6};




const uint8_t led_cols[LED_COL_N] = {A3, A1, A2};
const uint8_t led_rows[LED_ROW_N] = {16, 15, A0, 0};
const uint8_t led_map[NBUTTONS] = {2,3,4,5,6,7,8,9,10,11};
const uint8_t led_red = 0;
const uint8_t led_green = 1;

const uint8_t toggles[NBUTTONS] = {1, 1, 1, 1, 0, 0, 0, 0, 0, 0};
const uint8_t groups[NBUTTONS] = {0, 0, 0, 0, 1, 1, 1, 1, 1, 1};

uint8_t t_state[NBUTTONS]; //toggle states
uint8_t key_state[NBUTTONS]; //key states
uint8_t led_state[LED_COL_N*LED_ROW_N];

uint8_t counter = 0;

void setup() {
  for (uint8_t i=0;i<NBUTTONS;i++)
    {
      pinMode(button_pin[i], INPUT_PULLUP);      
    }
  for (uint8_t i=0;i<LED_COL_N;i++)
        {
          pinMode(led_cols[i],OUTPUT);
          digitalWrite(led_rows[i],HIGH);
        }
  for (uint8_t i=0;i<LED_ROW_N;i++)
        {
          pinMode(led_rows[i],OUTPUT);
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
 
  for (char i=0;i<NBUTTONS;i++)
    {
      if (digitalRead(button_pin[i])!=key_state[i]) // the button has either been pressed, or released
        {
          if (toggles[i]) 
            { // the key is a toggle key, i.e. it sends a different code on alternating keypresses...
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
                  if (t_state[i]) //the state was on
                    {
                      t_state[i] = 0;        
                      //if (led_pin[i]!=NOLED) digitalWrite(led_pin[i],HIGH);
                      led_state[led_map[i]] = 0;
                      Keyboard.press(TOGGLEMOD);
                      Keyboard.press(BASE_SCANCODE+i);
                      delay(REG_DELAY);
                      Keyboard.release(BASE_SCANCODE+i);
                      Keyboard.release(TOGGLEMOD);
                    }
                    else
                    {
                      t_state[i] = 1;                      
                      //if (led_pin[i]!=NOLED) digitalWrite(led_pin[i],LOW);
                      led_state[led_map[i]] = 1;
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
                  
                  if (!groups[i]) led_state[led_map[i]] = 0;
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
                    { //the key is part of some group
                      for (uint8_t cancel = 0; cancel<NBUTTONS; cancel++)
                        if (groups[cancel] == groups[i]) led_state[led_map[cancel]] = 0; //kill all other LEDs in the group
                    }                    
                  led_state[led_map[i]] = 1;

                  
                  #ifdef DEBUG
                  Serial.print("Key press: ");
                  Serial.println(uint8_t(i));
                  #endif
                }
            }
        }        
    }  
} //

ISR(TIMER1_OVF_vect)
{
    static uint8_t row = 0;
    static uint8_t ledcounter = LED_COL_N;
    digitalWrite(led_rows[row],LOW);
    row++;
    if (row == LED_ROW_N) 
    {
      row = 0;
      ledcounter = 0;
    }
    for (uint8_t col = 0; col<LED_COL_N; col++)
       {
          if (led_state[ledcounter++])
            digitalWrite(led_cols[col],LOW);
            else
            digitalWrite(led_cols[col],HIGH);        
       }
    digitalWrite(led_rows[row],HIGH);    
}

//
