
//inclusions
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
//end inclussions

//definitions
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates
#define TotalServos 31
//subtract one from the total amount of servo boards (done to save on processing time)
//example: 3 servoboards, a value of 2 is put in
#define TotalServoBoards 1
//end inclusions

//global variables
int ServoMedian = 280;

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
  delay(1500);
  Wire.begin();
  Wire.setClock(100000);
  InitializeAllServoBoard();
}

void loop() {
  //start loop
//serial monitor
  if (Serial.available() != 0)
  {
    SerialParser();
  }
//end serial monitor

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
           SetGlobalServo(Settings[0],SOV[Settings[0]]);
          }                                   
          else
          {
            SetGlobalServo(Settings[0],SCV[Settings[0]]); 
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
        case 'm':       //set servo to median position
          SetGlobalServo(Settings[0],ServoMedian);
          break;
        case 'n':       //set median position value
          ServoMedian = Settings[0];
          break;
        case 'c':       //set servo to position
          Serial.println("Moving Servo");
          SetGlobalServo(Settings[0],Settings[1]);
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
  for(int i = 0; i <= TotalServos; i++)
  {
    SetGlobalServo(i, ServoMedian);
    //Serial.print("servo: ");Serial.println(i);
  }
}

//i2c stuff
//addres for board with no pullups : 0x40

void SetLocalServo(uint8_t Address,uint8_t ServoNum,uint16_t PWMValue)
{
  //default address = 0x40
  uint8_t I2CAddress = 0x40 + Address;
  //Serial.print("I2Caddress: 0x");Serial.println(I2CAddress, HEX);
  Wire.beginTransmission(I2CAddress);
  Wire.write(0x06+4*ServoNum); //Start writing to the servo off position
  Wire.write(0);//write the on ?delay? time to 0 (turns the servo on instiantly)
  Wire.write(0>>8);//shift bits for the second byte
  Wire.write(PWMValue); //write the time the servo spends off
  Wire.write(PWMValue>>8);  //shift the bits for the second byte
  Wire.endTransmission();   //end the transmission
}

void SetGlobalServo(uint8_t ServoNumber, uint16_t PWMValue)
{
   int ServoBoardNum = DetermineServoBoard(ServoNumber);
   ServoNumber = ServoNumber - (ServoBoardNum*16);
   SetLocalServo(ServoBoardNum, ServoNumber, PWMValue);
} 

int DetermineServoBoard(int GlobalServoNumber)
{
  switch (GlobalServoNumber)
  {
    case 0 ... 15:
      return 0;
    case 16 ... 31:
      return 1;
    case 32 ... 47:
      return 2;
    case 48 ... 63:
      return 3;
  }
}

//make a function that resets the servo number back to 0

void InitializeServoBoard(int Address)
{
  //read address 0
  int I2CAddy = 0x40 + Address;
  WriteToSubAddress(I2CAddy,0x00,0x30);
  WriteToSubAddress(I2CAddy,0xFE,0x83);
  WriteToSubAddress(I2CAddy,0x00,0x20);
  WriteToSubAddress(I2CAddy,0x00,0xA0);
}

void InitializeAllServoBoard()
{
  //read address 0
  byte I2CAddy = 64;
  for(byte i = 0; i <= TotalServoBoards; i++)
  {
    I2CAddy = I2CAddy + i;
    //Serial.print("InitServo: 0x");Serial.println(I2CAddy, HEX);
    //Serial.print("i: ");Serial.println(i);
    WriteToSubAddress(I2CAddy,0x00,0x30);   //set board to sleep
    WriteToSubAddress(I2CAddy,0xFE,0x83);   //change prescaller
    WriteToSubAddress(I2CAddy,0x00,0x20);   //set board awake
    WriteToSubAddress(I2CAddy,0x00,0xA0);   //set board auto index
  }
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
  Wire.beginTransmission(ServoAddy);
  Wire.write(SubAddy);
  Wire.write(value);
  Wire.endTransmission(); 
  delay(5);
}
