#include <FastLED.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "secrets.h"
#include "page.h"

// Spectacle Halloween avec interface web (Heltec WiFi LoRa 32 V3).
// 8 scenes, chacune activable et de duree reglable; fondu de 1 s entre les scenes.
// Les scenes sont des fonctions "sans etat" du temps: render(scene, tc, len, buf),
// ce qui permet de rendre deux scenes en meme temps pour le fondu enchaine.
#define DATA_PIN 4
#define NUM_LEDS 100
#define LED_TYPE WS2812B
#define COLOR_ORDER BRG

#define NUM_SCENES 8
#define FADE_MS 1000
#define HUE_RED 0

CRGB leds[NUM_LEDS];
CRGB next[NUM_LEDS];

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, RST_OLED, SCL_OLED, SDA_OLED);
WebServer server(80);

// Etat modifiable depuis l'interface web
bool sceneEnabled[NUM_SCENES] = {true, true, true, true, true, true, true, true};
uint16_t sceneSecs[NUM_SCENES] = {15, 15, 15, 15, 15, 15, 15, 15};
int8_t lockScene = -1;  // -1 = spectacle normal, sinon scene jouee en boucle
uint8_t brightness = 170;
uint8_t hueOrange = 20, huePurple = 200, hueGreen = 96;
CRGBPalette16 finalePal;

uint8_t curScene = 0;
uint32_t sceneStart = 0;

const char *SCENE_NAMES[NUM_SCENES] = {
  "Flammes", "Vague maudite", "Poison", "Apparition",
  "Orage hante", "Zombies", "Chaos", "Final maudit"
};

static void updatePalette() {
  finalePal = CRGBPalette16(CHSV(hueOrange, 255, 255), CHSV(huePurple, 255, 255),
                            CHSV(hueGreen, 255, 255), CHSV(HUE_RED, 255, 255));
}

// Hash 8 bits deterministe (motifs pseudo-aleatoires stables dans le temps)
static uint8_t h8(uint32_t x) {
  x = (x ^ 61) ^ (x >> 16);
  x *= 9;
  x ^= x >> 4;
  x *= 0x27d4eb2d;
  x ^= x >> 15;
  return x & 0xFF;
}

// Couleur de flamme vivante: teinte rouge->jaune et intensite qui scintillent.
static CRGB flameColor(int i, uint32_t tc, float level) {
  uint32_t tn = tc + (int32_t)(600 * sinf(tc / 1500.0f));  // vitesse qui varie
  uint8_t hue = qsub8(inoise8(i * 25, tn / 4), 40) / 5;     // 0..~36
  uint8_t v = 100 + scale8(inoise8(i * 60 + 1000, tn * 2 / 3), 155);
  return CHSV(hue, 255, (uint8_t)(v * level));
}

// 1. Le reveil des flammes: les LED s'allument une a une, de braises a feu vif
static void sceneFlames(uint32_t tc, uint32_t len, CRGB *b) {
  float p = tc / (float)len;
  for (int i = 0; i < NUM_LEDS; i++) {
    float th = h8(i) / 255.0f * 0.8f;
    float reveal = constrain((p * 1.25f - th) / 0.2f, 0.0f, 1.0f);
    b[i] = flameColor(i, tc, 0.5f + 0.5f * reveal);
  }
}

// 2. La vague maudite: tete rouge, traine orange qui s'efface, 3 passages
static void sceneWave(uint32_t tc, uint32_t len, CRGB *b) {
  const int TAIL = 30;
  uint32_t period = len / 3;
  float pos = (tc % period) / (float)period * (NUM_LEDS + TAIL);
  for (int i = 0; i < NUM_LEDS; i++) {
    b[i] = CHSV(HUE_RED, 255, 30);
    float d = pos - i;
    if (d < 0 || d >= TAIL) continue;
    CRGB c = d < 6 ? CRGB(CHSV(HUE_RED, 255, 255))
                   : CRGB(CHSV(hueOrange, 255, (uint8_t)(255 * (1.0f - (d - 6) / (TAIL - 6)))));
    b[i] |= c;
  }
}

