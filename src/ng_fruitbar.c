#include "e_mod_main.h"
#include "ng_fruitbar.h"

/*
static int            _border_check          (Ngi_Box *box, E_Border *bd);
static Ngi_Item_Fruitbar *_border_find        (Ngi_Box *box, E_Border *bd);

static Eina_Bool      _cb_border_event       (void *data, int type, void *event);
static Eina_Bool      _cb_desk_show          (void *data, int type, void *event);

static void           _fruitbar_item_new              (Ngi_Box *box, E_Border *bd);
static void           _fruitbar_item_cb_free          (Ngi_Item *it);
static void           _fruitbar_item_set_icon         (Ngi_Item_Fruitbar *it);
static void           _fruitbar_item_set_label        (Ngi_Item_Fruitbar *it);

static void           _fruitbar_item_cb_mouse_down    (Ngi_Item *it, Ecore_Event_Mouse_Button *ev);
static void           _fruitbar_item_cb_mouse_up      (Ngi_Item *it, Ecore_Event_Mouse_Button *ev);
static void           _fruitbar_item_cb_mouse_in      (Ngi_Item *it);
static void           _fruitbar_item_cb_mouse_out     (Ngi_Item *it);
static void           _fruitbar_item_cb_drag_start    (Ngi_Item *it);
static void           _fruitbar_item_cb_drag_end      (E_Drag *drag, int dropped);

static Eina_Bool      _ngi_fruitbar_cb_show_window        (void *data);
static void           _ngi_fruitbar_cb_drop_enter         (void *data, const char *type, void *event_info);
static void           _ngi_fruitbar_cb_drop_move          (void *data, const char *type, void *event_info);
static void           _ngi_fruitbar_cb_drop_end           (void *data, const char *type, void *event_info);
static void           _ngi_fruitbar_cb_drop_leave         (void *data, const char *type, void *event_info);
*/

static Ecore_X_Atom ECOMORPH_ATOM_THUMBNAIL = 0;

void
ngi_fruitbar_init(void)
{
   ECOMORPH_ATOM_THUMBNAIL = ecore_x_atom_get("__ECOMORPH_THUMBNAIL");
}

void
ngi_fruitbar_add_event_handlers(Ngi_Box *box)
{
   ngi_fruitbar_launcher_add_event_handlers(box);
   ngi_fruitbar_taskbar_add_event_handlers(box);
}

void
ngi_fruitbar_new(Ng *ng, Config_Box *cfg)
{
   Ngi_Box *box = ngi_box_new(ng);

   if (!box) return;

   box->cfg = cfg;
   cfg->box = box;

   ngi_fruitbar_add_event_handlers(box);

   //Fill in applications as launchers
   ngi_fruitbar_launcher_box_fill(box);

   //Add already running tasks
   ngi_fruitbar_taskbar_box_fill(box);
}

void
ngi_fruitbar_remove(Ngi_Box *box)
{
   Ecore_Event_Handler *h;

   e_drop_handler_del(box->drop_handler);

   EINA_LIST_FREE(box->handlers, h)
     ecore_event_handler_del(h);

   ngi_box_free(box);
}

