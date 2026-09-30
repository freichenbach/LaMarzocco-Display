# Raspberry Pi / Home Assistant: Absturz-Checkliste

Das Shot-Logging (`docs/home-assistant-logging.md`) hängt komplett an diesem
Pi: jeder Absturz reißt die InfluxDB-Schreibvorgänge ab, und ein
fehlgeschlagener Upload wird von der Firmware nicht wiederholt (siehe
"Notes" dort). Diese Checkliste grenzt die Ursache der häufigen Abstürze ein.

Diese Session läuft in einem Cloud-Container ohne Zugriff auf dein Heimnetz
und kann sich nicht selbst per SSH verbinden. Bitte per SSH/Terminal am Pi
oder über die Home-Assistant-Oberfläche selbst durchgehen und mir die
Ausgaben zurückmelden - daraus grenze ich dann die eigentliche Ursache ein.

## 1. Stromversorgung (häufigste Ursache bei Pi + Home Assistant)

```
vcgencmd get_throttled
dmesg | grep -i voltage
```

`get_throttled` liefert eine Bitmaske, hex-codiert:

| Bit | Bedeutung |
|-----|-----------|
| 0 | Undervoltage **jetzt** aktiv |
| 1 | ARM-Frequenz aktuell gedrosselt |
| 2 | Throttling aktuell aktiv |
| 3 | Soft-Temperaturlimit aktuell aktiv |
| 16 | Undervoltage ist **seit dem letzten Boot vorgekommen** |
| 17 | Frequenz-Drosselung ist vorgekommen |
| 18 | Throttling ist vorgekommen |
| 19 | Soft-Temperaturlimit ist vorgekommen |

Alles ungleich `0x0` (z.B. `throttled=0x50000` = Bits 16+18, also Undervoltage
+ Throttling seit dem letzten Boot) ist ein starkes Indiz: schwaches
Netzteil/Kabel, oder zu viele USB-Geräte am selben Bus.

## 2. Thermik

```
vcgencmd measure_temp
```

Dauerhaft nahe 80°C (Throttling-Schwelle) deutet auf fehlende Kühlung hin -
zusammen mit Bit 3/19 oben.

## 3. SD-Karte

HA + die neuen InfluxDB-Schreibvorgänge erzeugen viele kleine, häufige
Schreibzugriffe - eine der Hauptursachen für SD-Karten-Verschleiß und
-Korruption auf Dauer:

```
dmesg | grep -iE 'mmc|i/o error|ext4-fs error|blk_update_request'
```

Jede Zeile hier ist verdächtig. Falls noch nicht geschehen: Umstieg auf
Boot von SSD/USB reduziert dieses Risiko deutlich.

## 4. Arbeitsspeicher / OOM

```
journalctl | grep -i 'killed process\|out of memory'
du -sh /path/to/config/home-assistant_v2.db   # Pfad je nach Installation
```

Eine stark angewachsene Recorder-Datenbank (Standard-SQLite von HA, nicht die
separate InfluxDB) ist ein bekannter Grund für Speicherdruck auf einem Pi mit
wenig RAM. Falls sie mehrere hundert MB/GB groß ist: `recorder:` in
`configuration.yaml` mit `purge_keep_days` und `exclude` einschränken.

## 5. Absturz-Historie

```
journalctl --list-boots
journalctl -b -1 -p err..alert
```

`--list-boots` zeigt, wie oft und wann neu gestartet wurde - daran erkennt
man z.B. ob die Abstürze zeitlich gehäuft auftreten (bestimmte Uhrzeit,
nach bestimmten Aktionen). `-b -1` zeigt die letzte Sitzung vor dem aktuellen
Boot; `-p err..alert` filtert auf Fehler/kritische Meldungen kurz vor dem
Absturz.

**Funktioniert nur, wenn Journald-Logs den Reboot überleben** - siehe Punkt 7.

## 6. Home-Assistant-/Supervisor-Logs

- HA-UI: Settings → System → Logs.
- Falls Home Assistant OS/Supervised: `ha core logs`, `ha supervisor logs`,
  `ha os logs` - sucht nach der Fehlermeldung direkt vor dem letzten Absturz.
- Bei einem Add-on-Crash (statt System-Crash) steht die Ursache meist direkt
  im Log des betroffenen Add-ons.

## 7. Vorsorge für den nächsten Absturz

Falls `journalctl --list-boots` nur den aktuellen Boot zeigt (Logs
überleben den Neustart nicht), das jetzt aktivieren, damit der *nächste*
Absturz überhaupt auswertbar ist:

```
sudo mkdir -p /var/log/journal
sudo sed -i 's/#Storage=auto/Storage=persistent/' /etc/systemd/journald.conf
sudo systemctl restart systemd-journald
```

## Ergebnisse zurückmelden

Am hilfreichsten: die Ausgaben von `vcgencmd get_throttled`,
`journalctl --list-boots` und alles Auffällige aus Punkt 3/4/6 hier
einfügen oder als Datei schicken - daraus lässt sich die Ursache meist schon
eingrenzen, ohne dass ich selbst am Gerät sitzen muss.
