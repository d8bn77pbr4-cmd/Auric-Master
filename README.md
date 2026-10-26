# Auric Master – VST3 Mixbus/Mastering-Plugin

All-in-one-Mastering-Kette: **Input → Tone (Bass/Air) → Resonance Control (Frequenz-Clashes) → Glue-Kompressor (Punch) → Stereobreite → Röhren-Sättigung (Warmth) → Lookahead-Limiter (4× Oversampling) → Output/Mix**.

| Regler  | Funktion |
|---------|----------|
| INPUT   | Pegel in die Kette (± 12 dB) – mehr Input = lauter in den Limiter |
| BASS    | Low-Shelf 100 Hz (± 6 dB) |
| AIR     | High-Shelf 12 kHz (± 6 dB) |
| **Resonance Control** | Dynamische spektrale Resonanz-Unterdrückung (FFT, stereo-gelinkt) |
| DEPTH   | Wie stark herausstehende Resonanzen abgesenkt werden |
| SHARPNESS | Hoch = nur schmale, „pfeifende“ Resonanzen; niedrig = auch breite Anhebungen |
| SENSITIVITY | Ab wie viel dB über dem geglätteten Spektrum eingegriffen wird |
| SPEED   | Reaktionszeit (Attack/Release) |
| ON / DELTA | Ein/Aus · DELTA = nur das Entfernte hören (zum Finden von Clashes) |
| Clash-Punkte | MUD, BOX, HARSH, SIBILANCE im Display: ziehen = Frequenz/Stärke (−100…+100 %), Mausrad = Breite, Doppelklick = Reset |
| PUNCH   | Glue-Kompression (Threshold & Ratio gekoppelt, Auto-Makeup) |
| WARMTH  | Asymmetrische tanh-Sättigung (gerade Obertöne), oversampled |
| WIDTH   | M/S-Stereobreite 0–200 % |
| CEILING | Limiter-Ceiling (−3 … 0 dBFS) |
| OUTPUT  | Ausgangspegel |
| MIX     | Dry/Wet (latenzkompensiert) |

Hinweis: Durch die FFT-Analyse hat das Plugin ca. 45 ms Latenz. FL Studio gleicht das automatisch aus (PDC).

## Bauen unter Windows (für FL Studio)

**Variante A – ohne lokale Tools (GitHub Actions):**
1. Ordner als GitHub-Repo hochladen.
2. Unter *Actions* läuft „Build Windows VST3“ automatisch.
3. Artefakt `AuricMaster-VST3-Windows` herunterladen, entpacken.

**Variante B – lokal:**
1. Visual Studio 2022 (Workload „Desktopentwicklung mit C++“) und CMake installieren.
2. Im Projektordner:
   ```
   cmake -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release --target AuricMaster_VST3
   ```
3. Ergebnis: `build/AuricMaster_artefacts/Release/VST3/Auric Master.vst3`

## In FL Studio laden
1. `Auric Master.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
2. FL Studio → *Options → Manage plugins → Find plugins*.
3. Im Mixer auf einen Insert-Slot (z. B. Master) → *Auric Master*.
