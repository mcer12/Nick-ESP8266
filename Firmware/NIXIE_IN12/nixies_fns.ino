void setDigit(uint16_t digit, uint16_t value) {

  if (digit > 3) return; // there is only 4 digits!

  byte* digitDriverPins = driverPins[digit];
  byte* digitValues = digits[digit][value];

  for (int i = 0; i < sizeof(digitDriverPins); i++) {
    shift.set(digitDriverPins[i], digitValues[i]); // set single pin HIGH
  }
}

void setAllDigitsTo(uint16_t value) {
  for (int i = 0; i < sizeof(digits); i++) {
    setDigit(sizeof(digits) - 1 - i, value);
  }
}

void blankDigit(uint16_t digit) {
  if (digit > 3) return; // there is only 4 digits!

  byte* digitDriverPins = driverPins[digit];

  for (int i = 0; i < sizeof(digitDriverPins); i++) {
    shift.set(digitDriverPins[i], 1); // set all ABCD high to turn off digit
  }
}

void blankAllDigits() {
  shift.setAllHigh();
}

void showTime() {
  int splitTime[4] = {
    (hour() / 10) % 10,
    hour() % 10,
    (minute() / 10) % 10,
    minute() % 10,
  };
  for (int i = 0; i < sizeof(splitTime); i++) {
    if (i == 0 && splitTime[0] == 0 && !showLeadingZero) {
      blankDigit(i);
      continue;
    }
    setDigit(i, splitTime[i]);
  }
}
