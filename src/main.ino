//#include <Arduino.h>

/* PJR
    Was based on example sketch to show TFT lib capabilities
    TODO:
      Remote update - using ESP8266UpdateServer
      Use proper time from NTP - Done (but ntp server is pfsense !!!)
      Config Html Pages
      Draw the proper date
      Draw seconds ?


      Read weather (from somewhere)

      Now with BME280 support via SPI ...

----
 For a more accurate clock, it would be better to use the RTClib library.
 But this is just a demo.

A few colour codes:

code	color
0x0000	Black
0xFFFF	White
0xBDF7	Light Gray
0x7BEF	Dark Gray
0xF800	Red
0xFFE0	Yellow
0xFBE0	Orange
0x79E0	Brown
0x7E0	Green
0x7FF	Cyan
0x1F	Blue
0xF81F	Pink

----
 */

// Define this to avoid connecting to WiFi
// useful for developing when travelling
// Instead put up a portal on the default AP
#define jNOWIFI
#define jBME280

#include <Arduino.h>

#ifdef jBME280

#include <BME280Spi.h>
#include <Wire.h>
#include <PubSubClient.h>

#define BME_CS D2 // or at least it should be ...

BME280Spi::Settings bme_settings(BME_CS);
BME280Spi bme(bme_settings);

float temp(NAN), hum(NAN), pres(NAN);

BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
BME280::PresUnit presUnit(BME280::PresUnit_Pa);

// in ms
#define RD_SENSOR_INTERVAL 60000

#endif


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

#include <ArduinoJson.h>

#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>

// For NTP support
#include <TimeLib.h>
#include <WiFiUdp.h>

const char *sday[] = {"???", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
const char *smonth[] = {"???", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};


TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

uint32_t targetTime = 0;       // for next 1 second timeout

byte omm = 99;
boolean initial = 1;
byte xcolon = 0;
unsigned int colour = 0;

#define BUTTON D1

// Pulling stuff from emoncms ...
HTTPClient emon;

const char* emonpi="192.168.16.31";
// Needed to read data ...
const char* emonpi_key="ac5917ee9dd4abf77baeb7118fd303dd";

long t_last_emonpi_rd = 0;

#ifdef jBME280


WiFiClient espClient;

PubSubClient client(espClient);
long tlastSensorRd = 0;
char msg[100];

// *** MQTT *** ----------------------------------------------------------------

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("outTopic", "hello world");
      // ... and resubscribe
      //client.subscribe("inTopic");
    } else {
      //***********************************************
      // Not battery friendly ...
      // **********************************************
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


#endif


static uint8_t conv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9')
    v = *p - '0';
  return 10 * v + *++p - '0';
}

uint8_t hh=conv2d(__TIME__), mm=conv2d(__TIME__+3), ss=conv2d(__TIME__+6);  // Get H, M, S from compile time

const char* host = "pjr-clock";
const char* ssid = "ROGERS";
const char* password = "*jaylm123456!";

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

// *** ORIGINAL DISPLAY UPDATER *** --------------------------------------------

