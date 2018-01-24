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


// Only called when the time is known ...
void update_lcd()
{

  // Anti flicker needed ??
  // Remember what was last displayed ...

  // Day changed ...
  // Hour changed ...
  // Sec changed ...
  // Overwrite in Background colour, then redraw
  // Only draw the minimum !

    // if (second()==0 || initial) {
      // initial = 0;
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor (8, 52);

// *** DRAW THE REAL DATE ***
// Each day !
    //tft.print(__DATE__); // This uses the standard ADAFruit small font

// *** DRAW THE REAL WEATHER ***
// Say every hour ?
      //tft.setTextColor(TFT_BLUE, TFT_BLACK);
      //tft.drawCentreString("It is rainy",120,48,2); // Next size up font 2


    // Update digital time
    byte xpos = 6;
    byte ypos = 0;

    // Every minute, redisplay the time
    // if (omm != mm) { // Only redraw every minute to minimise flicker
    if (omm != minute())
    { // Only redraw every minute to minimise flicker
      omm = minute();
      // Uncomment ONE of the next 2 lines, using the ghost image demonstrates text overlay as time is drawn over it
      tft.setTextColor(0x39C4, TFT_BLACK);  // Leave a 7 segment ghost image, comment out next line!
      //tft.setTextColor(TFT_BLACK, TFT_BLACK); // Set font colour to black to wipe image
      // Font 7 is to show a pseudo 7 segment display.
      // Font 7 only contains characters [space] 0 1 2 3 4 5 6 7 8 9 0 : .
      tft.drawString("88:88",xpos,ypos,7); // Overwrite the text to clear it
      tft.setTextColor(0xFBE0, TFT_BLACK); // Orange

      // omm = mm;

      if (hour()<10) xpos+= tft.drawChar('0',xpos,ypos,7);
      // if (hh<10) xpos+= tft.drawChar('0',xpos,ypos,7);
//      xpos+= tft.drawNumber(hh,xpos,ypos,7);
      xpos+= tft.drawNumber(hour(),xpos,ypos,7);
      xcolon=xpos;
      xpos+= tft.drawChar(':',xpos,ypos,7);
//      if (mm<10) xpos+= tft.drawChar('0',xpos,ypos,7);
//      tft.drawNumber(mm,xpos,ypos,7);
      if (minute()<10) xpos+= tft.drawChar('0',xpos,ypos,7);
      tft.drawNumber(minute(),xpos,ypos,7);
    }

//    if (ss%2) { // Flash the colon
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

      if (second() % 15 == 0)
      {
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
              return;
            }
            else
            {
              int val = root["value"];
              long t = root["time"];

              Serial.printf("Value read is %d from %ld\n", val, t_last_emonpi_rd);

              // Update the screen ...
              char buf[20];
              sprintf(buf, "%d", val);

              // Clear old value ...
              tft.fillRect (0, 75, 160,50, TFT_BLACK);

              // Red text means that there's a problem with the emonpi !
              tft.setTextColor(
                      t != t_last_emonpi_rd ? TFT_WHITE : TFT_RED
                    );
              tft.drawString(buf,8,75, 7);

              t_last_emonpi_rd = t;

            }
          }

        }
        else
          Serial.println("rq failed!");

      }


}

void default_display()
{
  Serial.printf("Default display");
}

void setup_display()
{
  Serial.printf("Setup display");
}

// Display Modes
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

                            };

// responds to the button ...
void switch_display_mode()
{
  if((display_mode+1) < (sizeof(displays)/sizeof(display_mode_t)))
    display_mode++;
  else
    display_mode = 0;
}

extern WiFiUDP Udp;
extern unsigned int localPort;

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

void setup(void){

// Debug ...
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting ...");

  // Button
  pinMode(BUTTON, INPUT);
  pinMode(BUILTIN_LED, OUTPUT);

  digitalWrite(BUILTIN_LED, 1); //off

  if(digitalRead(BUTTON))
  {
    printf("Button pressed on load\n");
    b_state = b_down;
  }
  else
  {
    b_state = b_inactive;
  }



  // Set up LCD ..
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Note: the new fonts do not draw the background colour

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor (8, 0);
  tft.print("Starting WiFi ..."); // standard ADAFruit small font


  targetTime = millis() + 1000;

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

  setSyncProvider(getNtpTime);
  setSyncInterval(300);

}

extern time_t prevDisplay;

void loop(void)
{

  httpServer.handleClient();
  check_button();

  if(b_state == b_up)
  {
    Serial.printf("Button up ... display mode was %s(%d)", displays[display_mode].name, display_mode);
    switch_display_mode();
    Serial.printf(" now %s(%d)\n", displays[display_mode].name, display_mode);

    b_state = b_inactive;
  }

  // Once a second ?
  // Has the time changed? and/or has it even been set?
  if (timeStatus() != timeNotSet)
  {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
      update_lcd();
    }
  }
}
