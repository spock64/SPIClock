# 1 "/var/folders/5s/yxp9nbtn4zv0vygn0bj1fnhr0000gn/T/tmp2o1qRF"
#include <Arduino.h>
# 1 "/Users/j/Documents/PlatformIO/Projects/SPI-Clock/src/main.ino"
# 43 "/Users/j/Documents/PlatformIO/Projects/SPI-Clock/src/main.ino"
#define jNOWIFI 


#include <Arduino.h>

#ifdef jBME280
#include <BME280SPI.h>
#include <Wire.h>
#include <PubSubClient.h>
#endif


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

#include <ArduinoJson.h>

#include <TFT_eSPI.h>
#include <SPI.h>


#include <TimeLib.h>
#include <WiFiUdp.h>

const char *sday[] = {"???", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
const char *smonth[] = {"???", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

#ifdef jBME280
BME280SPI bme;
#endif

TFT_eSPI tft = TFT_eSPI();

uint32_t targetTime = 0;

byte omm = 99;
boolean initial = 1;
byte xcolon = 0;
unsigned int colour = 0;

#define BUTTON D1


HTTPClient emon;

const char* emonpi="192.168.16.31";

const char* emonpi_key="ac5917ee9dd4abf77baeb7118fd303dd";

long t_last_emonpi_rd = 0;

#ifdef jBME280


WiFiClient espClient;

PubSubClient client(espClient);
long lastMsg = 0;
char msg[100];
int value = 0;
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
static uint8_t conv2d(const char* p);
int getEmonEnergy();
void default_display();
void setup_display();
void switch_display_mode();
void check_button();
void setup(void);
void loop(void);
void digitalClockDisplay();
void printDigits(int digits);
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
#line 109 "/Users/j/Documents/PlatformIO/Projects/SPI-Clock/src/main.ino"
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();


  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);


  } else {
    digitalWrite(BUILTIN_LED, HIGH);
  }

}



