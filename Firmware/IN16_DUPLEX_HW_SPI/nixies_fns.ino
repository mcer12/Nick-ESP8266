
void ICACHE_RAM_ATTR shiftSetValue(uint8_t pin, bool value) {
  //(value) ? bitSet(bytes[pinsToRegisterMap[pin]], pinsToBitsMap[pin]) : bitClear(bytes[pinsToRegisterMap[pin]], pinsToBitsMap[pin]);
  (value) ? bitSet(bytes[pin / 8], pin % 8) : bitClear(bytes[pin / 8], pin % 8);
}

void ICACHE_RAM_ATTR shiftSetAll(bool value) {
  for (int i = 0; i < registersCount * 8; i++) {
    //(value) ? bitSet(bytes[pinsToRegisterMap[i]], pinsToBitsMap[i]) : bitClear(bytes[pinsToRegisterMap[i]], pinsToBitsMap[i]);
    (value) ? bitSet(bytes[i / 8], i % 8) : bitClear(bytes[i / 8], i % 8);
  }
}

void ICACHE_RAM_ATTR shiftWriteBytes(byte a1/*, byte a2, byte a3, byte a4, byte a5, byte a6*/) {

  SPI.transfer(a1);
  //SPI.transfer(a2);
  //SPI.transfer(a3);
  //SPI.transfer(a4);
  //SPI.transfer(a5);
  //SPI.transfer(a6);

  // set gpio through register manipulation, fast!
  GPOS = 1 << LATCH;
  GPOC = 1 << LATCH;
  /*
    digitalWrite(LATCH, HIGH);
    digitalWrite(LATCH, LOW);
  */
}
/*
  void ICACHE_RAM_ATTR TimerHandler()
  {
  for (int i = 0; i < 6; i++) {
    for (int ii = 0; ii < 8; ii++) {
      if (dutyState < segmentBrightness[i][ii]) {
        shiftSetValue(digitPins[i][ii], true);
      } else {
        shiftSetValue(digitPins[i][ii], false);
      }
    }
  }

  shiftWriteBytes(bytes[5], bytes[4], bytes[3], bytes[2], bytes[1], bytes[0]); // Digits are reversed (first shift register = last digit etc.)

  if (dutyState >= pwmResolution) dutyState = 0;
  else dutyState++;
  }
*/
void ICACHE_RAM_ATTR refreshScreen() {

  // Only one ISR timer is available so if we want the dots to not glitch during wifi connection, we need to put it here...
  // speed of the dots depends on refresh frequency of the display
  if (enableConnectingAnimation) {

    int stepCount = connectingAnimationSteps / 4;

    for (int i = 0; i < 4; i++) {
      if (connectingAnimationState >= stepCount * i && connectingAnimationState < stepCount * (i + 1)) {
        digitsCache[i] = 1;
      } else {
        digitsCache[i] = 10;
      }
    }

    connectingAnimationState++;
    if (connectingAnimationState >= connectingAnimationSteps) connectingAnimationState = 0;
  }



  if (multiplexState == 0 && pwmState <= bri_vals[bri]) {


    for (int i = 0; i < 4; i++) {
      shiftSetValue(pinsFix[0][i], digitPins[0][digitsCache[0]][i]);
      shiftSetValue(pinsFix[1][i], digitPins[2][digitsCache[2]][i]);
    }

    shiftWriteBytes(/*bytes[5], bytes[4], bytes[3], bytes[2], bytes[1],*/ bytes[0]); // Digits are reversed (first shift register = last digit etc.)

    //WRITE_PERI_REG(0x60000304, (1 << 5)); // ON
    //WRITE_PERI_REG( 0x60000308, (1 << 4) ); // OFF
    //digitalWrite(5, 1);
    //digitalWrite(4, 0);

    GPOS = 1 << 5;
    GPOC = 1 << 4;

    multiplexState++;

    return;

  }
  if (multiplexState == 2 && pwmState <= bri_vals[bri]) {

    for (int i = 0; i < 4; i++) {
      shiftSetValue(pinsFix[0][i], digitPins[1][digitsCache[1]][i]);
      shiftSetValue(pinsFix[1][i], digitPins[3][digitsCache[3]][i]);
    }

    shiftWriteBytes(/*bytes[5], bytes[4], bytes[3], bytes[2], bytes[1],*/ bytes[0]); // Digits are reversed (first shift register = last digit etc.)

    GPOC = 1 << 5;
    GPOS = 1 << 4;

    multiplexState++;
    return;

  }

  // Or darken
  GPOC = 1 << 5;
  GPOC = 1 << 4;

  multiplexState++;
  if (multiplexState > 2) {
    multiplexState = 0;
    pwmState++;
  }
  if (pwmState > pwmResolution) pwmState = 0;
  return;

}

