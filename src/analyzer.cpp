#include "analyzer.h"
#include <iostream>
#include <vector>
#include <memory>
#include <iomanip>
#include <algorithm>
#include <cmath>

#include "Superpowered.h"
#include "SuperpoweredDecoder.h"
#include "SuperpoweredAnalyzer.h"
#include "SuperpoweredSimple.h"

// Tabelas de tonalidade (Camelot e OpenKey).
// O índice retornado pelo Superpowered aponta para uma dessas strings.
static const char* camelot_keys[24] = {
    "8B","9B","10B","11B","12B","1B","2B","3B","4B","5B","6B","7B",
    "5A","6A","7A","8A","9A","10A","11A","12A","1A","2A","3A","4A"
};

static const char* openkey_keys[24] = {
    "1d","2d","3d","4d","5d","6d","7d","8d","9d","10d","11d","12d",
    "1m","2m","3m","4m","5m","6m","7m","8m","9m","10m","11m","12m"
};

// Construtor: deixado vazio porque as inicializações globais
// do Superpowered normalmente são feitas na função principal.
Analyzer::Analyzer() {}

// Função principal de análise.
// Recebe o caminho do arquivo e devolve um struct com todos os resultados.
AudioAnalysis Analyzer::analyze(const std::string& path) {
    AudioAnalysis result;
    result.path = path;

    // Extrai apenas o nome do ficheiro a partir do caminho completo.
    size_t lastSlash = path.find_last_of("/\\");
    result.filename = (lastSlash == std::string::npos)
                          ? path
                          : path.substr(lastSlash + 1);

    // Cria um decoder para abrir e ler o arquivo de áudio.
    std::unique_ptr<Superpowered::Decoder> decoder(new Superpowered::Decoder());
    int openReturn = decoder->open(path.c_str());

    // Se falhar ao abrir, abandona cedo com mensagem de erro.
    if (openReturn != Superpowered::Decoder::OpenSuccess) {
        result.errorMessage = "Decoder open error: " + std::to_string(openReturn);
        return result;
    }

    // Coletamos dados básicos.
    result.durationSec = decoder->getDurationSeconds();
    result.sampleRate = decoder->getSamplerate();

    unsigned int samplerate = decoder->getSamplerate();
    unsigned int framesPerChunk = decoder->getFramesPerChunk();

    // Se vier dado inválido, melhor parar do que processar lixo.
    if (samplerate == 0 || framesPerChunk == 0) {
        result.errorMessage = "Invalid samplerate or framesPerChunk";
        return result;
    }

    // O analisador precisa de um mínimo de duração,
    // senão alguns algoritmos podem falhar internamente.
    int analysisDuration = (int)result.durationSec + 1;
    if (analysisDuration < 5) analysisDuration = 5;

    // Criamos o analisador de áudio propriamente dito.
    std::unique_ptr<Superpowered::Analyzer> analyzer(
        new Superpowered::Analyzer(samplerate, analysisDuration)
    );

    // Buffers usados na leitura do áudio.
    std::vector<short int> intBuffer(framesPerChunk * 2);
    std::vector<float> floatBuffer(framesPerChunk * 2);

    // Variáveis para cálculo da energia (RMS) média.
    double totalRms = 0.0;
    long long totalFrames = 0;

    // Loop de leitura e processamento até acabar o arquivo.
    while (true) {
        int frames = decoder->decodeAudio(intBuffer.data(), framesPerChunk);
        if (frames < 1) break;  // fim do arquivo

        // Converte audio em short para float.
        Superpowered::ShortIntToFloat(intBuffer.data(), floatBuffer.data(), frames);

        // Alimenta o analisador interno com o bloco atual.
        analyzer->process(floatBuffer.data(), frames);

        // Cálculo do RMS para medir energia do áudio.
        for (int i = 0; i < frames * 2; ++i) {
            totalRms += floatBuffer[i] * floatBuffer[i];
        }
        totalFrames += frames * 2;
    }

    // Gera os resultados finais: BPM, tonalidade, loudness e etc.
    analyzer->makeResults(60, 200, 0, 0, false, 0, false, false, true);

    result.bpm = analyzer->bpm;
    result.averageDb = analyzer->averageDb;
    result.keyIndex = analyzer->keyIndex;

    // Se a key for válida, converte o índice para Camelot/OpenKey.
    if (result.keyIndex >= 0 && result.keyIndex < 24) {
        result.keyCamelot = camelot_keys[result.keyIndex];
        result.keyOpenKey = openkey_keys[result.keyIndex];
    } else {
        result.keyCamelot = "???";
        result.keyOpenKey = "???";
    }

    // Calcula energia RMS final, arredondada.
    if (totalFrames > 0) {
        double rmsAvg = std::sqrt(totalRms / totalFrames);
        result.energy = std::round(rmsAvg * 100.0) / 100.0;
    }

    result.success = true;
    return result;
}
