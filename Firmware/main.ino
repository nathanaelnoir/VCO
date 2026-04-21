#include <MozziGuts.h>
#include <Oscil.h> // oscillator template
#include <tables/sin2048_int8.h> // sine table for oscillator
#include <Smooth.h>

extern const byte gain_table[8][256] PROGMEM;
extern const byte harm_table[8][256] PROGMEM;

Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin1(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin2(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin3(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin4(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin5(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin6(SIN2048_DATA);
Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin7(SIN2048_DATA);
//Oscil <SIN2048_NUM_CELLS, AUDIO_RATE> aSin8(SIN2048_DATA);

#define CONTROL_RATE 128 // Hz, powers of 2 are most reliable

float smoothness = 0.5f;
Smooth <byte> Harm_Smooth(smoothness); // to smooth Harmonic
Smooth <byte> Gain_Smooth(smoothness); // to smooth Gain

int freq1 = 110; // base freq of OSC1
int freq1_old = 110;
int voct = 1000; // external V/OCT LSB
int voct_old = 1000;
int freqv1 = 440; // freq1 apply voct
byte harm_knob = 0; // AD wave knob
byte harm_knob_old = 0;
byte gain = 127;

const static int factor = 512;
const float kVoctExponentStep = 5.0f / 1024.0f;

void setup()
{
  startMozzi(CONTROL_RATE); // :)
}

void updateControl()
{
  // Original control path: sums are passed into Smooth<byte> directly.
  harm_knob = constrain(Harm_Smooth.next(mozziAnalogRead(3) + mozziAnalogRead(5)), 0, 255);

  // Original gain path: gain is read from PROGMEM in updateAudio() every sample.
  gain = constrain(Gain_Smooth.next(mozziAnalogRead(1) + mozziAnalogRead(4)), 0, 255);

  // OSC frequency knob
  freq1 = mozziAnalogRead(0) >> 3;

  // Frequency setting
  voct = mozziAnalogRead(7);
  if ((voct != voct_old) || (freq1 != freq1_old) || (harm_knob != harm_knob_old))
  {
    freqv1 = freq1 * pow(2.0f, voct * kVoctExponentStep); // V/oct apply

    aSin1.setFreq(freqv1); // set the frequency
    aSin2.setFreq(freqv1 * (pgm_read_byte(&(harm_table[0][harm_knob]))));
    aSin3.setFreq(freqv1 * (pgm_read_byte(&(harm_table[1][harm_knob]))));
    aSin4.setFreq(freqv1 * (pgm_read_byte(&(harm_table[2][harm_knob]))));
    aSin5.setFreq(freqv1 * (pgm_read_byte(&(harm_table[3][harm_knob]))));
    aSin6.setFreq(freqv1 * (pgm_read_byte(&(harm_table[4][harm_knob]))));
    aSin7.setFreq(freqv1 * (pgm_read_byte(&(harm_table[5][harm_knob]))));
    //aSin8.setFreq(freqv1 * (pgm_read_byte(&(harm_table[6][harm_knob]))));

    voct_old = voct;
    freq1_old = freq1;
    harm_knob_old = harm_knob;
  }
}

int updateAudio()
{
  return MonoOutput::from8Bit(
    aSin1.next() * (pgm_read_byte(&(gain_table[0][gain]))) / factor +
    aSin2.next() * (pgm_read_byte(&(gain_table[1][gain]))) / factor +
    aSin3.next() * (pgm_read_byte(&(gain_table[2][gain]))) / factor +
    aSin4.next() * (pgm_read_byte(&(gain_table[3][gain]))) / factor +
    aSin5.next() * (pgm_read_byte(&(gain_table[4][gain]))) / factor +
    aSin6.next() * (pgm_read_byte(&(gain_table[5][gain]))) / factor +
    aSin7.next() * (pgm_read_byte(&(gain_table[6][gain]))) / factor
    // + aSin8.next() * (pgm_read_byte(&(gain_table[7][gain]))) / factor
  );
}

void loop()
{
  audioHook(); // required here
}
