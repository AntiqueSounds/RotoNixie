/*
 * Nixie Calculator 
 * This code leverages the Arduinix shield http://www.arduinx.com/ origional open source versions. 
 * Written by M. Keith Moore http://www.glowtubeglow.com
 * (2017-2018)
 * No guarantees are epressed or implied.
 * 
 * 
 * This Software runs a rudimentary 4 digit calculator that can add, subtract, multiply, or divide. 
 * The input device is a rotary telephone that is wired using a simple wire interface (pullup resistors) 
 * to the rotary selector and to the hook switch on the phone (flash switch is the same). 
 * There is a simple printed circuit board available to provide this interface. Contact the author if desired. 
 * The hardware required are: 
 *    - Arduino 
 *    - Button interface (mentioned above)
 *    - Modified rotary phone 
 *    - Optional nixie header board for arduinix (also available from the author) 
 *    
 * The hook (phone cradle receive button called the "HOOK" or the "FLASH" button) is used for all state collection 
 * such as RESET, or SET CALCULATION (Add, Subtract, Multiply, Divide)
 * 
 * Software runs an elementary state loop IDLE, SLEEP, ROTARY ENTRY, HOOK BUTTON ENTRY.
 * 
 * Loop checks for input and sets state and drives the calculation. 
 * ==========================
 * Rotary - Enter digits left to right into the operands. 
 * 1-4 digits can be selected, each selection from the rotary places the digit into the next rightmost decimal position.  
 * For each calculation, there are two operands. The hook button is used to enter the final commit into the operand register (first or second). 
 * ==========================
 * Buttons (Rotary or hook) are interpreted and drive state changes or rotary input for operands in calculation. 
 * 
 * Hook button - Hook is interpreted in three ways: short, medium, and long presses. 
 * 
 * Short hook press = Press 1/2 to 1 second in duration. 
 * Short press is the "select" function. If hook is pressed between 1/2 and 1 second it is interpreted as a short press. 
 * Short press places the calculator into the next state (two states for the calculator)
 * The first short press after a reset or mode setting will enter the value on the display into the first calculating register. 
 * For example, if the rotary had previously entered 123 onto the nixie display, then 123 will go into the first value for the calculation. 
 * After a second value has been entered, a short HOOK press will enter the value 
 * into the calculator and execute the calculation and display the result on the nixies.  
 * 
 * Medium hook press = Press more than 1 second and less than 2 seconds. 
 * Medium press is used to select the calculation functional mode (+, -, X, or /) . 
 * Each medium press of the hook will rotate to the next calculation function:  ADD, then SUBTRACT, then MULTIPLY, then DIVIDE, then back to ADD, etc. 
 * The nixie will show which calculation is requested by displaying on the COLON dots in the middle of the numbers.  
 *    - The initial value is ADD which is displayed as the NO dots displayed at all. Colon dots are off for an ADD function
 *    - Next selection is the SUBTRACT which is displayed using the lower dot on the colon (like a period character).
 *    - MULTIPLY follows the SUBTRACT and is displayed as the upper dot of the colon. Just the top dot is lit. 
 *    - The DIVIDE function is the last in rotation and is represented with both dots of the colon being lit. A full colon is lit. 
 *    - Subsequent medium press will roll back to an ADD function. And the loop starts again. 
 * 
 * Long hook press = More than 2 seconds.
 * Long press is a RESET. A reset will put the calculator back to zero and into the add function.  
 * 
 * ============================
 * 
 * Over and underflows result in zeros being displayed. 
 * 
 * This is a 4 digit calculator so only 0-9999 can be displayed and no negative values are displayed.  
 * 
 * There is logic for Sleep mode to protect the nixies from cathode poisoning. THe device goes into sleep mode after 60 seconds. 
 * The sleep code is buggy and has not yet been refined. If the code goes into sleep mode, 
 * the nixie display will shut off and the devices will show a pulsing LED back-light. 
 * If it works properly, the device is awaken with a hook press of any length. If all else fails use the LONG hook press (RESET). 
 * 
 */
