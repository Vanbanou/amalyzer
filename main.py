#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
🎵 Amalyzer - Analisador de Áudio Completo
Análise de BPM, Key, Energy, Danceability e muito mais!
"""

import os
import sys
import json
import csv
import re
import argparse
from math import floor
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path
from typing import List, Dict, Optional, Any, Tuple, Set

try:
    import essentia
    import essentia.standard as es
    import essentia.streaming as ess
    from mutagen import File
    from mutagen.id3 import TPE1, TALB, TKEY, TBPM, ID3, TextFrame, Encoding
    from mutagen.flac import FLAC
    from mutagen.mp4 import MP4
    from mutagen.mp4 import MP4Tags as MP4Tag
    from tqdm import tqdm
except ImportError as e:
    print(f"[ 🔥 ] Erro de dependência: {e}. Verifique se 'essentia', 'mutagen' e 'tqdm' estão instalados.",
          file=sys.stderr)
    print(f"[ ℹ️  ] Execute: pip install essentia-streaming mutagen tqdm", file=sys.stderr)
    sys.exit(1)


# ==============================
# 🎨 CONSTANTES DE COR E LOG
# ==============================

class AnsiColors:
    BLUE = "\033[94m"
    YELLOW = "\033[93m"
    RED = "\033[91m"
    GREEN = "\033[92m"
    RESET = "\033[0m"


IS_SILENT = False
LOG_WIDTH = 75


def init_worker(silent_state: bool):
    """Define o estado global IS_SILENT para cada processo worker."""
    global IS_SILENT
    IS_SILENT = silent_state


# ==============================
# 🖨️ FUNÇÃO DE LOG CENTRALIZADA
# ==============================

def log(level: str, message: str, file_path: Optional[Path] = None, icon: str = "📝"):
    """Imprime logs formatados, respeitando o modo silencioso."""
    if IS_SILENT and level != 'ERROR':
        return

    if file_path:
        message = f"({file_path.name}) {message}"

    if level == 'INFO':
        color = AnsiColors.BLUE
        prefix = f"[ {icon:<2} ]"
    elif level == 'WARNING':
        color = AnsiColors.YELLOW
        prefix = f"[ ⚠️ ]"
    elif level == 'ERROR':
        color = AnsiColors.RED
        prefix = f"[ 🔥 ]"
    elif level == 'SUCCESS':
        color = AnsiColors.GREEN
        prefix = f"[ ✅ ]"
    else:
        color = AnsiColors.RESET
        prefix = f"[ {icon:<2} ]"

    stream = sys.stderr if level in ('WARNING', 'ERROR') else sys.stdout
    print(f"{color}{prefix} {message}{AnsiColors.RESET}", file=stream)


# ==============================
# 🗺️ MAPA DE NOTAS (CAMELOT)
# ==============================

CAMELOT_MAP: Dict[str, str] = {
    'B major': '1B', 'F# major': '2B', 'C# major': '3B', 'G# major': '4B',
    'D# major': '5B', 'A# major': '6B', 'F major': '7B', 'C major': '8B',
    'G major': '9B', 'D major': '10B', 'A major': '11B', 'E major': '12B',
    'Db major': '3B', 'Ab major': '4B', 'Eb major': '5B', 'Bb major': '6B',
    'G# minor': '1A', 'D# minor': '2A', 'A# minor': '3A', 'F minor': '4A',
    'C minor': '5A', 'G minor': '6A', 'D minor': '7A', 'A minor': '8A',
    'E minor': '9A', 'B minor': '10A', 'F# minor': '11A', 'C# minor': '12A',
    'Eb minor': '2A', 'Bb minor': '3A',
}


def convert_to_camelot(key: str, scale: str) -> Optional[str]:
    """Converte a notação de key (ex: 'C', 'major') para Camelot (ex: '8B')."""
    if not key or not scale:
        return None
    key_fixed = key.replace('sharp', '#')
    lookup = f"{key_fixed} {scale}"
    return CAMELOT_MAP.get(lookup)


# ==============================
# 🎵 FUNÇÕES DE ANÁLISE
# ==============================

def get_metadata(file_path: Path) -> Dict[str, Any]:
    """Lê metadados básicos e tamanho do arquivo."""
    try:
        title, artist, album, _, genre, track, date, pool, bitrate, length, sample_rate, channels = \
            es.MetadataReader(filename=str(file_path))()
        tags = {name: pool[name] for name in pool.descriptorNames()}

        try:
            file_size_bytes = file_path.stat().st_size
            file_size_mb = round(file_size_bytes / (1024 * 1024), 2)
        except Exception as e:
            log('WARNING', f"Não foi possível ler o tamanho do arquivo: {e}", file_path)
            file_size_mb = 0.0

        return {
            "title": str(title) if title else "",
            "artist": str(artist) if artist else "",
            "album": str(album) if album else "",
            "genre": str(genre) if genre else "",
            "track": str(track) if track else "",
            "date": str(date) if date else "",
            "bitrate": bitrate,
            "length_sec": length,
            "length_str": f"{floor(length / 60):02.0f}:{floor(length % 60):02.0f}",
            "file_size_mb": file_size_mb,
            "sample_rate": sample_rate,
            "channels": channels,
            "tags_raw": tags
        }
    except Exception as e:
        log('WARNING', f"Erro ao ler metadados: {e}", file_path)
        return {}


def get_bpm(file_path: Path) -> Dict[str, Any]:
    """Analisa BPM e retorna um dicionário."""
    try:
        audio = es.MonoLoader(filename=str(file_path))()
        rhythm_extractor = es.RhythmExtractor2013(method="multifeature")
        bpm, _, confidence, _, _ = rhythm_extractor(audio)
        return {
            "bpm": round(float(bpm), 2) if bpm else 0.0,
            "bpm_confidence": round(float(confidence), 2) if confidence else 0.0
        }
    except Exception as e:
        log('WARNING', f"Erro ao processar BPM: {e}", file_path)
        return {"bpm": 0.0, "bpm_confidence": 0.0}


def get_key(file_path: Path) -> Dict[str, Any]:
    """Analisa Tonalidade (Key) e retorna um dicionário, incluindo Camelot."""
    try:
        loader = ess.MonoLoader(filename=str(file_path))
        framecutter = ess.FrameCutter(frameSize=4096, hopSize=2048, silentFrames='noise')
        windowing = ess.Windowing(type='blackmanharris62')
        spectrum = ess.Spectrum()
        spectralpeaks = ess.SpectralPeaks(orderBy='magnitude',
                                          magnitudeThreshold=0.00001,
                                          minFrequency=20,
                                          maxFrequency=3500,
                                          maxPeaks=60)
        hpcp_key = ess.HPCP(size=36, referenceFrequency=440,
                            bandPreset=False, minFrequency=20, maxFrequency=3500,
                            weightType='cosine', nonLinear=False, windowSize=1.)
        key = ess.Key(profileType='edma', numHarmonics=4, pcpSize=36,
                      slope=0.6, usePolyphony=True, useThreeChords=True)
        pool = essentia.Pool()
        loader.audio >> framecutter.signal
        framecutter.frame >> windowing.frame >> spectrum.frame
        spectrum.spectrum >> spectralpeaks.spectrum
        spectralpeaks.magnitudes >> hpcp_key.magnitudes
        spectralpeaks.frequencies >> hpcp_key.frequencies
        hpcp_key.hpcp >> key.pcp
        key.key >> (pool, 'tonal.key_key')
        key.scale >> (pool, 'tonal.key_scale')
        key.strength >> (pool, 'tonal.key_strength')
        essentia.run(loader)
        
        key_detected = str(pool['tonal.key_key'][0]) if pool['tonal.key_key'].size > 0 else ""
        scale_detected = str(pool['tonal.key_scale'][0]) if pool['tonal.key_scale'].size > 0 else ""
        strength = float(pool['tonal.key_strength'][0]) if pool['tonal.key_strength'].size > 0 else 0.0
        
        camelot_key = convert_to_camelot(key_detected, scale_detected)

        return {
            "key": key_detected,
            "scale": scale_detected,
            "key_strength": round(strength, 2),
            "key_camelot": camelot_key
        }
    except Exception as e:
        log('WARNING', f"Erro ao processar Key: {e}", file_path)
        return {"key": "", "scale": "", "key_strength": 0.0, "key_camelot": None}


def get_rhythm_features(file_path: Path) -> Dict[str, Any]:
    """Analisa Danceability."""
    try:
        audio = es.MonoLoader(filename=str(file_path))()
        danceability_algo = es.Danceability()
        danceability, _ = danceability_algo(audio)
        return {
            "danceability": round(float(danceability), 2) if danceability else 0.0,
        }
    except Exception as e:
        log('WARNING', f"Erro ao processar Danceability: {e}", file_path)
        return {"danceability": 0.0}


def get_energy(file_path: Path) -> Dict[str, Any]:
    """Calcula a energia da música."""
    try:
        audio = es.MonoLoader(filename=str(file_path))()
        
        frame_cutter = es.FrameCutter(frameSize=1024, hopSize=512)
        window = es.Windowing(type='hann')
        rms_algo = es.RMS()

        rms_values = []
        for frame in es.FrameGenerator(audio, frameSize=1024, hopSize=512):
            rms = rms_algo(frame)
            rms_values.append(float(rms))

        if not rms_values:
            energy_avg = 0.0
        else:
            energy_avg = sum(rms_values) / len(rms_values)

        return {
            "energy": round(energy_avg, 2),
        }
    except Exception as e:
        log('WARNING', f"Erro ao processar Energia: {e}", file_path)
        return {"energy": 0.0}


def analyze_file(file_path: Path) -> Dict[str, Any]:
    """Executa TODAS as análises em um único arquivo e combina os resultados."""
    data = {"filename": file_path.name, "path": str(file_path)}
    data.update(get_metadata(file_path))
    data.update(get_bpm(file_path))
    data.update(get_key(file_path))
    data.update(get_rhythm_features(file_path))
    data.update(get_energy(file_path))
    return data


def get_basic_metadata(file_path: Path) -> Dict[str, Any]:
    """Executa apenas a leitura de metadados básicos e tamanho (modo --list)."""
    data = {"filename": file_path.name, "path": str(file_path)}
    data.update(get_metadata(file_path))
    return data


# ==============================
# 🖥️ FUNÇÕES DE EXIBIÇÃO E SAÍDA
# ==============================

def print_analysis_table_simple(results: List[Dict[str, Any]], sort_by_list: List[str],
                                output_file: Optional[str] = None):
    """Imprime a tabela de ANÁLISE padrão com separadores |."""
    if not results:
        log('INFO', "Nenhum resultado para exibir.")
        return

    def get_sort_key(key: str):
        internal_key = {
            'name': 'filename', 'bpm': 'bpm', 'size': 'file_size_mb', 'key': 'key_camelot',
            'dance': 'danceability', 'energy': 'energy', 'album': 'album', 'artist': 'artist', 'title': 'title'
        }.get(key, key)
        default_val = '0' if internal_key in ['key_camelot', 'album', 'filename', 'artist', 'title'] else 0
        return lambda x: x.get(internal_key, default_val)

    for key in reversed(sort_by_list):
        results.sort(key=get_sort_key(key))

    cols_def: List[Tuple[str, str, int]] = [
        ("filename", "ARQUIVO", 20),
        ("file_size_mb", "TAM(MB)", 7),
        ("bpm", "BPM", 6),
        ("energy", "ENERG", 6),
        ("key_camelot", "KEY", 5),
        ("danceability", "DANCE", 6),
        ("artist", "ARTISTA", 20),
        ("album", "ÁLBUM", 20),
    ]

    total_width = sum(w for _, _, w in cols_def) + len(cols_def) * 2 + (len(cols_def) - 1)
    lines = []
    header_parts = [f" {name:<{width}} " for _, name, width in cols_def]
    header = " | ".join(header_parts)
    lines.append("-" * total_width)
    lines.append(header)
    lines.append("-" * total_width)

    for item in results:
        line_parts = []
        for key, _, width in cols_def:
            value = item.get(key)
            if isinstance(value, float):
                if key == 'file_size_mb':
                    value = f"{value:.2f}"
                elif key == 'energy':
                    value = f"{value:.2f}"
                else:
                    value = f"{value:.2f}"
            value = str(value or '---')
            value = (value[:width - 1] + '…') if len(value) > width else value
            line_parts.append(f" {value:<{width}} ")
        lines.append(" | ".join(line_parts))

    lines.append("-" * total_width)
    output_text = "\n".join(lines)

    if output_file:
        try:
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(output_text)
            log('SUCCESS', f"Tabela salva em: {output_file}")
        except Exception as e:
            log('ERROR', f"Erro ao salvar a tabela: {e}")

    if not IS_SILENT:
        print(output_text)


def print_list_table(results: List[Dict[str, Any]], sort_by_list: List[str],
                     output_file: Optional[str] = None):
    """Imprime uma tabela simples de metadados (modo --list)."""
    if not results:
        log('INFO', "Nenhum resultado para exibir.")
        return

    def get_sort_key(key: str):
        internal_key = {'name': 'filename', 'size': 'file_size_mb',
                        'album': 'album', 'artist': 'artist',
                        'title': 'title'}.get(key, 'filename')
        default_val = '0' if internal_key in ['album', 'artist', 'title', 'filename'] else 0
        return lambda x: x.get(internal_key, default_val)

    valid_sort_keys = [k for k in sort_by_list if k in ['name', 'size', 'album', 'artist', 'title']]
    if not valid_sort_keys:
        valid_sort_keys = ['name']

    for key in reversed(valid_sort_keys):
        results.sort(key=get_sort_key(key))

    cols_def: List[Tuple[str, str, int]] = [
        ("filename", "ARQUIVO", 30),
        ("title", "TÍTULO", 30),
        ("artist", "ARTISTA", 25),
        ("album", "ÁLBUM", 25),
        ("file_size_mb", "TAM(MB)", 7),
    ]

    total_width = sum(w for _, _, w in cols_def) + len(cols_def) * 2 + (len(cols_def) - 1)
    lines = []
    header_parts = [f" {name:<{width}} " for _, name, width in cols_def]
    header = " | ".join(header_parts)
    lines.append("-" * total_width)
    lines.append(header)
    lines.append("-" * total_width)

    for item in results:
        line_parts = []
        for key, _, width in cols_def:
            value = item.get(key)
            if isinstance(value, float):
                value = f"{value:.2f}"
            value = str(value or '---')
            value = (value[:width - 1] + '…') if len(value) > width else value
            line_parts.append(f" {value:<{width}} ")
        lines.append(" | ".join(line_parts))

    lines.append("-" * total_width)
    output_text = "\n".join(lines)

    if output_file:
        try:
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(output_text)
            log('SUCCESS', f"Lista salva em: {output_file}")
        except Exception as e:
            log('ERROR', f"Erro ao salvar a lista: {e}")

    if not IS_SILENT:
        print(output_text)


def save_to_json(results: List[Dict[str, Any]], output_file: str):
    if not results:
        return
    log('INFO', f"Salvando resultados em JSON: {output_file}", icon="💾")
    try:
        clean_results = [{k: v for k, v in res.items() if k not in ('instrumentalness', 'tags_raw')} for res in results]
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(clean_results, f, indent=4, ensure_ascii=False)
        log('SUCCESS', "JSON salvo com sucesso!")
    except Exception as e:
        log('ERROR', f"Erro ao salvar JSON: {e}")


def save_to_csv(results: List[Dict[str, Any]], output_file: str):
    if not results:
        return
    log('INFO', f"Salvando resultados em CSV: {output_file}", icon="💾")

    all_keys: Set[str] = set()
    for res in results:
        all_keys.update(res.keys())

    preferred_order = ['filename', 'path', 'bpm', 'energy', 'key_camelot', 'danceability', 'key', 'scale',
                       'length_sec', 'file_size_mb', 'title', 'artist', 'album', 'genre']

    fieldnames = preferred_order + sorted(list(all_keys - set(preferred_order) - {'instrumentalness', 'tags_raw'}))

    try:
        with open(output_file, 'w', newline='', encoding='utf-8') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction='ignore')
            writer.writeheader()
            clean_results = [{k: v for k, v in res.items() if k not in ('instrumentalness', 'tags_raw')} for res in
                             results]
            writer.writerows(clean_results)
        log('SUCCESS', "CSV salvo com sucesso!")
    except Exception as e:
        log('ERROR', f"Erro ao salvar CSV: {e}")


def save_metadata_file(data: Dict[str, Any]):
    file_path_str = data.get('path')
    if not file_path_str:
        return

    output_file = Path(file_path_str).with_suffix(Path(file_path_str).suffix + ".analisemetadata")
    log('INFO', f"Gerando metadados: {output_file.name}", icon="💾")

    data_to_save = {k: v for k, v in data.items() if k not in ('path', 'tags_raw', 'instrumentalness')}

    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(data_to_save, f, indent=4, ensure_ascii=False)
    except Exception as e:
        log('ERROR', f"Erro ao salvar .analisemetadata: {e}", output_file)


# ==============================
# 🏷️ FUNÇÃO DE ESCRITA DE TAGS
# ==============================

def clean_album_prefix(album_str: Optional[str]) -> str:
    """Remove o prefixo BPM|ENERGY|KEY do início da tag Album."""
    if not album_str:
        return ""
    regex_pattern = r'^\s*([0-9\.]+\s*\|\s*)?([0-9\.]+\s*\|\s*)?([A-Za-z0-9\#]+\s*\|\s*)'
    album_cleaned = re.sub(regex_pattern, '', album_str, 1)
    return album_cleaned.strip()


def write_tags_to_file(file_path: Path, new_bpm: Optional[float], new_energy: Optional[float],
                       new_key: Optional[str], tags_to_write: List[str],
                       is_force: bool = False):
    """
    Escreve as tags BPM, Energy e Key no arquivo, preservando a capa.
    """
    bpm_value_float = new_bpm
    bpm_value_album = round(bpm_value_float) if bpm_value_float else None
    bpm_value_tag = int(bpm_value_float) if bpm_value_float else None

    energy_value_album = f"{new_energy:.2f}" if new_energy is not None else None
    key_str = new_key or ""

    new_tag_parts = []
    if bpm_value_album is not None:
        new_tag_parts.append(f"{bpm_value_album}")
    if energy_value_album is not None:
        new_tag_parts.append(energy_value_album)
    if key_str:
        new_tag_parts.append(key_str)
    new_tag_combined = " | ".join(new_tag_parts)

    if not new_tag_combined:
        log('INFO', "Nada para escrever (BPM, Energy ou Key não fornecidos).", file_path, icon="➖")
        return

    log_messages = []
    if 'bpm' in tags_to_write and bpm_value_tag is not None:
        log_messages.append(f"BPM ({bpm_value_tag})")
    if 'energy' in tags_to_write and energy_value_album is not None:
        log_messages.append(f"Energy ({energy_value_album})")
    if 'key' in tags_to_write and key_str:
        log_messages.append(f"Key ({key_str})")

    log_messages.append(f"ALBUM {'(FORÇADO)' if is_force else '(PREFIXADO)'}")

    if log_messages:
        log('INFO', f"Escrevendo tags: {', '.join(log_messages)}", file_path, icon="🏷️")

    try:
        audio_file = File(str(file_path))
        if audio_file is None:
            if file_path.suffix.lower() in ('.m4a', '.mp4'):
                audio_file = MP4(str(file_path))
            else:
                raise ValueError("Formato de arquivo não suportado pelo Mutagen.")

        preserved_cover = None
        if isinstance(audio_file.tags, ID3):
            preserved_cover = audio_file.tags.getall("APIC")
        elif isinstance(audio_file, FLAC):
            preserved_cover = list(audio_file.pictures)
        elif isinstance(audio_file, MP4):
            preserved_cover = audio_file.tags.get('covr', None)

        tags = audio_file.tags
        if tags is None:
            audio_file.add_tags()
            tags = audio_file.tags
            if tags is None:
                raise ValueError(f"Não foi possível inicializar tags")

        if is_force:
            final_album = new_tag_combined
        else:
            old_album_raw = None
            if isinstance(tags, ID3):
                old_album_raw = tags.get('TALB', [None])[0]
            elif isinstance(tags, MP4Tag):
                old_album_raw = tags.get('\xa9alb', [None])[0]
            elif isinstance(tags, FLAC):
                old_album_raw = tags.get('album', [None])[0]

            old_album_str = ""
            if isinstance(old_album_raw, bytes):
                try:
                    old_album_str = old_album_raw.decode('utf-8')
                except UnicodeDecodeError:
                    old_album_str = old_album_raw.decode('latin-1', errors='ignore')
            elif old_album_raw:
                old_album_str = str(old_album_raw)

            cleaned_album = clean_album_prefix(old_album_str.strip())
            final_album = f"{new_tag_combined} | {cleaned_album}" if cleaned_album else new_tag_combined

        if isinstance(tags, ID3):
            if 'key' in tags_to_write:
                tags.delall('TKEY')
                tags.add(TKEY(encoding=Encoding.UTF8, text=[key_str]))
            if 'bpm' in tags_to_write:
                tags.delall('TBPM')
                tags.add(TBPM(encoding=Encoding.UTF8, text=[f"{bpm_value_tag}"]))
            if 'energy' in tags_to_write and energy_value_album is not None:
                tags.delall('TXXX:ENERGY')
                tags.add(TextFrame(encoding=Encoding.UTF8, desc='ENERGY', text=[energy_value_album]))
            tags.delall('TALB')
            tags.add(TALB(encoding=Encoding.UTF8, text=[final_album]))

        elif isinstance(tags, MP4Tag):
            if 'key' in tags_to_write:
                tags['----:com.apple.iTunes:initialkey'] = [key_str.encode('utf-8')]
            if 'bpm' in tags_to_write:
                tags['tmpo'] = [bpm_value_tag]
            if 'energy' in tags_to_write and energy_value_album is not None:
                tags['----:com.apple.iTunes:energy'] = [energy_value_album.encode('utf-8')]
            tags['\xa9alb'] = [final_album]

        elif isinstance(tags, FLAC):
            if 'key' in tags_to_write:
                tags['initialkey'] = [key_str]
            if 'bpm' in tags_to_write:
                tags['bpm'] = [f"{bpm_value_tag}"]
            if 'energy' in tags_to_write and energy_value_album is not None:
                tags['energy'] = [energy_value_album]
            tags['album'] = [final_album]
        else:
            log('WARNING', f"Não há lógica de escrita de tags (Mutagen) para o formato: {type(tags)}", file_path)
            return

        if preserved_cover:
            if isinstance(audio_file.tags, ID3):
                for pic in preserved_cover:
                    audio_file.tags.add(pic)
            elif isinstance(audio_file, FLAC):
                audio_file.clear_pictures()
                for pic in preserved_cover:
                    audio_file.add_picture(pic)
            elif isinstance(audio_file, MP4):
                audio_file.tags['covr'] = preserved_cover

        audio_file.save(v2_version=3)
        log('SUCCESS', f"Tags aplicadas. Álbum: '{final_album}'", file_path)

    except Exception as e:
        log('ERROR', f"ERRO FATAL ao escrever tags com Mutagen: {e}", file_path)


# ==============================
# 🚀 FUNÇÕES DE BUSCA E EXECUÇÃO
# ==============================

def find_audio_files(paths: List[str], recursive: bool, exts: Optional[str], limit: Optional[int]) -> List[Path]:
    audio_files: List[Path] = []

    if exts:
        supported_exts = tuple(f'.{e.strip().lower()}' for e in exts.split(','))
    else:
        supported_exts = ('.mp3', '.flac', '.ogg', '.wav', '.m4a', '.aif', '.aiff')

    for path_str in paths:
        path = Path(path_str)
        if path.is_file():
            if path.suffix.lower() in supported_exts:
                audio_files.append(path)
        elif path.is_dir():
            pattern = '**/*' if recursive else '*'
            for f in path.glob(pattern):
                if f.is_file() and f.suffix.lower() in supported_exts:
                    audio_files.append(f)

    return audio_files[:limit] if limit is not None and limit > 0 else audio_files


def filter_results(results: List[Dict[str, Any]], args: argparse.Namespace) -> List[Dict[str, Any]]:
    """Aplica os filtros de BPM, Tamanho e Key nos resultados."""
    filtered = []
    bpm_min, bpm_max = args.bpm_min, args.bpm_max
    size_min, size_max = args.size_min, args.size_max
    target_key = args.key

    for item in results:
        bpm = item.get('bpm')
        if bpm is not None:
            if (bpm_min is not None and bpm < bpm_min) or \
                    (bpm_max is not None and bpm > bpm_max):
                continue

        size = item.get('file_size_mb')
        if size is not None:
            if (size_min is not None and size < size_min) or \
                    (size_max is not None and size > size_max):
                continue

        key = item.get('key_camelot')
        if target_key and key != target_key:
            continue

        filtered.append(item)
    return filtered


def run_analysis(args: argparse.Namespace):
    """Gerencia a lógica principal de análise e saída."""

    global IS_SILENT
    IS_SILENT = args.q

    tags_to_write = args.put.split(',') if args.put else []
    is_put_force = args.put_force

    if not IS_SILENT:
        os.system('cls' if os.name == 'nt' else 'clear')

    if not args.paths:
        log('ERROR', "Nenhum arquivo ou pasta fornecida.")
        sys.exit(1)

    audio_files = find_audio_files(
        args.paths, recursive=args.r, exts=args.ext, limit=args.limit
    )

    if not audio_files:
        log('INFO', "Nenhum arquivo de áudio encontrado.")
        return

    if args.list:
        analysis_func = get_basic_metadata
        print_func = print_list_table
        log('INFO', "Iniciando modo de listagem rápida (sem análise)...", icon="🎵")
        if tags_to_write or is_put_force or args.meta or args.key or args.bpm_min or args.bpm_max:
            log('WARNING',
                "Opções de análise/escrita (--put, --put-force, --meta, --key, --bpm*) são ignoradas no modo --list.")
        tags_to_write = []
        is_put_force = False
        run_meta = False
    else:
        analysis_func = analyze_file
        print_func = print_analysis_table_simple
        log('INFO', f"Iniciando análise em paralelo com {os.cpu_count() or 4} processos...", icon="🚀")
        run_meta = args.meta

    results = []
    max_workers = os.cpu_count() or 4

    try:
        with ProcessPoolExecutor(max_workers=max_workers,
                                 initializer=init_worker,
                                 initargs=(IS_SILENT,)) as executor:

            future_to_file = {executor.submit(analysis_func, f): f for f in audio_files}

            pbar = tqdm(as_completed(future_to_file),
                        total=len(audio_files),
                        desc="Processando arquivos",
                        disable=IS_SILENT,
                        ncols=LOG_WIDTH)

            for future in pbar:
                file_path = future_to_file[future]
                try:
                    data = future.result()
                    if data:
                        results.append(data)
                except Exception as exc:
                    log('ERROR', f"Erro no processamento: {exc}", file_path)

    except Exception as e:
        log('ERROR', f"Erro fatal no pool de processamento: {e}")
        return

    final_results = filter_results(results, args)

    if not final_results:
        log('INFO', "Nenhum arquivo permaneceu após a filtragem.")
        return

    if tags_to_write:
        log('INFO', f"Iniciando escrita de Tags ({', '.join(tags_to_write)})", icon="📝")
        for item in final_results:
            new_bpm = item.get('bpm') if 'bpm' in tags_to_write else None
            new_energy = item.get('energy') if 'energy' in tags_to_write else None
            new_key = item.get('key_camelot') if 'key' in tags_to_write else None

            if is_put_force and not ('bpm' in tags_to_write and 'key' in tags_to_write and 'energy' in tags_to_write):
                log('WARNING',
                    "O argumento -put-force deve ser usado com -put bpm,energy,key para garantir o álbum completo. Escrevendo apenas o que foi fornecido.",
                    Path(item['path']))

            write_tags_to_file(
                file_path=Path(item['path']),
                new_bpm=new_bpm,
                new_energy=new_energy,
                new_key=new_key,
                tags_to_write=tags_to_write,
                is_force=is_put_force
            )
        if not IS_SILENT:
            print("-" * LOG_WIDTH)

    if run_meta:
        log('INFO', "Gerando arquivos .analisemetadata", icon="📝")
        for item in final_results:
            save_metadata_file(item)

    sort_list = args.sort.split(',') if args.sort else ['name']

    if args.csv:
        output_file_csv = args.o or "analysis_results.csv"
        save_to_csv(final_results, output_file_csv)
    else:
        if not IS_SILENT:
            if args.list:
                print("\n## 🎵 Lista de Músicas (Metadados)\n")
            else:
                print("\n## 📊 Resultados da Análise\n")

        print_func(final_results, sort_list, output_file=args.o)


def parse_arguments() -> argparse.Namespace:
    """Configura e processa os argumentos da linha de comando."""

    parser = argparse.ArgumentParser(
        description="🎵 Amalyzer - Analisador de Áudio Robusto 🎵",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog="Exemplo (Análise): %(prog)s -r -put bpm,energy,key -sort bpm ./musicas\n"
               "Exemplo (Listagem): %(prog)s -r -l -sort artist ./musicas"
    )

    parser.add_argument(
        "paths",
        nargs='+',
        help="Um ou mais arquivos ou pastas para analisar."
    )

    filter_group = parser.add_argument_group("Opções de Filtro")
    filter_group.add_argument("-bpm-min", type=float, help="Filtrar por BPM mínimo (ignorado no modo --list).")
    filter_group.add_argument("-bpm-max", type=float, help="Filtrar por BPM máximo (ignorado no modo --list).")
    filter_group.add_argument("-size-min", type=float, help="Filtrar por Tamanho mínimo (em MB).")
    filter_group.add_argument("-size-max", type=float, help="Filtrar por Tamanho máximo (em MB).")
    filter_group.add_argument("-key", type=str,
                              help="Filtrar por key exata (Camelot, ex: '8B') (ignorado no modo --list).")
    filter_group.add_argument("-ext", type=str,
                              help="Extensões permitidas (ex: mp3,flac,wav). Padrão: todas suportadas.")
    parser.add_argument("-limit", type=int, help="Analisar apenas os primeiros X arquivos encontrados.")

    output_group = parser.add_argument_group("Opções de Saída e Processamento")
    output_group.add_argument(
        "-sort",
        type=str,
        help="Ordenar por campos (ex: 'bpm,energy,key').\n"
             "  Opções (Análise): name|bpm|size|key|dance|energy|album|artist|title\n"
             "  Opções (Lista): name|size|album|artist|title"
    )
    output_group.add_argument(
        "-put",
        type=str,
        help="ESCREVE tags no arquivo (ex: -put bpm,energy,key). Opções: bpm|energy|key (ignorado no modo --list)."
    )
    output_group.add_argument(
        "-put-force",
        action="store_true",
        help="FORÇA a escrita das tags no campo Álbum, SUBSTITUINDO o álbum original."
    )
    output_group.add_argument(
        "-l", "--list",
        action="store_true",
        help="Modo de listagem rápida: lista apenas metadados (título, artista, etc.) sem análise pesada."
    )
    output_group.add_argument("-csv", action="store_true", help="Gerar saída em formato CSV em vez de tabela.")
    output_group.add_argument("-o", type=str, help="Salvar a saída (tabela ou CSV) em um arquivo.")
    output_group.add_argument("-meta", action="store_true",
                              help="Criar um arquivo .analisemetadata (JSON) para cada áudio (ignorado no modo --list).")
    output_group.add_argument("-r", action="store_true", help="Pesquisa recursiva em subdiretórios.")
    output_group.add_argument("-q", action="store_true", help="Modo silencioso (sem logs de progresso ou tabela).")

    return parser.parse_args()


def main():
    """Função principal para executar o script."""
    try:
        args = parse_arguments()
        run_analysis(args)

    except SystemExit:
        pass
    except Exception as e:
        print(f"{AnsiColors.RED}[ 🔥 ] Erro inesperado na execução: {e}{AnsiColors.RESET}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()