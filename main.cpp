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
// 🎨 CONSTANTES OTIMIZADAS
// ==============================

const std::string R = "\033[0m";
const std::string RED = "\033[91m";
const std::string GRN = "\033[92m";
const std::string YEL = "\033[93m";
const std::string BLU = "\033[94m";
const std::string CYN = "\033[96m";
const std::string MAG = "\033[95m";
const std::string B = "\033[1m";
const std::string DIM = "\033[2m";

bool IS_SILENT = false;

// ==============================
// 🎵 ESTRUTURAS
// ==============================

struct ProgramArgs {
    std::vector<std::string> paths;
    bool recursive = false;
    std::vector<std::string> extensions = {".mp3", ".flac", ".ogg", ".wav", ".m4a", ".aif", ".aiff"};
    bool csv = false;
    std::string outputFile;
    bool quiet = false;
    
    double minBpm = 0.0;
    double maxBpm = 0.0;
    double minSizeMB = 0.0;
    double maxSizeMB = 0.0;
    std::string targetKey = "";
    int limit = 0;

    std::vector<std::string> tagsToWrite;
    bool putForce = false;
    bool listMode = false;
    bool meta = false;
    std::vector<std::string> sortBy = {"name"};
};

// ==============================
// 🛠️ FUNÇÕES COMPACTAS
// ==============================

std::string trunc(const std::string& str, size_t w) {
    if (str.length() > w) return str.substr(0, w - 1) + "…";
    return str;
}

std::string lower(const std::string& str) {
    std::string l = str;
    std::transform(l.begin(), l.end(), l.begin(), ::tolower);
    return l;
}

void progress(int cur, int tot, const std::string& file) {
    if (IS_SILENT) return;
    
    int w = 30;
    float p = (float)cur / tot;
    int pos = w * p;
    
    std::cout << "\r" << CYN << "[";
    for (int i = 0; i < w; ++i) {
        if (i < pos) std::cout << GRN << "=" << CYN;
        else if (i == pos) std::cout << GRN << ">" << CYN;
        else std::cout << DIM << "." << CYN;
    }
    std::cout << "]" << R;
    
    std::cout << " " << B << GRN << std::setw(3) << (int)(p * 100.0) << "%" << R;
    std::cout << " " << CYN << "(" << cur << "/" << tot << ")" << R;
    std::cout << " " << DIM << trunc(file, 25) << R << "  " << std::flush;
}

void log(const std::string& lvl, const std::string& msg, const std::string& det = "") {
    if (IS_SILENT && lvl != "ERR") return;

    if (lvl == "INF") std::cout << BLU << "[i]" << R;
    else if (lvl == "WRN") std::cout << YEL << "[!]" << R;
    else if (lvl == "ERR") std::cerr << RED << "[x]" << R;
    else if (lvl == "OK") std::cout << GRN << "[✓]" << R;
    else std::cout << "   ";

    std::string out = msg;
    if (!det.empty()) out += " (" + det + ")";
    
    if (lvl == "ERR") std::cerr << " " << out << std::endl;
    else std::cout << " " << out << std::endl;
}

// ==============================
// 💾 JSON COMPACTO
// ==============================

std::string escJson(const std::string& s) {
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
                if ('\x00' <= c && c <= '\x1f')
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                else o << c;
        }
    }
    return o.str();
}