//#define RANDOM
#define VERSION ".01a"
// #define DEBUG
#define HOOK A3
#define ROTARY A2
#define LED A1
#define ADD 0
#define SUBTRACT 1
#define MULTIPLY 2
#define DIVIDE 3
#define HOME 0
#define SLEEP 1
#define ENTRY1 2
#define ENTRY2 3
#define TOTAL 4
#define FINAL 5
#define debounceDelay 10 // ms debounce - this is for rotary debounce adjust if necessary
#define fdebounceDelay 15 // ms debouce value - this is for flash hook
#define hookSleep 60000 // 1 minute idle goes to sleep 
#define hookLong 1750 // almost 2 seconds for a long press
#define hookMedium 850 // almost a second for a short press
#define hookShort fdebounceDelay
#define resetArray NumberArray[0]=0; NumberArray[1]=0; NumberArray[2]=0; NumberArray[3]=0; 
#define bumpState lastCalcState=calcState; ++calcState; 
#define ledOff 0    
#define ledLow 100
#define ledMedium 150
#define ledHigh 200
#define blinkDelay 1000  // sleep led blink length
// note: Becuase there are only pins A0-A5 available, the actual dimming will not work. 
// I just left the code in in case at some point, I will free up some other pins for PWM stealing from the arduinix by using pins 18-19. 

int sleeper = 0; // pulsing led value
boolean trigger = false;
boolean needToPrint = false;
int count;

int lastState = LOW;  // maybe I should have used booleans for these?
int trueState = LOW;
long lastStateChangeTime = 0;
int lastflash = 0;
int flastState = LOW;
int ftrueState = LOW;
long flastStateChangeTime = 0;
long sleepTimer = millis();
int flastflash = 0;
int cleared = 0;// constants
int dialHasFinishedRotatingAfterMs = 140; // was 100
//int debounceDelay = 15; //was 10 for rotary
//int fdebounceDelay = 10; // for flash hook
int operation = ADD;  // prime with ADD. Order is Add, SUBTRACT, MULTIPLY, DIVIDE
int calcState = ENTRY1; // Set to beginning state - HOME, ENTRY1, ENTRY2, TOTAL are current states
int lastCalcState = SLEEP;  // State will actually be changed in main loop
int operand1 = 0;
int operand2 = 0;
int result = 0;
long debugTimer = 0;
long hookTimer = 0;
int NumberArray[6] = {0, 0, 0, 0, 0, 0};  // Most importasnt array of all. Holds the current display being made to the nixies  

// Arduinix 4 tub board
// v1.0
//
// This code runs a 4 bulb tube board setup.
// NOTE: the delay is setup for IN-17 nixie bulbs but also works for IN-12. Others might need for rewiring anodes.
//
// Originally by Jeremy Howa - Heavily modified by M. Keith Moore for testing 4 character nixies.
// www.robotpirate.com
// www.arduinix.com
// 
//
// Note: Anod pin 3 is not used with this tube board but the code sets it up.
//
// Anod to number diagram
//
//
//          num array position
//            0   1   2   3
// Anod 0     #           #
// Anod 1         #   #
//
// Anod 1  Array #0 Colon 1
// Anod 0  Array #0 Colon 2

// SN74141 : Truth Table
//D C B A #
//L,L,L,L 0
//L,L,L,H 1
//L,L,H,L 2
//L,L,H,H 3
//L,H,L,L 4
//L,H,L,H 5
//L,H,H,L 6
//L,H,H,H 7
//H,L,L,L 8
//H,L,L,H 9

// SN74141 (1)


int ledPin_0_a = 2;
int ledPin_0_b = 3;
int ledPin_0_c = 4;
int ledPin_0_d = 5;
// SN74141 (2)
int ledPin_1_a = 6;
int ledPin_1_b = 7;
int ledPin_1_c = 8;
int ledPin_1_d = 9;

// anod pins
int ledPin_a_1 = 10;
int ledPin_a_2 = 11;
int ledPin_a_3 = 12;
int ledPin_a_4 = 13;
boolean randomFlag = false;

