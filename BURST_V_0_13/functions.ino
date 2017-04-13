void calculate_clock() {

  ////  Read the encoder button state


  if (digitalRead(PING_BUTTON) == 0 ) {
    bitWrite(encoder_button_state, 0, 1);
  }
  else {
    bitWrite(encoder_button_state, 0, 0);
  }
  /// Read the ping input state
  if (digitalRead(PING_STATE) == 0 ) {
    bitWrite(ping_in_state, 0, 1);
  }
  else {
    bitWrite(ping_in_state, 0, 0);
  }


  /// if ping or tap button have raised
  if ((encoder_button_state == 1) ||  (ping_in_state == 1)) {
    tempo_tic_temp = millis();
    tap_tempo_array [tap_index] = tempo_tic_temp - old_tempo_tic;
    averaged_taps = 0;
    for (int taps = 0 ; taps <= max_taps ; taps ++) {
      averaged_taps = averaged_taps + tap_tempo_array [taps];
    }
    master_clock_temp = averaged_taps / (max_taps + 1);
    max_taps ++;
    if (max_taps > 3) max_taps = 3;
    tap_index ++;
    if (tap_index > 3) tap_index = 0;
    old_tempo_tic = tempo_tic_temp;

    calcTimePortions();

    if (encoder_button_state == 1) {
      bitWrite(encoder_button_state, 1, 1);
      EEPROM.write(0, master_clock_temp & 0xFF);
      EEPROM.write(1, (master_clock_temp >> 8) & 0xFF);
      EEPROM.write(2, (master_clock_temp >> 16) & 0xFF);
      EEPROM.write(3, (master_clock_temp >> 24) & 0xFF);
    }

    if (ping_in_state == 1) bitWrite(ping_in_state, 1, 1);

  }
  if ((encoder_button_state == 2) || (ping_in_state == 2)) {
    bitWrite(encoder_button_state, 1, 0);
    bitWrite(ping_in_state, 1, 0);
  }
}


void read_trigger() {
  bool trigger_cv_state = digitalRead(TRIGGER_STATE);
  trigger_button_state = digitalRead(TRIGGER_BUTTON);
  triggered = !(trigger_button_state && trigger_cv_state);
  if (triggered == LOW) trigger_first_pressed = HIGH;
}

void start_burst_init() {
  burst_time_start = millis();
  digitalWrite (OUT_STATE, LOW);
  digitalWrite (OUT_LED, LOW);

  burst_started = HIGH;
  repetition_counter = 0;

  // sub_division_counter = 0;

  elapsed_time_since_prev_repetition_old = 0;
  burst_time_accu = 0;
  output_state = HIGH;
  first_burst = HIGH;

  switch (distribution_sign) {
    case DISTRIBUTION_SIGN_POSITIVE:
      elapsed_time_since_prev_repetition_new = fscale(0, clock_divided, 0, clock_divided, time_portions, distribution);
      elapsed_time_since_prev_repetition = elapsed_time_since_prev_repetition_new;
      break;
    case DISTRIBUTION_SIGN_NEGATIVE:
      if (repetitions > 1) {
        elapsed_time_since_prev_repetition_old = fscale(0, clock_divided, 0, clock_divided, time_portions * (repetitions - 1), distribution);
        elapsed_time_since_prev_repetition_new = fscale(0, clock_divided, 0, clock_divided, time_portions * (repetitions - 2), distribution);
        elapsed_time_since_prev_repetition = elapsed_time_since_prev_repetition_old - elapsed_time_since_prev_repetition_new;
      }
      else {
        elapsed_time_since_prev_repetition_new = time_portions;
        elapsed_time_since_prev_repetition = time_portions;
      }
      break;
    case DISTRIBUTION_SIGN_ZERO:
      elapsed_time_since_prev_repetition_new = time_portions;
      elapsed_time_since_prev_repetition = time_portions;
      break;
  }
}

