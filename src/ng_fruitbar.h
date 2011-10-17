#ifndef NG_FRUITBAR_H
#define NG_FRUITBAR_H

#include "e_mod_main.h"

//Functions necesssary to overlay disappearing tasks with their launchers
//corresponds
void      ngi_fruitbar_taskbar_box_fill            (Ngi_Box *box);
void      ngi_fruitbar_launcher_box_fill           (Ngi_Box *box);

void      item_ngi_fruitbar_launcher_cb_free       (Ngi_Item *it);
void      item_ngi_fruitbar_launcher_cb_drag_start (Ngi_Item *it);
void      item_ngi_fruitbar_launcher_cb_mouse_up   (Ngi_Item *it, Ecore_Event_Mouse_Button *ev);
void      item_ngi_fruitbar_launcher_cb_mouse_down (Ngi_Item *it, Ecore_Event_Mouse_Button *ev);
#endif
