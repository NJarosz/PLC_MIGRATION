# PLC Migration — Hardware Design Notes

Living reference for converting the breadboard prototype to production-ready hardware.
Update this file as decisions are made or new considerations arise.

**Button & LED cheatsheet:** https://claude.ai/artifact/3kWwCnVDfKDpNK5DDHyxVJ

---

## Decisions Made

| Topic | Decision | Notes |
|-------|----------|-------|
| Load voltage | 120VAC load switching | Relay contact side switches mains; coil side stays low-voltage |
| Control voltage | 5VDC (relay coils) / 3.3V (logic) | Same topology as existing Pi + relay module setup |
| Relay channels | 8 | Designed for 8 from the start; workbench uses 8-ch blue module |
| Output connector | Aviation-style 9-pin | 4 switched outputs + commons; exact pinout TBD |
| Load type | Assume inductive (worst case) | RC snubbers across contacts required |
| RFID | 13.56 MHz MIFARE | Uses existing cards already in production |
| Cable runs | As short as practical | Short runs eliminate most shielding/noise concerns |
| PCB approach | Two-board split (main + HMI) | Main board in enclosure; HMI board on door; 20-pin IDC ribbon cable |
| MCU on PCB | Module approach (WeAct STM32 + ESP32-WROOM-32) | Avoids bare-chip complexity on first board spin |
| PCB design tool | KiCad | User already has it installed |
| Safety relay | Simple NC E-stop in series with load rail | Skip Pilz for first build; add SIL relay when productizing |
| Manual mode switches | Panel toggle switches (4 or 8) | One per relay; pendant connector footprint reserved for future |
| Relay channels | 8, fully routed | All GPIO allocated and defined in firmware |
| IP rating | IP65 | Shop floor coolant/dust protection |
| 120VAC on PCB | Off the board for first spin | Relay common rail distributed externally via terminal blocks; bring on-board in future revision once rest of design is validated |
| Relay type (PCB v1) | Blue relay module, off-board | Module mounts on standoffs directly to the main PCB (matching M3 holes); 10-pin ribbon from main PCB header to module. Clean single-unit assembly. |
| Relay type (PCB v2) | Omron G5LE-1-E, on-board | Relays move onto the main PCB with proper PCB slots, creepage/clearance distances, and ULN2803A driver. One wire from common rail to a screw terminal; traces distribute to all 8 relay COMs. |

---

## Development Sequence

Starting with the HMI board: all low-voltage signals, simpler layout, good KiCad warmup, and immediately useful — plugs into the existing Nucleo workbench setup via 20-pin ribbon while main board design continues in parallel.

| Phase | Task | Status |
|-------|------|--------|
| 0 | Workbench validation — wire real production-intent parts to Nucleo, run firmware through them | Not started |
| 1 | Learn KiCad — 30-min tutorial, get familiar with schematic entry and footprint assignment | Not started |
| 2 | HMI board schematic — ribbon connector, LCD, RFID header, buttons, E-stop connector, pilot LEDs | Not started |
| 3 | HMI board PCB layout — straightforward, all low-voltage; good first layout exercise | Not started |
| 4 | Order HMI board prototype from JLCPCB (5 boards, ~$15 shipped) | Not started |
| 5 | Assemble HMI board, connect to Nucleo via 20-pin ribbon, validate all panel functions | Not started |
| 6 | Main board schematic — power section → STM32/ESP32 → ULN2803A → relay module header → connectors | Not started |
| 7 | Main board PCB layout — relay module M3 mounting holes, relay driver, power rails, SWD/debug headers | Not started |
| 8 | Order main board prototype from JLCPCB | Not started |
| 9 | Assemble main board, mount relay module on standoffs, connect HMI board ribbon | Not started |
| 10 | Full system integration test with enclosure | Not started |
| 11 | PCB v2 — integrate Omron G5LE relays on-board with proper PCB slots and clearances | Not started |

### Workbench Validation Parts to Order
- 8-channel blue relay module (active-low) — already on hand
- 40mm NC mushroom E-stop button (simple two-contact; skip Pilz for workbench validation)
- GX16-9 aviation connector pair (panel mount + cable plug)
- RC522 SPI RFID reader module (same type already in use in firmware)
- Mean Well RS-15-5 PSU (120VAC in, 5VDC out)
- GX16-10 aviation connector pair for pendant (8 signal wires + 3.3V + GND)