// Only called when the time is known ...
// void update_lcd()
// {
//
//   // Anti flicker needed ??
//   // Remember what was last displayed ...
//
//   // Day changed ...
//   // Hour changed ...
//   // Sec changed ...
//   // Overwrite in Background colour, then redraw
//   // Only draw the minimum !
//
//     // if (second()==0 || initial) {
//       // initial = 0;
//     tft.setTextColor(TFT_GREEN, TFT_BLACK);
//     tft.setCursor (8, 52);
//
// // *** DRAW THE REAL DATE ***
// // Each day !
//     //tft.print(__DATE__); // This uses the standard ADAFruit small font
//
// // *** DRAW THE REAL WEATHER ***
// // Say every hour ?
//       //tft.setTextColor(TFT_BLUE, TFT_BLACK);
//       //tft.drawCentreString("It is rainy",120,48,2); // Next size up font 2
//
//
//     // Update digital time
//     byte xpos = 6;
//     byte ypos = 0;
//
//     // Every minute, redisplay the time
//     // if (omm != mm) { // Only redraw every minute to minimise flicker
//     if (omm != minute())
//     { // Only redraw every minute to minimise flicker
//       omm = minute();
//       // Uncomment ONE of the next 2 lines, using the ghost image demonstrates text overlay as time is drawn over it
//       tft.setTextColor(0x39C4, TFT_BLACK);  // Leave a 7 segment ghost image, comment out next line!
//       //tft.setTextColor(TFT_BLACK, TFT_BLACK); // Set font colour to black to wipe image
//       // Font 7 is to show a pseudo 7 segment display.
//       // Font 7 only contains characters [space] 0 1 2 3 4 5 6 7 8 9 0 : .
//       tft.drawString("88:88",xpos,ypos,7); // Overwrite the text to clear it
//       tft.setTextColor(0xFBE0, TFT_BLACK); // Orange
//
//       // omm = mm;
//
//       if (hour()<10) xpos+= tft.drawChar('0',xpos,ypos,7);
//       // if (hh<10) xpos+= tft.drawChar('0',xpos,ypos,7);
// //      xpos+= tft.drawNumber(hh,xpos,ypos,7);
//       xpos+= tft.drawNumber(hour(),xpos,ypos,7);
//       xcolon=xpos;
//       xpos+= tft.drawChar(':',xpos,ypos,7);
// //      if (mm<10) xpos+= tft.drawChar('0',xpos,ypos,7);
// //      tft.drawNumber(mm,xpos,ypos,7);
//       if (minute()<10) xpos+= tft.drawChar('0',xpos,ypos,7);
//       tft.drawNumber(minute(),xpos,ypos,7);
//     }
//
// //    if (ss%2) { // Flash the colon
//     if (second()%2)
//     { // Flash the colon
//       tft.setTextColor(0x39C4, TFT_BLACK);
//       xpos+= tft.drawChar(':',xcolon,ypos,7);
//       tft.setTextColor(0xFBE0, TFT_BLACK);
//     }
//     else {
//       tft.drawChar(':',xcolon,ypos,7);
//     }
//
//       // Erase the old text with a rectangle, the disadvantage of this method is increased display flicker
//       tft.fillRect (0, 52, 160,10, TFT_BLACK);
//       tft.setTextColor(TFT_WHITE);
//
//       char buffer[30];
//       sprintf(buffer,"%02d:%02d:%02d %02d %s %d", hour(), minute(), second(), day(), smonth[month()], year());
//       tft.drawString(buffer,8,52);
//
//       if (second() % 15 == 0)
//       {
//           // Every 15 seconds
//
//         // Pull some energy data ...
//         char rq[100];
//         int rc;
//
//         sprintf(rq, "http://%s/emoncms/feed/timevalue.json?id=1&apikey=%s", emonpi, emonpi_key);
//         emon.begin(rq);
//
//         Serial.println("making http rq");
//         Serial.println(rq);
//
//         rc = emon.GET();
//
//         if(rc>0)
//         {
//           Serial.printf("RC=%d/n", rc);
//
//           if(rc == HTTP_CODE_OK)
//           {
//             String payload = emon.getString();
//
//             Serial.print("Response ''");
//             Serial.print(payload);
//             Serial.println("'");
//
//             // Need ArduinoJson ...
//             StaticJsonBuffer<200> jsonBuffer;
//             JsonObject& root = jsonBuffer.parseObject(payload);
//
//             if (!root.success())
//             {
//               Serial.println("parseObject() failed");
//               return;
//             }
//             else
//             {
//               int val = root["value"];
//               long t = root["time"];
//
//               Serial.printf("Value read is %d from %ld\n", val, t_last_emonpi_rd);
//
//               // Update the screen ...
//               char buf[20];
//               sprintf(buf, "%d", val);
//
//               // Clear old value ...
//               tft.fillRect (0, 75, 160,50, TFT_BLACK);
//
//               // Red text means that there's a problem with the emonpi !
//               tft.setTextColor(
//                       t != t_last_emonpi_rd ? TFT_WHITE : TFT_RED
//                     );
//               tft.drawString(buf,8,75, 7);
//
//               t_last_emonpi_rd = t;
//
//             }
//           }
//
//         }
//         else
//           Serial.println("rq failed!");
//
//       }
//
//
// }


// *** I/F to emoncms ... ***

