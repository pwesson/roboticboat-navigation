#include <SD.h>
#include <Servo.h>
#include <math.h>

//  ____                _       _               
// |  _ \   _   _    __| |   __| |   ___   _ __ 
// | |_) | | | | |  / _` |  / _` |  / _ \ | '__|
// |  _ <  | |_| | | (_| | | (_| | |  __/ | |   
// |_| \_\  \__,_|  \__,_|  \__,_|  \___| |_|   
                                                
#include <Rudder.h>

//   ____   __  __   ____    ____      _    _
//  / ___| |  \/  | |  _ \  / ___|    / |  / |
// | |     | |\/| | | |_) | \___ \    | |  | |
// | |___  | |  | | |  __/   ___) |   | |  | |
//  \____| |_|  |_| |_|     |____/    |_|  |_|

#include <CompassCMPS11_Teensy.h>

//     _          _            __                  _   _        ____   ____    ____
//    / \      __| |   __ _   / _|  _ __   _   _  (_) | |_     / ___| |  _ \  / ___|
//   / _ \    / _` |  / _` | | |_  | '__| | | | | | | | __|   | |  _  | |_) | \___ \ .
//  / ___ \  | (_| | | (_| | |  _| | |    | |_| | | | | |_    | |_| | |  __/   ___) |
// /_/   \_\  \__,_|  \__,_| |_|   |_|     \__,_| |_|  \__|    \____| |_|     |____/

#include <AdaGPS.h>

// __        __  _               _
// \ \      / / (_)  _ __     __| |
//  \ \ /\ / /  | | | '_ \   / _` |
//   \ V  V /   | | | | | | | (_| |
//    \_/\_/    |_| |_| |_|  \__,_|
//  

#include <Wind.h>

#include <Adafruit_SSD1306_Teensy.h>

Adafruit_SSD1306_Teensy display;

//Is Robot in control of the human
bool IsHuman = true;

int i=0;

//Radio Control Receiver
int ch1 = 0;
int ch2 = 0;
int ch3 = 0;

//LED light
bool islighton = false;

// Rudder is centered
bool iscentered = true;

// Timer
unsigned long mytime;
unsigned long lasttime;
unsigned long lastadjust;

//Radio Control Servo
Servo servoRudder;
Servo servoSails;
int rudder = 90;

// Declare the compass class
CompassCMPS11_Teensy compass1;

//A GPS library
AdaGPS gps;

//
// The Adafruit GPS flashes once every 15 seconds when it has found a fix
//

// Global variables
String inputSerial2 = "";         // a string to hold incoming data
boolean IsReadySerial2 = false;  // whether the string is complete

// Setup the weighted least squares
float BoatHeading = 0;
float oldBoatHeading = 0;
float bearingchanged = 0;
float toWind = 0;

signed char pitch = 0;
signed char roll = 0;

Wind mywind;

Rudder myrudder;

float gyrox = 0;
float gyroy = 0;
float gyroz = 0;

float accelx = 0;
float accely = 0;
float accelz = 0;

void setup()
{
  // Keep the User informed
  Serial.begin(9600);

  // initialize the LED pin as an output.
  pinMode(13, OUTPUT);

  // Setup the input pins used received by the RC receiver
  pinMode(16, INPUT);
  pinMode(17, INPUT);
  pinMode(14, INPUT); //IsRobot flag

  // Setup the output pins used by the servos
  pinMode(20, OUTPUT);
  pinMode(21, OUTPUT);
  
  //Setup servo connection for rudder and sail
  servoRudder.attach(20);
  servoSails.attach(21);

  //Set the rudder range
  myrudder.maxposition = 90 + 40;
  myrudder.minposition = 90 - 40;

  // initialize with the I2C addr 0x3C (for the 128x32)
  // 1 = i2c bus on the Teensy
  // 4 = reset pin
  display.begin(0x3C, 1, 4);  
  
  // Initialise the GPS module
  Serial.println("Initializing GPS");
  Serial2.begin(9600);
  delay(100);

  // Just want RMC and GGA messages
  // Send setup command to the GPS module
  Serial2.println("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28");
  delay(100);

  // 1MHz update
  // Send setup command to the GPS module
  Serial2.println("$PMTK220,1000*1F");
  delay(100);

  // Initialize the compass and initial readings
  compass1.begin(0x60, 0);

  BoatHeading = 0; //compass1.getBearing();
  oldBoatHeading = BoatHeading;

  // Initialise the SD card
  InitialiseSDcard(10);

  //Set the last time
  lasttime = 0;
  lastadjust = 0;
}

