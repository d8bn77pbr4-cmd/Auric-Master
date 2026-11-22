# Auric Master 3 – Handbuch

Version 3.0 · VST3 (Windows, macOS) und AU (macOS)

---

## 1. Überblick

Auric Master 3 ist eine modulare Mix- und Mastering-Suite. Alle Module liegen in einer festen, praxiserprobten Reihenfolge und lassen sich einzeln ein- und ausschalten:

```
Input → RESONANZ → EQ + KLANG (+ Dynamic EQ) → [Weiche] DYNAMIK · FARBE · STEREO → GLUE
      → MIX (parallel) → LIMITER (Drive · Clipper · True-Peak-Limiter) → Output
```

Grundsätze:
- **Neu eingefügt verändert das Plugin nichts:** bit-transparent, ohne Latenz. Du schaltest nur ein, was du brauchst.
- **Deterministisch:** Gleiche Einstellung bedeutet gleiches Ergebnis, egal ob du ab Songanfang oder ab Refrain abspielst. Nichts regelt im Hintergrund nach.
- **Lautheit per LERNEN:** einmal messen, danach bleibt der Drive fest. Strophe und Refrain behalten ihren Lautstärkeunterschied.
- **Messung nach Norm:** Lautheit nach ITU-R BS.1770-4 / EBU R128, True Peak mit 4-facher Interpolation.

---

## 2. Installation

### macOS (empfohlen: Installer)
1. GitHub → *Actions* → neuester Lauf → Artefakt **AuricMaster3-macOS-Installer** laden und entpacken.
2. `Auric Master 3 Installer.pkg` mit **Rechtsklick → Öffnen** starten. Das ist nötig, weil das Paket nicht bei Apple notarisiert ist.
3. Der Installer legt das VST3 nach `/Library/Audio/Plug-Ins/VST3`, das AU nach `/Library/Audio/Plug-Ins/Components` und meldet das AU beim System an.
4. FL Studio → *Options → Manage plugins → Find installed plugins* → Stern bei *Auric Master 3*.

### Windows (empfohlen: Installer)
1. Artefakt **AuricMaster3-Setup-Windows** laden, `AuricMaster3-Setup-Windows.exe` starten.
2. Bei der SmartScreen-Warnung: *Weitere Informationen → Trotzdem ausführen*. Das Setup ist nicht signiert.
3. Das Plugin landet in `C:\Program Files\Common Files\VST3`. Deinstallation über *Apps & Features*.

### Manuell
Siehe README: Die Plugin-Ordner lassen sich auch direkt kopieren.

---

## 3. Schnellstart

### Mastering in fünf Schritten
1. Auric Master 3 als **letztes Plugin auf den Master** legen.
2. Genre wählen, **ASSISTENT** klicken und einen typischen, vollen Teil des Songs abspielen (z. B. den Refrain). Nach 15 s sind alle Module eingestellt, danach misst der Assistent selbst die Lautheit ein.
3. Mit **GLEICH LAUT** gegen das Original vergleichen: Die lautere Version wird leiser gemacht, so hörst du fair.
4. Nach Gehör anpassen: KLANG, GLUE, BREITE auf der Seite EINFACH. Details auf den Profi-Seiten.
5. Wenn du danach am Klang etwas änderst, noch einmal **LERNEN** klicken. Vor dem Export GLEICH LAUT ausschalten (beim Export ist es automatisch aus).

### Als Bus-Werkzeug im Mix
Werk-Preset **Mix-Bus – Glue 2 dB** oder **Drum-Bus – Punch (parallel)** laden. Der Limiter ist dort aus, die Latenz bleibt 0 ms.

---

## 4. Bedienung allgemein

| Aktion | Wirkung |
|---|---|
| Knopf ziehen | Wert ändern |
| Doppelklick auf Knopf | Standardwert |
| Klick auf die Wertanzeige | Wert eintippen |
| Modul-Schalter (EINFACH, oder oben links auf jeder Seite) | Modul ein/aus, klickfrei |
| Punkt im Tab | grün = Modul aktiv |
| Cmd/Strg + Z, Cmd/Strg + Umschalt + Z | Undo / Redo |
| A B C D | vier komplette Einstellungen zum Vergleichen, im Projekt gespeichert |
| ORIGINAL | Bypass mit Latenzausgleich, identisch mit dem Bypass-Knopf der DAW |
| MONO / SIDE | Abhören in Mono (Kompatibilität prüfen) bzw. nur den Stereo-Anteil. Wirkt auch auf ORIGINAL; beim Export automatisch aus |
| GLEICH LAUT | fairer Vergleich (nur zum Hören) |
| Fensterecke ziehen | Größe 60–160 %, wird im Projekt gespeichert |

