#include <FS.h>
#include <ArduinoJson.h>
#include <math.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
//#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
//#include <ArduinoOTA.h>
#include <TimeLib.h>
#include <Ticker.h>
#include <ShiftRegister74HC595.h>

#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

#include <TimeLib.h>
#include <Timezone.h>

#define AP_NAME "NICK_IN12D_"
#define FW_VERSION "2.00"
#define CONFIG_TIMEOUT 300000 // 300000 = 5 minutes

// ONLY CHANGE DEFINES BELOW IF YOU KNOW WHAT YOU'RE DOING!
#define NORMAL_MODE 0
#define OTA_MODE 1
#define CONFIG_MODE 2
#define CONFIG_MODE_LOCAL 3
#define CONNECTION_FAIL 4
#define UPDATE_SUCCESS 1
#define UPDATE_FAIL 2
#define A 0
#define B 1
#define C 2
#define D 3

// User global vars
const char* update_path = "/update";
const char* update_username = "nick";
const char* update_password = "nick";
const char* ntpServerName = "pool.ntp.org";
RgbColor colorConfigMode = RgbColor(60, 0, 60);
RgbColor colorConfigSave = RgbColor(0, 0, 60);
RgbColor colorWifiConnecting = RgbColor(90, 45, 0);
RgbColor colorWifiSuccess = RgbColor(0, 60, 0);
RgbColor colorWifiFail = RgbColor(90, 0, 0);
RgbColor colorStartupDisplay = RgbColor(0, 70, 50);
RgbColor red[] = {
  RgbColor(40, 0, 0), // LOW
  RgbColor(60, 0, 0), // MEDIUM
  RgbColor(90, 0, 0), // HIGH
};
RgbColor green[] = {
  RgbColor(0, 30, 0), // LOW
  RgbColor(0, 40, 0), // MEDIUM
  RgbColor(0, 60, 0), // HIGH
};
RgbColor colonColorDefault[] = {
  RgbColor(36, 14, 2), // MEDIUM
  RgbColor(50, 19, 3), // MEDIUM
  RgbColor(70, 22, 4), // HIGH
};
//RgbColor colonColorDefault = RgbColor(90, 27, 7);
//RgbColor colonColorDefault = RgbColor(38, 12, 2);
uint8_t pinsFix[2][4] = { // A,B,C,D
  {7, 5, 4, 6},
  {3, 1, 0, 2},
};
uint8_t digitPins[4][11][4] = {
  {
    {1, 1, 0, 0},//0
    {0, 0, 0, 1},//1
    {0, 1, 0, 0},//2
    {1, 0, 0, 1},//3
    {1, 0, 1, 0},//4
    {0, 0, 1, 0},//5
    {0, 0, 0, 0},//7
    {1, 0, 0, 0},//6
    {0, 1, 1, 0},//8
    {1, 1, 1, 0},//9
    {1, 1, 1, 1},// blank hack
  },
  {
    {1, 1, 0, 0},//0
    {1, 0, 0, 1},//1
    {0, 1, 0, 0},//2
    {0, 0, 0, 1},//3
    {0, 0, 0, 0},//4
    {1, 0, 0, 0},//5
    {1, 0, 1, 0},//6
    {0, 0, 1, 0},//7
    {0, 1, 1, 0},//8
    {1, 1, 1, 0},//9
    {1, 1, 1, 1},// blank hack
  },
  {
    {1, 0, 0, 1},//0
    {0, 0, 0, 1},//1
    {0, 0, 0, 0},//2
    {1, 0, 0, 0},//3
    {1, 0, 1, 0},//4
    {0, 0, 1, 0},//5
    {0, 1, 1, 0},//6
    {1, 1, 1, 0},//7
    {1, 1, 0, 0},//8
    {0, 1, 0, 0},//9
    {1, 1, 1, 1},// blank hack
  },
  {
    {0, 1, 0, 0},//0
    {0, 0, 0, 1},//1
    {0, 0, 0, 0},//2
    {1, 0, 0, 0},//3
    {1, 1, 0, 0},//4
    {1, 1, 1, 0},//5
    {0, 1, 1, 0},//6
    {1, 0, 1, 0},//7
    {0, 0, 1, 0},//8
    {1, 0, 0, 1},//9
    {1, 1, 1, 1},// blank hack
  },
};
// Cathode poisoning prevention pattern --> circle through least used digits, prioritize number 7
uint8_t healPattern[4][10] = {
  {7, 3, 4, 7, 5, 6, 7, 8, 7, 9},
  {4, 5, 6, 7, 8, 9, 6, 7, 8, 9},
  {6, 7, 8, 7, 9, 6, 7, 8, 9, 7},
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
};
// Multiplexing timings (in milliseconds) for brightness, first value = ON TIME, second value = OFF TIME
// These values need to be as low as possible but also the lower they are, the more likely there will be visible 
// glitching due to wifi activity. 2ms should be a minimum. Glitching is less noticable with higher brightness.
uint8_t bri_vals[3][2] = {
  {2, 4}, // LOW
  {2, 2}, // MEDIUM
  {5, 1}, // HIGH
};


// Better left alone global vars
unsigned long configStartMillis, prevDisplayMillis;
const uint8_t PixelCount = 5; // make sure to set this to the number of pixels in your strip
uint8_t deviceMode = NORMAL_MODE;
bool timeUpdateFirst = true;
bool toggleSeconds = false;
byte mac[6];
volatile uint8_t multiplexState = 0;
volatile uint8_t digitsCache[] = {0, 0, 0, 0};
uint8_t healIterator, bri;
uint8_t timeUpdateStatus = 0; // 0 = no update, 1 = update success, 2 = update fail,
uint8_t failedAttempts = 0;
RgbColor colonColor;
IPAddress ip_addr;

