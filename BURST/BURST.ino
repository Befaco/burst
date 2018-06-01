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

///////////////// LIBRARIES

#include "DebugUtils.h" // <- DEBUG define in here
// #define ENC_DECODER (1 << 2)
#include <ClickEncoder.h>
#include <EEPROM.h>
#include <TimerOne.h>
#define BOUNCE_LOCK_OUT // use lock out method for better response
#include <Bounce2.h>

///////////////// PIN DEFINITIONS

////// ANALOG INS

#define CV_DIVISIONS    A0
#define CV_PROBABILITY  A1
#define CV_REPETITIONS  A2
#define CV_DISTRIBUTION A3

////// DIGITAL INS

#define CYCLE_SWITCH    A4
#define CYCLE_STATE     A5

#define PING_STATE      2
#define ENCODER_BUTTON  3
#define ENCODER_1       5
#define ENCODER_2       4
#define TRIGGER_STATE   8
#define TRIGGER_BUTTON  11

////// DIGITAL OUTS

#define TEMPO_STATE     0
#define OUT_LED         1
#define OUT_STATE       7
#define EOC_STATE       9

const int ledPin[4] = { 12, 13, 10, 6 };

////// Encoder

ClickEncoder *encoder;
int16_t encoderValue, lastEncoderValue;

void timerIsr()
{
  encoder->service();
}

////// VARIABLES

// tempo and counters
unsigned long masterClock = 0;             /// the master clock is the result of averaged time of ping input or encoder button
unsigned long masterClock_Temp = 0;        /// we use the temp variables to avoid the parameters change during a burst execution
unsigned long clockDivided = 0;            /// the result of div/mult the master clock depending on the div/mult potentiometer/input
unsigned long clockDivided_Temp = 0;            /// we use the temp variables to avoid the parameters change during a burst execution

float timePortions = 0;                      /// the linear portions of time depending on clock divided and number of repetitions in the burst. if the distribution is linear this will give us the duration of every repetition.
/// if it is not linear it will be used to calculate the distribution
float timePortions_Temp = 0;                 /// we use the temp variables to avoid the parameters change during a burst execution

unsigned long firstBurstTime = 0;         /// the moment when the burst start
unsigned long burstTimeStart = 0;         /// the moment when the burst start
unsigned long burstTimeAccu = 0;          /// the accumulation of all repetitions times since the burst have started

unsigned long repetitionRaiseStart = 0;       /// the time when the previous repetition pulse have raised

byte repetitionCounter = 0;            /// the current repetition number since the burst has started

unsigned long elapsedTimeSincePrevRepetition = 0;               /// the difference between previous repetition time and current repetition time. the difference between one repetition and the next one
unsigned long elapsedTimeSincePrevRepetitionNew = 0;           /// the position of the new repetition inside the time window
unsigned long elapsedTimeSincePrevRepetitionOld = 0;           /// the position of the previous repetition inside the time window

unsigned long ledQuantityTime = 0;        /// time counter used to turn off the led when we are chosing the number of repetitions with the encoder
unsigned long ledParameterTime = 0;        /// time counter used to turn off the led when we are chosing the number of repetitions with the encoder

bool inEoc = false;
bool wantsEoc = false;
unsigned long eocCounter = 0;              /// a counter to turn off the eoc led and the eoc output

//    divisions
int divisions;                            //// value of the time division or the time multiplier
int divisions_Temp = 0;
int divisionCounter = 0;

unsigned long triggerButtonPressedTime = 0;

////// Repetitions

#define MAX_REPETITIONS 32                  /// max number of repetitions

byte repetitions = 1;                       /// number of repetitions
byte repetitionsOld = 1;
byte repetitions_Temp = 1;                  /// temporal value that adds the number of repetitions in the encoder and the number number added by the cv quantity input
byte repetitionsEncoder = 0;
byte repetitionsEncoder_Temp = 0;

///// Random
int randomPot = 0;
int randomPot_Temp = 0;

///// Distribution
enum {
  DISTRIBUTION_SIGN_NEGATIVE = 0,
  DISTRIBUTION_SIGN_POSITIVE = 1,
  DISTRIBUTION_SIGN_ZERO = 2
};

float distribution = 0;
byte distributionSign = DISTRIBUTION_SIGN_POSITIVE;
float distributionIndexArray[9];       /// used to calculate the position of the repetitions
int curve = 5;                         /// the curved we apply to the  pow function to calculate the distribution of repetitions
float distribution_Temp = 0;
byte distributionSign_Temp = DISTRIBUTION_SIGN_POSITIVE;

//// Trigger
uint8_t triggerButtonState = LOW;           /// the trigger button
bool triggered = false;                        /// the result of both trigger button and trigger input
bool triggerFirstPressed = 0;

// #define DELAYED_TRIGGER // uncomment to enable delay from trigger time to actual burst start (for dig. modules where CV is updated after gate)

#ifdef DELAYED_TRIGGER
#define DELAYED_TRIGGER_MS (2)
unsigned long triggerTime = 0;
#endif

//// Cycle
bool cycleSwitchState = 0;             /// the cycle switch
bool cycleInState = 0;                 /// the cycle input
bool cycle = false;                    /// the result of both cycle switch and cycle input
bool resetPhase = false;

/// ping
byte pingInState = 0;

/// output
bool outputState = LOW;

bool recycle = false;     // recycle = start a new burst within a set of bursts (mult)
bool resync = false;      // resync = start a new burst, but ensure that we're correctly phased

