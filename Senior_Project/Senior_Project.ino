#include <PID_v1.h>

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
//#define PULSE         10    //clock counts to keep triac gate high, main terminals stay "shorted" until AC crosses zero again
                            //ensures we get the main terminals to stay triggered
#define HOT_LED_PIN   4     //probably need to change pin (6 on chip currently)
#define KP            2 
#define KI            5
#define KD            1

//may need to modify these, check AVcc level and recalculate, also check where temp sense gets +5V
//used to convert adc reading into degree F                            
#define TEMP_OFFSET   28.2    
#define TEMP_SLOPE    .445  //.4545454545

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


double temperature = 0;
double setpoint = 0;
double pid_output;
boolean set_state = false;
boolean hot_led_state = false;
unsigned long alarm_timer = 0xFFFFFFFF;
const long alarm_period = 1;    //1ms on/off time, 500Hz, closest can get to 1kHz using millis, Blink Without Delay Arduino
long print_cnt = 50000;
int key_temp = 0;
boolean zero_state = false; //if we need to care about zero sensing or not
//boolean on_state = false;    //if we are a state to care about alarms
int windowsize = 516; //518 - 2, practically turns triac off (see ISRs below), same as 518 and get full wave again 

PID myPID(&temperature, &pid_output, &setpoint, KP, KI, KD, REVERSE);





void setup() {
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(SETPOINT_PIN, OUTPUT);
  pinMode(ZERO_SENSE, INPUT);
  pinMode(GATE_PIN, OUTPUT);
  pinMode(HOT_LED_PIN, OUTPUT);       //controls alarm speaker and led bjts

  // set up Timer1 
  //(see ATMEGA 328 data sheet pg 134 for more details)
  OCR1A = 2;       //initialize the comparator (5=roughly full sine wave)
  TIMSK1 = 0x03;    //enable comparator A and overflow interrupts
  TCCR1A = 0x00;    //timer control registers set for
  TCCR1B = 0x00;    //normal operation, timer disabled

  //Creates an interrupt triggered on rising edge of zero-crossing circuit output
  //INT0, pin 4 on chip
  attachInterrupt(0, zero_crossing, RISING);  //currently working
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  
  // Print a message to the LCD.
  lcd.print("Temp = ");
  lcd.setCursor(0,1);
  lcd.print("SETp = ");

  //setup PID limits (
  myPID.SetOutputLimits(2, windowsize);
  myPID.SetMode(AUTOMATIC);

  //test variables
  //temperature = 200;

  
}

//edit OCR1A to control when to turn on hotplate (should be able to use PID here)
//for alarm timer can use millis() (when >120 set varible to millis, and then calculate difference later on) can also use for auto shutoff