void reconnect() {

  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    if (client.connect("ESP8266Client")) {
      Serial.println("connected");




    } else {



      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

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

uint8_t hh=conv2d(__TIME__), mm=conv2d(__TIME__+3), ss=conv2d(__TIME__+6);

const char* host = "pjr-clock";
const char* ssid = "ROGERS";
const char* password = "*jaylm123456!";

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
# 329 "/Users/j/Documents/PlatformIO/Projects/SPI-Clock/src/main.ino"
int getEmonEnergy()
{




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


  return -1;
}




void default_display()
{

  Serial.printf("Default display");

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor (8, 52);


  byte xpos = 6;
  byte ypos = 0;



  if (omm != minute())
  {
    omm = minute();

    tft.setTextColor(0x39C4, TFT_BLACK);



    tft.drawString("88:88",xpos,ypos,7);
    tft.setTextColor(0xFBE0, TFT_BLACK);

    if (hour()<10) xpos+= tft.drawChar('0',xpos,ypos,7);
    xpos+= tft.drawNumber(hour(),xpos,ypos,7);
    xcolon=xpos;
    xpos+= tft.drawChar(':',xpos,ypos,7);
    if (minute()<10) xpos+= tft.drawChar('0',xpos,ypos,7);
    tft.drawNumber(minute(),xpos,ypos,7);
  }


  if (second()%2)
  {
    tft.setTextColor(0x39C4, TFT_BLACK);
    xpos+= tft.drawChar(':',xcolon,ypos,7);
    tft.setTextColor(0xFBE0, TFT_BLACK);
  }
  else {
    tft.drawChar(':',xcolon,ypos,7);
  }


  tft.fillRect (0, 52, 160,10, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);

  char buffer[30];
  sprintf(buffer,"%02d:%02d:%02d %02d %s %d", hour(), minute(), second(), day(), smonth[month()], year());
  tft.drawString(buffer,8,52);

  if (second() % 15 == 0)
  {
    static int curKW = 1000;

#ifdef jNOWIFI

    curKW++;
#else
    if((curKW = getEmonEnergy()) == -1)
    {


    }
    else
    {

#endif

      char buf[20];
      sprintf(buf, "%d", curKW);


      tft.fillRect (0, 75, 160,50, TFT_BLACK);
      tft.setTextColor(TFT_WHITE);
      tft.drawString(buf,8,75, 7);

#ifndef jNOWIFI
    }
#endif

  }


}

void setup_display()
{
  Serial.printf("Setup display");
}



int display_mode = 0;

typedef void (*display_func)();

typedef struct {
  const char * name;
  display_func func;
} display_mode_t;



display_mode_t displays[] = { {"Setup", setup_display},
                              {"Default", default_display},

                            };


void switch_display_mode()
{
  if((display_mode+1) < (sizeof(displays)/sizeof(display_mode_t)))
    display_mode++;
  else
    display_mode = 0;
}




typedef enum {b_inactive, b_down, b_up} b_state_t;

b_state_t b_state;

void check_button(){
  int b;
  static int last_b = -1;

  b = digitalRead(BUTTON);

  if(b != last_b)
  {
    last_b = b;

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



void setup(void){


  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting ...");

#ifdef jNOWIFI
  Serial.println("*** NO WIFI - WILL NOT ENTER STA MODE ***");
#endif


  pinMode(BUTTON, INPUT);
  pinMode(BUILTIN_LED, OUTPUT);

  digitalWrite(BUILTIN_LED, 1);

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

#ifdef jBME280
  pinMode(CS_BME, OUTPUT);

  digitalWrite(CS_BME, 0);


  Wire.begin(D3,D4);

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


  }


  digitalWrite(CS_BME, 1);

#endif


targetTime = millis() + 1000;


  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);

#ifdef jNOWIFI


  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor (8, 0);
  tft.print("*** DEV WiFi OFF ***");
  WiFi.mode(WIFI_OFF);


  setTime(11,0,0,4,5,1999);




#else


  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor (8, 0);
  tft.print("Starting WiFi ...");

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);



  while(WiFi.waitForConnectResult() != WL_CONNECTED){
    WiFi.begin(ssid, password);
    Serial.println("WiFi failed, retrying.");
    tft.setCursor (8, 0);
    tft.print("Re-trying WiFi ...");
  }

  tft.setCursor (8, 16);
  tft.print("Getting the time ...");


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
#endif

}

extern time_t prevDisplay;



void loop(void)
{

#ifdef jNOWIFI
#else
  httpServer.handleClient();
#endif

  check_button();

  if(b_state == b_up)
  {
    Serial.printf("Button up ... display mode was %s(%d)", displays[display_mode].name, display_mode);
    switch_display_mode();
    Serial.printf(" now %s(%d)\n", displays[display_mode].name, display_mode);

    b_state = b_inactive;
  }

#ifdef jBME280



  float temp(NAN), hum(NAN), pres(NAN);

  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);

#ifdef jNOWIFI
#else


  if (!client.connected())
  {
    reconnect();
  }


  client.loop();
#endif

  long now = millis();
  if (now - lastMsg > 2000)
  {
    lastMsg = now;
    ++value;

    bme.read(pres, temp, hum, tempUnit, presUnit);





    int t, h, p;



    t = (int) (temp * 100.0);
    h = (int) (hum * 100.0);
    p = (int) pres;







    snprintf (msg, 75, "{ \"T\" : \"%d.%02d\", \"H\" : \"%d.%02d\", \"P\" : \"%d.%02d\" }",
                            t / 100, t % 100,
                            h / 100, h % 100,
                            p / 100, p %100);



    Serial.print("Publish message: "); Serial.println(msg);

#ifdef jNOWIFI
#else

    client.publish("outTopic", msg);
#endif

  }

#endif


  if (timeStatus() != timeNotSet)
  {
    if (now() != prevDisplay)
    {
      extern void digitalClockDisplay();

      prevDisplay = now();
      digitalClockDisplay();


      default_display();
    }
  }

}
# 1 "/Users/j/Documents/PlatformIO/Projects/SPI-Clock/src/time.ino"



static const char ntpServerName[] = "192.168.16.1";







const int timeZone = 0;






WiFiUDP Udp;
unsigned int localPort = 8888;

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

time_t prevDisplay = 0;

void digitalClockDisplay()
{

  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.printf("%d\r",year());

}

void printDigits(int digits)
{

  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}



const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime()
{
  IPAddress ntpServerIP;

  while (Udp.parsePacket() > 0) ;
  Serial.println("Transmit NTP Request");

  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;

      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0;
}


void sendNTPpacket(IPAddress &address)
{

  memset(packetBuffer, 0, NTP_PACKET_SIZE);


  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;

  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;


  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}