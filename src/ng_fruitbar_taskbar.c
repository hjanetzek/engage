#include "e_mod_main.h"
#include "ng_fruitbar.h"
#include <X11/Xlib.h>

static int            _border_check          (Ngi_Box *box, E_Border *bd);
static Ngi_Item_Fruitbar *_border_find        (Ngi_Box *box, E_Border *bd);

static Eina_Bool      ngi_fruitbar_taskbar_cb_border_event       (void *data, int type, void *event);
static Eina_Bool      ngi_fruitbar_taskbar_cb_desk_show          (void *data, int type, void *event);

static void           _item_new              (Ngi_Box *box, E_Border *bd);
static void           _item_task_fill                             (Ngi_Item_Fruitbar *it, E_Border *bd);
static void           _item_ngi_fruitbar_taskbar_cb_free          (Ngi_Item *it);
static void           _item_set_icon         (Ngi_Item_Fruitbar *it);
static void           _item_set_label        (Ngi_Item_Fruitbar *it);
static void           _item_launcher_reset                       (Ngi_Item_Fruitbar *it);
static void           _item_ngi_fruitbar_taskbar_cb_mouse_down    (Ngi_Item *it, Ecore_Event_Mouse_Button *ev);
static void           _item_ngi_fruitbar_taskbar_cb_mouse_up      (Ngi_Item *it, Ecore_Event_Mouse_Button *ev);
static void           _item_ngi_fruitbar_taskbar_cb_mouse_in      (Ngi_Item *it);
static void           _item_ngi_fruitbar_taskbar_cb_mouse_out     (Ngi_Item *it);
static void           _item_ngi_fruitbar_taskbar_cb_drag_start    (Ngi_Item *it);
static void           _item_ngi_fruitbar_taskbar_cb_drag_end      (E_Drag *drag, int dropped);

static Eina_Bool      ngi_fruitbar_taskbar_cb_show_window        (void *data);
static void           ngi_fruitbar_taskbar_cb_drop_enter         (void *data, const char *type, void *event_info);
static void           ngi_fruitbar_taskbar_cb_drop_move          (void *data, const char *type, void *event_info);
static void           ngi_fruitbar_taskbar_cb_drop_end           (void *data, const char *type, void *event_info);
static void           ngi_fruitbar_taskbar_cb_drop_leave         (void *data, const char *type, void *event_info);

extern Evas_Object    *_border_icon_add                           (E_Border *bd, Evas *e);

static Ecore_X_Atom ECOMORPH_ATOM_THUMBNAIL = 0;

void
ngi_fruitbar_taskbar_init(void)
{
   ECOMORPH_ATOM_THUMBNAIL = ecore_x_atom_get("__ECOMORPH_THUMBNAIL");
}

static void
_ngi_fruitbar_taskbar_add_dnd_event_handlers(Ngi_Box *box)
{
   const char *drop[] = {"text/uri-list","text/x-moz-url", "enlightenment/x-file"};

   // "enlightenment/border", "enlightenment/desktop",
   box->drop_handler = e_drop_handler_add(box->ng->win->drop_win, box,
                                          ngi_fruitbar_taskbar_cb_drop_enter, ngi_fruitbar_taskbar_cb_drop_move,
                                          ngi_fruitbar_taskbar_cb_drop_leave, ngi_fruitbar_taskbar_cb_drop_end,
                                          drop, 3, 0, 0, 0, 0);
}

