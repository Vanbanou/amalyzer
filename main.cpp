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
// 🎨 CONSTANTES COMPACTAS
// ==============================

const std::string RESET = "\033[0m";
const std::string RED = "\033[91m";
const std::string GREEN = "\033[92m";
const std::string YELLOW = "\033[93m";
const std::string BLUE = "\033[94m";
const std::string CYAN = "\033[96m";
const std::string MAGENTA = "\033[95m";
const std::string BOLD = "\033[1m";
const std::string DIM = "\033[2m";

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
    
    // Filters
    double minBpm = 0.0;
    double maxBpm = 0.0;
    double minSizeMB = 0.0;
    double maxSizeMB = 0.0;
    std::string targetKey = "";
    int limit = 0;

    // Output/Action
    std::vector<std::string> tagsToWrite;
    bool putForce = false;
    bool listMode = false;
    bool meta = false;
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

// Barra de progresso compacta
void drawProgressBar(int current, int total, const std::string& currentFile) {
    if (IS_SILENT) return;
    
    int barWidth = 30; // Reduzido para mobile
    float progress = (float)current / total;
    int pos = barWidth * progress;
    
    std::cout << "\r" << CYAN << "[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << GREEN << "█";
        else std::cout << DIM << "░";
    }
    std::cout << CYAN << "]" << RESET;
    
    // Informações compactas
    std::cout << " " << BOLD << (int)(progress * 100.0) << "%";
    std::cout << " " << CYAN << "(" << current << "/" << total << ")" << RESET;
    
    // Nome do arquivo muito curto
    if (!currentFile.empty()) {
        std::cout << " " << truncate(fs::path(currentFile).filename().string(), 15);
    }
    std::flush(std::cout);
}

// Logs compactos
void log(const std::string& level, const std::string& message, const std::string& detail = "") {
    if (IS_SILENT && level != "ERROR") return;

    std::string prefix;
    if (level == "INFO") prefix = BLUE + "ℹ ";
    else if (level == "WARNING") prefix = YELLOW + "⚠ ";
    else if (level == "ERROR") prefix = RED + "✗ ";
    else if (level == "SUCCESS") prefix = GREEN + "✓ ";
    else prefix = "";

    if (level == "ERROR") {
        std::cerr << prefix << RESET << message;
        if (!detail.empty()) std::cerr << " " << DIM << "(" << detail << ")" << RESET;
        std::cerr << std::endl;
    } else {
        std::cout << prefix << RESET << message;
        if (!detail.empty()) std::cout << " " << DIM << "(" << detail << ")" << RESET;
        std::cout << std::endl;
    }
}

// ==============================
// 💾 JSON / METADATA
// ==============================

std::string escapeJsonString(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (c >= 0 && c <= 0x1f) {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    o << c;
                }
        }
    }
    return o.str();
}

void saveMetadataFile(const AudioAnalysis& data) {
    if (data.path.empty()) return;

    fs::path p(data.path);
    fs::path jsonPath = p.parent_path() / (p.stem().string() + p.extension().string() + ".analisemetadata");
    
    log("INFO", "Salvando metadata: " + p.filename().string());

    std::ofstream f(jsonPath);
    if (!f.is_open()) {
        log("ERROR", "Erro ao salvar metadata", jsonPath.string());
        return;
    }

    f << "{\n";
    f << "  \"filename\": \"" << escapeJsonString(data.filename) << "\",\n";
    f << "  \"title\": \"" << escapeJsonString(data.title) << "\",\n";
    f << "  \"artist\": \"" << escapeJsonString(data.artist) << "\",\n";
    f << "  \"album\": \"" << escapeJsonString(data.album) << "\",\n";
    f << "  \"genre\": \"" << escapeJsonString(data.genre) << "\",\n";
    f << "  \"bpm\": " << std::fixed << std::setprecision(2) << data.bpm << ",\n";
    f << "  \"key\": \"" << escapeJsonString(data.keyCamelot) << "\",\n";
    f << "  \"energy\": " << std::fixed << std::setprecision(2) << data.energy << ",\n";
    f << "  \"length\": " << std::fixed << std::setprecision(2) << data.durationSec << ",\n";
    f << "  \"size_mb\": " << std::fixed << std::setprecision(2) << data.fileSizeMB << "\n";
    f << "}";
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
            log("ERROR", "Erro ao abrir arquivo", res.filename);
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

        // Configurar frames ID3v2
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
            log("SUCCESS", "Tags salvas: " + truncate(finalAlbum, 40), res.filename);
        } else {
            log("ERROR", "Erro ao salvar tags", res.filename);
        }

    } catch (const std::exception& e) {
        log("ERROR", "Exceção: " + std::string(e.what()), res.filename);
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
        log("ERROR", "Erro ao escanear: " + std::string(e.what()));
    }
}