int getEmonEnergy()
{
  // DUMMY FOR NOW
  // Every 15 seconds

  // Pull some energy data ...
  char rq[100];
  int rc;

  sprintf(rq, "http://%s/emoncms/feed/timevalue.json?id=1&apikey=%s", emonpi, emonpi_key);
  emon.begin(rq);

  Serial.println("making http rq");
  Serial.println(rq);

  rc = emon.GET();

  if(rc>0)
  {
    Serial.printf("RC=%d/n", rc);

    if(rc == HTTP_CODE_OK)
    {
      String payload = emon.getString();

      Serial.print("Response ''");
      Serial.print(payload);
      Serial.println("'");

      // Need ArduinoJson ...
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(payload);

      if (!root.success())
      {
        Serial.println("parseObject() failed");
        return -1;
      }
      else
      {
        int val = root["value"];
        long t = root["time"];

        Serial.printf("Value read is %d from %ld\n", val, t_last_emonpi_rd);

        t_last_emonpi_rd = t;

      }
    }

  }
  else
    Serial.println("rq failed!");

  // endstop ...
  return -1;
}

// *** DISPLAY HANDLERS *** ----------------------------------------------------

int redraw_display = 1;

void default_display()
{
  // Called every second ?
  Serial.printf("Default display");

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor (8, 52);

  // Update digital time
  byte xpos = 6;
  byte ypos = 0;

  // Every minute, redisplay the time
  // Only redraw every minute to minimise flicker
  if ((omm != minute()) ||
        redraw_display
    )
  {
    omm = minute();
    // Uncomment ONE of the next 2 lines, using the ghost image demonstrates text overlay as time is drawn over it
    tft.setTextColor(0x39C4, TFT_BLACK);  // Leave a 7 segment ghost image, comment out next line!
    //tft.setTextColor(TFT_BLACK, TFT_BLACK); // Set font colour to black to wipe image
    // Font 7 is to show a pseudo 7 segment display.
    // Font 7 only contains characters [space] 0 1 2 3 4 5 6 7 8 9 0 : .
    tft.drawString("88:88",xpos,ypos,7); // Overwrite the text to clear it
    tft.setTextColor(0xFBE0, TFT_BLACK); // Orange

    if (hour()<10) xpos+= tft.drawChar('0',xpos,ypos,7);
    xpos+= tft.drawNumber(hour(),xpos,ypos,7);
    xcolon=xpos;
    xpos+= tft.drawChar(':',xpos,ypos,7);
    if (minute()<10) xpos+= tft.drawChar('0',xpos,ypos,7);
    tft.drawNumber(minute(),xpos,ypos,7);
  }

  // Flash the colon
  if (second()%2)
  { // Flash the colon
    tft.setTextColor(0x39C4, TFT_BLACK);
    xpos+= tft.drawChar(':',xcolon,ypos,7);
    tft.setTextColor(0xFBE0, TFT_BLACK);
  }
  else {
    tft.drawChar(':',xcolon,ypos,7);
  }

  // Erase the old text with a rectangle, the disadvantage of this method is increased display flicker
  tft.fillRect (0, 52, 160,10, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);

  char buffer[30];
  sprintf(buffer,"%02d:%02d:%02d %02d %s %d", hour(), minute(), second(), day(), smonth[month()], year());
  tft.drawString(buffer,8,52);

  if ((second() % 15 == 0) ||
        redraw_display
      )
  {
    static int curKW = 1000; // initialiser only needed for NOWIFI mode !

#ifdef jNOWIFI
    // Dummy energy value
    curKW++;
#else
    if((curKW = getEmonEnergy()) == -1)
    {
      // Could not get the data from emoncms
      // TODO: What to do?
    }
    else
    {

#endif // jNOWIFI
      // Update the screen ...
      char buf[20];
      sprintf(buf, "%d", curKW);

      // Clear old value & redraw ...
      tft.fillRect (0, 75, 160,50, TFT_BLACK);
      tft.setTextColor(TFT_WHITE);
      tft.drawString(buf,8,75, 7);

#ifndef jNOWIFI
    }
#endif // jNOWIFI

  }

  redraw_display = 0;
}

void setup_display()
{
  Serial.printf("Setup display");

  if(redraw_display)
  {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Note: the new fonts do not draw the background colour
    tft.setCursor (8, 0);
    tft.print("*** Setup display ***"); // standard ADAFruit small font

    redraw_display = 0;
  }

}

