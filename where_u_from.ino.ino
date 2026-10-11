#include "TFT_eSPI.h"
#include "SPI.h"




float loopLength = 60.0f; 

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

struct Point {
  float x, y;
};

struct ColorRGB {
  float r, g, b;
};


struct Cloud {
  float x, y, speed;
};
Cloud clouds[3];

struct Star {
  int x, y;
  float phase;
};
Star stars[40];

struct ShootingStar {
  float x, y;
  bool active;
};
ShootingStar ss = {0, 0, false};


Point fillPath[350];
int fillCount = 0;


inline uint16_t colorRGB565(ColorRGB c) {
  uint8_t r = (uint8_t)c.r;
  uint8_t g = (uint8_t)c.g;
  uint8_t b = (uint8_t)c.b;
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}


ColorRGB blend(ColorRGB c1, ColorRGB c2, float t) {
  return {
    c1.r + (c2.r - c1.r) * t,
    c1.g + (c2.g - c1.g) * t,
    c1.b + (c2.b - c1.b) * t
  };
}


void addPoint(Point* arr, int &count, float x, float y) {
  if (count < 350) {
    arr[count].x = x;
    arr[count].y = y;
    count++;
  }
}

void addCubicBezier(Point* arr, int &count, Point p0, Point p1, Point p2, Point p3, int steps = 10) {
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / (float)steps;
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    float x = uuu * p0.x + 3.0f * uu * t * p1.x + 3.0f * u * tt * p2.x + ttt * p3.x;
    float y = uuu * p0.y + 3.0f * uu * t * p1.y + 3.0f * u * tt * p2.y + ttt * p3.y;
    addPoint(arr, count, x, y);
  }
}

void initSVGGeometry() {
  fillCount = 0;
  addCubicBezier(fillPath, fillCount, {0.0f, 125.122f}, {22.7039f, 121.938f}, {69.0f, 115.0f}, {74.5f, 113.605f});
  addCubicBezier(fillPath, fillCount, {74.5f, 113.605f}, {80.5913f, 109.672f}, {92.0f, 95.0f}, {98.0f, 87.6218f});
  addCubicBezier(fillPath, fillCount, {98.0f, 87.6218f}, {100.215f, 85.9364f}, {104.0f, 85.5f}, {106.0f, 86.6218f});
  addCubicBezier(fillPath, fillCount, {106.0f, 86.6218f}, {106.554f, 87.7453f}, {98.0f, 105.0f}, {91.9229f, 113.605f});
  addCubicBezier(fillPath, fillCount, {91.9229f, 113.605f}, {88.462f, 116.414f}, {60.0f, 125.0f}, {49.284f, 130.261f});
  addCubicBezier(fillPath, fillCount, {49.284f, 130.261f}, {71.0f, 133.5f}, {43.21f, 134.5f}, {32.5f, 136.0f});
  addCubicBezier(fillPath, fillCount, {32.5f, 136.0f}, {22.5081f, 139.184f}, {20.0f, 140.0f}, {8.5f, 143.0f});
  addCubicBezier(fillPath, fillCount, {8.5f, 143.0f}, {30.0f, 144.5f}, {7.38337f, 145.0f}, {0.0f, 145.627f});
  addPoint(fillPath, fillCount, 0.0f, 145.627f);
  addPoint(fillPath, fillCount, 0.0f, 125.122f);
}

void drawFilledPolygon(TFT_eSprite &canvas, const Point* pts, int numPts, uint16_t color) {
  if (numPts < 3) return;

  float minY = pts[0].y;
  float maxY = pts[0].y;
  for (int i = 1; i < numPts; i++) {
    if (pts[i].y < minY) minY = pts[i].y;
    if (pts[i].y > maxY) maxY = pts[i].y;
  }

  int startY = max(0, (int)minY);
  int endY = min(239, (int)maxY);

  for (int y = startY; y <= endY; y++) {
    float nodeX[64];
    int nodes = 0;
    int j = numPts - 1;

    for (int i = 0; i < numPts; i++) {
      if ((pts[i].y < y && pts[j].y >= y) || (pts[j].y < y && pts[i].y >= y)) {
        if (pts[j].y != pts[i].y) {
          nodeX[nodes++] = pts[i].x + (float)(y - pts[i].y) / (pts[j].y - pts[i].y) * (pts[j].x - pts[i].x);
        }
      }
      j = i;
    }

    for (int i = 0; i < nodes - 1; i++) {
      for (int k = i + 1; k < nodes; k++) {
        if (nodeX[i] > nodeX[k]) {
          float temp = nodeX[i];
          nodeX[i] = nodeX[k];
          nodeX[k] = temp;
        }
      }
    }

    for (int i = 0; i < nodes; i += 2) {
      if (nodeX[i] >= 135) break;
      if (nodeX[i + 1] > 0) {
        int x0 = max(0, (int)nodeX[i]);
        int x1 = min(134, (int)nodeX[i + 1]);
        if (x1 >= x0) canvas.drawFastHLine(x0, y, x1 - x0 + 1, color);
      }
    }
  }
}


