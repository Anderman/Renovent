# Renovent 
De originele controller is een display met toetsen en 1 IC
De IC zet een 3 bit counter om in 8 select lijnen van het display.

Display is een  [liteon](https://eu.mouser.com/datasheet/3/281/1/LTC_4627JS.pdf)
De 8 segementen worden in volgorde aan gestuurd met een freq van 500uS
Welke segmenten verlicht worden gaat met digit 1 - 4

Het programma zal dus het continue het display moeten uitlezen en wat er op staat omzetten in een 4 char array.
Hiervan met gebruik gemaakt worden van mapping om een 8 bit array om te zetten in een Char. Waarschijnlijk kan de DP los en hebben we een 7 bitarray naar char mapping nodig.

De switches werken als volgt

Er is 1 output Key_node.
sw1 (OK) = key_node=1 als C=1 (counter=1)
Sw2 (+ ) = key_node=1 als D=1 (counter=5)
Sw3 (F ) = key_node=1 als E=1 (counter=6)
Sw4 (- ) = key_node=1 als DP=1 (counter=3)

## ESP32 aansluiting

In het ESP32-schema worden de relevante signalen van de originele controller eerst via een SN74LVC245 naar 3V3 gebracht. De ESP32 ziet dus alleen de 3V3-varianten van deze signalen.

### Ingangen voor het uitlezen van display en toetsen

- BIT0 -> BIT0_3V3 -> GPIO35
- BIT1 -> BIT1_3V3 -> GPIO36
- BIT2 -> BIT2_3V3 -> GPIO37
- Digit1 -> Digit1_3V3 -> GPIO38
- Digit2 -> Digit2_3V3 -> GPIO39
- Digit3 -> Digit3_3V3 -> GPIO40
- Digit4 -> Digit4_3V3 -> GPIO41
- KEY_NODE -> KEY_NODE_3V3 -> GPIO42

Deze acht ingangen zijn voldoende om het gemultiplexte display en de toetsstatus te reconstrueren.

### Uitgang voor toets simulatie

- KEY_DOWN <- GPIO43

KEY_DOWN gaat via een BSS138-transistor naar de originele schakeling. Daarmee kan de ESP32 een toetsdruk simuleren zonder direct 5V op een GPIO te zetten.

### Overige aansluitingen op het ESP32-bord

- SDA_1 -> GPIO7
- SCL_1 -> GPIO8
- LED -> GPIO12

## Aanpak in software

De displaycontroller loopt elke 500 us door de 8 segment-selects. Daardoor is het handiger om een complete scan te reconstrueren dan om losse leds te lezen.

Aanbevolen aanpak:

1. Lees BIT0, BIT1 en BIT2 continu in.
2. Gebruik deze 3 bits om te bepalen welk segment actief is: A, C, B, DP, F, D, E, G volgens de mapping in `Docs/mappng.txt`.
3. Lees tegelijk Digit1 tot en met Digit4 in en sla per actieve segment-select op welke digits hoog zijn.
4. Lees tegelijk KEY_NODE in. Als KEY_NODE hoog is tijdens een bepaalde segment-select, dan hoort daar een specifieke toets bij.
5. Zodra alle 8 select-stappen langsgekomen zijn, zet je de verzamelde segmentdata om naar 4 karakters.

## Interrupt of polling

Voor deze hardware is polling meestal de beste keuze.

- Gebruik een snelle taak of timer die bijvoorbeeld elke 50 tot 100 us de 8 ingangen leest.
- Bouw daarmee steeds een volledig displayframe op.
- Blokkeer interrupts niet langdurig.

Waarom geen lange critical sections:

- De ESP32 gebruikt interrupts intern zelf ook voor WiFi, USB en timing.
- Het display is langzaam genoeg om betrouwbaar te samplen zonder harde ISR-logica.
- Polling maakt het eenvoudiger om glitches of overgangsmomenten van de teller te filteren.

Alleen als later blijkt dat polling net niet stabiel genoeg is, kan een interrupt op een vaste synchronisatielijn nuttig zijn. In de huidige opzet is daar waarschijnlijk geen noodzaak voor.

## Huidige softwarebasis

Er is nu een eerste ESP32 software-opzet toegevoegd in de map `src`.

- `src/main.cpp` start WiFi, de webserver en de display reader.
- `src/wifiSetup.*` gebruikt WiFiManager, net als in het andere project.
- `src/display_reader.*` leest BIT0..2, Digit1..4 en KEY_NODE in en bouwt daar een stabiel displayframe van op.
- `src/webui/webui.*` publiceert een simpele HTTP API en serveert statische bestanden uit SPIFFS.
- `data/index.html` is een eenvoudige debugpagina.

### Webinterface

Na boot:

- hoofd-UI op poort 80
- WiFiManager portal op poort 8080

Beschikbare endpoints:

- `GET /api/status`
- `POST /api/keydown`

`/api/status` geeft onder andere terug:

- gedecodeerde displaytekst
- ruwe segmentmaskers per digit
- actieve toets op basis van `KEY_NODE`
- WiFi-status

### PlatformIO

Er is ook een `partitions/default.csv` toegevoegd zodat de SPIFFS data-partitie beschikbaar is voor de webpagina.

Voor lokaal gebruik zijn deze commando's relevant:

1. `platformio run`
2. `platformio run --target buildfs`
3. `platformio run --target uploadfs`
4. `platformio run --target upload`

### Backtrace decoderen

Een ESP32-S3 backtrace uit de WebUI of seriele output kun je direct tegen de actuele firmware-ELF symboliseren met:

```powershell
.\scripts\decode_backtrace.ps1 "0x4037EF71 -> 0x40377C96 -> 0x40376AEF"
```

Het script gebruikt standaard:

- `.pio/build/esp32s3mini_ota/firmware.elf`
- de PlatformIO `xtensa-esp32s3-elf-addr2line.exe`

Belangrijk: decode altijd tegen de ELF van dezelfde firmware-build als waarmee de crash is ontstaan, anders kloppen functies en regelnummers mogelijk niet.