![Bandolibre keyboard](documentation/images/3dmodel_keyboard_tilt.webp)

🇬🇧 **English** | 🇫🇷 [Français](README.fr.md) | 🇪🇸 [Español](README.es.md)

# Bandolibre

An open-source MIDI controller that brings the Argentine bandoneon into the digital age.

**Bandolibre focuses on one goal:** turning key presses and bellows movement into MIDI — faithfully, instantly, and without the cost or noise of an acoustic instrument. Plug it into any computer, tablet, or phone over USB and you're ready to play, compose, or practice.

An open initiative by [L'Atelier du bandonéon libre](https://github.com/bandolibre)

---

## See it play

[![Bandolibre demo with a clarinet virtual instrument](https://img.youtube.com/vi/6s1wlRKlAk4/maxresdefault.jpg)](https://youtu.be/6s1wlRKlAk4)

Watch the demos: [with a clarinet virtual instrument](https://youtu.be/6s1wlRKlAk4) · [with strings virtual instruments](https://youtu.be/nJ0j7DtbYDk).

---

## Who is it for?

- **Students** who don't own an instrument yet and want to start learning
- **Players** who want to practice silently — at home, on the road, at any hour
- **Composers** entering scores into notation software one note at a time
- **Musicians** triggering synths and samplers on stage or in the studio

---

## How it works

- **142-key Rheinische Lage layout** — both hands, exact fingering of the Argentine bandoneon
- **Hall-effect sensors** on every key — no mechanical contact, no wear, scanned 2400 times per second
- **Blade spring + sensor pair** for the bellows — measures push/pull effort and emits MIDI CC#11 (Expression), just like the real thing
- **Two 6.35 mm expression pedal inputs** — compatible with M-Audio EX-P pedals; pedal 1 sends CC#1 (Modulation), pedal 2 sends CC#4 (Foot Controller)
- **USB-MIDI** out of the box — plug into any DAW, notation app, or synth; no drivers needed

---

## Features

**Keyboard tuning** — a button cycle through the three keyboard layouts: Rheinische Tonlage (bisonoric, 142 tones), Peguri, or Manoury.

**Table mode** — a button toggles table mode: keys fire immediately at fixed velocity, no bellows movement required. Handy for entering a score note by note without working the bellows.

**Play anywhere** — Bandolibre is bus-powered over USB; any phone, tablet, or laptop with a soft synth becomes the sound engine. A small USB hub with a headphone jack and a power pass-through gives you audio, charging, and MIDI from a single cable — tested and pocket-sized.

**Adaptable** — standard USB-MIDI is compatible with the whole adapter ecosystem: plug in a USB Bluetooth MIDI adapter to play wirelessly, or a USB-to-DIN-5 adapter to drive vintage hardware synths.

---

## Anyone can build it

This is a fully open DIY project. PCBs are designed for economic JLCPCB two-layer fabrication — affordable and easy to order. The mechanical parts are 3D-printable (a FabLab near you works great). Firmware is open-source and flashable with a standard [ST-LINK probe](https://www.st.com/en/development-tools/stlink-v3minie.html) and a [TC-2070-IDC-050](https://www.tag-connect.com/product/tc2070-idc-050) cable.

Building one costs about as much as a decent MIDI keyboard or a good pair of studio headphones like the DT-770 Pro.


The parts list, tools, consumables and step-by-step assembly are in the assembly instructions:

<a href="documentation/assembly_instructions.md"><img width="320" alt="Assembly instructions" src="https://img.shields.io/badge/Assembly_instructions-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA1NzYgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTU0Mi4yMiAzMi4wNWMtNTQuOCAzLjExLTE2My43MiAxNC40My0yMzAuOTYgNTUuNTktNC42NCAyLjg0LTcuMjcgNy44OS03LjI3IDEzLjE3djM2My44N2MwIDExLjU1IDEyLjYzIDE4Ljg1IDIzLjI4IDEzLjQ5IDY5LjE4LTM0LjgyIDE2OS4yMy00NC4zMiAyMTguNy00Ni45MiAxNi44OS0uODkgMzAuMDItMTQuNDMgMzAuMDItMzAuNjZWNjIuNzVjLjAxLTE3LjcxLTE1LjM1LTMxLjc0LTMzLjc3LTMwLjd6TTI2NC43MyA4Ny42NEMxOTcuNSA0Ni40OCA4OC41OCAzNS4xNyAzMy43OCAzMi4wNSAxNS4zNiAzMS4wMSAwIDQ1LjA0IDAgNjIuNzVWNDAwLjZjMCAxNi4yNCAxMy4xMyAyOS43OCAzMC4wMiAzMC42NiA0OS40OSAyLjYgMTQ5LjU5IDEyLjExIDIxOC43NyA0Ni45NSAxMC42MiA1LjM1IDIzLjIxLTEuOTQgMjMuMjEtMTMuNDZWMTAwLjYzYzAtNS4yOS0yLjYyLTEwLjE0LTcuMjctMTIuOTl6Ii8+PC9zdmc+"></a>

They are best built in batches of five — JLCPCB minimum orders make that the natural unit. Team up with friends or reach out to [L'Atelier du bandonéon libre](https://github.com/bandolibre) to express interest in a community build.

At fifty units, PCB fabrication and Gateron switches — the two biggest line items — drop by half again.

- **Firmware:** see [`code/`](code/) — build with `just build` and `just flash`, flash via ST-LINK
- **Firmware updates:** put the board in DFU mode and it shows up as a USB Mass Storage drive — drag the `.uf2` file onto it and it reboots on the new firmware, no programmer needed

---

## Configuration tool

The configuration tool is a web page that displays and changes the instrument's parameters over USB-MIDI — it works on a computer, tablet or phone. It lets you tune the bellows by dragging the push and pull curves (or picking a preset) to shape how effort turns into expression, and calibrate the rest position. Changes apply immediately, so you can adjust the feel while playing. It also calibrates the pedals, switches tuning, sensitivity and table mode, edits every firmware property, and shows both keyboards live as you play.

<a href="https://bandolibre.github.io/tools/midi.html"><img width="385" alt="Open the configuration tool" src="https://img.shields.io/badge/Open_the_configuration_tool-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA1MTIgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTQ5NiAzODRIMTYwdi0xNmMwLTguOC03LjItMTYtMTYtMTZoLTMyYy04LjggMC0xNiA3LjItMTYgMTZ2MTZIMTZjLTguOCAwLTE2IDcuMi0xNiAxNnYzMmMwIDguOCA3LjIgMTYgMTYgMTZoODB2MTZjMCA4LjggNy4yIDE2IDE2IDE2aDMyYzguOCAwIDE2LTcuMiAxNi0xNnYtMTZoMzM2YzguOCAwIDE2LTcuMiAxNi0xNnYtMzJjMC04LjgtNy4yLTE2LTE2LTE2em0wLTE2MGgtODB2LTE2YzAtOC44LTcuMi0xNi0xNi0xNmgtMzJjLTguOCAwLTE2IDcuMi0xNiAxNnYxNkgxNmMtOC44IDAtMTYgNy4yLTE2IDE2djMyYzAgOC44IDcuMiAxNiAxNiAxNmgzMzZ2MTZjMCA4LjggNy4yIDE2IDE2IDE2aDMyYzguOCAwIDE2LTcuMiAxNi0xNnYtMTZoODBjOC44IDAgMTYtNy4yIDE2LTE2di0zMmMwLTguOC03LjItMTYtMTYtMTZ6bTAtMTYwSDI4OFY0OGMwLTguOC03LjItMTYtMTYtMTZoLTMyYy04LjggMC0xNiA3LjItMTYgMTZ2MTZIMTZDNy4yIDY0IDAgNzEuMiAwIDgwdjMyYzAgOC44IDcuMiAxNiAxNiAxNmgyMDh2MTZjMCA4LjggNy4yIDE2IDE2IDE2aDMyYzguOCAwIDE2LTcuMiAxNi0xNnYtMTZoMjA4YzguOCAwIDE2LTcuMiAxNi0xNlY4MGMwLTguOC03LjItMTYtMTYtMTZ6Ii8+PC9zdmc+"></a>

<p>
  <img src="documentation/images/configuration_tool.webp" alt="Configuration tool, bellows curve" width="49%">
  <img src="documentation/images/configuration_tool_keyboard.webp" alt="Configuration tool, live keyboards" width="49%">
</p>

---

## Design

![overview](documentation/images/3dmodel_overview.webp)

The mechanical parts are modeled in Onshape — [the full 3D model](https://cad.onshape.com/documents/313e70e978bf056a8dd7d76c/v/5c5fbc4088ac379c1bd1b53a/e/c6a89cb028bdc195ff70596f?showReturnToWorkspaceLink=tru) is public and interactive. Reference [photography](keyboard_picture/) of real instruments was used to accurately reproduce the shape, key placement, and keyboard tilt of both hands.
![handle_layout](documentation/images/3dmodel_handle.webp)
![keyboard_layout](documentation/images/3dmodel_keyboard_layout.webp)

The PCBs are designed with EasyEDA.
![3d_pcb](documentation/images/pcb_main_board_3d.png)

The bellows is replaced by a **blade spring instrumented with two Hall-effect sensors** that read its flexion. Load cell solutions were ruled out early — they are too rigid and remove the tactile feedback players rely on to feel and modulate their effort — it plays like pressing on a wall. The blade spring preserves that proprioceptive feedback while being simple and durable. Blade thickness can be chosen to tune the instrument's stiffness, from light to firm.

![blade spring](documentation/images/bandolibre_blade.webp)

A sensitivity selector button cycles through three amplification levels so the player can adjust how much bellows travel is needed to reach full expression — useful for quiet practice or a stiffer spring.

For a detailed breakdown of the firmware behavior and controls, see [`documentation/features.md`](documentation/features.md).

---

## Sound

The best match so far is the **SWAM** family of simulation instruments from [Audio Modeling](https://audiomodeling.com/): they are physically modelled rather than sampled, so CC#11 continuously drives the model itself. Bellows intensity comes out as real dynamics — timbre changing with pressure, notes swelling and dying under the bellows — instead of a volume fade over a fixed sample. [Native Instruments Session Strings](https://www.native-instruments.com/en/products/komplete/cinematic/session-strings-2) also works very well.

Modern bandoneon libraries, oddly enough, work poorly for us. They either assume the wrong keyboard layout — chromatic or accordion mappings, with no push/pull distinction — or offer only shallow CC#11 support, with dynamics baked into velocity-triggered samples that the bellows cannot reshape once a note has started.

For practice — and especially on a phone — a simple bandoneon soundfont is enough: [European Bandoneon V2.5 by Jörg Bleymehl](https://musical-artifacts.com/artifacts/1862) gives good results. It loads in any SF2-capable player, costs almost nothing in CPU, and turns a phone or tablet into a usable practice rig without a laptop or a plugin host.

So we are actively looking for virtual instruments with deep expressive support — rich polyphony and CC#11 as a primary articulation driver. If you know of one, or want to help build something tailored to the bandoneon, suggestions and contributions are very welcome.

---

## Where we are

<p>
  <img src="documentation/images/bandolibre_overview.webp" alt="Bandolibre overview" width="49%">
  <img src="documentation/images/bandolibre_main_module.webp" alt="Bandolibre main module" width="49%">
</p>

Five units are built and working. The firmware handles all 142 keys, bellows push/pull, pedals, and MIDI output reliably. These instruments are currently on loan to bandoneon teachers who are giving us hands-on feedback while we polish the software.

The 3D models and PCB designs are solid — no rework planned there. The active focus right now is tuning the bellows simulation: getting the inertia model right so that short notes feel like the real instrument, not like a sensor.

There is a lot to explore on the software side. Because Hall-effect sensors measure key position continuously — not just on/off — the firmware has access to the full travel of every key at all times. This opens the door to **MPE (MIDI Polyphonic Expression)**: per-note pressure, slide, and lift curves, independently for each of the 142 keys simultaneously.

---

## How to get a device

Bandolibre stays a DIY project: the design is open and anyone can build one. We are polishing the software and collecting feedback on the hardware.

We also miss the paperwork that would allow the association handle a transaction  — passing on boards, a kit, or a finished instrument to someone who asks. 

If you'd like to hear from us when it becomes possible:

<a href="https://forms.gle/amgxEX4XTy9Jfd538"><img width="294" alt="Join the interest list" src="https://img.shields.io/badge/Join_the_interest_list-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAzODQgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTMzNiA2NGgtODBjMC0zNS4zLTI4LjctNjQtNjQtNjRzLTY0IDI4LjctNjQgNjRINDhDMjEuNSA2NCAwIDg1LjUgMCAxMTJ2MzUyYzAgMjYuNSAyMS41IDQ4IDQ4IDQ4aDI4OGMyNi41IDAgNDgtMjEuNSA0OC00OFYxMTJjMC0yNi41LTIxLjUtNDgtNDgtNDh6TTk2IDQyNGMtMTMuMyAwLTI0LTEwLjctMjQtMjRzMTAuNy0yNCAyNC0yNCAyNCAxMC43IDI0IDI0LTEwLjcgMjQtMjQgMjR6bTAtOTZjLTEzLjMgMC0yNC0xMC43LTI0LTI0czEwLjctMjQgMjQtMjQgMjQgMTAuNyAyNCAyNC0xMC43IDI0LTI0IDI0em0wLTk2Yy0xMy4zIDAtMjQtMTAuNy0yNC0yNHMxMC43LTI0IDI0LTI0IDI0IDEwLjcgMjQgMjQtMTAuNyAyNC0yNCAyNHptOTYtMTkyYzEzLjMgMCAyNCAxMC43IDI0IDI0cy0xMC43IDI0LTI0IDI0LTI0LTEwLjctMjQtMjQgMTAuNy0yNCAyNC0yNHptMTI4IDM2OGMwIDQuNC0zLjYgOC04IDhIMTY4Yy00LjQgMC04LTMuNi04LTh2LTE2YzAtNC40IDMuNi04IDgtOGgxNDRjNC40IDAgOCAzLjYgOCA4djE2em0wLTk2YzAgNC40LTMuNiA4LTggOEgxNjhjLTQuNCAwLTgtMy42LTgtOHYtMTZjMC00LjQgMy42LTggOC04aDE0NGM0LjQgMCA4IDMuNiA4IDh2MTZ6bTAtOTZjMCA0LjQtMy42IDgtOCA4SDE2OGMtNC40IDAtOC0zLjYtOC04di0xNmMwLTQuNCAzLjYtOCA4LThoMTQ0YzQuNCAwIDggMy42IDggOHYxNnoiLz48L3N2Zz4="></a>

It is useful for us to see how many people are interested and what they'd play it for. Your answers stay with the association and are only used to contact you about Bandolibre.

---

## Community

Questions, ideas, or just curious?
Join [L'Atelier du bandonéon libre](https://bandolibre.github.io).

The main board communicates digitally with the wing boards and can support any layout. It is possible to design a new keyboard for a different system — Rheinische Lage, Club, Einheitsbandoneon — and reuse the main board.

Working on something similar? Let the association know — we'd love to connect.

<a href="mailto:bandolibre@googlegroups.com"><img width="311" alt="Email the association" src="https://img.shields.io/badge/Email_the_association-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA1MTIgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTUwMi4zIDE5MC44YzMuOS0zLjEgOS43LS4yIDkuNyA0LjdWNDAwYzAgMjYuNS0yMS41IDQ4LTQ4IDQ4SDQ4Yy0yNi41IDAtNDgtMjEuNS00OC00OFYxOTUuNmMwLTUgNS43LTcuOCA5LjctNC43IDIyLjQgMTcuNCA1Mi4xIDM5LjUgMTU0LjEgMTEzLjYgMjEuMSAxNS40IDU2LjcgNDcuOCA5Mi4yIDQ3LjYgMzUuNy4zIDcyLTMyLjggOTIuMy00Ny42IDEwMi03NC4xIDEzMS42LTk2LjMgMTU0LTExMy43ek0yNTYgMzIwYzIzLjIuNCA1Ni42LTI5LjIgNzMuNC00MS40IDEzMi43LTk2LjMgMTQyLjgtMTA0LjcgMTczLjQtMTI4LjcgNS44LTQuNSA5LjItMTEuNSA5LjItMTguOXYtMTljMC0yNi41LTIxLjUtNDgtNDgtNDhINDhDMjEuNSA2NCAwIDg1LjUgMCAxMTJ2MTljMCA3LjQgMy40IDE0LjMgOS4yIDE4LjkgMzAuNiAyMy45IDQwLjcgMzIuNCAxNzMuNCAxMjguNyAxNi44IDEyLjIgNTAuMiA0MS44IDczLjQgNDEuNHoiLz48L3N2Zz4="></a>

---

## Further readings

- [Other electronic bandoneon projects](documentation/other-projects.md)

---

## License

[![CC BY-NC-SA 4.0](https://mirrors.creativecommons.org/presskit/buttons/88x31/svg/by-nc-sa.eu.svg)](LICENSE.md)

Free to build, modify, and share for non-commercial use.
