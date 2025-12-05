// ==============================
// 📦 BIBLIOTECAS PADRÃO C++
// ==============================
#include <iostream>   // Entrada/saída (cout, cerr)
#include <fstream>    // Manipulação de arquivos
#include <string>     // Strings
#include <vector>     // Arrays dinâmicos
#include <cmath>      // Funções matemáticas (round, etc)
#include <algorithm>  // Algoritmos (sort, find, transform)
#include <iomanip>    // Formatação de saída (setw, setprecision)
#include <sstream>    // String streams
#include <filesystem> // Manipulação de sistema de arquivos
#include <set>        // Conjuntos ordenados
#include <map>        // Mapas (dicionários)
#include <stdexcept>  // Exceções padrão

// ==============================
// 📦 BIBLIOTECAS DO PROJETO
// ==============================
// Classe Amalyzer que faz a análise de áudio usando Superpowered SDK
#include "analyzer.h"
#include "config_manager.h"

// ==============================
// 📦 BIBLIOTECAS TAGLIB
// ==============================
// TagLib é usado para ler e escrever metadados de arquivos de áudio
#include <taglib/tag.h>                      // Interface básica de tags
#include <taglib/fileref.h>                  // Referência genérica a arquivos
#include <taglib/tfile.h>                    // Arquivo TagLib
#include <taglib/textidentificationframe.h> // Frames de texto ID3v2
#include <taglib/id3v2tag.h>                 // Tags ID3v2 (MP3)
#include <taglib/mp4file.h>                  // Arquivos MP4/M4A
#include <taglib/flacfile.h>                 // Arquivos FLAC
#include <taglib/oggfile.h>                  // Arquivos OGG
#include <taglib/vorbisfile.h>               // Arquivos Vorbis
#include <taglib/xiphcomment.h>              // Comentários Xiph (OGG/FLAC)

// Alias para facilitar o uso do namespace filesystem
namespace fs = std::filesystem;

// ==============================
// 🎨 CONSTANTES DE CORES ANSI
// ==============================
// Códigos ANSI para colorir a saída no terminal
const std::string RESET = "\033[0m";    // Reset todas as formatações
const std::string RED = "\033[91m";     // Vermelho (erros)
const std::string GREEN = "\033[92m";   // Verde (sucesso, BPM)
const std::string YELLOW = "\033[93m";  // Amarelo (avisos, key)
const std::string BLUE = "\033[94m";    // Azul (informações)
const std::string CYAN = "\033[96m";    // Ciano (progresso)
const std::string MAGENTA = "\033[95m"; // Magenta
const std::string BOLD = "\033[1m";     // Negrito
const std::string DIM = "\033[2m";      // Texto esmaecido

// Flag global para controlar se deve suprimir a saída
bool IS_SILENT = false;

// ==============================
// 🎵 ESTRUTURAS DE DADOS
// ==============================

// AudioAnalysis é definida em analyzer.h e contém:
// - path, filename: caminho e nome do arquivo
// - bpm, energy, keyCamelot: resultados da análise
// - title, artist, album, genre: metadados
// - fileSizeMB, durationSec: informações do arquivo
// - success, errorMessage: status da análise

/**
 * @struct ProgramArgs
 * @brief Estrutura que armazena todos os argumentos da linha de comando
 */
struct ProgramArgs {
    // Arquivos e diretórios para processar
    std::vector<std::string> paths;
    
    // Opções de busca
    bool recursive = false;  // -r: buscar recursivamente em subdiretórios
    std::vector<std::string> extensions = {".mp3", ".flac", ".ogg", ".wav", ".m4a", ".aif", ".aiff"};
    
    // Opções de saída
    bool csv = false;              // -csv: exportar para CSV
    std::string outputFile;        // -o: arquivo de saída
    bool quiet = false;            // -q: modo silencioso
    
