#ifndef TRIGGERS_H
#define TRIGGERS_H

#include <Arduino.h>
#include <ArduinoFFT.h>

// NOTE: SAMPLES and SAMPLING_FREQUENCY are now defined in the main .cpp file.

using TriggerCallback = void (*)(bool isActive, uint8_t value);

// The AudioTrigger class is now a "template". This allows it to create arrays
// of a size that is defined in your main file, which is a more stable design.
template<size_t SAMPLES>
class AudioTrigger {
public:
    // The constructor is now simpler.
    AudioTrigger(int threshold = 10000, int peakMax = 60000, int minBrightness = 20)
        : threshold_(threshold),
          peakMax_(peakMax),
          minBrightness_(minBrightness),
          callback_(nullptr),
          FFT() {}

    // Method to register the callback function
    void onTrigger(TriggerCallback cb) {
        callback_ = cb;
    }

    // The update function now takes the audio buffer as an argument.
    void update(volatile int16_t sampleBuffer[]) {
        if (!callback_) return;

        // Copy volatile PDM buffer to a local buffer for FFT processing
        for(size_t i=0; i<SAMPLES; i++) {
            vReal[i] = sampleBuffer[i];
            vImag[i] = 0;
        }

        // Perform FFT analysis
        FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
        FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);
        FFT.complexToMagnitude(vReal, vImag, SAMPLES);

        // Analyze the specific frequency bins that correspond to bass.
        double bassMagnitude = 0;
        for (int i = 1; i < 5; i++) { // Bins 1-4 cover the typical bass range
            bassMagnitude += vReal[i];
        }

        // Print the detected magnitude for easy tuning of the threshold
        // Serial.print("Bass Magnitude: ");
        // Serial.println(bassMagnitude);

        // If bass magnitude is over the threshold, fire the callback.
        if (bassMagnitude > threshold_) {
            int value = map(bassMagnitude, threshold_, peakMax_, minBrightness_, 255);
            callback_(true, constrain(value, minBrightness_, 255));
        } else {
            callback_(false, 0);
        }
    }

    // Allows the threshold to be changed on the fly from main.cpp
    void setThreshold(int newThreshold) {
        threshold_ = newThreshold;
    }

private:
    int threshold_;
    int peakMax_;
    int minBrightness_;
    TriggerCallback callback_;

    // FFT-related variables
    ArduinoFFT<double> FFT;
    // These arrays now correctly use the template parameter for their size.
    double vReal[SAMPLES];
    double vImag[SAMPLES];
};

#endif // TRIGGERS_H