void serialEvent2()
{
  //This event is called when Serial2 receives new bytes
  while (Serial2.available())
  {
    // Read the new byte:
    gps.read((char)Serial2.read());
  }
}


void loop()
{
  // Timer
  mytime = millis();

  // Has 1 seconds passed yet?
  // Approx once an hour this will roll over to zero again.
  if (abs(mytime - lasttime) > 1000)
  {
    // So we enter this section every 1 second
    lasttime = mytime;

    // The change in the bearing over 1 second.
    // Can be positive or negative and greater than 180 or even 360 degrees
    // The issue is going from 10 to 350 degrees is -20 degrees rather than +340 degrees
    bearingchanged = BoatHeading - oldBoatHeading;

    // Ensure the change in degree is between -180 to 180 degrees.
    if (bearingchanged > 180) bearingchanged -= 360;
    if (bearingchanged < -180) bearingchanged += 360;

    // Update the rudder machine learning with a new 1 second observation
    if (!IsHuman)
    {
      // Add a reading when the Robot is in control.
      // Only happens when the robot is in control. This means it not being pushed around.
      myrudder.AddReading(mytime, ch1, bearingchanged);
    }

    // Update the old bearing
    oldBoatHeading = BoatHeading;

    //The GPS signal must be active (not void)
    if (gps.gpsstatus == 'A')
    {
      //Flash the light when receiving a GPS signal
      if (islighton)
      {
        // Turn off the light
        digitalWrite(13, LOW);
        islighton = false;
      }
      else
      {
        // Turn on the light
        digitalWrite(13, HIGH);
        islighton = true;
      }
    }
    else
    {
      // Turn off the light as no valid GPS location
      digitalWrite(13, LOW);
      islighton = false;
    }

    // Update the OLED

    display.clearDisplay();
    display.setPosition(0,0);
    display.print("CMP:");
    display.print(BoatHeading,0);
    display.print(" ROL:");
    display.print(roll);
    display.print("  ");
    if (IsHuman) display.println("H");
    if (!IsHuman) display.println("R");
  
    display.print(gps.latitude, 7);
    display.print(" ");
    display.println(gps.longitude, 7);
  
    display.print("Wind:");
    display.print(mywind.Direction);
    display.print(" ");
    display.print(toWind,0);
    display.print(" Rud:");
    display.println(rudder);
    
    display.print("Path ");
    display.print(myrudder.alongtrack);
    display.print(", ");
    display.println(myrudder.crosstrack);
    
    display.update_display();
  }

  // Read the pulse width of each channel.
  // The problem here is that the code blocks until it has read the signal.
  // This can take up to 20ms (per channel) if the radio control transmitter is off (not transmitting)
  ch3 = pulseIn(14, HIGH, 20000);
  ch1 = pulseIn(16, HIGH, 20000);
  ch2 = pulseIn(17, HIGH, 20000);

  // Map the value to the range 0 to 180 which is what radio control servos like
  ch1 = map(ch1, 1000, 2000, 0, 180);
  ch2 = map(ch2, 1000, 2000, 0, 180);
  ch3 = map(ch3, 1000, 2000, 0, 180);

  //If the left RC lever is down - turn on robot
  if (ch3 > 100)
  {
    //The transmitter is indicating to turn on the Robot by the position of channel2
    if (IsHuman)
    {
      //The Human was in control, so now initialise the Robot task

      //The Robot wants to know the line AB to follow.
      myrudder.NewLine(gps.latitude, gps.longitude, BoatHeading);      
    }

    //The Human has handed control of the boat to the Robot
    IsHuman = false;
  }
  else
  {
    //The Human has regained control of the boat
    IsHuman = true;

    myrudder.ClearLine();
  }

  // Read the compass.
  BoatHeading = compass1.getBearing();

  // Degrees to the Wind
  toWind = BoatHeading - mywind.Direction;

  // Ensure angle in degree is between -180 to 180 degrees.
  if (toWind > 180) toWind -= 360;
  if (toWind < -180) toWind += 360;

  //Note sometimes the compass may return 0 if it can't read.
  //In this case, keep the old bearing
  if (BoatHeading == 0) BoatHeading = oldBoatHeading;
  
  // Read the pitch and roll of the boat
  pitch = compass1.getPitch();
  roll = compass1.getRoll();
  
  // Read the compass gyro
  gyrox = compass1.getGyroX() * compass1._gyroScale;
  gyroy = compass1.getGyroY() * compass1._gyroScale;
  gyroz = compass1.getGyroZ() * compass1._gyroScale;

  // Read the compass accelerator
  accelx = compass1.getAcceleroX() * compass1._accelScale;
  accely = compass1.getAcceleroY() * compass1._accelScale;
  accelz = compass1.getAcceleroZ() * compass1._accelScale;

  myrudder.GyroUpdateMean(BoatHeading, gyroz);

  myrudder.AddGyroReading(rudder, gyroz);

  //Want to update the Wind direction estimate
  mywind.Update(BoatHeading, roll);

  // The Human has to control the boat if there is no GPS signal
  if (IsHuman || gps.gpsstatus == 'V')
  {
    // Human sets the rudder position
    rudder = ch1;
    servoRudder.write(rudder);

    // Human sets the sails position
    servoSails.write(ch2);

    //Have sometime for the servo to move
    delay(100);
  }
  else
  {
    // Update the new rudder position
    rudder = myrudder.Update(gps.latitude, gps.longitude, BoatHeading);
    
    // Robot updates the rudder position
    servoRudder.write(rudder);

    // Human still controls the sails
    servoSails.write(ch2);
  }

  // Print data to Serial Monitor window
  Serial.print("micromagic5,$TMR,");
  Serial.print(mytime);
  Serial.print(",$RC,");
  Serial.print(ch1);
  Serial.print(",");
  Serial.print(ch2);
  Serial.print(",");
  Serial.print(ch3);
  Serial.print("\t$CMP,");
  Serial.print(BoatHeading);
  Serial.print(",");
  Serial.print(pitch);
  Serial.print(",");
  Serial.print(roll);
  Serial.print("\t$GYR,");
  Serial.print(gyrox, 4);
  Serial.print(",");
  Serial.print(gyroy, 4);
  Serial.print(",");
  Serial.print(gyroz - myrudder.gyrozmean, 4);
  Serial.print(",");
  Serial.print(myrudder.gyro.slope);
  Serial.print(",");
  Serial.print(myrudder.gyro.intercept);
  Serial.print("\t$GPS,");
  Serial.print(gps.gpsdate, 0);
  Serial.print(",");
  Serial.print(gps.gpstime, 0);
  Serial.print(",");
  Serial.print(gps.latitude, 8);
  Serial.print(",");
  Serial.print(gps.latNS);
  Serial.print(",");
  Serial.print(gps.longitude, 8);
  Serial.print(",");
  Serial.print(gps.lonEW);
  Serial.print(",");
  Serial.print(gps.altitude);
  Serial.print(",");
  Serial.print(gps.fixquality);
  Serial.print(",");
  Serial.print(gps.numsatelites);
  Serial.print(",");
  Serial.print(gps.gpsknots);
  Serial.print(",");
  Serial.print(gps.gpstrack);
  Serial.print(",");
  Serial.print(gps.gpsstatus);
  Serial.print(",$WND,");
  Serial.print(mywind.Direction);
  Serial.print("\t$WLS,");
  Serial.print(myrudder.wls.slope);
  Serial.print(",");
  Serial.print(myrudder.wls.intercept);
  Serial.print("\t$LINE,");
  Serial.print(myrudder.alongtrack);
  Serial.print(",");
  Serial.print(myrudder.crosstrack);
  Serial.print(",$WAYA,");
  Serial.print(myrudder.waypointA.latitude, 8);
  Serial.print(",");
  Serial.print(myrudder.waypointA.longitude, 8);
  Serial.print(",$WAYB,");
  Serial.print(myrudder.waypointB.latitude, 8);
  Serial.print(",");
  Serial.print(myrudder.waypointB.longitude, 8);
  Serial.print(",$TAR,");
  Serial.print(myrudder.targetbearing);
  Serial.print(",$RUD,");
  Serial.print(rudder);
  Serial.print(",");
  Serial.print(myrudder.localdistance);
  Serial.print(",");
  Serial.println(myrudder.sensitivity);

  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  if (dataFile)
  {
    dataFile.print("micromagic5,$TMR,");
    dataFile.print(mytime);
    dataFile.print(",$RC,");
    dataFile.print(ch1);
    dataFile.print(",");
    dataFile.print(ch2);
    dataFile.print(",");
    dataFile.print(ch3);
    dataFile.print(",$CMP,");
    dataFile.print(BoatHeading);
    dataFile.print(",");
    dataFile.print(pitch);
    dataFile.print(",");
    dataFile.print(roll);
    dataFile.print(",$ACC,");
    dataFile.print(accelx, 4);
    dataFile.print(",");
    dataFile.print(accely, 4);
    dataFile.print(",");
    dataFile.print(accelz, 4);
    dataFile.print(",$GYR,");
    dataFile.print(gyrox, 4);
    dataFile.print(",");
    dataFile.print(gyroy, 4);
    dataFile.print(",");
    dataFile.print(gyroz, 4);
    dataFile.print(",$GPS,");
    dataFile.print(gps.gpsdate, 0);
    dataFile.print(",");
    dataFile.print(gps.gpstime, 0);
    dataFile.print(",");
    dataFile.print(gps.latitude, 8);
    dataFile.print(",");
    dataFile.print(gps.latNS);
    dataFile.print(",");
    dataFile.print(gps.longitude, 8);
    dataFile.print(",");
    dataFile.print(gps.lonEW);
    dataFile.print(",");
    dataFile.print(gps.altitude);
    dataFile.print(",");
    dataFile.print(gps.fixquality);
    dataFile.print(",");
    dataFile.print(gps.numsatelites);
    dataFile.print(",");
    dataFile.print(gps.gpsknots);
    dataFile.print(",");
    dataFile.print(gps.gpstrack);
    dataFile.print(",");
    dataFile.print(gps.gpsstatus);
    dataFile.print(",$WND,");
    dataFile.print(mywind.Direction);
    dataFile.print(",$WLS,");
    dataFile.print(myrudder.wls.slope);
    dataFile.print(",");
    dataFile.print(myrudder.wls.intercept);
    dataFile.print(",$LINE,");
    dataFile.print(myrudder.alongtrack);
    dataFile.print(",");
    dataFile.print(myrudder.alongtrackobjective);
    dataFile.print(",");
    dataFile.print(myrudder.crosstrack);
    dataFile.print(",$WAYA,");
    dataFile.print(myrudder.waypointA.latitude, 8);
    dataFile.print(",");
    dataFile.print(myrudder.waypointA.longitude, 8);
    dataFile.print(",$WAYB,");
    dataFile.print(myrudder.waypointB.latitude, 8);
    dataFile.print(",");
    dataFile.print(myrudder.waypointB.longitude, 8);
    dataFile.print(",$WAYD,");
    dataFile.print(myrudder.waypointD.latitude, 8);
    dataFile.print(",");
    dataFile.print(myrudder.waypointD.longitude, 8);
    dataFile.print(",$WAYE,");
    dataFile.print(myrudder.waypointE.latitude, 8);
    dataFile.print(",");
    dataFile.print(myrudder.waypointE.longitude, 8);
    dataFile.print(",$TAR,");
    dataFile.print(myrudder.targetbearing);
    dataFile.print(",$RUD,");
    dataFile.print(rudder);
    dataFile.print(",");
    dataFile.print(myrudder.localdistance);
    dataFile.print(",");
    dataFile.println(myrudder.sensitivity);

    dataFile.close();
    delay(100);
  }
}

