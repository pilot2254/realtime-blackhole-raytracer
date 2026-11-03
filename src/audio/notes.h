#pragma once
// Note names for the score, spelled like Arduino's pitches.h (NOTE_A4, NOTE_CS5, ...). Each name is a MIDI
// note number, not a frequency: the synth computes the exact equal-tempered pitch with noteHz(), so chords
// stay in tune (pitches.h rounds to whole hertz, e.g. C4 = 262 instead of 261.63, which makes stacked voices
// beat against each other) and the sub-bass can go below its lowest note, B0.
#include <cmath>

enum Note : int {
    NOTE_C0 = 12, NOTE_CS0 = 13, NOTE_D0 = 14, NOTE_DS0 = 15, NOTE_E0 = 16, NOTE_F0 = 17,
    NOTE_FS0 = 18, NOTE_G0 = 19, NOTE_GS0 = 20, NOTE_A0 = 21, NOTE_AS0 = 22, NOTE_B0 = 23,
    NOTE_C1 = 24, NOTE_CS1 = 25, NOTE_D1 = 26, NOTE_DS1 = 27, NOTE_E1 = 28, NOTE_F1 = 29,
    NOTE_FS1 = 30, NOTE_G1 = 31, NOTE_GS1 = 32, NOTE_A1 = 33, NOTE_AS1 = 34, NOTE_B1 = 35,
    NOTE_C2 = 36, NOTE_CS2 = 37, NOTE_D2 = 38, NOTE_DS2 = 39, NOTE_E2 = 40, NOTE_F2 = 41,
    NOTE_FS2 = 42, NOTE_G2 = 43, NOTE_GS2 = 44, NOTE_A2 = 45, NOTE_AS2 = 46, NOTE_B2 = 47,
    NOTE_C3 = 48, NOTE_CS3 = 49, NOTE_D3 = 50, NOTE_DS3 = 51, NOTE_E3 = 52, NOTE_F3 = 53,
    NOTE_FS3 = 54, NOTE_G3 = 55, NOTE_GS3 = 56, NOTE_A3 = 57, NOTE_AS3 = 58, NOTE_B3 = 59,
    NOTE_C4 = 60, NOTE_CS4 = 61, NOTE_D4 = 62, NOTE_DS4 = 63, NOTE_E4 = 64, NOTE_F4 = 65,
    NOTE_FS4 = 66, NOTE_G4 = 67, NOTE_GS4 = 68, NOTE_A4 = 69, NOTE_AS4 = 70, NOTE_B4 = 71,
    NOTE_C5 = 72, NOTE_CS5 = 73, NOTE_D5 = 74, NOTE_DS5 = 75, NOTE_E5 = 76, NOTE_F5 = 77,
    NOTE_FS5 = 78, NOTE_G5 = 79, NOTE_GS5 = 80, NOTE_A5 = 81, NOTE_AS5 = 82, NOTE_B5 = 83,
    NOTE_C6 = 84, NOTE_CS6 = 85, NOTE_D6 = 86, NOTE_DS6 = 87, NOTE_E6 = 88, NOTE_F6 = 89,
    NOTE_FS6 = 90, NOTE_G6 = 91, NOTE_GS6 = 92, NOTE_A6 = 93, NOTE_AS6 = 94, NOTE_B6 = 95,
    NOTE_C7 = 96, NOTE_CS7 = 97, NOTE_D7 = 98, NOTE_DS7 = 99, NOTE_E7 = 100, NOTE_F7 = 101,
    NOTE_FS7 = 102, NOTE_G7 = 103, NOTE_GS7 = 104, NOTE_A7 = 105, NOTE_AS7 = 106, NOTE_B7 = 107,
    NOTE_C8 = 108, NOTE_CS8 = 109, NOTE_D8 = 110, NOTE_DS8 = 111, NOTE_E8 = 112, NOTE_F8 = 113,
    NOTE_FS8 = 114, NOTE_G8 = 115, NOTE_GS8 = 116, NOTE_A8 = 117, NOTE_AS8 = 118, NOTE_B8 = 119,
};

// A4 = 440 Hz, twelve-tone equal temperament
inline float noteHz(int note) { return 440.0f * std::pow(2.0f, (note - NOTE_A4) / 12.0f); }
// the same pitch class moved into [lo, lo + 11]
inline int inOctave(int note, int lo) { while (note < lo) note += 12; while (note >= lo + 12) note -= 12; return note; }
