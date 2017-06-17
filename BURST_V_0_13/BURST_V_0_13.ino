/************************************
  BURST 0.13 - BEFACO
  written by Eloi Flores - Winter Modular



  NOTES, KNOWN BUGS and IMPROVEMENTS :

  When cycle on, no trigger, and external ping. There is a process to resync the burst with the ping, because if not the burst is not in phase with the ping.
  This is done taking the difference time between the cycle and the moment when the trigger button has been pressed.
  A proportion is calculated when the trigger button is pressed to provide the position of the trigger in relation with the time window.
  This calculation uses float, known as the evil datatype in terms of processing consume. Another way to do that?

  The resync doesn't work properly with the combination of :
  - Distribution   o     o    o   o  o oo
  - Small amount of repetitions (depending on tempo but 8 7 or less; as lower tempo, the amount of repetitions where we have de-sync is higher)

  Testing the module we noticed that an attenuator in the quantity input could be very useful.
  One solution could be to use [encoder + encoder button hold] to adjust this attenuationof quantity CV in 32 steps.


  At high frequencies there is a gap at the end of the burst
*/




///////////////// LIBRARYS

#include <ClickEncoder.h>
#include <TimerOne.h>
#include <EEPROM.h>
#include "DebugUtils.h"

///////////////// PIN DEFINITIONS


////// ANALOG INS

#define CV_DIVISIONS    A0
#define CV_PROBABILITY  A1
#define CV_QUANTITY     A2
#define CV_DISTRIBUTION A3

////// DIGITAL INS

#define CYCLE_SWITCH    A4
#define CYCLE_STATE     A5

#define PING_STATE      2
#define PING_BUTTON     3
#define ENCODER_1       4
#define ENCODER_2       5
#define TRIGGER_STATE   8
#define TRIGGER_BUTTON  11

////// DIGITAL OUTS

#define EOC_LED         0
#define OUT_LED         1
#define OUT_STATE       7
#define EOC_STATE       9

const int led_pin[4] = { 12, 13, 10, 6 };

////// Encoder

ClickEncoder *encoder;
int16_t encoder_value, last_encoder_value;

void timerIsr() {
  encoder->service();
}

////// VARIABLES

// tempo and counters
unsigned long master_clock = 0;             /// the master clock is the result of averaged time of ping input or encoder button
unsigned long master_clock_temp = 0;        /// we use the temp variables to avoid the parameters change during a burst execution
unsigned long clock_divided = 0;            /// the result of div/mult the master clock depending on the div/mult potentiometer/input
unsigned long clock_divided_temp = 0;            /// we use the temp variables to avoid the parameters change during a burst execution

int time_portions = 0;                      /// the linear portions of time depending on clock divided and number of repetitions in the burst. If the distribution is linear this will give us the duration of every repetition.
/// if it is not linear it will be used to calculate the distribution
int time_portions_temp = 0;                 /// we use the temp variables to avoid the parameters change during a burst execution

unsigned long burst_time_start = 0;         /// the moment when the burst start
unsigned long burst_time_accu = 0;          /// the accumulation of all repetitions times since the burst have started

unsigned long repetition_raise_start = 0;       /// the time when the previous repetition pulse have raised

byte repetition_counter = 0;            /// the current repetition number since the burst has started

unsigned long elapsed_time_since_prev_repetition = 0;               /// the difference between previous repetition time and current repetition time. the difference between one repetition and the next one
unsigned long elapsed_time_since_prev_repetition_new = 0;           /// the position of the new repetition inside the time window
unsigned long elapsed_time_since_prev_repetition_old = 0;           /// the position of the previous repetition inside the time window

unsigned long led_quantity_time = 0;        /// time counter used to turn off the led when we are chosing the number of repetitions with the encoder

bool in_eoc = false;
bool wants_eoc = false;
unsigned long eoc_counter = 0;              /// a counter to turn off the eoc led and the eoc output


//    divisions
int divisions;                            //// value of the time division or the time multiplier
int divisions_pot;

////// Repetitions

#define MAX_REPETITIONS 32                  /// max number of repetitions

byte repetitions = 1;                       /// number of repetitions
byte repetitions_old = 1;
byte repetitions_temp = 1;                  /// temporal value that adds the number of repetitions in the encoder and the number number added by the CV quantity input
byte repetitions_encoder = 0;
byte repetitions_encoder_temp = 0;

///// Random
int random_pot = 0;
int random_dif = 0;

///// Distribution
int distribution_pot = 0;
enum {
  DISTRIBUTION_SIGN_NEGATIVE = 0,
  DISTRIBUTION_SIGN_POSITIVE = 1,
  DISTRIBUTION_SIGN_ZERO = 2
};

byte distribution_sign = DISTRIBUTION_SIGN_POSITIVE;
float distribution = 0;
float distribution_index_array [9];       /// used to calculate the position of the repetitions
int curve = 5;                            /// the curved we apply to the  pow function to calculate the distribution of repetitions

