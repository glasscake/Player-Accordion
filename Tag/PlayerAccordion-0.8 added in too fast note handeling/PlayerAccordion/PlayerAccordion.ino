//inclusions
#include <Wire.h>
#include <NativeEthernet.h>
#include <AppleMIDI.h>
//end inclussions

//apple midi stuff
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEC};
            //169.254.142.141
byte ip[] = {169,254,142,145};
APPLEMIDI_CREATE_DEFAULTSESSION_INSTANCE();
int Mic = A9;
//end apple midi

#define DEBUG true
#ifdef DEBUG
#define DEBUG_PRINT(x)     Serial.print (x)
#define DEBUG_PRINTLN(x)  Serial.println (x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x) 
#endif

//DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINT(" Position: ")

//definitions
//total number of possible servos
#define TotalServos 79
//subtract one from t2he total amount of servo boards (done to save on confusion where servoboard 0 is a single board)
//example: 3 servoboards, a value of 2 is put in
#define TotalServoBoards 4

//Settings
#define DSOTS false  //default settings on track start
#define CVOTS false  //Change voice on track start
#define ACV   false //auto change the voice to for notes. Can cause collisions
#define RHC 3       //right hand channel
#define LHC 4       //left hand channel

#define LPNL 36      //lowest playable midi note - Left Hand
#define HPNL 81      //highest playable midi note - Left Hand
#define LPNR 52      //lowest playable midi note - Right Hand
#define HPNR 93      //highest playable midi note - Right Hand
//end inclusions

//global variables
int ServoMedian = 280;

elapsedMillis CurrentMillis;
unsigned long KeyDelayTime = 130; //Just slightly longer than the slowest key
unsigned long IOEOnByteTimer[TotalServos+1];
unsigned long IOEOffByteTimer[TotalServos+1];
int Position = 0;
int CRHV  = 0;  //current right hand voice
int CLHV  = 0;  //current left hand voice

//initialize the Servo Off value
                      //0,1,2,3,4,5,6,7,8,9
int SCV[TotalServos+1] =                             {300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300};
//Initalize the servo ON value
                      //0,1,2,3,4,5,6,7,8,9
int SOV[TotalServos+1] =                              {300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300,
                                                      300,300,300,300,300,300,300,300,300,300};
                                                      
//Initalize the on delay                            //0,1,2,3,4,5,6,7,8,9
unsigned long IOEOnByteTimerModifer[TotalServos+1] =   {100,100,100,100,100,100,100,100,100,100,
                                                      100,100,100,100,100,100,100,100,100,100,
                                                      100,100,100,100,100,100,100,100,100,100,
                                                      100,100,100,100,100,100,100,100,100,100,
                                                      100,100,100,100,100,100,100,100,100,100,
                                                      100,100,100,100,100,100,100,100,100,100,
                                                      100,100,100,100,100,100,100,100,100,100,
                                                      100,100,100,100,100,100,100,100,100,100};
//Initialize the OFF delay
unsigned long IOEOffByteTimerModifer[TotalServos+1] =  {10,10,10,10,10,10,10,10,10,10,
                                                      10,10,10,10,10,10,10,10,10,10,
                                                      10,10,10,10,10,10,10,10,10,10,
                                                      10,10,10,10,10,10,10,10,10,10,
                                                      10,10,10,10,10,10,10,10,10,10,
                                                      10,10,10,10,10,10,10,10,10,10,
                                                      10,10,10,10,10,10,10,10,10,10,
                                                      10,10,10,10,10,10,10,10,10,10};
void setup() 
{
  Serial.begin(115200);
  delay(1500);
  Ethernet.begin(mac, ip);
  pinMode(Mic,INPUT);
  Wire.begin();
  Wire.setClock(400000);
  MIDI.begin();
  //setup handlers for RTP midi commands
  //AppleMIDI.setHandleConnected(OnAppleMidiConnected);
  //AppleMIDI.setHandleDisconnected(OnAppleMidiDisconnected);
  //set up handlers for basic midi commands
  MIDI.setHandleNoteOff(MidiNoteOff);
  MIDI.setHandleNoteOn(MidiNoteOn);
  MIDI.setHandleControlChange(MidiControlChange);
  //InitializeAllServoBoard();
  CalculateKeyDelta();
  SoftStartReset();
}