// ==============================
// 📊 SAÍDA COMPACTA
// ==============================

void printCompactTable(std::vector<AudioAnalysis>& results, const ProgramArgs& args) {
    if (args.quiet || results.empty()) return;

    // Cabeçalho minimalista
    if (!IS_SILENT) {
        if (args.listMode) {
            std::cout << CYAN << "📁 LISTA (" << results.size() << " arquivos)" << RESET << "\n";
        } else {
            std::cout << GREEN << "📊 ANÁLISE (" << results.size() << " arquivos)" << RESET << "\n";
        }
        std::cout << std::string(60, '-') << "\n";
    }

    // Linhas compactas
    for(const auto& res : results) {
        if (args.listMode) {
            // Modo lista: nome + metadata básica
            std::cout << truncate(res.filename, 30);
            if (!res.title.empty()) {
                std::cout << " - " << truncate(res.title, 20);
            }
            std::cout << " " << DIM << std::fixed << std::setprecision(1) << res.fileSizeMB << "MB" << RESET;
            std::cout << "\n";
        } else {
            // Modo análise: tudo em uma linha
            // Nome do arquivo
            std::cout << truncate(res.filename, 20);
            
            // BPM
            if(res.bpm >= 0.1) {
                std::cout << " " << GREEN << std::setw(3) << (int)res.bpm << "bpm" << RESET;
            } else {
                std::cout << " " << DIM << "---" << RESET;
            }
            
            // Key
            if(!res.keyCamelot.empty() && res.keyCamelot != "???") {
                std::cout << " " << YELLOW << res.keyCamelot << RESET;
            }
            
            // Energy (apenas se significativa)
            if(res.energy >= 0.1) {
                std::cout << " E:" << std::fixed << std::setprecision(1) << res.energy;
            }
            
            // Tamanho
            std::cout << " " << DIM << std::fixed << std::setprecision(1) << res.fileSizeMB << "M" << RESET;
            
            std::cout << "\n";
        }
    }
    
    // Rodapé minimalista
    if (!IS_SILENT) {
        std::cout << std::string(60, '-') << "\n";
    }
}

void saveCsv(const std::vector<AudioAnalysis>& results, const std::string& filename) {
    std::ofstream f(filename);
    if (!f.is_open()) {
        log("ERROR", "Erro ao criar CSV: " + filename);
        return;
    }
    
    // Cabeçalho compacto
    f << "arquivo,caminho,bpm,energy,key,tempo,tamanho,titulo,artista,album\n";
    
    for (const auto& r : results) {
        f << "\"" << r.filename << "\","
          << "\"" << r.path << "\","
          << std::fixed << std::setprecision(2) << r.bpm << ","
          << std::fixed << std::setprecision(2) << r.energy << ","
          << "\"" << r.keyCamelot << "\","
          << std::fixed << std::setprecision(2) << r.durationSec << ","
          << std::fixed << std::setprecision(2) << r.fileSizeMB << ","
          << "\"" << r.title << "\","
          << "\"" << r.artist << "\","
          << "\"" << r.album << "\"\n";
    }
    log("SUCCESS", "CSV salvo: " + filename);
}