void setup()
{
  pinMode(ledPin_0_a, OUTPUT);
  pinMode(ledPin_0_b, OUTPUT);
  pinMode(ledPin_0_c, OUTPUT);
  pinMode(ledPin_0_d, OUTPUT);

  pinMode(ledPin_1_a, OUTPUT);
  pinMode(ledPin_1_b, OUTPUT);
  pinMode(ledPin_1_c, OUTPUT);
  pinMode(ledPin_1_d, OUTPUT);

  pinMode(ledPin_a_1, OUTPUT);
  pinMode(ledPin_a_2, OUTPUT);
  pinMode(ledPin_a_3, OUTPUT);
  pinMode(ledPin_a_4, OUTPUT);


  Serial.begin(9600); // OPen debug terminal
  Serial.print("Arduinix rotary phone dialer toy version - "); Serial.println(VERSION);
  pinMode( 14, INPUT ); // set the vertual pin 14 (pin 0 on the analog inputs )
  digitalWrite(14, HIGH); // set pin 14 as a pull up resistor.

  pinMode( 15, INPUT ); // set the vertual pin 15 (pin 1 on the analog inputs )
  digitalWrite(15, HIGH); // set pin 15 as a pull up resistor.
  pinMode(ROTARY, INPUT);

  analogWrite (LED,ledOff);
}

////////////////////////////////////////////////////////////////////////
//
// DisplayNumberSet
// Use: Passing anod number, and number for bulb 1 and bulb 2, this function
//      looks up the truth table and opens the correct outs from the arduino
//      to light the numbers given to this funciton (num1,num2).
//      On a 6 nixie bulb setup.
//
// Change to handle only one number at a time for testing purposes - MKM
//
////////////////////////////////////////////////////////////////////////
// void DisplayNumberSet( int anod, int num1, int num2 )
void DisplayNumberSet( int anod, int num1, int num2 )
{
  int anodPin;
  int a, b, c, d;

  // set defaults.
  a = 0; b = 0; c = 0; d = 0; // will display a zero.
  anodPin =  ledPin_a_1;     // default on first anod.

  // Select what anod to fire.
  switch ( anod )
  {
    case 0:    anodPin =  ledPin_a_1;    break;
    case 1:    anodPin =  ledPin_a_2;    break;
    case 2:    anodPin =  ledPin_a_3;    break;
    case 3:    anodPin =  ledPin_a_4;    break;
  }

  // Load the a,b,c,d.. to send to the SN74141 IC (1)
  switch ( num1 )
  {
    case 0: a = 0; b = 0; c = 0; d = 0; break;
    case 1: a = 1; b = 0; c = 0; d = 0; break;
    case 2: a = 0; b = 1; c = 0; d = 0; break;
    case 3: a = 1; b = 1; c = 0; d = 0; break;
    case 4: a = 0; b = 0; c = 1; d = 0; break;
    case 5: a = 1; b = 0; c = 1; d = 0; break;
    case 6: a = 0; b = 1; c = 1; d = 0; break;
    case 7: a = 1; b = 1; c = 1; d = 0; break;
    case 8: a = 0; b = 0; c = 0; d = 1; break;
    case 9: a = 1; b = 0; c = 0; d = 1; break;
    default: break;  // used to no-op the number in the array
  }

  // Write to output pins.
  digitalWrite(ledPin_0_d, d);
  digitalWrite(ledPin_0_c, c);
  digitalWrite(ledPin_0_b, b);
  digitalWrite(ledPin_0_a, a);

  // Load the a,b,c,d.. to send to the SN74141 IC (2)
  switch ( num2 )
  {
    case 0: a = 0; b = 0; c = 0; d = 0; break;
    case 1: a = 1; b = 0; c = 0; d = 0; break;
    case 2: a = 0; b = 1; c = 0; d = 0; break;
    case 3: a = 1; b = 1; c = 0; d = 0; break;
    case 4: a = 0; b = 0; c = 1; d = 0; break;
    case 5: a = 1; b = 0; c = 1; d = 0; break;
    case 6: a = 0; b = 1; c = 1; d = 0; break;
    case 7: a = 1; b = 1; c = 1; d = 0; break;
    case 8: a = 0; b = 0; c = 0; d = 1; break;
    case 9: a = 1; b = 0; c = 0; d = 1; break;
    default: break;
  }

  // Write to output pins
  digitalWrite(ledPin_1_d, d);
  digitalWrite(ledPin_1_c, c);
  digitalWrite(ledPin_1_b, b);
  digitalWrite(ledPin_1_a, a);

  // Turn on this anod.
  digitalWrite(anodPin, HIGH);

  // Delay
  // NOTE: With the difference in Nixie bulbs you may have to change
  //       this delay to set the update speed of the bulbs. If you
  //       dont wait long enough the bulb will be dim or not light at all
  //       you want to set this delay just right so that you have
  //       nice bright output yet quick enough so that you can multiplex with
  //       more bulbs.
  delay(3);

  // Shut off this anod.
  digitalWrite(anodPin, LOW);
}