void enableScreen() {
  ITimer.attachInterruptInterval(TIMER_INTERVAL_uS, refreshScreen);
}

void disableScreen() {
  ITimer.detachInterrupt();
}

void initScreen() {
  pinMode(DATA, INPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(LATCH, OUTPUT);
  digitalWrite(LATCH, LOW);

  bri = json["bri"].as<int>();
  //crossFadeTime = json["fade"].as<int>();

  SPI.begin();
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV16);

  ITimer.attachInterruptInterval(TIMER_INTERVAL_uS, refreshScreen);
  // 30ms slow 240ms, 20 => medium 160ms, 15 => quick 120ms
  /*
    if (crossFadeTime > 0) {
      fade_animation_ticker.attach_ms(crossFadeTime, handleFade); // handle dimming animation
    } else {
      fade_animation_ticker.attach_ms(20, handleFade); // handle dimming animation
    }
  */
  blankAllDigits();
}




void setDigit(uint8_t digit, uint8_t value) {
  //shift.set(digitPins[digit][value], 1); // set single pin HIGH
  digitsCache[digit] = value;
}

void setAllDigitsTo(uint16_t value) {
  for (int i = 0; i < 4; i++) {
    setDigit(i, value);
  }
}

void blankDigit(uint8_t digit) {
  setDigit(digit, 10);
}

void blankAllDigits() {
  for (int i = 0; i < 4; i++) {
    setDigit(i, 10);
  }
}

void showTime() {
  int hours = hour();
  if (hours > 12 && json["t_format"].as<int>() == 0) { // 12 / 24 h format
    hours -= 12;
  }
  int splitTime[4] = {
    (hours / 10) % 10,
    hours % 10,
    (minute() / 10) % 10,
    minute() % 10,
  };

  for (int i = 0; i < 4; i++) {
    if (i == 0 && splitTime[0] == 0 && json["zero"].as<int>() == 0) {
      blankDigit(i);
      continue;
    }
    setDigit(i, splitTime[i]);
  }
}

void cycleDigits() {
  updateColonColor(azure[bri]);
  strip.Show();
  for (int i = 0; i < 10; i++) {
    setDigit(0, i);
    setDigit(1, i);
    setDigit(2, i);
    setDigit(3, i);
    delay(1000);
  }
  strip.ClearTo(RgbColor(0, 0, 0));
  strip_show();
}

void showIP(int delay_ms) {
  IPAddress ip_addr = WiFi.localIP();
  blankDigit(0);
  if ((ip_addr[3] / 100) % 10 == 0) {
    blankDigit(1);
  } else {
    setDigit(1, (ip_addr[3] / 100) % 10);
  }
  if ((ip_addr[3] / 10) % 10 == 0) {
    blankDigit(2);
  } else {
    setDigit(2, (ip_addr[3] / 10) % 10);
  }
  setDigit(3, (ip_addr[3]) % 10);

  updateColonColor(azure[bri]);
  strip_show();
  delay(delay_ms);
  strip.ClearTo(RgbColor(0, 0, 0));
  strip_show();
}

void toggleNightMode() {
  if (json["nmode"].as<int>() == 0) return;
  if (hour() >= 22 || hour() <= 6) {
    bri = 0;
    return;
  }
  bri = json["bri"].as<int>();
}

void healingCycle() {
  strip.ClearTo(RgbColor(0, 0, 0)); // red
  strip_show();
  for (int i = 0; i < 4; i++) {
    setDigit(i, healPattern[i][healIterator]);
  }
  healIterator++;
  if (healIterator > 9) healIterator = 0;
}