// ==============================
// ❓ HELP COMPACTO
// ==============================

void printHelp(const char* progName) {
    std::cout << "🎵 Amalyzer - Analisador de Áudio (Mobile)\n\n"
              << "Uso: " << progName << " [opções] <arquivos/pastas>\n\n"
              << "Opções principais:\n"
              << "  -r          Busca recursiva\n"
              << "  -q          Silencioso\n"
              << "  -l          Lista rápida\n"
              << "  -csv        Saída CSV\n"
              << "  -o <file>   Salvar saída\n"
              << "  -meta       Gerar metadata\n"
              << "  -limit N    Limitar arquivos\n\n"
              << "Filtros:\n"
              << "  -bpm-min N  BPM mínimo\n"
              << "  -bpm-max N  BPM máximo\n"
              << "  -size-min N Tamanho min (MB)\n"
              << "  -size-max N Tamanho max (MB)\n"
              << "  -key K      Key Camelot\n\n"
              << "Tags:\n"
              << "  -put bpm,energy,key  Escrever tags\n"
              << "  -put-force           Forçar escrita\n"
              << "  -sort campo          Ordenar\n\n"
              << "Ex: " << progName << " -r -put bpm,key ./musicas\n";
}

// ==============================
// 🚀 MAIN (OTIMIZADA)
// ==============================

