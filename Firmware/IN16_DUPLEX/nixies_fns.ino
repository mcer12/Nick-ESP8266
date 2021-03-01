void setDigit(uint8_t digit, uint8_t value) {
  //shift.set(digitPins[digit][value], 1); // set single pin HIGH
  digitsCache[digit] = value;
}

void setAllDigitsTo(uint16_t value) {
  for (int i = 0; i < 4; i++) {
    setDigit(i, value);
  }
}

void initScreen() {
  ticker.attach_ms(bri_vals[bri][0], refreshScreen, multiplexState); // medium brightness
  blankAllDigits();
}

void refreshScreen(uint8_t state) {
  if (state == 0) {

    for (int i = 0; i < 4; i++) {
      shift.setNoUpdate(pinsFix[0][i], digitPins[0][digitsCache[0]][i]);
      shift.setNoUpdate(pinsFix[1][i], digitPins[2][digitsCache[2]][i]);
    }
    shift.updateRegisters();

    digitalWrite(5, 1);
    digitalWrite(4, 0);


    multiplexState = state + 1;
    ticker.attach_ms(bri_vals[bri][0], refreshScreen, multiplexState); // medium brightness
    return;

  }
  if (state == 2) {

    for (int i = 0; i < 4; i++) {
      shift.setNoUpdate(pinsFix[0][i], digitPins[1][digitsCache[1]][i]);
      shift.setNoUpdate(pinsFix[1][i], digitPins[3][digitsCache[3]][i]);
    }
    shift.updateRegisters();

    digitalWrite(5, 0);
    digitalWrite(4, 1);

    multiplexState = state + 1;
    ticker.attach_ms(bri_vals[bri][0], refreshScreen, multiplexState); // medium brightness
    return;

  }

  // Or darken
  digitalWrite(5, 0);
  digitalWrite(4, 0);
  //shift.setAllHigh();

  if (state > 2) multiplexState = 0;
  else multiplexState = state + 1;

  ticker.attach_ms(bri_vals[bri][1], refreshScreen, multiplexState); // medium brightness

  return;
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
  } else if (hours == 0 && json["t_format"].as<int>() == 0) {
    hours = 12;
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
  strip.ClearTo(colorStartupDisplay);
  strip.Show();
  for (int i = 0; i < 10; i++) {
    setDigit(0, i);
    setDigit(1, i);
    setDigit(2, i);
    setDigit(3, i);
    delay(1000);
  }
  strip.ClearTo(RgbColor(0, 0, 0));
  strip.Show();
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

  strip.ClearTo(colorStartupDisplay);
  strip.Show();
  delay(delay_ms);
  strip.ClearTo(RgbColor(0, 0, 0));
  strip.Show();
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
    strip.Show();
    for (int i = 0; i < 4; i++) {
      setDigit(i, healPattern[i][healIterator]);
    }
    healIterator++;
    if(healIterator > 9) healIterator = 0;
}
