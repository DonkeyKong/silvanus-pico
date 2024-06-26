# Silvanus Pico
Silvanus Pico is a firmware for the Pi Pico W microcontroller designed to drive a plant-watering machine. It improves on the original Silvanus by using way less power, being way less likely to fail, and supporting 4 independent watering pumps and 2 independent outlets for grow lights.

## Wait a minute, what is this machine?
Silvanus Pico is the brains for a plant watering machine you design and put together yourself. The reference design consists of...

- Pico W microcontroller
- 4x 12V peristaltic pumps for watering plants
- 4x MOSFET motor drivers to control pumps with GPIO
- 2x power relays to control grow lights from GPIO
- 2x momentary switches, for triggering manual watering cycles and overriding the lights
- 8x WS2812B, NeoPixel, or compatible individually addressable LEDs for status lights
- 12V 1A power supply for the motors
- 5V DC-DC buck converter to power the Pico and other electronics off the motor supply

Actually designing and assembling this machine is up to you. It can take many forms!

## Software Features
- Waters your plants daily from a reservoir
- Supports up to 4 plants (or more with T-junctions and drip irrigation tips)
- Manages 2 grow lights on a daily schedule
- Gets the time from the internet automatically

## GPIO Mapping 

Mode | Pin | Name | Description
----|---|----------|-------------
In | GP0 | Water button | Trigger a programmed watering cycle as configured right now
In | GP1 | Light button | Tap to toggle both lights on / off. Hold to resume auto behavior.
Out | GP2 | Pump 1 Relay | Sends high signal to run water pump 1
Out | GP3 | Pump 2 Relay | Sends high signal to run water pump 2
Out  | GP4 | Pump 3 Relay | Sends high signal to run water pump 3
Out  | GP5 | Pump 4 Relay | Sends high signal to run water pump 4
Out  | GP6 | LEDs | Controls a chain of 8 RGB LEDs (WS2812b / NeoPixel) used as an operating indicator on the front case
Out  | GP7 | Mains Relay 1 | Send low signal to turn on mains power to Light 1 outlet
Out  | GP8 | Mains Relay 2 | Send low signal to turn on mains power to Light 2 outlet

Inputs are assumed to be momentary switches that make a connection to ground when pressed. The lines are internally pulled up to 3.3v. You may need extra pullups if noise is a problem.

## Serial Communication Protocol

When connecting to Silvanus Pico via USB, the pico will advertise a serial device upon which commands are accepted. The interace is telnet-like but control characters like arrows, backspace, etc are not supported.

Commands are given all in lower case, parameters separated with a single space, and ending with a single `\n` newline character.

> Warning: Ensure external power supplies, if any, are connected to VSYS on the pico, not VBUS, before connecting USB. Powering the pico on the VBUS pin and then connecting USB may fry the pico, the PC or both.

### `reboot`
Reboot the microcontroller. Shuts off lights and powers down pumps before rebooting.

### `prog`

Reboot to pi pico bootloader for firmware programming. Flashes all LEDs red 3 times to confirm.

## Build Requirements
You'll need to clone the [pico-sdk](https://github.com/raspberrypi/pico-sdk) next to this repo on your disk, as build scripts will be looking for `../pico-sdk` for necessary build files. While not entirely necessary, you'll probably also want vscode and docker installed, as this project is configured to build easily with no setup if you have these tools.

## Possible Future Development
- None planned