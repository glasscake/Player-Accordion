//inclusions
#include <Wire.h>
#include <NativeEthernet.h>
#include <AppleMIDI.h>
#include <SPI.h>
#include <SD.h>
//end inclussions

//apple midi stuff
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEC};
            //169.254.142.141
byte ip[] = {169,254,142,145};

APPLEMIDI_CREATE_INSTANCE(EthernetUDP, MIDI, "Accordion", 5004);
int Mic = A9;
//end apple midi

#define DEBUG true
#ifdef DEBUG
#define dbprint(x)     Serial.print (x)
#define dbprintln(x)  Serial.println (x)
#else
#define dbprint(x)
#define dbprintln(x) 
#endif

#define ENABLEeth true
//#define ENABLEwaitforserial true

//dbprint("CT:");dbprint(CurrentMillis);dbprint(" Position: ")

//definitions
//total number of possible servos
#define TotalServos 79
#define totalservosp1 80
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
#define LPNR 53      //lowest playable midi note - Right Hand
#define HPNR 93      //highest playable midi note - Right Hand
//end inclusions

byte BassMap[24] = {5, 4, 3, 2, 1, 0, 11, 10, 9, 8, 7, 6, 17, 16, 15, 14, 25, 12, 23, 22, 21, 20, 19, 18};

//global variables
int ServoMedian = 280;

elapsedMillis CurrentMillis;
unsigned long KeyDelayTime = 130; //Just slightly longer than the slowest key
unsigned long IOEOnByteTimer[totalservosp1];
unsigned long IOEOffByteTimer[totalservosp1];
int Position = 0;
int CRHV  = 0;  //current right hand voice
int CLHV  = 0;  //current left hand voice

//initialize the Servo Off value
int SCV[totalservosp1];
//Initalize the servo ON value
int SOV[totalservosp1];                                    
//Initalize the on delay
int OnTimeMod[totalservosp1];
//Initialize the OFF delay
int OffTimeMod[totalservosp1];

File CurrentFile;

void setup() 
{
  Serial.begin(115200);                               //initialize serial
  #ifdef ENABLEwaitforserial
    while (!Serial)                                     //wait for serial to conenct
    {
      ;
    }
    Serial.println("Serial Connected. Setup()");
  #endif
  dbprintln("Ethernet.Begin");
  #ifdef ENABLEeth
    Ethernet.begin(mac, ip);                          //initalize ethernet
  #endif
  dbprintln("Ethernet.Begin Pass");
  pinMode(Mic,INPUT);                                 //Mic as anlog input
  dbprintln("Wire.begin()");
  Wire.begin();                                       //begin i2c
  dbprintln("Wire.begin() Pass");
  dbprintln("Wire.Setclock()");
  Wire.setClock(100000);                              //set i2c clock speed
  dbprintln("Wire.Setclock() Pass");
  dbprintln("MIDI.begin()");
  MIDI.begin(MIDI_CHANNEL_OMNI);                      //initilize midi port read all channels
  dbprintln("MIDI.begin() Pass");
  dbprintln("MIDI.sethandle()");
  MIDI.setHandleNoteOff(MidiNoteOff);                 //setup function for note off
  MIDI.setHandleNoteOn(MidiNoteOn);                   //setup function for note on
  MIDI.setHandleControlChange(MidiControlChange);     //setup function for control change
  dbprintln("MIDI.sethandle() Pass");
  dbprintln("SD.begin(BUILTIN_SDCARD)");
  SD.begin(BUILTIN_SDCARD);                           //initalize SD card
  dbprintln("SD.begin(BUILTIN_SDCARD) Pass");
  variablesfromSD();
  CalculateKeyDelta();                                //create the delta from key on
  SoftStartReset();                                   //initalize servo boards
}

void loop() 
{
  //start loop
  
  //serial monitor
  if (Serial.available() != 0)
  {
    //dbprintln("Serial Available");
    SerialParser();
  }
  //end serial monitor
  #ifdef ENABLEeth
    CheckNotes(8);  //check a number of servo boards at a time
    MIDI.read();    //read any midi messages on the third channel in the que for the piano/right hand
  #endif
  //end loop
}