    // Filtros de análise
    double minBpm = 0.0;          // -bpm-min: BPM mínimo
    double maxBpm = 0.0;          // -bpm-max: BPM máximo
    double minSizeMB = 0.0;       // -size-min: tamanho mínimo em MB
    double maxSizeMB = 0.0;       // -size-max: tamanho máximo em MB
    std::string targetKey = "";   // -key: filtrar por tonalidade (ex: 8B)
    int limit = 0;                // -limit: limitar número de arquivos

    // Opções de escrita de tags
    std::vector<std::string> tagsToWrite;  // -put: tags para escrever (bpm,energy,key)
    bool putForce = false;                 // -putforce: sobrescrever álbum completamente
    
    // Modos de operação
    bool listMode = false;  // -l: modo lista (sem análise)
    bool meta = false;      // -meta: gerar arquivos .analisemetadata
    
    // Ordenação
    std::vector<std::string> sortBy = {"name"};  // -sort: critérios de ordenação
};

// ==============================
// 🛠️ FUNÇÕES AUXILIARES
// ==============================

/**
 * @brief Trunca uma string se ela exceder a largura especificada
 * @param str String a ser truncada
 * @param width Largura máxima
 * @return String truncada com "…" no final se necessário
 */
std::string truncate(const std::string& str, size_t width) {
    if (str.length() > width) {
        return str.substr(0, width - 1) + "…";
    }
    return str;
}

/**
 * @brief Preenche uma string com espaços para ter largura fixa
 * @param str String a ser preenchida
 * @param width Largura desejada
 * @return String com largura fixa (truncada ou preenchida com espaços)
 * 
 * Esta função garante que todas as strings tenham exatamente o mesmo tamanho,
 * o que é essencial para alinhar colunas na saída do terminal.
 */
std::string padString(const std::string& str, size_t width) {
    if (str.length() > width) {
        return str.substr(0, width - 3) + "...";
    }
    std::string result = str;
    result.resize(width, ' ');  // Preenche com espaços até atingir a largura
    return result;
}

/**
 * @brief Converte uma string para minúsculas
 * @param str String a ser convertida
 * @return String em minúsculas
 */
std::string toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

/**
 * @brief Desenha uma barra de progresso animada no terminal
 * @param current Número de arquivos processados
 * @param total Total de arquivos
 * @param currentFile Nome do arquivo sendo processado atualmente
 * 
 * Exibe uma barra de progresso colorida com:
 * - Barra visual com blocos preenchidos (█) e vazios (░)
 * - Percentual de conclusão
 * - Nome do arquivo atual (truncado)
 */
void drawProgressBar(int current, int total, const std::string& currentFile) {
    if (IS_SILENT) return;
    
    int barWidth = 20;  // Largura da barra (otimizado para mobile)
    float progress = (float)current / total;
    int pos = barWidth * progress;
    
    // Desenha a barra com caracteres Unicode
    std::cout << "\r" << CYAN << "[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << GREEN << "█";  // Bloco preenchido
        else std::cout << DIM << "░";             // Bloco vazio
    }
    std::cout << CYAN << "]" << RESET;
    
    // Percentual e nome do arquivo
    std::cout << " " << BOLD << GREEN << (int)(progress * 100.0) << "%" << RESET;
    std::cout << " " << DIM << truncate(currentFile, 20) << RESET << std::flush;
}

/**
 * @brief Registra uma mensagem no console com nível de severidade
 * @param level Nível: "INFO", "WARNING", "ERROR", "SUCCESS"
 * @param message Mensagem principal
 * @param detail Detalhes opcionais (nome de arquivo, etc)
 * 
 * Cada nível tem um ícone e cor diferentes:
 * - INFO: [i] azul
 * - WARNING: [!] amarelo
 * - ERROR: [x] vermelho (vai para stderr)
 * - SUCCESS: [✓] verde
 */
