#define TAP_TEMPO_AVG_COUNT (2)

unsigned long encoderLastTime = 0;
unsigned long encoderTaps[TAP_TEMPO_AVG_COUNT] = {0,0};
unsigned long encoderDuration = 0;
byte encoderTapsCurrent = 0;
byte encoderTapsTotal = 0; // 2 means use the duration between encoderTaps[0] and encoderTaps[1]; 3 means average of [0-2]

unsigned long pingLastTime = 0;
unsigned long pingDuration = 0;

void calculateClock(unsigned long now)
{
  ////  Read the encoder button state
  if (digitalRead(PING_BUTTON) == 0) {
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
  else if ((triggered == HIGH)) {
    calcTimePortions();
  }

  // TODO: hold encoder for 2s should stop the ping, like the PEG

  if ((encoderButtonState == 2) || (pingInState == 2)) {
    bitWrite(encoderButtonState, 1, 0);
    bitWrite(pingInState, 1, 0);
  }
}

void readTrigger()
{
  bool triggerCvState = digitalRead(TRIGGER_STATE);
  TriggerButtonState = digitalRead(TRIGGER_BUTTON);
  triggered = !(TriggerButtonState && triggerCvState);
  if (triggered == LOW) {
    triggerFirstPressed = HIGH;
  }
}

void startBurstInit(unsigned long now)
{
  burstTimeStart = now;
  digitalWrite (OUT_STATE, LOW);
  digitalWrite (OUT_LED, LOW);

  burstStarted = true;
  repetitionCounter = 0;

  elapsedTimeSincePrevRepetitionOld = 0;
  burstTimeAccu = 0;
  outputState = HIGH;
  firstBurst = HIGH;
  recycle = false;

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
                                                 timePortions * repetitions,
                                                 distribution);
      elapsedTimeSincePrevRepetitionNew = fscale(0,
                                                 clockDivided,
                                                 0,
                                                 clockDivided,
                                                 timePortions * (repetitions - 1),
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

void readDivision()                                    //// read divisions
{
  int divisionsPot = analogRead(CV_DIVISIONS);

  switch (divisionsPot) {
  case 0 ... 105:
    divisions = -5;
    break;
  case 121 ... 219:
    divisions = -4;
    break;
  case 235 ... 333:
    divisions = -3;
    break;
  case 349 ... 446:
    divisions = -2;
    break;
  case 462 ... 560:
    divisions = 0;
    break;
  case 576 ... 674:
    divisions = 2;
    break;
  case 690 ... 787:
    divisions = 3;
    break;
  case 803 ... 901:
    divisions = 4;
    break;
  case 917 ... 1023:
    divisions = 5;
    break;
  }
  // SERIAL_PRINTLN("divisionsPot %d", divisionsPot);
}

void readRepetitions(unsigned long now)
{
  encoderValue += encoder->getValue();
  if (encoderValue != lastEncoderValue) {
    lastEncoderValue = encoderValue;
    if (encoderValue >= 4) {
      encoderValue = 0;
      repetitionsEncoder_Temp++;
      if (repetitionsEncoder_Temp > MAX_REPETITIONS) {
        repetitionsEncoder_Temp = MAX_REPETITIONS;
      }
      //  calculateDistribution = HIGH;
    }
    if (encoderValue <= -4) {
      encoderValue = 0;
      repetitionsEncoder_Temp--;
      if (repetitionsEncoder_Temp < 1) {
        repetitionsEncoder_Temp = 1;
      }
      //calculateDistribution = HIGH;
    }
  }

  int cvQuantityValue = analogRead(CV_QUANTITY);
  cvQuantityValue = map (cvQuantityValue, 0, 1023, 15, -16); // 32 steps
  int temp = (int)repetitionsEncoder_Temp + cvQuantityValue; // it could be negative, need to cast
  repetitions_Temp = constrain(temp, 1, MAX_REPETITIONS);

  if (repetitions_Temp != repetitionsOld) {
    for (int i = 0; i < 4; i++) {
      digitalWrite(ledPin[i], bitRead(repetitions_Temp - 1, i));
    }
    ledQuantityTime = now;
    repetitionsOld = repetitions_Temp;

    //repetitionCounter = repetitions_Temp - 1 ;
  }
}

void readRandom()
{
  randomPot = analogRead(CV_PROBABILITY);
  // SERIAL_PRINTLN("randomPot %d", randomPot);
}

void readDistribution()
{
  int distributionPot = analogRead(CV_DISTRIBUTION);

  // SERIAL_PRINTLN("distributionPot %d", distributionPot);
  switch (distributionPot) {
  case -1 ... 56:
    distribution = distributionIndexArray [8];
    distributionSign = DISTRIBUTION_SIGN_NEGATIVE;
    break;
  case 57 ... 113:
    distribution = distributionIndexArray [7];
    distributionSign = DISTRIBUTION_SIGN_NEGATIVE;
    break;
  case 114 ... 170:
    distribution = distributionIndexArray [6];
    distributionSign = DISTRIBUTION_SIGN_NEGATIVE;
    break;
  case 171 ... 227:
    distribution = distributionIndexArray [5];
    distributionSign = DISTRIBUTION_SIGN_NEGATIVE;
    break;
  case 228 ... 284:
    distribution = distributionIndexArray [4];
    distributionSign = DISTRIBUTION_SIGN_NEGATIVE;
    break;
  case 285 ... 341:
    distribution = distributionIndexArray [3];
    distributionSign = DISTRIBUTION_SIGN_NEGATIVE;
    break;
  case 342 ... 398:
    distribution = distributionIndexArray [2];
    distributionSign = DISTRIBUTION_SIGN_NEGATIVE;
    break;
  case 399 ... 455:
    distribution = distributionIndexArray [1];
    distributionSign = DISTRIBUTION_SIGN_NEGATIVE;
    break;
  case 456 ... 568:   // this was too narrow and easy to miss, adjusted all the others to be a little more narrow, and this is ~twice as wide
    distribution = distributionIndexArray [0];
    distributionSign = DISTRIBUTION_SIGN_ZERO;
    break;
  case 569 ... 625:
    distribution = distributionIndexArray [1];
    distributionSign = DISTRIBUTION_SIGN_POSITIVE;
    break;
  case 626 ... 682:
    distribution = distributionIndexArray [2];
    distributionSign = DISTRIBUTION_SIGN_POSITIVE;
    break;
  case 683 ... 739:
    distribution = distributionIndexArray [3];
    distributionSign = DISTRIBUTION_SIGN_POSITIVE;
    break;
  case 740 ... 796:
    distribution = distributionIndexArray [4];
    distributionSign = DISTRIBUTION_SIGN_POSITIVE;
    break;
  case 797 ... 853:
    distribution = distributionIndexArray [5];
    distributionSign = DISTRIBUTION_SIGN_POSITIVE;
    break;
  case 854 ... 910:
    distribution = distributionIndexArray [6];
    distributionSign = DISTRIBUTION_SIGN_POSITIVE;
    break;
  case 911 ... 967:
    distribution = distributionIndexArray [7];
    distributionSign = DISTRIBUTION_SIGN_POSITIVE;
    break;
  case 968 ... 1024:
    distribution = distributionIndexArray [8];
    distributionSign = DISTRIBUTION_SIGN_POSITIVE;
    break;
  }
}

void readCycle()
{
  cycleSwitchState = digitalRead(CYCLE_SWITCH);
  cycleInState = digitalRead(CYCLE_STATE);
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

void createDistributionIndex ()
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
  if ((now >= ledQuantityTime + 250) && (noMoreBursts)) {
    for (int i = 0; i < 4; i++) {
      digitalWrite(ledPin[i], bitRead(repetitionCounter, i));
    }
  }
}

void handlePulseDown(unsigned long now)
{
  if ((outputState == HIGH) && (burstStarted == true)) {
    if (now >= (burstTimeStart + burstTimeAccu + 2)) {
      outputState = !outputState;
      digitalWrite(OUT_STATE, !(outputState * noMoreBursts));
      digitalWrite(OUT_LED, (outputState * noMoreBursts));
      randomDif = randomPot - random(1023);
      if ((firstBurst == HIGH) && (randomDif <= 0) && TriggerButtonState) {
        noMoreBursts = false;
      }
      firstBurst = LOW;
      burstTimeStart = burstTimeStart;
    }
  }
}

void handleTempo(unsigned long now)
{
  static bool inTempo = false;

  // SERIAL_PRINTLN("now %lu tempoTic %lu inTempo %d", now, tempoTic, inTempo);
  if ((now >= tempoTic) && (now < tempoTic + 100)) {
    if (!inTempo) {
      digitalWrite(TEMPO_LED, HIGH);
    }
    inTempo = true;
  }
  else if (inTempo) {
    digitalWrite(TEMPO_LED, LOW);
    inTempo = false;
  }
}

void enableEOC(unsigned long now)
{
  if (inEoc) {
    eocCounter = now;
    handleEOC(now, 0); // turn off the EOC if necessary
  }
  // turn it (back) on
#if !EOC_LED_IS_TEMPO
  digitalWrite(EOC_LED, HIGH);
#endif
  digitalWrite(EOC_STATE, LOW);
  inEoc = true;
  wantsEoc = false;
  eocCounter = now;
}

void handleEOC(unsigned long now, int width)
{
  if (inEoc && now >= eocCounter + width) {
#if !EOC_LED_IS_TEMPO
    digitalWrite(EOC_LED, LOW);
#endif
    digitalWrite(EOC_STATE, HIGH);
    inEoc = false;
  }
}

void handlePulseUp(unsigned long now, bool inCycle)
{
  int inputValue;

  // pulse up - burst time
  if ((outputState == LOW) && (burstStarted == true)) {
    if (now >= (burstTimeStart + elapsedTimeSincePrevRepetition + burstTimeAccu)) { // time for a repetition
      if (repetitionCounter < repetitions - 1) { // is it the last repetition?
        outputState = !outputState;
        digitalWrite(OUT_STATE, !(outputState * noMoreBursts));
        digitalWrite(OUT_LED, (outputState * noMoreBursts));
        burstTimeAccu += elapsedTimeSincePrevRepetition;

        repetitionCounter++;

        switch (distributionSign) {
        case DISTRIBUTION_SIGN_POSITIVE:
          inputValue = timePortions * (repetitionCounter + 1);
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

          inputValue = timePortions * ((repetitions - 1) - repetitionCounter);
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
          elapsedTimeSincePrevRepetitionNew = timePortions * (repetitionCounter + 1);
          elapsedTimeSincePrevRepetition = elapsedTimeSincePrevRepetitionNew -
                                           elapsedTimeSincePrevRepetitionOld;
          break;
        }
      }
      else { // it's the end of the burst, but not necessarily the end of a set of bursts (mult)
        enableEOC(now);

        noMoreBursts = true;
        burstStarted = false;

        readCycle();

        if (inCycle) {
          recycle = true;
          divisionCounter++;
          if (divisions >= 0 || divisionCounter >= -(divisions)) {
            resync = true;
            divisionCounter = 0;
          }
        }

        for (int i = 0; i < 4; i++) {
          digitalWrite(ledPin[i], bitRead(repetitions - 1, i));
        }
      }
    }
  }
}

void doResync(unsigned long now)
{
  // we are at the start of a new burst
  readDivision(); // get the new value in advance of calctimeportions();
  calcTimePortions();

  // try to mantain proportional difference between ping and trigger
  // with external clock and cycle, and the clock changes
  if (masterClock_Temp != masterClock) {
    triggerDifference = (float)masterClock_Temp / triggerDifProportional;
    masterClock = masterClock_Temp;
  }

  repetitions = repetitions_Temp;
  clockDivided = clockDivided_Temp;
  timePortions = timePortions_Temp;

  // ensure that the clock advances
  while (tempoTic_Temp + triggerDifference <= now) {
    tempoTic_Temp += masterClock;
  }
  tempoTic = tempoTic_Temp;

  readRandom();
  readDistribution();
  readCycle();
  resync = false;
}

void calcTimePortions()
{
  if (divisions > 0) {
    clockDivided_Temp = masterClock_Temp * divisions;
  }
  else if (divisions < 0) {
    clockDivided_Temp = masterClock_Temp / (-divisions);
  }
  else if (divisions == 0) {
    clockDivided_Temp = masterClock_Temp;
  }
  timePortions_Temp = clockDivided_Temp / repetitions_Temp;
}