//// Trigger
uint8_t trigger_button_state = LOW;           /// the trigger button
bool triggered = false;                        /// the result of both trigger button and trigger input
long trigger_difference = 0;            /// the time difference between the trigger and the ping
bool trigger_first_pressed = 0;
float trigger_dif_proportional = 0;
//// Cycle
bool cycle_switch_state = 0;             /// the cycle switch
bool cycle_in_state = 0;                 /// the cycle input
bool cycle = 0;                          /// the result of both cycle switch and cycle input


/// ping
byte ping_in_state = 0;


/// output
bool output_state = HIGH;


/// flags
bool burst_started = 0;                   // if the burst is active or not
bool no_more_bursts = HIGH;               // used for probability to stop bursts, HIGH -> NO repetitions, LOW -> repetitions
bool first_burst = 0;                     // high if we are in the first repetition, LOW if we are in any of the other repetitions
bool resync = 0;                          // active when the resync in cycle can be done

//// encoder button and tap tempo
byte encoder_button_state = 0;
unsigned long tempo_tic = 0;                        /// everytime a pulse is received in ping input or the encoder button (tap) is pressed we store the time
unsigned long tempo_tic_temp = 0;

void setup() {

  //// remove to activate eoc_led and out_led
  //Serial.begin(9600);
  //Serial.println("HOLA");

  /// Encoder
  encoder = new ClickEncoder(ENCODER_2, ENCODER_1, 3);

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);

  // Encoder pins
  pinMode (ENCODER_1, INPUT_PULLUP);
  pinMode (ENCODER_2, INPUT_PULLUP);


  pinMode (CYCLE_SWITCH, INPUT_PULLUP);
  pinMode (CYCLE_STATE, INPUT_PULLUP);
  pinMode (PING_STATE, INPUT_PULLUP);
  pinMode (TRIGGER_STATE, INPUT_PULLUP);
  pinMode (TRIGGER_BUTTON, INPUT_PULLUP);
  pinMode (PING_BUTTON, INPUT_PULLUP);



  pinMode (EOC_LED, OUTPUT);
  pinMode (OUT_LED, OUTPUT);

  pinMode (OUT_STATE, OUTPUT);
  digitalWrite(OUT_STATE, HIGH);

  pinMode (EOC_STATE, OUTPUT);
  digitalWrite(EOC_STATE, HIGH);

  create_distribution_index();

  for (int i = 0; i < 4; i++) {
    pinMode(led_pin[i], OUTPUT);
  }

  master_clock = (EEPROM.read(0) & 0xFF) + (((long)EEPROM.read(1) << 8) & 0xFFFF) + (((long)EEPROM.read(2) << 16) & 0xFFFFFF) + (((long)EEPROM.read(3) << 24) & 0xFFFFFFFF);
  master_clock_temp = master_clock;
  repetitions = EEPROM.read(4);
  if (repetitions < 1) repetitions = 1;

  repetitions_temp = repetitions;
  repetitions_old = repetitions;
  repetitions_encoder = repetitions;
  repetitions_encoder_temp = repetitions;

  read_division();
  calcTimePortions();
}

void loop() {

  unsigned long current_time = millis();

  if ((triggered == HIGH) && (trigger_first_pressed == HIGH)) { ///// we read the values and pots and inputs, and store the time difference between ping clock and trigger
    if (wants_eoc) {
      enableEOC(current_time);
    }
    repetition_counter = 0;

    if (repetitions_encoder_temp != repetitions_encoder) {
      repetitions_encoder = repetitions_encoder_temp;
      EEPROM.write(4, repetitions_encoder_temp);
    }

    doResync(current_time);

    trigger_difference = burst_time_start - tempo_tic_temp;       /// when we press the trigger button we define the phase difference between the external clock and our burst
    trigger_dif_proportional = (float)master_clock_temp / (float)trigger_difference;
    triggered = trigger_first_pressed = LOW;
  }

  calculate_clock(current_time); // we read the ping in and the encoder button to get : master clock, clock divided and time_portions
  read_trigger(); // we read the trigger input and the trigger button to see if it is high or low, if it is high and it is the first time it is.
  read_repetitions(current_time); // we read the number of repetitions in the encoder, we have to attend this process often to avoid missing encoder tics.

  handleEOC(current_time, 30);
  handleLEDs(current_time);

  if (cycle == HIGH) { // CYCLE ON
    // TODO: investigate to ensure that we don't have drift wrt ping when cycling
    // It's actually extremely unlikely that we do, but it's worth verifying
    // However, the use of trigger difference in relation to cycle was not correct, since the
    // difference is an absolute amount of the entire master_clock, and wasn't being
    // corrected for mult/div. For now, I'm just ensuring that we've finished a division before
    // permitting a resync. Otherwise, we need something more complex than the original test.
    // We'd need to know which is the first burst of any particular cycle and ensure that it triggers
    // at the proportionally correct time depending on mult/div.
    if (burst_time_start && (current_time >= (burst_time_start + clock_divided)) && resync) {
      if (repetitions != repetitions_temp) {
        EEPROM.write(4, repetitions_temp);
      }
      doResync(current_time);
    }
  }

  handlePulseDown(current_time);
  handlePulseUp(current_time, cycle);

}
