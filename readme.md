# welcome to yalda ! 

A lightweight Rich Presence app in pure C (win32 handles and unix sockets) and fully CLI.
It looks at the apps running on your system and lets you pick one to show off on your Discord profile.
It reads data from the window titles, grabs album art for music players, and counts through the song. Might do more in the future.

## How it works (the two .exe thing)

* **yalda_daemon.exe** : the engine. Runs hidden in your system tray and does all the work (talking to Discord, fetching album art, watching window titles). Start it once, forget it exists.
* **yalda.exe** : the window. Shows you every open app with numbers, you press a number, done. You can also just double-click the tray icon.

## Features

* **Cross-Platform (Mac, Linux & Windows machines)**
* *Unix part isn't done yet* <(＿　＿)>
* music mode: album art (from iTunes) & a song timer
* notices when the song changes all by itself
* daemon hides in the tray, no console windows, promise (￣▽￣)

## Installation

### Pre-compiled
 * check releases (～￣▽￣)～

## Building from source

  **Requirements for windows :** I recommend [w64devkit](https://github.com/skeeto/w64devkit) but ig u can do whatever u want (￣▽￣)

  ```bash
    > git clone https://codeberg.org/adiyn/easy-rich-presence.git
    > cd easy-rich-presence
    > make
  ```

### Roadmap
 * unix version (todo)
 * pause detection (todo)
 * go outside (todo)

### License

Distributed under the Apache 2.0 License. See LICENSE for more information.

### Author

  * adiyn
### AI
  * overall < 5% ai ? I had issues with understanding how named pipes worked and the flags, I have no idea who came up with these names but zamn...

# ₍ᐢ. .ᐢ₎ ₊˚⊹
