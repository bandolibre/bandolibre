# Bandolibre user manual

This manual is for players. It explains how to connect the Bandolibre, set up
your music software, use the function buttons and pedals, tune the feel of the
bellows, and update the firmware. You don't need any tools or technical
knowledge.

---

## Overview

The Bandolibre is a MIDI controller shaped like an Argentine bandoneon. It makes
no sound on its own: it sends what you play to a computer, tablet or phone,
where a virtual instrument turns it into sound.

- **Two keyboards**, left and right hand, with the 142-tone Rheinische Tonlage
  layout.
- **Bellows**: a spring blade between the two hands. It measures how hard you
  push or pull, the way a real bellows would respond.
- **Three function buttons** on the main module: left, middle and right.
- **Two pedal inputs** (6.35 mm jacks) for expression pedals.
- **One USB Type-B port**, for both power and MIDI.

---

## Connecting

Plug the Bandolibre into your computer, tablet or phone with a USB Type-B
cable. That single cable powers the instrument and carries MIDI. There is no
battery, power adapter or driver.

It appears as a MIDI device named **Bandolibre**.

---

## Setting up your software

In your DAW, notation program or synth app, select **Bandolibre** as the MIDI
input.

- The **left hand** plays on **MIDI channel 1**, the **right hand** on
  **channel 2**. You can give each hand its own instrument, or send both
  channels to the same one.
- The bellows sends **CC#11 (Expression)** on both channels, continuously while
  you play. It also sets the **velocity** of each note: the harder you push or
  pull when you press a key, the higher the velocity.
- Pedal 1 sends **CC#1 (Modulation)** and pedal 2 **CC#4 (Foot Controller)**,
  also on both channels.

---

## Choosing a sound

The Bandolibre needs a virtual instrument to make sound. Whatever you use:

- choose an instrument that responds to **CC#11**, otherwise the bellows has no
  effect on the sound;
- make it listen on **channels 1 and 2** (or on all channels), otherwise one
  hand stays silent.

