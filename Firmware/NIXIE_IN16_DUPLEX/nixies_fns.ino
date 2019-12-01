void setDigit(uint16_t digit, uint16_t value) {
  //shift.set(digitPins[digit][value], 1); // set single pin HIGH
  digitsCache[digit] = value;
}

void setAllDigitsTo(uint16_t value) {
  for (int i = 0; i < 4; i++) {
    setDigit(i, value);
  }
}

void refreshScreen(uint8_t state) {
  if (state == 0) {

    digitalWrite(5, 1);
    digitalWrite(4, 0);
    
    for (int i = 0; i < 4; i++) {
        shift.setNoUpdate(pinsFix[0][i], digitPins[0][digitsCache[0]][i]);
    }
    for (int i = 0; i < 4; i++) {
        shift.setNoUpdate(pinsFix[1][i], digitPins[0][digitsCache[2]][i]);
    }
    
    shift.updateRegisters();

    multiplexState = state + 1;
    ticker.attach_ms(REFRESH_RATE, refreshScreen, multiplexState); // medium brightness

    return;
  }

  if (state == 2) {
    
    digitalWrite(5, 0);
    digitalWrite(4, 1);
    
    for (int i = 0; i < 4; i++) {
        shift.setNoUpdate(pinsFix[0][i], digitPins[1][digitsCache[1]][i]);
    }
    for (int i = 0; i < 4; i++) {
        shift.setNoUpdate(pinsFix[1][i], digitPins[1][digitsCache[3]][i]);
    }
    shift.updateRegisters();

    multiplexState = state + 1;
    ticker.attach_ms(REFRESH_RATE, refreshScreen, multiplexState); // medium brightness
    return;
  }

  // Or darken
  digitalWrite(5, 0);
  digitalWrite(4, 0);
  shift.setAllHigh();

  if (state > 2) multiplexState = 0;
  else multiplexState = state + 1;

  ticker.attach_ms(DARK_RATE, refreshScreen, multiplexState); // medium brightness

  return;
}

void blankDigit(uint16_t digit) {
  /*
  for (int i = 0; i < 10; i++) {
    setDigit(digitPins[digit][i], 0);
  }
  */
}

void blankAllDigits() {
  for (int i = 0; i < 4; i++) {
    setDigit(i, 0);
  }
}

void showTime() {
  int splitTime[4] = {
    (hour() / 10) % 10,
    hour() % 10,
    (minute() / 10) % 10,
    minute() % 10,
  };
  for (int i = 0; i < 4; i++) {
    if (i == 0 && splitTime[0] == 0 && !showLeadingZero) {
      blankDigit(i);
      continue;
    }
    setDigit(i, splitTime[i]);
  }
}
