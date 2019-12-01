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
#define AP_NAME "NIXIECLOCK_IN12_DUPLEX"

const uint16_t PixelCount = 5; // make sure to set this to the number of pixels in your strip
NeoPixelBus<NeoGrbFeature, NeoWs2813Method> strip(PixelCount);

NeoPixelAnimator animations(PixelCount);
RgbColor currentColor;

// parameters: (number of shift registers, data pin, clock pin, latch pin)
ShiftRegister74HC595 shift(1, 13, 14, 15);

bool showLeadingZero = true;
bool toggleSeconds = false;

int REFRESH_RATE = 3; // ms
int DARK_RATE = 1; // ms
volatile uint8_t multiplexState = 0;
volatile uint8_t digitsCache[] = {0, 0, 0, 0};

uint8_t pinsFix[2][4] = { // A,B,C,D
  {7, 5, 4, 6},
  {3, 1, 0, 2},
};
uint8_t digitPins[4][10][4] = {
  {
    {1, 1, 0, 0},//0
    {0, 0, 0, 1},//1
    {0, 1, 0, 0},//2
    {1, 0, 0, 1},//3
    {1, 0, 1, 0},//4
    {0, 0, 1, 0},//5
    {1, 0, 0, 0},//6
    {0, 0, 0, 0},//7
    {0, 1, 1, 0},//8
    {1, 1, 1, 0},//9
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
    strip.ClearTo(RgbColor(0, 90, 90));
    strip.Show();
  });

  ArduinoOTA.begin();

  ticker.attach_ms(REFRESH_RATE, refreshScreen, multiplexState); // medium brightness

  strip.Begin();
  strip.Show();


  for (int i = 0; i < 10; i++) {
    setDigit(0, 9 - i);
    setDigit(1, i);
    setDigit(2, i);
    setDigit(3, 9 - i);
    delay(500);
  }
  
  /*
  int REFRESH_RATE_ORIG = REFRESH_RATE; // ms
  int DARK_RATE_ORIG = DARK_RATE; // ms
  REFRESH_RATE = 303;
  DARK_RATE = 101;
  
  setDigit(0, 1);
  setDigit(1, 2);
  setDigit(2, 3);
  setDigit(3, 4);

  while (REFRESH_RATE > REFRESH_RATE_ORIG && DARK_RATE > DARK_RATE_ORIG) {
    REFRESH_RATE -= 3;
    DARK_RATE -= 1;
    delay(200);
  }

  REFRESH_RATE = 3;
  DARK_RATE = 1;

  delay(2000);
  */
}

// the loop function runs over and over again forever
void loop() {

  ArduinoOTA.handle();

  if (millis() - prevDisplay >= 1000) { //update the display only if time has changed
    prevDisplay = millis();
    Serial.println("another second");
    if (timeUpdateResult) {
      if (timeUpdateResult == UPDATE_SUCCESS) {
        strip.ClearTo(RgbColor(0, 250, 0)); // green
        strip.Show();

        for (int i = 0; i < 10; i++) {
          for (int ii = 0; ii < 10; ii++) {
            setAllDigitsTo(ii);
            delay(50);
          }
        }

      }
      if (timeUpdateResult == UPDATE_FAIL) {
        strip.ClearTo(RgbColor(250, 0, 0)); // red
        strip.Show();
        delay(1000);
      }
      timeUpdateResult = 0;
    }
    showTime();
    strip.ClearTo(RgbColor(0, 0, 0));

    toggleSeconds = !toggleSeconds;
    if (toggleSeconds) {
      //SetupAnimations(RgbColor(0, 0, 0), RgbColor(38, 12, 2));

      SetupAnimations(RgbColor(90, 27, 7), RgbColor(0, 0, 0));
    }
    else {
      strip.ClearTo(RgbColor(0, 0, 0));
      SetupAnimations(RgbColor(0, 0, 0), RgbColor(90, 27, 7));
    }
    if (failedAttempts > 2) {
      strip.ClearTo(RgbColor(90, 0, 0));
    }
  }

  if (animations.IsAnimating()) animations.UpdateAnimations();
  strip.Show();

}