**Undo** erfasst Bedien-Aktionen, Preset-Laden, den Assistenten und LERNEN, aber **nicht** die Automation deiner DAW.

### Presets
- **Pfeile oder Liste:** Werk-Presets und eigene Presets wählen. Ein `*` bedeutet: Das Preset wurde geändert.
- **`···`:** Preset speichern (gleicher Name = überschreiben), löschen, Preset-Ordner öffnen.
- **Speicherort eigener Presets:** `Dokumente/Auric Master 3/Presets` (Dateien `.am3preset`, lassen sich kopieren und teilen).
- **Was ein Preset nicht ändert:** ORIGINAL, GLEICH LAUT und die Genre-Wahl.
- **Threshold-Werte der Werk-Presets:** Sie gehen von einem Mix um −18 bis −14 LUFS aus. Bei Master-Presets danach **LERNEN** klicken.

---

## 5. Die Module

### RESONANZ (Latenz ca. 43 ms, nur wenn aktiv)
Dynamische Resonanzkontrolle: Frequenzen, die kurzzeitig aus dem Spektrum herausstechen, werden genau in diesem Moment abgesenkt.
- **TIEFE:** Stärke der Absenkung.
- **SCHÄRFE:** nur schmale Spitzen bis auch breite Überhöhungen.
- **SELEKTIVITÄT:** wie deutlich eine Resonanz herausragen muss.
- **ATTACK / RELEASE:** Reaktions- und Rückstellzeit.
- **6 Bänder in der Grafik:** ziehen = Frequenz und Stärke (+ = mehr Kontrolle, − = schützen), Mausrad = Breite.
- **DELTA:** nur das hören, was die Resonanz-Kontrolle entfernt. Klingt das Delta nach Musik statt nach Resonanzen, ist die TIEFE zu hoch. Nur zum Abhören: beim Export automatisch aus, nicht Teil von Presets.

### EQ + KLANG (keine Latenz)
- **LOW CUT / HIGH CUT:** 24 dB/Okt., ganz links bzw. rechts = aus.
- **4 Bänder:** Low Shelf, Glocke, Glocke, High Shelf, jeweils mit FREQ, GAIN (±12 dB) und Q.
- **MODUS:** Stereo, nur Mitte (Mono-Anteil) oder nur Seite (Stereo-Anteil).
- **KLANG (Seite EINFACH):** breite Tiefen/Mitten/Höhen-Regler (Shelf 100 Hz, Glocke 1,2 kHz, Shelf 10 kHz, je ±6 dB).
- **Dynamic EQ (DYN):** Ein Band arbeitet nur, wenn sein Frequenzbereich lauter ist als THRESHOLD.
  - **RANGE:** maximale zusätzliche Absenkung (−) oder Anhebung (+), voll erreicht bei 10 dB über der Schwelle, mit weichem Übergang.
  - **DYN ATTACK / DYN RELEASE:** gelten für alle dynamischen Bänder.
  - **Anzeige:** gestrichelte Linie = statische Kurve, gelbe Linie = Range, Punkt = aktuelle Änderung.
  - Typisch: Härte bei 2–5 kHz nur in lauten Stellen absenken, dröhnende Basstöne bändigen.

### DYNAMIK – 3-Band-Kompression
- **WEICHE 1 / 2:** Trennfrequenzen (Linkwitz-Riley, Summe exakt flach); auch direkt in der Grafik ziehbar.
- **Pro Band:** THRESHOLD (dBFS RMS), RATIO, ATTACK, RELEASE, GAIN.
- **MODUS pro Band:** Stereo (ganzes Band), Mid (nur der Mono-Anteil) oder Side (nur der Stereo-Anteil). Beispiele: harte Becken nur in der Seite der Höhen zähmen, den Bass nur in der Mitte verdichten.
- **SOLO:** nur dieses Band hören. **BYPASS:** Band unbearbeitet.
- **Anzeige:** Die Balken zeigen die aktuelle Gain-Reduktion je Band.

### FARBE
- **WÄRME:** Tape-artige Bandsättigung des Bassbereichs. Bringt Obertöne, damit der Bass auch auf kleinen Lautsprechern hörbar wird.
- **GLANZ:** pegelabhängiger Exciter für die Höhen; die erzeugten Obertöne bleiben unter der Nyquist-Frequenz (kein Aliasing).

### STEREO
- **BREITE MITTEN / HÖHEN:** 0–200 %, 100 % = unverändert. Der Bass bleibt unberührt.
- **MONO-BASS:** Unterhalb dieser Frequenz wird das Signal mono (Club, Vinyl, Handy).
- **Vektorskop und Korrelation:** Die Korrelation sollte über 0 bleiben.