void loop() 
{
  //DEBUG_PRINT("Loop Begin: ");DEBUG_PRINTLN(CurrentMillis);
  //DEBUG_PRINT("OffByte[29]");DEBUG_PRINTLN(IOEOffByteTimer[29]);
  //start loop
  //serial monitor
  if (Serial.available() != 0)
  {
    DEBUG_PRINTLN("Serial Available");
    SerialParser();
  }
//end serial monitor
  CheckNotes(16);  //check a number of servo boards at a time
  MIDI.read(RHC);    //read any midi messages on the third channel in the que for the piano/right hand
  MIDI.read(LHC);    //Read any midi messages on the fourth channel in the que for the bass/left hand
  //end loop
}

//probably something wrong in here
void CheckNotes(int CheckLength)
{
  //Check length just determines how many servos are checked at a time
  //DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINT(" CheckNotes");
  for (int i = 0; i <= CheckLength; i++)                  //check a number of bytes at a time
  {
    if (Position > TotalServos)                             //determine if we have wraped around how many IOE banks we have
    {
      Position = 0;                                         //reset the wrap around
      //DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINT(" Position = 0 ");
    }
    //DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINT(" for int i = 0; i <= CheckLength; i++ ");
    if(SCV[Position] > 0)                             //check to see if the servo is populated
    {
     // DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINT("if");
      //check On noted
      if (IOEOnByteTimer[Position] != 0     and         //Determine if a value is stored in the byte timer
          IOEOnByteTimer[Position] <= CurrentMillis)    //If the timer is less than the current time continue
      {
        SetGlobalServo(Position, SOV[Position]);  //send the On note
        IOEOnByteTimer[Position] = 0;                   //set the timmer back to 0
      }
      //Check Off Notes
      if (IOEOffByteTimer[Position] != 0     and   //make sure the timmer has a value in it
          IOEOffByteTimer[Position] <= CurrentMillis)   //if the timmer has expired contiune
      {
        SetGlobalServo(Position, SCV[Position]);    //send the off note
        IOEOffByteTimer[Position] = 0;                    //set the timmer back to 0
      }
      Position++;                                         //increase the global array variable
    }
  }
}

//refacter this to seperate left hand and right hand right away then do more testing
//get rid of the defines, or make mroe defines for left hand lowest note right hand lowest note
void MidiNoteOn(byte channel, byte note, byte velocity)   //function called when receving a note
{
  DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINT(" MidiNoteOn: ");DEBUG_PRINT(channel);DEBUG_PRINT(", ");DEBUG_PRINTLN(note);
  bool Procede = false;
  if(channel == LHC)
  {
    DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINTLN("channel == LHC");
    if (note >= LPNL and note <= HPNL)                      //test for playable notes
    {
      DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINTLN("note >= LPNL and note <= HPNL");
      note = note%12;
      if(note >= 60 and note <= 71)  //range for bank 4
      {
        DEBUG_PRINTLN("note >= 60 and note <= 71");
        note = note + 12;
      }
      note = note + 48;   //offset for base servos
      Procede = true;
    }
  }
  else if (note >= LPNR and note <= HPNR)                      //test for playable notes
  {
      DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINTLN("note >= LPNR and note <= HPNR");
      note = note - LPNR;
      Procede = true;
  }
  if(Procede == true)
  {
    DEBUG_PRINTLN("Procede == true");
    IOEOnByteTimer[note] = CurrentMillis + IOEOnByteTimerModifer[note];
    if(IOEOnByteTimer[note] < IOEOffByteTimer[note] + IOEOnByteTimerModifer[note] + IOEOffByteTimerModifer[note])
    {
      DEBUG_PRINTLN("IOEOnByteTimer[note] < IOEOffByteTimer[note] + IOEOnByteTimerModifer[note] + IOEOffByteTimerModifer[note]");
      if(IOEOnByteTimer[note] < IOEOffByteTimer[note])
      {
        DEBUG_PRINTLN("IOEOnByteTimer[note] < IOEOffByteTimer[note]");
        IOEOffByteTimer[note] = IOEOnByteTimer[note] - IOEOffByteTimerModifer[note] - 10;
      }
      else
      {
        DEBUG_PRINTLN("else");
        //                      subtract the time required to turn the note on from the off time so the on time is correct. also keep in mind the gap so the note is not cut short
        //                                                                             add back the already exisiting gap
        IOEOffByteTimer[note] = IOEOffByteTimer[note] - IOEOnByteTimerModifer[note] + (IOEOffByteTimer[note] - IOEOnByteTimer[note]);
      }
    }
  }
}

