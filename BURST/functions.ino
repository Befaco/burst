#define TAP_TEMPO_AVG_COUNT (2)
#define TRIGGER_LENGTH (10) // 10ms is a good length to be picked up by other modules

int trigLen = TRIGGER_LENGTH;
unsigned long encoderLastTime = 0;
unsigned long encoderTaps[TAP_TEMPO_AVG_COUNT] = {0,0};
unsigned long encoderDuration = 0;
byte encoderTapsCurrent = 0;
byte encoderTapsTotal = 0; // 2 means use the duration between encoderTaps[0] and encoderTaps[1]; 3 means average of [0-2]

unsigned long pingLastTime = 0;
unsigned long pingDuration = 0;

/// flags
bool burstStarted = LOW; // if the burst is active or not

int calibratedRepetitions = 511;
int calibratedDistribution = 511;
int calibratedDivisions = 511;
int calibratedProbability = 511;

void doLedFlourish(int times)
{
  while (times--) {
    int cur = 0;
    while (cur < 16) {
      for (int i = 0; i < 4; i++) {
        digitalWrite(ledPin[i], bitRead(cur, i));
      }
      cur++;
      delay(10);
    }
    delay(20);
  }
}

void doLightShow()
{
  int count = 0;
  while (count < 3) {
    int cur = 15;
    while (cur >= 0) {
      for (int i = 3; i >= 0; i--) {
        digitalWrite(ledPin[i], bitRead(cur, i));
        delay(2);
      }
      delay(10);
      cur--;
    }
    cur = 0;
    while (cur < 16) {
      for (int i = 3; i >= 0; i--) {
        digitalWrite(ledPin[i], bitRead(cur, i));
        delay(2);
      }
      delay(10);
      cur++;
    }
    delay(20);
    count++;
  }
}

void doCalibration()
{
  calibratedRepetitions = analogRead(CV_REPETITIONS);
  calibratedDistribution = analogRead(CV_DISTRIBUTION);
  calibratedDivisions = analogRead(CV_DIVISIONS);
  calibratedProbability = analogRead(CV_PROBABILITY);

  EEPROM.write(6, (calibratedRepetitions & 0xff));
  EEPROM.write(7, ((calibratedRepetitions >> 8) & 0xff));

  EEPROM.write(8, (calibratedDistribution & 0xff));
  EEPROM.write(9, ((calibratedDistribution >> 8) & 0xff));

  EEPROM.write(10, (calibratedDivisions & 0xff));
  EEPROM.write(11, ((calibratedDivisions >> 8) & 0xff));

  EEPROM.write(12, (calibratedProbability & 0xff));
  EEPROM.write(13, ((calibratedProbability >> 8) & 0xff));

  doLedFlourish(3);
}

bool checkCalibrationMode()
{
  bounce.update();
  int buttonDown = !bounce.read();

  if (digitalRead(ENCODER_BUTTON) == 0 && buttonDown) {
    // SERIAL_PRINTLN("new calibration");
    doCalibration();
    return true;
  }
  else {
    // SERIAL_PRINTLN("reading stored calibrations");

    calibratedRepetitions = ((EEPROM.read(6) & 0xff) | ((EEPROM.read(7) << 8) & 0xff00));
    // SERIAL_PRINTLN("calibratedRepetitions: %d %04x", calibratedRepetitions, calibratedRepetitions);
    if (!calibratedRepetitions || calibratedRepetitions == 0xFFFF) calibratedRepetitions = 511;

    calibratedDistribution = ((EEPROM.read(8) & 0xff) | ((EEPROM.read(9) << 8) & 0xff00));
    // SERIAL_PRINTLN("calibratedDistribution: %d %04x", calibratedDistribution, calibratedDistribution);
    if (!calibratedDistribution || calibratedDistribution == 0xFFFF) calibratedDistribution = 511;

    calibratedDivisions = ((EEPROM.read(10) & 0xff) | ((EEPROM.read(11) << 8) & 0xff00));
    // SERIAL_PRINTLN("calibratedDivisions: %d %04x", calibratedDivisions, calibratedDivisions);
    if (!calibratedDivisions || calibratedDivisions == 0xFFFF) calibratedDivisions = 511;

    calibratedProbability = ((EEPROM.read(12) & 0xff) | ((EEPROM.read(13) << 8) & 0xff00));
    // SERIAL_PRINTLN("calibratedProbability: %d %04x", calibratedProbability, calibratedProbability);
    if (!calibratedProbability || calibratedProbability == 0xFFFF) calibratedProbability = 511;
  }
  return false;
}

