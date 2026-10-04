# 🎃 Halloween LED

Un spectacle d'Halloween pour une bande de 100 LED adressables **WS2815** (12 V), piloté par une carte
**Heltec WiFi LoRa 32 V3** (ESP32-S3) avec écran OLED intégré et une interface web
pour tout contrôler depuis un téléphone.

Huit animations s'enchaînent en boucle avec un fondu d'une seconde entre chacune.
Depuis le téléphone, on peut activer ou désactiver chaque animation, régler sa durée,
en faire jouer une seule en boucle, changer la luminosité et les couleurs, et voir un
aperçu en direct de la bande.

## Fonctionnalités

- **8 scènes animées** (voir le tableau plus bas), avec fondu enchaîné entre les scènes
- **Interface web** sur `http://halloween.local` (ou l'adresse IP affichée sur l'OLED)
  - aperçu en direct des 100 LED
  - activer ou désactiver chaque scène, durée réglable de 5 à 60 s
  - mode « ↻ Boucle » pour jouer une seule scène en continu
  - luminosité
  - choix des trois couleurs principales (orange, violet, vert)
- **Écran OLED** : scène en cours, temps écoulé, barre de progression et adresse web
- Aucune dépendance Internet : la page est servie directement par la carte

## Les scènes

| # | Scène | Description |
|---|-------|-------------|
| 1 | 🔥 Le réveil des flammes | Les LED s'allument une à une, des braises jusqu'au feu vif |
| 2 | 🩸 La vague maudite | Une tête rouge suivie d'une traînée orange parcourt la bande, trois fois |
| 3 | ☠️ Le poison | Vert toxique et violet se poursuivent en vagues |
| 4 | 👻 L'apparition | Une traînée blanche glaciale passe sur un fond violet, aller puis retour |
| 5 | ⚡ L'orage hanté | Éclairs blancs et rouges sur un ciel violet |
| 6 | 🧟 L'invasion zombie | Des vagues vertes gagnent du terrain, avec des touches de rouge sombre |
| 7 | 🎃 Le chaos | Blocs des quatre couleurs qui défilent dans le désordre |
| 8 | 💀 Le final maudit | Les couleurs accélèrent, un éclair blanc traverse la bande, puis retour aux flammes |

Avec les réglages par défaut (8 scènes × 15 s), un cycle complet dure 2 minutes.

## Matériel

