# Babylon Infernalis

Dieses Projekt habe ich unter großer Mithilfe von Claude Code realisiert.

Ein nativer Windows-Port von **Indiana Jones and the Infernal Machine** für das Nintendo 64, erstellt mit
[N64: Recompiled](https://github.com/N64Recomp/N64Recomp). Der MIPS-Code des Spiels wird statisch nach C übersetzt
und läuft direkt auf dem PC; die Grafik zeichnet [RT64](https://github.com/rt64/rt64).

*English version: [README.md](README.md)*

> **Es sind keine Spieldaten enthalten.** Dieses Repository enthält nur den Code, die Konfiguration und die Werkzeuge
> des Ports. Zum Bauen und Spielen brauchst du deine eigene ROM der US-Fassung (siehe [BUILDING.md](BUILDING.md)).
> Bitte nicht nach ROMs oder fertigen Programmdateien fragen.

Dies ist ein inoffizielles Fan-Projekt ohne Verbindung zu Lucasfilm, LucasArts, Disney, Factor 5 oder Nintendo.
„Indiana Jones“ ist eine Marke von Lucasfilm Ltd.

## Stand: v0.1.0 (Vorabversion)

Das Spiel startet und läuft mit Musik, Effekten und Sprache. Das erste Level (Canyonlands) wurde ausgiebig von Hand
gespielt; alle 16 Level wurden in einem automatischen Test ohne Absturz geladen. Das Spiel wurde noch **nicht**
komplett durchgespielt. Was geht, was gemacht wurde und welche Fehler bekannt sind, steht in
[CHANGELOG.md](CHANGELOG.md).

- Windows 10/11, x64, Direct3D 12 oder Vulkan.
- Getestet mit dem **Nintendo Switch Online N64 Controller** (Bluetooth), für den eine eigene Tastenbelegung
  eingebaut ist. Andere Controller und die Tastatur nutzen die Standardbelegung und lassen sich unter *Controls*
  umbelegen.
- Nur 4:3 und die originale Bildrate (siehe bekannte Fehler).

## Fehlerberichte

Während des Spielens **F9** drücken (oder die *Capture*-Taste am Switch-Online-N64-Controller). Das Programm legt
neben der exe einen Bericht in `bugreports\<Datum_Uhrzeit>\` an: Bildschirmfoto, Speicherabzug, Displayliste,
Sitzungsprotokoll und eine `notiz.txt` für eine kurze Beschreibung. Jede Sitzung schreibt außerdem ein Protokoll
nach `logs\`.

## Weiteres

Aufbau des Repositorys, Danksagungen und Lizenz: siehe [README.md](README.md). Die ausführlichen
Entwicklungsnotizen (Speicheraufbau, Microcode-Analyse, jeder Fehler und seine Ursache) stehen in
[NOTES.md](NOTES.md).
