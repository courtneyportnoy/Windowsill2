///////////////LIBRARIES///////////////////
#include <Adafruit_VC0706.h>
#include <SD.h>
#include <SoftwareSerial.h>   

// for wifi
#include <SPI.h>
#include <WiFi.h>

/////////////////////////////
//VARS
//the time we give the sensor to calibrate (10-60 secs according to the datasheet)
int calibrationTime = 30;        

int pirPin = 7;    //the digital pin connected to the PIR sensor's output
int currentState; 
int lastState;
String pictureTaken = "";


//******************************** vars for wifi *****************************//
char ssid[] = "itpsandbox"; //  your network SSID (name) 
char pass[] = "NYU+s0a!+P?";    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
WiFiClient client;

const char server[] = "courtney-mitchell.com";

boolean readingDate = false;
int theTime = 0;

unsigned long lastConnectionTime = 0;          // last time you connected to the server, in milliseconds
boolean lastConnected = false;                 // state of the connection last time through the main loop
const unsigned long postingInterval = 300*1000;  // delay between updates, in milliseconds


//************************* vars for camera**************************************//
#define chipSelect 4 //define SD pin
// On Uno: camera TX connected to pin 2, camera RX to pin 3:
SoftwareSerial cameraconnection = SoftwareSerial(2, 5);
Adafruit_VC0706 cam = Adafruit_VC0706(&cameraconnection);

/////////////////////////////
//SETUP
void setup(){
  Serial.begin(9600);
  pinMode(pirPin, INPUT);

  //digitalWrite(pirPin, LOW);

  //give the sensor some time to calibrate
  Serial.print("calibrating sensor ");
  for(int i = 0; i < calibrationTime; i++){
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" done");
  Serial.println("SENSOR ACTIVE");
  delay(50);

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  } 
  else {
    Serial.println("Card Found!");
  } 

  // locate camera
  if (cam.begin()) {
    Serial.println("Camera Found:");
  } 
  else {
    Serial.println("No camera found?");
    return;
  } 
  while( status != WL_CONNECTED) { 
    status = WiFi.begin(ssid, pass);
    Serial.println("attempting to connect to the internet");
    delay(10000);
  }
  Serial.println("Connected to wifi");
  printWifiStatus();

}

////////////////////////////
//LOOP
void loop(){
  // check current state of sensor
  currentState = digitalRead(pirPin);
  if(currentState != lastState && currentState == HIGH) {
    Serial.println("state changed, taking picture");
    pictureTaken = takePicture(); //if picture taken is true,     
  }

  //if not connected, connect
  if(!client.connected() && (millis() - lastConnectionTime > postingInterval)) {
    Serial.println("not connected, too much time passed, sending httpRequest not");
    httpRequest("");
  }
  // store the state of the connection for next time through
  if(client.connected()) {
    Serial.println("i'm connected, getting hour now");
    theTime = getHour();      
  }
  if (theTime != 0) {
    //make sure we got an hour successfully, set last connection time to current time

    lastConnectionTime = millis();
  }

  if (pictureTaken != "") {

    httpRequest(pictureTaken);
    //pictureTaken = httpRequest();
  }

  lastState = currentState;
  lastConnected = client.connected();
}