For practice, a simple bandoneon soundfont is enough:
[European Bandoneon V2.5](https://musical-artifacts.com/artifacts/1862) by Jörg
Bleymehl gives good results and works on any device below. The
[Sound](../README.md#sound) section of the README has more background.

### On a computer

Use a DAW (Reaper, Ableton Live, Logic, Cubase…) or a notation program, and
load a virtual instrument plugin on the Bandolibre's MIDI input.

- The physically modelled **SWAM** instruments from
  [Audio Modeling](https://audiomodeling.com/) give the most expressive
  results: the bellows drives the sound itself, not just its volume.
- [Native Instruments Session Strings](https://www.native-instruments.com/en/products/komplete/cinematic/session-strings-2)
  also works very well.

### On an Android phone or tablet

Use the **FlowTones** app:

1. Plug in the Bandolibre and open FlowTones.
2. Load the European Bandoneon soundfont and select the Bandolibre as MIDI
   input.
3. Link the bellows to the volume with **MIDI learn**: start MIDI learn on the
   volume control, then push or pull the bellows. FlowTones assigns CC#11 to
   the volume, and the bellows now shapes the loudness of every note.

### On an iPhone or iPad

Plug in the Bandolibre, then open a synth app that accepts MIDI input. For the
soundfont, use a player such as **bs-16i**. The SWAM instruments are also
available for iPhone and iPad from the App Store.

---

## Playing

The Bandolibre plays like a bandoneon:

- **A note sounds only while the bellows moves.** Pressing a key with the
  bellows at rest plays nothing, as on the real instrument.
- **Push and pull play different notes.** With the default Rheinische Tonlage
  tuning, each key has one note when you push and another when you pull.
- **The bellows controls the dynamics.** How hard you push or pull sets how
  loud each note starts, and keeps shaping the sound for as long as it lasts.

---

## Function buttons

### Left button: table mode

Press the **left button** to switch table mode on or off.

In table mode you can play with the instrument resting flat on a table. Each key
sounds as soon as you press it, without moving the bellows. Every key plays its
**pull** note, and every note plays at the same fixed loudness.

This is useful for entering a score into notation software one note at a time.
Press it again to go back to normal playing.

### Middle button: bellows sensitivity

Press the **middle button** to step through three sensitivity levels: **low → medium →
high**, then back to low.

At a higher level, you reach full loudness with less bellows effort. Use it for
quiet practice, or if the spring blade feels too stiff. Sensitivity has no
effect in table mode.

### Right button: tuning

Press the **right button** to step through the three keyboard systems:

| System | Push and pull |
|---|---|
| **Rheinische Tonlage** (default) | Different notes (bisonoric), the Argentine bandoneon |
| **Peguri** | Same note both ways (unisonoric) |
| **Manoury** | Same note both ways (unisonoric) |

After Manoury it goes back to Rheinische Tonlage.

The Peguri and Manoury right-hand keyboards normally have 40 buttons; the
Bandolibre has 38. The two missing notes are the lowest D4 and D♯4. Everything
from E4 upward is complete.

You can switch tuning while holding keys: notes already sounding keep their
pitch until you release them.

---

## Pedals

You can connect up to two expression pedals of the M-Audio EX-P type. If your
pedal has a mode switch, set it to **M-Audio**.

- **Pedal 1** sends **CC#1 (Modulation)**.
- **Pedal 2** sends **CC#4 (Foot Controller)**.

Most instruments already respond to these controllers. In your DAW you can also
use MIDI Learn to assign a pedal to any other parameter.

If a pedal doesn't cover its full range, or never reaches zero, calibrate it in
the [configuration tool](#configuration-tool).

---

## Configuration tool

The configuration tool is a web page that displays and changes the
instrument's settings over USB-MIDI. There is nothing to install.

It works in browsers that support Web MIDI, such as Chrome or Edge, on a
computer or an Android phone or tablet. It does not work on iPhone or iPad.

With it you can:

- **tune the bellows**: drag the push and pull curves, or pick a preset, to
  choose how your effort turns into loudness;
- **calibrate the rest position** of the bellows;
- **calibrate the pedals**;
- switch tuning, sensitivity and table mode;
- see both keyboards live as you play.

Changes apply immediately, so you can adjust the feel while playing.

[Open the configuration tool](https://bandolibre.github.io/tools/midi.html)

<p>
  <img src="images/configuration_tool.webp" alt="Configuration tool, bellows curve" width="49%">
  <img src="images/configuration_tool_keyboard.webp" alt="Configuration tool, live keyboards" width="49%">
</p>

---

## Settings are reset when you unplug

The Bandolibre does not yet remember its settings. When you unplug it, it goes
back to its defaults: Rheinische Tonlage tuning, low sensitivity, table mode
off, and the default bellows and pedal settings.

If you have tuned the bellows or pedals in the configuration tool, you need to
set them again after reconnecting.

---

## Updating the firmware

The firmware is the software inside the instrument. You update it over the same
USB cable, without any extra tool or software.

1. Download the latest `main-g474.uf2` from the
   [releases page](https://github.com/bandolibre/bandolibre/releases).
2. Unplug the Bandolibre.
3. Hold down the **right function button** and plug the cable back in. Keep
   holding it until a drive appears.
4. A drive called **BANDOLIBRE** appears, like a USB stick. The light above the
   right button blinks slowly while it waits.
5. Copy `main-g474.uf2` onto that drive.
6. The light blinks faster during the copy, then stays on for about a second.
   The drive then disappears and the Bandolibre restarts with the new firmware.

The drive exists only to receive the firmware file; files copied onto it do not
stay there.

### If something goes wrong

- **The copy was interrupted, or the cable came out.** Nothing is damaged: the
  new firmware is only installed once the whole file has arrived. Start again
  from step 2.
- **The BANDOLIBRE drive appears without you holding the right button.** The instrument
  found no valid firmware and is waiting for one. Copy the `.uf2` file again.
- **Nothing happens when you copy the file.** Check that it is the
  `main-g474.uf2` file from the releases page. Any other file is ignored, so a
  wrong file cannot damage the instrument.
- **The drive does not appear.** Hold the right button *before* plugging in the cable, and
  keep holding it for a second or two afterwards.

---

## Troubleshooting

**No sound at all**
- Check that **Bandolibre** is selected as the MIDI input in your software.
- Check that the instrument listens on channel 1 (left hand) and channel 2
  (right hand), or on all channels.
- Move the bellows while pressing a key, or turn on table mode (left button).
- Check that your instrument responds to CC#11: some stay silent while CC#11
  is at 0.

**Notes are too quiet or too loud**
- Press the middle button to change the bellows sensitivity.
- Adjust the bellows curves in the configuration tool.

**Wrong notes**
- Press the right button until the tuning you play is selected. After unplugging, the
  tuning goes back to Rheinische Tonlage.

**A note keeps sounding after you release the key**
- Unplug and reconnect the Bandolibre. Most software also stops stuck notes on
  its own when the instrument is disconnected.

---

## Getting help

Questions, problems or ideas? Write to the association:

<a href="mailto:bandolibre@googlegroups.com"><img width="311" alt="Email the association" src="https://img.shields.io/badge/Email_the_association-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA1MTIgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTUwMi4zIDE5MC44YzMuOS0zLjEgOS43LS4yIDkuNyA0LjdWNDAwYzAgMjYuNS0yMS41IDQ4LTQ4IDQ4SDQ4Yy0yNi41IDAtNDgtMjEuNS00OC00OFYxOTUuNmMwLTUgNS43LTcuOCA5LjctNC43IDIyLjQgMTcuNCA1Mi4xIDM5LjUgMTU0LjEgMTEzLjYgMjEuMSAxNS40IDU2LjcgNDcuOCA5Mi4yIDQ3LjYgMzUuNy4zIDcyLTMyLjggOTIuMy00Ny42IDEwMi03NC4xIDEzMS42LTk2LjMgMTU0LTExMy43ek0yNTYgMzIwYzIzLjIuNCA1Ni42LTI5LjIgNzMuNC00MS40IDEzMi43LTk2LjMgMTQyLjgtMTA0LjcgMTczLjQtMTI4LjcgNS44LTQuNSA5LjItMTEuNSA5LjItMTguOXYtMTljMC0yNi41LTIxLjUtNDgtNDgtNDhINDhDMjEuNSA2NCAwIDg1LjUgMCAxMTJ2MTljMCA3LjQgMy40IDE0LjMgOS4yIDE4LjkgMzAuNiAyMy45IDQwLjcgMzIuNCAxNzMuNCAxMjguNyAxNi44IDEyLjIgNTAuMiA0MS44IDczLjQgNDEuNHoiLz48L3N2Zz4="></a>

To be told when boards, kits or finished instruments become available:

<a href="https://forms.gle/amgxEX4XTy9Jfd538"><img width="294" alt="Join the interest list" src="https://img.shields.io/badge/Join_the_interest_list-7a8288?style=flat&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAzODQgNTEyIj48cGF0aCBmaWxsPSJ3aGl0ZSIgZD0iTTMzNiA2NGgtODBjMC0zNS4zLTI4LjctNjQtNjQtNjRzLTY0IDI4LjctNjQgNjRINDhDMjEuNSA2NCAwIDg1LjUgMCAxMTJ2MzUyYzAgMjYuNSAyMS41IDQ4IDQ4IDQ4aDI4OGMyNi41IDAgNDgtMjEuNSA0OC00OFYxMTJjMC0yNi41LTIxLjUtNDgtNDgtNDh6TTk2IDQyNGMtMTMuMyAwLTI0LTEwLjctMjQtMjRzMTAuNy0yNCAyNC0yNCAyNCAxMC43IDI0IDI0LTEwLjcgMjQtMjQgMjR6bTAtOTZjLTEzLjMgMC0yNC0xMC43LTI0LTI0czEwLjctMjQgMjQtMjQgMjQgMTAuNyAyNCAyNC0xMC43IDI0LTI0IDI0em0wLTk2Yy0xMy4zIDAtMjQtMTAuNy0yNC0yNHMxMC43LTI0IDI0LTI0IDI0IDEwLjcgMjQgMjQtMTAuNyAyNC0yNCAyNHptOTYtMTkyYzEzLjMgMCAyNCAxMC43IDI0IDI0cy0xMC43IDI0LTI0IDI0LTI0LTEwLjctMjQtMjQgMTAuNy0yNCAyNC0yNHptMTI4IDM2OGMwIDQuNC0zLjYgOC04IDhIMTY4Yy00LjQgMC04LTMuNi04LTh2LTE2YzAtNC40IDMuNi04IDgtOGgxNDRjNC40IDAgOCAzLjYgOCA4djE2em0wLTk2YzAgNC40LTMuNiA4LTggOEgxNjhjLTQuNCAwLTgtMy42LTgtOHYtMTZjMC00LjQgMy42LTggOC04aDE0NGM0LjQgMCA4IDMuNiA4IDh2MTZ6bTAtOTZjMCA0LjQtMy42IDgtOCA4SDE2OGMtNC40IDAtOC0zLjYtOC04di0xNmMwLTQuNCAzLjYtOCA4LThoMTQ0YzQuNCAwIDggMy42IDggOHYxNnoiLz48L3N2Zz4="></a>
