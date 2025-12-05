#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <set>
#include <map>
#include <stdexcept>

// Include our new Analyzer class
#include "analyzer.h"

// TagLib Includes
#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/tfile.h>
#include <taglib/textidentificationframe.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4file.h>
#include <taglib/flacfile.h>
#include <taglib/oggfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/xiphcomment.h>

namespace fs = std::filesystem;

// ==============================
// 🎨 CONSTANTES E GLOBAIS
// ==============================

const std::string RESET = "\033[0m";
const std::string RED = "\033[91m";
const std::string GREEN = "\033[92m";
const std::string YELLOW = "\033[93m";
const std::string BLUE = "\033[94m";

bool IS_SILENT = false;

// ==============================
// 🎵 ESTRUTURAS
// ==============================

// AudioAnalysis is now defined in analyzer.h

struct ProgramArgs {
    std::vector<std::string> paths;
    bool recursive = false;
    std::vector<std::string> extensions = {".mp3", ".flac", ".ogg", ".wav", ".m4a", ".aif", ".aiff"};
    bool csv = false;
    std::string outputFile;
    bool quiet = false;
    double minBpm = 0.0;
    double maxBpm = 0.0;
    double minDuration = 0.0;
    double maxDuration = 0.0;
    std::string targetKey = "";
    std::vector<std::string> tagsToWrite;
    bool putForce = false;
    bool listMode = false;
    std::vector<std::string> sortBy = {"name"};
};

// ==============================
// 🛠️ FUNÇÕES AUXILIARES
// ==============================

std::string truncate(const std::string& str, size_t width) {
    if (str.length() > width) {
        return str.substr(0, width - 1) + "…";
    }
    return str;
}

std::string toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

void log(const std::string& level, const std::string& message, const std::string& detail = "") {
    if (IS_SILENT && level != "ERROR") return;

    if (level == "INFO") std::cout << BLUE << "[ ℹ️ ] " << RESET;
    else if (level == "WARNING") std::cout << YELLOW << "[ ⚠️ ] " << RESET;
    else if (level == "ERROR") std::cerr << RED << "[ 🔥 ] " << RESET;
    else if (level == "SUCCESS") std::cout << GREEN << "[ ✅ ] " << RESET;

    if (level == "ERROR") std::cerr << message;
    else std::cout << message;

    if (!detail.empty()) {
        if (level == "ERROR") std::cerr << " (" << detail << ")";
        else std::cout << " (" << detail << ")";
    }
    
    if (level == "ERROR") std::cerr << std::endl;
    else std::cout << std::endl;
}

// ==============================
// 🏷️ ESCRITA DE TAGS
// ==============================

std::string cleanAlbumPrefix(const std::string& albumStr) {
    if (albumStr.empty()) return "";
    std::string temp = albumStr;
    int count = 0;
    while (count < 3) {
        size_t pos = temp.find(" | ");
        if (pos == std::string::npos) break;
        std::string prefix = temp.substr(0, pos);
        bool isPrefix = false;
        if (std::all_of(prefix.begin(), prefix.end(), [](char c){ return ::isalnum(c) || c == '.' || c == '#'; })) {
            isPrefix = true;
        }
        if (isPrefix) temp = temp.substr(pos + 3);
        else break;
        count++;
    }
    return temp;
}