// 3. Le poison: vert toxique et violet qui se poursuivent en vagues
static void scenePoison(uint32_t tc, uint32_t len, CRGB *b) {
  uint32_t tn = tc + (int32_t)(1500 * sinf(tc / 2000.0f));
  int16_t span = (int8_t)(huePurple - hueGreen);  // plus court chemin sur la roue des teintes
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t m = sin8(i * 4 - tn / 7);
    uint8_t hue = hueGreen + span * m / 255;
    uint8_t v = 110 + scale8(sin8(i * 10 + tn / 5), 145);
    b[i] = CHSV(hue, 255, v);
  }
}

// 4. L'apparition: traine blanche glaciale sur fond violet constant (2 passages)
static void sceneGhost(uint32_t tc, uint32_t len, CRGB *b) {
  const int TAIL = 40;
  uint32_t period = len / 2;
  bool rev = (tc / period) & 1;
  float pos = (tc % period) / (float)period * (NUM_LEDS + TAIL);
  for (int i = 0; i < NUM_LEDS; i++) {
    b[i] = CHSV(huePurple, 255, 70);
    int j = rev ? NUM_LEDS - 1 - i : i;
    float d = pos - j;
    if (d < 0 || d >= TAIL) continue;
    float f = 1.0f - d / TAIL;
    b[i] |= CHSV(145, d < 5 ? 30 : 90, (uint8_t)(255 * f * f));
  }
}

// 5. L'orage hante: eclairs blancs et rouges qui parcourent la bande, fond violet
static void sceneStorm(uint32_t tc, uint32_t len, CRGB *b) {
  for (int i = 0; i < NUM_LEDS; i++) b[i] = CHSV(huePurple, 255, 60);
  uint32_t slot = tc / 350;
  uint32_t tl = tc % 350;
  if ((h8(slot) & 3) == 0 || tl > 220) return;
  if ((h8(tl / 25 + slot * 11) & 3) == 0) return;  // scintillement de l'eclair
  int dir = (h8(slot * 3 + 7) & 1) ? 1 : -1;
  int start = h8(slot * 3 + 1) * NUM_LEDS / 255;
  int bolt = 15 + h8(slot * 5 + 2) % 25;
  int head = start + dir * (int)(tl * 0.15f);
  bool red = h8(slot * 13) & 1;
  for (int k = 0; k < bolt; k++) {
    int i = head - dir * k;
    if (i < 0 || i >= NUM_LEDS) continue;
    uint8_t v = 255 - 255 * k / bolt;
    b[i] |= red ? CRGB(CHSV(HUE_RED, 255, v)) : CRGB(v, v, v);
  }
}

// 6. L'invasion zombie: vagues vertes qui gagnent du terrain, touches de rouge sombre
static void sceneZombie(uint32_t tc, uint32_t len, CRGB *b) {
  float reach = tc / (float)len * 1.1f * NUM_LEDS;
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t w = sin8(i * 6 - tc / 5);
    uint8_t v = i < reach ? 50 + scale8(w, 205) : 25;
    b[i] = CHSV(hueGreen, 255, v);
    if (inoise8(i * 45, tc / 12 + 9000) > 185) b[i] = CHSV(HUE_RED, 255, 90);
  }
}

// 7. Le chaos: 4 couleurs en blocs qui defilent de facon desordonnee
static void sceneChaos(uint32_t tc, uint32_t len, CRGB *b) {
  const uint8_t hues[4] = {hueOrange, huePurple, hueGreen, HUE_RED};
  uint32_t off = tc / 40;
  for (int i = 0; i < NUM_LEDS; i++) {
    b[i] = CHSV(hues[h8((i + off) / 3) & 3], 255, 255);
  }
}

