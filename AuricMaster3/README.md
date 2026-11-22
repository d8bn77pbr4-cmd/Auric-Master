# Auric Master 3 – Mix- & Mastering-Suite (VST3 / AU)

Neu aufgebaute Version nach einer objektiven Analyse von Auric Master 2. Ziel: ein Plugin, das sich wie ein Profi-Werkzeug verhält und in jeder normalen Plugin-Kette funktioniert – auf Kanälen, Bussen und am Master. Auric Master 2 bleibt unverändert installierbar.

## Was sich gegenüber Auric Master 2 grundlegend geändert hat

| Thema | Auric Master 2 | Auric Master 3 |
|---|---|---|
| Lautheitsziel | regelte ständig nach (Strophe wurde lauter als der Refrain) | **LERNEN**: misst einmal, stellt den Drive ein, danach fest |
| Kompressoren | Schwelle lief mit dem Songpegel mit | klassisch: Threshold, Ratio, Attack, Release, Makeup |
| Verhalten | abhängig davon, wo man abspielt (bis 4,8 LU) | deterministisch: gleiche Einstellung = gleiches Ergebnis |
| Neu eingefügt | +13 dB lauter, 48 ms Latenz | **verändert nichts**, 0 ms Latenz, bit-transparent |
| Module | nicht einzeln abschaltbar | jedes Modul einzeln ein/aus, klickfrei |
| Latenz | immer ca. 48 ms | nur für aktive Module (Resonanz 43 ms, Limiter 5 ms) |
| DAW-Bypass | ohne Latenzausgleich | latenzausgeglichen (= ORIGINAL) |
| Fehlerhafte Signale (NaN) | Plugin blieb stumm | werden abgefangen, Plugin läuft weiter |
| Exciter-Aliasing | −37 dB | −79 dB |
| CPU bei 192 kHz | 36 % | 24 % (Überabtastung passt sich der Samplerate an) |

## Signalkette

```
Input → RESONANZ (FFT) → EQ + KLANG → [Frequenzweiche] DYNAMIK · FARBE · STEREO → GLUE
      → MIX (parallel) → LIMITER (Drive · Clipper · True-Peak-Limiter) → Output
```
Die Frequenzweiche läuft nur, wenn ein Bandmodul aktiv ist, deshalb ist das Plugin mit Standardeinstellungen exakt transparent.

## Module und Seiten

- **EINFACH:** Modul-Schalter in Signalreihenfolge, Klangkurve, KLANG (Tiefen/Mitten/Höhen), Glue-Threshold, Breite, Input/Output/Mix, Lautheitsziel mit LERNEN, Drive, Ceiling, Clipper, Limiter-Charakter.
- **EQ:** Low Cut, High Cut (je 24 dB/Okt.), 4 Bänder (Low Shelf, 2 Glocken, High Shelf) mit Modus Stereo/Mitte/Seite. Punkte in der Kurve ziehen, Mausrad = Q.
  - **Dynamic EQ:** Jedes Band kann dynamisch arbeiten (DYN). Liegt sein Frequenzbereich über THRESHOLD, kommt bis zu RANGE an Absenkung (−) oder Anhebung (+) dazu; die volle Range ist bei 10 dB über der Schwelle erreicht. Attack und Release gelten für alle Bänder. In der Kurve zeigt die gestrichelte Linie die statische EQ-Kurve, die gelben Linien die Range und die Punkte die aktuelle dynamische Änderung.