void writeTags(const AudioAnalysis& res, const std::vector<std::string>& tagsToWrite, bool force) {
    if (tagsToWrite.empty() || (res.bpm < 0.1 && res.energy < 0.01)) return;

    std::stringstream bpmSs, energySs;
    bpmSs << std::fixed << std::setprecision(0) << std::round(res.bpm);
    energySs << std::fixed << std::setprecision(2) << res.energy;
    
    std::string bpmStr = bpmSs.str();
    std::string energyStr = energySs.str();
    std::string keyStr = res.keyCamelot;

    std::vector<std::string> parts;
    if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "bpm") != tagsToWrite.end()) parts.push_back(bpmStr);
    if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "energy") != tagsToWrite.end()) parts.push_back(energyStr);
    if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "key") != tagsToWrite.end()) parts.push_back(keyStr);

    if (parts.empty()) return;

    std::string newPrefix;
    for (size_t i = 0; i < parts.size(); ++i) {
        newPrefix += parts[i];
        if (i < parts.size() - 1) newPrefix += " | ";
    }

    try {
        TagLib::FileRef f(res.path.c_str());
        if (f.isNull() || !f.tag()) {
            log("ERROR", "Falha ao abrir/ler tags", res.filename);
            return;
        }

        TagLib::Tag *tag = f.tag();
        std::string currentAlbum = tag->album().toCString(true);
        std::string finalAlbum;

        if (force) {
            finalAlbum = newPrefix;
        } else {
            std::string cleaned = cleanAlbumPrefix(currentAlbum);
            if (cleaned.empty()) finalAlbum = newPrefix;
            else finalAlbum = newPrefix + " | " + cleaned;
        }

        tag->setAlbum(finalAlbum);

        if (TagLib::ID3v2::Tag *id3 = dynamic_cast<TagLib::ID3v2::Tag *>(tag)) {
            if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "bpm") != tagsToWrite.end()) {
                 TagLib::ByteVector frameId("TBPM");
                 if (!id3->frameList(frameId).isEmpty()) id3->removeFrames(frameId);
                 TagLib::ID3v2::TextIdentificationFrame *frame = new TagLib::ID3v2::TextIdentificationFrame(frameId);
                 frame->setText(bpmStr);
                 id3->addFrame(frame);
            }
            if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "key") != tagsToWrite.end()) {
                 TagLib::ByteVector frameId("TKEY");
                 if (!id3->frameList(frameId).isEmpty()) id3->removeFrames(frameId);
                 TagLib::ID3v2::TextIdentificationFrame *frame = new TagLib::ID3v2::TextIdentificationFrame(frameId);
                 frame->setText(keyStr);
                 id3->addFrame(frame);
            }
            if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "energy") != tagsToWrite.end()) {
                TagLib::ID3v2::FrameList txxxFrames = id3->frameList("TXXX");
                for (TagLib::ID3v2::Frame *frame : txxxFrames) {
                    if (TagLib::ID3v2::UserTextIdentificationFrame *txxx = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame *>(frame)) {
                        if (txxx->description() == "ENERGY") {
                            id3->removeFrame(frame);
                        }
                    }
                }
                TagLib::ID3v2::UserTextIdentificationFrame *txxx = new TagLib::ID3v2::UserTextIdentificationFrame();
                txxx->setDescription("ENERGY");
                txxx->setText(energyStr);
                id3->addFrame(txxx);
            }
        } else if (TagLib::Ogg::XiphComment *ogg = dynamic_cast<TagLib::Ogg::XiphComment *>(tag)) {
             if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "bpm") != tagsToWrite.end()) ogg->addField("BPM", bpmStr);
             if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "key") != tagsToWrite.end()) ogg->addField("INITIALKEY", keyStr);
             if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "energy") != tagsToWrite.end()) ogg->addField("ENERGY", energyStr);
        }

        if (f.save()) {
            log("SUCCESS", "Tags salvas: " + finalAlbum, res.filename);
        } else {
            log("ERROR", "Falha ao salvar tags", res.filename);
        }

    } catch (const std::exception& e) {
        log("ERROR", "Exceção ao salvar tags: " + std::string(e.what()), res.filename);
    }
}

// ==============================
// 📂 ARQUIVOS E DIRETÓRIOS
// ==============================