#define ANALOG_MIN 20 // 0 + 20
#define ANALOG_MAX 1003 // 1023 - 20

int mapCalibratedAnalogValue(int val, int calibratedValue, int mappedValue, int start, int stop)
{
  int calVal = calibratedValue - 5; // buffer of -5 from calibration
  int outVal;

  val = constrain(val, ANALOG_MIN, ANALOG_MAX); // chop 20 off of both sides
  if (val <= calVal) {
    outVal = map(val, ANALOG_MIN, calVal + 1, start, mappedValue + (start > stop ? -1 : 1)); // 32 steps
  }
  else {
    outVal = map(val, calVal, ANALOG_MAX + 1, mappedValue, stop + (start > stop ? -1 : 1));
  }
  return outVal;
}

void calculateClock(unsigned long now)
{
  ////  Read the encoder button state
  if (digitalRead(ENCODER_BUTTON) == 0) {
    bitWrite(encoderButtonState, 0, 1);
  }
  else {
    bitWrite(encoderButtonState, 0, 0);
  }
  /// Read the ping input state
  if (digitalRead(PING_STATE) == 0) {
    bitWrite(pingInState, 0, 1);
  }
  else {
    bitWrite(pingInState, 0, 0);
  }

  if (encoderButtonState == 3) {
    if (encoderLastTime && encoderButtonState == 3 && (now - encoderLastTime > 5000)) { // held longer than 5s
      if (triggerButtonPressedTime && (now - triggerButtonPressedTime > 5000)) {
        probabilityAffectsEOC = !probabilityAffectsEOC;
        EEPROM.write(15, probabilityAffectsEOC);
        triggerButtonPressedTime = 0;
      }
      else {
        retriggerMode = !retriggerMode;
        EEPROM.write(14, retriggerMode);
      }
      doLedFlourish(2);
      encoderLastTime = 0;
      encoderTapsCurrent = 0;
      encoderTapsTotal = 0;
      encoderDuration = 0;
    }
    return;
  }

  /// if ping or tap button have raised
  if ((encoderButtonState == 1) || (pingInState == 1)) {
    byte ignore = false;
    unsigned long duration;
    unsigned long average;

    if (encoderButtonState == 1) {
      duration = (now - encoderLastTime);

      // this logic is weird, but it works:
      // if we've stored a duration, test the current duration against the stored
      //    if that's an outlier, reset everything, incl. the duration, to 0 and ignore
      //    otherwise, store the duration
      // if we haven't stored a duration, check whether there's a previous time
      // (if there isn't we just started up, I guess). And if there's no previous time
      // store the previous time and ignore. If there is a previous time, store the duration.
      // Basically, this ensures that outliers start a new tap tempo collection, and that the
      // first incoming taps are properly handled.
      if ((encoderDuration &&
           (duration > (encoderDuration * 2) || (duration < (encoderDuration >> 1))))
          || (!(encoderDuration || encoderLastTime))) {
        ignore = true;
        encoderTapsCurrent = 0;
        encoderTapsTotal = 0; // reset collection, we had an outlier
        encoderDuration = 0;
      }
      else {
        encoderDuration = duration;
      }
      encoderLastTime = now;

      if (!ignore) {
        tempoTic_Temp = now;
        encoderTaps[encoderTapsCurrent++] = duration;
        if (encoderTapsCurrent >= TAP_TEMPO_AVG_COUNT) {
          encoderTapsCurrent = 0;
        }
        encoderTapsTotal++;
        if (encoderTapsTotal > TAP_TEMPO_AVG_COUNT) {
          encoderTapsTotal = TAP_TEMPO_AVG_COUNT;
        }
        if (encoderTapsTotal > 1) { // currently this is 2, but it could change
          average = 0;
          for (int i = 0; i < encoderTapsTotal; i++) {
            average += encoderTaps[i];
          }
          masterClock_Temp = (average / encoderTapsTotal);
        }
        else {
          masterClock_Temp = encoderTaps[0]; // should be encoderDuration
        }
        calcTimePortions();
      }

      // we could do something like -- tapping the encoder will immediately enable the encoder's last tempo
      // if it has been overridden by the ping input. So you could tap a fast tempo, override it with a slow
      // ping, pull the ping cable and then tap the encoder to switch back. Similarly, sending a single ping
      // clock would enable the last ping in tempo. In this way, it would be possible to switch between two
      // tempi. Not sure if this is a YAGNI feature, but it would be possible.

      bitWrite(encoderButtonState, 1, 1);
      EEPROM.write(0, masterClock_Temp & 0xff);
      EEPROM.write(1, (masterClock_Temp >> 8) & 0xff);
      EEPROM.write(2, (masterClock_Temp >> 16) & 0xff);
      EEPROM.write(3, (masterClock_Temp >> 24) & 0xff);
    }

    if (pingInState == 1) {
      duration = (now - pingLastTime);

      if ((pingDuration && (duration > (pingDuration * 2) || (duration < (pingDuration >> 1))))
          || (!(pingDuration || pingLastTime))) {
        ignore = true;
        pingDuration = 0;
      }
      else {
        pingDuration = duration;
      }
      pingLastTime = now;

      if (!ignore) {
        tempoTic_Temp = now;
        masterClock_Temp = pingDuration;
        calcTimePortions();
      }
      bitWrite(pingInState, 1, 1);
    }
  }

#if 0
  // set master clock to 0, like the PEG
  masterClock_Temp = 0;
  encoderDuration = 0;
  encoderLastTime = 0;
#endif
  if (encoderButtonState == 2) {
    bitWrite(encoderButtonState, 1, 0);
  }
  if (pingInState == 2) {
    bitWrite(pingInState, 1, 0);
  }
}

