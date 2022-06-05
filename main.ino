#include "TimerOne.h" // Timer Interrupt 

#define GeneratorOutput_PIN  6  // Arduino pin tied to generator
#define Serial_BaudRate 9600
#define CommandID_PingPong 0
#define CommandID_PulseWidth 1
#define CommandID_PulseHigh 2
#define CommandID_GetSettings 5
#define CommandID_Reset 9
#define TimerInterrruptInterval 500

/* *************************************************************
   declare functions
   ************************************************************* */
// declare Software reset functionality.
void(* resetFunc) (void) = 0;  // declare reset fuction at address 0


/* *************************************************************
   declare global variables
   ************************************************************* */
const int BUFFER_SIZE = 32;
uint8_t  SerialIncomming[BUFFER_SIZE];
int SerialIncomming_Lenght = 0;
int Serial_index=-1;  // -1 equals no valid data in SerialIncomming buffer
unsigned int PulseWidthHigh = 2;
unsigned int PulseWidthLow  = 1;
volatile unsigned int InterruptCnt = 0;
volatile unsigned int InterruptNextEvent = 0;


// ----------------------------------------------------------
/// Setup
// ----------------------------------------------------------
void setup()
{
  // Initialization code
  Serial.begin(Serial_BaudRate);
  Serial.println("Initialized_sw uart");
    // disable interrupts
  cli();
  // set up interrupt to detect rotations
  pinMode(GeneratorOutput_PIN, OUTPUT);
  digitalWrite (GeneratorOutput_PIN, LOW); // set default state
  //attachInterrupt(digitalPinToInterrupt(WIND_SPEED_PIN), rotationInterrupt, FALLING);
  
  // Setup the timer interupt
  //Timer1.initialize(1000000);// Note for accuracy:For Timer interrupt every 1 second - tested to be +/-16 msec over 15 minutes, slowly drifting at room temp to be a bit fast but within margin.
  Timer1.initialize(TimerInterrruptInterval);// call timer interrupt routine every 500 usec
  Timer1.attachInterrupt(timerInterrupt);
  // enable interrupts
  sei();
  Serial.println("Boot completed, generator enabled with default values - 1000 Hz, 50%% duty cycle");
  Serial.println("Press 'H' for help");
}


// ----------------------------------------------------------
/// Main loop
// ----------------------------------------------------------
void loop() 
{
 
  if (Serial.available()) 
  {   
    byte iByte = Serial.read();
    if (iByte == 'H')
    {
      Serial.println("----------------------------------------------------------");
      Serial.println("Generator using Arduino Nano, minumum step is 500 usec");
      Serial.println("Set high pulse width in usec: [1%%xxxx]");
      Serial.println("Set low pulse width in usec: [2%%xxxx]");
      Serial.println("   where xxxx is time in usec rouneded to nearest 500 usec");
      Serial.println("Get current settings: [5%%]");
      Serial.println("Reset to default    : [9%%]");
      Serial.println("----------------------------------------------------------");
      
      Serial_index = -1;      // reset communication
    }
    if (iByte == '[')
    {
      //Serial.println("start of msg"); // debug
      Serial_index = 0;
    }
    else if (Serial_index>=0 && Serial_index < BUFFER_SIZE) 
    {
      SerialIncomming[Serial_index++] = iByte;  // adding to message
      //Serial.println("."); // debug

      if (iByte == ']') 
      {
          // Serial.println("end of msg"); // debug
          SerialIncomming_Lenght = Serial_index-1;
          // call process function - evaluate and handle potential command.
          HandleSerialCommand();
          Serial_index = -1;

      }
    }
    else if (Serial_index >= BUFFER_SIZE)
    {
      Serial.println("Communication Reset-Message too long");
      Serial_index = -1;
    }
  }
}

void HandleSerialCommand()
{
  Serial.println("evaluating message");
  /* ***********************************************************************
      eval command format:
      SerialIncomming[0] & SerialIncomming[1] is command number
      SerialIncomming[2] is command / data separation character : %
      SerialIncomming[3] and up is command data if any
      SerialIncomming[SerialIncomming_Lenght] is "end of message" character
  /* *********************************************************************** */
  if (SerialIncomming[1] == '%' && SerialIncomming[SerialIncomming_Lenght] == ']')
  {
    //Serial.println("message has the right structure");  // debug
    if (SerialIncomming[0] == '0') // ping pong message
    {
      Serial.println("command 00 confirm"); // [00%]
      // no further action.
    }
    
    else if (SerialIncomming[0] == '1')  // set width high
    {
      char input[SerialIncomming_Lenght-1] ; 
      for (int i = 2 ; i <= SerialIncomming_Lenght ; i++)
      {
        input[i-2] = SerialIncomming[i];
      } 
      input[SerialIncomming_Lenght-1] ="\n";
      long value1 = atol(input);
      PulseWidthHigh = (unsigned int)(value1 / TimerInterrruptInterval);
            
      if (PulseWidthHigh< 1)
        PulseWidthHigh = 1;  // handle special case

      Serial.print("New logic high width: "); 
      Serial.println(PulseWidthHigh * TimerInterrruptInterval); 
    }
    else if (SerialIncomming[0] == '2')  // set width low
    {
      char input[SerialIncomming_Lenght-1] ; 
      for (int i = 2 ; i <= SerialIncomming_Lenght ; i++)
      {
        input[i-2] = SerialIncomming[i];
      } 
      input[SerialIncomming_Lenght-1] ="\n";
      long value1 = atol(input);
      PulseWidthLow = (unsigned int)(value1 / TimerInterrruptInterval);
      if (PulseWidthLow< 1)
          PulseWidthLow = 1;  // handle special case
      
      Serial.print("New logic Low width: "); 
      Serial.println(PulseWidthLow* TimerInterrruptInterval); 
    }
    else if (SerialIncomming[0]  == '5')  // get settings
    {
      Serial.println("\nCurrent settings:"); 
      Serial.print("Logic high width: ");
      Serial.print(PulseWidthHigh * TimerInterrruptInterval); 
      Serial.println(" usec"); 
      Serial.print("Logic low width : ");
      Serial.print(PulseWidthLow * TimerInterrruptInterval); 
      Serial.println(" usec"); 
    }
    else if (SerialIncomming[0] == '9')  // request software reset message
    {
      Serial.println("command 9 confirm"); // [9%]
      // performing software reset after a short period, so that serial message is sent
      delay(500);
      resetFunc(); //call reset
    }
    else
      Serial.println("command not supported");     
  }
  else
    Serial.println("invalid message - discarding");
}

void timerInterrupt()
{
  InterruptCnt++; // variable to keep track of overall timing.

  if (InterruptCnt>=InterruptNextEvent)
  {
    bool val = digitalRead(GeneratorOutput_PIN);  // read the input pin
    digitalWrite(GeneratorOutput_PIN, !val);      // set the inverted value
    if (val == 1)
    {
      InterruptNextEvent = InterruptCnt + PulseWidthLow;
    }
    else
      InterruptNextEvent = InterruptCnt + PulseWidthHigh;
  }
}


