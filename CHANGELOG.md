# Changelog

## v0.1.0 — first preview (2026-11-01)

*Deutsche Fassung weiter unten.*

First public version. Source code only: there is no prebuilt program, because it would contain the game's code.
You build it yourself from your own ROM, see [BUILDING.md](BUILDING.md).

### What works

- Boot, Factor 5 / LucasArts intros, title screen, menus, inventory, pause and death menus.
- Saving and loading (the save file is kept between sessions).
- The first level, Canyonlands, was played by hand up to the long conversation scene: walking, jumping, crawling,
  first-person view, inventory, cutscenes with speech.
- All 16 levels load and render without crashing (automated test, a few seconds each).
- Music, sound effects and speech, including the streamed dialogue in cutscenes. Cutscenes advance in time with the
  speech again.
- Fog, sky and level intros in every level checked.
- Resolution scaling, MS anti-aliasing, windowed/fullscreen, Direct3D 12 or Vulkan.
- **Nintendo Switch Online N64 controller** (Bluetooth): mapped and tested. C buttons are on the right stick axes as
  the frontend expects, *Capture* saves a bug report, *Home* opens the menu. Other controllers and the keyboard use
  the frontend's default mapping and can be remapped.
- Rumble Pak is emulated.
- Bug reports with F9 (or *Capture*): screenshot, RAM dump, display list with per-triangle data, logs.
- Own launcher (retro pixel art, made from scratch) and app icon.

### What was done