//probably something wrong in here
void CheckNotes(int CheckLength)
{
  //Check length just determines how many servos are checked at a time
  //dbprint("CT:");dbprint(CurrentMillis);dbprint(" CheckNotes");
  for (int i = 0; i <= CheckLength; i++)                  //check a number of bytes at a time
  {
    //dbprintln(Position);
    if (Position > TotalServos)                             //determine if we have wraped around how many IOE banks we have
    {
      Position = 0;                                         //reset the wrap around
      //dbprint("CT:");dbprint(CurrentMillis);dbprint(" Position = 0 ");
    }
    //dbprintln(Position);
    //dbprint("CT:");dbprint(CurrentMillis);dbprint(" for int i = 0; i <= CheckLength; i++ ");
    if(SCV[Position] > 0)                             //check to see if the servo is populated
    {
     //dbprint("CT:");dbprint(CurrentMillis);dbprintln("if SCV[position]>0");
      //check On noted
      if (IOEOnByteTimer[Position] != 0     and         //Determine if a value is stored in the byte timer
          IOEOnByteTimer[Position] <= CurrentMillis)    //If the timer is less than the current time continue
      {
        //dbprint("SENDING SERVO ON: "); dbprintln(Position);
        SetGlobalServo(Position, SOV[Position]);  //send the On note
        IOEOnByteTimer[Position] = 0;                   //set the timmer back to 0
      }
      //Check Off Notes
      if (IOEOffByteTimer[Position] != 0     and   //make sure the timmer has a value in it
          IOEOffByteTimer[Position] <= CurrentMillis)   //if the timmer has expired contiune
      {
        //dbprint("SENDING SERVO OFF: "); dbprintln(Position);
        SetGlobalServo(Position, SCV[Position]);    //send the off note
        IOEOffByteTimer[Position] = 0;                    //set the timmer back to 0
      }
    }
    Position++;                                         //increase the global array variable
  }
}

//refacter this to seperate left hand and right hand right away then do more testing
//get rid of the defines, or make mroe defines for left hand lowest note right hand lowest note
void MidiNoteOn(byte channel, byte note, byte velocity) //function called when receving a note
{
  //dbprint("CT:");dbprint(CurrentMillis);dbprint(" MidiNoteOn: ");dbprint(channel);dbprint(", ");dbprintln(note);
  if (channel == RHC or channel == LHC) {
    bool Procede = false;
    if (channel == LHC) {
      //dbprint("CT:");dbprint(CurrentMillis);dbprintln("channel == LHC");
      if (note >= LPNL and note <= HPNL) //test for playable notes on the left hand
      {
        //dbprint("CT:");dbprint(CurrentMillis);dbprintln(" note >= LPNL and note <= HPNL");
        int lefthandnote = note;
        note = note % 12;
        if (lefthandnote >= 60 and lefthandnote <= 71) //range for bank 4
        {
          //dbprintln("note >= 60 and note <= 71");
          note = note + 12;
        }
        //apply map array
        note = BassMap[note] + 45; //offset for base servos
        Procede = true;
      }
    } else if (note >= LPNR and note <= HPNR) //test for playable notes on the right hand
    {
      //dbprint("CT:");dbprint(CurrentMillis);dbprintln(" note >= LPNR and note <= HPNR");
      note = note - LPNR;
      Procede = true;
    }
    if (Procede == true) {
      //dbprintln("Procede == true");
      IOEOnByteTimer[note] = CurrentMillis + OnTimeMod[note];
      if (IOEOnByteTimer[note] < IOEOffByteTimer[note] + OnTimeMod[note] + OffTimeMod[note]) {
        //dbprintln("IOEOnByteTimer[note] < IOEOffByteTimer[note] + OnTimeMod[note] + OffTimeMod[note]");
        if (IOEOnByteTimer[note] < IOEOffByteTimer[note]) {
          //dbprintln("IOEOnByteTimer[note] < IOEOffByteTimer[note]");
          IOEOffByteTimer[note] = IOEOnByteTimer[note] - OffTimeMod[note] - 5;
        } else {
          //dbprintln("else");
          //subtract the time required to turn the note on from the off time so the on time is correct. also keep in mind the gap so the note is not cut short
          //                                                                             add back the already exisiting gap
          IOEOffByteTimer[note] = IOEOffByteTimer[note] - OnTimeMod[note] - 5 + (IOEOffByteTimer[note] - IOEOnByteTimer[note]);
        }
      }
      //dbprint("CT:");dbprint(CurrentMillis);dbprint(" IOEOnByteTimer[");dbprint(note);dbprint("] = ");dbprintln(IOEOnByteTimer[note]);
    }
  }
}

