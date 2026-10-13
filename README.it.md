# Film stasera

Un tile con i film in TV stasera in Italia: l'orario d'inizio, il titolo e, sotto, il canale e il genere. Appartiene
a **un sensore di Home Assistant** e non scarica niente da internet.

## Come si usa

1. In Home Assistant crea il sensore `sensor.film_stasera`: lo stato e' il numero di film, con tre attributi
   della stessa lunghezza, una voce per film: `titles`, `channels` e `starts` (secondi dal 1970). Facoltativi: `groups` (un numero da 1 a 4 per film, il colore del pallino
   davanti al nome del canale: 1 blu, 2 ambra, 3 verde, 4 viola; 0 o assente: nessun pallino) e `stale` (true o
   false: la guida e' vecchia).
2. Aggiungi il plugin a uno schermo in Tessera. Lo schermo si ricompila una volta.
3. In Layout metti il tile **Film stasera** dalla libreria (gruppo Plugin), da 1x1 a 2x3: mostra tutti i film che
   ci stanno. Poi scegli il sensore nell'ispettore.
4. Tocca il tile per aprire la scheda: tutti i film ancora in programma e quelli in onda, in colonne larghe quanto lo
   schermo. Quando non ci stanno tutti, passa da sola alla pagina successiva ogni 8 secondi.
5. Facoltativo: tutta la settimana nella scheda, qui sotto.

## Tutta la settimana

La scheda puo' mostrare oggi e i sei giorni dopo, uno dopo l'altro: dopo l'ultima pagina di un giorno passa al giorno
dopo, e dopo il settimo torna a oggi. La riga in alto dice il giorno e la pagina. Indietro su un altro giorno torna a
oggi; Indietro su oggi chiude la scheda. (Le schede di Tessera non ricevono ancora i tocchi, quindi non ci sono tasti
per scegliere il giorno.)

Un tile riceve al massimo circa 2,6 KB del suo sensore, troppo poco per una settimana, quindi la scheda chiede a Home
Assistant un giorno alla volta:

1. Crea un sensore per giorno accanto a quello del tile, con gli stessi attributi piu' `date`:
   `sensor.film_stasera_day_0` (oggi) fino a `sensor.film_stasera_day_6`. Un giorno con piu' di circa 3 KB di film
   viene tagliato dalla fine.
2. Aggiungi questo script a Home Assistant (Impostazioni, Automazioni e scene, Script, oppure `scripts.yaml` senza la
   prima riga). Cambia `sensor.film_stasera_day_` se i tuoi sensori hanno un altro nome.

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
            groups: "{{ state_attr(sensor, 'groups') or [] }}"
      - stop: The films of the day
        response_variable: films
```

3. In Tessera non serve altro: il plugin puo' chiedere `script.film_stasera_day` e nient'altro. Senza lo script la
   scheda mostra solo oggi.

## Da sapere

- Su un tile largo (circa 13 cm o piu', come quattro colonne di uno schermo da 7 pollici) i film vanno su due colonne,
  uno dopo l'altro, cosi' una serata intera sta in una pagina.
- Un film iniziato meno di due ore e mezza fa resta nel tile, con l'orario nel colore d'accento: la guida non ha gli
  orari di fine. Il tile segue l'orologio dello schermo, quindi un film esce dalla lista da solo.
- Quando non resta nessun film lo dice, e quando la guida e' vecchia lo scrive accanto alla data.
- Non servono chiavi ne' account.
- Provato su un vero Waveshare ESP32-S3-Touch-LCD-7B (7 pollici, firmware Tessera 0.55.0). Nel banco di prova dei
  plugin (`tools/preview.py`) si disegna senza avvisi anche sul Waveshare da 5 pollici e sul CYD (2,8 pollici).