| Pièce | Notes |
|-------|-------|
| Heltec WiFi LoRa 32 V3 | ESP32-S3, OLED SSD1306 128×64 intégré |
| Bande de 100 LED WS2815 (12 V) | Pilotée en mode `WS2812B` par FastLED, dont le signal est compatible. Ordre des couleurs **BRG** sur la bande utilisée (voir [Personnalisation](#personnalisation)) |
| Alimentation 12 V | Pour la bande. Choisir l'intensité selon la fiche technique de la bande |
| Câble USB-C | Programmation et alimentation de la carte |

## Branchement

```
Heltec V3                    Bande WS2815
---------                    ------------
GPIO 4  ───────────────────► DIN
GND     ──────────────────── GND ──┐
                                   ├── Alimentation 12 V
                             +12V ─┘
```

- La broche de données est **GPIO 4** (modifiable via `DATA_PIN`), branchée
  directement sur l'entrée DIN de la bande, sans autre composant : le signal de
  3,3 V de l'ESP32-S3 suffit pour la bande utilisée.
- **Les masses (GND) de la carte et de l'alimentation doivent être reliées.**
- La carte est alimentée par USB. **Ne jamais relier le +12 V à la carte** : seule
  la bande reçoit le 12 V.

## Installation

### 1. Préparer l'environnement

Avec l'**Arduino IDE** (2.x) :

1. *Fichier → Préférences → URL de gestionnaire de cartes supplémentaires* :
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. *Gestionnaire de cartes* : installer **esp32** par Espressif (testé avec 3.3.12).
3. *Gestionnaire de bibliothèques* : installer **FastLED** (testé avec 3.10.5) et
   **U8g2** (testé avec 2.36.19).
4. Carte : **Heltec WiFi LoRa 32(V3) / Wireless shell(V3) / ...**

Ou avec **arduino-cli** :

```bash
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install FastLED U8g2
```

### 2. Configurer le Wi-Fi

```bash
cp halloween_web/secrets.example.h halloween_web/secrets.h
```

Puis modifier `halloween_web/secrets.h` avec le nom et le mot de passe de votre
réseau. Ce fichier est ignoré par git et ne sera jamais publié.

> L'ESP32 ne se connecte qu'aux réseaux **2,4 GHz**.

### 3. Compiler et téléverser

Dans l'Arduino IDE, ouvrir `halloween_web/halloween_web.ino` et cliquer sur
*Téléverser*. Avec arduino-cli :

```bash
arduino-cli compile --fqbn esp32:esp32:heltec_wifi_lora_32_V3 halloween_web
arduino-cli upload  --fqbn esp32:esp32:heltec_wifi_lora_32_V3 -p /dev/ttyUSB0 halloween_web
```

(Remplacer `/dev/ttyUSB0` par le port de la carte : `arduino-cli board list`.)

## Utilisation

1. Brancher la carte. Le spectacle démarre immédiatement, même sans Wi-Fi.
2. Une fois connectée, l'OLED affiche l'adresse de la page, par exemple
   `http://192.168.1.42`.
3. Ouvrir cette adresse, ou `http://halloween.local`, sur un téléphone ou un
   ordinateur du même réseau.

Les réglages faits depuis la page sont gardés en mémoire vive : ils reviennent aux
valeurs par défaut quand la carte redémarre.

## Personnalisation

Les constantes en haut de `halloween_web.ino` :

| Constante | Défaut | Rôle |
|-----------|--------|------|
| `DATA_PIN` | `4` | Broche reliée à DIN |
| `NUM_LEDS` | `100` | Nombre de LED de la bande |
| `LED_TYPE` | `WS2812B` | Type de puce. Fonctionne avec la WS2815 utilisée; FastLED propose aussi `WS2815` (voir la doc FastLED) |
| `COLOR_ORDER` | `BRG` | Ordre des couleurs. Si le rouge s'affiche en bleu ou en vert, essayer `GRB` ou `RGB` |
| `FADE_MS` | `1000` | Durée du fondu entre deux scènes (ms) |
| `brightness` | `170` | Luminosité au démarrage (10 à 255) |
| `sceneSecs[]` | `15` partout | Durée de chaque scène au démarrage (s) |

Le nom réseau `halloween` (pour `halloween.local`) est défini dans `setup()`.

### Ajouter une scène

Chaque scène est une fonction **sans état** du temps :

```cpp
static void maScene(uint32_t tc, uint32_t len, CRGB *b);
```

- `tc` : temps écoulé depuis le début de la scène (ms)
- `len` : durée totale de la scène (ms)
- `b` : tableau de `NUM_LEDS` couleurs à remplir

Comme l'image ne dépend que de `tc`, la boucle principale peut calculer deux scènes
à la fois et les mélanger pour le fondu. Pour ajouter une scène : écrire la
fonction, l'ajouter dans `render()`, augmenter `NUM_SCENES`, compléter
`SCENE_NAMES`, `sceneEnabled[]` et `sceneSecs[]`, et ajouter une entrée au tableau
`SC` dans `page.h` (la boucle `for(let i=0;i<8;i++)` de `page.h` doit aussi suivre).

## API HTTP

La page web n'utilise que ces deux points d'accès, qui peuvent aussi servir à
automatiser le spectacle (avec `curl`, par exemple).

### `GET /state`

Renvoie l'état courant en JSON :

| Clé | Sens |
|-----|------|
| `s` | Scène en cours (0 à 7) |
| `t`, `d` | Temps écoulé et durée de la scène (ms) |
| `b` | Luminosité (10 à 255) |
| `l` | Scène verrouillée en boucle, ou `-1` |
| `n` | Nombre de LED |
| `en` | Scènes activées (`1`/`0`) |
| `du` | Durée de chaque scène (s) |
| `c` | Les trois couleurs, en hexadécimal |
| `px` | Couleur actuelle de chaque LED, 6 caractères hexadécimaux par LED |

### `GET /set?...`

Modifie un réglage et renvoie le nouvel état. Paramètres possibles :

| Paramètres | Effet |
|------------|-------|
| `b=170` | Luminosité (10 à 255) |
| `en=2&v=0` | Désactive la scène 2 (`v=1` pour la réactiver). Au moins une scène reste active |
| `du=2&s=30` | Durée de la scène 2 : 30 s (5 à 60) |
| `lock=4` | Joue la scène 4 en boucle; `lock=-1` reprend le spectacle |
| `col=0&c=ff7a1a` | Couleur 0 (orange), 1 (violet) ou 2 (vert), en hexadécimal sans `#` |

Exemple :

```bash
curl "http://halloween.local/set?lock=4"
```

## Dépannage

| Symptôme | Piste |
|----------|-------|
| La bande reste éteinte | Vérifier la masse commune, la broche `DATA_PIN`, le sens de la bande (DIN et non DOUT) |
| Couleurs inversées | Changer `COLOR_ORDER` |
| OLED noir | L'OLED est alimenté par `Vext` (GPIO 36), mis à `LOW` dans `setup()` |
| « WiFi: connexion... » reste affiché | Vérifier `secrets.h` et que le réseau est en 2,4 GHz |
| `halloween.local` ne répond pas | Utiliser l'adresse IP affichée sur l'OLED (le mDNS n'est pas pris en charge partout, notamment sur certains Android) |
| La carte n'apparaît pas comme port série | Essayer un autre câble USB-C (certains ne transportent que le courant) |

## Licence

[MIT](LICENSE)
