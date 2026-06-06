# AHS VCO `main_clean`

This README describes the behavior of `main_clean/main/main.ino` in musical and mathematical terms. The sketch is a three-mode Mozzi oscillator for Arduino Nano with:

- 2-operator FM mode
- chord oscillator-bank mode
- additive oscillator-bank mode

The firmware runs Mozzi in 2-pin PWM output mode with a control rate of 128 Hz. Audio generation happens in `updateAudio()`, while potentiometers, CV inputs, mode switching, oscillator frequencies, and table selections are updated in `updateControl()`.

## Inputs And Modes

The sketch reads these analog inputs:

| Name in code | Analog input | Main purpose |
| --- | ---: | --- |
| `AnalogRead0` | A0 | base pitch knob |
| `AnalogRead1` | A1 | mode-dependent parameter 1 |
| `Mode_CV` | A2 | mode CV selection |
| `AnalogRead3` | A3 | mode-dependent parameter 2 |
| `AnalogRead4` | A4 | CV for parameter 1 |
| `AnalogRead5` | A5 | CV for parameter 2 |
| `Gain_CV_*` | A6 | output level CV |
| `AnalogRead7` | A7 | V/oct input |

There are three synthesis modes:

```cpp
kModeFm       = 0
kModeChord    = 1
kModeAdditive = 2
```

The physical mode switch chooses a base ordering, and the mode CV chooses one of the three modes inside that ordering. Mathematically, the result is a discrete selector:

$$
M \in \{\mathrm{FM}, \mathrm{Chord}, \mathrm{Additive}\}
$$

where `Mode_CV` is computed by mapping the mode CV ADC value into:

$$
\mathrm{ModeCV} = \left\lfloor \operatorname{map}(A_2, 0, 1023, 0, 2) \right\rfloor
$$

The switch then permutes the three possible modes.

## Pitch Model

The sketch uses a lookup table called `voctpow`. It stores values from roughly `0` to `4.995`, indexed by the V/oct ADC input. The frequency logic repeatedly uses:

$$
f = f_\mathrm{base} \cdot 2^{V}
$$

where:

$$
V = \mathrm{voctpow}[\mathrm{ADC}]
$$

So the table is effectively a quantized exponential pitch control. If the V/oct ADC index increases by about one octave worth of table distance, the frequency doubles.

The helper:

```cpp
readVoctPow(index)
```

constrains the index to `0...1023`, then reads the floating-point value from program memory. This protects table access when chord offsets and inversion offsets are added.

## Output Level

The level CV is mapped to `0...255`:

$$
G = \operatorname{map}(A_6, 0, 1023, 0, 255)
$$

Audio samples are usually multiplied by:

$$
\frac{G}{256}
$$

Then the result is clipped to the signed 8-bit audio range:

$$
y = \operatorname{clip}(x, -128, 127)
$$

This is what `clipAudio8()` does.

## Wavetable Oscillators

The sketch uses Mozzi `Oscil` objects. Conceptually, each oscillator reads periodically through a table:

$$
x[n] = T(\phi[n])
$$

with phase:

$$
\phi[n+1] = \phi[n] + \Delta \phi
$$

where the phase increment is proportional to frequency:

$$
\Delta \phi \propto \frac{f}{f_s}
$$

Different tables give different waveforms:

| Wave enum | Table |
| --- | --- |
| `kWaveSaw` | saw |
| `kWaveSquare` | square |
| `kWaveTriangle` | triangle |
| `kWaveSine` | sine |
| `kWaveChebyshev` | Chebyshev waveshape |
| `kWaveHalfSine` | half sine |
| `kWaveSigmoid` | sigmoid waveshape |
| `kWavePhasor` | phasor |

## FM / 2-Operator Mode

FM mode uses one carrier oscillator and one modulator oscillator. In `main_clean`, both are cosine-table oscillators:

```cpp
aCarrier(COS2048_DATA)
aModulator(COS2048_DATA)
```

The base carrier frequency is computed from the pitch knob and V/oct input:

$$
f_c = (2270658 + 5000A_0) \cdot 2^V
$$

This value is a fixed-point Mozzi frequency value, not a simple Hz value. The important musical idea is:

- `A0` raises the base carrier frequency
- V/oct multiplies the frequency exponentially

The modulator frequency is derived from the carrier frequency and parameter 1:

$$
f_m \propto f_c \cdot \left(\frac{A_1}{2} + \frac{A_4}{2}\right)
$$

In code:

```cpp
mod_freq = ((carrier_freq >> 8) * (A1 / 2 + A4 / 2));
```

So parameter 1 and its CV act like a modulation-ratio or modulation-frequency control. It is not a clean integer ratio knob; it is a broad digital scaling of the carrier frequency.