// 8. Le final maudit: couleurs qui accelerent, eclair blanc, retour aux flammes.
// Le minutage est defini pour 16 s puis etire a la duree reelle (tcv).
static void sceneFinale(uint32_t tc, uint32_t len, CRGB *b) {
  float tcv = tc * 16000.0f / len;
  float ph = tcv < 9500 ? tcv * tcv / 4000.0f
                        : 9500.0f * 9500.0f / 4000.0f + (tcv - 9500) * (2 * 9500.0f / 4000.0f);
  float mix = constrain((tcv - 10500) / 5500.0f, 0.0f, 1.0f);
  float flashPos = (tcv - 9500) / 1500.0f * (NUM_LEDS + 20);
  bool flashing = tcv >= 9500 && tcv < 11000;
  for (int i = 0; i < NUM_LEDS; i++) {
    CRGB c = ColorFromPalette(finalePal, (uint8_t)(i * 4 + ph / 16.0f), 255, LINEARBLEND);
    if (mix > 0) nblend(c, flameColor(i, tc, 0.5f), (uint8_t)(mix * 255));
    if (flashing) {
      float d = flashPos - i;
      if (d >= 0 && d < 20) {
        uint8_t v = 255 - (uint8_t)(255 * d / 20);
        c |= CRGB(v, v, v);
      }
    }
    b[i] = c;
  }
}

static void render(uint8_t scene, uint32_t tc, uint32_t len, CRGB *b) {
  switch (scene) {
    case 0: sceneFlames(tc, len, b); break;
    case 1: sceneWave(tc, len, b); break;
    case 2: scenePoison(tc, len, b); break;
    case 3: sceneGhost(tc, len, b); break;
    case 4: sceneStorm(tc, len, b); break;
    case 5: sceneZombie(tc, len, b); break;
    case 6: sceneChaos(tc, len, b); break;
    default: sceneFinale(tc, len, b); break;
  }
}

static uint32_t sceneMs(uint8_t s) { return sceneSecs[s] * 1000UL; }

static int enabledCount() {
  int n = 0;
  for (int i = 0; i < NUM_SCENES; i++) n += sceneEnabled[i];
  return n;
}

// Prochaine scene a jouer: la scene verrouillee, ou la suivante activee.
static uint8_t nextScene(uint8_t s) {
  if (lockScene >= 0) return lockScene;
  for (int k = 1; k <= NUM_SCENES; k++) {
    uint8_t n = (s + k) % NUM_SCENES;
    if (sceneEnabled[n]) return n;
  }
  return s;
}

// ---------- Interface web ----------

static String stateJson() {
  uint32_t D = sceneMs(curScene);
  uint32_t t = min<uint32_t>(millis() - sceneStart, D);
  String s;
  s.reserve(1000);
  s += "{\"s\":" + String(curScene) + ",\"t\":" + String(t) + ",\"d\":" + String(D);
  s += ",\"b\":" + String(brightness) + ",\"l\":" + String(lockScene) + ",\"n\":" + String(NUM_LEDS);
  s += ",\"en\":[";
  for (int i = 0; i < NUM_SCENES; i++) s += String(sceneEnabled[i] ? 1 : 0) + (i < NUM_SCENES - 1 ? "," : "");
  s += "],\"du\":[";
  for (int i = 0; i < NUM_SCENES; i++) s += String(sceneSecs[i]) + (i < NUM_SCENES - 1 ? "," : "");
  s += "],\"c\":[";
  const uint8_t hues[3] = {hueOrange, huePurple, hueGreen};
  char hex[8];
  for (int i = 0; i < 3; i++) {
    CRGB c = CHSV(hues[i], 255, 255);
    snprintf(hex, sizeof hex, "%02x%02x%02x", c.r, c.g, c.b);
    s += String("\"") + hex + "\"" + (i < 2 ? "," : "");
  }
  s += "],\"px\":\"";
  for (int i = 0; i < NUM_LEDS; i++) {
    snprintf(hex, sizeof hex, "%02x%02x%02x", leds[i].r, leds[i].g, leds[i].b);
    s += hex;
  }
  s += "\"}";
  return s;
}