////////////////////////////////////////////////////////////////////////
//
// DisplayNumberString
// Use: passing an array that is 8 elements long will display numbers
//      on a 6 nixie bulb setup.
//
////////////////////////////////////////////////////////////////////////
void DisplayNumberStringSingle( int* array )
{
 // bank 1 (bulb 0,3)
  DisplayNumberSet(0,array[0],array[3]);   
  // bank 2 (bulb 1,2)
  DisplayNumberSet(1,array[1],array[2]);
  if (array[4]<1) DisplayNumberSet(3,array[4],11);  // 11 is a dummy value to ignore in the code (fall-thru case/switch value)
  if (array[5]<1) DisplayNumberSet(2,11,array[5]); // this colon is turned on if 0  

}
////////////////////////////////////////////////////////////////////////
void DisplayNumberString( int* array )
{
  // bank 1 (bulb 0,3)
  DisplayNumberSet(0, array[0], array[3]);
  // bank 2 (bulb 1,2)
  DisplayNumberSet(1, array[1], array[2]);
  if (array[4] < 1) DisplayNumberSet(3, array[4], 11); // 11 is a dummy value to ignore in the code (fall-thru case/switch value)
  if (array[5] < 1) DisplayNumberSet(2, 11, array[5]); // this colon is turned on if 0
}

// DisplayNumberSet(2,0,1); // colon 1
// DisplayNumberSet(3,0,1); // colon 2

long runTime = 0;       // Time from when we started.
//************************************
long previous = 0;
long interval = 1000;
int colon = 1;

//************************************
/********************************************
   SIngle Digit DIsplays
*/

/********************************************
   Shift Digit DIsplays
*/
void shiftDigits(int digit){  
  long StartDuration = (millis()) / 1000;
  long EachSec = StartDuration;
  long NowTime = ((millis()) / 1000);

      NumberArray[0] = NumberArray[1];
      NumberArray[1] = NumberArray[2];
      NumberArray[2] = NumberArray[3];
      NumberArray[3] = digit;
      switch(operation){
          case ADD: {  // 0/0
            NumberArray[4] = 1;          //Digit 4, wire 0 (value 1 is off)
            NumberArray[5] = 1;          //Digit 4, wire 0 
            break;
          }
          case SUBTRACT: {  //0/1
            NumberArray[4] = 1;          //Digit 4, wire 0
            NumberArray[5] = 0;          //Digit 4, wire 0
            break; 
          }
          case MULTIPLY: {   // 1/0
            NumberArray[4] = 0;          //Digit 4, wire 0
            NumberArray[5] = 0;          //Digit 4, wire 0
            break; 
          }
          case DIVIDE: {   // 1/1
            NumberArray[4] = 0;          //Digit 4, wire 0
            NumberArray[5] = 1;          //Digit 4, wire 0
            break;
          }
          default: {   // 0/0 
            NumberArray[4] = 1;          //Digit 4, wire 0
            NumberArray[5] = 1;          //Digit 4, wire 0 
            break;
          }        
        }; // end switch 
#ifdef DEBUG
      Serial.print("All-Array0=");Serial.print(NumberArray[0]);Serial.print(" - ");
      Serial.print("Array1=");Serial.print(NumberArray[1]);Serial.print(" - ");
      Serial.print("Array2=");Serial.print(NumberArray[2]);Serial.print(" - ");
      Serial.print("Array3=");Serial.print(NumberArray[3]);Serial.print(" -");
      Serial.print("Top=");Serial.print(NumberArray[4]);Serial.print(" - ");
      Serial.print("Bottom=");Serial.print(NumberArray[5]);Serial.println(". ");
#endif
      while ((EachSec=(millis()/1000) == NowTime)){ 
        DisplayNumberStringSingle( NumberArray );
 //       delay(250); // dummy for test
        };
}