void loop() {
  
  
  temperature = (TEMP_SLOPE * analogRead(A0)) + TEMP_OFFSET;  //Seems to be pretty close, will need to double check again
  if (print_cnt>=10000) {
    lcd.setCursor(7,0);
    lcd.print("   ");
    lcd.setCursor(7,0); 
    lcd.print((int)temperature);
    print_cnt = 0;
  }
  print_cnt++;

    //will use to start alarm timer and turn on and off the hot led, just keep in mind that millis overflows after 50 days (using 
  if ((temperature>=120) && !(hot_led_state) && set_state) {       //setpoint entered, hotplate temp >=120 and led not already on, start alarm timer
    alarm_timer = millis();                                    //Need to have setpoint to work
    digitalWrite(HOT_LED_PIN, HIGH);                           //Turn on HOT/timer activated LED
    hot_led_state = !hot_led_state;                                            
  }
  else if ((temperature<120) && (hot_led_state) && !set_state) {                   // if temp is back in safe range and hot led is on, turn off led
    digitalWrite(HOT_LED_PIN, LOW);
    hot_led_state = !hot_led_state;
  }

  if (((millis() - alarm_timer) >= 10000) && hot_led_state) {     //30000 is 30 seconds, alarm after that
    digitalWrite(ALARM_PIN, HIGH);                         //need to add code that will flash LED and speaker at 1kHz, probably also need to use millis for it
    if ((millis() - alarm_timer) >= 20000) {
      hot_led_state = false;
      set_state = false;
      zero_state = false;
      setpoint = 0;
      key_temp = 0;
      lcd.setCursor(7,1);
      lcd.print("       ");
      lcd.setCursor(7,1);
      lcd.print("ALARM");
      digitalWrite(HOT_LED_PIN, LOW);
      digitalWrite(SETPOINT_PIN, LOW);
      digitalWrite(ALARM_PIN, LOW);
            
    }
  }


//get setpoint with keypad

//for pid stuff
//only need when hotplate on

  if (set_state) {
    zero_state = true;
    myPID.Compute();
    OCR1A = pid_output;
  }
  else {
    zero_state = false;
    digitalWrite(GATE_PIN,LOW);
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
        alarm_timer = millis();
        digitalWrite(ALARM_PIN, LOW);
        break;
        
      case '#':
        hot_led_state = false;
        set_state = false;
        zero_state = false;
        setpoint = 0;
        key_temp = 0;
        lcd.setCursor(7,1);
        lcd.print("       ");
        digitalWrite(HOT_LED_PIN, LOW);
        digitalWrite(SETPOINT_PIN, LOW);
        digitalWrite(ALARM_PIN, LOW);
        break;
        
      default:
        if (key_temp == 0) {
          setpoint = 100*(key-'0');
          key_temp++;
          set_state = false;
          hot_led_state = false;
          digitalWrite(HOT_LED_PIN, LOW);
          digitalWrite(SETPOINT_PIN, LOW);
          lcd.setCursor(7,1);
          lcd.print("       ");
          lcd.setCursor(7,1);
          lcd.print((int)setpoint/100);
          break;
        }
        else if (key_temp == 1) {
          setpoint = setpoint + 10*(key-'0');
          key_temp++;
          set_state = false;
          hot_led_state = false;
          digitalWrite(HOT_LED_PIN, LOW);
          digitalWrite(SETPOINT_PIN, LOW);
          lcd.setCursor(7,1);
          lcd.print("       ");
          lcd.setCursor(7,1);
          lcd.print((int)setpoint/10);
          break;
        }
        else if (key_temp == 2) {
          setpoint = setpoint + (key-'0');
          key_temp = 0;
          if (setpoint >=100 && setpoint <=500) {
            set_state = true;
            lcd.setCursor(7,1);
            lcd.print("       ");
            lcd.setCursor(7,1);
            lcd.print((int)setpoint);
            digitalWrite(SETPOINT_PIN, HIGH); 
          }
          else {
            setpoint=0;
            set_state = false;
            hot_led_state = false;
            digitalWrite(SETPOINT_PIN, LOW);
            digitalWrite(HOT_LED_PIN, LOW);
            lcd.setCursor(7,1);
            lcd.print("       ");
            lcd.setCursor(7,1);
            lcd.print("INVALID");
          }
          break; 
        }
        break;
      
    }
  }
}

//ISR for zero crossing signal (currently working need 1M pull-down on pin 4 of chip), trying with interal pullup and no pulldown next (try again)
//Code modified from arduino ac phase control tutorial
void zero_crossing() {
  if (zero_state) {
    TCCR1B=0x04;                  //start timer with divide by 256 input
    TCNT1 = 0;                    //reset timer - count from zero
  }   
}

ISR(TIMER1_COMPA_vect){         //comparator match
  if (zero_state) {
    digitalWrite(GATE_PIN,HIGH);  //set TRIAC gate to high
    TCNT1 = 65536+(-518+OCR1A);          //trigger pulse width, will be full half wave - delay long
  }                                     //bigger than 518 and starts skipping some half waves, should be able to replace ocr1a with pid stuff
}

ISR(TIMER1_OVF_vect){           //timer1 overflow
  if (zero_state) {
    digitalWrite(GATE_PIN,LOW);   //turn off TRIAC gate
    TCCR1B = 0x00;                //disable timer, stop unintended triggers
  }
}