TimeChangeRule EDT = {"EDT", Last, Sun, Mar, 1, 120};  //UTC + 2 hours
TimeChangeRule EST = {"EST", Last, Sun, Oct, 1, 60};  //UTC + 1 hours
Timezone TZ(EDT, EST);
NeoPixelBus<NeoGrbFeature, NeoWs2813Method> strip(PixelCount);
NeoPixelAnimator animations(PixelCount);
RgbColor currentColor;
DynamicJsonDocument json(2048); // config buffer
ShiftRegister74HC595<1> shift(13, 14, 15);
Ticker ticker;
Ticker ledsTicker;
Ticker onceTicker;
Ticker colonTicker;
ESP8266WebServer server(80);
WiFiClient espClient;
WiFiUDP Udp;
ESP8266HTTPUpdateServer httpUpdateServer;
unsigned int localPort = 8888;  // local port to listen for UDP packets

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  Serial.begin(115200);

  pinMode (0, INPUT_PULLUP );
  pinMode (4, OUTPUT );
  pinMode (5, OUTPUT );

  digitalWrite(5, 0);
  digitalWrite(4, 0);

  strip.Begin();
  strip.ClearTo(RgbColor(0, 0, 0));
  strip.Show();

  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS: Failed to mount file system");
  }
  readConfig();

  WiFi.macAddress(mac);

  const char* ssid = json["ssid"].as<const char*>();
  const char* pass = json["pass"].as<const char*>();
  const char* ip = json["ip"].as<const char*>();
  const char* gw = json["gw"].as<const char*>();
  const char* sn = json["sn"].as<const char*>();

  if (ssid != NULL && pass != NULL && ssid[0] != '\0' && pass[0] != '\0') {
    Serial.println("WIFI: Setting up wifi");
    WiFi.mode(WIFI_STA);

    if (ip != NULL && gw != NULL && sn != NULL && ip[0] != '\0' && gw[0] != '\0' && sn[0] != '\0') {
      IPAddress ip_address, gateway_ip, subnet_mask;
      if (!ip_address.fromString(ip) || !gateway_ip.fromString(gw) || !subnet_mask.fromString(sn)) {
        Serial.println("Error setting up static IP, using auto IP instead. Check your configuration.");
      } else {
        WiFi.config(ip_address, gateway_ip, subnet_mask);
      }
    }

    // serializeJson(json, Serial);

    WiFi.begin(ssid, pass);

    for (int i = 0; i < 500; i++) {
      if (WiFi.status() != WL_CONNECTED) {
        if (i > 100) {
          deviceMode = CONFIG_MODE;
          Serial.print("WIFI: Failed to connect to: ");
          Serial.print(ssid);
          Serial.println(", going into config mode.");
          strip.ClearTo(colorWifiFail);
          strip.Show();
          delay(500);
          break;
        }
        if (i % 2 == 0) {
          strip.ClearTo(colorWifiConnecting);
        } else {
          strip.ClearTo(RgbColor(0, 0, 0));
        }
        strip.Show();
        delay(100);
      } else {
        Serial.println("WIFI: Connected...");
        Serial.print("WIFI: Connected to: ");
        Serial.println(WiFi.SSID());
        Serial.print("WIFI: Mac address: ");
        Serial.println(WiFi.macAddress());
        Serial.print("WIFI: IP address: ");
        Serial.println(WiFi.localIP());
        strip.ClearTo(colorWifiSuccess);
        strip.Show();
        delay(500);
        break;
      }
    }

  } else {
    deviceMode = CONFIG_MODE;
    Serial.println("SETTINGS: No credentials set, going to config mode.");
  }

  bri = json["bri"].as<int>();
  initScreen();

  if (deviceMode == CONFIG_MODE || deviceMode == CONNECTION_FAIL) {
    startConfigPortal(); // Blocking loop
  } else {
    ndp_setup();
    startLocalConfigPortal();
  }

  colonColor = colonColorDefault[bri];

  //initColon();

  if (json["rst_cycle"].as<unsigned int>() == 1) {
    cycleDigits();
    delay(500);
  }

  if (json["rst_ip"].as<unsigned int>() == 1) {
    showIP(5000);
    delay(500);
  }

}

// the loop function runs over and over again forever
void loop() {

  if (timeUpdateFirst == true && timeUpdateStatus == UPDATE_FAIL || deviceMode == CONNECTION_FAIL) {
    setAllDigitsTo(0);
    strip.ClearTo(red[3]); // red
    strip.Show();
    return;
  }

  if (millis() - prevDisplayMillis >= 1000) { //update the display only if time has changed
    prevDisplayMillis = millis();
    toggleNightMode();

    if (
      (json["cathode"].as<int>() == 1 && (hour() >= 2 && hour() <= 6) && minute() < 10) ||
      (json["cathode"].as<int>() == 2 && (((hour() >= 2 && hour() <= 6) && minute() < 10) || minute() < 1))
    ){
      healingCycle(); // do healing loop if the time is right :)
    } else {
      if (timeUpdateStatus) {
        if (timeUpdateStatus == UPDATE_SUCCESS) {
          setTemporaryColonColor(5, green[bri]);
        }
        if (timeUpdateStatus == UPDATE_FAIL) {
          if (failedAttempts > 2) {
            colonColor = red[bri];
          } else {
            setTemporaryColonColor(5, red[bri]);
          }
        }
        timeUpdateStatus = 0;
      }

      handleColon();
      showTime();
    }
  }

  if (animations.IsAnimating()) animations.UpdateAnimations();
  strip.Show();

  server.handleClient();
  //MDNS.update();
}
