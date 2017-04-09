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

    if ( divisions > 0) {
      clock_divided_temp = master_clock_temp * divisions;
    }
    if ( divisions < 0) {
      clock_divided_temp = master_clock_temp / (-divisions);
    }
    if ( divisions == 0) {
      clock_divided_temp = master_clock_temp;
    }
    time_portions_temp = clock_divided_temp / repetitions_temp;
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

  //sub_division_counter = 0;

  elapsed_time_since_prev_repetition_old = 0;
  burst_time_accu = 0;
  output_state = HIGH;
  first_burst = HIGH;

  switch ( distribution_sign ) { /// 0 negative , 1 positive, 2 zero
    case 1:
      elapsed_time_since_prev_repetition_new = fscale( 0, clock_divided, 0, clock_divided, time_portions, distribution);
      elapsed_time_since_prev_repetition = elapsed_time_since_prev_repetition_new;
      break;
    case 0:
      elapsed_time_since_prev_repetition_old = fscale( 0, clock_divided, 0, clock_divided, time_portions * (repetitions - 1), distribution);
      elapsed_time_since_prev_repetition_new = fscale( 0, clock_divided, 0, clock_divided, time_portions * (repetitions - 2), distribution);
      elapsed_time_since_prev_repetition = elapsed_time_since_prev_repetition_old - elapsed_time_since_prev_repetition_new;
      break;
    case 2:
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
    case -1 ... 60:
      distribution = distribution_index_array [8];
      distribution_sign = 0;
      break;
    case 61 ... 120:
      distribution = distribution_index_array [7];
      distribution_sign = 0;
      break;
    case 121 ... 180:
      distribution = distribution_index_array [6];
      distribution_sign = 0;
      break;
    case 181 ... 240:
      distribution = distribution_index_array [5];
      distribution_sign = 0;
      break;
    case 241 ... 300:
      distribution = distribution_index_array [4];
      distribution_sign = 0;
      break;
    case 301 ... 361:
      distribution = distribution_index_array [3];
      distribution_sign = 0;
      break;
    case 362 ... 421:
      distribution = distribution_index_array [2];
      distribution_sign = 0;
      break;
    case 422 ... 481:
      distribution = distribution_index_array [1];
      distribution_sign = 0;
      break;
    case 482 ... 541:
      distribution = distribution_index_array [0];
      distribution_sign = 2;
      break;
    case 542 ... 601:
      distribution = distribution_index_array [1];
      distribution_sign = 1;
      break;
    case 602 ... 661:
      distribution = distribution_index_array [2];
      distribution_sign = 1;
      break;
    case 662 ... 722:
      distribution = distribution_index_array [3];
      distribution_sign = 1;
      break;
    case 723 ... 782:
      distribution = distribution_index_array [4];
      distribution_sign = 1;
      break;
    case 783 ... 842:
      distribution = distribution_index_array [5];
      distribution_sign = 1;
      break;
    case 852 ... 902:
      distribution = distribution_index_array [6];
      distribution_sign = 1;
      break;
    case 903 ... 962:
      distribution = distribution_index_array [7];
      distribution_sign = 1;
      break;
    case 963 ... 1024:
      distribution = distribution_index_array [8];
      distribution_sign = 1;
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

float fscale( float originalMin, float originalMax, float newBegin, float newEnd, float inputValue, float curve) {

  float OriginalRange = 0;
  float NewRange = 0;
  float zeroRefCurVal = 0;
  float normalizedCurVal = 0;
  float rangedValue = 0;
  boolean invFlag = 0;


  // condition curve parameter
  // limit range

  if (curve > 10) curve = 10;
  if (curve < -10) curve = -10;

  curve = (curve * -.1) ; // - invert and scale - this seems more intuitive - postive numbers give more weight to high end on output
  curve = pow(10, curve); // convert linear scale into lograthimic exponent for other pow function


  // Check for out of range inputValues
  if (inputValue < originalMin) {
    inputValue = originalMin;
  }
  if (inputValue > originalMax) {
    inputValue = originalMax;
  }

  // Zero Refference the values
  OriginalRange = originalMax - originalMin;

  if (newEnd > newBegin) {
    NewRange = newEnd - newBegin;
  }
  else
  {
    NewRange = newBegin - newEnd;
    invFlag = 1;
  }

  zeroRefCurVal = inputValue - originalMin;
  normalizedCurVal  =  zeroRefCurVal / OriginalRange;   // normalize to 0 - 1 float


  // Check for originalMin > originalMax  - the math for all other cases i.e. negative numbers seems to work out fine
  if (originalMin > originalMax ) {
    return 0;
  }

  if (invFlag == 0) {
    rangedValue =  (pow(normalizedCurVal, curve) * NewRange) + newBegin;

  }
  else     // invert the ranges
  {
    rangedValue =  newBegin - (pow(normalizedCurVal, curve) * NewRange);
  }

  return rangedValue;
}

void create_distribution_index () {
  for ( int i  = 0 ; i <= 8 ; i++) {
    distribution_index_array [i] = mapfloat (i, 0, 8, 0, curve);
  }
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}