void drawClouds(TFT_eSprite &canvas, ColorRGB cloudCol) {
  uint16_t c = colorRGB565(cloudCol);
  for (int i = 0; i < 3; i++) {
    canvas.fillCircle(clouds[i].x, clouds[i].y, 10, c);
    canvas.fillCircle(clouds[i].x - 8, clouds[i].y + 4, 7, c);

    clouds[i].x += clouds[i].speed;
    
    if (clouds[i].x > 150) {
      clouds[i].x = -20;
      clouds[i].y = random(15, 225);
    }
  }
}


void drawAurora(TFT_eSprite &canvas, float timeStr, float intensity, int colorMode) {
  if (intensity <= 0.01f) return;

  ColorRGB cTop, cMid, cBot;

  
  if (colorMode == 0) {
    cTop = {15, 40, 70}; cMid = {20, 90, 50}; cBot = {10, 110, 40}; 
  } else if (colorMode == 1) {
    cTop = {60, 15, 40}; cMid = {100, 20, 50}; cBot = {130, 30, 30}; 
  } else {
    cTop = {40, 10, 70}; cMid = {70, 15, 80}; cBot = {20, 100, 60}; 
  }

  
  cTop.r *= intensity; cTop.g *= intensity; cTop.b *= intensity;
  cMid.r *= intensity; cMid.g *= intensity; cMid.b *= intensity;
  cBot.r *= intensity; cBot.g *= intensity; cBot.b *= intensity;

  for (int x = 0; x < 135; x += 2) {
    float wave = sin(x * 0.02f + timeStr) + cos(x * 0.04f - timeStr * 0.7f);
    int yStart = 10 + wave * 25;
    if (yStart < 0) yStart = 0;
    int height = 240 - yStart;

    
    for (int yOffset = 0; yOffset < height; yOffset += 3) {
      float t = (float)yOffset / (float)height;
      ColorRGB c;
      if (t < 0.5f) {
        c = blend(cTop, cMid, t * 2.0f);
      } else {
        c = blend(cMid, cBot, (t - 0.5f) * 2.0f);
      }

      uint16_t col = colorRGB565(c);
      int chunkHeight = 3;
      if (yOffset + chunkHeight > height) chunkHeight = height - yOffset;

      canvas.drawFastVLine(x, yStart + yOffset, chunkHeight, col);
      canvas.drawFastVLine(x + 1, yStart + yOffset, chunkHeight, col);
    }
  }
}

void setup() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(0);
  initSVGGeometry();

  spr.setColorDepth(16);
  spr.createSprite(135, 240);

  randomSeed(analogRead(0));

  
  for (int i = 0; i < 40; i++) {
    stars[i] = {(int)random(0, 135), (int)random(0, 240), random(0, 100) / 100.0f};
  }

  
  clouds[0] = {-10, 30, 0.15f};
  clouds[1] = {60, 110, 0.10f};
  clouds[2] = {120, 190, 0.20f};
}