void readTrigger(unsigned long now)
{
  bool triggerCvState = digitalRead(TRIGGER_STATE);
  bounce.update();
  int buttonDown = !bounce.read();
  if (buttonDown) {
    if (triggerButtonState) { // it's a new press
      triggerButtonPressedTime = now;
    }
    else if (!encoderButtonState && triggerButtonPressedTime && (now - triggerButtonPressedTime > 5000)) {
      // toggle initial ping output
      disableFirstClock = !disableFirstClock;
      EEPROM.write(5, disableFirstClock);
      doLedFlourish(2);
      triggerButtonPressedTime = 0;
    }
  }
  else {
    triggerButtonPressedTime = 0;
  }

  triggerButtonState = !buttonDown;

  triggered = !(triggerButtonState && triggerCvState);

  if (triggered && burstTimeStart && retriggerMode == NO_RETRIGGER) {
    triggered = LOW;
#ifdef DELAYED_TRIGGER
    triggerTime = 0;
#endif
    return;
  }

  if (triggered == LOW) {
    triggerFirstPressed = HIGH;
  }
  else if (triggerFirstPressed == LOW) {
    ; // don't update the trigger time, we're still pressed
  }
#ifdef DELAYED_TRIGGER
  else {
    triggerTime = now;
  }
#endif
}