void MidiNoteOff(byte channel, byte note, byte velocity)    //function called when ending a note
{
  DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINT(" MidiNoteOff: ");DEBUG_PRINT(channel);DEBUG_PRINT(", ");DEBUG_PRINTLN(note);
  bool Procede = false;
  //Serial.println("off note");
  if(channel == LHC)
  {
    DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINTLN("channel == LHC");
    if (note >= LPNL and note <= HPNL)                      //test for playable notes
    {
      DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINTLN("note >= LPNL and note <= HPNL");
      note = note%12;
      if(note >= 60 and note <= 71)  //range for bank 4
      {
        DEBUG_PRINTLN("note >= 60 and note <= 71");
        note = note + 12;
      }
      note = note + 48;   //offset for base servos
      Procede = true;
    }
  }
  else if (note >= LPNR and note <= HPNR)                      //test for playable notes
  {
    DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINTLN("note >= LPNR and note <= HPNR");
    note = note - LPNR;
    Procede = true;
  }
  if(Procede == true)
  {
    IOEOffByteTimer[note] = CurrentMillis + IOEOffByteTimerModifer[note];
    DEBUG_PRINT("CT:");DEBUG_PRINT(CurrentMillis);DEBUG_PRINT(" IOEOffByteTimer[");DEBUG_PRINT(note);DEBUG_PRINT("] = ");DEBUG_PRINTLN(IOEOffByteTimer[note]);
  }
}

void MidiControlChange(byte channel, byte control, byte value)  //other midi commands
{
  //Serial.print("CH: ");Serial.print(channel);Serial.print(" control: ");Serial.print(control);Serial.print(" value: ");Serial.println(value);
  switch (control)              //test to see what command is being sent
  {
    case 123:                   //all notes off command
      for (int i = 0; i <= 5; i++)  //just clear out the buffer
      {
        MIDI.read(LHC);
        MIDI.read(RHC);
        delay(5);
      }
      SetTimerArrayZero();      //set all the timmers to 0
      break;
    case 121:                   //reset device
      SoftStartReset();
      break;
    case 102:                   //voice change
      switch(value)
      {
        int VoiceServo = 0;
        case 0:
          VoiceServo = 42;
          break;
        case 1:
          VoiceServo = 43;
          break;
        case 2:
          VoiceServo = 44;
          break;
        case 3:
          VoiceServo = 45;
          break;
        if(channel == LHC)
        {                         //bass uses 24 servos, there are 2 servos left over on the piano side
          VoiceServo = VoiceServo + 26;    //set up the difference in servo numbers from one channel to another
        }
        //call the servo command
      }
      break;
    case 7:                   //volume change
      
      break;
  }
}

/*
void OnAppleMidiDisconnected(const ssrc_t & ssrc)
{
  MidiControlChange(1, 123, 0);
  SoftStartReset();
}
*/

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
  //Serial.print("I2Caddress: 0x");Serial.print(I2CAddress, HEX);
  //Serial.print("  ServoNum: ");Serial.println(ServoNum);
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
  int ServoBoardNum = ServoNumber/16;
  if(ServoBoardNum > 0)
  {
    ServoNumber = ServoNumber%16;
  }
  SetLocalServo(ServoBoardNum, ServoNumber, PWMValue);
} 

void CalculateKeyDelta()
{
  for (int i = 0; i <= TotalServos; i++)
  {
    if (IOEOnByteTimerModifer[i] <= KeyDelayTime  and IOEOnByteTimerModifer[i] != 0) //only run if the digital delay is longer than the physical delay
    {
      //create the difference between the physical delay and the digital delay so all keys play at the same time
      IOEOnByteTimerModifer[i] = KeyDelayTime - IOEOnByteTimerModifer[i];
    }
    if (IOEOffByteTimerModifer[i] <= KeyDelayTime  and IOEOffByteTimerModifer[i] != 0) //only run if the digital delay is longer than the physical delay
    {
      //create the difference between the physical delay and the digital delay so all keys play at the same time
      IOEOffByteTimerModifer[i] = KeyDelayTime - IOEOffByteTimerModifer[i];
    }
  }
}

void SoftStartReset()
{
  InitializeAllServoBoard();
  CloseAllServos();
  SetTimerArrayZero();
}

void CloseAllServos()
{
  for(int i = 0; i <= TotalServos; i++)
  {
    SetGlobalServo(i, SCV[i]);
  }
}

void SetTimerArrayZero()
{
  for (int i = 0; i <= TotalServos; i++)
  {
    DEBUG_PRINT("IOEON/OFF: ");DEBUG_PRINTLN(i);
    //for each position in the arrays set them to 0
    IOEOnByteTimer[i] = 0;                                //play no new notes
    IOEOffByteTimer[i] = CurrentMillis + KeyDelayTime;    //wait for old notes to turn off
  }
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