---

## Current Prototype Pinout (STM32 Nucleo-F446RE)

Verified against PLC_MIGRATION.ioc and main.h — use this for PCB layout.

| Signal             | MCU Pin | Peripheral   | Notes                                              |
|--------------------|---------|--------------|-----------------------------------------------------|
| E-Stop             | PB4     | GPIO_Input   | NC contact: 3.3V → contact → PB4, internal pull-down. Pin HIGH = normal, LOW = E-stop asserted. Fail-safe: wire break pulls LOW. |
| RUN button         | PB0     | GPIO_Input   | Active-low, internal pull-up. Panel-level cycle-start / bench fallback. Production cycle-start uses BTN_CYC_L/R/3. |
| FETCH button       | PB1     | GPIO_Input   | Active-low, internal pull-up                       |
| ACK button         | PB10    | GPIO_Input   | Active-low, internal pull-up                       |
| Relay 1            | PA0     | GPIO_Output  | Active-high into ULN2803A (ULN inverts → relay module IN active-low) |
| Relay 2            | PA1     | GPIO_Output  | Active-high into ULN2803A                         |
| Relay 3            | PA4     | GPIO_Output  | Active-high into ULN2803A                         |
| Relay 4            | PA5     | GPIO_Output  | Active-high into ULN2803A; shares Nucleo LD2 LED  |
| Relay 5            | PA6     | GPIO_Output  | Active-high into ULN2803A                         |
| Relay 6            | PA7     | GPIO_Output  | Active-high into ULN2803A                         |
| Relay 7            | PA8     | GPIO_Output  | Active-high into ULN2803A                         |
| Relay 8            | PA11    | GPIO_Output  | Active-high into ULN2803A                         |
| LCD SCL            | PB8     | I2C1_SCL     | 4.7kΩ pull-up to 3.3V                             |
| LCD SDA            | PB9     | I2C1_SDA     | 4.7kΩ pull-up to 3.3V                             |
| RFID CS            | PB12    | GPIO_Output  | Software chip-select for RC522, active-low         |
| RFID SCK           | PB13    | SPI2_SCK     | RC522 clock                                        |
| RFID MOSI          | PC1     | SPI2_MOSI    | RC522 data in                                      |
| RFID MISO          | PC2     | SPI2_MISO    | RC522 data out                                     |
| RFID RST           | PA12    | GPIO_Output  | RC522 hardware reset, active-low                   |
| ESP32 TX→MCU       | PA10    | USART1_RX    | 3.3V logic; supervisor_comms uses huart1           |
| ESP32 RX←MCU       | PA9     | USART1_TX    | 3.3V logic                                        |
| Debug UART TX      | PA2     | USART2_TX    | ST-Link virtual COM (printf output)                |
| Debug UART RX      | PA3     | USART2_RX    | ST-Link virtual COM                                |
| LED Armed          | PB5     | GPIO_Output  | Active-high; drives ribbon pin 17 → HMI board LED |
| LED Run            | PB7     | GPIO_Output  | Active-high; drives ribbon pin 15 → HMI board LED |
| LED Fault          | PB6     | GPIO_Output  | Active-high; drives ribbon pin 13 → HMI board LED |
| Cycle BTN Left     | PB14    | GPIO_Input   | Active-low, pull-up; hand button connector H-Button1 |
| Cycle BTN Right    | PB15    | GPIO_Input   | Active-low, pull-up; hand button connector H-Button1 |
| Cycle BTN 3        | PC11    | GPIO_Input   | Active-low, pull-up; hand button connector H-Button1 |
| Manual SW 1–8      | PC3–PC10 | GPIO_Input  | Active-high, pull-down; pendant connector. Read: (GPIOC->IDR >> 3) & 0xFF |

### Required .ioc update before bench testing

Most GPIO is now self-initialized in firmware (`Outputs_Init`, `Inputs_Init`) and does not depend on CubeMX-generated code. The one exception is **RFID_RST**, which the MFRC522 driver assumes was initialized by CubeMX. Before bench testing RFID, update the .ioc:

- Reassign RFID_RST from **PC8 → PA12**