void findFiles(const fs::path& root, std::vector<std::string>& files, const ProgramArgs& args) {
    try {
        if (fs::is_regular_file(root)) {
            std::string ext = toLower(root.extension().string());
            if (std::find(args.extensions.begin(), args.extensions.end(), ext) != args.extensions.end()) {
                files.push_back(root.string());
            }
            return;
        }

        if (fs::is_directory(root)) {
            if (args.recursive) {
                for (const auto& entry : fs::recursive_directory_iterator(root)) {
                    if (entry.is_regular_file()) {
                        std::string ext = toLower(entry.path().extension().string());
                        if (std::find(args.extensions.begin(), args.extensions.end(), ext) != args.extensions.end()) {
                            files.push_back(entry.path().string());
                        }
                    }
                }
            } else {
                for (const auto& entry : fs::directory_iterator(root)) {
                    if (entry.is_regular_file()) {
                        std::string ext = toLower(entry.path().extension().string());
                        if (std::find(args.extensions.begin(), args.extensions.end(), ext) != args.extensions.end()) {
                            files.push_back(entry.path().string());
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        log("ERROR", "Erro ao escanear diretório: " + std::string(e.what()));
    }
}

// ==============================
// 📊 SAÍDA
// ==============================

void printTable(std::vector<AudioAnalysis>& results, const ProgramArgs& args) {
    if (results.empty()) {
        log("INFO", "Nenhum resultado para exibir após a filtragem.");
        return;
    }

    std::string key = args.sortBy.empty() ? "name" : args.sortBy[0];
    
    std::sort(results.begin(), results.end(), [&](const AudioAnalysis& a, const AudioAnalysis& b) {
        if (key == "bpm") return a.bpm < b.bpm;
        if (key == "energy") return a.energy < b.energy;
        if (key == "key") return a.keyCamelot < b.keyCamelot;
        if (key == "size") return (double)fs::file_size(a.path) < (double)fs::file_size(b.path);
        return a.filename < b.filename;
    });

    int wFile = 30, wBpm = 6, wEnergy = 6, wKey = 6, wSize = 8, wArtist = 20;
    
    std::cout << std::string(wFile + wBpm + wEnergy + wKey + wSize + wArtist + 16, '-') << "\n";
    std::cout << "| " << std::left << std::setw(wFile) << "ARQUIVO"
              << " | " << std::setw(wBpm) << "BPM"
              << " | " << std::setw(wEnergy) << "ENERG"
              << " | " << std::setw(wKey) << "KEY"
              << " | " << std::setw(wSize) << "TAM(MB)"
              << " | " << std::setw(wArtist) << "ARTISTA" << " |\n";
    std::cout << std::string(wFile + wBpm + wEnergy + wKey + wSize + wArtist + 16, '-') << "\n";

    for (const auto& res : results) {
        std::string keyOut = res.keyCamelot.empty() || res.keyCamelot == "??? " ? "N/A" : res.keyCamelot;
        std::string bpmOut = (res.bpm > 0.1) ? (std::to_string((int)std::round(res.bpm))) : "N/A";
        std::string energyOut;
        if (res.energy > 0.01) {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(2) << res.energy;
            energyOut = ss.str();
        } else {
            energyOut = "N/A";
        }
        
        double sizeMB = 0.0;
        try { sizeMB = (double)fs::file_size(res.path) / (1024.0 * 1024.0); } catch(...) {}

        // Read metadata for display if not already populated (Amalyzer doesn't do it by default)
        std::string artist = "Unknown";
        try {
            TagLib::FileRef f(res.path.c_str());
            if (!f.isNull() && f.tag()) {
                artist = f.tag()->artist().toCString(true);
            }
        } catch(...) {}

        std::cout << "| " << std::left << std::setw(wFile) << truncate(res.filename, wFile)
                  << " | " << std::right << std::setw(wBpm) << bpmOut
                  << " | " << std::right << std::setw(wEnergy) << energyOut
                  << " | " << std::setw(wKey) << keyOut
                  << " | " << std::setw(wSize) << std::fixed << std::setprecision(2) << sizeMB
                  << " | " << std::left << std::setw(wArtist) << truncate(artist, wArtist) << " |\n";
    }
    std::cout << std::string(wFile + wBpm + wEnergy + wKey + wSize + wArtist + 16, '-') << "\n";
}

void saveCsv(const std::vector<AudioAnalysis>& results, const std::string& filename) {
    std::ofstream f(filename);
    if (!f.is_open()) {
        log("ERROR", "Não foi possível criar o arquivo CSV: " + filename);
        return;
    }
    
    f << "filename,path,bpm,energy,key_camelot,duration_sec,file_size_mb\n";
    
    auto csv_quote = [](const std::string& s) {
        std::string temp = s;
        std::string result;
        result.reserve(temp.length() + 2);
        for (char c : temp) {
            if (c == '"') result += '"';
            result += c;
        }
        return "\"" + result + "\"";
    };

    for (const auto& r : results) {
        double sizeMB = 0.0;
        try { sizeMB = (double)fs::file_size(r.path) / (1024.0 * 1024.0); } catch(...) {}

        f << csv_quote(r.filename) << ","
          << csv_quote(r.path) << ","
          << r.bpm << ","
          << r.energy << ","
          << csv_quote(r.keyCamelot) << ","
          << r.durationSec << ","
          << sizeMB << "\n";
    }
    log("SUCCESS", "CSV salvo em: " + filename);
}

// ==============================
// 🚀 MAIN
// ==============================

int main(int argc, char* argv[]) {
    ProgramArgs args;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-r") args.recursive = true;
        else if (arg == "-csv") args.csv = true;
        else if (arg == "-q") { args.quiet = true; IS_SILENT = true; }
        else if (arg == "-l" || arg == "--list") args.listMode = true;
        else if (arg == "-put-force") args.putForce = true;
        else if (arg == "-o" && i + 1 < argc) args.outputFile = argv[++i];
        else if (arg == "-min" && i + 1 < argc) { 
            try { args.minDuration = std::stod(argv[++i]); } catch (const std::invalid_argument&) {}
        }
        else if (arg == "-max" && i + 1 < argc) {
            try { args.maxDuration = std::stod(argv[++i]); } catch (const std::invalid_argument&) {}
        }
        else if (arg == "-bpm-min" && i + 1 < argc) {
            try { args.minBpm = std::stod(argv[++i]); } catch (const std::invalid_argument&) {}
        }
        else if (arg == "-bpm-max" && i + 1 < argc) {
            try { args.maxBpm = std::stod(argv[++i]); } catch (const std::invalid_argument&) {}
        }
        else if (arg == "-key" && i + 1 < argc) args.targetKey = argv[++i];
        else if (arg == "-ext" && i + 1 < argc) {
            std::string extList = argv[++i];
            std::stringstream ss(extList);
            std::string item;
            args.extensions.clear();
            while (std::getline(ss, item, ',')) {
                if (item[0] != '.') item = "." + item;
                args.extensions.push_back(toLower(item));
            }
        }
        else if (arg == "-put" && i + 1 < argc) {
            std::string tags = argv[++i];
            std::stringstream ss(tags);
            std::string item;
            while (std::getline(ss, item, ',')) args.tagsToWrite.push_back(toLower(item));
        }
        else if (arg == "-sort" && i + 1 < argc) {
            std::string sort = argv[++i];
            std::stringstream ss(sort);
            std::string item;
            args.sortBy.clear();
            while (std::getline(ss, item, ',')) args.sortBy.push_back(toLower(item));
        }
        else if (arg[0] != '-') args.paths.push_back(arg);
    }

    if (args.paths.empty()) {
        std::cout << "Uso: " << argv[0] << " [opções] <arquivos/pastas>\n";
        return 1;
    }

      Superpowered::Initialize("ExampleLicenseKey-WillExpire-OnNextUpdate");
    Amalyzer amalyzer;

    std::vector<std::string> files;
    for (const auto& p : args.paths) {
        findFiles(p, files, args);
    }

    if (files.empty()) {
        log("WARNING", "Nenhum arquivo encontrado.");
        return 0;
    }

    log("INFO", "Iniciando análise de " + std::to_string(files.size()) + " arquivos...");

    std::vector<AudioAnalysis> results;
    int processedCount = 0;
    int totalFiles = files.size();

    // Single-threaded execution
    for (const auto& fpath : files) {
        AudioAnalysis res;
        
        if (args.listMode) {
            res.path = fpath;
            res.filename = fs::path(fpath).filename().string();
            res.success = true; // Assume success for list mode
        } else {
            res = amalyzer.analyze(fpath);
        }

        if (res.success) {
            bool keep = true;
            if (args.minBpm > 0 && res.bpm < args.minBpm) keep = false;
            if (args.maxBpm > 0 && res.bpm > args.maxBpm) keep = false;
            if (args.minDuration > 0 && res.durationSec < args.minDuration) keep = false;
            if (args.maxDuration > 0 && res.durationSec > args.maxDuration) keep = false;
            if (!args.targetKey.empty() && toLower(res.keyCamelot) != toLower(args.targetKey)) keep = false;

            if (keep) {
                results.push_back(res);
            }
        } else {
            log("ERROR", "Falha: " + res.errorMessage, res.filename);
        }

        processedCount++;
        if (!args.quiet) {
            std::cout << "\r[" << processedCount << "/" << totalFiles << "] " << truncate(res.filename, 40) << "      " << std::flush;
        }
    }

    if (!args.quiet) std::cout << "\n";

    if (!args.tagsToWrite.empty() && !args.listMode) {
        log("INFO", "Escrevendo tags...");
        for (const auto& res : results) {
            if (res.bpm > 0.1 || res.energy > 0.01) {
                writeTags(res, args.tagsToWrite, args.putForce);
            }
        }
    }

    if (args.csv) {
        std::string out = args.outputFile.empty() ? "analysis_results.csv" : args.outputFile;
        saveCsv(results, out);
    } else {
        printTable(results, args);
    }

    return 0;
}