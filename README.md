# Auric Master – VST3 Mixbus/Mastering-Plugin

All-in-one-Mastering-Kette: **Input → Tone (Bass/Air) → Resonance Control (Frequenz-Clashes) → Glue-Kompressor (Punch) → Stereobreite → Röhren-Sättigung (Warmth) → Lookahead-Limiter (4× Oversampling) → Output/Mix**.

| Regler  | Funktion |
|---------|----------|
| INPUT   | Pegel in die Kette (± 12 dB) – mehr Input = lauter in den Limiter |
| BASS    | Low-Shelf 100 Hz (± 6 dB) |
| AIR     | High-Shelf 12 kHz (± 6 dB) |
| **Resonance Control** | Dynamische spektrale Resonanz-Unterdrückung (FFT, stereo-gelinkt). Eigener Tab **RESONANCE** mit großem Live-Display |
| DEPTH   | Wie stark herausstehende Resonanzen abgesenkt werden |
| SHARPNESS | Hoch = nur schmale, „pfeifende“ Resonanzen; niedrig = auch breite Anhebungen |
| SELECTIVITY | Ab wie viel dB über dem geglätteten Spektrum eingegriffen wird |
| ATTACK / RELEASE | Reaktionszeit der Absenkung (0,5–100 ms / 10–500 ms) |
| MIX / TRIM | Anteil der Bearbeitung und Pegelausgleich der Sektion |
| ON / DELTA / HARD | Ein/Aus · DELTA = nur das Entfernte hören · HARD = präziser, Standard (Soft) = weicher |
| 6 Bänder | LOW (Shelf), MUD, BOX, HARSH, SIBILANCE (Bell), HIGH (Shelf): ziehen = Frequenz/Stärke (−100…+100 %), Mausrad = Breite, Doppelklick = Reset |
| Live-Display | Spektrum mit Peak-Hold, Absenkung in Echtzeit (von oben), Markierungen der aktiven Resonanzen mit Frequenz und dB, Frequenzanzeige unter der Maus, „Reduction History“-Wasserfall |
| **AUTO-MATCH** | Erkennt Frequenz-Clashes automatisch und stellt die 6 Bänder + Depth ein (siehe unten) |
| Profil  | Referenz-Klangbalance: Pop, Hip-Hop / Trap, EDM, Rock, Acoustic – Wechsel passt die Bänder sofort neu an |
| PUNCH   | Glue-Kompression (Threshold & Ratio gekoppelt, Auto-Makeup) |
| WARMTH  | Asymmetrische tanh-Sättigung (gerade Obertöne), oversampled |
| WIDTH   | M/S-Stereobreite 0–200 % |
| CEILING | Limiter-Ceiling (−3 … 0 dBFS) |
| OUTPUT  | Ausgangspegel |
| MIX     | Dry/Wet (latenzkompensiert) |

Hinweis: Durch die FFT-Analyse hat das Plugin ca. 45 ms Latenz. FL Studio gleicht das automatisch aus (PDC).

## Auto-Match – so funktioniert es

1. **Beim ersten Einfügen** auf einen Mixer-Slot startet die Analyse automatisch. Spiele den Song ab – Stille wird ignoriert, es werden ca. 10 s echtes Audio ausgewertet (Fortschritt im Display).
2. Gemessen werden das **Langzeit-Spektrum** und wie oft jede Frequenz als Resonanz heraussticht.
3. Das Spektrum wird mit einer **Referenz-Kurve** für das gewählte Genre verglichen (Annäherung an typische kommerzielle Master, keine offizielle Norm).
4. Pro Band wird die stärkste **lokale Überhöhung** gesucht → Frequenz, Stärke und Breite werden gesetzt. Zu dünne Bereiche bekommen einen negativen Wert (werden geschützt). Die allgemeine Klangbalance zählt nur gedämpft – das ist Aufgabe von Bass/Air bzw. EQ.
5. Die gelb gestrichelte Linie im Display zeigt danach, wo der Mix über/unter der Referenz liegt.

**AUTO**-Button = Analyse neu starten (z. B. im Refrain). Gespeicherte Projekte werden beim Öffnen **nicht** neu analysiert – deine Einstellungen bleiben erhalten. Alle Werte lassen sich danach von Hand nachjustieren.

## Bauen unter Windows (für FL Studio)

**Variante A – ohne lokale Tools (GitHub Actions):**
1. Ordner als GitHub-Repo hochladen.
2. Unter *Actions* läuft „Build Plugins (Windows + macOS)“ automatisch.
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

## macOS

1. GitHub → *Actions* → Artefakt **AuricMaster-macOS** herunterladen und entpacken.
2. Kopieren:
   - `Auric Master.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - `Auric Master.component` → `~/Library/Audio/Plug-Ins/Components/`
3. Terminal (Gatekeeper-Sperre für unsignierte Plugins aufheben):
   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Auric Master.vst3" ~/Library/Audio/Plug-Ins/Components/"Auric Master.component"
   ```
4. FL Studio → *Options → Manage plugins → Find installed plugins* → Stern bei *Auric Master* setzen.
