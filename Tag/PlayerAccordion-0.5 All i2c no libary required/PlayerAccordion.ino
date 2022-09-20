

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();


//definitions
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates
#define TotalServos 16
#define TotalServoBoards 1
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
  //use the prebuilt libary to handle configuring the boards
  /*
  delay(5000);
  Serial.println("PWM.Begin");
  pwm.begin();
  Serial.println("PWM.setoscillatorfrequency");
  pwm.setOscillatorFrequency(27000000);
  Serial.println("PWM.setpwmfreq");
  pwm.setPWMFreq(SERVO_FREQ);  // Analog servos run at ~50 Hz updates
  delay(10);
  */
  InitializeServoBoard(0);
  Wire.begin();
  Wire.setClock(100000);
  //InitializeServoBoard(0);
}
// You can use this function if you'd like to set the pulse length in seconds
// e.g. setServoPulse(0, 0.001) is a ~1 millisecond pulse width. It's not precise!


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
        case'z':
                      //address,servo,value
          SetServo(0,Settings[0],Settings[1]);
          Serial.println("I2C Move Command");
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
      if(SerialIn == "Init")
      {
        Serial.println("initalizing");
        InitializeServoBoard(0);
        //directcopy();
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

/*
void InitializeServoBoards()
{
  for(int i = 0; i < TotalServoBoards; i++)
  {
    Adafruit_PWMServoDriver.begin(0x40+i);
    Adafruit_PWMServoDriver.setOscillatorFrequency(27000000);
    Adafruit_PWMServoDriver.setPWMFreq(SERVO_FREQ);  // Analog servos run at ~50 Hz updates
    delay(10);
  }
}
*/

//i2c stuff
//addres for board with no pullups : 0x40


void SetServo(uint8_t Address,uint8_t ServoNum,uint16_t PWMValue)
{
  //default address
  int I2CAddress = 0x40 + Address;
  //try setting a servo to a position
  Wire.beginTransmission(I2CAddress);
  //write what led to use
  Wire.write(0x06+4*ServoNum);//set the servo sub address
  Wire.write(0);//write the on time
  Wire.write(0>>8);//not sure what the bit shift is doing here
  Wire.write(PWMValue);
  Wire.write(PWMValue>>8);
  Wire.endTransmission();
}


void InitializeServoBoard(int Address)
{
  //read address 0
  int I2CAddy = 0x40 + Address;
  WriteToSubAddress(I2CAddy,0x00,0x30);
  WriteToSubAddress(I2CAddy,0xFE,0x83);
  WriteToSubAddress(I2CAddy,0x00,0x20);
  WriteToSubAddress(I2CAddy,0x00,0xA0);
}


void WriteToSubAddress(uint8_t ServoAddy, uint8_t SubAddy, uint8_t value)
{
  /*
  Serial.print("WriteToSubAddress(0x");
  Serial.print(ServoAddy, HEX);
  Serial.print(",0x");
  Serial.print(SubAddy, HEX);
  Serial.print(",0x");
  Serial.print(value, HEX);
  Serial.println(");");
  */
  //Serial.print("ServoAddress: ");Serial.println(ServoAddy, HEX);
  Wire.beginTransmission(ServoAddy);
  //Serial.print("subaddress: ");Serial.println(SubAddy, HEX);
  Wire.write(SubAddy);
  //Serial.print("value: ");Serial.println(value, HEX);
  Wire.write(value);
  Wire.endTransmission(); 
  delay(50);
}
