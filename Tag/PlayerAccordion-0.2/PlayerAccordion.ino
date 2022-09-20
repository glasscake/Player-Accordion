

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();


//definitions
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates
#define TotalServos 16

// our servo # counter
int ServoMedian = 280;
int tester = 0;

elapsedMillis CurrentMillis;
unsigned long TesterTimer = 0;
unsigned long KeyDelayTime = 130; //Just slightly longer than the slowest key

unsigned long IOEOnByteTimer[TotalServos];
unsigned long IOEOffByteTimer[TotalServos];

//initialize the Servo Off value
                      //0,1,2,3,4,5,6,7,8,9
int SCV[TotalServos] = {290,285,290,290,0,0,0,0,0,0,
                        0,0,0,0,0,0};
//Initalize the servo ON value
                      //0,1,2,3,4,5,6,7,8,9
int SOV[TotalServos] = {320,310,320,320,0,0,0,0,0,0,
                        0,0,0,0,0,0};
//Initalize the on delay                            //0,1,2,3,4,5,6,7,8,9
unsigned long IOEOnByteTimerModifer[TotalServos] =   {0,0,0,0,0,0,0,0,0,0,
                                                      0,0,0,0,0,0};
//Initialize the OFF delay
unsigned long IOEOffByteTimerModifer[TotalServos] =  {0,0,0,0,0,0,0,0,0,0,
                                                      0,0,0,0,0,0};
void setup() 
{
  Serial.begin(115200);
  Serial.println("8 channel Servo test!");
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);  // Analog servos run at ~50 Hz updates
  delay(10);
}
// You can use this function if you'd like to set the pulse length in seconds
// e.g. setServoPulse(0, 0.001) is a ~1 millisecond pulse width. It's not precise!
void setServoPulse(uint8_t n, double pulse) {
  double pulselength;
  pulselength = 1000000;   // 1,000,000 us per second
  Serial.print(pulselength); Serial.println(" us per period"); 
  pulselength /= 4096;  // 12 bits of resolution
  Serial.print(pulselength); Serial.println(" us per bit"); 
  pulse *= 1000000;  // convert input seconds to us
  pulse /= pulselength;
  Serial.println(pulse);
  pwm.setPWM(n, 0, pulse);
}

void loop() {
//serial monitor
  if (Serial.available() != 0)
  {
    SerialParser();
  }
//end serial monitor
/*
if(CurrentMillis > TesterTimer)
{
TesterTimer = CurrentMillis + 500;
for(int i = 0, s = 0, c = 1; i < TotalServos; i++)
  {
    //Serial.println("for loop");
  //check if i has gone past the first 16
  if( i > 16*c)
  {
    s = 0;
    c++;
  }
  if(tester == 0)
  {
    //Serial.print("Closed");
    //Serial.print(s);
    pwm.setPWM(s,0,SCV[s]);
  }
  else
  {
    //Serial.println("Open");
    pwm.setPWM(s,0,SOV[s]);
  }
  s++;
  }
  if(tester == 0)
  {
    tester = 1;
  }
  else
  {
    tester = 0;
  }
}
*/
//end loop
}

int SerialParser()
  {
    //get serial data in as a string
    String SerialIn = SerialReader();
    char CommandIdentifer;
    int Commas = CommaCount(SerialIn);                   //how many commas are in the string
    if (Commas > 0)
    {
      CommandIdentifer = SerialIn.charAt(0);                //retains the first idenifer
      //CommandIdentifer = SerialIn.substring(0,SerialIn.indexOf(",")-1);//return the command
      //SerialIn = SerialIn.substring(1, SerialIn.length()); //deletes the first identifer
      int Settings[Commas];                                //array of setting values
      for(int i = 0, fc = 0, nc = 0; i < Commas; i++)  //loop that parses the string info
      {
        //fc - first comma, nc - next comma
        fc = SerialIn.indexOf(",", nc);
        nc = SerialIn.indexOf(",", fc+1);
        if(nc == -1)
        {
          Settings[i] = SerialIn.substring(fc+1, SerialIn.length()).toInt();
          //Serial.println(Settings[i]);
        }
        else
        {
          Settings[i] = SerialIn.substring(fc+1, nc).toInt();
          //Serial.println(Settings[i]);
        }
      }
      switch(CommandIdentifer)
      {
        case 't':         //turn servo on or off
          if(Settings[1] == 1) //if command to turn on
          {
            pwm.setPWM(Settings[0],0,SOV[Settings[0]]); 
          }                                   
          else
          {
            pwm.setPWM(Settings[0],0,SCV[Settings[0]]); 
          }
          break;
        case 'i':         //set servo open/closed values
          if(Settings[1] == 1)
          {
            SOV[Settings[0]] = Settings[2];
          }
          else
          {
            SCV[Settings[0]] = Settings[2];
          }
          break;
        case 's':       //change the address of the servo board
          //placeholder
          break;
        case 'm':       //set servo to median position
          pwm.setPWM(Settings[0],0,ServoMedian);
          break;
        case 'n':       //set median position value
          ServoMedian = Settings[0];
          break;
        case 'c':       //set servo to position
          Serial.println("Moving Servo");
          pwm.setPWM(Settings[0],0,Settings[1]);
          break;
        default:
          Serial.println("unrecognized first character");
          break;
      }
    }
    else
    {
      if(SerialIn == "sam")//Set all servos median
      {
        SetAllServosMedian();
      }
      if(SerialIn == "report")
      {
        Serial.println("servo On values");
        PrintArray(SOV);
        Serial.println("servo Off Values");
        PrintArray(SCV);
      }
      if(SerialIn == "stress")
      {
      }
  }
      //commands with no parsing required or with the first comma not at position 1
}

void PrintArray(int Array[])
{
  for (int i = 0; i < sizeof(Array); i++)
  {
    Serial.print(Array[i]);
    Serial.print(",");
  }
}

int CommaCount(String input)
{
  int LCP = 0; //last comma position
  int TCC = 0; //total comma count
  LCP = input.indexOf(",");
  if(LCP == -1)         //no commas found
  {
    return -1;
  }
  else if(LCP != 1)     //comma in the wrong location
  {
    return -2;
  }
  TCC++;
  while(input.indexOf(",",LCP) != -1)
  {
    LCP = input.indexOf(",",LCP+1);
    TCC++;
  }
  return TCC-1;
}

String SerialReader()
{
  String SERIALIn;
  byte SerialByte;
  while (Serial.available() != 0)
  {
    SerialByte = Serial.read();
    SERIALIn = SERIALIn + char(SerialByte);
  }
  return SERIALIn;
}

void SetAllServosMedian()
{
  for(int i = 0, s = 0, c = 1; i < TotalServos; i++)
  {
    //check if i has gone past the first 16
    if( i > 16*c)
    {
      s = 0;
      c++;
    }
    pwm.setPWM(s,0,ServoMedian);
    s++;
  }
}
