#include <FaBoLCD_PCF8574.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include <Key.h>
#include <Keypad.h>

#define ALARM_PIN     12    //pin 18 on chip, used to control alarm speaker and led
#define SETPOINT_PIN  13    //pin 19 on chip, used control setpoint led
#define ZERO_SENSE    2     //pin 4 on chip, used as external interrupt pin to capture zero crossing event


const byte ROWS = 4; // Four rows
const byte COLS = 3; // Three columns

// Define the Keymap
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
// Connect keypad ROW0, ROW1, ROW2 and ROW3 to these Arduino pins (11-14 on chip).
byte rowPins[ROWS] = { 5, 6, 7, 8 };    

// Connect keypad COL0, COL1 and COL2 to these Arduino pins (15-17 on chip).
byte colPins[COLS] = { 9, 10, 11 }; 

//define the keypad using info above
Keypad kpd = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
FaBoLCD_PCF8574 lcd;

volatile byte state = LOW;

void setup() {
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(SETPOINT_PIN, OUTPUT);
  pinMode(ZERO_SENSE, INPUT);
  attachInterrupt(0, crossing, RISING);  //cureently working
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  
  // Print a message to the LCD.
  lcd.print("hello, world!");
  
}

void loop() {
 // interrupts();
  //digitalWrite(ALARM_PIN, state);  
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

//ISR for zero crossing signal (currently working nedd 1M pull-down on pin 4 of chip
void crossing() {
  state = !state; 
  digitalWrite(ALARM_PIN, state); 
}