static void
_ngi_fruitbar_taskbar_add_border_event_handlers(Ngi_Box *box)
{
   Ecore_Event_Handler *h;

   if (!box) return;

#define HANDLE(_event, _cb)						\
   box->handlers = eina_list_append(box->handlers, ecore_event_handler_add(_event, _cb, box));

   HANDLE(E_EVENT_BORDER_ADD,           ngi_fruitbar_taskbar_cb_border_event);
   HANDLE(E_EVENT_BORDER_REMOVE,        ngi_fruitbar_taskbar_cb_border_event);
   HANDLE(E_EVENT_BORDER_ICONIFY,       ngi_fruitbar_taskbar_cb_border_event);
   HANDLE(E_EVENT_BORDER_UNICONIFY,     ngi_fruitbar_taskbar_cb_border_event);
   HANDLE(E_EVENT_BORDER_ICON_CHANGE,   ngi_fruitbar_taskbar_cb_border_event);
   HANDLE(E_EVENT_BORDER_PROPERTY,      ngi_fruitbar_taskbar_cb_border_event);
   HANDLE(E_EVENT_BORDER_ZONE_SET,      ngi_fruitbar_taskbar_cb_border_event);
   HANDLE(E_EVENT_BORDER_URGENT_CHANGE, ngi_fruitbar_taskbar_cb_border_event);
   HANDLE(E_EVENT_BORDER_FOCUS_IN,      ngi_fruitbar_taskbar_cb_border_event);
   HANDLE(E_EVENT_BORDER_FOCUS_OUT,     ngi_fruitbar_taskbar_cb_border_event);
   HANDLE(E_EVENT_DESK_SHOW,            ngi_fruitbar_taskbar_cb_desk_show);
#undef HANDLE
}

void
ngi_fruitbar_taskbar_add_event_handlers(Ngi_Box *box)
{
    _ngi_fruitbar_taskbar_add_dnd_event_handlers(box);
    _ngi_fruitbar_taskbar_add_border_event_handlers(box);
}

void
ngi_fruitbar_taskbar_box_fill(Ngi_Box *box)
{
   E_Border *bd;
   E_Border_List *bl;

   if (!box) return;

   bl = e_container_border_list_first(box->ng->zone->container);

   while ((bd = e_container_border_list_next(bl)))
     {
        if (box->ng->zone == bd->zone)
	  _item_new(box, bd);
     }

   e_container_border_list_free(bl);
}

void
ngi_fruitbar_taskbar_new(Ng *ng, Config_Box *cfg)
{
   Ngi_Box *box = ngi_box_new(ng);

   if (!box) return;

   box->cfg = cfg;
   cfg->box = box;

   ngi_fruitbar_taskbar_add_event_handlers(box);
   ngi_fruitbar_taskbar_box_fill(box);
}

void
ngi_fruitbar_taskbar_remove(Ngi_Box *box)
{
   Ecore_Event_Handler *h;
   
   e_drop_handler_del(box->drop_handler);

   EINA_LIST_FREE(box->handlers, h)
     ecore_event_handler_del(h);

   ngi_box_free(box);
}

static int
_border_check(Ngi_Box *box, E_Border *bd)
{
   if (box->ng->zone != bd->zone)
      return 0;
   
   if (box->cfg->taskbar_show_iconified == 0 && bd->iconic)
      return 0;

   if (box->cfg->taskbar_show_iconified == 2 && !bd->iconic)
      return 0;

   if ((box->cfg->taskbar_skip_dialogs && bd->client.netwm.type == ECORE_X_WINDOW_TYPE_DIALOG)
       || (bd->user_skip_winlist)
       || (bd->client.netwm.state.skip_taskbar))
      return 0;

   /* XXX Xitk menu windows act like normal windows... */
   if (bd->client.icccm.transient_for &&
       bd->client.icccm.class &&
       !strcmp(bd->client.icccm.class, "Xitk"))
      return 0;

   return 1;
}

static Ngi_Item_Fruitbar *
_border_find(Ngi_Box *box, E_Border *bd)
{
   Eina_List *l;
   Ngi_Item_Fruitbar *it;

   EINA_LIST_FOREACH(box->items, l, it)
     {
        //simple as that
        if (it->border == bd) return it;
        //if it is a launcher item, try to match the .desktop files
        if (!it->border && (it->app == bd->desktop))
          {
             _item_task_fill(it, bd);
             return it;
          }
     }

   return NULL;
}