void MidiNoteOff(byte channel, byte note, byte velocity) //function called when ending a note
{
  //dbprint("CT:");dbprint(CurrentMillis);dbprint(" MidiNoteOff: ");dbprint(channel);dbprint(", ");dbprintln(note);
  if (channel == RHC or channel == LHC) {
    bool Procede = false;
    //Serial.println("off note");
    if (channel == LHC) {
      //dbprint("CT:");dbprint(CurrentMillis);dbprintln("channel == LHC");
      if (note >= LPNL and note <= HPNL) //test for playable notes
      {
        //dbprint("CT:");dbprint(CurrentMillis);dbprintln("note >= LPNL and note <= HPNL");
        int lefthandnote = note;
        note = note % 12;
        if (lefthandnote >= 60 and lefthandnote <= 71) //range for bank 4
        {
          //dbprintln("note >= 60 and note <= 71");
          note = note + 12;
        }
        //apply map array
        note = BassMap[note] + 45; //offset for base servos
        Procede = true;
      }
    } else if (note >= LPNR and note <= HPNR) //test for playable notes
    {
      //dbprint("CT:");dbprint(CurrentMillis);dbprintln("note >= LPNR and note <= HPNR");
      note = note - LPNR;
      Procede = true;
    }
    if (Procede == true) {
      IOEOffByteTimer[note] = CurrentMillis + OffTimeMod[note];
      //dbprint("CT:");dbprint(CurrentMillis);dbprint(" IOEOffByteTimer[");dbprint(note);dbprint("] = ");dbprintln(IOEOffByteTimer[note]);
    }
  }
}