void log(const std::string& level, const std::string& message, const std::string& detail = "") {
    if (IS_SILENT && level != "ERROR") return;

    // Ícones coloridos por nível
    if (level == "INFO") std::cout << BLUE << "[i] " << RESET;
    else if (level == "WARNING") std::cout << YELLOW << "[!] " << RESET;
    else if (level == "ERROR") std::cerr << RED << "[x] " << RESET;
    else if (level == "SUCCESS") std::cout << GREEN << "[✓] " << RESET;

    if (level == "ERROR") std::cerr << message;
    else std::cout << message;

    if (!detail.empty()) {
        if (level == "ERROR") std::cerr << " " << detail;
        else std::cout << " " << detail;
    }
    
    if (level == "ERROR") std::cerr << std::endl;
    else std::cout << std::endl;
}

// ==============================
// 💾 JSON / METADATA
// ==============================

/**
 * @brief Escapa caracteres especiais em uma string para formato JSON
 * @param s String a ser escapada
 * @return String com caracteres especiais escapados
 * 
 * Converte caracteres como aspas, barras invertidas e caracteres de controle
 * para suas representações escapadas em JSON (ex: " vira \")
 */
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

/**
 * @brief Salva os metadados de análise em um arquivo JSON
 * @param data Estrutura AudioAnalysis com os dados a serem salvos
 * 
 * Cria um arquivo .analisemetadata no mesmo diretório do arquivo de áudio
 * contendo informações como BPM, energy, key, duração, tamanho, etc.
 * Formato: nomeDoArquivo.extensão.analisemetadata
 */
void saveMetadataFile(const AudioAnalysis& data) {
    if (data.path.empty()) return;

    fs::path p(data.path);
    // Cria o caminho do arquivo JSON: audio.mp3 -> audio.mp3.analisemetadata
    fs::path jsonPath = p.parent_path() / (p.stem().string() + p.extension().string() + ".analisemetadata");
    
    log("INFO", "Gerando meta: " + jsonPath.filename().string());

    std::ofstream f(jsonPath);
    if (!f.is_open()) {
        log("ERROR", "Erro ao salvar meta", jsonPath.string());
        return;
    }

    f << "{";
    f << "\"file\":\"" << escapeJsonString(data.filename) << "\",";
    f << "\"title\":\"" << escapeJsonString(data.title) << "\",";
    f << "\"artist\":\"" << escapeJsonString(data.artist) << "\",";
    f << "\"bpm\":" << std::fixed << std::setprecision(2) << data.bpm << ",";
    f << "\"key\":\"" << escapeJsonString(data.keyCamelot) << "\",";
    f << "\"energy\":" << std::fixed << std::setprecision(2) << data.energy << ",";
    f << "\"len\":" << std::fixed << std::setprecision(1) << data.durationSec << ",";
    f << "\"size\":" << std::fixed << std::setprecision(2) << data.fileSizeMB << ",";
    f << "\"bitrate\":" << data.bitrate;
    f << "}";
}

// ==============================
// 🏷️ ESCRITA DE TAGS
// ==============================

/**
 * @brief Remove prefixos de análise anteriores do campo álbum
 * @param albumStr String do álbum que pode conter prefixos
 * @return String do álbum sem os prefixos de análise
 * 
 * Remove até 3 prefixos no formato "XXX | " onde XXX contém apenas
 * caracteres alfanuméricos, pontos ou hashtags (ex: "120|0.5|8A | ")
 * Isso permite preservar o nome original do álbum ao atualizar tags.
 */
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

/**
 * @brief Escreve tags de análise (BPM, Energy, Key) nos arquivos de áudio
 * @param res Estrutura AudioAnalysis com os dados da análise
 * @param tagsToWrite Lista de tags para escrever ("bpm", "energy", "key")
 * @param force Se true, sobrescreve completamente o campo álbum; se false, preserva o álbum original
 * 
 * Escreve as tags em dois locais:
 * 1. Campo ALBUM: formato "BPM | Energy | Key" ou "BPM | Energy | Key | Álbum Original"
 * 2. Campos específicos: TBPM, TKEY (ID3v2) ou campos equivalentes (OGG/FLAC)
 * 
 * Suporta formatos: MP3 (ID3v2), OGG, FLAC (Xiph Comments)
 */
