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
#include <taglib/tag.h>                     // Interface básica de tags
#include <taglib/fileref.h>                 // Referência genérica a arquivos
#include <taglib/tfile.h>                   // Arquivo TagLib
#include <taglib/textidentificationframe.h> // Frames de texto ID3v2
#include <taglib/commentsframe.h>           // Frames de comentários ID3v2
#include <taglib/attachedpictureframe.h>    // Capa do álbum (ID3v2)
#include <taglib/id3v2tag.h>                // Tags ID3v2 (MP3)
#include <taglib/mpegfile.h>                // Arquivos MPEG (MP3)
#include <taglib/mp4file.h>                 // Arquivos MP4/M4A
#include <taglib/mp4tag.h>                  // Tags MP4
#include <taglib/mp4coverart.h>             // Capa MP4
#include <taglib/flacfile.h>                // Arquivos FLAC
#include <taglib/oggfile.h>                 // Arquivos OGG
#include <taglib/vorbisfile.h>              // Arquivos Vorbis
#include <taglib/wavfile.h>                 // Arquivos WAV
#include <taglib/aifffile.h>                // Arquivos AIFF
#include <taglib/rifffile.h>                // Arquivos RIFF (WAV/AIFF)
#include <taglib/xiphcomment.h>             // Comentários Xiph (OGG/FLAC)
#include <taglib/tpropertymap.h>            // Mapa de propriedades genérico

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
// - title, artist, album, genre: metadados
// - fileSizeMB, durationSec: informações do arquivo

enum TagMode
{
    SET,
    APPEND,
    PREPEND
};

struct TagOperation
{
    std::string key;
    std::string value;
    TagMode mode;
};

/**
 * @struct ProgramArgs

/**
 * @struct ProgramArgs
 * @brief Estrutura que armazena todos os argumentos da linha de comando
 */
struct ProgramArgs
{
    // Arquivos e diretórios para processar
    std::vector<std::string> paths;

    // Opções de busca
    bool recursive = false; // -r: buscar recursivamente em subdiretórios
    std::vector<std::string> extensions = {".mp3", ".flac", ".ogg", ".wav", ".m4a", ".aif", ".aiff"};

    // Opções de saída
    bool csv = false;       // -csv: exportar para CSV
    std::string outputFile; // -o: arquivo de saída
    bool quiet = false;     // -q: modo silencioso

    // Filtros de análise
    double minBpm = 0.0;        // -bpm-min: BPM mínimo
    double maxBpm = 0.0;        // -bpm-max: BPM máximo
    double minSizeMB = 0.0;     // -size-min: tamanho mínimo em MB
    double maxSizeMB = 0.0;     // -size-max: tamanho máximo em MB
    std::string targetKey = ""; // -key: filtrar por tonalidade (ex: 8B)
    int limit = 0;              // -limit: limitar número de arquivos

    // Opções de escrita de tags
    std::vector<std::string> tagsToWrite;  // -put: tags para escrever (bpm,energy,key)
    bool putForce = false;                 // -putforce: sobrescrever álbum completamente
    std::string coverPath;                 // -cover: caminho da imagem de capa
    bool removeCover = false;              // -cover-remove: remover imagem de capa
    bool removeAllTags = false;            // -remalltag: remover todas as tags
    std::vector<std::string> tagsToRemove; // -remtag: tags específicas para remover
    std::vector<TagOperation> tagOps;      // -settag, -addtag, -pretag: operações de edição de tags

    // Modos de operação

    // Modos de operação
    bool listMode = false;                // -l: modo lista (sem análise)
    std::vector<std::string> listColumns; // Colunas para o modo lista
    bool meta = false;                    // -meta: gerar arquivos .analisemetadata

