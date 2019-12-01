#include <ESP8266WiFi.h>

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

#include <Ticker.h>
#include <ShiftRegister74HC595.h>

#include <TimeLib.h>
#include <Timezone.h>

#define A 0
#define B 1
#define C 2
#define D 3
#define UPDATE_SUCCESS 1
#define UPDATE_FAIL 2
#define AP_NAME "NIXIECLOCK_IN16_DUPLEX"
#define REFRESH_RATE 5
#define DARK_RATE 1

const uint16_t PixelCount = 5; // make sure to set this to the number of pixels in your strip
NeoPixelBus<NeoGrbFeature, NeoWs2813Method> strip(PixelCount);

NeoPixelAnimator animations(PixelCount);
RgbColor currentColor;

// parameters: (number of shift registers, data pin, clock pin, latch pin)
ShiftRegister74HC595 shift(1, 13, 14, 15);

bool showLeadingZero = true;
bool toggleSeconds = false;

volatile uint8_t multiplexState = 0;
volatile uint8_t digitsCache[] = {0, 0, 0, 0};

uint8_t pinsFix[2][4] = { // A,B,C,D
  {4, 6, 7, 5},
  {3, 1, 0, 2},
};
uint8_t digitPins[2][10][4] = {
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
  },
};

//static const char ntpServerName[] = "us.pool.ntp.org";
//static const char ntpServerName[] = "time.nist.gov";
//static const char ntpServerName[] = "time-a.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-b.timefreq.bldrdoc.gov";
static const char ntpServerName[] = "pool.ntp.org";
const int timeZone = 0;     // Central European Time

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

unsigned long prevDisplay = 0; // when the digital clock was displayed
uint8_t timeUpdateResult = 0; // 0 = no update, 1 = update success, 2 = update fail,
uint8_t failedAttempts = 0;

bool timeUpdated = false;
TimeChangeRule czEDT = {"CEST", Last, Sun, Mar, 1, 120};  //UTC - 2 hours
TimeChangeRule czEST = {"CEST", Last, Sun, Oct, 1, 60};  //UTC - 1 hours
Timezone czLocal(czEDT, czEST);

Ticker ticker;

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  Serial.begin(9600);

  pinMode (0, INPUT_PULLUP );
  pinMode (4, OUTPUT );
  pinMode (5, OUTPUT );


  digitalWrite(5, 0);
  digitalWrite(4, 0);

  strip.Begin();
  setAllDigitsTo(1);

  WiFiManager wifiManager;
  wifiManager.autoConnect(AP_NAME);

  ndp_setup();

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(AP_NAME);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    ticker.detach();
    digitalWrite(5, 0);
    digitalWrite(4, 0);
    strip.ClearTo(RgbColor(0, 4, 4));
    strip.Show();
  });

  ArduinoOTA.begin();

  //Serial.print("IP address: ");
  //Serial.println(WiFi.localIP());

  ticker.attach_ms(REFRESH_RATE, refreshScreen, multiplexState); // medium brightness

  strip.ClearTo(RgbColor(4, 3, 0));
  strip.Show();

  for (int i = 0; i < 10; i++) {
    setDigit(0, i);
    setDigit(1, 9 - i);
    setDigit(2, 9 - i);
    setDigit(3, i);
    delay(500);
  }


}

// the loop function runs over and over again forever
void loop() {

  ArduinoOTA.handle();

  if (millis() - prevDisplay >= 1000) { //update the display only if time has changed
    prevDisplay = millis();

    if (timeUpdateResult) {
      if (timeUpdateResult == UPDATE_SUCCESS) {
        strip.ClearTo(RgbColor(0, 4, 0));

        strip.Show();

        for (int i = 0; i < 5; i++) {
          for (int ii = 0; ii < 10; ii++) {
            setAllDigitsTo(ii);
            delay(50);
          }
        }

      }
      if (timeUpdateResult == UPDATE_FAIL) {
        strip.ClearTo(RgbColor(4, 0, 0));
        strip.Show();
        delay(1000);
      }
      timeUpdateResult = 0;
    }
    showTime();
    strip.ClearTo(RgbColor(0, 0, 0));

    toggleSeconds = !toggleSeconds;
    if (toggleSeconds) {
      strip.ClearTo(RgbColor(4, 1, 0));
      //SetupAnimations(RgbColor(0, 0, 0), RgbColor(4, 1, 0));
      //SetupAnimations(RgbColor(60, 24, 10), RgbColor(0, 0, 0)); // ENABLE FOR DOTS !!!!!!!!!!!!
    }
    else {
      strip.ClearTo(RgbColor(0, 0, 0));
      //SetupAnimations(RgbColor(0, 0, 0), RgbColor(60, 24, 10));
      //SetupAnimations(RgbColor(4, 1, 0), RgbColor(0, 0, 0));
    }
    if (failedAttempts > 2) {
      strip.ClearTo(RgbColor(4, 0, 0));
    }
  }

  if (animations.IsAnimating()) animations.UpdateAnimations();
  strip.Show();

}