void saveMeta(const AudioAnalysis& d) {
    if (d.path.empty()) return;
    fs::path p(d.path);
    fs::path jsonPath = p.parent_path() / (p.stem().string() + ".meta.json");
    
    log("INF", "Salvando metadados", trunc(jsonPath.filename().string(), 20));

    std::ofstream f(jsonPath);
    if (!f.is_open()) {
        log("ERR", "Erro ao salvar JSON", jsonPath.string());
        return;
    }

    f << "{\n";
    f << "  \"file\":\"" << escJson(d.filename) << "\",\n";
    f << "  \"title\":\"" << escJson(d.title) << "\",\n";
    f << "  \"artist\":\"" << escJson(d.artist) << "\",\n";
    f << "  \"album\":\"" << escJson(d.album) << "\",\n";
    f << "  \"genre\":\"" << escJson(d.genre) << "\",\n";
    f << "  \"bpm\":" << std::fixed << std::setprecision(2) << d.bpm << ",\n";
    f << "  \"key\":\"" << escJson(d.keyCamelot) << "\",\n";
    f << "  \"energy\":" << std::fixed << std::setprecision(2) << d.energy << ",\n";
    f << "  \"dur\":" << std::fixed << std::setprecision(2) << d.durationSec << ",\n";
    f << "  \"size\":" << std::fixed << std::setprecision(2) << d.fileSizeMB << ",\n";
    f << "  \"sr\":" << d.sampleRate << ",\n";
    f << "  \"ch\":" << d.channels << ",\n";
    f << "  \"br\":" << d.bitrate << "\n";
    f << "}";
}

// ==============================
// 🏷️ TAGS COMPACTAS
// ==============================

std::string cleanPrefix(const std::string& s) {
    if (s.empty()) return "";
    std::string t = s;
    int cnt = 0;
    while (cnt < 3) {
        size_t pos = t.find(" | ");
        if (pos == std::string::npos) break;
        std::string pre = t.substr(0, pos);
        bool isPre = std::all_of(pre.begin(), pre.end(), [](char c){ return ::isalnum(c) || c == '.' || c == '#'; });
        if (isPre) t = t.substr(pos + 3);
        else break;
        cnt++;
    }
    return t;
}

void writeTags(const AudioAnalysis& r, const std::vector<std::string>& tags, bool force) {
    if (tags.empty() || (r.bpm < 0.1 && r.energy < 0.01)) return;

    std::string bpmStr = std::to_string((int)std::round(r.bpm));
    std::string energyStr = std::to_string((int)(r.energy * 100)) + "%";
    std::string keyStr = r.keyCamelot;

    std::vector<std::string> parts;
    if (std::find(tags.begin(), tags.end(), "bpm") != tags.end()) parts.push_back(bpmStr);
    if (std::find(tags.begin(), tags.end(), "energy") != tags.end()) parts.push_back(energyStr);
    if (std::find(tags.begin(), tags.end(), "key") != tags.end()) parts.push_back(keyStr);
    if (parts.empty()) return;

    std::string prefix;
    for (size_t i = 0; i < parts.size(); ++i) {
        prefix += parts[i];
        if (i < parts.size() - 1) prefix += "|";
    }

    try {
        TagLib::FileRef f(r.path.c_str());
        if (f.isNull() || !f.tag()) {
            log("ERR", "Falha ao abrir tags", r.filename);
            return;
        }

        TagLib::Tag *tag = f.tag();
        std::string curr = tag->album().toCString(true);
        std::string finalAlbum;

        if (force) {
            finalAlbum = prefix;
        } else {
            std::string cleaned = cleanPrefix(curr);
            if (cleaned.empty()) finalAlbum = prefix;
            else finalAlbum = prefix + " | " + cleaned;
        }

        tag->setAlbum(finalAlbum);

        if (TagLib::ID3v2::Tag *id3 = dynamic_cast<TagLib::ID3v2::Tag *>(tag)) {
            if (std::find(tags.begin(), tags.end(), "bpm") != tags.end()) {
                 TagLib::ByteVector fid("TBPM");
                 if (!id3->frameList(fid).isEmpty()) id3->removeFrames(fid);
                 TagLib::ID3v2::TextIdentificationFrame *frame = new TagLib::ID3v2::TextIdentificationFrame(fid);
                 frame->setText(bpmStr);
                 id3->addFrame(frame);
            }
            if (std::find(tags.begin(), tags.end(), "key") != tags.end()) {
                 TagLib::ByteVector fid("TKEY");
                 if (!id3->frameList(fid).isEmpty()) id3->removeFrames(fid);
                 TagLib::ID3v2::TextIdentificationFrame *frame = new TagLib::ID3v2::TextIdentificationFrame(fid);
                 frame->setText(keyStr);
                 id3->addFrame(frame);
            }
            if (std::find(tags.begin(), tags.end(), "energy") != tags.end()) {
                TagLib::ID3v2::FrameList txxx = id3->frameList("TXXX");
                for (TagLib::ID3v2::Frame *fr : txxx) {
                    if (TagLib::ID3v2::UserTextIdentificationFrame *t = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame *>(fr)) {
                        if (t->description() == "ENERGY") id3->removeFrame(fr);
                    }
                }
                TagLib::ID3v2::UserTextIdentificationFrame *txxxNew = new TagLib::ID3v2::UserTextIdentificationFrame();
                txxxNew->setDescription("ENERGY");
                txxxNew->setText(energyStr);
                id3->addFrame(txxxNew);
            }
        } else if (TagLib::Ogg::XiphComment *ogg = dynamic_cast<TagLib::Ogg::XiphComment *>(tag)) {
             if (std::find(tags.begin(), tags.end(), "bpm") != tags.end()) ogg->addField("BPM", bpmStr);
             if (std::find(tags.begin(), tags.end(), "key") != tags.end()) ogg->addField("INITIALKEY", keyStr);
             if (std::find(tags.begin(), tags.end(), "energy") != tags.end()) ogg->addField("ENERGY", energyStr);
        }

        if (f.save()) {
            log("OK", "Tags: " + trunc(finalAlbum, 30), trunc(r.filename, 15));
        } else {
            log("ERR", "Falha ao salvar", r.filename);
        }

    } catch (const std::exception& e) {
        log("ERR", "Exceção: " + std::string(e.what()), r.filename);
    }
}