void writeTags(const AudioAnalysis& res, const std::vector<std::string>& tagsToWrite, bool force) {
    // Ignora se não há tags para escrever ou se a análise falhou
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
            log("ERROR", "Falha ao abrir tags", res.filename);
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

        // Escreve tags específicas dependendo do formato do arquivo
        // MP3 usa ID3v2, OGG/FLAC usam Xiph Comments
        if (TagLib::ID3v2::Tag *id3 = dynamic_cast<TagLib::ID3v2::Tag *>(tag)) {
            // === FORMATO MP3 (ID3v2) ===
            
            // Escreve BPM no frame TBPM
            if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "bpm") != tagsToWrite.end()) {
                 TagLib::ByteVector frameId("TBPM");
                 if (!id3->frameList(frameId).isEmpty()) id3->removeFrames(frameId);
                 TagLib::ID3v2::TextIdentificationFrame *frame = new TagLib::ID3v2::TextIdentificationFrame(frameId);
                 frame->setText(bpmStr);
                 id3->addFrame(frame);
            }
            
            // Escreve Key no frame TKEY
            if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "key") != tagsToWrite.end()) {
                 TagLib::ByteVector frameId("TKEY");
                 if (!id3->frameList(frameId).isEmpty()) id3->removeFrames(frameId);
                 TagLib::ID3v2::TextIdentificationFrame *frame = new TagLib::ID3v2::TextIdentificationFrame(frameId);
                 frame->setText(keyStr);
                 id3->addFrame(frame);
            }
            
            // Escreve Energy em um frame TXXX (user-defined text)
            if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "energy") != tagsToWrite.end()) {
                // Remove frames TXXX com descrição "ENERGY" existentes
                TagLib::ID3v2::FrameList txxxFrames = id3->frameList("TXXX");
                for (TagLib::ID3v2::Frame *frame : txxxFrames) {
                    if (TagLib::ID3v2::UserTextIdentificationFrame *txxx = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame *>(frame)) {
                        if (txxx->description() == "ENERGY") {
                            id3->removeFrame(frame);
                        }
                    }
                }
                // Cria novo frame TXXX para Energy
                TagLib::ID3v2::UserTextIdentificationFrame *txxx = new TagLib::ID3v2::UserTextIdentificationFrame();
                txxx->setDescription("ENERGY");
                txxx->setText(energyStr);
                id3->addFrame(txxx);
            }
        } else if (TagLib::Ogg::XiphComment *ogg = dynamic_cast<TagLib::Ogg::XiphComment *>(tag)) {
             // === FORMATO OGG/FLAC (Xiph Comments) ===
             // Adiciona campos diretamente (não precisa remover os antigos)
             if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "bpm") != tagsToWrite.end()) ogg->addField("BPM", bpmStr);
             if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "key") != tagsToWrite.end()) ogg->addField("INITIALKEY", keyStr);
             if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "energy") != tagsToWrite.end()) ogg->addField("ENERGY", energyStr);
        }

        if (f.save()) {
            log("SUCCESS", "Tags salvas", res.filename);
        } else {
            log("ERROR", "Falha ao salvar", res.filename);
        }

    } catch (const std::exception& e) {
        log("ERROR", "Erro tags: " + std::string(e.what()), res.filename);
    }
}

// ==============================
// 📂 ARQUIVOS E DIRETÓRIOS
// ==============================

/**
 * @brief Busca arquivos de áudio em um diretório ou adiciona um arquivo específico
 * @param root Caminho do arquivo ou diretório raiz
 * @param files Vetor onde os caminhos dos arquivos encontrados serão adicionados
 * @param args Argumentos do programa (contém extensões permitidas e flag recursivo)
 * 
 * Se root for um arquivo, adiciona-o à lista se tiver extensão válida.
 * Se root for um diretório, busca arquivos com extensões válidas:
 * - Modo recursivo (-r): busca em todos os subdiretórios
 * - Modo normal: busca apenas no diretório especificado
 */
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
        log("ERROR", "Erro scan: " + std::string(e.what()));
    }
}

