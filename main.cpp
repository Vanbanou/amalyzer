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
const std::string CYAN = "\033[96m";
const std::string MAGENTA = "\033[95m";
const std::string BOLD = "\033[1m";
const std::string DIM = "\033[2m";
const std::string BG_BLUE = "\033[44m";
const std::string BG_GREEN = "\033[42m";

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

std::string repeatString(const std::string& str, int count) {
    std::string result;
    result.reserve(str.length() * count);
    for (int i = 0; i < count; ++i) {
        result += str;
    }
    return result;
}

std::string toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

// Otimizada para ser mais curta e usar menos espaço horizontal
void drawProgressBar(int current, int total, const std::string& currentFile) {
    if (IS_SILENT) return;
    
    int barWidth = 30; // Reduzido
    float progress = (float)current / total;
    int pos = barWidth * progress;
    
    std::cout << "\r" << CYAN << "[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << GREEN << "█" << CYAN;
        else if (i == pos) std::cout << GREEN << "█" << CYAN;
        else std::cout << DIM << "░" << CYAN;
    }
    std::cout << "]" << RESET;
    
    // Percentage e Counter combinados para economizar espaço
    std::cout << " " << BOLD << GREEN << std::setw(3) << (int)(progress * 100.0) << "%" << RESET;
    std::cout << " " << DIM << "(" << current << "/" << total << ")" << RESET;
    
    // Current file truncado drasticamente
    std::cout << " " << DIM << truncate(fs::path(currentFile).filename().string(), 20) << RESET << "      " << std::flush;
}

