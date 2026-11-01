# Indy64 – Analyse-Notizen

ROM: *Indiana Jones and the Infernal Machine (USA)*, `NIJE`, 32 MiB, z64 (big endian),
SHA-1 `93300D63412D6C7432109B5AD2E4A8B9348E9538`, IPL3 CIC-6102. Arbeitskopie ohne Sonderzeichen: `rom/indy.z64`.

## Speicherlayout

| Virtuell | Quelle | Inhalt |
|---|---|---|
| `0x40000400`–`0x4004D02F` | ROM `0x1000`–`0x4DC30` (vom IPL3 kopiert) | Boot-Segment: libultra, Kernel, Grafik/Audio-Treiber, Microcode, Daten |
| `0x4004D030`–`0x40081D4F` | – | BSS (wird in `boot_main` genullt) |
| `0x40000000`–`0x407FFFFF` | RDRAM 0–8 MiB | fester TLB-Eintrag 0 (2×4-MiB-Seiten) |
| `0x80000000` / `0xA0000000` | dasselbe RDRAM | kseg0/kseg1-Spiegel; nur Bootcode und Exception-Handler laufen dort |
| `0x41000000`–`0x410BE61F` | ROM `0x4DC30`–`0x10C250` (`0xBE620` Bytes) | zweites Code- und Daten-Segment („VM-Fenster“), rohe ROM-Daten |

### Das VM-Fenster `0x41000000`

`main_thread` (`0x400360C0`) holt sich über `get_game_config` eine Konfiguration. Bit `0x800` in Wort `0x11`
entscheidet über den Modus (`vm_demand_paging`):

- **Am Stück geladen:** `osMapTLB` legt 6 Einträge zu je 2×64 KiB an. Das RAM dafür ist `heap_top − 0xC0000`.
  Dann lädt ein einziger `osPiRawStartDma` ROM `0x4DC30` mit Länge `0xBE620`.
- **Demand-Paging:** `vm_init_paging` (`0x40036E68`) legt Seitentabelle und Slot-Pool unterhalb von `heap_top` an.
  Die Anzahl der Einträge und Slots steht im Konfigurations-Struct (+4 und +6). Ein TLB-Fehler landet in
  `general_exception_handler` (`0x40000AE0`). Der wählt per LRU einen 4-KiB-Slot (`vm_page_pick_slot`,
  `vm_page_lru_touch`) und lädt die Seite **direkt im Handler** per PI-DMA, von `vm_cart_base` (`0x1004DC30`)
  + (vaddr − `0x41000000`). Danach leert er den I-Cache und trägt die Seite in die TLB ein.
- `vm_state` (`0x40000454`): +0C Fehlerzähler, +10 Seitentabelle, +14 Slot-Pool, +18 Slot-Anzahl, +1C Opfer-Slot,
  +20 ROM-Basis, +24 BadVAddr, +28 EPC.
- **Voreinstellung:** `game_config_default` (`0x40045890`) hat Flags = 0. Beim Start läuft also Demand-Paging mit
  512 Tabelleneinträgen und 90 Slots (360 KiB), und `heap_top` = `osMemSize`. Den Modus „am Stück“ kann nur eine
  andere Konfiguration über `game_restart` einschalten. **Offen:** woher diese stammt.

**Folge für eine Rekompilierung:** Es gibt genau zwei feste Segmente und keine echten Overlays. Das Fenster
`0x41000000` lässt sich als normales Segment behandeln. Den TLB- und Paging-Code (Handler, `osMapTLB`-Aufrufe)
ersetzt man durch Stubs, und im Laufzeitsystem bildet man die Adressen `0x40…` und `0x41…` auf das RDRAM ab.

## RSP-Microcode

| Symbol | RAM | ROM | Größe | Nutzer |
|---|---|---|---|---|
| `rspboot_text` | `0x40001700` | `0x2300` | `0xD0` | nur Audio-Task |
| `f5_gfx_ucode_text` | `0x400371B0` | `0x37DB0` | `0x2880` | `gfx_task_init`: dient zugleich als Boot (boot_size `0x1000`), lädt also selbst Overlays |
| `f5_gfx_ucode_data` | `0x40047610` | `0x48210` | `0xF0` | Grafik |
| `f5_audio_ucode_text` | `0x40039A30` | `0x3A630` | `0x1000` | `audio_task_submit` (Typ 2), vermutlich MusyX |
| `f5_audio_ucode_data` | `0x40047700` | `0x48300` | `0x800` | Audio |
| `rsp_unknown_tail` | `0x4003AA30` | `0x3B630` | `~0x420` | unbekannt |

Die Blöcke liegen einzeln in `analysis/ucode/` (`tools/extract_ucode.py`, mit SHA-1).

### Grafik-Microcode: bekannt und per HLE dokumentiert

- **Erkennung:** `f5_gfx_ucode_text` hat über die ersten 4096 Bytes, in Emulator-Byte-Reihenfolge (wortweise
  vertauscht), die CRC32 **`0x6859BF8E`**. Genau unter diesem Wert führt GLideN64 die Microcode als „Indiana Jones“
  (`src/GBI.cpp`, Typ `F5Indi_Naboo`, dieselbe Microcode wie in *Battle for Naboo*).
- **HLE-Implementierung:** `src/uCodes/F5Indi_Naboo.cpp` (rund 2300 Zeilen, GPL-2.0, Commit `a04081a`, zum Lesen in
  `third_party/GLideN64_ref/`).
- **Befehlssatz** nach GLideN64, bestätigt durch den Spielcode (`tools/dl_opcodes.py`):
  `0x01` MOVEMEM (`w0` = `01 oooo ss`: Offset und Länge−1, zum Beispiel `0x0105C03F` = 64 Bytes nach DMEM `0x5C0`),
  `0x02` SET_DLIST_ADDR, `0x05` GEOMETRY_GEN, `0x06` DL, `0x07` BRANCHDL, `0xB4` TRI2,
  `0xB5` JUMPDL, `0xB8` ENDDL, `0xB9`/`0xBA` SETOTHERMODE, `0xBC` MOVEWORD (direkt an eine DMEM-Adresse, z. B.
  `0xBC000128`), `0xBF` TRI1, `0xE4` TEXRECT, dazu die normalen RDP-Befehle `0xE6`–`0xFF`.
- **Wie das Spiel Display-Listen baut:** in verketteten Blöcken zu `0x100` Bytes. Ist ein Block voll, schreibt der
  Code `0xB5000000 00000000` (JUMPDL), holt einen neuen Block (`FUN_40006378`) und verkettet ihn. Deshalb ist `0xB5`
  der häufigste Befehl im Code.
- **Folge:** RT64 kennt diese Microcode nicht. Es gibt aber eine vollständige HLE-Referenz. Mögliche Wege:
  1. `F5Indi_Naboo.cpp` nach RT64 portieren. Das ist Lizenzsache: GPL-Code in RT64 (MIT) macht das Ergebnis GPL,
     wie Zelda64Recomp ohnehin.
  2. GLideN64 selbst als Renderer hinter das Laufzeitsystem hängen, über die klassische Plugin-Schnittstelle
     (`GFX_INFO`, `ProcessDList`). Ungeprüft, ob ultramodern das zulässt.
  3. Die Grafik-Microcode samt Overlays per RSPRecomp übersetzen und das RDP auf niedriger Ebene rendern
     (paraLLEl-RDP). Am genauesten, aber am aufwendigsten.

### RT64-Port der Grafik-Microcode (kompiliert, noch nie gelaufen)

Neue Datei `third_party/rt64/src/gbi/rt64_gbi_f5indi.cpp` mit rund 1200 Zeilen. Es ist ein Port der Indy-Teile von
GLideN64s `F5Indi_Naboo.cpp` und steht deshalb unter GPL-2.0. Die DMEM-Arithmetik (Beleuchtung, Environment-Mapping,
Partikel) ist Zeile für Zeile übernommen. Die Anbindung an die Grafik läuft über RT64:

| GLideN64 | RT64 |
|---|---|
| `gSPSWVertex` (Positionen aus DMEM) | `RSP::setVertexHost`, neu: wie `setVertex`, aber aus einem eigenen Puffer |
| Farbe/ST pro Dreiecksecke | `RSP::modifyVertex(RGBA/ST)`, kopiert schon benutzte Vertices selbst |
| `drawDMATriangles` | `RSP::drawIndexedTri` |
| `gSPForceMatrix`, `gSPViewport` | `RSP::forceMatrix`, `RSP::setViewport` |
| Vertex-Matrix für GEOMETRY_GEN 0x18 | `modelViewProjMatrix` vorübergehend ersetzen (`modelViewProjInserted`) |
| `gDPTextureRectangle` (Billboards) | `RDP::drawTexRect` mit 10.2-Koordinaten und `posScreen`/`posTransformed` aus dem Workload |
| DL-Blöcke, `RSP.F5DL` | eigener Stapel `nextBlock[depth]`, JUMPDL/DL/BRANCHDL setzen `*dl` auf den Blockkopf |

Änderungen an RT64 außerhalb der neuen Datei:
- `rt64_gbi.h`: `GBIUCode::F5INDI` und `GBI::startDisplayList`. Das ist ein Hook für den 8-Byte-Kopf der Wurzel-Liste
  und das Löschen der sieben DMEM-Variablen pro Task.