//// encoder button and tap tempo
byte encoderButtonState = 0;
unsigned long tempoTic = 0;       // the PREVIOUS tempo tick
unsigned long tempoTic_Temp = 0;  // the NEXT tempo tick
unsigned long tempoTimer = 0;
unsigned long tempoTimer_Temp = 0;

byte disableFirstClock = 0; // backward since we can't assume that there's anything saved on the EEPROM
byte probabilityAffectsEOC = 0;
byte silentBurst = false;

enum {
  RETRIGGER = 0,
  NO_RETRIGGER = 1
};
byte retriggerMode = RETRIGGER;

Bounce bounce;

bool wantsMoreBursts = HIGH;

void setup()
{
  //// remove to activate TEMPO_STATE and OUT_LED
#ifdef DEBUG
  Serial.begin(9600);
  Serial.println("HOLA");
#endif

  /// Encoder
  encoder = new ClickEncoder(ENCODER_2, ENCODER_1, ENCODER_BUTTON);

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
  pinMode (ENCODER_BUTTON, INPUT_PULLUP);

  pinMode (TEMPO_STATE, OUTPUT);
  pinMode (OUT_LED, OUTPUT);
  pinMode (OUT_STATE, OUTPUT);
  digitalWrite(OUT_STATE, HIGH);

  pinMode (EOC_STATE, OUTPUT);
  digitalWrite(EOC_STATE, HIGH);

  bounce.interval(10); // debounce 10ms
  bounce.attach(TRIGGER_BUTTON);

  createDistributionIndex();

  for (int i = 0; i < 4; i++) {
    pinMode(ledPin[i], OUTPUT);
  }

  masterClock = (EEPROM.read(0) & 0xff) + (((long)EEPROM.read(1) << 8) & 0xff00) +
                (((long)EEPROM.read(2) << 16) & 0xff0000) +
                (((long)EEPROM.read(3) << 24) & 0xff000000);
  if (masterClock < 1) masterClock = 120;
  masterClock_Temp = masterClock;

  repetitions = EEPROM.read(4);
  repetitions = constrain(repetitions, 1, MAX_REPETITIONS);

  disableFirstClock = EEPROM.read(5);

  repetitions_Temp = repetitions;
  repetitionsOld = repetitions;
  repetitionsEncoder = repetitions;
  repetitionsEncoder_Temp = repetitions;

  retriggerMode = EEPROM.read(14);
  probabilityAffectsEOC = EEPROM.read(15);

  if (!checkCalibrationMode()) {
    doLightShow();
  }

  readCycle();
  if (cycle == HIGH) {
    triggered = triggerFirstPressed = HIGH;
  }

  readDivision(0);
  calcTimePortions();
}

void loop()
{
  unsigned long currentTime = millis();

  if (burstTimeStart && firstBurstTime && cycle == HIGH && currentTime >= (firstBurstTime + masterClock)) { // force resync + tempo adjustment
    currentTime = firstBurstTime + masterClock; // spoof the current time so that we always remain in sync with the tempo
    triggered = triggerFirstPressed = HIGH;
    wantsEoc = true;
  }

  ///// we read the values and pots and inputs, and store the time difference between ping clock and trigger
  if ((triggered == HIGH && triggerFirstPressed == HIGH)
    || (resetPhase == true && currentTime >= tempoTic_Temp))
  {
    if (wantsEoc) {
      enableEOC(currentTime);
    }
    repetitionCounter = 0;

    if (repetitionsEncoder != repetitionsEncoder_Temp) {
      repetitionsEncoder = repetitionsEncoder_Temp;
      EEPROM.write(4, repetitionsEncoder);
    }
    triggerFirstPressed = LOW;

#ifndef DELAYED_TRIGGER
    doResync(currentTime);
    wantsMoreBursts = HIGH;
    startBurstInit(currentTime);
    resetPhase = false;
#endif
  }

#ifdef DELAYED_TRIGGER
  if (triggerTime && currentTime >= triggerTime + DELAYED_TRIGGER_MS) { // 2ms delay by default
    doResync(triggerTime);
    wantsMoreBursts = HIGH;
    startBurstInit(triggerTime);
    triggerTime = 0;
  }
#endif

  calculateClock(currentTime); // we read the ping in and the encoder button to get : master clock, clock divided and timePortions
  readTrigger(currentTime); // we read the trigger input and the trigger button to see if it is HIGH or LOW, if it is HIGH and it is the first time it is.
  readRepetitions(currentTime); // we read the number of repetitions in the encoder, we have to attend this process often to avoid missing encoder tics.

  readDivision(currentTime);
  readDistribution(currentTime);
  readRandom(currentTime);

  handleEOC(currentTime, 30);
  handleLEDs(currentTime);

  // do this before cycle resync
  handlePulseDown(currentTime);
  handlePulseUp(currentTime);

  if (burstTimeStart && cycle == HIGH) { // CYCLE ON
    if (recycle
          // ensure that the current cycle is really over
        && (currentTime >= (burstTimeStart + clockDivided))
          // ensure that the total burst (incl mult) is really over
        && (!resync || currentTime >= (firstBurstTime + masterClock)))
    {
      if (resync) {
        if (repetitionsEncoder != repetitionsEncoder_Temp) {
          repetitionsEncoder = repetitionsEncoder_Temp;
          EEPROM.write(4, repetitionsEncoder);
        }
        doResync(currentTime);
      }
      startBurstInit(currentTime);
    }
  }
  else if (!burstTimeStart || cycle == LOW) {
    if (currentTime >= tempoTic_Temp) { // ensure that the timer advances
      doResync(currentTime);
    }
  }
  handleTempo(currentTime);
}