While in the .ioc, also update the relay pins (all move to Port A) and manual switch pins (shift to PC3–PC10) to keep the .ioc in sync. Regenerate code after changes.

---

## Two-Board Architecture

### Concept
Split the design into a **main control board** (inside the enclosure, handles power and logic) and an **HMI board** (mounts to the enclosure door, handles everything the operator touches). Connected by a 16-pin IDC ribbon cable — the same type used in old PC drives. This keeps high-voltage wiring away from the operator interface and makes the door assembly modular.

```
┌─────────────────────────────────────────────────────────┐
│                     ENCLOSURE INTERIOR                  │
│                                                         │
│  ┌──────────────────────────────┐                       │
│  │       MAIN BOARD             │                       │
│  │  STM32F446 + ESP32-WROOM     │                       │
│  │  ULN2803A relay drivers      │  ←── 5V PSU            │
│  │  Relay connectors (4ch)      │  ←── 120VAC mains in  │
│  │  3.3V / 5V regulators        │  ───→ 120VAC out       │
│  │  Terminal block connectors   │      (via aviation     │
│  └──────┬───────────────────────┘       connector)      │
│         │ 20-pin IDC ribbon cable (J1)                  │
└─────────┼───────────────────────────────────────────────┘
          │  (through door hinge/gland)
┌─────────┼───────────────────────────────────────────────┐
│  DOOR   │                                               │
│  ┌──────┴───────────────────────┐                       │
│  │       HMI BOARD              │                       │
│  │  16×2 LCD with I2C backpack  │                       │
│  │  4× button connectors        │                       │
│  │  RFID reader module          │                       │
│  │  E-Stop connector            │                       │
│  └──────────────────────────────┘                       │
└─────────────────────────────────────────────────────────┘
```

### Ribbon Cable Signal Allocation (20-pin IDC)

E-stop connects directly to the main PCB via its own wiring (not through ribbon).
Hand button (BTN_CYC) signals pass through the ribbon from H-Button1 on the HMI board to the MCU.

This table reflects the **actual PCB J1 pinout** (IDC-Header_2x10_P2.54mm_Latch_Horizontal, B.Cu).
The odd/even numbering matches the IDC connector's physical rows: odd pins (1,3,5…19) on one row, even pins (2,4,6…20) on the other.

| Pin | Signal      | Net label   | MCU Pin | Notes                         |
|-----|-------------|-------------|---------|-------------------------------|
| 1   | GND         | GND         | —       | Ground                        |
| 2   | SPI MISO    | MISO        | PC2     | RFID RC522                    |
| 3   | RFID RST    | RFID_RST    | PC8     | RFID reset                    |
| 4   | SPI MOSI    | MOSI        | PC1     | RFID RC522                    |
| 5   | 3.3V        | 3V3         | —       | Power to HMI board            |
| 6   | SPI SCK     | SCK         | PB13    | RFID RC522                    |
| 7   | I2C SCL     | SCL         | PB8     | LCD                           |
| 8   | RFID CS     | SS          | PB12    | RFID chip select              |
| 9   | I2C SDA     | SDA         | PB9     | LCD                           |
| 10  | GND         | GND         | —       | Ground                        |
| 11  | 5V          | +5V         | —       | 5V to HMI board (LCD VCC); repurposed from redundant GND |
| 12  | BTN CYC 3   | BTN_CYC_3   | PC11    | Hand button 3rd, active-low   |
| 13  | LED FAULT   | LED_FAULT   | PB6     | PCB-mount LED drive (red)     |
| 14  | BTN CYC R   | BTN_CYC_R   | PB15    | Hand button right, active-low |
| 15  | LED RUN     | LED_RUN     | PB7     | PCB-mount LED drive (green)   |
| 16  | BTN CYC L   | BTN_CYC_L   | PB14    | Hand button left, active-low  |
| 17  | LED ARMED   | LED_ARMED   | PB5     | PCB-mount LED drive (yellow)  |
| 18  | Run button  | RUN         | PB0     | Run button, active-low        |
| 19  | Fetch button| FETCH       | PB1     | Fetch/log-off, active-low     |
| 20  | BTN ACK     | ACK         | PB10    | Acknowledge, active-low       |

---

## Power and Voltage Architecture

