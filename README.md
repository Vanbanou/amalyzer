# 🎵 Amalyzer - Analisador de Áudio Avançado

Analisador de áudio robusto em C++ para extrair BPM, energia, tonalidade (key) e outros metadados de arquivos de áudio.

## ✨ Características

- 🎼 **Análise de BPM** - Detecção precisa de tempo
- ⚡ **Análise de Energia** - Medição RMS da intensidade do áudio
- 🎹 **Detecção de Tonalidade** - Key em formato Camelot e OpenKey
- 📊 **Múltiplos Formatos** - Suporte para MP3, FLAC, OGG, WAV, M4A, AIF/AIFF
- 🏷️ **Escrita de Tags** - Atualização automática de metadados ID3
- 📁 **Busca Recursiva** - Análise de diretórios completos
- 🎨 **Interface Visual** - Barra de progresso e tabelas coloridas
- 💾 **Exportação** - CSV e JSON metadata

## 🚀 Instalação

### Binários Pré-compilados

Baixe os binários para sua plataforma na [página de releases](../../releases):

- **Linux x64**: `amalyzer-linux-x64.tar.gz`
- **Linux ARM64**: `amalyzer-linux-arm64.tar.gz`

```bash
# Extrair e instalar
tar -xzf amalyzer-linux-x64.tar.gz
cd amalyzer-linux-x64
sudo cp amalyzer /usr/local/bin/
```

### Compilar do Código Fonte

#### Dependências

- CMake 3.10+
- C++17 compiler (GCC 7+, Clang 5+)
- TagLib
- Superpowered SDK (incluído)

#### Ubuntu/Debian

```bash
sudo apt-get install build-essential cmake libtag1-dev
```

#### Compilação

```bash
git clone <seu-repositorio>
cd amalyzer
./build_and_run.sh
```

Ou manualmente:

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 📖 Uso

### Análise Básica

```bash
# Analisar um arquivo
./amalyzer audio.mp3

# Analisar diretório recursivamente
./amalyzer -r ./musicas/
```

### Filtros

```bash
# Filtrar por BPM
./amalyzer -r -bpm-min 120 -bpm-max 140 ./musicas/

# Filtrar por tonalidade
./amalyzer -r -key 8B ./musicas/

# Filtrar por tamanho
./amalyzer -r -size-min 5 -size-max 20 ./musicas/
```

### Ordenação

```bash
# Ordenar por BPM
./amalyzer -r -sort bpm ./musicas/

# Ordenar por múltiplos campos
./amalyzer -r -sort bpm,energy,key ./musicas/
```

### Escrita de Tags

```bash
# Escrever BPM, Energy e Key nas tags
./amalyzer -r -put bpm,energy,key ./musicas/

# Forçar substituição do campo Album
./amalyzer -r -put bpm,energy,key -put-force ./musicas/
```

### Exportação

```bash
# Exportar para CSV
./amalyzer -r -csv -o resultados.csv ./musicas/

# Gerar arquivos .analisemetadata (JSON)
./amalyzer -r -meta ./musicas/
```

### Modo Listagem

```bash
# Listar apenas metadados (sem análise pesada)
./amalyzer -r -l ./musicas/
```

## 🎨 Interface Visual

O Amalyzer possui uma interface visual moderna com:

- ✅ Barra de progresso animada com percentagem
- 📊 Tabelas com bordas duplas Unicode
- 🌈 Dados coloridos (BPM verde, Energy magenta, Key amarelo)
- 📋 Cabeçalhos visuais elegantes

## 🔧 Opções Completas

```
Uso: ./amalyzer [opções] <arquivos/pastas>

Opções:
  -r            Pesquisa recursiva em subdiretórios
  -q            Modo silencioso
  -l, --list    Modo de listagem rápida
  -csv          Gerar saída em CSV
  -o <arquivo>  Salvar saída em arquivo
  -meta         Criar arquivos .analisemetadata (JSON)
  -limit <N>    Analisar apenas os primeiros N arquivos

Filtros:
  -bpm-min <N>  BPM mínimo
  -bpm-max <N>  BPM máximo
  -size-min <N> Tamanho mínimo (MB)
  -size-max <N> Tamanho máximo (MB)
  -key <K>      Key exata (Camelot, ex: '8B')
  -ext <list>   Extensões (ex: mp3,flac)

Saída:
  -sort <list>  Ordenar por campos (name|bpm|size|key|energy|album|artist|title)
  -put <list>   Escrever tags (bpm|energy|key)
  -put-force    Forçar substituição do campo Album
```

## 🏗️ Arquitetura

- **main.cpp** - Interface CLI e lógica principal
- **analyzer.h** - Classe Amalyzer com integração Superpowered
- **Superpowered SDK** - Engine de análise de áudio de alta performance

## 🤝 Contribuindo

Contribuições são bem-vindas! Por favor, abra uma issue ou pull request.

## 🐛 Reportar Bugs

Encontrou um bug? Abra uma [issue](../../issues) com detalhes sobre o problema.