// ==============================
// 📊 SAÍDA COMPACTA
// ==============================

/**
 * @brief Exibe os resultados da análise em formato de tabela compacta
 * @param results Vetor com os resultados das análises
 * @param args Argumentos do programa (contém modo lista, quiet, etc)
 * 
 * Dois modos de exibição:
 * 1. Modo Lista (-l): Nome (25 chars) - Artista [Álbum] Tamanho
 * 2. Modo Análise: Nome (20 chars) BPM Key Energy Tamanho
 * 
 * Usa larguras fixas para garantir alinhamento perfeito das colunas.
 */
void printTable(std::vector<AudioAnalysis>& results, const ProgramArgs& args, ConfigManager& config) {
    if (args.quiet) return;

    if (results.empty()) {
        log("INFO", "Nenhum resultado.");
        return;
    }

    if (args.listMode) {
        // Lista ultra compacta com larguras fixas
        for(const auto& res : results) {
            // Nome do arquivo (configurável)
            int nameWidth = config.getInt("name_w", 25);
            std::cout << padString(res.filename, nameWidth);
            
            // Artista (configurável)
            int artistWidth = config.getInt("artist_w", 15);
            std::cout << DIM << " - ";
            if (!res.artist.empty()) {
                std::cout << padString(res.artist, artistWidth);
            } else {
                std::cout << padString("", artistWidth);  // Espaço vazio se não houver artista
            }
            std::cout << RESET;
            
            // Álbum (configurável)
            int albumWidth = config.getInt("album_w", 20);
            std::cout << DIM << " [";
            if (!res.album.empty()) {
                std::cout << padString(res.album, albumWidth);
            } else {
                std::cout << padString("", albumWidth);  // Espaço vazio se não houver álbum
            }
            std::cout << "]" << RESET;
            
            // Tamanho (sempre no mesmo lugar)
            std::cout << " " << CYAN << std::fixed << std::setprecision(1) << std::setw(4) << res.fileSizeMB << "MB" << RESET;
            std::cout << "\n";
        }
    } else {
        // Análise compacta - uma linha por arquivo
        for(const auto& res : results) {
            // Nome (configurável)
            int nameWidth = config.getInt("ana_name_w", 20);
            std::cout << padString(res.filename, nameWidth);
            
            // BPM (3 chars fixos)
            if(res.bpm >= 0.1) {
                std::cout << GREEN << std::setw(3) << (int)res.bpm << RESET;
            } else {
                std::cout << "   ";
            }
            
            // Key (4 chars fixos - espaço + 3 chars)
            if(!res.keyCamelot.empty() && res.keyCamelot != "???") {
                std::cout << " " << YELLOW << std::setw(3) << std::left << truncate(res.keyCamelot, 3) << RESET;
            } else {
                std::cout << "    ";
            }
            
            // Energy (5 chars fixos)
            if(res.energy >= 0.01) {
                std::cout << " " << std::fixed << std::setprecision(1) << std::setw(3) << res.energy;
            } else {
                std::cout << "     ";
            }
            
            // Size (6 chars fixos)
            std::cout << " " << DIM << std::fixed << std::setprecision(1) << std::setw(4) << res.fileSizeMB << "M" << RESET;
            
            std::cout << "\n";
        }
    }
}

/**
 * @brief Exporta os resultados para um arquivo CSV
 * @param results Vetor com os resultados das análises
 * @param filename Nome do arquivo CSV a ser criado
 * 
 * Formato CSV: file,path,bpm,energy,key,sec,mb,title,artist
 * Strings são escapadas (aspas duplas viram "")
 */