### The Key Concept
A relay has two completely separate, electrically isolated sides. The coil side is powered by low voltage (5V in this design) and controlled by the STM32. The contact side switches whatever load is connected — 120VAC in this case. The two sides never touch electrically. This is the same topology as the existing Pi + relay module setup; only the thing driving the coil changes (STM32 instead of Pi GPIO).

```
120VAC MAINS ─────────────────────────────────────────────────────────┐
      │                                                                │
      │  ┌──────────────────────────┐                                  │
      └──│   5V PSU (120VAC in)     │                                  │
         │  (Mean Well RS-15-5)     │                                  │
         └────────┬─────────────────┘                                  │
                  │ 5V                                                  │
         ┌────────┴───────────────────────┐                            │
         │                               │                             │
  ┌──────┴──────┐              ┌─────────┴────────┐                   │
  │ AMS1117-3.3 │              │  4-ch relay module│                   │
  └──────┬───────┘              │  coil side (5V)  │                   │
         │ 3.3V                 └────────┬──────────┘                  │
  STM32 + ESP32                 Relay contacts ──────────────────────┘
  LCD + RFID                          │
                               Aviation connector → Machine
```

### Power Supply — Simplified
Only two rails needed: 5V for relay coils and as input to the 3.3V regulator, and 3.3V for all logic.

| Rail  | Source                          | Consumers                          |
|-------|---------------------------------|------------------------------------|
| 5V    | AC-DC PSU (120VAC in, 5V out)   | Relay module coils, AMS1117 input  |
| 3.3V  | AMS1117-3.3 LDO from 5V        | STM32, ESP32, LCD, RFID reader     |

### Component Recommendations

| Component | Part | Notes |
|-----------|------|-------|
| Main PSU | Mean Well RS-15-5 | 120VAC in, 5VDC 3A out — panel mount |
| 3.3V LDO | AMS1117-3.3 | After 5V rail; simple, inexpensive |
| Relay driver (production) | ULN2803A (8-ch Darlington array) | **Active-high from MCU** — STM32 writes GPIO_SET to energize. Built-in flyback diodes on COM pin (connect to 5V relay rail). One IC drives all 8 relay coils. |
| Relay (production) | Omron G5LE-1-E (SPDT, 5V coil, 10A contacts) | PCB-mount. One per channel. KiCad footprint available. |
| Relay module (prototype/workbench only) | Generic 4-ch optocoupler module | **Active-low** — GPIO_RESET energizes. Firmware currently written for this. Polarity must be inverted in `Outputs_Apply` when moving to production PCB. |

---

## Relay Output Circuit

### Load Type — Inductive Loads (Assumed Worst Case)
When a relay contact opens while driving an inductive load (motor, solenoid, pneumatic valve coil), the collapsing magnetic field creates a voltage spike across the contacts. This causes arcing, which erodes the contacts over time.

**Solution: RC snubber across each relay contact pair**
- R = 100Ω, C = 0.1 µF (100 nF), rated for 250VAC
- Placed directly across the relay output contacts (i.e., on the load side, not the coil side)
- Absorbs the spike, prevents arcing
- Coil side already protected by ULN2803A built-in flyback diodes (COM pin tied to 5V relay rail)

```
Relay contact ──┬── (to load)
                │
               [R 100Ω]
                │
               [C 0.1µF 250VAC]
                │
Relay common  ──┘
```

### Output Connector — Aviation Style 9-Pin (GX20-9)
GX20-9 circular connector (20mm body), rated 10A per contact — use GX20 not GX16 here, GX16 is only rated 5A which is marginal for mains switching. Pre-wired pigtails available on Amazon/AliExpress.

8 relay outputs + 1 shared neutral = 9 pins exactly.

| Pin | Signal       |
|-----|-------------|
| 1   | Relay 1 NO  |
| 2   | Relay 2 NO  |
| 3   | Relay 3 NO  |
| 4   | Relay 4 NO  |
| 5   | Relay 5 NO  |
| 6   | Relay 6 NO  |
| 7   | Relay 7 NO  |
| 8   | Relay 8 NO  |
| 9   | Shared neutral (N return) |
| 9   | Spare       |

> NC (normally closed) contacts available on the relay but not brought out unless a specific application needs them.

---

## E-Stop Safety Circuit

### Design Goal
E-stop must cut 120VAC power to all loads independently of firmware — a hardware failsafe. The STM32 E-stop input is a secondary layer for logging and state machine purposes only.