/* *************************  DND  CALLBACKS  ************************** */

static void
ngi_fruitbar_taskbar_cb_drop_enter(void *data, const char *type, void *event_info)
{
   E_Event_Dnd_Enter *ev = (E_Event_Dnd_Enter *)event_info;
   Ngi_Box *box = (Ngi_Box *)data;
   Ng *ng = box->ng;

   ng->pos = (ng->horizontal ? ev->x : ev->y);

   ng->item_active = ngi_item_at_position_get(ng);

   if (!ng->item_active)
      return;  /* FIXME set edge_in before ?*/

   if (box->dnd_timer)
      ecore_timer_del(box->dnd_timer);

   box->dnd_timer = ecore_timer_add(0.5, ngi_fruitbar_taskbar_cb_show_window, ng->item_active);

   ng->dnd = 1;
   ngi_mouse_in(ng);
}

static void
ngi_fruitbar_taskbar_cb_drop_leave(void *data, const char *type, void *event_info)
{
   Ngi_Box *box = (Ngi_Box *)data;
   Ng *ng = box->ng;

   if (box->dnd_timer)
     {
	ecore_timer_del(box->dnd_timer);
	box->dnd_timer = NULL;
     }

   ng->item_active = NULL;
   ngi_mouse_out(ng);
}

static void
ngi_fruitbar_taskbar_cb_drop_move(void *data, const char *type, void *event_info)
{
   E_Event_Dnd_Move *ev = (E_Event_Dnd_Move *)event_info;
   Ngi_Box *box = (Ngi_Box *)data;
   Ng *ng = box->ng;
   Ngi_Item *it;

   ng->pos = (ng->horizontal ? ev->x : ev->y);

   it = ngi_item_at_position_get(ng);

   if (!it || !ng->item_active)
      return;

   if (ng->item_active != it)
     {
        if (box->dnd_timer)
           ecore_timer_del(box->dnd_timer);
	box->dnd_timer = NULL;

	if (it)
	  {
	     ngi_item_activate(ng);

	     box->dnd_timer = ecore_timer_add(0.5, ngi_fruitbar_taskbar_cb_show_window, it);
	  }
     }

   ngi_animate(ng);
}

static void
ngi_fruitbar_taskbar_cb_drop_end(void *data, const char *type, void *event_info)
{
   Ngi_Box *box = (Ngi_Box *)data;
   Ng *ng = box->ng;

   if (!ng->item_active)
      return;

   if (box->dnd_timer)
     {
	ecore_timer_del(box->dnd_timer);
	box->dnd_timer = NULL;
     }

   ng->item_active = NULL;
   ngi_mouse_out(ng);
}

static Eina_Bool
ngi_fruitbar_taskbar_cb_show_window(void *data)
{
   Ngi_Item_Fruitbar *it = (Ngi_Item_Fruitbar*) data;
   E_Desk *desk;

   if (!it) return EINA_FALSE;

   desk = e_desk_current_get(it->border->zone);

   if (desk != it->border->desk && !it->border->sticky)
     {
        e_desk_show(it->border->desk);
     }

   if (it->border->shaded)
     {
        e_border_unshade(it->border, E_DIRECTION_UP);
     }

   if (!it->border->visible)
     {
        e_border_show(it->border);
     }

   if (it->border->iconic)
     {
        e_border_uniconify(it->border);
     }
   else
     {
        e_border_raise(it->border);
     }

   e_border_focus_set(it->border, 1, 1);

   it->base.box->dnd_timer = NULL;

   return EINA_FALSE;
}

/* ************************** BORDER CALLBACKS ************************* */

