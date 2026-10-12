# Films tonight

A tile with the films on Italian TV tonight: the start time, the title, and under it the channel and the genre. It
belongs to **one sensor of Home Assistant** and fetches nothing from the internet itself.

## Set up

1. In Home Assistant, make the sensor `sensor.film_stasera` (the one this plugin was written for): a sensor whose
   state is the number of films, with four attributes of the same length, one entry per film: `titles`, `channels`,
   `starts` (seconds since 1970) and `genres`. Optional: `groups` (a number 1 to 4 per film, the colour of the dot before
   the channel's name: 1 blue, 2 amber, 3 green, 4 purple; 0 or missing draws no dot) and `stale` (true or false: the
   guide is old).
2. Add the plugin to a screen in Tessera. The screen builds once.
3. In Layout, place the **Films tonight** tile from the library's Plugins group (1x1 up to 2x3: it shows as many films
   as fit) and choose the sensor in the inspector.

## Good to know

- On a wide tile (about 13 cm or more, such as four columns of a 7-inch screen) the films run in two columns, one after
  the other, so a whole evening fits on one page.
- A film that started less than two and a half hours ago stays on the tile, with its time in the accent colour: the
  guide has no end times. The tile follows the screen's own clock, so a film leaves the list on its own.
- When no film is left it says so. When the guide is old it says that beside the date.
- It needs no key and no account.
- Tried on a real Waveshare ESP32-S3-Touch-LCD-7B (7 inch, Tessera firmware 0.55.0). On the plugin bench
  (`tools/preview.py`) it also draws without warnings on the 5-inch Waveshare and on the CYD (2.8 inch).
