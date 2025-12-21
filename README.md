# Hypr-brightness

#### A stupidly overengineered and fast C program... just to change your desktop monitor brightness without ddcutil 🥳.

## Usage
**hypr-brightness \<value\> [optional_flags]**

* Value: Use **-value** to decrease, **+value** to increase, or just **value** to set a specific brightness level.
* Flags:
    * -a [step] [delay]: Enable animation. step is the amount per iteration, and delay is the time in ms between steps.
    * -m [name]: Specify a monitor name (defaults to the monitor your cursor is currently on).
### Examples
* hypr-brightness -50 -a 2 20
    * Gradually reduces brightness by 50 units, decreasing by 2 every 20ms.
* hypr-brightness 100 -a -m DP-4
    * Gradually sets the DP-4 monitor to 100% brightness using default animation settings.

--- 
## Installation
You will need **cmake** and a **gcc** compiler.
First load the driver and add i2c group to your user:
```bash
sudo modprobe i2c-dev
sudo usermod -aG i2c $USER
```

### Standard Installation (/usr/local/bin)
```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

### Local Installation (~/.local/bin)
```bash
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=~/.local ..
make
make install
```

*Note: Ensure ~/.local/bin is in your PATH.*

## Uninstallation
Run this from your build directory:
```bash
make uninstall
```
---

## Why not just use ddcutil?
I wanted to achieve near-instant monitor brightness changes, which is difficult with ddcutil due to its extensive safety checks. While ddcutil has a -b flag, it isn't easily exposed via the API. 

Hypr-brightness trades off some of those checks for raw speed. By using a caching approach, it doesn't need to query the monitor's current state to perform an increment, allowing for smooth, gradual animations without the overhead.

---

## Important Considerations
* Compatibility: While built for Hyprland, it should work on other environments if you specify the display name manually with the -m flag.
* Desync: The cached brightness and physical monitor brightness can technically desync, even though that has never happened to me.
* Hardware: If this tool fails, your monitor or cable may not support DDC/CI signals. Try ddcutil first; if ddcutil works but this doesn't, please report a bug! If neither works, try a different HDMI/DP cable.