static void handleSet() {
  if (server.hasArg("b")) {
    brightness = constrain(server.arg("b").toInt(), 10, 255);
    FastLED.setBrightness(brightness);
  }
  if (server.hasArg("en") && server.hasArg("v")) {
    int i = server.arg("en").toInt();
    bool v = server.arg("v").toInt() != 0;
    if (i >= 0 && i < NUM_SCENES && (v || !sceneEnabled[i] || enabledCount() > 1)) sceneEnabled[i] = v;
  }
  if (server.hasArg("du") && server.hasArg("s")) {
    int i = server.arg("du").toInt();
    if (i >= 0 && i < NUM_SCENES) sceneSecs[i] = constrain(server.arg("s").toInt(), 5, 60);
  }
  if (server.hasArg("lock")) {
    int i = server.arg("lock").toInt();
    lockScene = (i >= 0 && i < NUM_SCENES) ? i : -1;
    if (lockScene >= 0) {
      curScene = lockScene;
      sceneStart = millis();
    }
  }
  if (server.hasArg("col") && server.hasArg("c")) {
    int i = server.arg("col").toInt();
    uint32_t rgb = strtoul(server.arg("c").c_str(), nullptr, 16);
    uint8_t h = rgb2hsv_approximate(CRGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255)).h;
    if (i == 0) hueOrange = h;
    else if (i == 1) huePurple = h;
    else if (i == 2) hueGreen = h;
    updatePalette();
  }
  server.send(200, "application/json", stateJson());
}

// ---------- OLED ----------

static void drawOled() {
  uint32_t D = sceneMs(curScene);
  uint32_t t = min<uint32_t>(millis() - sceneStart, D);
  bool wifi = WiFi.status() == WL_CONNECTED;
  char buf[24];
  oled.clearBuffer();
  oled.setFont(u8g2_font_helvB10_tr);
  snprintf(buf, sizeof buf, lockScene >= 0 ? "%d/%d boucle" : "%d/%d", curScene + 1, NUM_SCENES);
  oled.drawStr(0, 13, buf);
  snprintf(buf, sizeof buf, "%d/%ds", (int)(t / 1000), (int)(D / 1000));
  oled.drawStr(128 - oled.getStrWidth(buf), 13, buf);
  oled.drawHLine(0, 17, 128);
  oled.drawStr(0, 35, SCENE_NAMES[curScene]);
  oled.drawFrame(0, 40, 128, 6);
  oled.drawBox(0, 40, 128 * t / D, 6);
  oled.setFont(u8g2_font_6x10_tr);
  if (wifi) {
    snprintf(buf, sizeof buf, "http://%s", WiFi.localIP().toString().c_str());
    oled.drawStr(0, 60, buf);
  } else {
    oled.drawStr(0, 60, "WiFi: connexion...");
  }
  oled.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);  // alimente l'OLED
  delay(50);
  oled.begin();

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(brightness);
  updatePalette();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("halloween");
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  server.on("/", []() { server.send_P(200, "text/html; charset=utf-8", PAGE); });
  server.on("/state", []() { server.send(200, "application/json", stateJson()); });
  server.on("/set", handleSet);
  server.begin();

  sceneStart = millis();
}

void loop() {
  static uint32_t lastKey = UINT32_MAX;
  static bool mdnsUp = false;

  uint32_t now = millis();
  uint32_t D = sceneMs(curScene);
  uint32_t t = now - sceneStart;
  if (t >= D) {
    curScene = nextScene(curScene);
    sceneStart += D;
    D = sceneMs(curScene);
    t = now - sceneStart;
    if (t >= D) {  // duree raccourcie pendant la scene: on repart proprement
      sceneStart = now;
      t = 0;
    }
  }

  // L'horloge de chaque scene demarre FADE_MS avant sa frontiere, pour que le
  // fondu entrant (horloge 0..FADE_MS) se raccorde sans saut a la scene seule.
  render(curScene, t + FADE_MS, D + FADE_MS, leds);
  if (t >= D - FADE_MS) {
    uint8_t n = nextScene(curScene);
    render(n, t - (D - FADE_MS), sceneMs(n) + FADE_MS, next);
    uint8_t amt = (t - (D - FADE_MS)) * 255 / FADE_MS;
    for (int i = 0; i < NUM_LEDS; i++) nblend(leds[i], next[i], amt);
  }
  FastLED.show();

  if (!mdnsUp && WiFi.status() == WL_CONNECTED) {
    mdnsUp = MDNS.begin("halloween");
    Serial.println(WiFi.localIP());
  }
  server.handleClient();

  uint32_t key = (curScene * 1000 + t / 1000) * 4 + (lockScene >= 0) * 2 + (WiFi.status() == WL_CONNECTED);
  if (key != lastKey) {
    lastKey = key;
    drawOled();
  }
  delay(5);
}
