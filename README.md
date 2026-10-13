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
4. Tap the tile to open the card: every film still to come and the ones on air, in columns as wide as the screen. When
   they do not fit, it turns to the next page by itself every 8 seconds.
5. Optional: the whole week on the card, below.

## The whole week

The card can show today and the six days after it, one after the other: after the last page of a day it turns to the
next day, and after the seventh back to today. The line on top says the day and the page. Back on another day returns
to today; Back on today closes the card. (Tessera's cards take no taps yet, so there are no keys to choose a day.)

A tile gets at most about 2.6 KB of its sensor, too little for a week, so the card asks Home Assistant for one day at
a time:

1. Make one sensor per day beside the tile's, with the same attributes plus `date`: `sensor.film_stasera_day_0` (today)
   up to `sensor.film_stasera_day_6`. A day with more than about 3 KB of films is cut from the end.
2. Add this script to Home Assistant (Settings, Automations & scenes, Scripts, or `scripts.yaml` without the first
   line). Change `sensor.film_stasera_day_` if your sensors are named otherwise.

```yaml
script:
  film_stasera_day:
    alias: Film stasera - one day
    description: The films of one day for the Tessera plugin film_stasera (0 today, 1 tomorrow, up to 6).
    mode: parallel
    fields:
      day:
        description: 0 today, 1 tomorrow, up to 6
        example: 1
        selector:
          number: { min: 0, max: 6 }
    sequence:
      - variables:
          sensor: "sensor.film_stasera_day_{{ day | int(0) }}"
      - variables:
          films:
            date: "{{ state_attr(sensor, 'date') }}"
            titles: "{{ state_attr(sensor, 'titles') or [] }}"
            channels: "{{ state_attr(sensor, 'channels') or [] }}"
            starts: "{{ state_attr(sensor, 'starts') or [] }}"
            genres: "{{ state_attr(sensor, 'genres') or [] }}"
            groups: "{{ state_attr(sensor, 'groups') or [] }}"
      - stop: The films of the day
        response_variable: films
```

3. Nothing to do in Tessera: the plugin may ask for `script.film_stasera_day` and nothing else. Without the script the
   card shows today only.

## Good to know

- On a wide tile (about 13 cm or more, such as four columns of a 7-inch screen) the films run in two columns, one after
  the other, so a whole evening fits on one page.
- A film that started less than two and a half hours ago stays on the tile, with its time in the accent colour: the
  guide has no end times. The tile follows the screen's own clock, so a film leaves the list on its own.
- When no film is left it says so. When the guide is old it says that beside the date.
- It needs no key and no account.
- Tried on a real Waveshare ESP32-S3-Touch-LCD-7B (7 inch, Tessera firmware 0.55.0). On the plugin bench
  (`tools/preview.py`) it also draws without warnings on the 5-inch Waveshare and on the CYD (2.8 inch).