void startBurstInit(unsigned long now)
{
  burstTimeStart = now;
  burstStarted = HIGH;
  recycle = false;

  repetitionCounter = 0;
  elapsedTimeSincePrevRepetitionOld = 0;
  burstTimeAccu = 0;

  // enable the output so that we continue to count the repetitions,
  // whether we actually write to the output pin or not
  outputState = HIGH;

  int randomDif = randomPot - random(100);
  // trigger button overrides probability
  silentBurst = (randomDif <= 0);
  wantsMoreBursts = (!triggerButtonState) ? HIGH : (silentBurst) ? LOW : wantsMoreBursts;

  byte onoff = wantsMoreBursts && !disableFirstClock;

  digitalWrite(OUT_STATE, !onoff);
  digitalWrite(OUT_LED, onoff);

  switch (distributionSign) {
  case DISTRIBUTION_SIGN_POSITIVE:
    elapsedTimeSincePrevRepetitionNew = fscale(0,
                                               clockDivided,
                                               0,
                                               clockDivided,
                                               timePortions,
                                               distribution);
    elapsedTimeSincePrevRepetition = elapsedTimeSincePrevRepetitionNew;
    break;
  case DISTRIBUTION_SIGN_NEGATIVE:
    if (repetitions > 1) {
      elapsedTimeSincePrevRepetitionOld = fscale(0,
                                                 clockDivided,
                                                 0,
                                                 clockDivided,
                                                 round(timePortions * repetitions),
                                                 distribution);
      elapsedTimeSincePrevRepetitionNew = fscale(0,
                                                 clockDivided,
                                                 0,
                                                 clockDivided,
                                                 round(timePortions * (repetitions - 1)),
                                                 distribution);
      elapsedTimeSincePrevRepetition = elapsedTimeSincePrevRepetitionOld -
                                       elapsedTimeSincePrevRepetitionNew;
    }
    else {
      elapsedTimeSincePrevRepetitionNew = timePortions;
      elapsedTimeSincePrevRepetition = timePortions;
    }
    break;
  case DISTRIBUTION_SIGN_ZERO:
    elapsedTimeSincePrevRepetitionNew = timePortions;
    elapsedTimeSincePrevRepetition = timePortions;
    break;
  }
}

void readDivision(unsigned long now)                                    //// read divisions
{
  static int divisionsPotPrev = 0;
  int divisionsPot = analogRead(CV_DIVISIONS);
  int divisionsVal = mapCalibratedAnalogValue(divisionsPot, calibratedDivisions, 0, -4, 4);
  // SERIAL_PRINTLN("divisionsPot %d", divisionsPot);

  divisionsVal = divisionsVal + (divisionsVal > 0 ? 1 : divisionsVal < 0 ? -1 : 0); // skip 2/-2

  if (abs(divisionsPot - divisionsPotPrev) > 5) {
    if (now >= ledQuantityTime + 350) {
      byte bitDivisions = divisionsVal == 0 ? 0 : divisionsVal < 0 ? -(divisionsVal) - 1 : (16 - divisionsVal) + 1;
      if (!burstTimeStart) {
        for (int i = 0; i < 4; i++) {
          digitalWrite(ledPin[i], bitRead(bitDivisions, i));
        }
        ledParameterTime = now;
      }
    }
    divisions_Temp = divisionsVal;
    divisionsPotPrev = divisionsPot;
  }
}

static int calibratedCVZero = 719;

void readRepetitions(unsigned long now)
{
  bool encoderChanged = false;

  encoderValue += encoder->getValue();
  if (encoderValue != lastEncoderValue) {
    lastEncoderValue = encoderValue;
    if (encoderValue >= 4) {
      encoderValue = 0;
      repetitionsEncoder_Temp++;
      encoderChanged = true;
    }
    else if (encoderValue <= -4) {
      encoderValue = 0;
      repetitionsEncoder_Temp--;
      encoderChanged = true;
    }
  }
  if (repetitionsEncoder_Temp < 1) repetitionsEncoder_Temp = 1;
  if (repetitionsEncoder_Temp == 1 && encoderValue < 0) {
    lastEncoderValue = encoderValue = 0; // reset to avoid weirdness around 0
  }
  // repetitionsEncoder_Temp = constrain(repetitionsEncoder_Temp, 1, MAX_REPETITIONS);
  // SERIAL_PRINTLN("repEnc %d", repetitionsEncoder_Temp);

  int cvQuantityValue = analogRead(CV_REPETITIONS);
  cvQuantityValue = mapCalibratedAnalogValue(cvQuantityValue, calibratedRepetitions, 0, 32, -16);

  int temp = (int)repetitionsEncoder_Temp + cvQuantityValue; // it could be negative, need to cast
  repetitions_Temp = constrain(temp, 1, MAX_REPETITIONS);
  // SERIAL_PRINTLN("repTmp %d", repetitions_Temp);

  repetitionsEncoder_Temp = constrain(repetitionsEncoder_Temp, 1, MAX_REPETITIONS);

  if (repetitions_Temp != repetitionsOld) {
    if (encoderChanged || !burstTimeStart) {
      for (int i = 0; i < 4; i++) {
        digitalWrite(ledPin[i], bitRead(repetitions_Temp - 1, i));
      }
      if (encoderChanged) {
        ledQuantityTime = now;
      }
    }
    repetitionsOld = repetitions_Temp;
  }
}