void loop() {
  uint32_t msCycle = (uint32_t)(loopLength * 1000.0f);
  float frac = (millis() % msCycle) / (float)msCycle;
  int cycleCount = millis() / msCycle;
  bool isAuroraNight = (cycleCount % 3 == 0);
  int auroraColorMode = cycleCount % 3;

  ColorRGB skyNight      = {5, 10, 20};
  ColorRGB skySunrise    = {95, 56, 164};
  ColorRGB skyDay        = {121, 181, 241};
  ColorRGB skySunsetTop  = {56, 61, 124};
  ColorRGB skySunsetBot  = {226, 158, 103};

  ColorRGB planeNight    = {28, 28, 28};
  ColorRGB planeSunrise  = {110, 110, 110};
  ColorRGB planeDay      = {223, 223, 223};
  ColorRGB planeSunset   = {176, 176, 176};

  ColorRGB cloudNight    = {40, 40, 50};
  ColorRGB cloudSunrise  = {255, 160, 120};
  ColorRGB cloudDay      = {245, 245, 255};
  ColorRGB cloudSunset   = {255, 120, 100};

  ColorRGB topColor, botColor, planeColor, cloudColor;
  float nightIntensity = 0.0f;

  if (frac < 0.4f) {
    topColor = botColor = skyNight;
    planeColor = planeNight;
    cloudColor = cloudNight;
    nightIntensity = 1.0f;
  } else if (frac < 0.5f) {
    float t = (frac - 0.4f) / 0.1f;
    nightIntensity = 1.0f - t;
    if (t < 0.5f) {
      topColor = botColor = blend(skyNight, skySunrise, t * 2.0f);
      planeColor = blend(planeNight, planeSunrise, t * 2.0f);
      cloudColor = blend(cloudNight, cloudSunrise, t * 2.0f);
    } else {
      topColor = botColor = blend(skySunrise, skyDay, (t - 0.5f) * 2.0f);
      planeColor = blend(planeSunrise, planeDay, (t - 0.5f) * 2.0f);
      cloudColor = blend(cloudSunrise, cloudDay, (t - 0.5f) * 2.0f);
    }
  } else if (frac < 0.9f) {
    topColor = botColor = skyDay;
    planeColor = planeDay;
    cloudColor = cloudDay;
    nightIntensity = 0.0f;
  } else {
    float t = (frac - 0.9f) / 0.1f;
    nightIntensity = t;
    if (t < 0.5f) {
      topColor = blend(skyDay, skySunsetTop, t * 2.0f);
      botColor = blend(skyDay, skySunsetBot, t * 2.0f);
      planeColor = blend(planeDay, planeSunset, t * 2.0f);
      cloudColor = blend(cloudDay, cloudSunset, t * 2.0f);
    } else {
      topColor = blend(skySunsetTop, skyNight, (t - 0.5f) * 2.0f);
      botColor = blend(skySunsetBot, skyNight, (t - 0.5f) * 2.0f);
      planeColor = blend(planeSunset, planeNight, (t - 0.5f) * 2.0f);
      cloudColor = blend(cloudSunset, cloudNight, (t - 0.5f) * 2.0f);
    }
  }

  
  for (int y = 0; y < 240; y++) {
    ColorRGB rowColor = blend(topColor, botColor, y / 239.0f);
    spr.drawFastHLine(0, y, 135, colorRGB565(rowColor));
  }

  
  if (nightIntensity > 0.05f) {
    for (int i = 0; i < 40; i++) {
      float twinkle = (sin(millis() * 0.002f + stars[i].phase * 6.28f) + 1.0f) / 2.0f;
      float brightness = nightIntensity * twinkle;
      uint8_t c = (uint8_t)(255 * brightness);
      spr.drawPixel(stars[i].x, stars[i].y, spr.color565(c, c, c));
    }

    if (nightIntensity > 0.8f) {
      if (!ss.active && random(100) < 2) {
        ss.x = random(30, 135);
        ss.y = random(0, 150);
        ss.active = true;
      }

      if (ss.active) {
        for (int i = 0; i < 6; i++) {
          uint8_t fade = 255 - (i * 40);
          spr.drawPixel(ss.x + i, ss.y - i, spr.color565(fade, fade, fade));
        }
        ss.x -= 4;
        ss.y += 4;
        if (ss.y > 240 || ss.x < 0) ss.active = false;
      }
    }
  }

  
  if (isAuroraNight) {
    float auroraIntensity = 0.0f;
    
    if (nightIntensity > 0.85f) {
      float t = (nightIntensity - 0.85f) / 0.15f;
      auroraIntensity = t * t;
      if (auroraIntensity > 0.01f) {
        float animTime = millis() * 0.001f;
        drawAurora(spr, animTime, auroraIntensity, auroraColorMode);
      }
    }
  }

  
  drawClouds(spr, cloudColor);

  
  uint16_t pathFill = colorRGB565(planeColor);
  drawFilledPolygon(spr, fillPath, fillCount, pathFill);

  
  if ((millis() % 1000) < 150) {
    spr.fillCircle(92, 112, 3, spr.color565(255, 20, 20));
  }

  
  spr.pushSprite(0, 0);
  delay(16);
}