void read_division() {                                  //// READ DIVISIONS
  divisions_pot = analogRead (A0);

  switch (divisions_pot)
  {
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

  if (divisions != divisions_old) {
    // calculate_distribution = HIGH;
    divisions_old = divisions;

  }
}


void read_repetitions() {

  encoder_value += encoder->getValue();
  if (encoder_value != last_encoder_value) {
    last_encoder_value = encoder_value;
    if (encoder_value >= 4) {
      encoder_value = 0;
      repetitions_encoder_temp++;
      if (repetitions_encoder_temp > MAX_REPETITIONS) repetitions_encoder_temp = MAX_REPETITIONS;
      //  calculate_distribution = HIGH;
    }
    if (encoder_value <= -4) {
      encoder_value = 0;
      repetitions_encoder_temp--;
      if (repetitions_encoder_temp < 1) repetitions_encoder_temp = 1;
      //calculate_distribution = HIGH;
    }
  }

  int cv_quantity_value = analogRead(CV_QUANTITY);
  cv_quantity_value = map (cv_quantity_value, 0, 1000, 16, -16);
  repetitions_temp = repetitions_encoder_temp + cv_quantity_value;
  if (repetitions_temp > MAX_REPETITIONS) repetitions_temp = MAX_REPETITIONS;
  if (repetitions_temp < 1) repetitions_temp = 1;

  if (repetitions_temp != repetitions_old) {
    for (int i = 0 ; i < 4 ; i++) {
      digitalWrite(led_pin[i], bitRead(repetitions_temp - 1, i));
    }
    led_quantity_time = millis();
    repetitions_old = repetitions_temp;

    //repetition_counter = repetitions_temp - 1 ;
  }
}

void read_random() {
  random_pot = analogRead (CV_PROBABILITY);
}


void read_distribution() {
  distribution_pot = analogRead (CV_DISTRIBUTION);
  switch (distribution_pot)
  {
    case -1 ... 56:
      distribution = distribution_index_array [8];
      distribution_sign = DISTRIBUTION_SIGN_NEGATIVE;
      break;
    case 57 ... 113:
      distribution = distribution_index_array [7];
      distribution_sign = DISTRIBUTION_SIGN_NEGATIVE;
      break;
    case 114 ... 170:
      distribution = distribution_index_array [6];
      distribution_sign = DISTRIBUTION_SIGN_NEGATIVE;
      break;
    case 171 ... 227:
      distribution = distribution_index_array [5];
      distribution_sign = DISTRIBUTION_SIGN_NEGATIVE;
      break;
    case 228 ... 284:
      distribution = distribution_index_array [4];
      distribution_sign = DISTRIBUTION_SIGN_NEGATIVE;
      break;
    case 285 ... 341:
      distribution = distribution_index_array [3];
      distribution_sign = DISTRIBUTION_SIGN_NEGATIVE;
      break;
    case 342 ... 398:
      distribution = distribution_index_array [2];
      distribution_sign = DISTRIBUTION_SIGN_NEGATIVE;
      break;
    case 399 ... 455:
      distribution = distribution_index_array [1];
      distribution_sign = DISTRIBUTION_SIGN_NEGATIVE;
      break;
    case 456 ... 568: // this was too narrow and easy to miss, adjusted all the others to be a little more narrow, and this is ~twice as wide
      distribution = distribution_index_array [0];
      distribution_sign = DISTRIBUTION_SIGN_ZERO;
      break;
    case 569 ... 625:
      distribution = distribution_index_array [1];
      distribution_sign = DISTRIBUTION_SIGN_POSITIVE;
      break;
    case 626 ... 682:
      distribution = distribution_index_array [2];
      distribution_sign = DISTRIBUTION_SIGN_POSITIVE;
      break;
    case 683 ... 739:
      distribution = distribution_index_array [3];
      distribution_sign = DISTRIBUTION_SIGN_POSITIVE;
      break;
    case 740 ... 796:
      distribution = distribution_index_array [4];
      distribution_sign = DISTRIBUTION_SIGN_POSITIVE;
      break;
    case 797 ... 853:
      distribution = distribution_index_array [5];
      distribution_sign = DISTRIBUTION_SIGN_POSITIVE;
      break;
    case 854 ... 910:
      distribution = distribution_index_array [6];
      distribution_sign = DISTRIBUTION_SIGN_POSITIVE;
      break;
    case 911 ... 967:
      distribution = distribution_index_array [7];
      distribution_sign = DISTRIBUTION_SIGN_POSITIVE;
      break;
    case 968 ... 1024:
      distribution = distribution_index_array [8];
      distribution_sign = DISTRIBUTION_SIGN_POSITIVE;
      break;
  }
}

void read_cycle() {
  cycle_switch_state = digitalRead(CYCLE_SWITCH);
  cycle_in_state = digitalRead(CYCLE_STATE);
  if (cycle_switch_state == HIGH) {
    cycle = !cycle_in_state;
  }
  else {
    cycle = cycle_in_state;
  }
}

int32_t fscale(int32_t originalMin, int32_t originalMax, int32_t newBegin, int32_t newEnd, int32_t inputValue, float curve) {

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
  else
  {
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

void create_distribution_index () {
  for ( int i  = 0 ; i <= 8 ; i++) {
    // invert and scale in advance: positive numbers give more weight to high end on output
    // convert linear scale into logarithmic exponent for pow function in fscale()
    float val = mapfloat(i, 0, 8, 0, curve);
    val = val > 10. ? 10. : val < -10. ? -10. : val;
    val *= -0.1;
    val = pow(10, val);
    distribution_index_array[i] = val;
  }
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void handleLEDs(unsigned long current_time) {
    if ((current_time >= led_quantity_time + 250) && (no_more_bursts)) {
    for (int i = 0 ; i < 4 ; i++) {
      digitalWrite(led_pin[i], bitRead(repetition_counter, i));
    }
  }
}

void handlePulseDown(unsigned long current_time) {
  if ((output_state == HIGH) && (burst_started == HIGH)) {
    if (current_time >= (burst_time_start + burst_time_accu + 2)) {
      output_state = !output_state;
      digitalWrite(OUT_STATE, !(output_state * no_more_bursts));
      digitalWrite(OUT_LED, (output_state * no_more_bursts));
      random_dif = random_pot - random(1023);
      if ((first_burst == HIGH) && (random_dif <= 0) && trigger_button_state) {
        no_more_bursts = LOW;
      }
      first_burst = LOW;
    }
  }
}

void handleEOC(unsigned long current_time, int width) {
  if (current_time >= eoc_counter + width) {
    digitalWrite(EOC_LED, LOW);
    digitalWrite(EOC_STATE, HIGH);
  }
}

void handlePulseUp(unsigned long current_time, bool cycle) {
  int positionTemp = 0;
  int inputValue;

  // pulse up - burst time
  if ((output_state == LOW) && (burst_started == HIGH)) {
    if (current_time >= (burst_time_start + elapsed_time_since_prev_repetition + burst_time_accu)) { // time for a repetition

      if (repetition_counter < repetitions - 1) { // is it the last repetition?
        output_state = !output_state;
        digitalWrite(OUT_STATE, !(output_state * no_more_bursts));
        digitalWrite(OUT_LED, (output_state * no_more_bursts));
        burst_time_accu += elapsed_time_since_prev_repetition;
        repetition_counter++;

        switch (distribution_sign) {
          case DISTRIBUTION_SIGN_POSITIVE:
            inputValue = time_portions * (repetition_counter + 1);
            elapsed_time_since_prev_repetition_old = elapsed_time_since_prev_repetition_new;
            elapsed_time_since_prev_repetition_new = fscale(0, clock_divided, 0, clock_divided, inputValue, distribution);
            elapsed_time_since_prev_repetition = elapsed_time_since_prev_repetition_new - elapsed_time_since_prev_repetition_old;
            break;
          case DISTRIBUTION_SIGN_NEGATIVE:
            elapsed_time_since_prev_repetition_old = elapsed_time_since_prev_repetition_new;

            positionTemp = repetitions - repetition_counter - 2;
            if (positionTemp < 0) {
              inputValue = 0;
            }
            else if (positionTemp > 0) {
              inputValue = time_portions * positionTemp;
            }
            else {
              ; // won't be used
            }
            if (positionTemp != 0) {
              elapsed_time_since_prev_repetition_new = fscale(0, clock_divided, 0, clock_divided, inputValue, distribution);
              elapsed_time_since_prev_repetition = elapsed_time_since_prev_repetition_old - elapsed_time_since_prev_repetition_new;
            }
            break;
          case DISTRIBUTION_SIGN_ZERO:
            elapsed_time_since_prev_repetition_old = elapsed_time_since_prev_repetition_new;
            elapsed_time_since_prev_repetition_new = time_portions *  (repetition_counter + 1);
            elapsed_time_since_prev_repetition = elapsed_time_since_prev_repetition_new - elapsed_time_since_prev_repetition_old;
            break;
        }
      }
      else { // it's the end of the burst
        digitalWrite(EOC_LED, HIGH);
        digitalWrite(EOC_STATE, LOW);
        eoc_counter = current_time;
        no_more_bursts = HIGH;
        burst_started = LOW;

        if (cycle == HIGH) {
          resync = HIGH;
          // WE ADJUST THE VALUE OF TEMPO_TIC (FOR THE RESYNC) DEPENDING ON TIME_DIVISIONS AND THE AMOUNT OF SUB-BURSTS DONE
          sub_division_counter++;
          if (divisions <= 0 && sub_division_counter >= -(divisions)) {
              tempo_tic = tempo_tic_temp;
              resync = LOW;
          }
          read_cycle();
          /// try to mantain proportional difference between ping and trigger, with external clock and cycle, and the clock changes

          if (master_clock_temp != master_clock) {
            trigger_difference = (float)master_clock_temp / trigger_dif_proportional;
          }
        }

        for (int i = 0 ; i < 4 ; i++) {
          digitalWrite(led_pin[i], bitRead(repetitions - 1, i));
        }
      }
    }
  }
}

void doResync() {
  repetitions = repetitions_temp;
  clock_divided = clock_divided_temp;
  master_clock = master_clock_temp;
  time_portions = time_portions_temp;
  read_division();
  read_random();
  read_distribution();
  read_cycle();
  start_burst_init();
  resync = LOW;
}

void calcTimePortions() {
  if (divisions > 0) {
    clock_divided_temp = master_clock_temp * divisions;
  }
  else if (divisions < 0) {
    clock_divided_temp = master_clock_temp / (-divisions);
  }
  else if (divisions == 0) {
    clock_divided_temp = master_clock_temp;
  }
  time_portions_temp = clock_divided_temp / repetitions_temp;
}