static Eina_Bool
ngi_fruitbar_taskbar_cb_border_event(void *data, int type, void *event)
{
   E_Event_Border_Add *ev = event;
   Ngi_Box *box = data;
   Ngi_Item *it;
   Ngi_Item_Fruitbar *fit, *fbi;
   E_Border *bd = ev->border;
   Eina_List *iter;

   /* XXX workaround for e sending event after the
    * border_remove event */
   /* if (bd->already_unparented)
    *   return ECORE_CALLBACK_PASS_ON; */

   fit = _border_find(box, bd);
   it = &fit->base;

   if (type == E_EVENT_BORDER_FOCUS_IN)
     {
	if (it) ngi_item_signal_emit(it, "e,state,taskbar,focus,on");
     }
   else if (type == E_EVENT_BORDER_FOCUS_OUT)
     {
	if (it) ngi_item_signal_emit(it, "e,state,taskbar,focus,off");
     }
   else if (type == E_EVENT_BORDER_ICONIFY)
     {
	if (it) ngi_item_signal_emit(it, "e,state,taskbar,iconic,on");
     }
   else if (type == E_EVENT_BORDER_UNICONIFY)
     {
	if (it) ngi_item_signal_emit(it, "e,state,taskbar,iconic,off");
     }
   else if (type == E_EVENT_BORDER_ICON_CHANGE)
     {
	if (it) _item_set_icon((Ngi_Item_Fruitbar *)it);
     }
   else if (type == E_EVENT_BORDER_ADD)
     {
	_item_new(box, bd);
     }
   else if (type == E_EVENT_BORDER_REMOVE)
     {
        //reset callbacks if element was launcher
	if (fit)
          {
             if (fit->app)
               {
                  //Overlay another window instance of the same application with a launcher
                  if (fit->class)
                    {
                       EINA_LIST_FOREACH(fit->base.box->items, iter, fbi)
                         {
                            if (fit == fbi) continue;
                            if (fbi->class && (fbi->class == fit->class))
                              {
                                 fbi->app = fit->app;
                                 efreet_desktop_ref(fbi->app);
                                 break;
                              }
                         }
                       //if another item was found, remove old 'starter'
                       if (fbi && (fbi->class == fit->class))
                           ngi_item_remove(it);
                    }
                  _item_launcher_reset(fit);
               }
             else
               ngi_item_remove(it);
          }
     }
   else if (type == E_EVENT_BORDER_ZONE_SET)
     {
	if (box->ng->zone == bd->zone)
	  _item_new(box, bd);
	else if (it)
	  ngi_item_remove(it);
     }
   else if ((type == E_EVENT_BORDER_URGENT_CHANGE) && (it))
     {
	 if (bd->client.icccm.urgent)
	   {
	      ((Ngi_Item_Fruitbar *)it)->urgent = 1;
	      ngi_item_signal_emit(it, "e,state,taskbar,urgent,on");

	      if (box->ng->cfg->autohide_show_urgent)
		{
		   ngi_bar_lock(box->ng, 1);
		   ngi_animate(box->ng);
		}
	   }
	 else
	   {
	      ((Ngi_Item_Fruitbar *)it)->urgent = 0;
	      ngi_item_signal_emit(it, "e,state,taskbar,urgent,off");

	      if (box->ng->cfg->autohide_show_urgent)
		{
		   ngi_bar_lock(box->ng, 0);
		   ngi_animate(box->ng);
		}
	   }
     }
   else if (type == E_EVENT_BORDER_PROPERTY)
     {
	if (it)
	  {
	     if (!_border_check(box, bd))
	       ngi_item_remove(it);
	     else
	       _item_set_label((Ngi_Item_Fruitbar *)it);
	  }
	else
	  {
	     _item_new(box, ev->border);
	  }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
ngi_fruitbar_taskbar_cb_desk_show(void *data, int type, void *event)
{
   E_Event_Desk_Show *ev = event;
   Ngi_Box *box = data;
   Ngi_Item_Fruitbar *it;
   Eina_List *l;

   if (ev->desk->zone != box->ng->zone)
     return ECORE_CALLBACK_PASS_ON;

   if (!box->cfg->taskbar_show_desktop)
     return ECORE_CALLBACK_PASS_ON;
   
   EINA_LIST_FOREACH(box->items, l, it)
     {
	if ((it->border->desk == ev->desk) || (it->border->sticky))
	  {
	     evas_object_show(it->base.obj);
	     evas_object_show(it->base.over);	     
	     it->base.scale = 1.0;
	  }
	else
	  {
	     evas_object_hide(it->base.obj);
	     evas_object_hide(it->base.over);
	     it->base.scale = 0.0;
	  }
     }
   
   ngi_thaw(box->ng);

   return ECORE_CALLBACK_PASS_ON;
}

/* ***************************  TASKBAR  ITEM  *************************** */

static void
_item_launcher_reset(Ngi_Item_Fruitbar *it)
{
   Ngi_Item *item;

   if (!it) return;

   item = &it->base;

   it->base.cb_free = item_ngi_fruitbar_launcher_cb_free;
   it->base.cb_mouse_in = NULL;
   it->base.cb_mouse_out = NULL;
   it->base.cb_mouse_down = item_ngi_fruitbar_launcher_cb_mouse_down;
   it->base.cb_mouse_up = item_ngi_fruitbar_launcher_cb_mouse_up;
   it->base.cb_drag_start = item_ngi_fruitbar_launcher_cb_drag_start;

   if (it->border)
     {
        e_object_unref(E_OBJECT(it->border));
        it->border = NULL;
     }

   if (it->class)
   {
       eina_stringshare_del(it->class);
       it->class = NULL;
   }

   ngi_item_signal_emit(item, "e,state,taskbar,indicated,off");
}

static void
_item_task_fill(Ngi_Item_Fruitbar *it, E_Border *bd)
{
   Ngi_Item *item;

   if (!it || !bd) return;

   item = &it->base;

   it->border = bd;
   it->base.cb_free       = _item_ngi_fruitbar_taskbar_cb_free;
   it->base.cb_mouse_in   = _item_ngi_fruitbar_taskbar_cb_mouse_in;
   it->base.cb_mouse_out  = _item_ngi_fruitbar_taskbar_cb_mouse_out;
   it->base.cb_mouse_down = _item_ngi_fruitbar_taskbar_cb_mouse_down;
   it->base.cb_mouse_up   = _item_ngi_fruitbar_taskbar_cb_mouse_up;
   it->base.cb_drag_start = _item_ngi_fruitbar_taskbar_cb_drag_start;

   _item_set_icon(it);
   _item_set_label(it);

   if (bd->client.icccm.class && bd->client.icccm.class[0])
     it->class = eina_stringshare_ref(bd->client.icccm.class);

   it->urgent = bd->client.icccm.urgent;

   e_object_ref(E_OBJECT(bd));
   ngi_item_signal_emit(item, "e,state,taskbar,indicated,on");
}

static void
_item_new(Ngi_Box *box, E_Border *bd)
{
   Ngi_Item_Fruitbar *it, *l_it = NULL, *ll_it = NULL;
   Ngi_Item *item;
   Eina_List *l;

   if (!_border_check(box, bd))
      return;

   if (_border_find(box, bd))
      return;

   it = E_NEW(Ngi_Item_Fruitbar, 1);
   it->base.box = box;
   item = (Ngi_Item*) it;
   ngi_item_init_defaults(item);

   _item_task_fill(it, bd);

   if (bd->client.icccm.class && bd->client.icccm.class[0])
     {
	EINA_LIST_FOREACH(box->items, l, l_it)
	  {
	     if (!l_it->class)
	       {
		  continue;
	       }
	     else if (l_it->class == it->class)
	       {
		  ll_it = l_it;
	       }
	     else if ((ll_it) && (l_it->class != it->class))
	       {
		  break;
	       }
	  }
     }
   //match desktop files for grouping
   if (bd->desktop)
     {
        EINA_LIST_FOREACH(box->items, l, l_it)
          {
             if (bd->desktop == l_it->app)
               {
                  ll_it = l_it;
                  break;
               }
          }
     }
   if (ll_it)
     box->items = eina_list_append_relative(box->items, it, ll_it);
   else if (box->cfg->fruitbar_append_right)
     box->items = eina_list_append(box->items, it);
   else
     box->items = eina_list_prepend(box->items, it);

   if ((box->cfg->fruitbar_show_desktop) &&
       (bd->desk != e_desk_current_get(box->ng->zone)) &&
       (!bd->sticky))
     {
	ngi_item_show(item, 1);
	item->scale = 0.0;
     }
   else
     {
	ngi_item_show(item, 0);
     }

   if (bd->iconic)
     ngi_item_signal_emit(item, "e,state,taskbar,iconic,on");

   if (it->urgent)
     ngi_item_signal_emit(item, "e,state,taskbar,urgent,on");
}

static void
_item_ngi_fruitbar_taskbar_cb_free(Ngi_Item *item)
{
   Ngi_Item_Fruitbar *it = (Ngi_Item_Fruitbar *) item;

   if (it->urgent)
     ngi_bar_lock(item->box->ng, 0);

   if (it->border)
     e_object_unref(E_OBJECT(it->border));

   if (it->o_icon)
     evas_object_del(it->o_icon);

   if (it->o_proxy)
     edje_object_part_unswallow(it->base.obj, it->o_proxy);

   E_FREE(it);
}

static void
_item_set_icon(Ngi_Item_Fruitbar *it)
{
   Evas_Object *o, *oo;
   Evas *e = it->base.box->ng->evas;

   if (it->o_icon)
     {
	edje_object_part_unswallow(it->base.obj, it->o_icon);
	evas_object_del(it->o_icon);
     }
   if (it->o_proxy)
     {
	edje_object_part_unswallow(it->base.obj, it->o_proxy);
	evas_object_del(it->o_proxy);
     }

   o = _border_icon_add(it->border, e);
   edje_object_part_swallow(it->base.obj, "e.swallow.content", o);
   evas_object_show(o);
   it->o_icon = o;

   oo = evas_object_image_filled_add(e);
   evas_object_image_fill_set(oo, 0, 0, 1, 1);
   evas_object_image_source_set(oo, it->base.obj);
   edje_object_part_swallow(it->base.over, "e.swallow.content", oo);
   evas_object_show(oo);
   it->o_proxy = oo;

   if (it->border->iconic)
      ngi_item_signal_emit((Ngi_Item*)it, "e,state,taskbar,iconic,on");
   if (it->urgent)
     ngi_item_signal_emit((Ngi_Item*)it, "e,state,taskbar,urgent,on");
}

static void
_item_set_label(Ngi_Item_Fruitbar *it)
{
   const char *title;
   unsigned int max_len = 36;

   title = e_border_name_get(it->border);

   if ((!title) || (!title[0]))
      title = D_("No name!");

   if (strlen(title) > max_len)
     {
        char *abbv;
        const char *left, *right;
   
        abbv = E_NEW(char, E_CLIENTLIST_MAX_CAPTION_LEN + 4);
        left = title;
        right = title + (strlen(title) - (max_len / 2));
   
        strncpy(abbv, left, max_len / 2);
        strncat(abbv, "...", 3);
        strncat(abbv, right, max_len / 2);
   
   	ngi_item_label_set((Ngi_Item*)it, abbv);
	E_FREE(abbv);
	
   	return;
     }

   ngi_item_label_set((Ngi_Item*)it, title);
}

/* **************************  ITEM  CALLBACKS  ************************** */

static void
_item_ngi_fruitbar_taskbar_cb_mouse_down(Ngi_Item *item, Ecore_Event_Mouse_Button *ev)
{
   Ngi_Item_Fruitbar *it = (Ngi_Item_Fruitbar *) item;
   Evas_Coord x, y, w, h;
   E_Border *bd;
   Ng *ng;

   ng = item->box->ng;
   bd = it->border;

   if (ev->buttons == 3 && bd)
     {
        int dir = E_MENU_POP_DIRECTION_AUTO;
        evas_object_geometry_get(item->obj, &x, &y, &w, &h);

        x += ng->win->popup->x + ng->zone->x;
        y += ng->win->popup->y + ng->zone->y;

        switch(ng->cfg->orient)
          {
           case E_GADCON_ORIENT_TOP:
              dir = E_MENU_POP_DIRECTION_DOWN;
              y += h;
              break;

           case E_GADCON_ORIENT_BOTTOM:
              dir = E_MENU_POP_DIRECTION_UP;
              break;

           case E_GADCON_ORIENT_LEFT:
              dir = E_MENU_POP_DIRECTION_RIGHT;
              x += w;
              break;

           case E_GADCON_ORIENT_RIGHT:
              dir = E_MENU_POP_DIRECTION_LEFT;
              break;
          } /* switch */
        ngi_border_menu_show(item->box, it->border, x, y, dir, ev->timestamp);
     }
   else if (ev->buttons == 1 && ev->double_click && !it->border->iconic)
     {
        e_border_iconify(it->border);
     }
   else
      item->mouse_down = 1;
}

static void
_item_ngi_fruitbar_taskbar_cb_mouse_up(Ngi_Item *item, Ecore_Event_Mouse_Button *ev)
{
   Ngi_Item_Fruitbar *it = (Ngi_Item_Fruitbar *) item;
   E_Border *bd = it->border;

   if (!item->mouse_down)
      return;

   item->mouse_down = 0;

   if (it->urgent)
     {
        it->urgent = 0;

	ngi_item_signal_emit((Ngi_Item*)it, "e,state,taskbar,urgent,off");
	ngi_bar_lock(item->box->ng, 0);
     }

   if (ev->buttons != 1)
     return;

   if (bd->shaded)
     e_border_unshade(bd, E_DIRECTION_UP);

   if (bd->shaded)
     e_border_unshade(bd, E_DIRECTION_UP);

   if ((e_desk_current_get(bd->zone) != bd->desk) && (!bd->sticky))
     e_desk_show(bd->desk);

   if (bd->iconic)
     {
	e_border_uniconify(bd);

	if (it->border_was_fullscreen)
	  e_border_fullscreen(bd, (E_Fullscreen)e_config->fullscreen_policy);

	it->border_was_fullscreen = 0;
     }
   else
     {
	e_border_raise(bd);
     }
   if (bd->focused)
     {
	char buf[1024];
	E_Action *act = e_action_find("scale-windows");

	if (!act) return;
	if (!bd->client.icccm.class) return;
	  
	snprintf(buf, 1024, "go_scale_class:%s", bd->client.icccm.class);
	act->func.go(NULL, buf);
     }
   else
     e_border_focus_set(bd, 1, 1);
}


static void
_item_ngi_fruitbar_taskbar_cb_drag_del(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ng *ng = data;

   ngi_bar_lock(ng, 0);
   ngi_thaw(ng);
}

static void
_item_ngi_fruitbar_taskbar_cb_drag_start(Ngi_Item *item)
{
   Ngi_Item_Fruitbar *it = (Ngi_Item_Fruitbar *) item;
   E_Drag *d;
   Evas_Object *o;
   Evas_Coord x, y, w, h, px, py;
   Ng *ng = item->box->ng;

   if (!it->border)
      return;

   edje_object_signal_emit(ng->o_label, "e,state,label,hide", "e");

   evas_object_geometry_get(it->o_icon, &x, &y, &w, &h);

   if (!ngi_config->use_composite)
     {
	x -= ng->win->rect.x;
	y -= ng->win->rect.y;
     }

   const char *drag_types[] = { "enlightenment/border" };

   d = e_drag_new(ng->zone->container, x, y, drag_types, 1,
                  it->border, -1, NULL, _item_ngi_fruitbar_taskbar_cb_drag_end);

   o = _border_icon_add(it->border,e_drag_evas_get(d));
   e_object_ref(E_OBJECT(it->border));
   e_drag_object_set(d, o);
   e_drag_resize(d, w, h);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _item_ngi_fruitbar_taskbar_cb_drag_del, ng);

   ecore_x_pointer_xy_get(ng->win->input, &px, &py);
   e_drag_start(d, px, py);

   ngi_bar_lock(ng, 1);
}

static void
_item_ngi_fruitbar_taskbar_cb_drag_end(E_Drag *drag, int dropped)
{
   E_Border *bd = (E_Border *)drag->data;

   if (dropped)
     {
        e_object_unref(E_OBJECT(bd));
        return;
     }

   E_Desk *desk = e_desk_current_get(bd->zone);
   int w = bd->zone->w;
   int h = bd->zone->h;

   if (desk != bd->desk)
      e_border_desk_set(bd, desk);

   if ((bd->w < w) && (bd->h < w))
     {
	int dx = abs(drag->x - w/2);
	int dy = abs(drag->y - h/2);

	if (sqrt(dx*dx + dy*dy) < w/8)
	  {
	     drag->x = w/2;
	     drag->y = h/2;
	  }

	int x = drag->x - bd->w/2;

	if (bd->w < w - 50)
	  {
	     if (x < 50) x = 50;
	     if (x + bd->w > w) x = w - bd->w - 50;
	  }
	else x = bd->x;

	int y = drag->y - bd->h/2;

	if (bd->h < h - 50)
	  {
	     if (y < 50) y = 50;
	     if (y + bd->h > h) y = h - bd->h - 50;
	  }
	else y = bd->y;

	e_border_move(bd, x, y);
   }

   if (bd->iconic)
      e_border_uniconify(bd);
   else
      e_border_raise(bd);

   if (bd->shaded)
      e_border_unshade(bd, E_DIRECTION_UP);

   e_object_unref(E_OBJECT(bd));
}

static Eina_Bool
ngi_fruitbar_taskbar_cb_preview_hide(void *data)
{
   Ng *ng = data;

   ng->effect_timer = NULL;

   ecore_x_client_message32_send(e_manager_current_get()->root,
                                 ECORE_X_ATOM_NET_WM_ICON_GEOMETRY,
                                 SubstructureNotifyMask,
                                 0, 0, 0, 0, 0);
   return EINA_FALSE;
}

static void
_item_ngi_fruitbar_taskbar_cb_mouse_in(Ngi_Item *item)
{
   Ngi_Item_Fruitbar *it = (Ngi_Item_Fruitbar *) item;

   Ng *ng = item->box->ng;

   if (ng->cfg->ecomorph_features)
     {
        if (ng->effect_timer)
           ecore_timer_del(ng->effect_timer);

        ng->effect_timer = NULL;

        ecore_x_client_message32_send(e_manager_current_get()->root,
                                      ECORE_X_ATOM_NET_WM_ICON_GEOMETRY,
                                      SubstructureNotifyMask,
                                      it->border->win, item->pos,
                                      (double)ng->size * item->scale,
                                      ng->cfg->orient,0);
     }
}

static void
_item_ngi_fruitbar_taskbar_cb_mouse_out(Ngi_Item *it)
{
   Ng *ng = it->box->ng;
   if (ng->cfg->ecomorph_features)
     {
        if (ng->effect_timer)
           ecore_timer_del(ng->effect_timer);

        ng->effect_timer = ecore_timer_add(0.2, ngi_fruitbar_taskbar_cb_preview_hide, ng);
     }
}