void MidiControlChange(byte channel, byte control, byte value) //other midi commands
{
  if (channel == RHC or channel == LHC) {
    //Serial.print("CH: ");Serial.print(channel);Serial.print(" control: ");Serial.print(control);Serial.print(" value: ");Serial.println(value);
    switch (control) //test to see what command is being sent
    {
    case 123: //all notes off command
      for (int i = 0; i <= 5; i++) //just clear out the buffer
      {
        MIDI.read(LHC);
        MIDI.read(RHC);
        delay(5);
      }
      SetTimerArrayZero(); //set all the timmers to 0
      break;
    case 121: //reset device
      SoftStartReset();
      break;
    case 102: //voice change
    int VoiceServo = 0;
      switch (value) {
      case 0:
        VoiceServo = 41;
        break;
      case 1:
        VoiceServo = 42;
        break;
      case 2:
        VoiceServo = 43;
        break;
      case 3:
        VoiceServo = 44;
        break;
      }
      if (channel == LHC) 
        { //bass uses 24 servos, there are 2 servos left over on the piano side
          VoiceServo = VoiceServo + 27; //set up the difference in servo numbers from one channel to another
          //servo 68 is the first voice servo for left hand
        }
        //dbprint("CT:");dbprint(CurrentMillis);dbprint("VOICE SERVO: ");dbprintln(VoiceServo);
        IOEOnByteTimer[VoiceServo] = CurrentMillis + 20;
        IOEOffByteTimer[VoiceServo] = CurrentMillis + 1020;
      break;
    case 7: //volume change
      //not implemented
      break;
    }
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
    //dbprintln("received Serial Command");
    String SerialIn = SerialReader();
    char CommandIdentifer;
    int Commas = CommaCount(SerialIn);                   //how many commas are in the string
    if (Commas > 0)
    {
      CommandIdentifer = SerialIn.charAt(0);                //retains the first idenifer
      int Settings[Commas];                                //array of setting values
      for(int i = 0, fc = 0, nc = 0; i < Commas; i++)  //loop that parses the string info
      {
        //fc - first comma, nc - next comma
        fc = SerialIn.indexOf(",", nc);
        nc = SerialIn.indexOf(",", fc+1);
        if(nc == -1)
        {
          Settings[i] = SerialIn.substring(fc+1, SerialIn.length()).toInt();
        }
        else
        {
          Settings[i] = SerialIn.substring(fc+1, nc).toInt();
        }
      }
      switch(CommandIdentifer)
      {
        case 't':         //turn servo on or off
          if(Settings[1] == 1) //if command to turn on
          {
           dbprint("Servo: ");dbprint(Settings[0]);dbprint(" value: ");dbprintln(SOV[Settings[0]]);
           SetGlobalServo(Settings[0],SOV[Settings[0]]);
          }                                   
          else
          {
            dbprint("Servo: ");dbprint(Settings[0]);dbprint(" value: ");dbprintln(SCV[Settings[0]]);
            SetGlobalServo(Settings[0],SCV[Settings[0]]); 
          }
          break;
        case 'i':         //set servo open/closed values
        //i,servo num, open(1)/closed(0),value
          if(Settings[1] == 1)
          {
            SOV[Settings[0]] = Settings[2];
            dbprint("Servo: ");dbprint(Settings[0]);dbprint(" SOV: ");dbprintln(SOV[Settings[0]]);
          }
          else
          {
            SCV[Settings[0]] = Settings[2];
            dbprint("Servo: ");dbprint(Settings[0]);dbprint(" SCV: ");dbprintln(SCV[Settings[0]]);
          }
          break;
        case 'm':       //set servo to median position
        //m,servo
          SetGlobalServo(Settings[0],ServoMedian);
          break;
        case 'n':       //set median position value
        //n,value
          ServoMedian = Settings[0];
          break;
        case 'c':       //move servo to position
        //c,servo,position
          Serial.print("Moving Servo: ");Serial.println(Settings[1]);
          SetGlobalServo(Settings[0],Settings[1]);
          break;
        case 'q':
        //q,servo num
          RecordKeyOnDelay(Settings[0],0);
          break;
        default:
          dbprintln("unrecognized first character");
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
        PrintArrayServos(SOV,TotalServos);
        Serial.println("servo Off Values");
        PrintArrayServos(SCV,TotalServos);
        Serial.println("On Modifier");
        PrintArrayServos(OnTimeMod, TotalServos);
        Serial.println("Off Modifier");
        PrintArrayServos(OffTimeMod, TotalServos);
      }
      if(SerialIn == "int")
      {
        InitializeAllServoBoard();
      }
      if(SerialIn == "int1")
      {
        SoftStartReset();
      }
      if(SerialIn == "sdr")
      {
        Serial.println("SD read");
        variablesfromSD();
      }
      if(SerialIn == "sdw")
      {
        Serial.println("SD Write");
        PublishArrayToSD(SCV,TotalServos,"SCV.txt");
        PublishArrayToSD(SOV,TotalServos,"SOV.txt");
        PublishArrayToSD(SCV,TotalServos,"SCV.txt");
        PublishArrayToSD(SCV,TotalServos,"SCV.txt");
        Serial.println("SD Write Done");
      }
  }
}

void PrintArrayServos(int Array[], int len)
{
  for (int i = 0; i <= len; i++)
  {
    Serial.print(Array[i]);
    Serial.print(",");
    if(((i+1)%10) == 0 and i != 0)
    {
      Serial.println("");
    }
  }
  Serial.println("");
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
    if (OnTimeMod[i] <= KeyDelayTime  and OnTimeMod[i] != 0) //only run if the digital delay is longer than the physical delay
    {
      //create the difference between the physical delay and the digital delay so all keys play at the same time
      OnTimeMod[i] = KeyDelayTime - OnTimeMod[i];
    }
    if (OffTimeMod[i] <= KeyDelayTime  and OffTimeMod[i] != 0) //only run if the digital delay is longer than the physical delay
    {
      //create the difference between the physical delay and the digital delay so all keys play at the same time
      OffTimeMod[i] = KeyDelayTime - OffTimeMod[i];
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
    //dbprint("IOEON/OFF: ");dbprintln(i);
    //for each position in the arrays set them to 0
    IOEOnByteTimer[i] = 0;                                //play no new notes
    IOEOffByteTimer[i] = CurrentMillis + 5;    //wait for old notes to turn off
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
  int I2CAddy = 0x40;
  for(int i = 0; i <= TotalServoBoards; i++)
  {
    /*
    Serial.println(I2CAddy);
    Serial.print("InitServo: 0x");Serial.println(I2CAddy, HEX);
    Serial.print("i: ");Serial.println(i);
    */
    WriteToSubAddress(I2CAddy,0x00,0x30);   //set board to sleep
    WriteToSubAddress(I2CAddy,0xFE,0x83);   //change prescaller
    WriteToSubAddress(I2CAddy,0x00,0x20);   //set board awake
    WriteToSubAddress(I2CAddy,0x00,0xA0);   //set board auto index
    I2CAddy = I2CAddy + 1;
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
  //delay(5);
}

void RecordKeyOnDelay(int Key, int BackgroundNoise)
{
 //setup background noise level
 //clipping the mic so it is positive only
 int Threshold = 300;
 int reading = 0;
 unsigned long Watchdog = 1000;
 unsigned long KeyOnTime = 0;
 unsigned long KeyOffTime = 0;
 unsigned long StartTime;
 //figure out the background noise if its not passed
 if(BackgroundNoise == 0)
 {
    BackgroundNoise = AverageRead(Mic, 2, 5);
 }
 BackgroundNoise = BackgroundNoise+Threshold;
 //start the test
 //Serial.println("KeyOn");
 StartTime = CurrentMillis;
 SetGlobalServo(Key, SOV[Key]);
 while (reading < BackgroundNoise)
 {
    reading = AverageRead(Mic, 2, 3);
    KeyOnTime = CurrentMillis - StartTime;
    //Serial.print("reading:");Serial.print(reading);Serial.print(" Threshold:");Serial.print(BackgroundNoise);
    //Serial.print(" KeyOnTime:");Serial.println(KeyOnTime);
    if(KeyOnTime > Watchdog)
    {
      KeyOnTime = 0;
      break;
    }
  }
 //Serial.println("KeyOff");
 StartTime = CurrentMillis;
 SetGlobalServo(Key, SCV[Key]);
 while (reading > BackgroundNoise)
 {
    reading = AverageRead(Mic, 2, 3);
    KeyOffTime = CurrentMillis - StartTime;
    //Serial.print("reading:");Serial.print(reading);Serial.print(" Threshold:");Serial.print(BackgroundNoise);
    //Serial.print(" KeyOffTime:");Serial.println(KeyOffTime);
    if(KeyOffTime > Watchdog)
    {
      KeyOffTime = 0;
      break;
    }
  }
  /*
 Serial.print("KeyOn:");Serial.print(KeyOnTime);
 Serial.print(" KeyOff:");Serial.println(KeyOffTime);
 */
 Serial.print("Key:");Serial.print(Key);Serial.print(",");Serial.print(KeyOnTime);Serial.print(",");Serial.println(KeyOffTime);
}

int AverageRead(int Pin, int Delay, int samples)
{
  int average = 0;                             //inital value
  int reading = 0;
  for (int i = 0; i < samples; i++)            //how many times to check
  {
    reading  = analogRead(Pin);
    if(reading < 0)
    {
      reading = reading *-1;
    }
    average = average + reading;      //read the variable
    delay(Delay);                             //delay
  }
  return average / samples;                    //the average
}


//SD card stuff
void variablesfromSD()
{
  //load variables from an SD file
  dbprintln("varuablesfromsd()");
  //servo closed values
  CurrentFile = SD.open("SCV.txt");
  dbprint("CurrentFile = ");dbprintln(CurrentFile.name());
  readintoarray(SCV, TotalServos);
  createbackup(CurrentFile);
  CurrentFile.close();
  ////////////////////////
  //servo open values
  CurrentFile = SD.open("SOV.txt");
  dbprint("CurrentFile = ");dbprintln(CurrentFile.name());
  readintoarray(SOV, TotalServos);
  createbackup(CurrentFile);
  CurrentFile.close();
  ///////////////////////
  //On Timer Modifiers
  CurrentFile = SD.open("ONMod.txt");
  dbprint("CurrentFile = ");dbprintln(CurrentFile.name());
  readintoarray(OnTimeMod, TotalServos);
  createbackup(CurrentFile);
  CurrentFile.close();
  ///////////////////////
  //Off Timer Modifiers
  CurrentFile = SD.open("OFFMod.txt");
  dbprint("CurrentFile = ");dbprintln(CurrentFile.name());
  readintoarray(OffTimeMod, TotalServos);
  createbackup(CurrentFile);
  CurrentFile.close();
  ///////////////////////
  
  //print the arrays
  #ifdef DEBUG
    dbprintln("SCV:");
    PrintArrayServos(SCV, TotalServos);
    dbprintln("SOV:");
    PrintArrayServos(SOV, TotalServos);
    dbprintln("OnTimeMod:");
    PrintArrayServos(OnTimeMod,TotalServos);
    dbprintln("OffTimeMod:");
    PrintArrayServos(OffTimeMod, TotalServos);
  #endif
}

void readintoarray(int arry[], int len)
{
  //read a line from an SD card into an array
  int i = 0;
  String readline;
  while (i <= len)
  {
    readline = sdreadline();
    arry[i] = readline.toInt();
    //dbprint("arry[");dbprint(i);dbprint("] = ");dbprintln(arry[i]);
    i++;
  }
}


String sdreadline()
{
  //read a single line out of a file
  //dbprintln("sdreadline()");
  char readchar;
  String fullline;
  readchar = CurrentFile.read();
  //clear out new line returns
  while(readchar == '\r' or readchar == '\n') 
  {
    readchar = CurrentFile.read();
  }
  while (true)
  {
    //dbprintln("while loop");
    //test for end of line or end of file
    if (readchar == '\r' or readchar == '\n' or readchar == ' ' or readchar == 0xFF)
    {
      //dbprintln("break;");
      break;
    }
    //create string from char
    fullline = fullline + readchar;
    readchar = CurrentFile.read();
    //dbprint("readchar = ");dbprintln(readchar);
    //dbprint("fullline = ");dbprintln(fullline);
  }
  //test for end of file
  if(readchar == 0xFF and fullline.length() == 0)
  {
    dbprintln("end of file;");
    return "-1";
  }
  return fullline;
}

void createbackup (File orgfile)
{
  //reopen the original file incase it has been mid way through the read
  orgfile = SD.open(orgfile.name());
  //create the new filename
  String newname = orgfile.name();
  //new file name is identical except it has .log
  newname = newname.substring(0, newname.length()-3) + "log";
  //convert string to char array
  char S2Char[11];
  newname.toCharArray(S2Char,11);
  //if log exists delete it
  if(SD.exists(S2Char) == true)
  {
    SD.remove(S2Char);
  }
  //create the log file
  File Backup = SD.open(S2Char, FILE_WRITE);
  dbprint("backup filename = ");dbprintln(Backup.name());
  //create a buffer
  size_t n;  
  byte buf[1024];
  //read in a buffer and write a buffer
  while ((n = orgfile.read(buf, sizeof(buf))) > 0) {
    Backup.write(buf, n);
  }
  //close the file to save it
  Backup.close();
}

void PublishArrayToSD (int arry[],int len, String Filename)
{
  //convert string to char array
  char S2Char[11];
  Filename.toCharArray(S2Char,11);
  //Delete the file if it exists
  if(SD.exists(S2Char) == true)
  {
    SD.remove(S2Char);
  }
  //open the file for writing
  File curfile = SD.open(S2Char, FILE_WRITE);
  //write the info
  for(int i = 0; i <= len; i++)
  {
    curfile.println(arry[i]);
  }
}
