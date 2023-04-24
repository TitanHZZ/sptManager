/*
 * This file is part of wmctrl.
 *
 * https://github.com/saravanabalagi/wmctrl
 * 
 */

// basic platform check for no error reports on windows
#ifdef __linux__

#include <X11/Xatom.h>
#include <X11/Xmu/WinUtil.h>
#include <glib.h>

#define _NET_WM_STATE_ADD      1   // add/set property
#define MAX_PROPERTY_VALUE_LEN 4096

static gchar *get_window_property(Display *disp, Window win, Atom xa_prop_type, gchar *prop_name, unsigned long *size) {
    Atom xa_prop_name, xa_ret_type;
    gchar *ret;
    unsigned long ret_nitems, ret_bytes_after, tmp_size;
    int ret_format;
    unsigned char *ret_prop;

    xa_prop_name = XInternAtom(disp, prop_name, False);
    /* MAX_PROPERTY_VALUE_LEN / 4 explanation (XGetWindowProperty manpage):
     *
     * long_length = Specifies the length in 32-bit multiples of the
     *               data to be retrieved.
     *
     * NOTE:  see
     * http://mail.gnome.org/archives/wm-spec-list/2003-March/msg00067.html
     * In particular:
     *
     * 	When the X window system was ported to 64-bit architectures, a
     * rather peculiar design decision was made. 32-bit quantities such
     * as Window IDs, atoms, etc, were kept as longs in the client side
     * APIs, even when long was changed to 64 bits.
     *
     */
    if (XGetWindowProperty(disp, win, xa_prop_name, 0, MAX_PROPERTY_VALUE_LEN / 4, False, xa_prop_type, &xa_ret_type, &ret_format, &ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
        // cannot get property
        return NULL;
    }

    if (xa_ret_type != xa_prop_type) {
        // cannot get property
        XFree(ret_prop);
        return NULL;
    }

    // correct 64 bit architecture implementation of 32 bit data
    // null terminate the result to make string handling easier --> aka 'c str'
    tmp_size = (ret_format / 8) * ret_nitems;
    if(ret_format == 32){
      tmp_size *= sizeof(long) / 4;
    }
    ret = (gchar *) g_malloc(tmp_size + 1);
    memcpy(ret, ret_prop, tmp_size);
    ret[tmp_size] = '\0'; // put null byte at the end

    if (size) {
      *size = tmp_size;
    }

    XFree(ret_prop);
    return ret;
}

static Window *get_client_list(Display *disp, unsigned long *size) {
    Window *client_list = NULL;

    client_list = (Window *) get_window_property(disp, DefaultRootWindow(disp), XA_WINDOW, (char *) "_NET_CLIENT_LIST", size);
    if (!client_list) {
        client_list = (Window *) get_window_property(disp, DefaultRootWindow(disp), XA_CARDINAL, (char *) "_WIN_CLIENT_LIST", size);
    }

    return client_list;
}

static void send_client_msg(Display *disp, Window win, char *msg, unsigned long data0, unsigned long data1, unsigned long data2, unsigned long data3, unsigned long data4) {
  XEvent event;
  long mask = SubstructureRedirectMask | SubstructureNotifyMask;

  event.xclient.type = ClientMessage;
  event.xclient.serial = 0;
  event.xclient.send_event = True;
  event.xclient.message_type = XInternAtom(disp, msg, False);
  event.xclient.window = win;
  event.xclient.format = 32;
  event.xclient.data.l[0] = data0;
  event.xclient.data.l[1] = data1;
  event.xclient.data.l[2] = data2;
  event.xclient.data.l[3] = data3;
  event.xclient.data.l[4] = data4;

  XSendEvent(disp, DefaultRootWindow(disp), False, mask, &event);
}

static gchar *get_window_title (Display *disp, Window win) {
    gchar *title_utf8;
    gchar *wm_name;
    gchar *net_wm_name;

    wm_name = get_window_property(disp, win, XA_STRING, (gchar *)"WM_NAME", NULL);
    net_wm_name = get_window_property(disp, win, XInternAtom(disp, "UTF8_STRING", False), (gchar *)"_NET_WM_NAME", NULL);

    if (net_wm_name) {
        title_utf8 = g_strdup(net_wm_name);
    } else {
        if (wm_name) {
            title_utf8 = g_locale_to_utf8(wm_name, -1, NULL, NULL, NULL);
        } else {
            title_utf8 = NULL;
        }
    }

    g_free(wm_name);
    g_free(net_wm_name);
    
    return title_utf8;
}

/// @brief
/// @param disp
/// @param mode
static void execute_action_window(Display *disp, char mode) {
    Window target_window = 0;
    Window *client_list;
    unsigned long client_list_size;

    if ((client_list = get_client_list(disp, &client_list_size)) == NULL) {
        return;
    }

    for (size_t i = 0; i < client_list_size / sizeof(Window); i++) {
      // get window name from id
      gchar *window_name = get_window_title(disp, client_list[i]);

      if (window_name) {

        // check if window has "Spotify" in its name (Spotify client has changed name several times before but "Spotify" is always in its name)
        // check substring
        if (strstr(window_name, "Spotify"))
        {
          target_window = client_list[i];
          g_free(window_name);
          break;
        }

        g_free(window_name);
      }
    }

    g_free(client_list);

    // make sure we have a valid window
    if (target_window) {
      switch (mode) {
          case 'c':
              // close window
              send_client_msg(disp, target_window, (char *) "_NET_CLOSE_WINDOW", 0, 0, 0, 0, 0);
              break;
          case 'M' : {
                  // maximize window
                  Atom prop1 = XInternAtom(disp, "_NET_WM_STATE_MAXIMIZED_VERT", False);
                  Atom prop2 = XInternAtom(disp, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
                  send_client_msg(disp, target_window, (char *) "_NET_WM_STATE", _NET_WM_STATE_ADD, (unsigned long) prop1, (unsigned long) prop2, 0, 0);
                  break;
              }
          case 'm': {
                  // minimize window
                  Atom prop1 = XInternAtom(disp, "_NET_WM_STATE_HIDDEN", False);
                  send_client_msg(disp, target_window, (char *) "_NET_WM_STATE", _NET_WM_STATE_ADD, (unsigned long) prop1, (unsigned long) NULL, 0, 0);
                  break;
          }
          default:
              break;
      }
    }
}

void spt_window_action(char action) {
    Display *disp = XOpenDisplay(NULL);

    if (disp) {
        // execute command
        execute_action_window(disp, action);
        XCloseDisplay(disp);
    }
}

#endif // ifdef __linux__
