# Synthgularity — Calabi Yau

Sintetizador VST3 / AU / CLAP (y app Standalone) hecho en JUCE 8. Sigue el flujo de señal
del análisis de Alaskan Bites 3, pero su fuente principal es un **oscilador de síntesis
granular** que carga samples **WAV, AIFF, FLAC, MP3 y OGG**.

Versión 0.1.0, primera build funcional.

## Flujo de señal

```
GRAIN (sample) ─┐
WAVETABLE A ────┤   FM 3x3 + Grain/Noise como moduladores (Phase o Thru-Zero)
WAVETABLE B ────┤
MODULATOR ──────┼──> mezcla de voz ──> BITE ──> FILTRO ──> AMP ──> (suma de voces)
SUB, NOISE ─────┤                     (antes o después del filtro)
PLAITS (24) ────┘
                     ──> STEREO ──> FLANGER ──> PHASER ──> DELAY ──> RINGS ──> CLOUDS ──> salida

Modulación: LFO 1-4, Chaos 1-2 (Lorenz), Macros 1-4, SEQ lanes 1-4, 3 envolventes,
velocity, nota, mod wheel, aftertouch, pitch bend, random  ──> matriz de 16 slots
ARP (32 pasos: on, pitch, vel, tie, ratchet) ──> voces       ORB: morph entre 4 sonidos
```

## Oscilador granular

| Control | Qué hace |
|---|---|
| Mode | **Cloud**: granos asíncronos a DENSITY por segundo; el teclado cambia la velocidad de lectura. **Pitched**: un grano por período de la nota; la nota define la altura y el sample conserva sus formantes (voces, coros). |
| Position / Scan | Dónde se leen los granos (también clic o arrastre sobre la onda). Scan recorre el archivo solo, -2x a 2x tiempo real. |
| Spray | Dispersión aleatoria de la posición. |
| Size / Density | Duración del grano (5–1000 ms) y granos por segundo (1–200). |
| Pitch / Pitch Rnd | Transposición fija y aleatoria por grano. |
| Keytrack / Root Note | Si el teclado transpone (modo Cloud) y qué nota toca el sample a su altura original. |
| Window | Hann, Triangle, Tukey, Gauss, Perc, Swell. |
| Reverse / Stereo | Probabilidad de grano al revés y paneo aleatorio por grano. |

Arrastrá un archivo sobre la forma de onda o usá **LOAD SAMPLE**. Los archivos se normalizan
a -1 dBFS y se cortan a 3 minutos. Sin archivo suena una textura incorporada.

## Interfaz y marca

La interfaz sigue el manual de marca Calabi Yau (paleta Cosmos, Syncopate para display, Montserrat
para texto, marcos achaflanados con pernos, damero, estrellas, rejilla en perspectiva y la gota).
Los tokens están en `Source/ui/LookAndFeel.h` y las primitivas de dibujo en `Source/ui/LookAndFeel.cpp`.
Las fuentes van incrustadas desde `Resources/fonts/`.

**Logotipo:** el manual pide usar siempre el archivo oficial. Copiá `logo-calabi-yau.png` en
`Resources/` y recompilá: aparece en el encabezado. Sin el archivo se muestra el texto "BY CALABI YAU".

## Compilar en Mac

Requisitos: Xcode (o Command Line Tools) y CMake 3.22+ (`brew install cmake`).

```bash
bash setup.sh                       # descarga JUCE, Mutable y CLAP en libs/
cmake -B build -G Xcode
cmake --build build --config Release
```

Los plugins quedan en `build/Synthgularity_artefacts/Release/` (`VST3/`, `AU/`, `CLAP/`,
`Standalone/`). Copialos a `/Library/Audio/Plug-Ins/VST3`, `/Components` y `/CLAP`.

**Sin compilar vos:** subí la carpeta a un repositorio de GitHub. El workflow
`.github/workflows/build.yml` compila Mac (universal, con validación `auval`) y Windows, y deja
los zips en la pestaña *Actions* del repositorio.

## Presets

Se guardan en `Documentos/Calabi Yau/Synthgularity/Presets/*.sgpreset` (XML). Un preset guarda
todos los parámetros, los pasos de SEQ y ARP, las 4 esquinas del ORB y la ruta del sample.
Vienen 10 presets de fábrica (Cloud Pad, Pitched Choir, Frozen Scan, Glitch Rain, Psy Bite,
Hyper Reese, Plaits Chiptune, Ringing Cave, Clouds Drift).

## Estructura del código

| Archivo | Contenido |
|---|---|
| `Source/Params.h/.cpp` | Lista única de los 290 parámetros (IDs, rangos, menús) |
| `Source/dsp/GranularOsc` | Oscilador granular (64 granos por voz, interpolación Hermite) |
| `Source/dsp/Sample` | Carga de WAV/AIFF/FLAC/MP3/OGG y textura por defecto |
| `Source/dsp/Wavetable` | Wavetables anti-aliasing (mipmaps por octava) y carga de WAV como wavetable |
| `Source/dsp/Voice` | Voz completa: fuentes, FM, bite, filtro SVF/Ladder, envolventes, matriz |
| `Source/dsp/Synth` | Asignación de voces (Poly/Mono/Legato), ARP, SEQ lanes, LFO, Chaos |
| `Source/dsp/Fx` | Stereo, Flanger, Phaser, Delay + Rings y Clouds |
| `Source/dsp/MutablePlaits/Rings/Clouds.cpp` | Puentes de sample rate (Plaits 48 kHz, Rings 48 kHz, Clouds 32 kHz). Van en archivos separados porque los tres módulos definen macros con el mismo nombre. |
| `Source/PluginEditor.cpp`, `Source/ui/` | Interfaz (9 páginas) |
| `tests/` | Render offline de todos los presets y capturas de la interfaz |

Para cambiar nombre, fabricante o códigos del plugin: las primeras líneas de `CMakeLists.txt`.

## Probado

En Linux (x86_64): compilan VST3, CLAP y Standalone; los 10 presets renderizan sin NaN ni clipping,
entre 20x y 230x más rápido que tiempo real (8 voces); carga de MP3 y FLAC verificada; las 9 páginas de
la interfaz se renderizan. **No probado todavía en Mac ni en un DAW**: el workflow de GitHub o tu
Xcode son el primer build de macOS.

## Limitaciones conocidas (v0.1)

- El modo *Stretch* de Clouds quedó fuera (no producía audio fuera del hardware); están Granular,
  Looping Delay y Spectral.
- El ORB mezcla parámetros; los pasos de SEQ/ARP no se morfean entre esquinas.
- Rings y Clouds agregan unos ms de latencia (no se informa al host).
- El ARP reserva memoria en el hilo de audio al cambiar de paso; conviene optimizarlo antes del lanzamiento.
- La ruta del sample se guarda, no el audio: si movés el archivo, el preset vuelve a la textura por defecto.

Licencias de terceros: ver `THIRD_PARTY_NOTICES.md`.
