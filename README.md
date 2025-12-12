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

- **Linux x64**: `amalyzer-linux-x64.zip`
- **Android ARM64 (Termux)**: `amalyzer-android-arm64.zip`

> **Nota**: Os binários são compilados com link estático de todas as bibliotecas, criando executáveis standalone.

```bash
# Linux
unzip amalyzer-linux-x64.zip
cd amalyzer-linux-x64
sudo cp amalyzer /usr/local/bin/

# Android/Termux
unzip amalyzer-android-arm64.zip
cd amalyzer-android-arm64
cp amalyzer $PREFIX/bin/
```

### Dependências de Runtime

Se o binário não executar, instale as dependências básicas:

```bash
# Ubuntu/Debian
sudo apt-get install libtag1v5

# Arch Linux
sudo pacman -S taglib
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
chmod +x ./build.sh
./build.sh
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
🎵 Amalyzer - Mobile 🎵

Uso: ./amalyzer [opções] <arquivos>

Opções:
  -r          Recursivo
  -q          Silencioso
  -l <cols>   Lista rápida (cols opcional: name,artist,size...)
  -csv        Saída CSV
  -o <file>   Salvar em arquivo
  -meta       Gerar .analisemetadata
  -limit <N>  Limitar a N arquivos

Filtros:
  -bpm-min/max N   Filtrar por BPM
  -size-min/max N  Filtrar por tamanho (MB)
  -key <K>         Filtrar por key (ex: 8B)
  -ext <list>      Extensões (ex: mp3,flac)

Saída/Tags:
  -sort <list>     Ordenar (name,bpm,size,key,energy)
  -put <list>      Escrever tags (bpm,energy,key)
  -putforce        Forçar escrita (sobrescrever álbum)
  -config <k=v>    Atualizar configuração (ex: name_w=50)
  -config          Listar configurações atuais
  -cover <path>    Embutir imagem de capa (jpg/png)
  -remcover        Remover imagem de capa
  -rem <list>      Remover tags específicas (artist,title,album...)
  -remall          Remover TODAS as tags
  -settag k=v      Definir tag (ex: artist="Nome")
  -addtag k=v      Adicionar ao fim da tag (ex: title=" (Remix)")
  -pretag k=v      Adicionar ao início da tag (ex: title="[Intro] ")

Tags Suportadas (comuns):
  ARTIST, TITLE, ALBUM, COMMENT, GENRE, YEAR, TRACK, DISC
  COMPOSER, ALBUMARTIST, ENCODEDBY, COPYRIGHT, URL, BPM, INITIALKEY

Ex: ./amalyzer -r -put bpm,key -sort bpm ./musicas
```

## 🤝 Contribuindo

Contribuições são bem-vindas! Por favor, abra uma issue ou pull request.