int main(int argc, char* argv[]) {
    ProgramArgs args;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp(argv[0]);
            return 0;
        }
        else if (arg == "-r") args.recursive = true;
        else if (arg == "-csv") args.csv = true;
        else if (arg == "-q") { args.quiet = true; IS_SILENT = true; }
        else if (arg == "-l" || arg == "--list") args.listMode = true;
        else if (arg == "-put-force") args.putForce = true;
        else if (arg == "-meta") args.meta = true;
        else if (arg == "-o" && i + 1 < argc) args.outputFile = argv[++i];
        else if (arg == "-limit" && i + 1 < argc) {
             try { args.limit = std::stoi(argv[++i]); } catch (...) {}
        }
        else if (arg == "-size-min" && i + 1 < argc) {
             try { args.minSizeMB = std::stod(argv[++i]); } catch (...) {}
        }
        else if (arg == "-size-max" && i + 1 < argc) {
             try { args.maxSizeMB = std::stod(argv[++i]); } catch (...) {}
        }
        else if (arg == "-bpm-min" && i + 1 < argc) {
            try { args.minBpm = std::stod(argv[++i]); } catch (...) {}
        }
        else if (arg == "-bpm-max" && i + 1 < argc) {
            try { args.maxBpm = std::stod(argv[++i]); } catch (...) {}
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
        printHelp(argv[0]);
        return 1;
    }

    // Inicialização minimalista
    if (!IS_SILENT) {
        #ifndef _WIN32
        system("clear");
        #endif
        std::cout << GREEN << "🎵 AMALYZER" << RESET << " (mobile)\n";
    }

    Superpowered::Initialize("ExampleLicenseKey-WillExpire-OnNextUpdate");
    Amalyzer amalyzer;

    // Encontrar arquivos
    std::vector<std::string> files;
    for (const auto& p : args.paths) {
        findFiles(p, files, args);
    }

    if (files.empty()) {
        log("INFO", "Nenhum arquivo encontrado.");
        return 0;
    }

    if (args.limit > 0 && files.size() > (size_t)args.limit) {
        files.resize(args.limit);
    }

    if (!IS_SILENT) {
        std::cout << CYAN << "Arquivos: " << files.size() << RESET << "\n";
    }

    if (args.listMode) {
        log("INFO", "Listando...");
    } else {
        log("INFO", "Analisando...");
    }

    // Processamento
    std::vector<AudioAnalysis> results;
    int processedCount = 0;
    int totalFiles = files.size();

    for (const auto& fpath : files) {
        AudioAnalysis res;
        res.path = fpath;
        res.filename = fs::path(fpath).filename().string();
        
        // Tamanho do arquivo
        try {
            res.fileSizeMB = (double)fs::file_size(fpath) / (1024.0 * 1024.0);
        } catch(...) { res.fileSizeMB = 0.0; }

        // Metadata
        try {
            TagLib::FileRef f(fpath.c_str());
            if (!f.isNull() && f.tag()) {
                res.title = f.tag()->title().toCString(true);
                res.artist = f.tag()->artist().toCString(true);
                res.album = f.tag()->album().toCString(true);
                res.genre = f.tag()->genre().toCString(true);
                if (f.audioProperties() && args.listMode) {
                    res.durationSec = f.audioProperties()->lengthInSeconds();
                }
            }
        } catch(...) {}

        if (!args.listMode) {
            // Análise
            AudioAnalysis analysis = amalyzer.analyze(fpath);
            if (analysis.success) {
                res.bpm = analysis.bpm;
                res.energy = analysis.energy;
                res.keyCamelot = analysis.keyCamelot;
                res.keyIndex = analysis.keyIndex;
                res.durationSec = analysis.durationSec;
                res.success = true;
            } else {
                res.success = false;
                res.errorMessage = analysis.errorMessage;
            }
        } else {
            res.success = true;
        }

        if (res.success) {
            // Aplicar filtros
            bool keep = true;
            if (args.minBpm > 0 && res.bpm < args.minBpm) keep = false;
            if (args.maxBpm > 0 && res.bpm > args.maxBpm) keep = false;
            if (args.minSizeMB > 0 && res.fileSizeMB < args.minSizeMB) keep = false;
            if (args.maxSizeMB > 0 && res.fileSizeMB > args.maxSizeMB) keep = false;
            if (!args.targetKey.empty() && toLower(res.keyCamelot) != toLower(args.targetKey)) keep = false;

            if (keep) results.push_back(res);
        } else if (!args.listMode) {
            log("ERROR", "Falha: " + res.errorMessage, res.filename);
        }

        processedCount++;
        if (!args.quiet) {
            drawProgressBar(processedCount, totalFiles, res.filename);
        }
    }

    if (!args.quiet) std::cout << "\n";

    if (results.empty()) {
        log("INFO", "Nenhum resultado após filtros.");
        return 0;
    }

    // Escrever tags
    if (!args.tagsToWrite.empty() && !args.listMode) {
        log("INFO", "Escrevendo tags...");
        for (const auto& res : results) {
            writeTags(res, args.tagsToWrite, args.putForce);
        }
    }

    // Gerar metadata
    if (args.meta && !args.listMode) {
        log("INFO", "Gerando metadata...");
        for (const auto& res : results) {
            saveMetadataFile(res);
        }
    }

    // Ordenar
    for (auto it = args.sortBy.rbegin(); it != args.sortBy.rend(); ++it) {
        std::string key = *it;
        std::stable_sort(results.begin(), results.end(), [&](const AudioAnalysis& a, const AudioAnalysis& b) {
            if (key == "bpm") return a.bpm < b.bpm;
            if (key == "energy") return a.energy < b.energy;
            if (key == "key") return a.keyCamelot < b.keyCamelot;
            if (key == "size") return a.fileSizeMB < b.fileSizeMB;
            if (key == "album") return a.album < b.album;
            if (key == "artist") return a.artist < b.artist;
            if (key == "title") return a.title < b.title;
            return a.filename < b.filename;
        });
    }

    // Saída
    if (args.csv) {
        std::string out = args.outputFile.empty() ? "resultado.csv" : args.outputFile;
        saveCsv(results, out);
    } else {
        printCompactTable(results, args);
        
        // Resumo minimalista
        if (!IS_SILENT) {
            std::cout << GREEN << "✓ Pronto: " << results.size() << " arquivos" << RESET << "\n";
        }
    }

    return 0;
}