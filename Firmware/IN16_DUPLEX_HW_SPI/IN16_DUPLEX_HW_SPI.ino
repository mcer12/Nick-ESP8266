#include <FS.h>
#include <ArduinoJson.h>
#include <math.h>

#include "ESP8266TimerInterrupt.h"
#include "SPI.h"

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Ticker.h>

#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

#include <TimeLib.h>
#include <Timezone.h>

#define AP_NAME "NICK_IN16D_"
#define FW_NAME "NICK"
#define FW_VERSION "2.1.1"
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
#define DATA 13
#define CLOCK 14
#define LATCH 15
#define TIMER_INTERVAL_uS 500 // 700uS is a minimum that doesn't produce ghosting using 400k pulldown resistors on optocouplers. 200k might allow to go below 500uS.

// User global vars
const char* dns_name = "nick"; // only for AP mode
const char* update_path = "/update";
const char* update_username = "nick";
const char* update_password = "nick";
const char* ntpServerName = "pool.ntp.org";

const int connectingAnimationSteps = 500; // dotsAnimationSteps * TIMER_INTERVAL_uS = one animation cycle time in microseconds

HsbColor red[] = {
  HsbColor(RgbColor(100, 0, 0)), // LOW
  HsbColor(RgbColor(150, 0, 0)), // MEDIUM
  HsbColor(RgbColor(200, 0, 0)), // HIGH
};
HsbColor green[] = {
  HsbColor(RgbColor(0, 100, 0)), // LOW
  HsbColor(RgbColor(0, 150, 0)), // MEDIUM
  HsbColor(RgbColor(0, 200, 0)), // HIGH
};
HsbColor blue[] = {
  HsbColor(RgbColor(0, 0, 100)), // LOW
  HsbColor(RgbColor(0, 0, 150)), // MEDIUM
  HsbColor(RgbColor(0, 0, 200)), // HIGH
};
HsbColor yellow[] = {
  HsbColor(RgbColor(100, 100, 0)), // LOW
  HsbColor(RgbColor(150, 150, 0)), // MEDIUM
  HsbColor(RgbColor(200, 200, 0)), // HIGH
};
HsbColor purple[] = {
  HsbColor(RgbColor(100, 0, 100)), // LOW
  HsbColor(RgbColor(150, 0, 150)), // MEDIUM
  HsbColor(RgbColor(200, 0, 200)), // HIGH
};
HsbColor azure[] = {
  HsbColor(RgbColor(0, 100, 100)), // LOW
  HsbColor(RgbColor(0, 150, 150)), // MEDIUM
  HsbColor(RgbColor(0, 200, 200)), // HIGH
};

RgbColor colonColorDefault[] = {
  RgbColor(10, 3, 0), // LOW
  RgbColor(10, 3, 0), // LOW
  RgbColor(17, 5, 1), // HIGH
};
//RgbColor colonColorDefault = RgbColor(90, 27, 7);
//RgbColor colonColorDefault = RgbColor(38, 12, 2);
uint8_t pinsFix[2][4] = { // A,B,C,D
  {4, 6, 7, 5},
  {3, 1, 0, 2},
};
uint8_t digitPins[4][11][4] = {
  {
    {1, 1, 0, 0},
    {0, 0, 0, 1},
    {0, 0, 1, 0},
    {0, 1, 0, 0},
    {0, 0, 0, 0},
    {1, 0, 0, 0},
    {1, 0, 1, 0},
    {1, 0, 0, 1},
    {0, 1, 1, 0},
    {1, 1, 1, 0},
    {1, 1, 1, 1},
  },
  {
    {1, 1, 0, 0},
    {0, 0, 0, 1},
    {0, 0, 1, 0},
    {0, 1, 0, 0},
    {0, 0, 0, 0},
    {1, 0, 0, 0},
    {1, 0, 1, 0},
    {1, 0, 0, 1},
    {0, 1, 1, 0},
    {1, 1, 1, 0},
    {1, 1, 1, 1},
  },
  {
    {1, 1, 0, 0},
    {0, 0, 0, 1},
    {0, 0, 1, 0},
    {0, 1, 0, 0},
    {0, 0, 0, 0},
    {1, 0, 0, 0},
    {1, 0, 1, 0},
    {1, 0, 0, 1},
    {0, 1, 1, 0},
    {1, 1, 1, 0},
    {1, 1, 1, 1},
  },
  {
    {1, 1, 0, 0},
    {0, 0, 0, 1},
    {0, 0, 1, 0},
    {0, 1, 0, 0},
    {0, 0, 0, 0},
    {1, 0, 0, 0},
    {1, 0, 1, 0},
    {1, 0, 0, 1},
    {0, 1, 1, 0},
    {1, 1, 1, 0},
    {1, 1, 1, 1},
  },
};
// Cathode poisoning prevention pattern --> circle through least used digits, prioritize number 7
uint8_t healPattern[4][10] = {
  {7, 3, 4, 7, 5, 6, 7, 8, 7, 9},
  {4, 5, 6, 7, 8, 9, 6, 7, 8, 9},
  {6, 7, 8, 7, 9, 6, 7, 8, 9, 7},
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
};

const uint8_t pwmResolution = 8; // should be in the multiples of 4 to enable smooth crossfade
uint8_t bri_vals[3] = {
  1, // LOW
  3, // MEDIUM
  8, // HIGH
};

