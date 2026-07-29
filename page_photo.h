#pragma once
#include "util.h"

void renderPhoto() {
  if (!gPhotoId.length() || !gPhotoSprite.getBuffer()) {
    d.fillScreen(TFT_WHITE);
    drawHeader("Photo", "", false);
    drawCentered("No photo yet", H / 2 - 20, &fonts::DejaVu40);
    d.display();
    return;
  }

  gPhotoSprite.pushSprite(0, 0);   // pre-decoded + dithered by fetchPhoto()
  d.display();
}