### Why a Safety Relay?
A standard relay coil + contact is not self-checking. If the output relay contacts weld shut (from a large inductive spike), the firmware sees the relay as "off" but the output is still live. A safety relay module uses force-guided contacts — the main contacts and monitoring contacts are mechanically linked, so if the main contacts weld, the monitoring circuit physically cannot indicate "safe." The module refuses to reset until the fault is cleared. This is what makes a machine safe to be near.

For this design: **Pilz PNOZ X1 (24VDC, 2-channel)** — ~$80, widely stocked, simple to wire, proven in field.

### Safety Relay Wiring
```
120VAC L ──────────────────────────────────── Load common rail
                                                     │
24VDC ──[E-Stop NC contact CH1]──[E-Stop NC CH2]── PNOZ A1 coil ── GND
                  ↑
         Two NC contacts from the same E-stop
         button (dual-channel monitoring)

PNOZ safety output (13-14 NO) ──── in series with 120VAC L to relay commons
                                    (cuts all load power on E-stop)

PNOZ monitoring output (21-22 NC) ──── STM32 PB4 (E-stop GPIO input)
                                         (tells MCU safety relay is open)
```

### E-Stop Button Spec
- 40mm red mushroom head, twist-release, IP65
- **Dual NC contacts** (required for two-channel safety relay input)
- IEC 60947-5-5 compliant
- Schneider XB5-AS8445 or Eaton M22-PVT45P-R or equivalent

---

## RFID Reader

### Selected: 13.56 MHz MIFARE, SPI (RC522), 3.3V logic
- Firmware uses the RC522 driver over SPI2 (PB13 SCK, PC1 MOSI, PC2 MISO, PB12 CS, PC8 RST)
- Compatible with existing MIFARE employee cards already in production
- The common blue RC522 module (Amazon, ~$2) is what the firmware is written against

### Mounting
- Panel cutout on HMI board or door — recessed is better than surface-mount for durability
- Keep away from metal frame edges (metal detuning reduces read range)
- Built-in LED/buzzer feedback preferred for operator confirmation

### Cable / HMI Board
- RC522 on HMI board requires 5 signals through the ribbon cable (MOSI, MISO, SCK, CS, RST)
- Ribbon cable is 20-pin IDC (J1 on HMI board); all signals fit with room for 3 GNDs and 3V3
- Distance is door thickness + short ribbon run; SPI at low speed is fine at this length

---

## HMI Board — Panel Components

| Component | Spec | Connector on HMI PCB | Notes |
|-----------|------|----------------------|-------|
| PB1 Fetch/Logoff | 22mm yellow, momentary, IP65 | I-Button1 (pins 2 + GND) | MENU |
| ACK | 22mm blue, momentary, IP65 | I-Button1 (pins 3 + GND) | ACK |
| PB0 Run | 22mm green, momentary, IP65 | I-Button1 (pins 5 + GND) | Panel-level cycle start / bench fallback |
| LCD | 16×2 HD44780 + PCF8574 I2C | LCD1 (4-pin: SCL/SDA/3V3/GND) | Upper panel area |
| RFID Reader | RC522 SPI module | RFID1 (8-pin header) | Beneath LCD |
| LED Armed | PCB-mount LED, yellow (D1) | soldered to PCB | Right of LCD, vertical stack |
| LED Running | PCB-mount LED, green (D2) | soldered to PCB | Right of LCD, vertical stack |
| LED Fault | PCB-mount LED, red (D3) | soldered to PCB | Right of LCD, vertical stack |
| Hand buttons | External station, momentary | H-Button1 (4-pin: GND/CYC_L/CYC_R/CYC_3) | Pendant-style cable |

**Not on this panel:**
- **E-stop** — mounts separately on top of enclosure or near operator position; wires directly to main PCB, not through ribbon
- **PB0 (Run/Cycle Start)** — machine-mounted, connects directly to main PCB via the 4-pin hand button connector

### Panel Layout (rough)

```
┌─────────────────────────────────────┐
│                                     │
│  [PB1]  ┌──────────┐  [●] Armed    │
│         │  16×2    │  [●] Running  │
│  [ACK]  │  LCD     │  [●] Fault    │
│         └──────────┘               │
│  [PB0]  ┌──────────┐               │
│         │  RFID    │               │
│         │ (hidden) │               │
│         └──────────┘               │
└─────────────────────────────────────┘
         HMI PCB: 80 × 80 mm
```