void saveCsv(const std::vector<AudioAnalysis>& results, const std::string& filename) {
    std::ofstream f(filename);
    if (!f.is_open()) {
        log("ERROR", "Erro CSV: " + filename);
        return;
    }
    
    // Colunas compactas
    f << "file,path,bpm,energy,key,sec,mb,title,artist\n";
    
    auto csv_quote = [](const std::string& s) {
        std::string temp = s;
        std::string result;
        result.reserve(temp.length() + 2);
        for (char c : temp) {
            if (c == '"') result += "\"\"";
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
          << std::fixed << std::setprecision(1) << r.durationSec << ","
          << std::fixed << std::setprecision(1) << r.fileSizeMB << ","
          << csv_quote(r.title) << ","
          << csv_quote(r.artist) << "\n";
    }
    log("SUCCESS", "CSV: " + filename);
}

// ==============================
// ❓ HELP COMPACTO
// ==============================

/**
 * @brief Exibe a mensagem de ajuda com todas as opções disponíveis
 * @param progName Nome do programa (argv[0])
 * 
 * Mostra:
 * - Opções básicas (-r, -q, -l, -csv, -o, -meta, -limit)
 * - Filtros (-bpm-min/max, -size-min/max, -key, -ext)
 * - Saída/Tags (-sort, -put, -putforce)
 * - Exemplo de uso
 */
void printHelp(const char* progName) {
    std::cout << "🎵 Amalyzer - Mobile 🎵\n\n"
              << "Uso: " << progName << " [opções] <arquivos>\n\n"
              << "Opções:\n"
              << "  -r          Recursivo\n"
              << "  -q          Silencioso\n"
              << "  -l          Lista rápida\n"
              << "  -csv        Saída CSV\n"
              << "  -o <file>   Salvar em arquivo\n"
              << "  -meta       Gerar .analisemetadata\n"
              << "  -limit <N>  Limitar a N arquivos\n\n"
              << "Filtros:\n"
              << "  -bpm-min/max N   Filtrar por BPM\n"
              << "  -size-min/max N  Filtrar por tamanho (MB)\n"
              << "  -key <K>         Filtrar por key (ex: 8B)\n"
              << "  -ext <list>      Extensões (ex: mp3,flac)\n\n"
              << "Saída/Tags:\n"
              << "  -sort <list>     Ordenar (name,bpm,size,key,energy)\n"
              << "  -put <list>      Escrever tags (bpm,energy,key)\n"
              << "  -putforce        Forçar escrita (sobrescrever álbum)\n"
              << "  -config <k=v>    Atualizar configuração (ex: name_w=50)\n"
              << "  -config          Listar configurações atuais\n\n"
              << "Ex: " << progName << " -r -put bpm,key -sort bpm ./musicas\n";
}

// ==============================
// 🚀 FUNÇÃO PRINCIPAL
// ==============================

/**
 * @brief Função principal do programa
 * 
 * Fluxo de execução:
 * 1. Parse dos argumentos da linha de comando
 * 2. Inicialização do Superpowered SDK
 * 3. Busca de arquivos de áudio
 * 4. Processamento (leitura de tags e/ou análise)
 * 5. Aplicação de filtros
 * 6. Escrita de tags (se solicitado)
 * 7. Geração de metadados (se solicitado)
 * 8. Ordenação dos resultados
 * 9. Saída (CSV ou tabela no terminal)
 */
int main(int argc, char* argv[]) {
    ConfigManager config;
    ProgramArgs args;
    
    // ==============================
    // PARSE DOS ARGUMENTOS
    // ==============================
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
        else if (arg == "-put-force" || arg == "-putforce") args.putForce = true;
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
        else if (arg == "-config") {
            if (i + 1 < argc && argv[i+1][0] != '-') {
                std::string configPair = argv[++i];
                size_t delimiterPos = configPair.find('=');
                if (delimiterPos != std::string::npos) {
                    std::string key = configPair.substr(0, delimiterPos);
                    std::string value = configPair.substr(delimiterPos + 1);
                    config.setValue(key, value);
                    config.save();
                    log("SUCCESS", "Config atualizada: " + key + "=" + value);
                } else {
                    log("ERROR", "Formato inválido para -config. Use key=value");
                }
            } else {
                config.print();
                return 0;
            }
        }
        else if (arg[0] != '-') args.paths.push_back(arg);  // Argumentos sem '-' são caminhos
    }

    // Verifica se pelo menos um caminho foi fornecido
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

    // Inicializa o SDK Superpowered (necessário para análise de áudio)
    Superpowered::Initialize("ExampleLicenseKey-WillExpire-OnNextUpdate");
    Amalyzer amalyzer;  // Instância do analisador de áudio

    // ==============================
    // BUSCA DE ARQUIVOS
    // ==============================
    std::vector<std::string> files;
    for (const auto& p : args.paths) {
        findFiles(p, files, args);
    }

    if (files.empty()) {
        log("INFO", "Nenhum arquivo.");
        return 0;
    }

    // Aplica limite de arquivos se especificado
    if (args.limit > 0 && files.size() > (size_t)args.limit) {
        files.resize(args.limit);
    }

    if (!IS_SILENT) {
        std::cout << BOLD << CYAN << "🎵 Amalyzer" << RESET << "\n";
        std::cout << DIM << "════════════════════════" << RESET << "\n\n";
    }
    
    if (args.listMode) {
        log("INFO", "Modo lista...");
    } else {
        log("INFO", "Analisando...");
    }

    // ==============================
    // PROCESSAMENTO DOS ARQUIVOS
    // ==============================
    std::vector<AudioAnalysis> results;
    int processedCount = 0;
    int totalFiles = files.size();

    for (const auto& fpath : files) {
        AudioAnalysis res;
        res.path = fpath;
        res.filename = fs::path(fpath).filename().string();
        
        // Obtém tamanho do arquivo
        try {
            res.fileSizeMB = (double)fs::file_size(fpath) / (1024.0 * 1024.0);
        } catch(...) { res.fileSizeMB = 0.0; }

        // Lê metadados básicos usando TagLib (título, artista, álbum, etc)
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

        // Realiza análise de áudio (BPM, Energy, Key) se não estiver em modo lista
        if (!args.listMode) {
            // Perform Audio Analysis
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

        // Aplica filtros (BPM, tamanho, key)
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

    if (!args.quiet) std::cout << "\n\n";

    // Verifica se há resultados após aplicar filtros
    if (results.empty()) {
        log("INFO", "Sem resultados após filtros.");
        return 0;
    }

    // ==============================
    // ESCRITA DE TAGS
    // ==============================
    if (!args.tagsToWrite.empty() && !args.listMode) {
        log("INFO", "Escrevendo tags...");
        for (const auto& res : results) {
             writeTags(res, args.tagsToWrite, args.putForce);
        }
        if (!IS_SILENT) std::cout << DIM << "────────────────────────" << RESET << "\n";
    }

    // ==============================
    // GERAÇÃO DE METADADOS
    // ==============================
    if (args.meta && !args.listMode) {
        log("INFO", "Gerando meta...");
        for (const auto& res : results) {
            saveMetadataFile(res);
        }
    }

    // ==============================
    // ORDENAÇÃO DOS RESULTADOS
    // ==============================
    // Aplica critérios de ordenação em ordem reversa (último critério tem prioridade)
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

    // ==============================
    // SAÍDA DOS RESULTADOS
    // ==============================
    if (args.csv) {
        std::string out = args.outputFile.empty() ? "analysis.csv" : args.outputFile;
        saveCsv(results, out);
    } else {
        if (!IS_SILENT) {
            if (args.listMode) {
                std::cout << BOLD << "📁 Lista (" << results.size() << ")" << RESET << "\n\n";
            } else {
                std::cout << BOLD << "📊 Resultados (" << results.size() << ")" << RESET << "\n\n";
            }
        }
        printTable(results, args, config);
        
        // Resumo ultra compacto
        if (!IS_SILENT) {
            std::cout << "\n" << DIM << "─ Total: " << results.size() << " arquivos" << RESET << "\n";
        }
    }
    

    return 0;
}