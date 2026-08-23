#ifndef __DISPLAY_H
#define __DISPLAY_H

#include "lvgl.h"
#include "Arduino.h"
#include "TFT_eSPI.h"

#ifdef FNK0104N_3P5_320x480_ST77922
#define LV_COLOR_16_SWAP 1
#endif

#define TFT_DIRECTION 1 // TFT direction

class Display
{
private:
public:
  void init();
  void routine();
};

#endif