- `rt64_gbi.cpp`: Eintrag `F5_INDI`, Text-Hash `0x1000` Bytes = `0x6326DDEF39B95B19`, Daten-Hash `0xF0` Bytes =
  `0xA7D53D8915E7AA44`. Beide sind XXH3-64 über die RDRAM-Byte-Reihenfolge und mit `tools/.venv` (Paket `xxhash`)
  berechnet. Dazu kommt der Setup-Fall.
- `rt64_interpreter.cpp`: ruft `startDisplayList` vor der Schleife auf.
- `rt64_rsp.h/.cpp`: `setVertexHost`.
- `CMakeLists.txt`: die neue Datei.

**Prüfung:** `tools/check_rt64_syntax.ps1` übersetzt die vier geänderten Dateien mit clang `-fsyntax-only`. Alle sind
fehlerfrei. Dafür liegen die reinen Header-Submodule unter `third_party/rt64/src/contrib`: hlslpp, plume, xxHash,
imgui, implot, im3d, re-spirv, ddspp, stb und zstd, dazu SDL2 2.30.9 (VC) unter `third_party/SDL2`. Gebaut und
gelinkt ist RT64 noch nicht; dafür fehlen unter anderem dxc und die Vulkan-Header von plume.

**Bewusst offen**, als TODO im Code markiert:
- **Nebel:** `MOVEWORD` 0x160/0x164 enthält Multiplikator und Offset in s15.16. Die Umrechnung auf RT64s F3D-Einheiten
  steht noch aus. Außerdem setzt `modifyVertex(RGBA)` den Nebel-Index der Ecke zurück, das muss beim Einbau des Nebels
  berücksichtigt werden.
- **Billboards:** Bei `setPrimDepth` ist die Skalierung ungeprüft. Bei `setPrimColor` werden LOD-Anteil und -Minimum
  auf 0 gesetzt.
- **„Prim shading“:** Ohne `G_SHADE` setzt GLideN64 die Eckfarben auf die Prim-Farbe. Wie RT64 das ohne `G_SHADE`
  behandelt, ist ungeprüft.
- **Partikel:** Der optionale Partikel-Pfad von GLideN64 (`F5INDI_PARTICLE_OPT`) ist nicht portiert. Benutzt wird der
  normale Texrect-Weg.
- **Naboo:** Die Naboo-Befehle fehlen; wird einer erreicht, schlägt eine Assertion an.

**Wichtig für das Laufzeitsystem:** Die Microcode maskiert alle RDRAM-Adressen auf 24 Bit. Zeiger in das VM-Fenster
`0x41xxxxxx` muss das Spiel deshalb vorher über `osVirtualToPhysical` umrechnen. N64Recomp ersetzt diese Funktion
durch eine eigene, und die muss `0x41000000`+ auf die physische Adresse abbilden, an die das Laufzeitsystem das Fenster
legt (im Original `heap_top − 0xC0000`).

### Ton läuft (2026-10-31)

Drei Fehler, alle behoben:
1. **Overlays im Audio-Microcode:** Er lädt während der Arbeit Code nach IMEM 0x680 nach, und zwar ucode+0x600
   (0x918 Bytes, identisch mit dem Startbild) und ucode+0xF20 (0x1A4 Bytes, neuer Code). Ohne Overlay-Eintrag lief
   der falsche Code, die Ausgabe blieb 0, und die DMA schrieb über das Ende von `dmem` hinaus. Jetzt in
   `recomp/rsp/f5_audio.rsp.toml` als `[[overlay_slots]]`. Neu erzeugen: `RSPRecomp.exe <absoluter Pfad zur toml>`,
   danach `tools/fix_rsp_output.py indy64recomp/rsp/f5_audio_ucode.cpp` (entfernt Schreibzugriffe auf `$zero` aus
   Datenwörtern).
2. **AI-Ereignis nur einmal pro Bild:** MusyX liefert pro AI-Ereignis genau einen Puffer mit 192 Stereo-Frames. Bei
   60 Ereignissen pro Sekunde sind das 11 520 statt 22 050 Frames. `ultramodern/src/audio.cpp` hat jetzt einen
   AI-Thread (`start_ai_thread`), der zusätzlich ein Ereignis schickt, sobald weniger als ~40 ms gepuffert sind und
   der letzte Puffer angekommen ist (wie der echte AI-Interrupt).
3. **Füllstand falsch umgerechnet** (`main.cpp`, von Harvest Moon übernommen): Die SDL-Warteschlange liegt schon in
   der Ausgaberate (48 kHz) vor, wurde aber mit der Spielrate gemessen. Dadurch sah sie ~2,2× voller aus, und die
   Sprung-Logik verwarf jedes zweite Sample (Ton eine Oktave zu hoch).
Ergebnis: Frames pro Sekunde = eingestellte Rate (22 050 / 24 000 / 32 000, das Spiel wechselt), Puffer stabil bei
~55–65 ms, keine Knackser in der Aufnahme. Werkzeuge: `INDY64_AUDIO_RAW=<datei>` (Rohaufnahme 16 Bit Stereo +
`[audio]`-Zeile pro Sekunde), `tools/audio_stats.py` (Pegel je Abschnitt, schreibt WAV). Hörprobe:
`indy64recomp/screens/canyonlands_ton.wav`. **Nicht gehört:** Ob Musik und Effekte richtig klingen (Tonhöhe,
Hall), muss jemand anhören.

### Spieltest im Hintergrund (2026-10-31)