// ==============================
// 📂 FILES COMPACTO
// ==============================

void findFiles(const fs::path& root, std::vector<std::string>& files, const ProgramArgs& args) {
    try {
        if (fs::is_regular_file(root)) {
            std::string ext = lower(root.extension().string());
            if (std::find(args.extensions.begin(), args.extensions.end(), ext) != args.extensions.end())
                files.push_back(root.string());
            return;
        }

        if (fs::is_directory(root)) {
            auto iter = args.recursive ? fs::recursive_directory_iterator(root) : fs::directory_iterator(root);
            for (const auto& e : iter) {
                if (e.is_regular_file()) {
                    std::string ext = lower(e.path().extension().string());
                    if (std::find(args.extensions.begin(), args.extensions.end(), ext) != args.extensions.end())
                        files.push_back(e.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        log("ERR", "Erro no scan: " + std::string(e.what()));
    }
}

// ==============================
// 📊 SAÍDA ULTRA-COMPACTA
// ==============================

void printCompact(std::vector<AudioAnalysis>& res, const ProgramArgs& args) {
    if (args.quiet) return;
    if (res.empty()) {
        log("INF", "Sem resultados");
        return;
    }

    for(const auto& r : res) {
        // Nome (15 chars)
        std::cout << trunc(r.filename, 15);
        
        // Metadados básicos
        if (!r.title.empty() || !r.artist.empty()) {
            std::cout << DIM << " | " << R;
            if (!r.artist.empty()) std::cout << trunc(r.artist, 10);
            if (!r.title.empty()) std::cout << " - " << trunc(r.title, 12);
        }
        
        // Análise se disponível
        if (!args.listMode) {
            if (r.bpm >= 0.1) std::cout << " " << GRN << (int)r.bpm << R;
            if (r.energy >= 0.01) std::cout << " E" << std::fixed << std::setprecision(0) << r.energy*100 << "%";
            if (!r.keyCamelot.empty() && r.keyCamelot != "???") std::cout << " " << YEL << r.keyCamelot << R;
            std::cout << DIM << " " << std::fixed << std::setprecision(1) << r.fileSizeMB << "MB" << R;
        }
        
        std::cout << "\n";
    }
}

void saveCsv(const std::vector<AudioAnalysis>& res, const std::string& fname) {
    std::ofstream f(fname);
    if (!f.is_open()) {
        log("ERR", "Não criou CSV: " + fname);
        return;
    }
    
    f << "file,path,bpm,energy,key,dur,size,title,artist,album,genre\n";
    
    for (const auto& r : res) {
        auto q = [](const std::string& s) {
            std::string t = s;
            size_t pos = 0;
            while ((pos = t.find('"', pos)) != std::string::npos) {
                t.replace(pos, 1, "\"\"");
                pos += 2;
            }
            return "\"" + t + "\"";
        };

        f << q(r.filename) << ","
          << q(r.path) << ","
          << std::fixed << std::setprecision(2) << r.bpm << ","
          << std::fixed << std::setprecision(2) << r.energy << ","
          << q(r.keyCamelot) << ","
          << std::fixed << std::setprecision(2) << r.durationSec << ","
          << std::fixed << std::setprecision(2) << r.fileSizeMB << ","
          << q(r.title) << ","
          << q(r.artist) << ","
          << q(r.album) << ","
          << q(r.genre) << "\n";
    }
    log("OK", "CSV salvo: " + fname);
}

// ==============================
// ❓ HELP COMPACTO
// ==============================

void help(const char* prog) {
    std::cout << "Amalyzer - Analisador de Áudio (Termux)\n\n"
              << "Uso: " << prog << " [opções] <arquivos>\n\n"
              << "Opções:\n"
              << "  -r          Recursivo\n"
              << "  -q          Silencioso\n"
              << "  -l          Lista rápida\n"
              << "  -csv        Saída CSV\n"
              << "  -o FILE     Salvar em arquivo\n"
              << "  -meta       Gerar JSON\n"
              << "  -limit N    Limitar arquivos\n\n"
              << "Filtros:\n"
              << "  -bpm-min N  BPM mínimo\n"
              << "  -bpm-max N  BPM máximo\n"
              << "  -size-min N Tamanho mínimo MB\n"
              << "  -size-max N Tamanho máximo MB\n"
              << "  -key K      Filtro por key\n"
              << "  -ext LIST   Extensões (mp3,flac)\n\n"
              << "Tags:\n"
              << "  -put LIST   Escrever tags (bpm,energy,key)\n"
              << "  -put-force  Substituir album\n"
              << "  -sort LIST  Ordenar (name,bpm,size,key,energy)\n\n"
              << "Ex: " << prog << " -r -put bpm,key -sort bpm ./musicas\n";
}

// ==============================
// 🚀 MAIN COMPACTA
// ==============================

int main(int argc, char* argv[]) {
    ProgramArgs args;
    
    // Parse args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { help(argv[0]); return 0; }
        else if (arg == "-r") args.recursive = true;
        else if (arg == "-csv") args.csv = true;
        else if (arg == "-q") { args.quiet = true; IS_SILENT = true; }
        else if (arg == "-l") args.listMode = true;
        else if (arg == "-put-force") args.putForce = true;
        else if (arg == "-meta") args.meta = true;
        else if (arg == "-o" && i + 1 < argc) args.outputFile = argv[++i];
        else if (arg == "-limit" && i + 1 < argc) { try { args.limit = std::stoi(argv[++i]); } catch (...) {} }
        else if (arg == "-size-min" && i + 1 < argc) { try { args.minSizeMB = std::stod(argv[++i]); } catch (...) {} }
        else if (arg == "-size-max" && i + 1 < argc) { try { args.maxSizeMB = std::stod(argv[++i]); } catch (...) {} }
        else if (arg == "-bpm-min" && i + 1 < argc) { try { args.minBpm = std::stod(argv[++i]); } catch (...) {} }
        else if (arg == "-bpm-max" && i + 1 < argc) { try { args.maxBpm = std::stod(argv[++i]); } catch (...) {} }
        else if (arg == "-key" && i + 1 < argc) args.targetKey = argv[++i];
        else if (arg == "-ext" && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string item;
            args.extensions.clear();
            while (std::getline(ss, item, ',')) {
                if (item[0] != '.') item = "." + item;
                args.extensions.push_back(lower(item));
            }
        }
        else if (arg == "-put" && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string item;
            while (std::getline(ss, item, ',')) args.tagsToWrite.push_back(lower(item));
        }
        else if (arg == "-sort" && i + 1 < argc) {
            std::stringstream ss(argv[++i]);
            std::string item;
            args.sortBy.clear();
            while (std::getline(ss, item, ',')) args.sortBy.push_back(lower(item));
        }
        else if (arg[0] != '-') args.paths.push_back(arg);
    }

    if (args.paths.empty()) {
        help(argv[0]);
        return 1;
    }

    // Não limpar tela no Termux
    Superpowered::Initialize("ExampleLicenseKey-WillExpire-OnNextUpdate");
    Amalyzer amalyzer;

    // Encontrar arquivos
    std::vector<std::string> files;
    for (const auto& p : args.paths) findFiles(p, files, args);

    if (files.empty()) {
        log("INF", "Nenhum arquivo encontrado");
        return 0;
    }

    if (args.limit > 0 && files.size() > (size_t)args.limit) {
        files.resize(args.limit);
    }

    // Cabeçalho mínimo
    if (!IS_SILENT) {
        std::cout << B << CYN << "Amalyzer " << R << DIM << files.size() << " arquivos\n" << R;
    }
    
    log("INF", args.listMode ? "Modo lista..." : "Analisando...");

    // Processamento
    std::vector<AudioAnalysis> results;
    int proc = 0, tot = files.size();

    for (const auto& f : files) {
        AudioAnalysis res;
        res.path = f;
        res.filename = fs::path(f).filename().string();
        
        // Tamanho
        try { res.fileSizeMB = (double)fs::file_size(f) / (1024.0 * 1024.0); } catch(...) { res.fileSizeMB = 0.0; }

        // Tags básicas
        try {
            TagLib::FileRef file(f.c_str());
            if (!file.isNull() && file.tag()) {
                res.title = file.tag()->title().toCString(true);
                res.artist = file.tag()->artist().toCString(true);
                res.album = file.tag()->album().toCString(true);
                res.genre = file.tag()->genre().toCString(true);
                if (file.audioProperties()) {
                    res.bitrate = file.audioProperties()->bitrate();
                    res.sampleRate = file.audioProperties()->sampleRate();
                    res.channels = file.audioProperties()->channels();
                    if (args.listMode) res.durationSec = file.audioProperties()->lengthInSeconds();
                }
            }
        } catch(...) {}

        // Análise se necessário
        if (!args.listMode) {
            AudioAnalysis analysis = amalyzer.analyze(f);
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

        // Filtros
        if (res.success) {
            bool keep = true;
            if (args.minBpm > 0 && res.bpm < args.minBpm) keep = false;
            if (args.maxBpm > 0 && res.bpm > args.maxBpm) keep = false;
            if (args.minSizeMB > 0 && res.fileSizeMB < args.minSizeMB) keep = false;
            if (args.maxSizeMB > 0 && res.fileSizeMB > args.maxSizeMB) keep = false;
            if (!args.targetKey.empty() && lower(res.keyCamelot) != lower(args.targetKey)) keep = false;
            if (keep) results.push_back(res);
        } else if (!args.listMode) {
            log("ERR", "Falha análise: " + trunc(res.errorMessage, 20), res.filename);
        }

        proc++;
        if (!args.quiet) progress(proc, tot, res.filename);
    }

    if (!args.quiet) std::cout << "\n";

    if (results.empty()) {
        log("INF", "Sem arquivos após filtro");
        return 0;
    }

    // Tags
    if (!args.tagsToWrite.empty() && !args.listMode) {
        log("INF", "Escrevendo tags...");
        for (const auto& r : results) writeTags(r, args.tagsToWrite, args.putForce);
    }

    // Meta
    if (args.meta && !args.listMode) {
        log("INF", "Gerando JSONs...");
        for (const auto& r : results) saveMeta(r);
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
        std::string out = args.outputFile.empty() ? "analysis.csv" : args.outputFile;
        saveCsv(results, out);
    } else {
        if (!IS_SILENT && !args.quiet) {
            std::cout << "\n" << B << YEL << (args.listMode ? "Lista:" : "Análise:") << R << "\n";
        }
        printCompact(results, args);
        
        if (!IS_SILENT && !args.quiet) {
            std::cout << DIM << "\nTotal: " << results.size() << " arquivos" << R << "\n";
        }
    }

    return 0;
}