- **DYNAMIK:** 3-Band-Kompression. Pro Band Threshold, Ratio, Attack, Release, Gain, Modus (Stereo/Mid/Side), dazu SOLO und BYPASS. Die Weichen lassen sich in der Grafik ziehen.
- **GLUE:** Bus-Kompressor mit Kennlinie und Gain-Reduktions-Verlauf, Auto-Release, Sidechain-Hochpass, Makeup, Mix (Parallel-Kompression).
- **STEREO & FARBE:** Vektorskop, Breite für Mitten und Höhen, Mono-Bass, WÄRME (Tape-Bandsättigung), GLANZ (Exciter).
- **LIMITER:** Lautheitsverlauf mit Ziellinie, LERNEN, Drive, Ceiling (True Peak), Clipper, Charakter, Output, DELTA (nur hören, was entfernt wird), OVERSAMPLING HQ (8x, Aliasing −81 dB statt −51 dB).
- **RESONANZ:** dynamische Resonanzkontrolle mit 6 ziehbaren Bändern und DELTA.
- **Rechts, immer sichtbar:**
  - LUFS integriert/momentan/kurzzeit, LRA, True Peak, PLR, PSR.
  - Streaming-Anpassung (Spotify, YouTube, Apple), Abweichung vom Ziel.
  - Peak-Meter Ein/Aus, Korrelation, Gain-Reduktion aller Stufen.
- **Kopfzeile:** Snapshots A–D, Undo/Redo (Cmd/Strg+Z, Cmd/Strg+Umschalt+Z), GLEICH LAUT (nur zum Hören, beim Export automatisch aus), ORIGINAL (= DAW-Bypass), MONO/SIDE (Abhören), ASSISTENT.
- **Presets (rechts oben):**
  - 10 Werk-Presets als Startpunkte: Master sauber / warm & breit / laut / Hip-Hop / akustisch, Mix-Bus-Glue, Drum-Bus parallel, Resonanzen glätten, nur Limiter.
  - Eigene Presets über `···` speichern; sie liegen als Dateien in `Dokumente/Auric Master 3/Presets`.
  - Ein `*` zeigt an, dass das Preset geändert wurde. ORIGINAL, GLEICH LAUT und das Genre bleiben beim Laden unverändert.
  - Nach dem Laden eines Master-Presets LERNEN klicken.
- **Undo** erfasst nur deine Bedien-Aktionen (dazu Preset, Assistent, LERNEN), nicht die Automation der DAW.

## LERNEN (Lautheitsziel)

1. Ziel wählen (z. B. *Streaming −14 LUFS*).
2. Song abspielen, am besten einen lauten Teil, und **LERNEN** klicken.
3. Das Plugin misst bis zu 20 s, stellt den Drive ein und prüft das Ergebnis in ein bis zwei kurzen Durchgängen.
4. Danach bleibt der Drive **fest**. Nochmal klicken während der Messung = mit dem bisher Gemessenen fertig.

Getestet: Ziele −14/−16/−9/−7 LUFS werden über den ganzen Song auf ±0,2 LU getroffen, der True Peak bleibt unter dem Ceiling. Ist das Ziel mit +24 dB Drive nicht erreichbar, sagt LERNEN das ausdrücklich.

## Auto-Master-Assistent

Genre wählen, ASSISTENT klicken und 15 s abspielen. Danach stellt der Assistent EQ, Multiband (Thresholds aus den gemessenen Bandpegeln), Glue, Breite, Mono-Bass, Farbe, Resonanzen und den Limiter ein und führt anschließend LERNEN aus. RÜCKGÄNGIG stellt alles zurück. Der Assistent ist ein Startpunkt, das Feintuning geschieht nach Gehör.

## Qualitätssicherung

Bei jedem Build in GitHub Actions (Windows und macOS) laufen:
- die Test-Suite (`tests/TestMain.cpp`, 54 Prüfungen),
- **pluginval** mit höchster Strenge (10), das Standard-Prüfwerkzeug für Plugins (State, Automation aus anderen Threads, Puffergrößen, Bus-Layouts, Editor während der Verarbeitung, Parameter-Fuzzing),
- **auval** (Apples Audio-Unit-Prüfung).

Schlägt einer davon fehl, wird kein Plugin ausgeliefert. Geprüft werden unter anderem:

- **Messung:** Lautheitsmessung nach BS.1770/EBU auf ±0,03 LU, LRA, True Peak.
- **Transparenz:** Standardeinstellungen bit-transparent (−148 dB Restsignal), keine Latenz, Frequenzweiche flach.
- **Determinismus:** Abspielstart ab Refrain vs. Songanfang: 0,00 LU Unterschied. Strophe/Refrain-Kontrast bleibt erhalten.
- **LERNEN und Kompressor:** Ziele erreicht, nicht erreichbare Ziele werden gemeldet, Kennlinie exakt.
- **Dynamic EQ:** unter der Schwelle inaktiv, volle Range bei lautem Signal, frequenzselektiv, statisch + dynamisch kombiniert, Mitte-Band ignoriert die Seite.
- **Presets und Undo:** alle Werk-Presets laden und spielen, eigene Presets speichern/laden, Änderungs-Markierung, Undo/Redo, Automation erzeugt keine Undo-Schritte.
- **DELTA und Mid/Side:** normaler Ausgang + Delta = Eingang der Stufe (Fehler < 10⁻⁶); Mid-, Side- und Stereo-Modus reagieren nur auf ihren Signalanteil.
- **Robustheit:**
  - NaN/Inf-Schutz, latenzausgeglichener Host-Bypass, klickfreies Schalten von 11 Bedienelementen.
  - Aliasing von Exciter und Clipper, Mono-Spuren, Snapshots mit Projekt-Neuladen, große Host-Puffer.
- **Assistent:** neutral, zu viel Bass, zu dumpf, zu breit, zu komprimiert, wiederholbar, Rückgängig.
- **CPU:** ca. 8 % eines Kerns bei 48 kHz (alle Module an).

Zusätzlich lokal: AddressSanitizer/UBSan ohne Befund.

Ausführliche Anleitung: **[Handbuch](docs/Handbuch.md)**.

## Installation

GitHub → *Actions* → neuester Lauf. Am einfachsten sind die Installer:
- **macOS:** Artefakt **AuricMaster3-macOS-Installer** → `Auric Master 3 Installer.pkg` per **Rechtsklick → Öffnen** starten. Er installiert VST3 + AU für alle Benutzer und meldet das AU an.
- **Windows:** Artefakt **AuricMaster3-Setup-Windows** → Setup starten (SmartScreen: *Weitere Informationen → Trotzdem ausführen*).

Alternativ manuell mit den Artefakten **AuricMaster3-VST3-Windows** bzw. **AuricMaster3-macOS**:

### macOS
1. `AuricMaster3-macOS` herunterladen und in *Downloads* entpacken (ggf. die innere Zip-Datei noch einmal doppelklicken), sodass `~/Downloads/dist/` entsteht.
2. Terminal:
   ```
   mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components && cp -R ~/Downloads/dist/"Auric Master 3.vst3" ~/Library/Audio/Plug-Ins/VST3/ && cp -R ~/Downloads/dist/"Auric Master 3.component" ~/Library/Audio/Plug-Ins/Components/ && xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Auric Master 3.vst3" ~/Library/Audio/Plug-Ins/Components/"Auric Master 3.component" && echo "Installiert!"
   ```
3. FL Studio → *Options → Manage plugins → Find installed plugins* → Stern bei *Auric Master 3*.
4. Falls es nicht erscheint: `killall -9 AudioComponentRegistrar; auval -v aufx AuM3 Auri`, dann FL Studio neu starten.

### Windows
`Auric Master 3.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren, dann in FL Studio *Find installed plugins*.

## Grenzen (ehrlich)
- **Gatekeeper-Hinweis:** Ohne Apple-Developer-Konto ist das Mac-Plugin nur ad-hoc signiert, daher der `xattr`-Befehl. Eine Notarisierung braucht ein Apple-Developer-Konto (99 €/Jahr).
- **Kein AAX (Pro Tools):** Dafür ist ein Avid-Developer-Zugang nötig.
- **Klang nicht in FL getestet:** Der Klang wurde messtechnisch geprüft, nicht in FL Studio selbst. Rückmeldungen nach Gehör fließen in die nächsten Phasen ein.

## Geplant
Siehe [ROADMAP.md](ROADMAP.md): Installer, Handbuch, weitere Profi-Funktionen und zum Schluss ein analoges Studio-Design (Bedienung bleibt gleich).