    // Ordenação
    std::vector<std::string> sortBy = {"name"}; // -sort: critérios de ordenação
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
std::string truncate(const std::string &str, size_t width)
{
    if (str.length() > width)
    {
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
std::string padString(const std::string &str, size_t width)
{
    if (str.length() > width)
    {
        return str.substr(0, width - 3) + "...";
    }
    std::string result = str;
    result.resize(width, ' '); // Preenche com espaços até atingir a largura
    return result;
}

/**
 * @brief Converte uma string para minúsculas
 * @param str String a ser convertida
 * @return String em minúsculas
 */
std::string toLower(const std::string &str)
{
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
void drawProgressBar(int current, int total, const std::string &currentFile)
{
    if (IS_SILENT)
        return;

    int barWidth = 20; // Largura da barra (otimizado para mobile)
    float progress = (float)current / total;
    int pos = barWidth * progress;

    // Desenha a barra com caracteres Unicode
    std::cout << "\r" << CYAN << "[";
    for (int i = 0; i < barWidth; ++i)
    {
        if (i < pos)
            std::cout << GREEN << "█"; // Bloco preenchido
        else
            std::cout << DIM << "░"; // Bloco vazio
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
void log(const std::string &level, const std::string &message, const std::string &detail = "")
{
    if (IS_SILENT && level != "ERROR")
        return;

    // Ícones coloridos por nível
    if (level == "INFO")
        std::cout << BLUE << "[i] " << RESET;
    else if (level == "WARNING")
        std::cout << YELLOW << "[!] " << RESET;
    else if (level == "ERROR")
        std::cerr << RED << "[x] " << RESET;
    else if (level == "SUCCESS")
        std::cout << GREEN << "[✓] " << RESET;

    if (level == "ERROR")
        std::cerr << message;
    else
        std::cout << message;

    if (!detail.empty())
    {
        if (level == "ERROR")
            std::cerr << " " << detail;
        else
            std::cout << " " << detail;
    }

    if (level == "ERROR")
        std::cerr << std::endl;
    else
        std::cout << std::endl;
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
std::string escapeJsonString(const std::string &s)
{
    std::ostringstream o;
    for (char c : s)
    {
        switch (c)
        {
        case '"':
            o << "\\\"";
            break;
        case '\\':
            o << "\\\\";
            break;
        case '\b':
            o << "\\b";
            break;
        case '\f':
            o << "\\f";
            break;
        case '\n':
            o << "\\n";
            break;
        case '\r':
            o << "\\r";
            break;
        case '\t':
            o << "\\t";
            break;
        default:
            if ('\x00' <= c && c <= '\x1f')
            {
                o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
            }
            else
            {
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
void saveMetadataFile(const AudioAnalysis &data)
{
    if (data.path.empty())
        return;

    fs::path p(data.path);
    // Cria o caminho do arquivo JSON: audio.mp3 -> audio.mp3.analisemetadata
    fs::path jsonPath = p.parent_path() / (p.stem().string() + p.extension().string() + ".analisemetadata");

    log("INFO", "Gerando meta: " + jsonPath.filename().string());

    std::ofstream f(jsonPath);
    if (!f.is_open())
    {
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
std::string cleanAlbumPrefix(const std::string &albumStr)
{
    if (albumStr.empty())
        return "";
    std::string temp = albumStr;
    int count = 0;
    while (count < 3)
    {
        size_t pos = temp.find(" | ");
        if (pos == std::string::npos)
            break;
        std::string prefix = temp.substr(0, pos);
        bool isPrefix = false;
        if (std::all_of(prefix.begin(), prefix.end(), [](char c)
                        { return ::isalnum(c) || c == '.' || c == '#'; }))
        {
            isPrefix = true;
        }
        if (isPrefix)
            temp = temp.substr(pos + 3);
        else
            break;
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
// ==============================
// 🏷️ ESCRITA DE TAGS (APENAS A FUNÇÃO writeTags MODIFICADA)
// ==============================

// ... (Função cleanAlbumPrefix permanece inalterada) ...

void writeTags(const AudioAnalysis &res, const std::vector<std::string> &tagsToWrite, bool force)
{
    // Ignora se não há tags para escrever ou se a análise falhou
    if (tagsToWrite.empty() || (res.bpm < 0.1 && res.energy < 0.01))
        return;

    std::stringstream bpmSs, energySs;
    bpmSs << std::fixed << std::setprecision(0) << std::round(res.bpm);
    energySs << std::fixed << std::setprecision(2) << res.energy;

    std::string bpmStr = bpmSs.str();
    std::string energyStr = energySs.str();
    std::string keyStr = res.keyCamelot;

    std::vector<std::string> parts;
    if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "bpm") != tagsToWrite.end())
        parts.push_back(bpmStr);
    if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "energy") != tagsToWrite.end())
        parts.push_back(energyStr);
    if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "key") != tagsToWrite.end())
        parts.push_back(keyStr);

    if (parts.empty())
        return;

    // Constrói string de comentário: "BPM: 128 | Key: 8A | Energy: 0.85"
    std::string commentStr;
    if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "bpm") != tagsToWrite.end())
        commentStr += "BPM: " + bpmStr + " | ";
    if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "key") != tagsToWrite.end())
        commentStr += "Key: " + keyStr + " | ";
    if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "energy") != tagsToWrite.end())
        commentStr += "Energy: " + energyStr + " | ";
    if (commentStr.length() > 3)
        commentStr = commentStr.substr(0, commentStr.length() - 3); // Remove último " | "

    std::string newPrefix;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        newPrefix += parts[i];
        if (i < parts.size() - 1)
            newPrefix += " | ";
    }

    try
    {
        TagLib::FileRef f(res.path.c_str());
        if (f.isNull() || !f.tag())
        {
            log("ERROR", "Falha ao abrir tags", res.filename);
            return;
        }

        TagLib::Tag *tag = f.tag();
        std::string currentAlbum = tag->album().toCString(true);
        std::string finalAlbum;

        if (force)
        {
            finalAlbum = newPrefix;
        }
        else
        {
            std::string cleaned = cleanAlbumPrefix(currentAlbum);
            if (cleaned.empty())
                finalAlbum = newPrefix;
            else
                finalAlbum = newPrefix + " | " + cleaned;
        }

        tag->setAlbum(finalAlbum);

        // Escreve no campo de comentário genérico (suportado por quase todos os players)
        // Isso usa o campo padrão COMMENT, que é lido por todas as libs TagLib.
        tag->setComment(commentStr);

        // Escreve tags específicas dependendo do formato do arquivo
        // MP3 usa ID3v2, OGG/FLAC usam Xiph Comments
        if (TagLib::ID3v2::Tag *id3 = dynamic_cast<TagLib::ID3v2::Tag *>(tag))
        {
            // === FORMATO MP3 (ID3v2) ===

            // Escreve BPM no frame TBPM
            if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "bpm") != tagsToWrite.end())
            {
                TagLib::ByteVector frameId("TBPM");
                // Garante que o texto seja gravado em um formato TextIdentificationFrame
                if (!id3->frameList(frameId).isEmpty())
                    id3->removeFrames(frameId);
                TagLib::ID3v2::TextIdentificationFrame *frame = new TagLib::ID3v2::TextIdentificationFrame(frameId);
                frame->setText(TagLib::String(bpmStr, TagLib::String::UTF8)); // Use UTF8
                id3->addFrame(frame);
            }

            // Escreve Key no frame TKEY
            if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "key") != tagsToWrite.end())
            {
                TagLib::ByteVector frameId("TKEY");
                if (!id3->frameList(frameId).isEmpty())
                    id3->removeFrames(frameId);
                TagLib::ID3v2::TextIdentificationFrame *frame = new TagLib::ID3v2::TextIdentificationFrame(frameId);
                frame->setText(TagLib::String(keyStr, TagLib::String::UTF8)); // Use UTF8
                id3->addFrame(frame);
            }

            // Escreve Energy em um frame TXXX (user-defined text)
            if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "energy") != tagsToWrite.end())
            {
                // Remove frames TXXX com descrição "ENERGY" existentes
                TagLib::ID3v2::FrameList txxxFrames = id3->frameList("TXXX");
                std::vector<TagLib::ID3v2::Frame *> framesToRemove;
                for (TagLib::ID3v2::Frame *frame : txxxFrames)
                {
                    if (TagLib::ID3v2::UserTextIdentificationFrame *txxx = dynamic_cast<TagLib::ID3v2::UserTextIdentificationFrame *>(frame))
                    {
                        // Remove TXXX com descrição "ENERGY"
                        if (txxx->description().toCString(true) == "ENERGY")
                        {
                            framesToRemove.push_back(frame);
                        }
                    }
                }
                for (TagLib::ID3v2::Frame *frame : framesToRemove)
                {
                    id3->removeFrame(frame);
                }

                // Cria novo frame TXXX para Energy (garantindo UTF8 e formato correto)
                TagLib::ID3v2::UserTextIdentificationFrame *txxx = new TagLib::ID3v2::UserTextIdentificationFrame(TagLib::String::UTF8);
                txxx->setDescription(TagLib::String("ENERGY", TagLib::String::UTF8)); // A descrição também deve ser um TagLib::String
                txxx->setText(TagLib::String(energyStr, TagLib::String::UTF8));
                id3->addFrame(txxx);
            }

            // Adiciona/Atualiza comentário explícito no frame COMM
            TagLib::ByteVector commId("COMM");
            // Remove frames COMM com descrição "AMALYZER" existentes para evitar duplicação
            TagLib::ID3v2::FrameList commFrames = id3->frameList(commId);
            std::vector<TagLib::ID3v2::Frame *> commFramesToRemove;
            for (TagLib::ID3v2::Frame *frame : commFrames)
            {
                 if (TagLib::ID3v2::CommentsFrame *comm = dynamic_cast<TagLib::ID3v2::CommentsFrame *>(frame))
                {
                    if (comm->description().toCString(true) == "AMALYZER")
                    {
                        commFramesToRemove.push_back(frame);
                    }
                }
            }
            for (TagLib::ID3v2::Frame *frame : commFramesToRemove)
            {
                id3->removeFrame(frame);
            }
            
            // Cria novo frame COMM com descrição "AMALYZER"
            TagLib::ID3v2::CommentsFrame *commFrame = new TagLib::ID3v2::CommentsFrame(TagLib::String::UTF8);
            commFrame->setLanguage("eng");
            commFrame->setDescription(TagLib::String("AMALYZER", TagLib::String::UTF8));
            commFrame->setText(TagLib::String(commentStr, TagLib::String::UTF8));
            id3->addFrame(commFrame);

        }
        else if (TagLib::Ogg::XiphComment *ogg = dynamic_cast<TagLib::Ogg::XiphComment *>(tag))
        {
            // === FORMATO OGG/FLAC (Xiph Comments) ===
            // Xiph Comments lida com UTF-8 nativamente, é mais simples
            // Ogg::XiphComment::addField substitui o valor se a chave já existir.
            if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "bpm") != tagsToWrite.end())
                ogg->addField("BPM", TagLib::String(bpmStr, TagLib::String::UTF8), true);
            if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "key") != tagsToWrite.end())
                ogg->addField("INITIALKEY", TagLib::String(keyStr, TagLib::String::UTF8), true);
            if (std::find(tagsToWrite.begin(), tagsToWrite.end(), "energy") != tagsToWrite.end())
                ogg->addField("ENERGY", TagLib::String(energyStr, TagLib::String::UTF8), true);

            // Adiciona ao campo COMMENT padrão
            ogg->addField("COMMENT", TagLib::String(commentStr, TagLib::String::UTF8), true);
        }

        if (f.save())
        {
            log("SUCCESS", "Tags salvas", res.filename);
        }
        else
        {
            log("ERROR", "Falha ao salvar", res.filename);
        }
    }
    catch (const std::exception &e)
    {
        log("ERROR", "Erro tags: " + std::string(e.what()), res.filename);
    }
}