// Function to take picture and save it to SD card
String takePicture() {

  Serial.println("VC0706 Camera snapshot test");
  // Set the picture size - you can choose one of 640x480, 320x240 or 160x120 
  // cam.setImageSize(VC0706_640x480);        // biggest
  //cam.setImageSize(VC0706_320x240);        // medium
  cam.setImageSize(VC0706_160x120);          // small

  // You can read the size back from the camera (optional, but maybe useful?)
  int imgsize = cam.getImageSize();
  //  Serial.print("Image size: ");
  //  if (imgsize == VC0706_640x480) Serial.println("640x480");
  //  if (imgsize == VC0706_320x240) Serial.println("320x240");
  //  if (imgsize == VC0706_160x120) Serial.println("160x120");

  Serial.println("Snap in 3 secs...");
  delay(3000);

  if (! cam.takePicture()) 
    Serial.println("Failed to snap!");
  else 
    Serial.println("Picture taken!");

  // Create an image with the name IMAGExx.JPG
  char filename[13];
  strcpy(filename, "IMAGE00.JPG");
  for (int i = 0; i < 100; i++) {
    filename[5] = '0' + i/10;
    filename[6] = '0' + i%10;
    // create if does not exist, do not open existing, write, sync after write
    if (! SD.exists(filename)) {
      break;
    }
  }

  // Open the file for writing
  File imgFile = SD.open(filename, FILE_WRITE);

  // Get the size of the image (frame) taken  
  uint16_t jpglen = cam.frameLength();
  Serial.print("Storing ");
  Serial.print(jpglen, DEC);
  Serial.print(" byte image.");
  Serial.print(" as ");
  Serial.println(filename);

  int32_t time = millis();
  pinMode(8, OUTPUT);
  // Read all the data up to # bytes!
  byte wCount = 0; // For counting # of writes
  while (jpglen > 0) {
    // read 32 bytes at a time;
    uint8_t *buffer;
    uint8_t bytesToRead = min(32, jpglen); // change 32 to 64 for a speedup but may not work with all setups!
    buffer = cam.readPicture(bytesToRead);
    imgFile.write(buffer, bytesToRead);
    if(++wCount >= 64) { // Every 2K, give a little feedback so it doesn't appear locked up
      Serial.print('.');
      wCount = 0;
    }
    //Serial.print("Read ");  Serial.print(bytesToRead, DEC); Serial.println(" bytes");
    jpglen -= bytesToRead;
  }
  imgFile.close();

  //  time = millis() - time;
  //  Serial.println("done!");
  //  Serial.print(time); 
  //  Serial.println(" ms elapsed");
  return filename;
}


// this method makes a HTTP connection to the server:
void httpRequest(String thisFile) {
  char filename[13];
  thisFile.toCharArray(filename, 13);
  unsigned long contentLength = 0;
  File imgFile = SD.open(filename, FILE_WRITE);
  contentLength = imgFile.size();

  // if there's a successful connection:
  if (client.connect(server, 80)) {
    Serial.println(F("connecting to courtney's server..."));
    // send the HTTP PUT request:
    client.println(F("POST /windowsill/fileupload.php HTTP/1.1"));
    client.println(F("Host: courtney-mitchell.com"));
    client.print(F("Content-Type: multipart/form-data; boundary=--cmDKce13"));
    client.println(F("User-Agent: arduino-ethernet"));

    // Open the file for writing

    // this is where the file read goes. work out content length
    client.print(F("Content-length:"));
    client.println(contentLength);
    Serial.println(contentLength);
    client.println();
    client.println(F("--cmDKce13"));
    client.println(F("Content-Disposition: form-data; name=\"submit\""));
    client.println();
    client.println(F("Upload "));
    client.println(F("--cmDKce13"));
    client.print(F("Content-Disposition: form-data; name=\"file\"; filename="));
    client.println(filename);
    client.println("Content-Type: image/jpeg");

    Serial.println("Beginning image upload");
    int bytes = 0;
    // while file available, write to client
    while (imgFile.available()) {
      client.write(imgFile.read());
      bytes++;
      Serial.println(bytes);
    }
    // close the file:
    imgFile.close();

    client.println(F("--cmDKce13"));    
    client.println(F("Connection: close"));
    client.println();

    // note the time that the connection was made:
    lastConnectionTime = millis();
  } 
  else {
    // if you couldn't make a connection:
    Serial.println(F("connection failed"));
    Serial.println(F("disconnecting."));
    client.stop();
  }
}

int getHour() {
  String numberString;
  int reading = 0;

  int theNumber=0;
  while (client.available()) {
    // read a character:
    char c = client.read();
    // pointy bracket opens the numberstring:
    if (c == '<') {
      // empty the string for new data:
      numberString = "";
      // start reading the number:
      reading = true; 
    } 
    else {
      // if you're reading, and you haven't hit the >:
      if (reading) {
        if (c != '>') {
          // add the new char to the string:
          numberString += c;
        } 
        else {
          char argh[4];
          numberString.toCharArray(argh, 4);
          theNumber = atoi(argh);
          Serial.println(theNumber); 
          client.stop();
          //return the number
          return theNumber;
        }
      }
    }
  }
  return theNumber;
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}