Mit `test_bg.ps1` und Skript-Eingaben (Stick `UP/<ms>`, Steuerkreuz `DD` für Menüs; der Stick springt in Menüs nur
unzuverlässig weiter): Laufen, Springen, Absturz in die Schlucht → Todesmenü („Continue / Restart Level / Abort Game &
Save / Abort Game“), „Restart Level“, Inventar (START: Peitsche, Medizin, Feuerzeug, IQ), „Abort Game & Save“ →
„Are you sure?“ → Level-Auswahl. Alles richtig gezeichnet, kein Absturz. **Speichern geht:** Die Datei
`…\Indy64Recompiled\saves\indiana_jones_infernal_machine.bin` wird geschrieben (beim Start aus der Claude-App liegt
sie unter `AppData\Local\Packages\Claude_…\LocalCache\Local\Indy64Recompiled\`), Spielstand „AAA“ wird über
Programmstarts hinweg geladen. **Kleiner Fehler:** In abgedunkelten Bildern (Todesmenü, Inventar) ist die rechte
Bildspalte bunt gesprenkelt (vermutlich liest der Abdunkel-Effekt das Bild aus dem RAM und die letzte Spalte ist
ungültig). Noch nicht untersucht.

**Himmel im Level-Intro (geprüft 2026-10-31):** Früher war er am Intro-Anfang schwarz (z. B. `screens/bg16_68s.png`).
Mit dem aktuellen Stand ist er ab dem ersten Bild da: Das Bild blendet aus Schwarz ein, Himmel und Wolken sind
sofort sichtbar, wie im Referenzvideo. Schnellaufnahme mit `test_bg.ps1 -BurstFrom <s> -BurstTo <s>` (~2 Bilder/s).
Ursache war vermutlich die RT64-Zwischenbildberechnung (gleichzeitig behoben). Achtung: Wann das Intro kommt,
schwankt von Lauf zu Lauf um bis zu 20 s.

### Veröffentlichung vorbereitet: v0.1.0 (2026-11-01)

- Git-Repository in `D:\Project Indy64` (nur angelegt, noch kein Commit). `.gitignore` ist eine Positivliste:
  141 Dateien, 2,2 MB; ROM, erzeugter Spielcode (`RecompiledFuncs`, `rsp/f5_audio_ucode.cpp`), Dumps, Ghidra,
  Referenzen und die Bibliotheken bleiben draußen. `.gitattributes`: `* -text` (Patches byte-genau).
- Bibliotheken: `deps/deps.json` (Repo + Commit: N64ModernRuntime ca568b6, darin N64Recomp 2b6f056, RT64 f0d8c9f,
  RecompFrontend b3b7ebb) und `deps/patches/*.patch` mit unseren Änderungen. `tools/setup_deps.py` lädt alles
  (fetch_tree) und spielt die Patches ein; geprüft: Ergebnis inhaltlich identisch mit `indy64recomp/lib`.
  Nach Änderungen an lib: `tools/make_dep_patches.py <git.exe>`.
- Spielcode aus eigener ROM: `tools/generate_code.cmd` (prüft ROM per `tools/check_rom.py`, N64Recomp, RSPRecomp,
  fix_rsp_output); geprüft: byte-identisch mit dem bisherigen Stand.
- Dokumente: README.md (EN), README.de.md, BUILDING.md, CHANGELOG.md (v0.1.0, EN+DE), THIRD_PARTY_NOTICES.md, LICENSE.
- Einstellungen: Seitenverhältnis, HUD-Platzierung, Bildrate ausgeblendet (`src/game/config.cpp`); Mods-Tab und
  Launcher-Punkt „Mods“ entfernt.
- **Lizenzfrage:** GLideN64 ist „GPL version 2“ ohne „or later“; der F5-Port ist davon abgeleitet, der Rest GPL-3.0.
  Deshalb nur Quellcode, keine exe veröffentlichen, bis die GLideN64-Autoren zustimmen (noch nicht angefragt).

### Neuer Name „Babylon Infernalis“, eigener Launcher (2026-11-01)

- Name im Fenster/Launcher: `program_name` in `include/indy64_config.h`. `program_id` bleibt `Indy64Recompiled`
  (AppData-Ordner mit Einstellungen, Spielständen und eingelesener ROM). Build-Ziel/exe: `BabylonInfernalis`
  (CMake, `tools/build_indy64.cmd`, `test_bg.ps1`, `test_run.ps1`, `capture_window.ps1` angepasst).
- Launcher: `src/main/launcher.cpp` (eigener Hintergrund `assets/Babylon.svg` mit Pixel-Titel, Standardtitel
  entfernt). Hintergrund und App-Symbol (`icons/app.png`, `icons/app.ico`) erzeugt `tools/make_branding.py`
  (Retro-Pixelstil: Farbbänder, gestreifte Sonne, Zikkurat, Dünen; kein fremdes Bildmaterial).
- Vorlagen-Reste (Harvest Moon: ungenutzte Quellen, Patches, Bilder, Schriften, Kuh-Symbol, flatpak, .github,
  README/BUILDING, .gitmodules) und alte Debug-Dumps liegen in `D:\Project Indy64\archive\`. Zurückgeholt nur drei
  allgemeine Schnittstellen-Header in `patches/` (ui_funcs.h, patch_helpers.h, recompui_event_structs.h), die
  RecompFrontend einbindet. Fenster-/Audio-Code in main.cpp folgt der Recomp-Vorlage (GPL-3.0, Herkunft genannt).
- Spielordner jetzt `release\BabylonInfernalis\` (mit LICENSE.txt).

### Aufgeräumt, Spielordner (2026-11-01)

TEMP-Ausgaben im Overlay-Wechsel von `rsp/f5_audio_ucode.cpp` entfernt (nach einem RSPRecomp-Neulauf nicht wieder
einbauen). Die übrigen Diagnosen sind per Umgebungsvariable schaltbar oder auf wenige Zeilen begrenzt und bleiben.
Spielordner: `release\Indy64Recompiled\` (exe, pdb, SDL2.dll, dxcompiler.dll, dxil.dll, assets\, LIESMICH.txt,
Lizenz von mupen64plus-rsp-hle). Hintergrundtest von dort: Level, Fehlerbericht, sauberes Beenden. Nach jedem Build
neu kopieren (exe + pdb). Die Lizenz des Projekts selbst ist GPL-3.0 (aus der Harvest-Moon-Vorlage).

### Absturz beim Beenden behoben (2026-11-01)

`recomp::start` gab RDRAM frei, während nie gejointe Spiel-Threads (auf Message-Queues blockiert) noch liefen →
Lesezugriff auf freigegebenen Speicher; der Crash-Handler las selbst noch RDRAM und brach ab (kurze crash.log).
Jetzt: kein Freigeben unter Windows (`librecomp/src/recomp.cpp`), danach `TerminateProcess` am Ende von `main`
(Sitzungsprotokoll „Programm beendet“). Test-Eingabe `QUIT` in `INDY64_AUTOINPUT` (wie der Quit-Knopf der
Abfrage). Zwei Hintergrundläufe (Intro, Level): sauber beendet, kein crash.log.

### Ich-Ansicht/Kriechen: Streifen unten, fehlende Felsstücke (2026-11-01)

In der Ich-Ansicht beim Blick nach oben und beim Kriechen fehlte unten ein waagerechter Streifen der 3D-Welt (dort
stand ein altes Bild), und nahe Felsteile verschwanden; auf dem N64 nicht. Die Kante ist waagerecht und liegt immer
gleich hoch, passt also zum Nah-Clipping: Der Boden direkt vor der Kamera liegt vor der Near-Plane und wurde von der
GPU weggeschnitten. Versuch mit `NoN = true` für F5_INDI: **half nicht, zurückgenommen.** Das verschwindende
Felsstück macht das N64-Original genauso (Portal-Culling des Spiels), nur der Streifen ist ein Fehler. Das Spiel
löscht das Bild nicht (kein Fill-Rect); Bereiche ohne Geometrie zeigen also den alten Pufferinhalt. Auf dem N64 ist
das das Bild von vor 3 Frames (fällt nicht auf), bei uns evtl. viel älter. Offen: zeichnet das Spiel dort wirklich
nichts? Dafür schreibt der Fehlerbericht jetzt pro Dreieck Clip-Koordinaten, Scissor und „drawn/DROPPED“ in
`display_list.txt` (`rt64_gbi_f5indi.cpp`, drawTri).

**Ursache gefunden:** Den unteren Rand decken drei Bodendreiecke mit einem Eckpunkt hinter der Kamera (w < 0) ab;
sie werden gezeichnet (auch mit Nah-Clipping bis weit unter den Bildrand). RT64 schätzt aber in `RSP::drawIndexedTri`
(`rt64_rsp.cpp`) den gezeichneten Bereich aus den projizierten Ecken, und ein Eckpunkt hinter der Kamera landet
oben. `drawColorRect.bottom` wurde ~424 statt 448, und daraus wird die Höhe des Farbziels gemacht
(`nativeColorHeight` in `rt64_workload_queue.cpp`) → Zeilen darunter behielten ein altes Bild. Behoben: bei einem
Eckpunkt mit w ≤ 0 zählt der ganze Scissor, und die Rückseiten-Prüfung dort wird übersprungen.
Werkzeug: `scratchpad coverage.py` (Abdeckung der Bildzeilen aus den tri-Zeilen). **Vom Spieler bestätigt: weg.**

### Dialog-Streams: verworfene Ladeaufträge (2026-11-01, zweiter Teil)

Die gesprochenen Dialoge sind Geräusch-Typ 2 (`FUN_41087c0c` → `FUN_40034dd8`, 11 kHz, zwei Stream-Plätze ab
0x65FA0). Ihre Tabelle `mort` (0x330 Einträge, Zeiger `DAT_400683fc`, Anzahl `DAT_40068400`) lädt `FUN_4003567c`
über `FUN_40035df0`: Auftrag in eine Liste, die der Audio-Frame (`FUN_40033e28`) per `FUN_40035c70` an die
DMA-Liste `DAT_4005ba8c` hängt; einen Frame später meldet `FUN_40035c70` „fertig“. Die Liste geht aber nur an den
Lade-Thread (`FUN_40031548`), wenn der Frame einen Puffer auffüllt; der nächste Frame setzt sie zurück
(`FUN_40031594`). Bei uns kommt manchmal ein AI-Ereignis ohne Auffüllung → Auftrag verloren, Tabelle blieb 0/leer
(Emulator: 0x330 Einträge), alle Dialoge wurden still übersprungen. **Behoben** mit dem Hook
`indy64_audio_dma_fix` (main.cpp, toml bei 0x4003410C): eine liegengebliebene Liste wird nachgereicht.
Sitzungsprotokoll: „Audio: N Ladeauftraege … nachgereicht“. Danach Tabelle geladen und ab ~75 s Stream-Plätze
belegt (`INDY64_POLL=683F0,30`). **Muss angehört werden.**

Nebenbei: `INDY64_AUDIO_HLE=1` ersetzt den übersetzten Audio-Microcode durch mupen64plus-rsp-hle `musyx_v2_task`
(`src/audio_hle/`, GPL-2.0+), zum Vergleichen. Damit stürzte das Spiel wegen desselben Fehlers bei 73 s ab
(Tabelle `count` geladen, Zeiger 0).

### Sprachausgabe: Ursache gefunden (2026-11-01)

Vergleich der Speicherabzüge kurz nach dem Start von `s_9` (Emulator Frame 4360/4460, bei uns `INDY64_VOICE_DUMP`):
Sprachdatei und MusyX-Sequenzplätze waren gleich, aber die OSIoMesg-Blöcke des Spiels ab 0x54DF8 (Ziel, ROM-Adresse
0xB1…, Größe) waren bei uns leer. libultras `osPiStartDma` füllt den übergebenen OSIoMesg selbst aus (type, pri,
retQueue, dramAddr, devAddr, size); der Ersatz in `librecomp/src/pi.cpp` tat das nicht, und die Fertig-Nachricht war
0 statt dieses Blocks. MusyX streamt die Sprach-Samples aus der ROM und erkennt angekommene Stücke an diesem Block,
also kam die Sprache nie an und der Sprach-Platz blieb belegt (danach keine weiteren Bemerkungen). Beides ist jetzt
wie in libultra. Ergebnis: Der Sprach-Platz (Tabelle 0x7E8F0) wird nach ~20 s wieder frei wie im Emulator. **Muss
noch angehört werden.** Werkzeuge: `INDY64_PI_LOG=1` (jeder ROM-Lesezugriff), `INDY64_RSP_DMA_RANGE`,
`D:\Tools\BizHawk\indy_speech.lua` / `indy_dumpv.lua`.

### Sprachausgabe fehlt (Stand 2026-10-31)

Indy sagt bei uns nichts (im Original am Levelanfang „Time to go to work“). Befunde:
- Sprach-Pfad: `FUN_41087c0c` (Geräusch abspielen, Typ 1 = Sprache; Sperren: `DAT_4007fff8`, 64 Plätze ab
  0x7E8F0 à 0x5C, freier Heap ≥ 0x10400) → `FUN_41009bc0` (Auftrag an Queue +0x49C) → Lade-Thread `FUN_410098a8`
  (lädt `sound/s_NNN`, `FUN_41009dac` → MusyX-Gruppe, `FUN_40028f24` → `FUN_40022400` = Sequenz starten, Rückgabe =
  Sequenznummer, −1 = Fehler). Diagnose-Hooks in der Recomp-toml → Sitzungsprotokoll „Sprache: …“.
- Bei uns: `s_9` wird angefordert, geladen (0x8026E9E0) und als Sequenz 0 gestartet, aber nie fertig; der
  Sprach-Platz (Typ 1) bleibt belegt, danach kommen keine weiteren Sprach-Aufträge. Der Audio-Microcode liest die
  Sprachdatei nie (`INDY64_RSP_DMA_RANGE=26E9E0,2A0000` → 0 Lesezugriffe).
- Emulator (`D:\Tools\BizHawk\indy_speech.lua`): `s_9` bei Frame 4330, `s_113` bei 5545, Platz frei bei 6705.
- Speicher/Konfiguration gleich wie im Original (freier Heap bei uns sogar größer).
- Nächster Schritt: Speicherabzüge kurz nach dem Start vergleichen: bei uns `INDY64_VOICE_DUMP=1` →
  `indy64recomp\ram_voice_a/b.bin` (0,25 s/2 s nach dem Start), Emulator `indy_dumpv.lua` → `D:\Tools\BizHawk\dumpv\
  ram_a/b.bin` (Frame 4360/4460). Vergleichen: geladene Datei, MusyX-Sequenzplätze (0x59B90 + i·0xA30, Startflag
  0x5A588), Stimmen.
- Nebenbei behoben: PI-DMA meldet jetzt das OSIoMesg als Fertig-Nachricht (`librecomp/src/pi.cpp`), wie libultra.

### Testspiel-Hilfen und Ton-Stottern (2026-10-31)

**Fehlerberichte:** F9 oder die Capture-Taste am Controller → `build\bugreports\<Zeit>\` mit `screenshot.png`,
`display_list.txt` (nächstes Bild), `ram.bin`, `info.txt` (Level, Bildrate, Controller), `session.log`,
`stderr_tail.log`, `notiz.txt`. Jede Sitzung schreibt `build\logs\session_<Zeit>.log` (Controller samt SDL-Zuordnung,
erster Druck jeder Taste/Achse, Levelwechsel, Bildraten-Einbrüche, Ton-Aussetzer, Bildrate pro Minute); ohne Konsole
geht stderr nach `logs\stderr_<Zeit>.log`. Ein `crash.log` der letzten Sitzung wird beim Start nach
`bugreports\<Zeit>_absturz\` verschoben. Code: `src/main/bug_report.cpp`, Haken `recompinput::sdl_event_hook`,
RT64 `requestDisplayListDump`/`indy64DisplayListCount`. Testskript: `REPORT` in `INDY64_AUTOINPUT`.

**Controller „Nintendo N64 Controller“ (Switch Online, Bluetooth):** SDL 2.30 erkennt ihn. Zuordnung laut Log:
`a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,leftstick:b7,leftshoulder:b9,rightshoulder:b10,lefttrigger:a4,
righttrigger:a5,misc1:b15`, also A, B, L, R, Z (lefttrigger), ZR (righttrigger), Start, Capture (misc1). Die vier
**Bunte Leiste rechts behoben:** Das Spiel setzt den Scissor auf Breite − 1 / Höhe − 1 (`ED000000 0063C6FC` bei
400×448), die letzte Spalte und Zeile werden nie gezeichnet und zeigten RDRAM-Reste. `VI::cropRectangle` schneidet
sie jetzt ab. **Offen:** Absturz beim Beenden (Lesezugriff auf RDRAM 0xC3A00 nach dem Schließen, `crash.log` ohne Stack).

Tasten nacheinander gedrückt (Sitzungsprotokoll). Laut Spieler waren C-oben und C-links zunächst vertauscht, richtig:
C-links = x (b2), C-unten = righttrigger (a5), C-oben = y (b3),
C-rechts = back (b4), ZR = leftstick (b7), Home = guide (b5). Die Standardbelegung passte nicht (N64-B kam als
C-rechts an). Jetzt eigene SDL-Zuordnung für diese GUID in `main.cpp` (nach dem Laden der controllerdb): B → west,
R → righttrigger, C-Tasten → ±rightx/righty, Home → back (Menü), Capture → misc1 (Fehlerbericht).
**Start per Doppelklick:** Das Programm braucht `assets\` und `recompcontrollerdb.txt` im Arbeitsordner. `main()`
wechselt jetzt selbst in den Ordner neben der exe oder eine Ebene höher (sonst schloss sich das Fenster sofort).

**Ton-Stottern:** `tools/audio_repeats.py` zählt Puffer, die einen früheren exakt wiederholen. Alle Treffer liegen
genau 8 Puffer zurück (MusyX-Ring aus 8 × 192 Frames ≈ 70 ms). Anfangs 6,4 % hörbare Wiederholungen, gehäuft in
Sekunden mit weniger Audio-Aufgaben. Maßnahmen: AI-Ereignisse im Takt der Wiedergabe statt einmal pro VI
(`ultramodern/src/audio.cpp`, VI-AI-Nachricht in `events.cpp` entfernt); keine AI-Ereignisse mehr, wenn seit 7
Ereignissen keine Audio-Aufgabe nachgefüllt hat (`notify_audio_refill`, Notbremse 200 ms); Unterbrechungspunkte
`preempt_point` (`mesgqueue.cpp`) in der Lade-/Cache-Schleife von Thread 16 (`FUN_4101dd98`, `FUN_41018714`,
`FUN_4101887c`, `FUN_41018178`, Hooks in der Recomp-toml). Danach 4,5 %. Bei den restlichen Treffern warten alle
Spiel-Threads ganz normal (`INDY64_STALL_TRACE=1` → `stall.log`/`repeat.log`). Möglicherweise sind das echte
Schleifen im Ton (z. B. Umgebungsgeräusch). **Muss angehört werden.**

### Alle Level geladen, Nebel repariert (2026-10-31)

**Level direkt starten:** GameShark-Code „Start on Level“ `D0080C37 0000` + `80080C37 00xx` (xx: 00 Canyonlands,
01 Babylon, 02 Tian Shan, 03 Shambala, 04 Palawan Lagoon, 05 Palawan Volcano, 06 Palawan Temple, 07 Jeep, 08
Teotihuacan, 09 Olmec Valley, 0A Pudovkin, 0B Meroe, 0C King Sol's Mines, 0D Nub's Tomb, 0E The Machine, 0F Aetherium;
Quelle almarsguides.com). Im Programm: `INDY64_GS="<code>,<code>"` (`crash_log.cpp`, unterstützt 80/81/D0/D1).
Rundgang: `tools/test_levels.ps1 [-Levels n] [-Seconds s]` → `screens\lvl<NN>*.png`, Logs in `indy64recomp\levels\`.
Ergebnis: alle 16 Level laden ohne Absturz, Übersicht `screens\levels_sheet.png`.

**Nebel:** In Palawan Lagoon und Palawan Temple war alles in Nebelfarbe. Ursache: Der Port setzt die Eckfarben pro
Dreieck mit `RSP::modifyVertex(G_MWO_POINT_RGBA)`, und das schaltet in RT64 den Nebel des Eckpunkts ab (MODIFYVTX-
Verhalten). Dann wurde das Farb-Alpha (0xFF) zum Nebelanteil. Jetzt bekommt der Eckpunkt danach wieder
`rsp.curFogIndex`, wie in GLideN64 kommt der Nebel aus der Tiefe. Außerdem werden die Nebelwerte (MOVEWORD 0x160
Faktor, 0x164 Versatz, s15.16) jetzt an RT64 übergeben (×256 wie F3D). Standardwerte des Spiels: 4999,17 / −4998,17.

### Audio-Microcode: mit RSPRecomp übersetzt

- **Konfiguration:** `recomp/rsp/f5_audio.rsp.toml` (ROM `0x3A630`, IMEM `0x1080`, `0xF80` Bytes). Ergebnis ist
  `recomp/rsp/f5_audio_ucode.cpp` (84 KB). RSPRecomp **muss mit absolutem Pfad zur Konfiguration** aufgerufen werden,
  sonst stürzt es ab (leerer `parent_path` bei `create_directories`).
- **Datenwörter:** Zwei Wörter sind keine Befehle (`0x1678`, `0x1F9C`, jeweils direkt hinter einem Sprung). Das ist
  harmlos.
- **Kein Verteiler:** Es gibt keine Befehls-Sprungtabelle. Die Microcode ruft Unterprogramme über `jal`/`jr` auf,
  einige über `$fp`/`$t0` als Rücksprungregister, und liest Parameter aus DMEM `0xD60`…. Die Daten enthalten die
  übliche N64-Resampling-Tabelle. Ob zur Laufzeit „Unhandled jump target“ auftaucht, zeigt erst ein Test.
  Werkzeug: `tools/rsp_jump_targets.py`.

- **Grafik:** RT64 kennt die Factor-5-Grafik-Microcode nicht. Das ist das größte Hindernis für einen nativen Port:
  Entweder wird die Microcode per RSP-Rekompilierung (RSPRecomp) samt Overlays nachgebildet und das RDP auf
  niedriger Ebene dargestellt, oder man schreibt ein HLE-Frontend für ihr Display-List-Format. Für Letzteres kann die
  GLideN64-Implementierung als Vorlage dienen.
- **Audio:** Mit `osSpTask` und rspboot läuft die Audio-Microcode im üblichen Rahmen. RSPRecomp kann sie übersetzen.

## libultra

Die Signaturen von n64sym (`third_party/n64sym/builtin_signatures.sig`, MIT-Lizenz, shygoo) werden mit eigenem Code
abgeglichen (`tools/sigmatch.py`); ausgeführt wird von n64sym nichts. Ergebnis:

- **Lage:** libultra liegt zusammenhängend bei `0x4003AE50`–`0x40044C00`, direkt hinter der Microcode.
- **Umfang:** 124 Funktionen per Signatur, dazu 50 Datensymbole aus den Relokationen, etwa `__osRunQueue`, `__osPiDevMgr`, `osViModeNtscLan1`,
  `__osContPifRam` und `osClockRate`. Die Größen stehen ebenfalls in `symbols.csv`.
- **Bibliotheken:** Enthalten sind EEPROM, Controller-Pak (Pfs), Rumble Pak (Motor), 64DD (Leo) und `sprintf`/`_Printf`.
- **Factor-5-Kopie von osInitialize:** `boot_main` ruft eine eigene Kopie bei `0x40036AF0` auf. Die Bibliotheksversion
  bei `0x4003D080` hat keine Aufrufer.
- **Identische Funktionspaare:** Wo zwei Funktionen denselben Code haben, ist die Zuordnung über die Link-Reihenfolge
  aufgelöst (`tools/merge_sigs.py`). Bei `osMotorStop`/`osMotorStart` ist die Reihenfolge nicht geprüft.
- **Grenze:** Funktionen unter `0x14` Bytes erkennen die Signaturen nicht eindeutig. Drei davon (`__osGetSR`,
  `__osSetSR`, `__osSetFpcCsr`) sind von Hand ergänzt.

Ablauf bei neuen Signaturen:
`sigmatch.py rom sig symbols/libultra_matches.csv`, dann `merge_sigs.py symbols/libultra_matches.csv symbols/symbols.csv`,
dann `tools/rebuild_ghidra.ps1`.

## N64Recomp: Stand

**N64Recomp läuft fehlerfrei durch** (Exit-Code 0, 2536 Funktionen). Das Ergebnis sind 52 Dateien in
`recomp/RecompiledFuncs` mit zusammen rund 25 MB C-Code. clang meldet für alle 49 `.c`-Dateien nur Funktionen und Makros,
die das Laufzeitsystem bereitstellen muss, sonst keine Fehler (`-fsyntax-only`). Gebaut oder gelinkt ist noch nichts,
und kein Laufzeitsystem ist angebunden.

- **Recompiler:** N64Recomp `ffb39cd` samt Submodulen liegt unter `third_party/N64Recomp`. Gebaut wird mit
  `tools/build_n64recomp.cmd` (MSVC + Ninja), das Ergebnis liegt in `third_party/N64Recomp-build/N64Recomp.exe`.
- **Ein Durchlauf:** `tools/run_recomp.ps1 [-Rebuild]`. Das Skript exportiert die Funktionen aus Ghidra und erzeugt
  `indy64.syms.toml` sowie `indy64.recomp.toml`, die Basis plus erzeugte Patches. Dann ruft es N64Recomp auf und schreibt
  `recomp/recomp_run.log`.
- **Erzeugte Patches** (`tools/make_recomp_config.py`):
  - 16× `cache` → `nop`
  - 6× `bltzal $zero` → `lui`/`ori ra`. Damit ermittelt handgeschriebener Code seine eigene Adresse, um PC-relative
    Tabellen zu finden und Code-Schnipsel per `jalr` anzuspringen.
  - Von Hand: der Einsprung, nämlich die `mtc0`-Befehle für die TLB und der Sprung über das TLB-Alias, ersetzt durch `j boot_main`.
- **Platzhalter (`stubs`):** TLB- und Paging-Code, `install_exception_vectors`, `cache_create_dirty_range` und
  `osInitialize_lib_unused`.
- **Korrekturen an Funktionsgrenzen** (Spalte `size` in `symbols.csv`, angewendet von `ApplySymbols.java`):
  - `rle_unpack` zusammengeführt
  - Tabellen in Assembler als Daten markiert
  - Code-Schnipsel `asm_snippet_*` angelegt
  - `cache_create_dirty_range` liegt mitten in `.data`
  - rspboot ist aus den Codebereichen ausgenommen
- **Prüfwerkzeuge:**
  - `tools/scan_hw_insns.py`: Befehle, die N64Recomp nicht übersetzen kann
  - `tools/scan_boundaries.py`: Datenfunde, PC-Tricks, Sprünge mitten in fremde Funktionen
  - `tools/runtime_needs.py`: was das Laufzeitsystem bereitstellen muss

**Was das Laufzeitsystem noch liefern muss** (Ausgabe von `runtime_needs.py`):

| Funktion | Aufrufer | Vorschlag |
|---|---|---|
| `osYieldThread` | 11 Spielfunktionen | in ultramodern ergänzen (Thread abgeben) |
| `osViGetCurrentField` | `vi_scheduler_thread` | einfach 0 zurückgeben |
| `osMapTLB`, `osPiRawStartDma` | `main_thread` (VM-Fenster laden) | `main_thread` so patchen, dass es ROM `0x4DC30`–`0x10C250` einmal nach `0x41000000` kopiert |
| `__osSpRawStartDma`, `__osSpDeviceBusy` | `gfx_task_start` | Platzhalter; `osSpTaskLoad` lädt `ucode_data` ohnehin |
| `MEM_WU` (Makro für `lwu`) | `FUN_41029c94` | im Laufzeitsystem-Header ergänzen |

Dazu kommen die großen Punkte: die Adressumsetzung für `0x40…` und `0x41…` (die Speicher-Makros ziehen
`0x80000000` ab) und die Grafik der Factor-5-Microcode.

## Host-Programm `indy64recomp/` (im Aufbau)

**Grundlage:** *Harvest Moon 64 Recompiled* (`HarvestMoon64Recomp/HarvestMoon64Recomp` @ `ddd991c`, GPL-3.0), mit
genau seinem Versionssatz, weil die Bibliotheken nur in dieser Kombination zusammenpassen:
- N64ModernRuntime `ca568b6` (darin N64Recomp `2b6f056`)
- RecompFrontend `b3b7ebb` (Menüs mit RmlUi, Eingabe, RT64-Anbindung `rt64_render_context.cpp`)
- RT64 `f0d8c9f`

Unsere neuesten Versionen (RT64 `d9817c6`, N64ModernRuntime `cdf5abb`) haben sich davon wegentwickelt. Vor allem die
Konfigurations-API unterscheidet sich. Die Harvest-Moon-spezifischen Teile (Decomp, Symbole) sind nicht geladen.
- **Download:** `tools/fetch_tree.py <owner/repo> <commit> <ziel> --skip … --exclude …` holt ein Repository samt aller
  Submodule rekursiv, ohne git. zstd braucht `--exclude "zstd-*/tests*"`, weil dort symbolische Links liegen.
- **F5-Port:** Er ist in `indy64recomp/lib/rt64` übertragen. Die neue Datei sowie `rt64_gbi.h/.cpp`,
  `rt64_interpreter.cpp` und `rt64_rsp.h` sind dort identisch und wurden kopiert. `rt64_rsp.cpp` und `CMakeLists.txt`
  sind von Hand angepasst. Die Syntaxprüfung ist fehlerfrei
  (`check_rt64_syntax.ps1 -Root indy64recomp\lib\rt64`).
- **Erster Build:** Der Build von RT64 `d9817c6` mit dem Port lief durch: 470 Ziele, 0 Fehler, `third_party/rt64-build/rt64.lib`.
  Diese Kopie dient nur noch als Referenz.

**Speicher: das zentrale Problem für das Laufzeitsystem.** librecomp reserviert 4 GB Adressraum, und `MEM_*` rechnet
`rdram + addr − 0x80000000`. Nur die ersten 512 MB (kseg0) sind beschreibbar.
- **Folge:** `0x40xxxxxx` landet bei Offset `0xC0000000`. Dort ist der Speicher nicht zugänglich (PAGE_NOACCESS), und
  es wäre ohnehin nicht derselbe Speicher, den RT64 und DMA über die physischen bzw. kseg0-Adressen sehen.
- **Plan:** In `librecomp/src/recomp.cpp` den Speicher als Pagefile-Mapping anlegen (`VirtualAlloc2` mit
  `MEM_RESERVE_PLACEHOLDER`, `CreateFileMapping`, `MapViewOfFile3`). Dann wird dasselbe RDRAM bei Offset 0 (`0x80…`) und
  bei `0xC0000000` (`0x40…`) eingeblendet.
- **VM-Fenster:** `0x41000000` bekommt eine eigene physische Lage, zum Beispiel phys `0x00C00000`, zwischen den
  librecomp-Bereichen `0x80801000` und `0x81000000`. Das Host-Programm füllt sie beim Start mit ROM `0x4DC30`–`0x10C250`.
  Das Fenster ist auf dem N64 nur lesbar, denn im Paging-Modus werden Seiten verworfen und neu geladen.
- **osVirtualToPhysical:** `0x40…` → phys = addr − `0x40000000`, `0x41…` → `0x00C00000` + (addr − `0x41000000`).
- **Voreinstellung des Spiels:** Es läuft im Paging-Modus. `main_thread` nimmt damit den Zweig mit
  `install_exception_vectors` und `vm_init_paging`, beide sind Platzhalter, und der Speicher ist schon da.
  `osMapTLB` und `osPiRawStartDma` werden nur im anderen Zweig aufgerufen, brauchen aber Stubs fürs Linken.

**Stand (2026-10-30):** Die Schritte oben sind umgesetzt; das Programm startet, zeigt das Factor-5-Intro und erreicht
nach rund 100 s ein Level (HUD sichtbar, 3D-Welt noch schwarz). Bauen: `tools/build_indy64.cmd`, testen:
`tools/test_run.ps1 -Name <n> -Seconds <s> [-Every <s>]` (Bilder in `indy64recomp/screens`, `crash.log`, `sample.log`).

**Änderungen am Laufzeitsystem** (alle mit „Indy64“ kommentiert):
- `threads.cpp`: `__osRunningThread` (0x40047A00) wird beim Fortsetzen eines Threads geschrieben, die Task-Verwaltung
  des Spiels (`FUN_40005f30`, Liste 0x40056B94, OSThread bei +0x48) liest ihn. Erst *nach* der Prüfung auf
  „zerstört“, sonst trägt sich ein per `osDestroyThread` geweckter Thread fälschlich ein.
- `scheduling.cpp`/`ultra_translation.cpp`: `osYieldThread` wie libultra (`yield_to_ready_or_wait`).
- `events.cpp`: `osViSetMode` setzt die VI-Statusbits zurück wie libultra; das Spiel ruft nur `osViBlack(1)` und
  verlässt sich darauf, dass der nächste `osViSetMode` den Bildschirm wieder einschaltet.
- `overlays.cpp`: `get_function` löst bei unbekannter Adresse `__debugbreak()` aus, damit `crash.log` den Stapel zeigt.
- N64Recomp `analysis.cpp`: Sprungtabellen auch mit `lui/ori` und in einem anderen Abschnitt als die Funktion
  (Skript-VM `FUN_41029c94`, Tabelle 0x4004A3B0 mit 53 Befehlen).

**Spiel-Patches** (`recomp/indy64.recomp.base.toml`): zwei Warteschleifen ohne Pause (`FUN_400341cc`,
`FUN_4002fd2c`) rufen `yield_self`; die Argument-Holer der Skript-VM springen mit `jr t6/t7` zurück (→ `jr ra`);
der „native Funktion aufrufen“-Befehl springt in 8-Byte-Stücke bei 0x41029F74 (→ Hook + Rücksprung zur Schleife).

**3D-Welt schwarz, Stand der Suche:** Farben und Combiner sind richtig. Fast alle Welt-Dreiecke wurden aber als
„außerhalb des Bildes“ verworfen. Vermutete Ursache: `reset` im F5-Port löschte bei jedem Grafik-Auftrag das ganze
DMEM, GLideN64 behält es. Werte, die das Spiel nur selten setzt (z. B. die Basisadresse der indizierten Eckpunkte bei
0x154), gingen so verloren. Jetzt bleibt das DMEM erhalten. Außerdem läuft die Material-Unterliste jetzt vor dem
ersten sichtbaren Dreieck.

**Richtige Ursache (Test 43, per Vergleich mit Project64):** `FUN_40021a84` (Float-Matrix → Festkomma) wandelt die
Zeilen 1–3 per Aufruf von `FUN_40021ad4` um und *fällt dann ohne Sprung* in `FUN_40021ad4` hinein für Zeile 4. Die
rekompilierte Funktion endete dort, Zeile 4 (Verschiebung) wurde nie geschrieben. Behoben per Hook in
`indy64.recomp.base.toml`. Die Notlösungen unten (Zeile 4 = 0, Tiefe stauchen) sind wieder entfernt. Ergebnis:
N64-Logo (Zahnräder) und Factor-5-Logo werden komplett 3D und texturiert gezeichnet, kaum noch verworfene Dreiecke.
Weitere Fall-through-Funktionen gibt es laut Suche nicht. Offen: eine Szene aus weißen Streifen (vermutlich das
LucasArts-Logo aus Partikeln) und das Intro läuft im Kreis statt ins Hauptmenü.

**Hauptmenü sichtbar (Test 51):** Das Spiel nutzt Interlace-Hochauflösung (VI-Modus 8, VI-Breite 1024 für ein 512
breites Bild, Ursprung +0x400). `VI::fbAddress` in RT64 rechnete die Zeile mit der VI-Breite statt der Bildbreite
und fand das Bild nicht. Behoben in `rt64_vi.cpp`. Jetzt sichtbar: Logo, START, OPTIONS.
Offen: (1) das Hintergrundbild des Menüs (108 Kacheln, 216 Dreiecke, Textur RGBA16 ab 0x6AC3E0) und die Welt in der
Demo (Wüste) bleiben unsichtbar, obwohl Geometrie (Bildschirm-Koordinaten, Tiefe) und Texturdaten im Speicher
stimmen. Ausgeschlossen: Eckpunktfarben, Backface-Culling, Framebuffer-Kopien als Textur. Die Logos (auch Dreiecke)
gehen. Unterschied der Kacheln: Textur ohne Perspektivkorrektur (othermode H 0x2CCF), indizierte Eckpunkte.
(2) Text-Rechtecke laufen rechts aus (Textur-Kante/Clamp). (3) Ablauf: Copyright → N64-Logo → Factor 5 → Menü →
Demo → wieder von vorn, das passt zum Original (Attract-Modus).
Emulator-Hinweis: Project64 bleibt im Interpreter bei Grafik-Auftrag 377 in der Audio-Emulation hängen
(`__osAiDeviceBusy`-Schleife), unabhängig vom Audio-Plugin.

**Hintergrund + 3D-Welt sichtbar (Test 61):** Zwei weitere Fehler im F5-Port:
- RT64 rechnet auf der GPU „MVP × Inverse(Projektion)“. F5 setzt nie eine Projektion, nach `RSP::reset` ist sie
  eine Nullmatrix → GPU-Positionen unbrauchbar. Der Port setzt Projektion/View jetzt auf Einheitsmatrizen (`reset`).
- GLideN64 negiert y nach der Transformation (`gSPLoadSWVertexData`). Der Port negiert jetzt die y-Spalte der
  F5-Matrizen (`flipY`), damit Bild aufrecht und Dreiecks-Umlaufsinn/Culling richtig sind.
Ergebnis: Hauptmenü mit Indy-Gemälde, Factor-5-Logo mit 3D-Animation, Demo-Level im Schnee (Tempel, Schneeflocken).
Testschalter `INDY64_FLAT` (nur Schattierfarbe) bleibt als Hilfe im Port.

**Spielbar bis ins erste Level (Test 63):** Tastatur geht (Enter = Start, Leertaste = A). Ablauf: Menü → Enter →
„SELECT GAME“ → Spielstand „AAA“ anlegen → „Are you sure?“ → Canyonlands lädt, Indy, HUD, Wasserfall, Himmel
sichtbar. Testskript: `tools/test_run.ps1 -Keys "42:Enter,50:Enter,58:Enter,66:Enter,74:Enter,82:Space,90:Space,98:Enter,106:Space"`
(Tasten per `tools/send_key.ps1`). Offen im Level: einige Flächen nahe der Kamera zu riesigen Spitzen verzerrt,
Himmel nur teilweise (schwarze Bereiche, dort verschmieren Objekte), Bildausschnitt nur 400 Pixel breit. Die
GPU-Pipeline von RT64 clippt homogen korrekt; Verdacht eher auf einzelne Matrizen (z. B. `mtxVtxGen` für erzeugte
Geometrie) oder fehlende Himmels-/Hintergrund-Befehle.

**Level-Grafik (Test 68):** Tastaturbelegung nach Nutzer-Layout (WASD, Leertaste = A, E/F = B, L-Shift = Z,
Pfeile = C, R = R, Enter = Start) in `main.cpp` (vor `init_config`). Tiefenbereich für F5 erweitert: z/w von −1 bis 3
statt −1 bis 1 (Near-Plane bleibt, Far-Plane wird nicht mehr abgeschnitten) → Himmel deutlich vollständiger. Offen:
Spitzen von Geometrie sehr nah an der Kamera (w ≈ 0,8; mathematisch korrekt transformiert → Verdacht: Kamera
sitzt zu tief/zu nah, evtl. Fehler in Kamera-/Kollisionscode der CPU), doppelter Indy (alte Bilder scheinen durch),
rechts unten fehlt noch Himmel.

**Referenzvideo:** `D:\Project Indy64\reference\` (N64-Longplay, 6 h). Übersichtsbilder mit
`ffmpeg -ss <s> -t <s> -i <video> -vf "fps=1/3,scale=400:-1,tile=4x5" -frames:v 1 <png>` (ffmpeg per WinGet installiert,
Pfad siehe Sitzungsprotokoll: ...\WinGet\Packages\Gyan.FFmpeg...\bin\ffmpeg.exe). Canyonlands beginnt bei ~1:40.
Im Original: Bild voll 4:3, Kamera leicht erhöht hinter Indy, kaum Himmel. Bei uns: Kamera-Flug am Levelanfang
identisch, im Pausemenü kurz nach Levelstart auch richtig; ab ~10 s Spielzeit (auch ohne Eingabe) sitzt die Kamera
tief/unter dem Boden, zwei Indys sichtbar, Spitzen. Vermutung: Spiel-/Physik-/Kamera-Code (CPU) läuft falsch,
z. B. Indy fällt durch den Boden. Ein Spielbild (`tools/dl_summary.ps1` auf `dl_<n>.txt`): Welt mit einer Matrix
und vielen Portal-Scissors (`ED`), Offset 0x128 = (0x52, 0x1C1, 0x28); Figuren mit `MTX` + `VTXGENMTX`/`GEN:18`
(Matrizen plausibel). Außerdem ist unser Bild nur ~400 Pixel breit statt volle Breite (VI xScale 0x280).

**Statische Prüfung Kamera/Physik (ohne Lauf):** Die FR-Umschalt-Funktionen `FUN_40021f50`/`FUN_4002200c`
(Vektor × Matrix im 64-Bit-FPU-Modus) sind korrekt übersetzt; kein `ctc1` (Rundungsmodus) im Spiel; `cvt.w.s` per
`lrintf` korrekt; Skript-VM-Patch (`jr t6/t7` → `jr ra`) ist gleichwertig, weil die VM per `jalr t6, …` ruft.
**Hintergrund-Tests ohne Fenster im Vordergrund:** `tools/test_bg.ps1 -Name x -Seconds s -Every s -Input "<s>:<Taste>[/ms],..."`
(Tasten: A B Z START L R CU CD CL CR DU DD DL DR, Stick UP DOWN LEFT RIGHT; Zeit ab erster Eingabeabfrage).
Schalter im Programm: `INDY64_BACKGROUND` (Fenster außerhalb des Desktops, ohne Fokus), `INDY64_AUTOSTART`
(„Start Game“ automatisch), `INDY64_AUTOINPUT`. Screenshots dann per `PrintWindow`. Noch nicht ausprobiert.

**Kamera-Messung (Hintergrundlauf bg7, `INDY64_CAMLOG=1`, Log in `indy64recomp/camlog_bg7.txt`):** Erste Matrix
jedes 30. Bildes. Levelflug normal; ab Spielbeginn konstante Verschiebung (0, −8,66, 1,45, 2,33), aber die
Drehung ändert sich ohne jede Eingabe gleichmäßig weiter (Gieren), und die Kamera blickt nach oben (y-Spalte
(0, −0,169, 0,045, 0,045)). Echte Eingabe ist nachweislich null (`[input]`-Log leer). → Fehler in Kamera-/Spiellogik
der CPU (Phantom-Drehung bzw. Zielwinkel wird nie erreicht). Nächster Schritt: Kamera-Funktionen finden (über
OpenJones3D `sithCamera`/`jonesCamera` als Vorlage, Schreib-Überwachung auf die Kamera-Struktur) und vergleichen.

**Kamera-Spur, Funktionen (Ghidra):** `FUN_4000abf4` = Figuren-Hierarchie (Knochen, rekursiv), ruft
`FUN_4000aedc` (3×4-Matrizen `FUN_40021d2c`/`FUN_400220fc` × Projektionsfaktoren 0x40056BA0/BB4/BC8/BCC → `FUN_40021a84`
in den Pool `DAT_400542d0`). `FUN_40011dfc` (Objekte, Pool `DAT_40050e08`), `FUN_40018a3c` = Quaternion dreht Vektor.
Die eigentliche Kameralogik liegt vermutlich im VM-Fenster 0x41xxxxxx (portierte Jones3D-Engine) — noch nicht gefunden.
Sinnvoll wäre eine Referenz aus einem laufenden Emulator (Project64 4.0 dev hängt bei Grafik-Auftrag 377 in der
Audio-Emulation; Alternativen: Project64 3.0.1 stabil, mupen64plus, BizHawk mit Lua).

**GELÖST (2026-10-31): „Kamera falsch“, zwei Indys, Spitzen, Kaktus-Ketten = RT64-Zwischenbilder.** Die
Kamera und die Spiellogik waren richtig. Vergleich mit BizHawk/Ares64 (volle 8-MiB-Auszüge, `indy_dump8.lua`):
Der Matrix-Pool des Emulators liegt bei 0x637000 (bei uns 0x700000, anderes Heap-Layout wegen Demand-Paging im
Original), gefunden mit `tools/find_matrices.py`. Welt-, Indy- und Sprite-Matrizen stimmen bis auf ~1 % überein
(`tools/dl_matrices.py` auf `dl_<n>.txt` + `ram_<n>.bin`). Ursache war die Einstellung „Framerate: Display“: RT64
rechnet Zwischenbilder und ordnet dafür die Transformationen von Bild zu Bild über die Matrix-Adresse zu. Das Spiel
vergibt seinen Pool aber jedes Bild neu, also wurden fremde Objekte ineinander interpoliert. Mit „Original“ ist das
Spielbild wie im Emulator. Fix: `rt64_render_context.cpp` erzwingt `RefreshRate::Original`, Voreinstellung im Menü
ebenfalls „Original“. 60 fps bräuchten später Matrix-Gruppen-IDs (RT64-Extended-GBI) pro Objekt.
Falsche Spuren unterwegs: Framebuffer-Rückschreibungen (alle ≥ 0x64A000, berühren keine Spieldaten); Feld +0x7C im
Indy-Objekt 0x1E86E0 (nur Absprungpunkt eines A-Sprungs im Emulator-Lauf); Stack-Werte bei 0xB0618.
Neue Hilfen: `INDY64_POLL=<phys>,<size>` (`poll.log`, Speicher-Änderungen alle 2 ms), `INDY64_NOFLIP` (Y-Spiegelung
aus), Ausreißer-Log `INDY64_ODD_FROM=<list>` jetzt nur für genau diese Liste und auch für übergroße Dreiecke.
BizHawk-Lua: Ares64 meldet keine Speicher-Callbacks (`event.onmemoryexecute` ohne Wirkung), Mupen64Plus ebenso.
**GELÖST (2026-10-31): Bild zu schmal.** RT64 nahm quadratische Pixel an (400×448 → 0,89:1) und ignorierte die
horizontale Streckung des VI. Neu: `VI::pixelAspect()` = xScaleFloat × (Zeilen pro VI-Zeile) / (2 × yScaleFloat)
(1 für 320×240/640×480, 1,6 im Spiel 400×448, 1,43 für 448er-Menüs, 1,25 im 512er-Modus mit doppelter VI-Breite 1024).
Genutzt in `rt64_workload_queue.cpp` (Seitenverhältnis der Quelle) und `rt64_vi_renderer.cpp` (Bild beim Einpassen
ins Fenster horizontal strecken). Seitenverhältnis fest auf „Original“ (`rt64_render_context.cpp`, Menü-Voreinstellung
ebenso): „Expand“ verbreitert die Projektion, die Portal-Engine zeichnet aber nur den 4:3-Ausschnitt (Löcher, fehlende
Wände am Rand). Ergebnis: Spiel und Menü ~1,44:1 (448 bzw. 386 sichtbare Zeilen, wie auf dem Fernseher mit Rand oben/unten),
Zeitungs-Intro ~1,65:1. Breitbild wäre ein eigenes Projekt (Spiel-Patches für Sichtfeld/Portal-Clipping).

**Andere Factor-5-Recomps:** github.com/MikeSemicolonD/RogueSquadron64Recomp (Rogue Squadron, laut README spielbar,
GPL-3.0, N64Recomp + eigene Forks von N64ModernRuntime und RT64). Anderer F5-Grafik-Microcode (F3DFACTOR5) als Indy
(F5Indi), daher Grafik nicht direkt übertragbar. Nützlich: MusyX-Audio-Microcode per RSPRecomp mit Liste
`extra_indirect_branch_targets` (Sprünge über Tabellen); unser `f5_audio.rsp.toml` hat keine solche Liste — prüfen,
ob Indys MusyX-Version dieselben Ziele braucht (bisher keine „Unhandled jump“-Meldung gesehen, stdout ist aber
gepuffert). Doku dort: docs/reference/*.md (MusyX-Stimmen-Mischung, Debug-Werkzeuge). Kopien in D:\Tools\RogueSquadronRef.

**Ideen für später (2026-10-30):**
- RT64-Aufwertungen: höhere Auflösung, Kantenglättung, Zwischenbilder (60 fps; bei F5 schwieriger, Matrizen werden
  direkt gesetzt), Breitbild (braucht Spiel-Patches), hochauflösende Texturpakete. Die ROM selbst hat keine
  besseren Texturen oder Samples.
- N64-Beleuchtung in die PC-Version bringen: realistisch nur auf Basis von OpenJones3D
  (github.com/smlu/OpenJones3D, C-Nachbau der Jones3D-Engine, ~87 %, Licht/Renderer `rdroid` vollständig, braucht
  `Indy3D.exe` v1.0, Direct3D 6, keine Lizenzangabe). Hürden: andere Lichtdaten/Level, alter Renderer. Eigenes Projekt.
- OpenJones3D als Nachschlagewerk für dieses Projekt: Die N64-Version ist vermutlich ein Port derselben
  Sith/Jones3D-Engine; die Skript-VM `FUN_41029c94` (53 Befehle) sieht nach dem COG-System aus (ungeprüft).

**Referenz-Emulator:** Project64 4.0 in `D:\Tools\Project64` mit Skript `Scripts\indy_dump.js` (Speicherauszüge,
Schreib-Überwachung). Wichtig: „Force Interpreter CPU=1“ (sonst hängt das Spiel beim Start, Interrupts gehen
verloren) und GLideN64 als Grafik-Plugin. Auswertung: `tools/matrix_from_dump.ps1`.

**Frühere Vermutung (Test 38, überholt):** Das Spiel schreibt in seine Matrizen (Pool bei 0x700000) nur drei Zeilen,
die Positionen sind schon relativ zur Kamera. In der vierten Zeile steht, was vorher in diesem Speicher lag: alte
Bildpunkte eines 320×241-Startbildes, das an 0x700000 gerendert wurde (gefunden mit `INDY64_WATCH=700000`, siehe
`crash_log.cpp`). Der Port setzt diese Zeile jetzt auf 0. Danach sinkt der Anteil verworfener Dreiecke von etwa 89 %
auf etwa 37 %, und erste Welt-Geometrie erscheint (weiße Streifen, vermutlich Partikel). Außerdem bekommt die Welt
z/w knapp über 1, deshalb staucht der Port den Tiefenbereich (Viewport z / 1,5). Das ist noch nicht schön, die
Tiefe der Welt ist so praktisch konstant.

Ebenfalls geändert: `events.cpp` meldet „RSP fertig“ erst nach dem Einlesen der Display-Liste. Das war hier nicht
die Ursache, ist aber näher an der echten Hardware.

**Offen:** obiger Test, Logo-Ausblendung sehr langsam (Timing),
Fog/Prim-Depth/Prim-Shading im Port, Debug-Ausgaben reduzieren, Größe von `f5_audio_ucode_data` in `symbols.csv`.

**Lizenz:** GLideN64 steht unter „GPL version 2“ ohne „oder später“. Unser F5-Port ist davon abgeleitet, das
Laufzeitsystem, RecompFrontend und Harvest Moon stehen unter GPL-3.0. Für den privaten Gebrauch ist das unerheblich.
Ein veröffentlichtes Programm bräuchte aber die Erlaubnis der GLideN64-Autoren, um den Port unter GPL-3 zu stellen,
oder eine Neuimplementierung der Befehle.

## N64Recomp-Vorbereitung

- **Vollständige Funktionsliste:** `FillGaps.java` legt in jeder Codelücke eine Funktion an und entfernt Fehlfunde in
  Daten. Damit gibt es **2540 Funktionen** (785 im Boot-Segment, 1755 im VM-Fenster) ohne Überlappungen und ohne Lücken
  mit Code. Die übrigen Lücken sind die Microcode, Null-Padding und `vm_state`. Prüfen lässt sich das mit
  `tools/check_functions.py`. Handgeschriebene Assembler-Funktionen enthalten Konstanten mitten im Code (`1.0f`,
  `0x11111111`). Darum ist die Funktionsgröße die Code-Ausdehnung aus Ghidra und nicht der Abstand zur nächsten Funktion.
- **Symboldatei:** `recomp/indy64.syms.toml` (Modus `symbols_file_path`, ohne ELF), erzeugt mit
  `tools/make_recomp_syms.py`. Abschnitte: `.boot` (ROM `0x1000` → `0x40000400`) und `.vm` (ROM `0x4DC30` → `0x41000000`).
- **Konfiguration:** Die von Hand gepflegte Datei ist `recomp/indy64.recomp.base.toml`. Daraus erzeugt
  `tools/make_recomp_config.py` die Datei `indy64.recomp.toml`. `uses_mips3_float_mode = false`, weil das FR-Bit nicht
  gesetzt wird.
- **Libultra-Abdeckung:** N64Recomp ersetzt 58 der erkannten libultra-Funktionen durch Implementierungen im Laufzeitsystem
  (`reimplemented_funcs`) und übersetzt weitere 120 nicht (`ignored_funcs`). Vergleich mit `tools/recomp_lists.py`.
  Unbekannt sind nur `_bcopy`, `_bzero`, `_bcmp` und `_Printf` samt Hilfsfunktionen. Die sind reiner Code und werden
  normal übersetzt.
- **Offene Punkte für das Laufzeitsystem (N64ModernRuntime):**
  - Adressen `0x40…` und `0x41…` übersetzen. Die Speicher-Makros erwarten kseg0-Adressen (`0x80…`).
  - Den Sprung `entrypoint` → `jr 0x4000044C` → `boot_main` umbiegen.
  - `osPiRawStartDma` und `osMapTLB` stehen auf der Ignorier-Liste. Das Laufzeitsystem muss sie bereitstellen, oder
    `main_thread` wird so gepatcht, dass es das Fenster `0x41000000` einfach als Speicher vorfindet.
  - Grafik: Die Factor-5-Microcode wird von RT64 nicht unterstützt (siehe oben).
- **Build-Umgebung:** VS Build Tools 2022 (MSVC), CMake 4.4, Ninja und LLVM sind vorhanden. git fehlt.

## Kompression

Keine Yay0/Yaz0/MIO0/gzip-Signaturen. Die Daten ab ROM `0x10C250` haben eine Entropie von 7,0–8,0. Das sind entweder
eigene Kompression (Factor 5) oder Audio- bzw. Texturdaten. **Offen.**

## Werkzeuge

- **Symbole:** `symbols/symbols.csv` ist die einzige Quelle für Namen. Eintragen, dann
  `tools/rebuild_ghidra.ps1 -SymbolsOnly`. Ein vollständiger Neuaufbau aus dem ROM geht mit `tools/rebuild_ghidra.ps1`
  (rund 30 s). Aus dieser Datei entsteht später die Symbol-ELF für N64Recomp.
- **Ghidra:** Projekt `analysis/ghidra/indy64.gpr`, Programm `indy_boot.bin`, Sprache `MIPS:BE:64:64-32addr`,
  Compiler `o32`. 2536 Funktionen, davon 173 benannt. Ghidra-Skripte in `tools/ghidra/`:
  - `N64Setup.java`: Speicherlayout und VM-Fenster
  - `ApplySymbols.java`: spielt `symbols.csv` ein
  - `Report.java`: Statistik und fehlende Speicherbereiche
  - `RefsInRange.java`: alle Verweise in einen Adressbereich
- **MCP:** Server `ghidra-indy` in `.mcp.json`. Nach einem Neustart der Sitzung steht er direkt zur Verfügung.
  Ohne Neustart geht es über `tools/ghidra_q.py @abfragen.json` (Beispiele in `analysis/q/`). Das Projekt kann nur
  von einem Prozess gleichzeitig geöffnet sein.
- **Python-Hilfen:**
  - `tools/rom_survey.py`: Header, Strings, Entropie
  - `tools/find_rsp.py`: RSP-Code-Dichte
  - `tools/find_vm_base.py`: ROM-Basis des VM-Fensters
  - `tools/find_ucode_refs.py`: Zeiger in den Microcode-Bereich
  - `tools/find_insn.py`: Konstanten-Ladebefehle
  - `tools/mips_dis.py`: Roh-Disassembler (capstone)
  - `tools/sigmatch.py` und `tools/merge_sigs.py`: libultra-Erkennung

## Nächste Schritte

1. Die libultra-Audio-Bibliothek fehlt, offenbar wurde MusyX statt `n_audio` verwendet. Die Audio-Treiber im
   Boot-Segment müssen von Hand benannt werden.
2. Herausfinden, welche Konfiguration `game_restart` übergibt und ob dabei der Modus „am Stück“ greift.
3. Display-List-Format der Factor-5-Grafik-Microcode aus dem Grafik-Code des Spiels ableiten
   (Aufrufer von `gfx_task_init`).
4. Packformat der Daten ab ROM `0x10C250` untersuchen.
5. Laufzeitsystem: N64ModernRuntime (ultramodern + librecomp) holen und ein Projekt nach dem Muster von Zelda64Recomp
   aufsetzen. Dazu gehören die Adressumsetzung für `0x40…`/`0x41…`, die Funktionen aus der Tabelle oben und ein Patch,
   der `main_thread` ohne TLB auskommen lässt. Dann ein erster Build, zunächst ohne Grafik.
6. Grafik: Der HLE-Port nach RT64 ist geschrieben (siehe oben). Als Nächstes RT64 vollständig bauen, mit allen
   Submodulen, und zusammen mit dem Laufzeitsystem testen. Danach die TODOs angehen: Nebel, Prim-Tiefe, Prim-Shading.
7. `rle_unpack` (`0x410092E0`) ist ein Kandidat für das Packformat der Daten. Seine Aufrufer zeigen, welche Daten so
   gepackt sind.
