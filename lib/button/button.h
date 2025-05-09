#ifndef BUTTON_H
#define BUTTON_H

#include <stdlib.h>
#include "pico/stdlib.h"

#define BUTTON_A_PIN 5 // GPIO para botão A
#define BUTTON_B_PIN 6 // GPIO para botão B

void init_btn(uint8_t pin);
void init_btns();
bool btn_is_pressed(uint8_t pin);

#endif // BUTTON_H