/********************************************
   Shift Digit DIsplays
*/
int enumerate (){  
    
    int d3, d2, d1, d0 = 0;
      d3 = (NumberArray[0] * 1000);
      d2 = d3 + (NumberArray[1] * 100);
      d1 = d2 + (NumberArray[2] * 10);
      d0 = d1 + NumberArray[3];
 //     Serial.print("------------------->Enumerated digits="); Serial.println(d0);
      return d0;
} // end of enumerate
void unenumerate(int number){  


      int micro = number / 1000 ;
 //     Serial.println(micro);
      int milli =  (number % 1000) / 100;
 //      Serial.println(milli);
       
      int deca = (number % 100) / 10;
 //           Serial.println(deca);
      int digit = (number % 10);
  //          Serial.println(digit);
            

  // Fill in the Number array used to display on the tubes.
  //int NumberArray[6]={0,0,0,0,0,0};
  NumberArray[0] = micro;
  NumberArray[1] = milli;
  NumberArray[2] = deca;
  NumberArray[3] = digit;
#ifdef DEBUG
   Serial.print("All-Array0=");Serial.print(NumberArray[0]);Serial.print(" - ");
      Serial.print("Array1=");Serial.print(NumberArray[1]);Serial.print(" - ");
      Serial.print("Array2=");Serial.print(NumberArray[2]);Serial.print(" - ");
      Serial.print("Array3=");Serial.print(NumberArray[3]);Serial.print(" -");
      Serial.print("Top=");Serial.print(NumberArray[4]);Serial.print(" - ");
      Serial.print("Bottom=");Serial.print(NumberArray[5]);Serial.println(". ");
#endif
} // end of unenumerate
int calculate (int operand1,int operand2,int operation){
  int val; 
  
//  Serial.print("Operation="); Serial.print(operation); Serial.print(" Operand1="); Serial.print(operand1); Serial.print(" Operand2="); Serial.print(operand2);
  switch (operation){
    case ADD: {
//  Serial.print("Trying to ADD and I get ->");
      val = (operand1 + operand2);
 //     Serial.println(val);
      if (val > 9999) {
        val=9999;};
 //     Serial.println(val);  
      break;
    }
    case SUBTRACT: {
//  Serial.println("Trying to SUBTRACT");
  if (operand1 >= operand2) {
        val=operand1-operand2;
      } else
        val = 0; // negative number
      break;
    }
    case DIVIDE: {
 // Serial.println("Trying to DIVIDE");
  if ((operand2 <1) || (operand2 > operand1)) {
        val = 9999;
      } else
        val = round(operand1/operand2);
      break;
    }
    case MULTIPLY: {
 //       Serial.println("Trying to MULTIPLY");
      val=operand1*operand2;
      if (val > 9999) val = 9999;
      break;
    }
    default : {
 //     Serial.println("DEFAULTING in calculate!");
      val=0000;
      break;
      }

  };
#ifdef DEBUG
Serial.print("------------------> Result=>"); Serial.println(val);
#endif

    return val; 
}; // end of calculate

