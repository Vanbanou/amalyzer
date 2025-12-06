#ifndef ANALYZER_H
#define ANALYZER_H

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <iomanip>
#include <algorithm>
#include <cmath>

#include "superpowered/Superpowered.h"
#include "superpowered/SuperpoweredDecoder.h"
#include "superpowered/SuperpoweredAnalyzer.h"
#include "SuperpoweredSimple.h"

// === TABELA DE KEY (DJ) ===
static const char* camelot_keys[24] = {
    "8B","9B","10B","11B","12B","1B","2B","3B","4B","5B","6B","7B",
    "5A","6A","7A","8A","9A","10A","11A","12A","1A","2A","3A","4A"
};
static const char* openkey_keys[24] = {
    "1d","2d","3d","4d","5d","6d","7d","8d","9d","10d","11d","12d",
    "1m","2m","3m","4m","5m","6m","7m","8m","9m","10m","11m","12m"
};

struct AudioAnalysis {
    std::string filename;
    std::string path;
    double durationSec = 0.0;
    double bpm = 0.0;
    double averageDb = 0.0;
    double energy = 0.0;
    int keyIndex = -1;
    std::string keyCamelot = "";
    std::string keyOpenKey = "";
    
    // Metadata
    std::string title = "";
    std::string artist = "";
    std::string album = "";
    std::string genre = "";
    int year = 0;
    int track = 0;
    int bitrate = 0;
    int sampleRate = 0;
    int channels = 0;
    double fileSizeMB = 0.0;

    bool success = false;
    std::string errorMessage;
};

class Amalyzer {
public:
    Amalyzer() {
        // Initialize Superpowered if not already done externally, 
        // but usually it's better to do it once in main.
        // Superpowered::Initialize("..."); 
    }

    AudioAnalysis analyze(const std::string& path) {
        AudioAnalysis result;
        result.path = path;
        // Extract filename from path
        size_t lastSlash = path.find_last_of("/\\");
        result.filename = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
        result.success = false;

        std::unique_ptr<Superpowered::Decoder> decoder(new Superpowered::Decoder());
        int openReturn = decoder->open(path.c_str());
        if (openReturn != Superpowered::Decoder::OpenSuccess) {
            result.errorMessage = "Decoder open error: " + std::to_string(openReturn);
            return result;
        }

        result.durationSec = decoder->getDurationSeconds();
        result.sampleRate = decoder->getSamplerate();
        unsigned int samplerate = decoder->getSamplerate();
        unsigned int framesPerChunk = decoder->getFramesPerChunk();

        // Safety check
        if (samplerate == 0 || framesPerChunk == 0) {
            result.errorMessage = "Invalid samplerate or framesPerChunk";
            return result;
        }

        // Ensure minimum duration for analyzer to avoid crashes on very short files
        int analysisDuration = (int)result.durationSec + 1;
        if (analysisDuration < 5) analysisDuration = 5;

        std::unique_ptr<Superpowered::Analyzer> analyzer(
            new Superpowered::Analyzer(samplerate, analysisDuration)
        );

        std::vector<short int> intBuffer(framesPerChunk * 2);
        std::vector<float> floatBuffer(framesPerChunk * 2);

        double totalRms = 0.0;
        long long totalFrames = 0;

        while (true) {
            int frames = decoder->decodeAudio(intBuffer.data(), framesPerChunk);
            if (frames < 1) break;

            Superpowered::ShortIntToFloat(intBuffer.data(), floatBuffer.data(), frames);
            analyzer->process(floatBuffer.data(), frames);

            // Calculate Energy (RMS)
            for (int i = 0; i < frames * 2; ++i) {
                totalRms += floatBuffer[i] * floatBuffer[i];
            }
            totalFrames += frames * 2;
        }

        analyzer->makeResults(60, 200, 0, 0, false, 0, false, false, true);

        result.bpm = analyzer->bpm;
        result.averageDb = analyzer->averageDb;
        result.keyIndex = analyzer->keyIndex;

        if (result.keyIndex >= 0 && result.keyIndex < 24) {
            result.keyCamelot = camelot_keys[result.keyIndex];
            result.keyOpenKey = openkey_keys[result.keyIndex];
        } else {
            result.keyCamelot = "???";
            result.keyOpenKey = "???";
        }

        if (totalFrames > 0) {
             double rmsAvg = std::sqrt(totalRms / totalFrames);
             result.energy = std::round(rmsAvg * 100.0) / 100.0;
        }

        result.success = true;
        return result;
    }
};

#endif // ANALYZER_H