### Component Mounting (3D Assembly)

All panel components attach to the panel face itself; the HMI PCB floats behind on its own standoffs and connects to each via short wire harnesses. Nothing about PCB position is constrained by panel component position.

| Component | Panel interface | How it mounts | PCB connection |
|-----------|----------------|---------------|----------------|
| LCD | Rectangular cutout | 4× standoffs through panel; LCD clamps against inside face | 4-pin JST cable to HMI PCB |
| PB1, ACK | 22mm round holes | Threaded barrel through panel; lock nut from inside | 2-pin wire harness per button |
| LEDs × 3 | Small round holes + light pipes | Light pipe clips onto PCB LED, pokes through panel hole | Soldered to HMI PCB |
| RFID (RC522) | No cutout | Standoffs hold module flush against inside of panel face, antenna toward panel | 5-pin cable to HMI PCB header |

> **RFID zone must be non-metallic.** 13.56 MHz passes through plastic, fiberglass, and wood but is blocked by aluminum or steel. If the enclosure door is metal, fit a small ABS or polycarbonate insert in the RFID area. Read range drops dramatically without this.
>
> PCB-mount LEDs — light pipes route light from PCB to panel surface. Exact pipe length determined at enclosure design time; LEDs are functional on the bench without them.

---

## Regulatory / Compliance (Future Consideration)
- **UL 508A** (Industrial Control Panels) — if selling or deploying commercially in North America. Requires listed components, specific wire gauges, terminal marking, etc.
- **NEC Article 409** — US electrical code for industrial control panels
- **ISO 13849 / Performance Level d** — for E-stop safety function; PNOZ X1 is rated PLe Cat. 4, more than sufficient
- **EMC**: Line filter on 120VAC input (common-mode choke + X/Y capacitors); keeps relay switching noise off the mains and away from ESP32 WiFi

---

## Programming / Debug Access (Production Board)

The Nucleo dev board has a built-in ST-Link that handles USB programming and virtual COM. A production PCB needs these capabilities designed in explicitly.

### STM32 Firmware Updates
- **Primary: SWD header** — expose SWDIO (PA13), SWCLK (PA14), GND, 3.3V on a 4-pin 0.1" header or Tag-Connect pads. Use an external ST-Link V2 dongle (~$10) with STM32CubeIDE. Workflow is identical to Nucleo.
- **Backup: UART bootloader** — every STM32 has a ROM bootloader activated by pulling BOOT0 high at reset. Expose BOOT0, RESET, and USART2 pins on a header. Any USB-to-UART adapter + STM32CubeProgrammer can flash firmware without ST-Link.

### Debug Output Access
- Put a **CH340N USB-to-UART IC** on the main board wired to USART2 (PA2/PA3). Exposes a USB port (USB-C recommended) on the enclosure. Plug in a laptop → serial terminal → see all `printf` debug output. Also serves as the UART bootloader path, so one connector covers both debug and emergency reflash.

### ESP32 Firmware Updates
- Expose a **6-pin header**: GND, 3.3V, TXD0, RXD0, IO0 (boot mode), EN (reset). USB-to-UART adapter + `esptool.py` reflashes the ESP32 without disassembly.

### Future: OTA Updates via ESP32
- Wire two spare ESP32 GPIOs to STM32 BOOT0 and RESET on the schematic. Cost: two traces and two resistors. Enables ESP32 to put STM32 into bootloader mode and push a new firmware binary received over WiFi from the Pi. Not implemented yet — just wire for it.

### Summary

| Scenario | How to update |
|----------|--------------|
| Normal STM32 update | ST-Link V2 dongle → SWD header → STM32CubeIDE |
| Normal ESP32 update | USB-UART adapter → 6-pin header → esptool.py |
| Debug output | USB-C on board → any serial terminal |
| No ST-Link available | USB-UART adapter → UART bootloader via CH340N USB port |
| No laptop at all (future) | Pi pushes binary over WiFi → ESP32 flashes STM32 via UART bootloader |

---

## Manual Mode (Jog Mode)

