# GitHub Actions - Workflows de Build

Este projeto usa GitHub Actions para compilação automática de binários.

## 📋 Workflows Disponíveis

### 1. Build on Push (`build-on-push.yml`)
- **Trigger**: Push em branches `main`, `master`, `develop`
- **Objetivo**: Teste rápido de compilação
- **Plataforma**: Linux x64
- **Duração**: ~2-3 minutos

### 2. Build Release (`build-release.yml`)
- **Trigger**: 
  - Push em `main`/`master`
  - Tags `v*` (ex: `v1.0.0`)
  - Manual via workflow_dispatch
- **Objetivo**: Criar binários de release
- **Plataformas**: 
  - Linux x64
  - Linux ARM64 (via Docker/QEMU)
- **Artefatos**: Arquivos `.tar.gz` com binários
- **Release**: Automático quando tag `v*` é criada

## 🚀 Como Usar

### Teste Rápido
Apenas faça push do código:
```bash
git add .
git commit -m "Minha alteração"
git push
```

O workflow `build-on-push.yml` irá compilar e testar automaticamente.

### Criar Release

1. **Criar uma tag de versão:**
```bash
git tag v1.0.0
git push origin v1.0.0
```

2. **O workflow irá:**
   - Compilar para Linux x64
   - Compilar para Linux ARM64
   - Criar uma release no GitHub
   - Anexar os binários à release

3. **Baixar os binários:**
   - Vá para a página de [Releases](../../releases)
   - Baixe `amalyzer-linux-x64.tar.gz` ou `amalyzer-linux-arm64.tar.gz`

### Executar Manualmente

1. Vá para a aba **Actions** no GitHub
2. Selecione **Build Release Binaries**
3. Clique em **Run workflow**
4. Escolha a branch e execute

## 📦 Artefatos

Os workflows geram os seguintes artefatos:

- `amalyzer-linux-x64.tar.gz` - Binário para Linux x86_64
- `amalyzer-linux-arm64.tar.gz` - Binário para Linux ARM64

Cada arquivo contém:
- Binário `amalyzer`
- README.md

## 🔧 Configuração

### Dependências Instaladas Automaticamente
- build-essential
- cmake
- libtag1-dev
- git

### Cross-compilation ARM64
O workflow usa:
- **QEMU** para emulação ARM64
- **Docker Buildx** para builds multi-arquitetura
- **arm64v8/ubuntu:22.04** como imagem base

## ⚙️ Personalização

### Adicionar Novas Plataformas

Para adicionar suporte a outras plataformas, edite `.github/workflows/build-release.yml`:

```yaml
build-macos:
  runs-on: macos-latest
  steps:
    - uses: actions/checkout@v4
    - name: Build
      run: |
        mkdir build && cd build
        cmake ..
        make
```

### Modificar Versões de Dependências

Edite a seção de instalação de dependências:

```yaml
- name: Install dependencies
  run: |
    sudo apt-get install -y \
      build-essential \
      cmake \
      libtag1-dev \
      sua-dependencia-aqui
```

## 🐛 Troubleshooting

### Build Falha no ARM64
- Verifique se o QEMU está configurado corretamente
- Aumente o timeout se necessário
- Verifique logs em Actions → Build logs

### Release Não é Criada
- Certifique-se de que a tag começa com `v` (ex: `v1.0.0`)
- Verifique permissões do `GITHUB_TOKEN`
- Confirme que ambos os builds (x64 e ARM64) foram bem-sucedidos

### Artefatos Não Aparecem
- Verifique se o build completou com sucesso
- Artefatos expiram após 90 dias por padrão
- Para releases com tags, os binários ficam permanentes

## 📊 Status dos Workflows

Veja o status atual dos workflows na aba **Actions** do repositório.

Badges de status (adicione ao README principal):

```markdown
![Build Status](https://github.com/seu-usuario/amalyzer/workflows/Build%20on%20Push/badge.svg)
![Release](https://github.com/seu-usuario/amalyzer/workflows/Build%20Release%20Binaries/badge.svg)
```