void readRandom(unsigned long now)
{
  int randomVal = analogRead(CV_PROBABILITY);
  randomVal = mapCalibratedAnalogValue(randomVal, calibratedProbability, 10, 0, 20);
  // SERIAL_PRINTLN("randomPot %d", randomPot);
  randomVal *= 5;

  if (randomVal != randomPot_Temp) {
    if (now >= ledQuantityTime + 350) {
      byte bitRandom = map(randomVal, 0, 100, 15, 0);
      if (!burstTimeStart) {
        for (int i = 0; i < 4; i++) {
          digitalWrite(ledPin[i], bitRead(bitRandom, i));
        }
        ledParameterTime = now;
      }
    }
    randomPot_Temp = randomVal;
  }
}

void readDistribution(unsigned long now)
{
  static int distributionPotPrev = 0;
  int distributionPot = analogRead(CV_DISTRIBUTION);
  int distributionVal = mapCalibratedAnalogValue(distributionPot, calibratedDistribution, 0, -8, 8);
  // SERIAL_PRINTLN("distributionPot %d", distributionPot);

  float dist = distributionIndexArray[abs(distributionVal)];
  byte distSign = (distributionVal < 0 ? DISTRIBUTION_SIGN_NEGATIVE :
                      distributionVal > 0 ? DISTRIBUTION_SIGN_POSITIVE :
                      DISTRIBUTION_SIGN_ZERO);

  if (abs(distributionPot - distributionPotPrev) > 5 || dist != distribution_Temp) {
    if (now >= ledQuantityTime + 350) {
      byte bitDistribution = (distributionVal < 0) ? -(distributionVal) :
                             (distributionVal > 0) ? (16 - distributionVal) : 0;

      if (!burstTimeStart) {
        for (int i = 0; i < 4; i++) {
          digitalWrite(ledPin[i], bitRead(bitDistribution, i));
        }
        ledParameterTime = now;
      }
    }

    distribution_Temp = dist;
    distributionSign_Temp = distSign;

    distributionPotPrev = distributionPot;
  }
}

void readCycle()
{
  int buttonDown = !bounce.read();

  int cSwitchState = digitalRead(CYCLE_SWITCH);
  cycleInState = digitalRead(CYCLE_STATE);

  if (!cSwitchState && cycleSwitchState && buttonDown) {
    resetPhase = true;
    doLedFlourish(2);
  }
  cycleSwitchState = cSwitchState;

  if (cycleSwitchState == HIGH) {
    cycle = !cycleInState;
  }
  else {
    cycle = cycleInState;
  }
}

int32_t fscale(int32_t originalMin,
               int32_t originalMax,
               int32_t newBegin,
               int32_t newEnd,
               int32_t inputValue,
               float curve)
{
  int32_t originalRange = 0;
  int32_t newRange = 0;
  int32_t zeroRefCurVal = 0;
  float normalizedCurVal = 0;
  int32_t rangedValue = 0;
  boolean invFlag = 0;

  // Check for out of range inputValues
  if (inputValue < originalMin) {
    inputValue = originalMin;
  }
  if (inputValue > originalMax) {
    inputValue = originalMax;
  }

  // Zero Refference the values
  originalRange = originalMax - originalMin;

  if (newEnd > newBegin) {
    newRange = newEnd - newBegin;
  }
  else {
    newRange = newBegin - newEnd;
    invFlag = 1;
  }

  zeroRefCurVal = inputValue - originalMin;
  normalizedCurVal = ((float)zeroRefCurVal / (float)originalRange);   // normalize to 0 - 1 float

  // Check for originalMin > originalMax  - the math for all other cases i.e. negative numbers seems to work out fine
  if (originalMin > originalMax) {
    return 0;
  }

  if (invFlag == 0) {
    rangedValue =  (int32_t)((pow(normalizedCurVal, curve) * newRange) + newBegin);
  }
  else { // invert the ranges
    rangedValue =  (int32_t)(newBegin - (pow(normalizedCurVal, curve) * newRange));
  }

  return rangedValue;
}

