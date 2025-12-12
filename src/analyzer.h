#ifndef ANALYZER_H
#define ANALYZER_H

#include <string>

struct AudioAnalysis {
    std::string filename;
    std::string path;
    double durationSec;
    double bpm;
    double averageDb;
    double energy;
    int keyIndex;
    std::string keyCamelot;
    std::string keyOpenKey;

    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    int year;
    int track;
    int bitrate;
    int sampleRate;
    int channels;
    double fileSizeMB;

    bool success;
    std::string errorMessage;

    AudioAnalysis()
        : durationSec(0), bpm(0), averageDb(0), energy(0),
          keyIndex(-1), year(0), track(0), bitrate(0),
          sampleRate(0), channels(0), fileSizeMB(0),
          success(false) {}
};

class Analyzer {
public:
    Analyzer();
    AudioAnalysis analyze(const std::string& path);
};

#endif