### GLUE – Bus-Kompressor
- **THRESHOLD, RATIO (typisch 2:1–4:1), ATTACK:** länger = mehr Punch.
- **RELEASE:** mit **AUTO RELEASE** programmabhängig.
- **MAKEUP:** Pegelausgleich nach der Kompression.
- **SIDECHAIN-HPF:** Die Kick steuert den Kompressor weniger an, kein Pumpen.
- **MIX:** Parallel-Kompression.
- **Kennlinie:** Der gelbe Punkt zeigt den aktuellen Pegel, rechts die Gain-Reduktion der letzten 10 s.

### MIX (Seite EINFACH)
Mischt das unbearbeitete Signal vor dem Limiter wieder dazu (Parallel-Bearbeitung aller Module außer dem Limiter).

### LIMITER (Latenz ca. 5 ms, nur wenn aktiv)
- **DRIVE:** Verstärkung in den Limiter (−12 bis +24 dB). Wird von LERNEN gesetzt.
- **CEILING:** maximaler True Peak am Ausgang (Streaming-Empfehlung: −1 dBTP).
- **CLIPPER:** weicher Clipper vor dem Limiter, oversampled; nimmt schnelle Spitzen ab.
- **CHARAKTER:** Transparent (sauber), Punchy (ausgewogen), Laut (schnell und dicht).
- **OUTPUT:** Bei aktivem Limiter nur absenkbar, damit das Ceiling garantiert hält.
- **DELTA:** nur das hören, was Limiter und Clipper entfernen. Hört man darin deutlich Musik (Kick, Stimme), wird zu stark limitiert. Kann laut sein. Nur zum Abhören, beim Export automatisch aus.
- **OVERSAMPLING HQ** (Standard: an): 8-fach statt 4-fach bis 48 kHz (bzw. 4-fach statt 2-fach bis 96 kHz). Das Aliasing bei hartem Clipping sinkt von etwa −51 dB auf etwa −81 dB, bei rund 2 % mehr CPU. Wer CPU sparen muss, schaltet es aus. Über 96 kHz ist zusätzliche Überabtastung nicht nötig. Danach folgt immer ein True-Peak-Wächter, der das Ceiling garantiert.

---

## 6. Lautheit und LERNEN

| Ziel | Integrierte Lautheit | Hinweis |
|---|---|---|
| Streaming | −14 LUFS | Spotify, YouTube, Tidal, Amazon normalisieren etwa dorthin |
| Apple Music | −16 LUFS | |
| Laut / Sehr laut / Club | −11 / −9 / −7 LUFS | für Genres, die laut erwartet werden |

**So geht's:** Ziel wählen, Song abspielen (am besten einen lauten Teil), **LERNEN** klicken.
- Das Plugin misst bis zu 20 s und stellt den Drive ein.
- Danach prüft es das Ergebnis in ein bis zwei kurzen Durchgängen.
- Unter dem Knopf erscheint der Status, z. B. *Ziel −14 LUFS erreicht (gemessen −14,0), Drive +9,3 dB fest*.
- Ein Klick während der Messung beendet sie mit dem bisher Gemessenen.
- Ist das Ziel mit +24 dB Drive nicht erreichbar, steht das dort ausdrücklich. Dann INPUT anheben oder ein leiseres Ziel wählen.

**Messwerte im Panel rechts**
- **INTEGRIERT:** Lautheit seit RESET bzw. seit dem letzten LERNEN (gegatet nach Norm).
- **MOMENTAN (400 ms) / KURZZEIT (3 s):** aktuelle Lautheit.
- **LRA:** Lautheitsumfang; großer Wert = dynamischer Song.
- **TRUE PEAK:** höchster interpolierter Spitzenwert seit RESET. Rot, wenn das Ceiling überschritten wurde.
- **PLR** (True Peak minus integrierte Lautheit) und **PSR** (True Peak der letzten 3 s minus Kurzzeit-Lautheit): wie viel Dynamik bleibt. Unter etwa 8 dB wirkt ein Master meist gequetscht, über 12 dB offen und dynamisch.
- **STREAMING-ANPASSUNG:** um wie viel Spotify (−14), YouTube (−14, dreht nur leiser) und Apple Music (−16) den Song etwa verändern würden. Ein Wert wie −5 dB heißt: Die zusätzliche Lautheit bringt dort nichts, sie kostet nur Dynamik. Das ist eine Näherung; die Dienste ändern ihre Verfahren gelegentlich.
- **Abweichung vom Ziel:** grün unter 1 LU.
- **PEAK EIN/AUS, KORRELATION, GAIN-REDUKTION:** Gain-Reduktion jeder Stufe (Bänder, Glue, Clipper, Limiter).