void temperature_display()
{
  static int last_temp = 0;
  int t, h, p;

  Serial.printf("Temperature display");

  if(redraw_display)
  {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // Note: the new fonts do not draw the background colour

    last_temp = 0;      // Force redraw of temperature below ...
    redraw_display = 0;
  }


  // use two digits of precision
  // so t is in hundredths of degrees, and h is hundredths of millibars
  t = (int) (temp * 100.0);
  h = (int) (hum * 100.0);
  p = (int) pres;

  if(t != last_temp)
  {

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor (8, 52);

    // Update digital time
    byte xpos = 6;
    byte ypos = 0;

    tft.setTextColor(0x39C4, TFT_BLACK);
    tft.drawString("88:88",xpos,ypos,7);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    if (t < 1000) xpos+= tft.drawChar(' ',xpos,ypos,7);
    xpos+= tft.drawNumber(t / 100,xpos,ypos,7);
    xcolon=xpos;
    xpos+= tft.drawChar('.',xpos,ypos,7);
    if ((t % 100)<10) xpos+= tft.drawChar('0',xpos,ypos,7);
    tft.drawNumber(t % 100,xpos,ypos,7);

    last_temp = t;
  }

  // Pressure, Humidity ...
  snprintf (msg, 75, "H %d.%02d, P %d.%02d",
                          h / 100, h % 100,
                          p / 100, p %100);

  tft.fillRect (0, 52, 160,10, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(msg,8,52);                      

}

void other_display()
{
  Serial.printf("Other display");

  if(redraw_display)
  {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK); // Note: the new fonts do not draw the background colour
    tft.setCursor (8, 0);
    tft.print("*** Other display ***"); // standard ADAFruit small font
    redraw_display = 0;
  }


}

void last_display()
{
  Serial.printf("Last display");

  if(redraw_display)
  {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_BLUE, TFT_BLACK); // Note: the new fonts do not draw the background colour
    tft.setCursor (8, 0);
    tft.print("*** Last display ***"); // standard ADAFruit small font
    redraw_display = 0;
  }

}

// *** DISPLAY MODE & SWITCHING *** --------------------------------------------

int display_mode = 0;

typedef void (*display_func)();

typedef struct {
  const char * name;
  display_func func;
} display_mode_t;

// Set up is deliberately the first item
// Dodgy / cowboy / horrific logic will increment the state during start up ...
display_mode_t displays[] = { {"Setup", setup_display},
                              {"Default", default_display},
                              {"Temperature", temperature_display},
                              {"Other", other_display},
                              {"Last", last_display}
                            };

// responds to the button ...
void switch_display_mode()
{
  if((display_mode+1) < (sizeof(displays)/sizeof(display_mode_t)))
    display_mode++;
  else
    display_mode = 0;

  redraw_display = 1;
}

void refresh_display()
{
  // call the current display handler ...
  (displays[display_mode].func)();
}

// *** BME functions *** -------------------------------------------------------

#ifdef jBME280

void readBME()
{

  bme.read(pres, temp, hum, tempUnit, presUnit);
}

void publishBME()
{

  int t, h, p;

  // use two digits of precision
  // so t is in hundredths of degrees, and h is hundredths of millibars
  t = (int) (temp * 100.0);
  h = (int) (hum * 100.0);
  p = (int) pres;

  // This is needed as arduino / ESP does not have %f format ...
  snprintf (msg, 75, "{ \"T\" : \"%d.%02d\", \"H\" : \"%d.%02d\", \"P\" : \"%d.%02d\" }",
                          t / 100, t % 100,
                          h / 100, h % 100,
                          p / 100, p %100);

  Serial.print("Publish message: "); Serial.println(msg);

#ifndef jNOWIFI
  // publish
  if(client.connected())
    client.publish("outTopic", msg);
#endif

  }

#endif

// *** BUTTON HANDLING *** -----------------------------------------------------


typedef enum {b_inactive, b_down, b_up} b_state_t;

b_state_t b_state;

void check_button(){
  int b;
  static int last_b = -1;

  b = digitalRead(BUTTON);

  if(b != last_b)
  {
    last_b = b;
    // button state changed ...
    if(b)
    {
      Serial.printf("Button down\n");
      if(b_state != b_inactive)
      {
        Serial.printf("Bouncin' button down\n");
      }
      b_state = b_down;
    }
    else
    {
      Serial.printf("Button up\n");
      if(b_state != b_inactive)
      {
        Serial.printf("Bouncin' button up\n");
      }
      b_state = b_up;
    }

  }
}

extern WiFiUDP Udp;
extern unsigned int localPort;

// *** SETUP *** ---------------------------------------------------------------

