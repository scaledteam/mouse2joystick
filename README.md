# mouse2joystick
Linux program to translate mouse input into joystick input. Based on uinput and evdev. Initially made to imitate GTA 5 mouse steering in Trackmania Nations Forever.

## Compiling
Default input device is "/dev/input/event25". To compile code, run:
```
sudo ./mouse2joystick.c
```

It's self-compiling code, it will compile itself and run.

## Usage
When program is activated, press any side mouse button to grab mouse, and hold left mouse button to steer. To ungrab devide, press side button again.