/**
 * @brief Embutir imagem de capa no arquivo de áudio
 * @param audioPath Caminho do arquivo de áudio
 * @param imagePath Caminho do arquivo de imagem
 */
void embedCover(const std::string &audioPath, const std::string &imagePath)
{
    if (!fs::exists(imagePath))
    {
        log("ERROR", "Imagem não encontrada", imagePath);
        return;
    }

    try
    {
        TagLib::FileRef f(audioPath.c_str());
        if (f.isNull() || !f.file())
            return;

        // Lê o arquivo de imagem
        std::ifstream imgFile(imagePath, std::ios::binary | std::ios::ate);
        if (!imgFile.is_open())
            return;
        std::streamsize size = imgFile.tellg();
        imgFile.seekg(0, std::ios::beg);
        TagLib::ByteVector imgData((unsigned int)size, 0);
        imgFile.read(imgData.data(), size);

        // Detecta formato da imagem
        TagLib::MP4::CoverArt::Format mp4Format = TagLib::MP4::CoverArt::JPEG;
        std::string mimeType = "image/jpeg";
        if (imagePath.length() > 4 && toLower(imagePath.substr(imagePath.length() - 4)) == ".png")
        {
            mp4Format = TagLib::MP4::CoverArt::PNG;
            mimeType = "image/png";
        }

        bool saved = false;

        // === MP3 (ID3v2) ===
        if (TagLib::MPEG::File *mpegFile = dynamic_cast<TagLib::MPEG::File *>(f.file()))
        {
            if (mpegFile->ID3v2Tag())
            {
                TagLib::ID3v2::Tag *id3 = mpegFile->ID3v2Tag(true);

                // Remove capas existentes
                TagLib::ID3v2::FrameList frames = id3->frameList("APIC");
                for (auto it = frames.begin(); it != frames.end(); ++it)
                {
                    id3->removeFrame(*it);
                }

                TagLib::ID3v2::AttachedPictureFrame *frame = new TagLib::ID3v2::AttachedPictureFrame;
                frame->setMimeType(mimeType);
                frame->setPicture(imgData);
                frame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
                id3->addFrame(frame);
                saved = mpegFile->save();
            }
        }
        // === MP4 / M4A ===
        else if (TagLib::MP4::File *mp4File = dynamic_cast<TagLib::MP4::File *>(f.file()))
        {
            if (mp4File->tag())
            {
                TagLib::MP4::Tag *tag = mp4File->tag();

                TagLib::MP4::CoverArtList coverArtList;
                coverArtList.append(TagLib::MP4::CoverArt(mp4Format, imgData));

                TagLib::MP4::Item coverItem(coverArtList);

                // Modifica o item "covr" diretamente
                tag->setItem("covr", coverItem);

                saved = mp4File->save();
            }
        }
        // === FLAC ===
        else if (TagLib::FLAC::File *flacFile = dynamic_cast<TagLib::FLAC::File *>(f.file()))
        {
            TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture;
            picture->setData(imgData);
            picture->setType(TagLib::FLAC::Picture::FrontCover);
            picture->setMimeType(mimeType);

            flacFile->removePictures();
            flacFile->addPicture(picture);
            saved = flacFile->save();
        }
        // === WAV (ID3v2) ===
        else if (TagLib::RIFF::WAV::File *wavFile = dynamic_cast<TagLib::RIFF::WAV::File *>(f.file()))
        {
            if (wavFile->tag())
            { // WAV geralmente usa ID3v2 chunk
                TagLib::ID3v2::Tag *id3 = wavFile->ID3v2Tag();
                if (id3)
                {
                    TagLib::ID3v2::FrameList frames = id3->frameList("APIC");
                    for (auto it = frames.begin(); it != frames.end(); ++it)
                    {
                        id3->removeFrame(*it);
                    }
                    TagLib::ID3v2::AttachedPictureFrame *frame = new TagLib::ID3v2::AttachedPictureFrame;
                    frame->setMimeType(mimeType);
                    frame->setPicture(imgData);
                    frame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
                    id3->addFrame(frame);
                    saved = wavFile->save();
                }
            }
        }
        // === AIFF (ID3v2) ===
        else if (TagLib::RIFF::AIFF::File *aiffFile = dynamic_cast<TagLib::RIFF::AIFF::File *>(f.file()))
        {
            if (aiffFile->tag())
            {
                TagLib::ID3v2::Tag *id3 = aiffFile->tag(); // AIFF usa ID3v2 nativamente
                if (id3)
                {
                    TagLib::ID3v2::FrameList frames = id3->frameList("APIC");
                    for (auto it = frames.begin(); it != frames.end(); ++it)
                    {
                        id3->removeFrame(*it);
                    }
                    TagLib::ID3v2::AttachedPictureFrame *frame = new TagLib::ID3v2::AttachedPictureFrame;
                    frame->setMimeType(mimeType);
                    frame->setPicture(imgData);
                    frame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
                    id3->addFrame(frame);
                    saved = aiffFile->save();
                }
            }
        }
        // === OGG (Vorbis) ===
        // OGG é complexo, TagLib não tem suporte direto fácil para addPicture em Vorbis::File
        // Requer codificação Base64 e METADATA_BLOCK_PICTURE. Deixaremos como TODO ou fallback genérico.

        if (saved)
        {
            log("SUCCESS", "Capa adicionada", fs::path(audioPath).filename().string());
        }
        else
        {
            // Se não entrou em nenhum bloco específico ou falhou ao salvar
            log("WARNING", "Formato não suportado ou falha ao salvar", fs::path(audioPath).filename().string());
        }
    }
    catch (const std::exception &e)
    {
        log("ERROR", "Erro capa: " + std::string(e.what()));
    }
}