The phase deviation is computed from the modulator frequency and parameter 2:

$$
D \propto f_m \cdot (1 + A_3 + A_5)
$$

In code:

```cpp
deviation = ((mod_freq >> 16) * (1 + A3 + A5));
```

The FM signal is then:

$$
y[n] = \cos\left(\phi_c[n] + D \cdot \cos(\phi_m[n])\right)
$$

scaled by the level CV:

$$
y_\mathrm{out}[n] =
\operatorname{clip}
\left(
y[n] \cdot \frac{G}{256},
-128,
127
\right)
$$

This is classic phase modulation, which is closely related to FM. The audible sidebands depend on the ratio:

$$
r = \frac{f_m}{f_c}
$$

and the modulation index/deviation:

$$
\beta \approx D
$$

For sinusoidal FM, the idealized spectrum is often described as:

$$
y(t) = \sin(2\pi f_c t + \beta \sin(2\pi f_m t))
$$

which expands into sidebands around the carrier:

$$
f = f_c \pm k f_m
$$

for integer:

$$
k = 0, 1, 2, \dots
$$

In this sketch, the scaling is fixed-point and quite aggressive, so the result is more practical/performative than mathematically normalized FM.

## Chord Mode

Chord mode creates a five-oscillator stack. Four notes come from a chord table, and the fifth oscillator repeats the root.

The chord table stores pitch offsets:

```cpp
chord_table[8][4]
```

Each row is a chord type:

| Index | Meaning |
| ---: | --- |
| 0 | major |
| 1 | major 7 |
| 2 | major add 9 |
| 3 | sus 2 |
| 4 | minor add 9 |
| 5 | minor 7 |
| 6 | minor |
| 7 | root only |

The chord selector combines the parameter 2 knob and CV:

$$
c = \operatorname{clip}
\left(
\left\lfloor \frac{A_3}{128} \right\rfloor
+
\left\lfloor \frac{A_5}{128} \right\rfloor,
0,
7
\right)
$$

The inversion selector combines parameter 1 and its CV:

$$
i = \operatorname{clip}
\left(
\left\lfloor \frac{A_1}{64} \right\rfloor
+
\left\lfloor \frac{A_4}{64} \right\rfloor,
0,
15
\right)
$$

The inversion state chooses octave shifts for the chord tones:

$$
o_1,o_2,o_3,o_4 \in \{0,1,2\}
$$

and whether the repeated root oscillator is active:

$$
r_5 \in \{0,1\}
$$

The code calls these:

```cpp
inv_aply1
inv_aply2
inv_aply3
inv_aply4
inv_aply5
```

The constant:

```cpp
kOctaveTableStep = 205
```

is the approximate table-index distance for one octave in `voctpow`. So each chord oscillator frequency is:

$$
f_j =
f_\mathrm{root}
\text{ converted through }
2^{V + \mathrm{note}_j + 205o_j}
$$

More explicitly:

$$
f_j =
f_0 \cdot
2^{
\mathrm{voctpow}
\left[
A_7 + 205o_j + n_j
\right]
}
$$

where:

- \(f_0 = A_0 / 4\)
- \(A_7\) is the V/oct ADC index
- \(o_j\) is the inversion octave offset
- \(n_j\) is the chord-table pitch offset

The fifth voice repeats the root:

$$
f_5 =
f_0 \cdot
2^{
\mathrm{voctpow}
\left[
A_7 + n_1
\right]
}
$$

but its audio is multiplied by `inv_aply5`, so it can be switched on or off:

$$
y_5[n] \cdot r_5
$$

### Chord Waveform Selection

Chord mode has selectable waveform banks. Normally parameter 1 controls inversion. But if parameter 1 is fully clockwise:

$$
A_1 \ge 1020
$$

then parameter 2 becomes a waveform selector:

$$
w = \left\lfloor \frac{A_3}{128} \right\rfloor
$$

So the 0...1023 ADC range is split into eight waveform zones.

The chord output is approximately:

$$
y[n] =
\frac{x_1[n]}{8}
+
\frac{x_2[n]}{8}
+
\frac{x_3[n]}{8}
+
\frac{x_4[n]}{8}
+
r_5\frac{x_5[n]}{8}
$$

then scaled by level:

$$
y_\mathrm{out}[n] =
\operatorname{clip}
\left(
y[n]\frac{G}{256},
-128,
127
\right)
$$

This is a parallel oscillator-bank chord. It is not additive synthesis in the Fourier sense, because the oscillators are tuned to chord intervals rather than harmonic multiples of one fundamental.

## Additive Mode

Additive mode uses eight sine oscillators:

```cpp
aSin1 ... aSin8
```

The base frequency is:

$$
f_0 = \frac{A_0}{8} \cdot 2^{\mathrm{voctpow}[A_7]}
$$

The harmonic selector is:

$$
h =
\operatorname{clip}
\left(
\left\lfloor \frac{A_3}{4} \right\rfloor
+
\left\lfloor \frac{A_5}{4} \right\rfloor,
0,
255
\right)
$$

The gain-shape selector is:

$$
g =
\operatorname{clip}
\left(
\left\lfloor \frac{A_1}{4} \right\rfloor
+
\left\lfloor \frac{A_4}{4} \right\rfloor,
0,
255
\right)
$$

The harmonic table gives integer-ish frequency multipliers:

$$
m_k(h) = \mathrm{harm\_table}[k][h]
$$

The gain table gives amplitude weights:

$$
a_k(g) = \mathrm{gain\_table}[k][g]
$$

The oscillator frequencies are:

$$
f_1 = f_0
$$

and for the other seven oscillators:

$$
f_{k+2} = f_0 \cdot m_k(h)
$$

for:

$$
k = 0,1,2,3,4,5,6
$$

The output is:

$$
y[n] =
\sum_{k=0}^{7}
\left(
\frac{a_k(g)}{1024}
\right)
\sin(2\pi f_k n / f_s)
$$

then level-scaled:

$$
y_\mathrm{out}[n] =
\operatorname{clip}
\left(
y[n]\frac{G}{256},
-128,
127
\right)
$$

### Why It Is “Kind Of Fourier” But Not Exactly

Classical additive synthesis can recreate a periodic waveform by summing harmonically related sine waves:

$$
x(t) =
\sum_{k=1}^{N}
A_k \sin(2\pi k f_0 t + \phi_k)
$$

This is close to a finite Fourier series when:

1. all partial frequencies are exact integer multiples of one fundamental,
2. amplitudes \(A_k\) represent Fourier coefficients,
3. phases \(\phi_k\) are controlled or known.

The sketch resembles this because it sums sine oscillators:

$$
y(t) =
\sum_{k=1}^{8}
A_k \sin(2\pi f_k t)
$$

However, it is not a strict Fourier reconstruction because:

- the multipliers come from `harm_table`, not from fixed \(1,2,3,4,\dots\)
- the amplitude curves come from `gain_table`, not from Fourier analysis of a target waveform
- oscillator phases are not used as explicit Fourier phase coefficients
- some multiplier combinations may create clustered, repeated, or nonstandard partial structures
- the controls are designed for musical morphing, not exact spectral reconstruction

So additive mode is better described as:

$$
\text{table-controlled partial synthesis}
$$

or:

$$
\text{a weighted sum of sine partials}
$$

rather than a mathematically exact Fourier synthesizer.

The most accurate compact description is:

$$
y(t; h,g) =
\sum_{k=1}^{8}
A_k(g)
\sin
\left(
2\pi f_0 M_k(h)t
\right)
$$

where:

- \(M_k(h)\) comes from `harm_table`
- \(A_k(g)\) comes from `gain_table`
- \(h\) controls the partial frequency structure
- \(g\) controls the partial amplitude structure

## Clipping And Scaling

All modes eventually produce an 8-bit signed audio sample. Since several oscillators are summed together, the sketch divides partials by constants such as `8` or `1024`, then applies level CV.

The final signal path is generally:

$$
y_\mathrm{digital}
=
\operatorname{clip}
\left(
\alpha \cdot
\sum_k x_k[n],
-128,
127
\right)
$$

where \(\alpha\) includes:

- per-partial scaling
- gain-table values
- level CV

This means high-gain settings can still saturate. Saturation here is not analog soft clipping; it is integer-range limiting:

$$
\operatorname{clip}(x,-128,127)
=
\begin{cases}
-128 & x < -128 \\
x & -128 \le x \le 127 \\
127 & x > 127
\end{cases}
$$

## Summary

The three modes are mathematically different:

| Mode | Core equation | Musical behavior |
| --- | --- | --- |
| FM / 2-op | \(y(t)=\cos(\phi_c + D\cos(\phi_m))\) | sidebands, metallic tones, ratio/depth interaction |
| Chord | \(y(t)=\sum_{j=1}^{5} x_j(t)\) | stacked chord tones from a pitch-offset table |
| Additive | \(y(t)=\sum_{k=1}^{8} A_k(g)\sin(2\pi f_0M_k(h)t)\) | weighted sine partials, Fourier-like but table-shaped |

The important distinction is that chord mode sums independent musical notes, while additive mode sums partials related to one root frequency. Additive mode is Fourier-like because it is a sine sum, but it is not a true Fourier series because the frequency multipliers and amplitudes are chosen by musical lookup tables rather than derived from a target waveform.