// Better left alone global vars
volatile bool isPoweredOn = true;
const uint8_t registersCount = 1;
unsigned long configStartMillis, prevDisplayMillis;
const uint8_t PixelCount = 5; // Addressable LED count
uint8_t deviceMode = NORMAL_MODE;
bool timeUpdateFirst = true;
bool toggleSeconds = false;
byte mac[6];
volatile uint8_t multiplexState = 0;
volatile uint8_t pwmState = 0;
volatile uint8_t digitsCache[] = {0, 0, 0, 0};
volatile uint8_t bytes[registersCount];
uint8_t healIterator, bri;
uint8_t timeUpdateStatus = 0; // 0 = no update, 1 = update success, 2 = update fail,
uint8_t failedAttempts = 0;
volatile bool enableConnectingAnimation;
volatile unsigned short connectingAnimationState;
RgbColor colonColor;
IPAddress ip_addr;

TimeChangeRule EDT = {"EDT", Last, Sun, Mar, 1, 120};  //UTC + 2 hours
TimeChangeRule EST = {"EST", Last, Sun, Oct, 1, 60};  //UTC + 1 hours
Timezone TZ(EDT, EST);
NeoPixelBus<NeoGrbFeature, NeoWs2813Method> strip(PixelCount);
NeoGamma<NeoGammaTableMethod> colorGamma;
NeoPixelAnimator animations(PixelCount);
RgbColor currentColor;
DynamicJsonDocument json(2048); // config buffer
ESP8266Timer ITimer;
DNSServer dnsServer;
Ticker ticker;
Ticker onceTicker;
Ticker colonTicker;
ESP8266WebServer server(80);
WiFiUDP Udp;
ESP8266HTTPUpdateServer httpUpdateServer;
unsigned int localPort = 8888;  // local port to listen for UDP packets

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  Serial.begin(115200);

  pinMode (4, OUTPUT);
  pinMode (5, OUTPUT);

  digitalWrite(5, 0);
  digitalWrite(4, 0);


  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS: Failed to mount file system");
  }
  readConfig();

  initStrip();
  initRgbColon();
  initScreen();

  WiFi.macAddress(mac);

  const char* ssid = json["ssid"].as<const char*>();
  const char* pass = json["pass"].as<const char*>();
  const char* ip = json["ip"].as<const char*>();
  const char* gw = json["gw"].as<const char*>();
  const char* sn = json["sn"].as<const char*>();

  if (ssid != NULL && pass != NULL && ssid[0] != '\0' && pass[0] != '\0') {
    Serial.println("[WIFI] Connecting to: " + String(ssid));
    WiFi.mode(WIFI_STA);

    if (ip != NULL && gw != NULL && sn != NULL && ip[0] != '\0' && gw[0] != '\0' && sn[0] != '\0') {
      IPAddress ip_address, gateway_ip, subnet_mask;
      if (!ip_address.fromString(ip) || !gateway_ip.fromString(gw) || !subnet_mask.fromString(sn)) {
        Serial.println("[WIFI] Error setting up static IP, using auto IP instead. Check your configuration.");
      } else {
        WiFi.config(ip_address, gateway_ip, subnet_mask);
      }
    }
    // serializeJson(json, Serial);

    enableConnectingAnimation = true; // Start the dots animation

    updateColonColor(yellow[bri]);
    strip_show();

    WiFi.hostname(AP_NAME + macLastThreeSegments(mac));
    WiFi.begin(ssid, pass);

    for (int i = 0; i < 1000; i++) {
      if (WiFi.status() != WL_CONNECTED) {
        if (i > 200) { // 20s timeout
          enableConnectingAnimation = false;
          deviceMode = CONFIG_MODE;
          updateColonColor(red[bri]);
          strip_show();
          Serial.print("[WIFI] Failed to connect to: " + String(ssid) + ", going into config mode.");
          delay(500);
          break;
        }
        delay(100);
      } else {
        updateColonColor(green[bri]);
        enableConnectingAnimation = false;
        strip_show();
        Serial.print("[WIFI] Successfully connected to: ");
        Serial.println(WiFi.SSID());
        Serial.print("[WIFI] Mac address: ");
        Serial.println(WiFi.macAddress());
        Serial.print("[WIFI] IP address: ");
        Serial.println(WiFi.localIP());
        delay(1000);
        break;
      }
    }
  } else {
    deviceMode = CONFIG_MODE;
    Serial.println("[CONF] No credentials set, going to config mode.");
  }

  if (deviceMode == CONFIG_MODE || deviceMode == CONNECTION_FAIL) {
    startConfigPortal(); // Blocking loop
  } else {
    ndp_setup();
    startServer();
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
    strip_show();
    return;
  }

  if (millis() - prevDisplayMillis >= 1000) { //update the display only if time has changed
    prevDisplayMillis = millis();
    toggleNightMode();

    if (
      (json["cathode"].as<int>() == 1 && (hour() >= 2 && hour() <= 6) && minute() < 10) ||
      (json["cathode"].as<int>() == 2 && (((hour() >= 2 && hour() <= 6) && minute() < 10) || minute() < 1))
    ) {
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
  strip_show();

  server.handleClient();
  delay(1); // Keeps the ESP cold!
}