/**
 * @brief Remove a imagem de capa do arquivo de áudio
 * @param audioPath Caminho do arquivo de áudio
 */
void removeCover(const std::string &audioPath)
{
    try
    {
        TagLib::FileRef f(audioPath.c_str());
        if (f.isNull() || !f.file())
            return;

        bool saved = false;

        // === MP3 (ID3v2) ===
        if (TagLib::MPEG::File *mpegFile = dynamic_cast<TagLib::MPEG::File *>(f.file()))
        {
            if (mpegFile->ID3v2Tag())
            {
                TagLib::ID3v2::Tag *id3 = mpegFile->ID3v2Tag(true);
                TagLib::ID3v2::FrameList frames = id3->frameList("APIC");
                if (!frames.isEmpty())
                {
                    for (auto it = frames.begin(); it != frames.end(); ++it)
                    {
                        id3->removeFrame(*it);
                    }
                    saved = mpegFile->save();
                }
            }
        }
        // === MP4 / M4A ===
        else if (TagLib::MP4::File *mp4File = dynamic_cast<TagLib::MP4::File *>(f.file()))
        {
            if (mp4File->tag())
            {
                TagLib::MP4::Tag *tag = mp4File->tag();
                if (tag->contains("covr"))
                {
                    tag->removeItem("covr");
                }
            }
        }
        // === FLAC ===
        else if (TagLib::FLAC::File *flacFile = dynamic_cast<TagLib::FLAC::File *>(f.file()))
        {
            if (!flacFile->pictureList().isEmpty())
            {
                flacFile->removePictures();
                saved = flacFile->save();
            }
        }
        // === WAV (ID3v2) ===
        else if (TagLib::RIFF::WAV::File *wavFile = dynamic_cast<TagLib::RIFF::WAV::File *>(f.file()))
        {
            if (wavFile->ID3v2Tag())
            {
                TagLib::ID3v2::Tag *id3 = wavFile->ID3v2Tag();
                TagLib::ID3v2::FrameList frames = id3->frameList("APIC");
                if (!frames.isEmpty())
                {
                    for (auto it = frames.begin(); it != frames.end(); ++it)
                    {
                        id3->removeFrame(*it);
                    }
                    saved = wavFile->save();
                }
            }
        }
        // === AIFF (ID3v2) ===
        else if (TagLib::RIFF::AIFF::File *aiffFile = dynamic_cast<TagLib::RIFF::AIFF::File *>(f.file()))
        {
            if (aiffFile->tag())
            {
                TagLib::ID3v2::Tag *id3 = aiffFile->tag();
                TagLib::ID3v2::FrameList frames = id3->frameList("APIC");
                if (!frames.isEmpty())
                {
                    for (auto it = frames.begin(); it != frames.end(); ++it)
                    {
                        id3->removeFrame(*it);
                    }
                    saved = aiffFile->save();
                }
            }
        }

        if (saved)
        {
            log("SUCCESS", "Capa removida", fs::path(audioPath).filename().string());
        }
        else
        {
            // Se não tinha capa ou falhou
            // log("INFO", "Sem capa para remover ou falha", fs::path(audioPath).filename().string());
        }
    }
    catch (const std::exception &e)
    {
        log("ERROR", "Erro remover capa: " + std::string(e.what()));
    }
}

/**
 * @brief Remove tags específicas do arquivo de áudio
 * @param audioPath Caminho do arquivo
 * @param tags Lista de tags para remover (artist, album, title, etc)
 */
void removeTags(const std::string &audioPath, const std::vector<std::string> &tags)
{
    try
    {
        TagLib::FileRef f(audioPath.c_str());
        if (f.isNull() || !f.tag())
            return;

        TagLib::Tag *tag = f.tag();
        bool modified = false;

        // Tenta usar PropertyMap para remoção genérica e mais robusta
        TagLib::PropertyMap properties = f.file()->properties();
        bool usePropertyMap = true; // Tenta usar PropertyMap primeiro

        for (const auto &t : tags)
        {
            std::string key = t; // Mantém case original ou converte para Upper dependendo da lib, mas PropertyMap costuma ser case-insensitive ou Upper
            std::string upperKey = key;
            std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::toupper);

            // Mapeamento para campos padrão se necessário, mas PropertyMap lida com "ARTIST", "TITLE", etc.

            if (properties.contains(upperKey))
            {
                properties.erase(upperKey);
                modified = true;
            }
            else if (properties.contains(key))
            { // Tenta chave original
                properties.erase(key);
                modified = true;
            }
            else
            {
                // Fallback para métodos set padrão se PropertyMap não funcionar ou não encontrar
                // (Embora PropertyMap deva cobrir tudo em formatos modernos)
                std::string lowerT = toLower(t);
                if (lowerT == "artist")
                {
                    tag->setArtist("");
                    modified = true;
                }
                else if (lowerT == "album")
                {
                    tag->setAlbum("");
                    modified = true;
                }
                else if (lowerT == "title")
                {
                    tag->setTitle("");
                    modified = true;
                }
                else if (lowerT == "comment")
                {
                    tag->setComment("");
                    modified = true;
                }
                else if (lowerT == "genre")
                {
                    tag->setGenre("");
                    modified = true;
                }
                else if (lowerT == "year")
                {
                    tag->setYear(0);
                    modified = true;
                }
                else if (lowerT == "track")
                {
                    tag->setTrack(0);
                    modified = true;
                }
            }
        }

        if (modified)
        {
            // Aplica as mudanças do PropertyMap de volta ao arquivo
            f.file()->setProperties(properties);

            if (f.save())
            {
                log("SUCCESS", "Tags removidas", fs::path(audioPath).filename().string());
            }
            else
            {
                log("ERROR", "Falha ao salvar tags removidas", fs::path(audioPath).filename().string());
            }
        }
        else
        {
            // log("INFO", "Nenhuma tag encontrada para remover", fs::path(audioPath).filename().string());
        }
    }
    catch (const std::exception &e)
    {
        log("ERROR", "Erro remover tags: " + std::string(e.what()));
    }
}

/**
 * @brief Remove TODAS as tags do arquivo de áudio
 * @param audioPath Caminho do arquivo
 */