Graphics
- Ported the Factor 5 graphics microcode from GLideN64 to RT64 (the game does not use Nintendo's microcode).
- Fog: corner colours no longer switch fog off; fog factors are taken from the game.
- Correct pixel aspect of the video output; the never-drawn last row and column are cropped (coloured column on the
  right in darkened screens).
- First-person view / crawling: fixed a strip of an old picture at the bottom of the screen (RT64 sized the frame by
  the projected corners, which is wrong for triangles with a corner behind the camera).
- RT64's frame interpolation and widescreen are switched off, both break this game (see below).

Audio
- The MusyX audio microcode is recompiled with RSPRecomp, including the code it loads at run time.
- Audio interrupts are paced like the real hardware; fixed pitch one octave too high and stutter from repeated buffers.
- Speech: PI DMA now fills the I/O message like libultra, which MusyX needs to stream speech from the ROM.
- Dialogue streams: the game drops its ROM load list on audio frames without a buffer refill; such lists are now
  handed to the loader, so the dialogue table loads (before, all dialogue was skipped silently).

Runtime
- The game's virtual memory window at `0x41000000` (normally paged in through the TLB) is mapped as plain memory.
- Thread scheduling fixes (running-thread pointer, `osYieldThread`, pre-emption points in long loops).
- Crash on exit fixed.

### What was not done

- **No widescreen:** wider views show holes in the game's room culling.
- **No 60 fps / frame interpolation:** RT64 pairs matrices between frames by address, and this game reallocates them
  every frame, which made the camera and geometry jump. The game runs at its original rate.
- **No mod support** (the menu entry is removed) and no texture packs.
- Only the USA ROM is supported. Linux and macOS builds were not attempted.
- The game was not played through; levels 2–16 were only checked for loading.
- No frame-by-frame comparison with real hardware; only selected scenes were compared with a real N64 and an emulator.
- The *Controls* menu shows generic action names, not the game's.

### Known issues

- Occasional very short audio dropouts can occur (the session log counts a few per minute on the test machine).
- Rumble was felt only sporadically with the Switch Online N64 controller; not compared with the original.
- The stereo image leans to the right in places. Analysis suggests this is the game's own surround mix, but it was not
  confirmed on real hardware.
- In first-person view, some nearby scenery pops in and out at certain angles. The same happens on a real N64 (the
  game's own culling); it is not a port bug.
- The game's save type is not verified; all save types are allowed.
- Parts of the graphics microcode that the first level does not use (for example some particle and billboard paths)
  are untested and may show glitches in later levels.
- Since there was no complete comparison with the original, some quirks may be the original game's and some may be
  port bugs. Reports with F9 help to tell them apart.

---

## v0.1.0 — erste Vorabversion (01.11.2026)

Erste öffentliche Version. Nur Quellcode: Es gibt kein fertiges Programm, weil es den Code des Spiels enthielte.
Du baust es selbst aus deiner eigenen ROM, siehe [BUILDING.md](BUILDING.md).

### Was funktioniert

- Start, Intros von Factor 5 und LucasArts, Titelbild, Menüs, Inventar, Pause- und Todesmenü.
- Speichern und Laden (der Spielstand bleibt zwischen den Sitzungen erhalten).
- Das erste Level, Canyonlands, wurde von Hand bis zur langen Gesprächsszene gespielt: Laufen, Springen, Kriechen,
  Ich-Ansicht, Inventar, Zwischensequenzen mit Sprache.
- Alle 16 Level laden und werden gezeichnet, ohne Absturz (automatischer Test, je einige Sekunden).
- Musik, Effekte und Sprache, auch die gestreamten Dialoge in Zwischensequenzen. Die Zwischensequenzen laufen wieder
  im Takt der Sprache.
- Nebel, Himmel und Level-Intros in allen geprüften Leveln.
- Auflösungsskalierung, Kantenglättung (MSAA), Fenster/Vollbild, Direct3D 12 oder Vulkan.
- **Nintendo Switch Online N64 Controller** (Bluetooth): eigene Belegung, getestet. Die C-Tasten liegen wie vom
  Frontend erwartet auf dem rechten Stick, *Capture* speichert einen Fehlerbericht, *Home* öffnet das Menü. Andere
  Controller und die Tastatur nutzen die Standardbelegung und lassen sich umbelegen.
- Rumble Pak wird nachgebildet.
- Fehlerberichte mit F9 (oder *Capture*): Bildschirmfoto, Speicherabzug, Displayliste mit Daten je Dreieck, Protokolle.
- Eigener Launcher (Retro-Pixelgrafik, selbst erstellt) und App-Symbol.

### Was gemacht wurde

Grafik
- Den Grafik-Microcode von Factor 5 aus GLideN64 nach RT64 portiert (das Spiel nutzt nicht den von Nintendo).
- Nebel: Eckfarben schalten den Nebel nicht mehr ab; die Nebelwerte kommen aus dem Spiel.
- Richtiges Pixel-Seitenverhältnis der Bildausgabe; die nie gezeichnete letzte Zeile und Spalte werden abgeschnitten
  (bunte Spalte rechts in abgedunkelten Bildern).
- Ich-Ansicht/Kriechen: Streifen mit altem Bild am unteren Rand behoben (RT64 bemaß das Bild nach den projizierten
  Ecken, was bei Dreiecken mit einer Ecke hinter der Kamera falsch ist).
- Zwischenbild-Berechnung und Breitbild von RT64 sind abgeschaltet, beides stört dieses Spiel (siehe unten).

Ton
- Der MusyX-Audio-Microcode wird mit RSPRecomp übersetzt, einschließlich des Codes, den er zur Laufzeit nachlädt.
- Audio-Interrupts kommen im Takt der echten Hardware; Tonhöhe eine Oktave zu hoch und Stottern durch wiederholte
  Puffer behoben.
- Sprache: PI-DMA füllt die I/O-Nachricht jetzt wie libultra aus; MusyX braucht das, um Sprache aus der ROM zu streamen.
- Dialog-Streams: Das Spiel verwirft seine ROM-Ladeliste in Audio-Frames ohne Puffer-Auffüllung; solche Listen werden
  jetzt nachgereicht, damit die Dialog-Tabelle lädt (vorher wurden alle Dialoge still übersprungen).

Laufzeitsystem
- Das virtuelle Speicherfenster des Spiels bei `0x41000000` (sonst per TLB eingeblendet) wird als normaler Speicher
  bereitgestellt.
- Korrekturen an der Thread-Verwaltung (Zeiger auf den laufenden Thread, `osYieldThread`, Unterbrechungspunkte in
  langen Schleifen).
- Absturz beim Beenden behoben.

### Was nicht gemacht wurde

- **Kein Breitbild:** Breitere Ansichten zeigen Lücken in der Raum-Sichtbarkeitsberechnung des Spiels.
- **Keine 60 fps / Zwischenbilder:** RT64 ordnet Matrizen zwischen Bildern über ihre Adresse zu, dieses Spiel legt sie
  aber jedes Bild neu an; Kamera und Geometrie sprangen. Das Spiel läuft mit seiner originalen Bildrate.
- **Keine Mod-Unterstützung** (Menüpunkt entfernt) und keine Texturpakete.
- Nur die US-ROM wird unterstützt. Linux- und macOS-Builds wurden nicht versucht.
- Das Spiel wurde nicht durchgespielt; Level 2–16 wurden nur auf Laden geprüft.
- Kein Bild-für-Bild-Vergleich mit echter Hardware; nur einzelne Szenen wurden mit einem echten N64 und einem
  Emulator verglichen.
- Das Menü *Controls* zeigt allgemeine Aktionsnamen, nicht die des Spiels.

### Bekannte Fehler

- Gelegentlich können sehr kurze Ton-Aussetzer auftreten (das Sitzungsprotokoll zählt auf dem Testrechner einige pro
  Minute).
- Die Vibration war mit dem Switch-Online-N64-Controller nur sporadisch spürbar; nicht mit dem Original verglichen.
- Das Stereobild ist stellenweise rechtslastig. Die Analyse spricht dafür, dass das die Surround-Mischung des Spiels
  selbst ist; auf echter Hardware bestätigt ist es nicht.
- In der Ich-Ansicht tauchen manche nahen Felsteile bei bestimmten Blickwinkeln auf und verschwinden. Das passiert
  auf einem echten N64 genauso (Sichtbarkeitsberechnung des Spiels) und ist kein Fehler des Ports.
- Der Speichertyp des Spiels ist nicht geprüft; alle Speichertypen sind erlaubt.
- Teile des Grafik-Microcodes, die das erste Level nicht nutzt (z. B. manche Partikel- und Billboard-Wege), sind
  ungetestet und können in späteren Leveln Grafikfehler zeigen.
- Da es keinen vollständigen Vergleich mit dem Original gab, können manche Eigenheiten vom Original stammen und
  manche Fehler des Ports sein. Berichte mit F9 helfen, das zu unterscheiden.
