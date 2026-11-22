# Auric Master 3 – Roadmap und Entscheidungslog

Arbeitsweise: Jede Iteration durchläuft einen Entscheidungs-Loop (Schritt nach Kundennutzen wählen, gegen Alternativen prüfen) und einen Ergebnis-Loop (Build, alle Tests, ASan bei DSP-Änderungen, Screenshot bei UI-Änderungen, Selbstreview, CI grün). Priorität: Klangqualität und Zuverlässigkeit > Workflow > Komfort.

## Stand

| Phase | Inhalt | Status |
|---|---|---|
| 1 | Deterministische Engine, Module einzeln schaltbar, LERNEN, klassische Kompressoren, Host-Bypass, NaN-Schutz, Latenz nur bei Bedarf | fertig |
| 2 | Dynamic EQ (DYN, Threshold, Range, gemeinsames Attack/Release) | fertig |
| 2 | Presets (Werk + eigene, Browser, Änderungs-Markierung) | fertig |
| 2 | Undo / Redo (nur Bedien-Aktionen, keine Automation) | fertig |
| 3 | pluginval Strenge 10 (Windows + macOS) und auval in jedem Build | fertig |
| 3 | Installer (Windows Inno Setup, macOS .pkg mit AU-Registrierung) | fertig |
| 3 | Handbuch (docs/Handbuch.md) | fertig |
| 4 | Meter: PLR, PSR, Streaming-Anpassung | fertig |
| 4 | DELTA-Abhören (Resonanz, Limiter) | fertig |
| 4 | Mid/Side-Modus im Multiband | fertig |
| 4 | Abhören MONO / SIDE | fertig |
| 4 | Limiter OVERSAMPLING HQ (8x), Standard an | fertig |
| 4 | weitere nach Kundennutzen (Prüfung: Linear-Phase-EQ zurückgestellt) | offen |
| 5 | **Analoges Studio-Design** (Konsole Anthrazit, VU-Instrumente; vom Nutzer gewählt) – nur Optik, Bedienelemente und Steuerung unverändert (geprüft: 0 geänderte Layout-/Bedienzeilen) | fertig |

Nicht ohne Rückfrage: Dither, Referenz-Track (vom Nutzer früher abgelehnt).

## Entscheidungslog

- **pluginval/auval vor Dynamic EQ gezogen:** Zuverlässigkeit hat Vorrang. pluginval deckt typische Host-Probleme auf (State, Automation-Threads, Puffergrößen, Editor). Ergebnis: Windows und macOS SUCCESS bei Strenge 10, auval bestanden.
- **Dynamic EQ mit gemeinsamem Attack/Release statt pro Band:** Die Seite bleibt übersichtlich, die Zeitparameter sind trotzdem echt. Der Detektor arbeitet feed-forward auf dem Bandsignal vor dem EQ und folgt dem Bandmodus (Stereo/Mitte/Seite).
- **Undo über Parameter-Gesten statt JUCE-UndoManager:** Der UndoManager würde DAW-Automation mit aufzeichnen. Gesten erzeugt nur die Bedienoberfläche. Preset, Assistent und LERNEN zählen je als ein Schritt. ORIGINAL und GLEICH LAUT sind nicht Teil von Undo und Presets.
- **Installer vorgezogen vor Profi-Erweiterungen:** Die Terminal-Installation hat beim Nutzer schon Probleme gemacht. Beim macOS-pkg ist die Bundle-Relocation abgeschaltet (sonst würde macOS auf alte Kopien umleiten), ein Postinstall-Skript meldet das AU an.
- **DELTA nur für Resonanz und Limiter:** Nur Module, die etwas entfernen, ergeben ein sinnvolles Delta. Es wird zeitlich ausgerichtet, nicht weiterverarbeitet und ist beim Export aus.
- **Mid/Side als Modus pro Band:** Das deckt die typischen Mastering-Fälle ab, ohne die Regler zu verdoppeln.
- **HQ-Oversampling als Standard:** Das Aliasing sinkt bei hartem Clipping um 30 dB (−51 auf −81 dB), bei +0,1 ms Latenz und rund 2 % CPU. Über 96 kHz bleibt es bei der normalen Stufe (kein hörbarer Nutzen, aber CPU-Kosten).
- **Design nur über Zeichenroutinen:** LookAndFeel, Frontplatten, Meter. Layout und Bedienelemente blieben unangetastet (per Diff geprüft). VU-Referenzen: IN 0 VU = −18 LUFS, OUT 0 VU = Lautheitsziel, sodass die Nadel bei 0 bedeutet: "auf Ziel".
- **Linear-Phase-EQ zurückgestellt:** hohe Latenz (mehrere Tausend Samples) und Vorschwinger bei geringem Nutzen gegenüber dem vorhandenen minimalphasigen EQ.
- **Presets in realen Einheiten gespeichert:** Sie sind robust gegen spätere Bereichsänderungen. Neue Parameter späterer Versionen erhalten beim Laden ihren Standardwert.