void createDistributionIndex()
{
  for (int i  = 0; i <= 8; i++) {
    // invert and scale in advance: positive numbers give more weight to high end on output
    // convert linear scale into logarithmic exponent for pow function in fscale()
    float val = mapfloat(i, 0, 8, 0, curve);
    val = val > 10. ? 10. : val < -10. ? -10. : val;
    val *= -0.1;
    val = pow(10, val);
    distributionIndexArray[i] = val;
  }
}

float mapfloat(float x, float inMin, float inMax, float outMin, float outMax)
{
  return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

void handleLEDs(unsigned long now)
{
  if ((now >= ledQuantityTime + 350)) {
    if ((wantsMoreBursts && !silentBurst) || (now >= ledParameterTime + 750)) {
      for (int i = 0; i < 4; i++) {
        digitalWrite(ledPin[i], bitRead((burstStarted && !silentBurst) ? repetitionCounter : repetitions_Temp - 1, i));
      }
    }
  }
}

void handlePulseDown(unsigned long now)
{
  if ((outputState == HIGH) && (burstStarted == HIGH)) {
    if (now >= (burstTimeStart + burstTimeAccu + trigLen)) {
      outputState = LOW;
      digitalWrite(OUT_STATE, HIGH);
      digitalWrite(OUT_LED, LOW);
    }
  }
}

void enableEOC(unsigned long now)
{
  if (inEoc) {
    eocCounter = now;
    handleEOC(now, 0); // turn off the EOC if necessary
  }

  if (!(silentBurst && probabilityAffectsEOC)) {
    // turn it (back) on
    digitalWrite(EOC_STATE, LOW);
    inEoc = true;
    eocCounter = now;
  }
  wantsEoc = false;
}

void handleEOC(unsigned long now, int width)
{
  if (inEoc && now >= eocCounter + width) {
    digitalWrite(EOC_STATE, HIGH);
    inEoc = false;
  }
}

void handlePulseUp(unsigned long now)
{
  int inputValue;

  // pulse up - burst time
  if ((outputState == LOW) && (burstStarted == HIGH)) {
    if (now >= (burstTimeStart + elapsedTimeSincePrevRepetition + burstTimeAccu)) { // time for a repetition
      if (repetitionCounter < repetitions - 1) { // is it the last repetition?
        outputState = HIGH;
        digitalWrite(OUT_STATE, !wantsMoreBursts);
        digitalWrite(OUT_LED, wantsMoreBursts);
        burstTimeAccu += elapsedTimeSincePrevRepetition;

        repetitionCounter++;

        switch (distributionSign) {
        case DISTRIBUTION_SIGN_POSITIVE:
          inputValue = round(timePortions * (repetitionCounter + 1));
          elapsedTimeSincePrevRepetitionOld = elapsedTimeSincePrevRepetitionNew;
          elapsedTimeSincePrevRepetitionNew = fscale(0,
                                                     clockDivided,
                                                     0,
                                                     clockDivided,
                                                     inputValue,
                                                     distribution);
          elapsedTimeSincePrevRepetition = elapsedTimeSincePrevRepetitionNew -
                                           elapsedTimeSincePrevRepetitionOld;
          break;
        case DISTRIBUTION_SIGN_NEGATIVE:
          elapsedTimeSincePrevRepetitionOld = elapsedTimeSincePrevRepetitionNew;

          inputValue = round(timePortions * ((repetitions - 1) - repetitionCounter));
          if (!inputValue && divisions <= 0) {   // no EOC if we're dividing
            wantsEoc = true;   // we may not reach the else block below if the next trigger comes too quickly
          }
          elapsedTimeSincePrevRepetitionNew = fscale(0,
                                                     clockDivided,
                                                     0,
                                                     clockDivided,
                                                     inputValue,
                                                     distribution);
          elapsedTimeSincePrevRepetition = elapsedTimeSincePrevRepetitionOld -
                                           elapsedTimeSincePrevRepetitionNew;
          break;
        case DISTRIBUTION_SIGN_ZERO:
          elapsedTimeSincePrevRepetitionOld = elapsedTimeSincePrevRepetitionNew;
          elapsedTimeSincePrevRepetitionNew = round(timePortions * (repetitionCounter + 1));
          elapsedTimeSincePrevRepetition = elapsedTimeSincePrevRepetitionNew -
                                           elapsedTimeSincePrevRepetitionOld;
          break;
        }
      }
      else { // it's the end of the burst, but not necessarily the end of a set of bursts (mult)
        enableEOC(now);

        wantsMoreBursts = HIGH;
        burstStarted = LOW;

        readCycle();

        if (cycle) {
          recycle = true;
          divisionCounter++;
          if (divisions >= 0 || divisionCounter >= -(divisions)) {
            resync = true;
            divisionCounter = 0; // superstitious, this will also be reset in doResync()
          }
        }
        else {
          wantsMoreBursts = LOW;
          burstTimeStart = 0;
          firstBurstTime = 0;
        }
      }
    }
  }
}

void handleTempo(unsigned long now)
{
  static unsigned long tempoStart = 0;

  // using a different variable here (tempoTimer) to avoid side effects
  // when updating tempoTic. Now the tempoTimer is entirely independent
  // of the cycling, etc.

  if (!tempoStart && (now >= tempoTimer) && (now < tempoTimer + trigLen)) {
    digitalWrite(TEMPO_STATE, LOW);
    tempoStart = now;
  }
  else if (tempoStart && (now - tempoStart) > trigLen) {
    digitalWrite(TEMPO_STATE, HIGH);
    tempoStart = 0;
  }

  if (!tempoStart) { // only if we're outside of the pulse
    if (now >= tempoTimer) {
      tempoTimer = tempoTimer_Temp;
    }
    while (now >= tempoTimer) {
      tempoTimer += masterClock;
    }
  }
}

void doResync(unsigned long now)
{
  // get the new value in advance of calctimeportions();
  divisions = divisions_Temp;
  calcTimePortions();

  // don't advance the timer unless we've reached the next tick
  // (could happen with manual trigger)
  if (now >= tempoTic_Temp) {
    tempoTic = tempoTic_Temp;
    if (masterClock_Temp != masterClock) {
      masterClock = masterClock_Temp;
    }
    tempoTic_Temp = tempoTic + masterClock;
    tempoTimer_Temp = tempoTic_Temp;
  }

  // update other params
  repetitions = repetitions_Temp;
  clockDivided = clockDivided_Temp;
  timePortions = timePortions_Temp;
  distribution = distribution_Temp;
  distributionSign = distributionSign_Temp;
  randomPot = randomPot_Temp;

  if (timePortions < TRIGGER_LENGTH) {
    trigLen = (int)timePortions - 1;
    if (trigLen < 1) trigLen = 1;
  }
  else {
    trigLen = TRIGGER_LENGTH;
  }

  readCycle();
  resync = false;
  divisionCounter = 0; // otherwise multiple manual bursts will screw up the division count

  // time of first burst of a group of bursts
  firstBurstTime = now;
}

void calcTimePortions()
{
  if (masterClock_Temp < 1) masterClock_Temp = 1; // ensure masterClock > 0

  if (divisions > 0) {
    clockDivided_Temp = masterClock_Temp * divisions;
  }
  else if (divisions < 0) {
    clockDivided_Temp = masterClock_Temp / (-divisions);
  }
  else if (divisions == 0) {
    clockDivided_Temp = masterClock_Temp;
  }
  timePortions_Temp = (float)clockDivided_Temp / (float)repetitions_Temp;
}