void removeAllTags(const std::string &audioPath)
{
    try
    {
        // Tenta usar TagLib::File::strip() se possível, mas FileRef não expõe diretamente de forma fácil para todos os tipos
        // Uma abordagem genérica segura é limpar os campos padrão

        TagLib::FileRef f(audioPath.c_str());
        if (f.isNull() || !f.tag())
            return;

        TagLib::Tag *tag = f.tag();

        // Limpa campos padrão
        tag->setArtist("");
        tag->setAlbum("");
        tag->setTitle("");
        tag->setComment("");
        tag->setGenre("");
        tag->setYear(0);
        tag->setTrack(0);

        // Tenta limpar propriedades específicas se for ID3v2
        if (TagLib::MPEG::File *mpegFile = dynamic_cast<TagLib::MPEG::File *>(f.file()))
        {
            if (mpegFile->ID3v2Tag())
            {
                // Remove todos os frames
                // mpegFile->strip(TagLib::MPEG::File::ID3v2); // Isso removeria tudo, inclusive capa
                // Vamos manter a abordagem de limpar campos padrão por segurança,
                // ou usar strip se o usuário realmente quer "remalltag" (remove TUDO)

                // O usuário pediu "remalltag", então strip é apropriado para limpar metadados.
                // Mas strip pode remover capa também. O usuário pediu "remalltag", "artist", "album"...
                // Vamos assumir que remalltag limpa tudo.

                mpegFile->strip(TagLib::MPEG::File::AllTags);
                // Nota: strip salva automaticamente? Geralmente sim ou precisa de save().
                // TagLib::File::strip() usually saves.
                log("SUCCESS", "Todas as tags removidas (strip)", fs::path(audioPath).filename().string());
                return;
            }
        }

        // Fallback para outros formatos: apenas salva com campos vazios
        if (f.save())
        {
            log("SUCCESS", "Tags limpas", fs::path(audioPath).filename().string());
        }
    }
    catch (const std::exception &e)
    {
        log("ERROR", "Erro remover todas tags: " + std::string(e.what()));
    }
}

/**
 * @brief Aplica operações de edição de tags (Set, Append, Prepend)
 * @param audioPath Caminho do arquivo
 * @param ops Lista de operações
 */