////////////////////////////////////////////////////////////////////////
void loop()
{
  int reading = digitalRead(ROTARY);
  int freading = digitalRead(HOOK);
  int digit = 0; // holds the current end-state decimal 0-9 digit received from rotary
  boolean flash = false;

  if (reading != lastState) {
      lastState=reading; // Set last state to current state  
      sleepTimer = lastStateChangeTime = millis();
      delay (debounceDelay);  // debounce 
//      Serial.print("tick..");
     }
 
  if ((millis() - lastStateChangeTime) > dialHasFinishedRotatingAfterMs) {// the dial isn't being dialed, or has just finished being dialed.
    if (needToPrint) {// if it's only just finished being dialed, we need to send the number down the serial
      // line and reset the count. We mod the count by 10 because '0' will send 10 pulses.
        if (calcState == TOTAL || calcState == HOME){  /// we finished the last calulation so we assume a new one
          analogWrite(LED,ledLow);
          resetArray;
          operand1 = operand2 = 0;
          calcState = ENTRY1;
        };
        needToPrint = false;
        digit=count;
        count = 0;
        cleared = 0;
        shiftDigits(digit);  // shift and display the current value. 
      }; // end need to print
    }; // end finished rotary dial read
  
  if (reading != lastState) {
      lastState = reading; // lastState is reset upon a change
      sleepTimer = lastStateChangeTime = millis();
      delay (debounceDelay*2);
  //    Serial.println("time change");
     }
  
  if ((millis() - lastStateChangeTime) > debounceDelay) {// debounce - this happens once it's stablized
    if (reading != trueState) {// this means that the switch has either just gone from closed->open or vice versa.
      trueState = reading;
    //  Serial.print(count % 10, DEC);
      if (trueState == HIGH) {// increment the count of pulses if it's gone high.
        count++;needToPrint = true; // we'll need to print this number (once the dial has finished rotating)
      }
    }
// Check the flash button now. 
    if (freading != flastState) { 
          flastState = freading; // lastState is reset upon a change
          sleepTimer = flastStateChangeTime = millis();
    //      Serial.print("ZReset.");
          delay (fdebounceDelay);  // debounce 
  //        Serial.print("hick..");
    };
    if ((millis() - flastStateChangeTime) > fdebounceDelay) {// the dial isn't being dialed, or has just finished being dialed.
      if (freading != ftrueState) {// this means that the switch has either just gone from closed->open or vice versa.
          ftrueState = freading;
      if (ftrueState == HIGH) {// increment the count of pulses if it's gone high.
  //       Serial.println("Hook+");
         hookTimer = millis();
         if (calcState == ENTRY1){
          analogWrite(LED,ledLow);
         };
      } else
      {   // then it must be a rest back to off. Flash was on but now it is off. 
        if ((flastStateChangeTime - hookTimer) > hookLong){
   //       Serial.println("Long Hook");
          calcState = HOME;
          analogWrite(LED,ledLow);
          trigger = true; 
        } else
        if ((flastStateChangeTime - hookTimer) > hookMedium){
  //        Serial.print("Medium hook - changed operation to ");
            if (++operation > DIVIDE) { // shift to the next operation
              operation = ADD; // rest to ADD    
            };
  //          Serial.println(operation);
        } else
        if ((flastStateChangeTime - hookTimer) > hookShort){
  //        Serial.println("Short hook");
          if (calcState == ENTRY1) {
            operand1 = enumerate();
            bumpState;
            resetArray;
  //          analogWrite(LED,ledMedium);
            operand2 = 0; 
   //         Serial.print("Entry1 operand1="); Serial.println(operand1);   
          } else
          if (calcState == ENTRY2) {
              operand2 = enumerate();
              bumpState;
              result = 0;
              trigger = true;
  //            Serial.print("Entry2 operand2="); Serial.println(operand2);     
          };
        }; // short hook
      } // else flash length logic
     }  // end switch changed state
    } // end flash was detected
  };
 //  End of flash analysis
 //         Serial.print("Sleepy time="); Serial.print(sleepTimer); Serial.print(" Timer val="); Serial.println(millis()-sleepTimer); delay(200);
        if (((millis() - sleepTimer) > hookSleep) || ((millis() - lastStateChangeTime) > hookSleep)){
 //      if (((millis() - sleepTimer) > hookSleep)){
//          Serial.print("Sleepy time"); Serial.print(sleepTimer); Serial.print(" Timer val="); Serial.println(millis()-sleepTimer);   
              calcState=SLEEP; 
            trigger = true; 
        }

  if (trigger){
    trigger = false; 
//    Serial.println("trigger reset");
    switch (calcState) {
      default: {
        break;
      }
      case SLEEP: { // Sleep
        sleeper=0;  // set off at the start pulsate until flash is changed. 
 //       Serial.println("Zzzzz");

        while (sleepTimer > 0){
          while (++sleeper < blinkDelay) {
            digitalWrite(LED,HIGH);
            delay (1);
          }
        freading = digitalRead(HOOK);
        reading = digitalRead(ROTARY);
        if (freading){
            digitalWrite(LED,LOW);
            sleepTimer = 0;
          }
          while (--sleeper > 1) {
            digitalWrite(LED,LOW);
            delay (1);
          }
//          freading = digitalRead(HOOK);
//          reading = digitalRead(ROTARY);
          if (freading){
            digitalWrite(LED,LOW);
            sleepTimer = 0;
          }
        }; // no change in flash read
//        Serial.println("Waken!");
        lastStateChangeTime = sleepTimer = millis();
        break;
      }
      case TOTAL: {
   //     Serial.print("Totals executed...");
        result = calculate (operand1,operand2,operation);
  //      Serial.println(result);
        unenumerate (result);
  //      calcState = ENTRY1; 
        analogWrite(LED,ledHigh);
        break;
      }
      case HOME: {
        operand1 = operand2 = operation = result = 0;
        resetArray;
        calcState = ENTRY1;
        analogWrite(LED,ledLow);
        break;
      } 
    }; // switch calcState
  }; // trigger

  switch(operation){
          case ADD: {  // 0/0
            NumberArray[4] = 1;          //Digit 4, wire 0 (value 1 is off)
            NumberArray[5] = 1;          //Digit 4, wire 0 
            break;
          }
          case SUBTRACT: {  //0/1
            NumberArray[4] = 1;          //Digit 4, wire 0
            NumberArray[5] = 0;          //Digit 4, wire 0
            break; 
          }
          case MULTIPLY: {   // 1/0
            NumberArray[4] = 0;          //Digit 4, wire 0
            NumberArray[5] = 0;          //Digit 4, wire 0
            break; 
          }
          case DIVIDE: {   // 1/1
            NumberArray[4] = 0;          //Digit 4, wire 0
            NumberArray[5] = 1;          //Digit 4, wire 0
            break;
          }
          default: {   // 0/0 
            NumberArray[4] = 1;          //Digit 4, wire 0
            NumberArray[5] = 1;          //Digit 4, wire 0 
            break;
          }        
    }; // end switch 
    DisplayNumberString(NumberArray);

#ifdef DEBUG 
  if (millis() - debugTimer > 5000 ){   // every 5 seconds
    debugTimer = millis();
    Serial.println("Debug Display");
    Serial.println("=============");
    Serial.print("Operation="); Serial.println(operation);
    Serial.print("Operand1="); Serial.println(operand1);
    Serial.print("Operand2="); Serial.println(operand2);
    Serial.print("Result="); Serial.println(result);
    Serial.print("calcState="); Serial.println(calcState);
    Serial.print("lastCalcState="); Serial.println(lastCalcState);
     Serial.print("Array 0=");Serial.print(NumberArray[0]);Serial.print(" - ");
      Serial.print("1=");Serial.print(NumberArray[1]);Serial.print(" - ");
      Serial.print("2=");Serial.print(NumberArray[2]);Serial.print(" - ");
      Serial.print("3=");Serial.print(NumberArray[3]);Serial.print(" -");
      Serial.print("Top=");Serial.print(NumberArray[4]);Serial.print(" - ");
      Serial.print("Bottom=");Serial.print(NumberArray[5]);Serial.println(". ");
  };  // end of debug display
#endif
} // End of loop