void InitialiseSDcard(int testseconds)
{
  Serial.print("Initializing SD card...");

  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD failed");
    for (i=1;i<=testseconds;i++)
    {
      digitalWrite(13, HIGH);
      delay(100);
      digitalWrite(13, LOW);
      delay(900);
    }
    return;
  }
  Serial.println("done.");

  if (ReadWriteTest()){
    Serial.println("OK");
    digitalWrite(13, HIGH);
    delay(testseconds * 1000);
    digitalWrite(13, LOW);
  }
  else
  {
    Serial.println("ERROR");
    for (i=1;i<=testseconds;i++)
    {
      digitalWrite(13, HIGH);
      delay(500);
      digitalWrite(13, LOW);
      delay(500);
    }
  }  
}

void InfoTest()
{
  Sd2Card SDcard;
  SdVolume volume;
  SdFile root;

  // Card information
  Serial.print("\nCard info: ");
  switch(SDcard.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.print("SD1"); break;
    case SD_CARD_TYPE_SD2:
      Serial.print("SD2"); break;
    case SD_CARD_TYPE_SDHC:
      Serial.print("SDHC"); break;
    default:
      Serial.print("Unknown");
  }

  // Find the volume on the SD card
  if (!volume.init(SDcard)) {
    Serial.println("\nNo FAT16/FAT32 partition.");
    return;
  }

  // FAT type
  uint32_t volumesize;
  Serial.print(", FAT"); Serial.print(volume.fatType(), DEC);
  Serial.print(", ");
  
  // Sector size (or Blocks) is fixed at 512 bytes
  volumesize = volume.blocksPerCluster() * volume.clusterCount() * 512;
  Serial.print(volumesize/1024);
  Serial.println(" Kb"); 

  Serial.println("\nFiles on the SD card: ");
  root.openRoot(volume);
  
  // list all files in the card with date and size
  root.ls(LS_R | LS_DATE | LS_SIZE);
}