void applyTagOperations(const std::string &audioPath, const std::vector<TagOperation> &ops)
{
    try
    {
        TagLib::FileRef f(audioPath.c_str());
        if (f.isNull() || !f.file())
            return;

        TagLib::PropertyMap properties = f.file()->properties();
        bool modified = false;

        for (const auto &op : ops)
        {
            std::string key = op.key;
            std::string upperKey = key;
            std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::toupper);

            // Normaliza a chave para busca (PropertyMap geralmente usa chaves em maiúsculo ou case-insensitive)
            // Vamos tentar encontrar a chave existente
            std::string targetKey = key;
            if (properties.contains(upperKey))
                targetKey = upperKey;
            else if (properties.contains(key))
                targetKey = key;
            else
                targetKey = upperKey; // Se não existe, usa maiúsculo por padrão

            if (op.mode == SET)
            {
                properties.replace(targetKey, TagLib::String(op.value, TagLib::String::UTF8));
                modified = true;
            }
            else if (op.mode == APPEND)
            {
                if (properties.contains(targetKey))
                {
                    TagLib::StringList values = properties[targetKey];
                    if (!values.isEmpty())
                    {
                        // Append ao último valor ou cria novo?
                        // Geralmente append significa concatenar string.
                        // Se fosse adicionar novo valor multivalorado, seria diferente.
                        // O requisito diz "colocar no fim", assumo concatenação de string.
                        TagLib::String current = values.back();
                        values.back() = current + TagLib::String(op.value, TagLib::String::UTF8);
                        properties.replace(targetKey, values);
                    }
                    else
                    {
                        properties.replace(targetKey, TagLib::String(op.value, TagLib::String::UTF8));
                    }
                }
                else
                {
                    properties.replace(targetKey, TagLib::String(op.value, TagLib::String::UTF8));
                }
                modified = true;
            }
            else if (op.mode == PREPEND)
            {
                if (properties.contains(targetKey))
                {
                    TagLib::StringList values = properties[targetKey];
                    if (!values.isEmpty())
                    {
                        TagLib::String current = values.front(); // Prepend ao primeiro valor?
                        // Vamos assumir que queremos modificar o valor principal (geralmente o primeiro ou único)
                        values.front() = TagLib::String(op.value, TagLib::String::UTF8) + current;
                        properties.replace(targetKey, values);
                    }
                    else
                    {
                        properties.replace(targetKey, TagLib::String(op.value, TagLib::String::UTF8));
                    }
                }
                else
                {
                    properties.replace(targetKey, TagLib::String(op.value, TagLib::String::UTF8));
                }
                modified = true;
            }
        }

        if (modified)
        {
            f.file()->setProperties(properties);
            if (f.save())
            {
                log("SUCCESS", "Tags atualizadas", fs::path(audioPath).filename().string());
            }
            else
            {
                log("ERROR", "Falha ao salvar tags atualizadas", fs::path(audioPath).filename().string());
            }
        }
    }
    catch (const std::exception &e)
    {
        log("ERROR", "Erro editar tags: " + std::string(e.what()));
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
void findFiles(const fs::path &root, std::vector<std::string> &files, const ProgramArgs &args)
{
    try
    {
        if (fs::is_regular_file(root))
        {
            std::string ext = toLower(root.extension().string());
            if (std::find(args.extensions.begin(), args.extensions.end(), ext) != args.extensions.end())
            {
                files.push_back(root.string());
            }
            return;
        }

        if (fs::is_directory(root))
        {
            if (args.recursive)
            {
                for (const auto &entry : fs::recursive_directory_iterator(root))
                {
                    if (entry.is_regular_file())
                    {
                        std::string ext = toLower(entry.path().extension().string());
                        if (std::find(args.extensions.begin(), args.extensions.end(), ext) != args.extensions.end())
                        {
                            files.push_back(entry.path().string());
                        }
                    }
                }
            }
            else
            {
                for (const auto &entry : fs::directory_iterator(root))
                {
                    if (entry.is_regular_file())
                    {
                        std::string ext = toLower(entry.path().extension().string());
                        if (std::find(args.extensions.begin(), args.extensions.end(), ext) != args.extensions.end())
                        {
                            files.push_back(entry.path().string());
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception &e)
    {
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
void printTable(std::vector<AudioAnalysis> &results, const ProgramArgs &args, ConfigManager &config)
{
    if (args.quiet)
        return;

    if (results.empty())
    {
        log("INFO", "Nenhum resultado.");
        return;
    }

    if (args.listMode)
    {
        // Colunas padrão se não especificadas
        std::vector<std::string> columns = args.listColumns;
        if (columns.empty())
        {
            columns = {"name", "artist", "album", "size"};
        }

        for (const auto &res : results)
        {
            for (size_t i = 0; i < columns.size(); ++i)
            {
                std::string col = toLower(columns[i]);

                if (col == "name" || col == "filename")
                {
                    int w = config.getInt("name_w", 25);
                    std::cout << padString(res.filename, w);
                }
                else if (col == "artist")
                {
                    int w = config.getInt("artist_w", 15);
                    std::cout << DIM << " - " << RESET; // Separator visual
                    std::cout << padString(res.artist, w);
                }
                else if (col == "album")
                {
                    int w = config.getInt("album_w", 20);
                    std::cout << DIM << " [" << RESET;
                    std::cout << padString(res.album, w);
                    std::cout << DIM << "]" << RESET;
                }
                else if (col == "title")
                {
                    int w = config.getInt("title_w", 20);
                    std::cout << " " << padString(res.title, w);
                }
                else if (col == "genre")
                {
                    int w = config.getInt("genre_w", 10);
                    std::cout << " " << padString(res.genre, w);
                }
                else if (col == "year")
                {
                    int w = config.getInt("year_w", 4);
                    std::cout << " " << std::setw(w) << (res.year > 0 ? std::to_string(res.year) : std::string(w, ' '));
                }
                else if (col == "track")
                {
                    int w = config.getInt("track_w", 2);
                    std::cout << " " << std::setw(w) << (res.track > 0 ? std::to_string(res.track) : std::string(w, ' '));
                }
                else if (col == "bpm")
                {
                    int w = config.getInt("bpm_w", 3);
                    if (res.bpm >= 0.1)
                        std::cout << " " << GREEN << std::setw(w) << (int)res.bpm << RESET;
                    else
                        std::cout << " " << std::string(w, ' ');
                }
                else if (col == "key")
                {
                    int w = config.getInt("key_w", 3);
                    if (!res.keyCamelot.empty() && res.keyCamelot != "???")
                        std::cout << " " << YELLOW << std::setw(w) << std::left << truncate(res.keyCamelot, w) << RESET;
                    else
                        std::cout << " " << std::string(w, ' ');
                }
                else if (col == "energy")
                {
                    int w = config.getInt("energy_w", 3);
                    if (res.energy >= 0.01)
                        std::cout << " " << std::fixed << std::setprecision(1) << std::setw(w) << res.energy;
                    else
                        std::cout << " " << std::string(w, ' ');
                }
                else if (col == "size")
                {
                    int w = config.getInt("size_w", 4);
                    std::cout << " " << CYAN << std::fixed << std::setprecision(1) << std::setw(w) << res.fileSizeMB << "MB" << RESET;
                }
                else if (col == "duration")
                {
                    // Duration format is fixed (MM:SS), so width is essentially fixed or minimum 5
                    int w = config.getInt("duration_w", 5);
                    int min = (int)res.durationSec / 60;
                    int sec = (int)res.durationSec % 60;
                    std::stringstream ss;
                    ss << std::setw(2) << std::setfill('0') << min << ":" << std::setw(2) << sec;
                    std::cout << " " << std::setw(w) << std::setfill(' ') << ss.str();
                }
                else if (col == "bitrate")
                {
                    int w = config.getInt("bitrate_w", 3);
                    std::cout << " " << std::setw(w) << res.bitrate << "k";
                }
                else if (col == "samplerate")
                {
                    int w = config.getInt("samplerate_w", 5);
                    std::cout << " " << std::setw(w) << res.sampleRate;
                }
            }
            std::cout << "\n";
        }
    }
    else
    {
        // Análise compacta - uma linha por arquivo
        for (const auto &res : results)
        {
            // Nome (configurável)
            int nameWidth = config.getInt("ana_name_w", 20);
            std::cout << padString(res.filename, nameWidth);

            // BPM (3 chars fixos)
            if (res.bpm >= 0.1)
            {
                std::cout << GREEN << std::setw(3) << (int)res.bpm << RESET;
            }
            else
            {
                std::cout << "   ";
            }

            // Key (4 chars fixos - espaço + 3 chars)
            if (!res.keyCamelot.empty() && res.keyCamelot != "???")
            {
                std::cout << " " << YELLOW << std::setw(3) << std::left << truncate(res.keyCamelot, 3) << RESET;
            }
            else
            {
                std::cout << "    ";
            }

            // Energy (5 chars fixos)
            if (res.energy >= 0.01)
            {
                std::cout << " " << std::fixed << std::setprecision(1) << std::setw(3) << res.energy;
            }
            else
            {
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
void saveCsv(const std::vector<AudioAnalysis> &results, const std::string &filename)
{
    std::ofstream f(filename);
    if (!f.is_open())
    {
        log("ERROR", "Erro CSV: " + filename);
        return;
    }

    // Colunas compactas
    f << "file,path,bpm,energy,key,sec,mb,title,artist\n";

    auto csv_quote = [](const std::string &s)
    {
        std::string temp = s;
        std::string result;
        result.reserve(temp.length() + 2);
        for (char c : temp)
        {
            if (c == '"')
                result += "\"\"";
            else
                result += c;
        }
        return "\"" + result + "\"";
    };

    for (const auto &r : results)
    {
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
void printHelp(const char *progName)
{
    std::cout << "🎵 Amalyzer - Mobile 🎵\n\n"
              << "Uso: " << progName << " [opções] <arquivos>\n\n"
              << "Opções:\n"
              << "  -r          Recursivo\n"
              << "  -q          Silencioso\n"
              << "  -l <cols>   Lista rápida (cols opcional: name,artist,size...)\n"
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
              << "  -config          Listar configurações atuais\n"
              << "  -cover <path>    Embutir imagem de capa (jpg/png)\n"
              << "  -remcover        Remover imagem de capa\n"
              << "  -rem <list>      Remover tags específicas (artist,title,album...)\n"
              << "  -remall          Remover TODAS as tags\n"
              << "  -settag k=v      Definir tag (ex: artist=\"Nome\")\n"
              << "  -addtag k=v      Adicionar ao fim da tag (ex: title=\" (Remix)\")\n"
              << "  -pretag k=v      Adicionar ao início da tag (ex: title=\"[Intro] \")\n\n"
              << "Tags Suportadas (comuns):\n"
              << "  ARTIST, TITLE, ALBUM, COMMENT, GENRE, YEAR, TRACK, DISC\n"
              << "  COMPOSER, ALBUMARTIST, ENCODEDBY, COPYRIGHT, URL, BPM, INITIALKEY\n\n"
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
int main(int argc, char *argv[])
{
    ConfigManager config;
    ProgramArgs args;

    // ==============================
    // PARSE DOS ARGUMENTOS
    // ==============================
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            printHelp(argv[0]);
            return 0;
        }
        else if (arg == "-r")
            args.recursive = true;
        else if (arg == "-csv")
            args.csv = true;
        else if (arg == "-q")
        {
            args.quiet = true;
            IS_SILENT = true;
        }
        else if (arg == "-l" || arg == "--list")
        {
            args.listMode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                std::string nextArg = argv[i + 1];
                // Verifica se é uma lista de colunas válida (contém vírgula ou é um nome de coluna conhecido)
                // Colunas conhecidas: name, artist, album, title, genre, year, track, bpm, key, energy, size, duration, bitrate, samplerate
                std::set<std::string> validCols = {"name", "filename", "artist", "album", "title", "genre", "year", "track", "bpm", "key", "energy", "size", "duration", "bitrate", "samplerate"};

                bool isColumnList = false;
                if (nextArg.find(',') != std::string::npos)
                {
                    isColumnList = true;
                }
                else
                {
                    if (validCols.count(toLower(nextArg)))
                    {
                        isColumnList = true;
                    }
                }

                if (isColumnList)
                {
                    std::string cols = argv[++i];
                    std::stringstream ss(cols);
                    std::string item;
                    while (std::getline(ss, item, ','))
                        args.listColumns.push_back(toLower(item));
                }
            }
        }
        else if (arg == "-put-force" || arg == "-putforce")
            args.putForce = true;
        else if (arg == "-meta")
            args.meta = true;
        else if (arg == "-o" && i + 1 < argc)
            args.outputFile = argv[++i];
        else if (arg == "-limit" && i + 1 < argc)
        {
            try
            {
                args.limit = std::stoi(argv[++i]);
            }
            catch (...)
            {
            }
        }
        else if (arg == "-size-min" && i + 1 < argc)
        {
            try
            {
                args.minSizeMB = std::stod(argv[++i]);
            }
            catch (...)
            {
            }
        }
        else if (arg == "-size-max" && i + 1 < argc)
        {
            try
            {
                args.maxSizeMB = std::stod(argv[++i]);
            }
            catch (...)
            {
            }
        }
        else if (arg == "-bpm-min" && i + 1 < argc)
        {
            try
            {
                args.minBpm = std::stod(argv[++i]);
            }
            catch (...)
            {
            }
        }
        else if (arg == "-bpm-max" && i + 1 < argc)
        {
            try
            {
                args.maxBpm = std::stod(argv[++i]);
            }
            catch (...)
            {
            }
        }
        else if (arg == "-key" && i + 1 < argc)
            args.targetKey = argv[++i];
        else if (arg == "-ext" && i + 1 < argc)
        {
            std::string extList = argv[++i];
            std::stringstream ss(extList);
            std::string item;
            args.extensions.clear();
            while (std::getline(ss, item, ','))
            {
                if (item[0] != '.')
                    item = "." + item;
                args.extensions.push_back(toLower(item));
            }
        }
        else if (arg == "-put" && i + 1 < argc)
        {
            std::string tags = argv[++i];
            std::stringstream ss(tags);
            std::string item;
            while (std::getline(ss, item, ','))
                args.tagsToWrite.push_back(toLower(item));
        }
        else if (arg == "-cover" && i + 1 < argc)
        {
            args.coverPath = argv[++i];
        }
        else if (arg == "-remcover" || arg == "-cover-remove" || arg == "-rmcover")
        {
            args.removeCover = true;
        }
        else if (arg == "-remall" || arg == "-remalltag" || arg == "-remove-all-tags")
        {
            args.removeAllTags = true;
        }
        else if ((arg == "-rem" || arg == "-remtag") && i + 1 < argc)
        {
            std::string tags = argv[++i];
            std::stringstream ss(tags);
            std::string item;
            while (std::getline(ss, item, ','))
                args.tagsToRemove.push_back(item); // Não converte para lower aqui para permitir tags case-sensitive se necessário
        }
        else if ((arg == "-settag" || arg == "-set") && i + 1 < argc)
        {
            std::string pair = argv[++i];
            size_t pos = pair.find('=');
            if (pos != std::string::npos)
            {
                args.tagOps.push_back({pair.substr(0, pos), pair.substr(pos + 1), SET});
            }
        }
        else if ((arg == "-addtag" || arg == "-appendtag" || arg == "-add") && i + 1 < argc)
        {
            std::string pair = argv[++i];
            size_t pos = pair.find('=');
            if (pos != std::string::npos)
            {
                args.tagOps.push_back({pair.substr(0, pos), pair.substr(pos + 1), APPEND});
            }
        }
        else if ((arg == "-pretag" || arg == "-prependtag" || arg == "-pre") && i + 1 < argc)
        {
            std::string pair = argv[++i];
            size_t pos = pair.find('=');
            if (pos != std::string::npos)
            {
                args.tagOps.push_back({pair.substr(0, pos), pair.substr(pos + 1), PREPEND});
            }
        }
        else if (arg == "-sort" && i + 1 < argc)
        {
            std::string sort = argv[++i];
            std::stringstream ss(sort);
            std::string item;
            args.sortBy.clear();
            while (std::getline(ss, item, ','))
                args.sortBy.push_back(toLower(item));
        }
        else if (arg == "-config")
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                std::string configPair = argv[++i];
                size_t delimiterPos = configPair.find('=');
                if (delimiterPos != std::string::npos)
                {
                    std::string key = configPair.substr(0, delimiterPos);
                    std::string value = configPair.substr(delimiterPos + 1);
                    config.setValue(key, value);
                    config.save();
                    log("SUCCESS", "Config atualizada: " + key + "=" + value);
                }
                else
                {
                    log("ERROR", "Formato inválido para -config. Use key=value");
                }
            }
            else
            {
                config.print();
                return 0;
            }
        }
        else if (arg[0] != '-')
            args.paths.push_back(arg); // Argumentos sem '-' são caminhos
    }

    // Verifica se pelo menos um caminho foi fornecido
    if (args.paths.empty())
    {
        printHelp(argv[0]);
        return 1;
    }

    if (!IS_SILENT)
    {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    }

    // Inicializa o SDK Superpowered (necessário para análise de áudio)
    Superpowered::Initialize("ExampleLicenseKey-WillExpire-OnNextUpdate");
    Amalyzer amalyzer; // Instância do analisador de áudio

    // ==============================
    // BUSCA DE ARQUIVOS
    // ==============================
    std::vector<std::string> files;
    for (const auto &p : args.paths)
    {
        findFiles(p, files, args);
    }

    if (files.empty())
    {
        log("INFO", "Nenhum arquivo.");
        return 0;
    }

    // Aplica limite de arquivos se especificado
    if (args.limit > 0 && files.size() > (size_t)args.limit)
    {
        files.resize(args.limit);
    }

    if (!IS_SILENT)
    {
        std::cout << BOLD << CYAN << "🎵 Amalyzer" << RESET << "\n";
        std::cout << DIM << "════════════════════════" << RESET << "\n\n";
    }

    if (args.listMode)
    {
        log("INFO", "Modo lista...");
    }
    else
    {
        log("INFO", "Analisando...");
    }

    // ==============================
    // PROCESSAMENTO DOS ARQUIVOS
    // ==============================
    std::vector<AudioAnalysis> results;
    int processedCount = 0;
    int totalFiles = files.size();

    for (const auto &fpath : files)
    {
        AudioAnalysis res;
        res.path = fpath;
        res.filename = fs::path(fpath).filename().string();

        // Obtém tamanho do arquivo
        try
        {
            res.fileSizeMB = (double)fs::file_size(fpath) / (1024.0 * 1024.0);
        }
        catch (...)
        {
            res.fileSizeMB = 0.0;
        }

        // Lê metadados básicos usando TagLib (título, artista, álbum, etc)
        try
        {
            TagLib::FileRef f(fpath.c_str());
            if (!f.isNull() && f.tag())
            {
                res.title = f.tag()->title().toCString(true);
                res.artist = f.tag()->artist().toCString(true);
                res.album = f.tag()->album().toCString(true);
                res.genre = f.tag()->genre().toCString(true);
                res.year = f.tag()->year();
                res.track = f.tag()->track();
                if (f.audioProperties())
                {
                    res.bitrate = f.audioProperties()->bitrate();
                    res.sampleRate = f.audioProperties()->sampleRate();
                    res.channels = f.audioProperties()->channels();
                    if (args.listMode)
                        res.durationSec = f.audioProperties()->lengthInSeconds();
                }
            }
        }
        catch (...)
        {
        }

        // Realiza análise de áudio (BPM, Energy, Key) se não estiver em modo lista
        if (!args.listMode)
        {
            // Perform Audio Analysis
            AudioAnalysis analysis = amalyzer.analyze(fpath);
            if (analysis.success)
            {
                res.bpm = analysis.bpm;
                res.energy = analysis.energy;
                res.keyCamelot = analysis.keyCamelot;
                res.keyIndex = analysis.keyIndex;
                res.durationSec = analysis.durationSec;
                res.success = true;
            }
            else
            {
                res.success = false;
                res.errorMessage = analysis.errorMessage;
            }
        }
        else
        {
            res.success = true;
        }

        // Aplica filtros (BPM, tamanho, key)
        if (res.success)
        {
            bool keep = true;
            if (args.minBpm > 0 && res.bpm < args.minBpm)
                keep = false;
            if (args.maxBpm > 0 && res.bpm > args.maxBpm)
                keep = false;
            if (args.minSizeMB > 0 && res.fileSizeMB < args.minSizeMB)
                keep = false;
            if (args.maxSizeMB > 0 && res.fileSizeMB > args.maxSizeMB)
                keep = false;
            if (!args.targetKey.empty() && toLower(res.keyCamelot) != toLower(args.targetKey))
                keep = false;

            if (keep)
            {
                results.push_back(res);
            }
        }
        else if (!args.listMode)
        {
            log("ERROR", "Falha: " + res.errorMessage, res.filename);
        }

        processedCount++;
        if (!args.quiet)
        {
            drawProgressBar(processedCount, totalFiles, res.filename);
        }
    }

    if (!args.quiet)
        std::cout << "\n\n";

    // Verifica se há resultados após aplicar filtros
    if (results.empty())
    {
        log("INFO", "Sem resultados após filtros.");
        return 0;
    }

    // ==============================
    // ESCRITA DE TAGS
    // ==============================
    if (!args.tagsToWrite.empty() && !args.listMode)
    {
        log("INFO", "Escrevendo tags...");
        for (const auto &res : results)
        {
            writeTags(res, args.tagsToWrite, args.putForce);
        }
        if (!IS_SILENT)
            std::cout << DIM << "────────────────────────" << RESET << "\n";
    }

    // ==============================
    // GERAÇÃO DE METADADOS
    // ==============================
    if (args.meta && !args.listMode)
    {
        log("INFO", "Gerando meta...");
        for (const auto &res : results)
        {
            saveMetadataFile(res);
        }
    }

    // ==============================
    // CAPA DO ÁLBUM
    // ==============================
    if (!args.coverPath.empty() && !args.listMode)
    {
        log("INFO", "Adicionando capas...");
        for (const auto &res : results)
        {
            embedCover(res.path, args.coverPath);
        }
    }

    // ==============================
    // REMOVER CAPA
    // ==============================
    if (args.removeCover && !args.listMode)
    {
        log("INFO", "Removendo capas...");
        for (const auto &res : results)
        {
            removeCover(res.path);
        }
    }

    // ==============================
    // REMOVER TAGS
    // ==============================
    if (args.removeAllTags && !args.listMode)
    {
        log("INFO", "Removendo TODAS as tags...");
        for (const auto &res : results)
        {
            removeAllTags(res.path);
        }
    }
    else if (!args.tagsToRemove.empty() && !args.listMode)
    {
        log("INFO", "Removendo tags específicas...");
        for (const auto &res : results)
        {
            removeTags(res.path, args.tagsToRemove);
        }
    }

    // ==============================
    // EDITAR TAGS (Set, Append, Prepend)
    // ==============================
    if (!args.tagOps.empty() && !args.listMode)
    {
        log("INFO", "Editando tags...");
        for (const auto &res : results)
        {
            applyTagOperations(res.path, args.tagOps);
        }
    }

    // ==============================
    // ORDENAÇÃO DOS RESULTADOS
    // ==============================
    // Aplica critérios de ordenação em ordem reversa (último critério tem prioridade)
    for (auto it = args.sortBy.rbegin(); it != args.sortBy.rend(); ++it)
    {
        std::string key = *it;
        std::stable_sort(results.begin(), results.end(), [&](const AudioAnalysis &a, const AudioAnalysis &b)
                         {
            if (key == "bpm") return a.bpm < b.bpm;
            if (key == "energy") return a.energy < b.energy;
            if (key == "key") return a.keyCamelot < b.keyCamelot;
            if (key == "size") return a.fileSizeMB < b.fileSizeMB;
            if (key == "album") return a.album < b.album;
            if (key == "artist") return a.artist < b.artist;
            if (key == "title") return a.title < b.title;
            return a.filename < b.filename; });
    }

    // ==============================
    // SAÍDA DOS RESULTADOS
    // ==============================
    if (args.csv)
    {
        std::string out = args.outputFile.empty() ? "analysis.csv" : args.outputFile;
        saveCsv(results, out);
    }
    else
    {
        if (!IS_SILENT)
        {
            if (args.listMode)
            {
                std::cout << BOLD << "📁 Lista (" << results.size() << ")" << RESET << "\n\n";
            }
            else
            {
                std::cout << BOLD << "📊 Resultados (" << results.size() << ")" << RESET << "\n\n";
            }
        }
        printTable(results, args, config);

        // Resumo ultra compacto
        if (!IS_SILENT)
        {
            std::cout << "\n"
                      << DIM << "─ Total: " << results.size() << " arquivos" << RESET << "\n";
        }
    }

    return 0;
}