void setup(void){

// Debug ...
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting ...");

#ifdef jNOWIFI
  Serial.println("*** NO WIFI - WILL NOT ENTER STA MODE ***");
#endif

  // Button
  pinMode(BUTTON, INPUT);
  pinMode(BUILTIN_LED, OUTPUT);

  digitalWrite(BUILTIN_LED, 1); //off

  if(!digitalRead(BUTTON))
  {
    printf("Button DN on load\n");
    b_state = b_down;
  }
  else
  {
    printf("Button UP on load\n");
    b_state = b_inactive;
  }


targetTime = millis() + 1000;

  // Set up LCD ..
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Note: the new fonts do not draw the background colour

#ifdef jNOWIFI
  // development

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor (8, 0);
  tft.print("*** DEV WiFi OFF ***"); // standard ADAFruit small font
  WiFi.mode(WIFI_OFF);

  // Frig time
  setTime(11,0,0,4,5,1999);
  // Set AP mode
  // Set up portal ?
  // Set up updater ?

#else
  // production

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor (8, 0);
  tft.print("Starting WiFi ..."); // standard ADAFruit small font

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);


  // Should we wait here or just poll in the loop ?
  while(WiFi.waitForConnectResult() != WL_CONNECTED){
    WiFi.begin(ssid, password);
    Serial.println("WiFi failed, retrying.");
    tft.setCursor (8, 0);
    tft.print("Re-trying WiFi ..."); // standard ADAFruit small font
  }

  tft.setCursor (8, 16);
  tft.print("Getting the time ..."); // standard ADAFruit small font

  // *** SET UP UPDATER SERVER ***
  MDNS.begin(host);

  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);

  Serial.printf("Getting the time ...");
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");

  // How does this work then ?
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
#endif // jNOWIFI

#ifdef jBME280

  // TFT will already have initialised the SPI ...
  //SPI.begin(); // How are pins defined? Does the TFT setup handle this?
  // SPI.begin is done in the TFT_eSPI constructor ...

  while(!bme.begin())
  {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }

  switch(bme.chipModel())
  {
     case BME280::ChipModel_BME280:
       Serial.println("Found BME280 sensor! Success.");
       break;
     case BME280::ChipModel_BMP280:
       Serial.println("Found BMP280 sensor! No Humidity available.");
       break;
     default:
       Serial.println("Found UNKNOWN sensor! Error!");

       // What do we do now ?

  }

  // force a read first time through the loop ...
  // A bit of an odd way of diong it I know ...
  tlastSensorRd = millis() - RD_SENSOR_INTERVAL -1;

#endif // jBME280



}

extern time_t prevDisplay;

// *** MAIN LOOP *** -----------------------------------------------------------

void loop(void)
{
  int update_display = 0;

#ifdef jNOWIFI
#else
  httpServer.handleClient();
#endif

  check_button();

  // move between the display modes by button presses - triggers on button release
  if(b_state == b_up)
  {
    Serial.printf("Button up ... display mode was %s(%d)", displays[display_mode].name, display_mode);
    switch_display_mode();
    Serial.printf(" now %s(%d)\n", displays[display_mode].name, display_mode);

    b_state = b_inactive;
  }

#ifdef jBME280
#ifndef jNOWIFI

  // Try to keep the MQTT connection Open
  // Would do this differently if we were on batteries...
  // connects to MQTT if not connected already
  if (!client.connected())
  {
    reconnect();
  }

  // allows MQTT connection to work properly ...
  client.loop();

#endif // jNOWIFI


  long tnow = millis();
  if (tnow - tlastSensorRd > RD_SENSOR_INTERVAL)
  {
    tlastSensorRd = tnow;

    // Read sensor ...
    readBME();

    // Send to MQTT
    publishBME();

    update_display = 1;

  }

#endif  // jBME280

  // *** now() returns the time in seconds - so this is "every second"
  if (timeStatus() != timeNotSet)
  {
    if (now() != prevDisplay)
    { //update the display only if time has changed
      extern void digitalClockDisplay();

      prevDisplay = now();
      digitalClockDisplay();

      update_display = 1;
    }
  }

  if(update_display)
  {
    // switch displays every 5 seconds ... just for fun ...
    if((now() % 5) == 0)
      switch_display_mode();

    // Call the active displays
    refresh_display();

    update_display = 0;
  }

}