Gut zu wissen: Ein lauterer Master klingt auf Streaming-Plattformen nicht lauter, er wird nur stärker leiser gedreht. Für Streaming ist −14 LUFS mit erhaltener Dynamik meist die beste Wahl.

---

## 7. Assistent

1. Genre wählen (Pop, Hip-Hop/Trap, EDM, Rock, Acoustic) und **ASSISTENT** klicken.
2. 15 s abspielen. Der Assistent misst:
   - die Langzeit-Klangbalance im Vergleich zur Genre-Referenz,
   - die Pegel der drei Bänder und des Gesamtsignals,
   - das Verhältnis von Spitzenpegel zu Lautheit (PLR),
   - die Stereo-Korrelation und den Stereo-Anteil im Bass.
3. Daraus stellt er ein:
   - EQ (etwa 60 % der Abweichung, max. ±4 dB),
   - Multiband-Thresholds, Glue (ca. 1–2 dB Gain-Reduktion in lauten Stellen),
   - Breite, Mono-Bass, Farbe, Resonanzen und den Limiter.
4. Anschließend läuft automatisch **LERNEN**.
5. **RÜCKGÄNGIG** (unten) oder Undo stellt alles auf den Stand davor zurück.

Der Assistent ist ein Startpunkt, kein Ersatz für deine Ohren.

---

## 8. Latenz und CPU

| Aktive Module | Latenz |
|---|---|
| keines oder nur EQ / Dynamik / Farbe / Stereo / Glue | 0 ms |
| Limiter | ca. 5 ms |
| Resonanz | ca. 43 ms |

FL Studio gleicht die Latenz automatisch aus (PDC). Beim Ein- und Ausschalten von Resonanz oder Limiter wird die neue Latenz gemeldet; dabei entsteht eine kurze Pause von bis zu ca. 60 ms.

CPU (ein Kern, alle Module an): etwa 8 % bei 48 kHz, etwa 24 % bei 192 kHz.

---

## 9. Tipps aus der Praxis

- **Gain-Staging:** Der Mix sollte vor dem Master Luft haben (Spitzen um −6 dBFS). INPUT nur nutzen, wenn der Mix sehr leise oder sehr laut ankommt.
- **Erst Korrektur, dann Farbe, dann Lautheit:** EQ und Dynamik sauber einstellen, danach Farbe und Breite, zum Schluss LERNEN.
- **Breite mit Maß:** Über 130 % klingt es auf Mono-Geräten schnell dünn; die Korrelation im Blick behalten.
- **Mono-Bass** ab etwa 100 Hz ist für fast jede Produktion sinnvoll.
- **Vergleichen nur gleich laut:** Lauter klingt immer "besser", deshalb GLEICH LAUT nutzen.
- **Snapshots** für Varianten (A sanft, B laut) und dann blind vergleichen.

---

## 10. Fehlerbehebung

| Problem | Lösung |
|---|---|
| Plugin erscheint nicht in FL Studio | *Find installed plugins* ausführen. Mac: `killall -9 AudioComponentRegistrar`, FL neu starten. |
| Mac meldet "nicht geöffnet werden" | Installer per Rechtsklick → Öffnen. Bei manueller Installation den `xattr`-Befehl aus der README nutzen. |
| LERNEN sagt "kein Signal gemessen" | LERNEN während der Wiedergabe klicken. |
| LERNEN sagt "nicht erreichbar" | INPUT anheben oder leiseres Ziel wählen. |
| Leiser Aussetzer beim Einschalten von Resonanz/Limiter | Normal: Die Latenz ändert sich, die DAW gleicht neu aus. |
| Vergleich mit ORIGINAL klingt lauter/leiser | GLEICH LAUT einschalten. |

---

## 11. Technische Daten und Qualitätssicherung

- **Formate:** VST3 (Windows x64, macOS Universal), AU (macOS). Mono und Stereo, 44,1–192 kHz.
- **Messung:** BS.1770-4 / EBU R128, True Peak mit 4-facher Interpolation.
- **Prüfung bei jedem Build (Windows und macOS):**
  - 54 automatisierte Tests: Messgenauigkeit, Transparenz, Determinismus, LERNEN, Dynamic EQ, Mid/Side-Multiband, DELTA, MONO/SIDE, Presets, Undo, NaN-Schutz, klickfreies Schalten, Aliasing, Mono, große Puffer, Assistent.
  - **pluginval** mit höchster Strenge (10) und **auval**.
- **Grenzen:**
  - Nicht notarisiert/signiert (dafür wäre ein Apple-Developer- bzw. Code-Signing-Zertifikat nötig).
  - Kein AAX (Pro Tools).
