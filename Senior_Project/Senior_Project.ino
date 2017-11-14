
//enable support for i2c to 16x2 LCD module
#include <FaBoLCD_PCF8574.h>

//enable support for register access and interrupts
#include <avr/io.h>
#include <avr/interrupt.h>

//enable arduino library for the keypad
#include <Key.h>
#include <Keypad.h>

#define ALARM_PIN     12    //pin 18 on chip, used to control alarm speaker and led
#define SETPOINT_PIN  13    //pin 19 on chip, used control setpoint led
#define ZERO_SENSE    2     //pin 4 on chip, used as external interrupt pin to capture zero crossing event
#define GATE_PIN      3     //pin 5 on chip, used to control the triggering of the triac (pretty sure want PWM here, so chose PWM PIN)
#define PULSE         10    //clock counts to keep triac gate high, main terminals stay "shorted" until AC crosses zero again
                            //ensures we get the main terminals to stay triggered
#define HOT_PIN       4     //probably need to change pin (4 on chip currently)

//may need to modify these
//used to convert adc reading into degree F                            
#define TEMP_OFFSET   28.2    
#define TEMP_SLOPE    .4545454545

const byte ROWS = 4; // Four rows
const byte COLS = 3; // Three columns

// Define the Keymap
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
// Connect keypad ROW0, ROW1, ROW2 and ROW3 to these Arduino pins (11-14 on chip).  (will probably need to change these pins)
byte rowPins[ROWS] = { 5, 6, 7, 8 };    

// Connect keypad COL0, COL1 and COL2 to these Arduino pins (15-17 on chip).
byte colPins[COLS] = { 9, 10, 11 }; 

//define the keypad using info above
Keypad kpd = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
FaBoLCD_PCF8574 lcd;

volatile byte state = LOW;
int temperature = 0;
int setpoint = 0;
byte hot_led_en = 0;


void setup() {
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(SETPOINT_PIN, OUTPUT);
  pinMode(ZERO_SENSE, INPUT);
  pinMode(GATE_PIN, OUTPUT);
  digitalWrite(ZERO_SENSE, HIGH);

  // set up Timer1 
  //(see ATMEGA 328 data sheet pg 134 for more details)
  OCR1A = 10;       //initialize the comparator (10=roughly full sine wave)
  TIMSK1 = 0x03;    //enable comparator A and overflow interrupts
  TCCR1A = 0x00;    //timer control registers set for
  TCCR1B = 0x00;    //normal operation, timer disabled

  //Creates an interrupt triggered on rising edge of zero-crossing circuit output
  //INT0, pin 4 on chip
  attachInterrupt(0, zero_crossing, RISING);  //currently working
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  
  // Print a message to the LCD.
  lcd.print("hello, world!");
}

//edit OCR1A to control when to turn on hotplate (should be able to use PID here)
//for alarm timer can use millis() (when >120 set varible to millis, and then calculate difference later on) can also use for auto shutoff
void loop() {
  
  temperature = double(TEMP_SLOPE * analogRead(A0)) + TEMP_OFFSET;  //Seems to be pretty close, will need to double check again
  lcd.clear();
  lcd.home();
  lcd.print("Temp = ");
  lcd.print(temperature);
  delay(100);   //need to look up how to make smoother output (probably dont want to have delay)

    //will use to start alarm timer and turn on and off the hot led, just keep in mind that millis overflows after 50 days
//if (temperature>=120 && set == 1 && hot_led_en == 0) {       //setpoint enabled and hotplate temp >=120, start alarm timer
//  alarm_timer = millis();
//  digitalWrite(HOT_PIN, HIGH);
//  hot_led_en = 1;
//}
//else if (temperature<120 && hot_led_en == 1) {
//   digitalWrite(HOT_PIN, LOW);
//   hot_led_en = 0;
//}

  if ((millis() - alarm_timer) >= 30000) {    //30000 is 30 seconds, alarm after that
    digitalWrite(ALARM_PIN, OUTPUT);
  }
    
  

  //test stuff
  
  //Serial.begin(115200);
  //Serial.print("Temp = ");
  //Serial.println(temperature);
  //delay(1000);


  //keypad read, in test setup now
  char key = kpd.getKey();
  if(key){  // Check for a valid key.
    
    switch (key){
      case '*':
        digitalWrite(ALARM_PIN, LOW);
        break;
      case '#':
       // for (int i=0; i<250; i++) {
          digitalWrite(ALARM_PIN, HIGH);
         // delay(1);
         // digitalWrite(ALARM_PIN, LOW);
         // delay(1);
       // }
        break;
      case '5':
        lcd.noDisplay();
        break;
      case '6': 
        lcd.display();
        break;
      
    }
  }
}

//ISR for zero crossing signal (currently working need 1M pull-down on pin 4 of chip)
//Code modified from arduino ac phase control tutorial
void zero_crossing() {
  TCCR1B=0x04;                  //start timer with divide by 256 input
  TCNT1 = 0;                    //reset timer - count from zero   
}

ISR(TIMER1_COMPA_vect){         //comparator match
  digitalWrite(GATE_PIN,HIGH);  //set TRIAC gate to high
  TCNT1 = 65536-PULSE;          //trigger pulse width, will be PULSE counts long
}

ISR(TIMER1_OVF_vect){           //timer1 overflow
  digitalWrite(GATE_PIN,LOW);   //turn off TRIAC gate
  TCCR1B = 0x00;                //disable timer, stop unintended triggers
}