// Log mais direto
void log(const std::string& level, const std::string& message, const std::string& detail = "") {
    if (IS_SILENT && level != "ERROR") return;

    if (level == "INFO") std::cout << BLUE << "[ℹ️] " << RESET;
    else if (level == "WARNING") std::cout << YELLOW << "[⚠️] " << RESET;
    else if (level == "ERROR") std::cerr << RED << "[🔥] " << RESET;
    else if (level == "SUCCESS") std::cout << GREEN << "[✅] " << RESET;

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
// 💾 JSON / METADATA
// ==============================

std::string escapeJsonString(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
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
    
    log("INFO", "Gerando metadados: " + jsonPath.filename().string(), "💾");

    std::ofstream f(jsonPath);
    if (!f.is_open()) {
        log("ERROR", "Erro ao salvar .analisemetadata", jsonPath.string());
        return;
    }

    f << "{\n";
    f << "    \"filename\": \"" << escapeJsonString(data.filename) << "\",\n";
    f << "    \"title\": \"" << escapeJsonString(data.title) << "\",\n";
    f << "    \"artist\": \"" << escapeJsonString(data.artist) << "\",\n";
    f << "    \"album\": \"" << escapeJsonString(data.album) << "\",\n";
    f << "    \"genre\": \"" << escapeJsonString(data.genre) << "\",\n";
    f << "    \"bpm\": " << std::fixed << std::setprecision(2) << data.bpm << ",\n";
    f << "    \"bpm_confidence\": 0.0,\n"; // Placeholder
    f << "    \"key\": \"" << escapeJsonString(data.keyCamelot) << "\",\n"; // Using Camelot as key for simplicity or extract real key if needed
    f << "    \"scale\": \"\",\n"; 
    f << "    \"key_camelot\": \"" << escapeJsonString(data.keyCamelot) << "\",\n";
    f << "    \"key_strength\": 0.0,\n";
    f << "    \"energy\": " << std::fixed << std::setprecision(2) << data.energy << ",\n";
    f << "    \"length_sec\": " << std::fixed << std::setprecision(6) << data.durationSec << ",\n";
    f << "    \"file_size_mb\": " << std::fixed << std::setprecision(2) << data.fileSizeMB << ",\n";
    f << "    \"sample_rate\": " << data.sampleRate << ",\n";
    f << "    \"channels\": " << data.channels << ",\n";
    f << "    \"bitrate\": " << data.bitrate << "\n";
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
// 📊 SAÍDA (Otimizada para Termux)
// ==============================

void printTable(std::vector<AudioAnalysis>& results, const ProgramArgs& args) {
    if (args.quiet) return;

    if (results.empty()) {
        log("INFO", "Nenhum resultado para exibir.");
        return;
    }

    if (args.listMode) {
        // Modo de listagem compacta: [BPM KEY] Título/Nome do Arquivo (Artista) [TAM MB]
        // Prioridade para Título e Artista, fallback para Nome do Arquivo
        for(const auto& res : results) {
            std::string mainInfo = res.title.empty() ? res.filename : res.title;
            
            // Info principal
            std::cout << BOLD << truncate(mainInfo, 40) << RESET;

            // Artista (entre parênteses e dim)
            if (!res.artist.empty()) {
                std::cout << DIM << " (" << truncate(res.artist, 15) << ")" << RESET;
            }

            // Tamanho (no final da linha)
            std::cout << " [" << std::fixed << std::setprecision(1) << res.fileSizeMB << "MB]";
            std::cout << "\n";
        }
    } else {
        // Modo de análise compacta: [BPM | KEY | E:Energy] Nome_do_Arquivo [TAM MB]
        // Uma linha por arquivo, maximizando a informação por linha.
        for(const auto& res : results) {
            std::cout << BOLD << truncate(res.filename, 25) << RESET << " "; // Nome do arquivo

            // Bloco de análise [BPM KEY E:ENERGY]
            std::cout << CYAN << "[";
            
            // BPM
            if(res.bpm >= 0.1) {
                std::cout << GREEN << (int)std::round(res.bpm) << "bpm" << RESET;
            } else {
                std::cout << DIM << "---" << RESET;
            }
            
            // Key
            if(!res.keyCamelot.empty() && res.keyCamelot != "???") {
                std::cout << " | " << YELLOW << res.keyCamelot << RESET;
            }

            // Energy
            if(res.energy >= 0.01) {
                std::cout << " | E:" << std::fixed << std::setprecision(2) << res.energy;
            }
            
            std::cout << CYAN << "]" << RESET;
            
            // Tamanho
            std::cout << DIM << " [" << std::fixed << std::setprecision(1) << res.fileSizeMB << "MB]" << RESET;
            
            std::cout << "\n";
        }
    }
}

void saveCsv(const std::vector<AudioAnalysis>& results, const std::string& filename) {
    std::ofstream f(filename);
    if (!f.is_open()) {
        log("ERROR", "Não foi possível criar o arquivo CSV: " + filename);
        return;
    }
    
    // Match Python columns: filename,path,bpm,energy,key_camelot,key,scale,length_sec,file_size_mb,title,artist,album,genre
    f << "filename,path,bpm,energy,key_camelot,key,scale,length_sec,file_size_mb,title,artist,album,genre\n";
    
    auto csv_quote = [](const std::string& s) {
        std::string temp = s;
        std::string result;
        result.reserve(temp.length() + 2);
        for (char c : temp) {
            if (c == '"') result += "\"\""; // CSV escape
            else result += c;
        }
        return "\"" + result + "\"";
    };

    for (const auto& r : results) {
        f << csv_quote(r.filename) << ","
          << csv_quote(r.path) << ","
          << std::fixed << std::setprecision(2) << r.bpm << ","
          << std::fixed << std::setprecision(2) << r.energy << ","
          << csv_quote(r.keyCamelot) << ","
          << csv_quote(r.keyCamelot) << "," // key
          << "," // scale (empty)
          << std::fixed << std::setprecision(2) << r.durationSec << ","
          << std::fixed << std::setprecision(2) << r.fileSizeMB << ","
          << csv_quote(r.title) << ","
          << csv_quote(r.artist) << ","
          << csv_quote(r.album) << ","
          << csv_quote(r.genre) << "\n";
    }
    log("SUCCESS", "CSV salvo em: " + filename);
}

// ==============================
// ❓ HELP (Otimizada para Termux)
// ==============================

void printHelp(const char* progName) {
    std::cout << BOLD << YELLOW << "🎵 Amalyzer - CLI de Análise de Áudio\n\n" << RESET
              << BOLD << "Uso: " << progName << " [OPÇÕES] <ARQ/PASTAS>\n\n" << RESET
              << BOLD << "OPÇÕES GERAIS:\n" << RESET
              << " -r         Busca recursiva.\n"
              << " -q         Modo silencioso (sem progresso/tabela).\n"
              << " -l, --list Modo de listagem (só metadados).\n"
              << " -csv       Saída em formato CSV.\n"
              << " -o <arq>   Salvar saída em arquivo.\n"
              << " -meta      Cria .analisemetadata (JSON) por áudio.\n"
              << " -limit <N> Limita a análise aos primeiros N arquivos.\n\n"
              << BOLD << "FILTROS:\n" << RESET
              << " -bpm-min/max <N> Filtra por BPM.\n"
              << " -size-min/max <N> Filtra por Tamanho (MB).\n"
              << " -key <K>   Filtra por key Camelot (ex: '8B').\n"
              << " -ext <list> Extensões (ex: mp3,flac).\n\n"
              << BOLD << "AÇÕES/SAÍDA:\n" << RESET
              << " -sort <list> Ordena (ex: 'bpm,size'). Campos: name|bpm|size|key|energy|album|artist|title\n"
              << " -put <list> Escreve tags no arquivo (bpm|energy|key).\n"
              << " -put-force  FORÇA sobrescrita do campo Álbum.\n\n"
              << DIM << "Exemplo: " << progName << " -r -put bpm,key -sort bpm ./musicas" << RESET << "\n";
}

// ==============================
// 🚀 MAIN
// ==============================

int main(int argc, char* argv[]) {
    ProgramArgs args;
    
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

    if (!IS_SILENT) {
        #ifdef _WIN32
        system("cls");
        #else
        system("clear");
        #endif
    }

    Superpowered::Initialize("ExampleLicenseKey-WillExpire-OnNextUpdate");
    Amalyzer amalyzer;

    std::vector<std::string> files;
    for (const auto& p : args.paths) {
        findFiles(p, files, args);
    }

    if (files.empty()) {
        log("INFO", "Nenhum arquivo de áudio encontrado.");
        return 0;
    }

    // Limit files
    if (args.limit > 0 && files.size() > (size_t)args.limit) {
        files.resize(args.limit);
    }

    if (!IS_SILENT) {
        // Banner mais compacto
        std::cout << BOLD << CYAN << "─" << RESET << BOLD << YELLOW << " AMALYZER " << RESET << DIM << "(" << files.size() << " arquivos)" << RESET << "\n";
        std::cout << std::string(35, '-') << "\n";
    }
    
    if (args.listMode) {
        log("INFO", "Listagem rápida (sem análise)...", "🎵");
    } else {
        log("INFO", "Iniciando análise...", "🚀");
    }

    std::vector<AudioAnalysis> results;
    int processedCount = 0;
    int totalFiles = files.size();

    for (const auto& fpath : files) {
        AudioAnalysis res;
        res.path = fpath;
        res.filename = fs::path(fpath).filename().string();
        
        // Get File Size
        try {
            res.fileSizeMB = (double)fs::file_size(fpath) / (1024.0 * 1024.0);
        } catch(...) { res.fileSizeMB = 0.0; }

        // Read Metadata (TagLib)
        try {
            TagLib::FileRef f(fpath.c_str());
            if (!f.isNull() && f.tag()) {
                res.title = f.tag()->title().toCString(true);
                res.artist = f.tag()->artist().toCString(true);
                res.album = f.tag()->album().toCString(true);
                res.genre = f.tag()->genre().toCString(true);
                if (f.audioProperties()) {
                    res.bitrate = f.audioProperties()->bitrate();
                    res.sampleRate = f.audioProperties()->sampleRate();
                    res.channels = f.audioProperties()->channels();
                    if (args.listMode) res.durationSec = f.audioProperties()->lengthInSeconds();
                }
            }
        } catch(...) {}

        if (!args.listMode) {
            // Perform Audio Analysis
            AudioAnalysis analysis = amalyzer.analyze(fpath);
            if (analysis.success) {
                res.bpm = analysis.bpm;
                res.energy = analysis.energy;
                res.keyCamelot = analysis.keyCamelot;
                res.keyIndex = analysis.keyIndex;
                res.durationSec = analysis.durationSec; // More precise from decoder
                res.success = true;
            } else {
                res.success = false;
                res.errorMessage = analysis.errorMessage;
            }
        } else {
            res.success = true; // List mode always succeeds if file exists
        }

        if (res.success) {
            bool keep = true;
            if (args.minBpm > 0 && res.bpm < args.minBpm) keep = false;
            if (args.maxBpm > 0 && res.bpm > args.maxBpm) keep = false;
            if (args.minSizeMB > 0 && res.fileSizeMB < args.minSizeMB) keep = false;
            if (args.maxSizeMB > 0 && res.fileSizeMB > args.maxSizeMB) keep = false;
            if (!args.targetKey.empty() && toLower(res.keyCamelot) != toLower(args.targetKey)) keep = false;

            if (keep) {
                results.push_back(res);
            }
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
        log("INFO", "Nenhum arquivo permaneceu após a filtragem.");
        return 0;
    }

    // Write Tags
    if (!args.tagsToWrite.empty() && !args.listMode) {
        log("INFO", "Iniciando escrita de Tags...", "📝");
        for (const auto& res : results) {
             writeTags(res, args.tagsToWrite, args.putForce);
        }
        if (!IS_SILENT) std::cout << std::string(35, '-') << "\n"; // Linha menor
    }

    // Generate Meta
    if (args.meta && !args.listMode) {
        log("INFO", "Gerando arquivos .analisemetadata", "📝");
        for (const auto& res : results) {
            saveMetadataFile(res);
        }
    }

    // Sort Results
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
            return a.filename < b.filename; // Default 'name'
        });
    }

    // Output
    if (args.csv) {
        std::string out = args.outputFile.empty() ? "analysis_results.csv" : args.outputFile;
        saveCsv(results, out);
    } else {
        if (!IS_SILENT) {
            std::cout << "\n";
            if (args.listMode) {
                std::cout << BOLD << MAGENTA << "--- LISTAGEM DE ARQUIVOS ---\n" << RESET;
            } else {
                std::cout << BOLD << MAGENTA << "--- RESULTADOS DA ANÁLISE ---\n" << RESET;
            }
            std::cout << std::string(35, '-') << "\n";
        }
        printTable(results, args);
        
        // Summary
        if (!IS_SILENT) {
            std::cout << "\n" << BOLD << GREEN << "✓ " << RESET << "Total: " << BOLD << results.size() << RESET << "\n";
        }
    }

    return 0;
}