bool ReadWriteTest()
{
  File myFile;
  char filename[] = "testfile.txt";
  char writestring[] = "abcdefghijklmnopqrstuvwxyz1234567890";
  char readstring[40];

  // First remove the file is it already exists
  if (SD.exists(filename)) {
    SD.remove(filename);
  }
  
  // Open file to write
  myFile = SD.open(filename, FILE_WRITE);
  
  // If okay, write to the file
  if (myFile) {
    Serial.print("Writing to file...  ");
    myFile.print(writestring);
    myFile.close();
    Serial.print('[');
    Serial.print(writestring);
    Serial.println("] done.");
  } 
  else 
  {
    // Error writing to the file
    Serial.println("error opening testfile.txt");
  }
  
  // Open file to read. Which is the default option
  myFile = SD.open(filename, FILE_READ);
  if (myFile) {
    Serial.print("Reading from file...");
    int n = 0;
    while (myFile.available()) {
      if (n<39)
      {
        readstring[n] = myFile.read();
        readstring[n+1] = '\0';
      }
      n=n+1;
    }
    myFile.close();
    Serial.print('[');
    Serial.print(readstring);
    Serial.println("] done.");
  } 
  else 
  {
    // Error reading from the file
    Serial.println("error opening testfile.txt");
  }

  // Return true if the two char arrays are equal
  if (strcmp(writestring, readstring) == 0){
    return true;
  }
  return false;
}



