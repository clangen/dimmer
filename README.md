# dimmer

a display dimming app for windows with multi-monitor support.

**dimmer** is a no-frills program written in vanilla win32 with a minimal user interface. it lives in the system tray and uses virtually no resources. click the icon to see a list of monitors, and adjust your desired brightness.

the app works by applying a semi-transparent overlay on top of all other running programs. note that, by default, the operating system will place popup menus on top of this overlay. if this annoys you, you can select the `dim popups` option in the tray menu. it's not enabled by default because it's a gross hack that eats a few cpu cycles.

**dimmer** is also has very basic support for adjusting color temperature -- you can select 4000, 4500, 5000, 5500, or 6000 kelvin emulation. just like brightness, temperature can be changed on a per-monitor basis. 

# screenshot

it works like this:

![dimmer screenshot](https://raw.githubusercontent.com/clangen/clangen-projects-static/master/dimmer/screenshots/screenshot.png)

# installation

download, unzip, and run! no installation or additional runtimes required.

# license

standard 3-clause bsd. do whatever you want with it, just don't blame me if it breaks something.