Allows operator to energize individual relay outputs one at a time for wiring verification and sequence development. Loads fully energize in this mode — it is intended for controlled setup use, not production.

**Entry:** Hold PB0 (Run) + PB1 (Fetch) simultaneously for 2 seconds from STATE_IDLE. No E-stop required.  
**Exit:** PB1 press → returns to IDLE (all relays de-energize). E-stop press during manual mode → STATE_FAULT (all relays de-energize immediately).

**Button map (STATE_IDLE):**

| Action | Input | Hold |
|--------|-------|------|
| Fetch sequence from server | PB1 tap | — |
| Count reset (two-step) | ACK tap | — |
| Bypass arm (no RFID) | PB1 + ACK | 1 s |
| Enter manual/jog mode | PB0 + PB1 | 2 s |
| Normal arm | RFID tap | — |

**Logged** at LOG_TIER_A2 (same tier as login/bypass) for audit trail.

**GPIO allocation for 8-relay + 8-switch design (all pins confirmed free):**

| Function | Pins |
|----------|------|
| Relay outputs 5–8 (future) | PA0, PA1, PA4, PA8 |
| Manual mode switches 1–4 | PC0, PC3, PC4, PC5 |
| Manual mode switches 5–8 (future) | PC6, PC7, PC9, PC10 |

**Pendant connector:** GX16-10 (10-pin aviation connector) for 8-relay pendant — 3.3V, GND, and one signal wire per switch (PC0/PC3/PC4/PC5/PC6/PC7/PC9/PC10) = 10 pins exactly. Panel-mount male on enclosure (low voltage, exposed pins safe), female on pendant cable. Pre-wired pigtails available. Series LED circuit inside pendant: 3.3V → switch → GPIO node → LED → 220Ω → GND. GPIO configured as input with pull-down; switch closed = GPIO HIGH = relay on.

---

## Open Questions / Decisions Remaining

- [ ] Relay channel pinout on aviation connector: shared common vs. independent return per channel?
- [ ] Do any specific load channels need NC (normally closed) relay contacts instead of NO?
- [ ] Will the enclosure be wall-mount or machine-mount? (affects cable gland placement)
- [ ] Tower light / stack light: add RGB tower light to top of enclosure for status visible across shop floor? (connects to LED_* signals on ribbon, easy to add)
- [ ] Enclosure size: defer until PCB footprints are known
- [ ] Mains input: hardwired conduit or IEC inlet + cord? (IEC inlet is easier to service)
- [ ] Hand button station: two-hand control (2 inputs, safety) or single cycle-start? Determines connector pin count.

---

## Potential Upgrades (Revisit Before PCB v2)

### External Cycle-Start / Hand Button Connector
Reserve a **4-pin connector footprint** on the main PCB for external cycle-start inputs — 3 GPIO signals + GND. This accommodates:
- Two-hand control station (2 signals used, 1 spare)
- Up to 3 independent external inputs (limit switches, foot pedal, etc.)

Plain momentary buttons: GPIO configured with internal pull-up, button pulls signal to GND (active-low — same as PB0/PB1/ACK). No 3.3V needed in the connector unless buttons also have series indicator LEDs. Assign 3 GPIO pins on the main PCB schematic even if the connector is unpopulated on v1.

### Input-Triggered Sequence Step Advancement
Currently all sequence steps advance purely on a timer (`duration_ms`). A planned upgrade would allow steps to advance on an external input signal instead of (or in addition to) a timer — for example, a relay fires an actuator, the actuator trips a limit switch, and that signal triggers the next step.

**Scope of change when ready:**
- `SequenceStep_t` struct: add `trigger` (enum: TIMER / INPUT / BOTH) and `input_id` (index into external input table)
- `SequenceEngine_Update()`: change step-advancement condition from a one-liner to a ~15-line check
- `supervisor_comms.c`: update sequence deserialization to handle new fields
- Flask server: update sequence JSON schema to carry trigger type and input ID
- State machine: minimal changes — `STATE_RUNNING` behavior is unchanged

**Design decisions to make before implementing:**
- Which GPIO pins serve as sequence inputs? (Reserve on PCB — see connector above)
- Timeout behavior if expected input never arrives (fault? indefinite wait?)
- Whether sequence inputs share the hand-button connector or get their own

Estimated implementation effort once decisions are made: 2–4 hours.
