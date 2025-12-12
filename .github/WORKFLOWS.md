---

# amalyzer - GitHub Actions Workflows

Este projeto utiliza **GitHub Actions** para compilação automática de binários em múltiplas plataformas, incluindo Linux e Android.

---

## 📋 Workflows Disponíveis

### 1. Build on Push

* **Arquivo:** `.github/workflows/build-on-push.yml`
* **Trigger:** Push em branches `main`, `master`, `develop`
* **Objetivo:** Teste rápido de compilação
* **Plataforma:** Linux x64
* **Artefato:** Apenas logs de build
* **Duração:** ~2-3 minutos

### 2. Build Release

* **Arquivo:** `.github/workflows/build-release.yml`
* **Trigger:**

  * Push em `main` ou `master`
  * Tag `v*` (ex: `v1.0.0`)
  * Manual via `workflow_dispatch`
* **Objetivo:** Criar binários de release
* **Plataformas:**

  * Linux x64
  * Linux ARM64
  * Android (ARM64, ARMv7, x86_64, x86)
* **Artefatos:** `.tar.gz` ou `.zip` contendo:

  * Binário `amalyzer`
  * `README.md`
* **Release:** Criada automaticamente quando tag `v*` é detectada

---

## 🚀 Como Usar

### Teste Rápido

1. Faça alterações no código:

```bash
git add .
git commit -m "Minha alteração"
git push
```

2. O workflow `build-on-push` será executado automaticamente e reportará erros de compilação.

### Criar Release

1. Crie uma tag de versão:

```bash
git tag v1.0.0
git push origin v1.0.0
```

2. O workflow `build-release` irá:

   * Compilar binários para todas as plataformas configuradas
   * Criar a release no GitHub
   * Anexar os artefatos correspondentes

3. Baixe os binários na página de [Releases](../../releases).

### Executar Manualmente

1. Vá para a aba **Actions** no GitHub.
2. Selecione **Build Release Binaries**.
3. Clique em **Run workflow**, escolha a branch e execute.

---

## 📦 Artefatos Gerados

| Plataforma     | Arquivo                            |
| -------------- | ---------------------------------- |
| Linux x64      | `amalyzer-linux-x64.tar.gz`        |
| Linux ARM64    | `amalyzer-linux-arm64.tar.gz`      |
| Android ARM64  | `amalyzer-android-arm64.zip`       |
| Android ARMv7  | `amalyzer-android-armeabi-v7a.zip` |
| Android x86_64 | `amalyzer-android-x86_64.zip`      |
| Android x86    | `amalyzer-android-x86.zip`         |

Cada pacote inclui o binário `amalyzer` e o `README.md`.
Artefatos expiram após 90 dias; releases com tag permanecem permanentemente.

---

## 🔧 Configuração

### Dependências Instaladas Automaticamente

* Linux: `build-essential`, `cmake`, `git`, `pkg-config`, `zlib1g-dev`, `libtag1-dev`
* Android: NDK (versão r26d), cross-compilation toolchain

### Cross-compilation Android

* Suporta múltiplas ABIs: ARM64, ARMv7, x86_64 e x86
* NDK + toolchain configurados no workflow

---

## ⚙️ Personalização

### Adicionar Novas Plataformas

Edite o workflow `.github/workflows/build-release.yml` adicionando novos jobs. Exemplo MacOS:

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

### Modificar Dependências

Altere a seção de instalação:

```yaml
- name: Install dependencies
  run: |
    sudo apt-get install -y build-essential cmake libtag1-dev git pkg-config zlib1g-dev
```

---

## 🐛 Troubleshooting

* **Build falha em ARM64**: Verifique QEMU/Docker para cross-compilation
* **Release não criada**: Certifique-se que a tag começa com `v`
* **Artefatos não aparecem**: Confirme que o build completou com sucesso

---

## 📊 Status dos Workflows

Adicione badges no README principal:

```markdown
![Build Status](https://github.com/seu-usuario/amalyzer/workflows/Build%20on%20Push/badge.svg)
![Release](https://github.com/seu-usuario/amalyzer/workflows/Build%20Release%20Binaries/badge.svg)
```
