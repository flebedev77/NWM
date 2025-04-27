#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include "client.h"
#include "config.h"

static Display* display;
static Window root_window;
static int screen;
static DynamicArray* client_windows;

static Window wmcheckwin;

enum { WMName, WMCheck };
static Atom utf8string;
static Atom atoms[2];

void RemoveTitlebar(Client w)
{
  XWindowAttributes attrs;
  if (XGetWindowAttributes(display, w.child, &attrs)) {
    XReparentWindow(display, w.child, root_window, 0, 0);
    XRemoveFromSaveSet(display, w.child);
  } else {
    printf("RemoveTitlebar: Attempted to reparent an invalid window\n");
  }
  XUnmapWindow(display, w.frame); 
  XDestroyWindow(display, w.frame);
  removeClient(client_windows, w.index);
}

void AddTitlebar(Window w)
{
  XWindowAttributes x_window_attrs;
  if (XGetWindowAttributes(display, w, &x_window_attrs) == 0)
  {
    printf("AddTitlebar: failed to get window attributes\n");
    return;
  }

  const Window frame = XCreateSimpleWindow(
      display,
      root_window,
      x_window_attrs.x,
      x_window_attrs.y,
      x_window_attrs.width,
      x_window_attrs.height,
      FRAME_BORDER_WIDTH,
      FRAME_BORDER_COLOR,
      FRAME_BG_COLOR);

  XSelectInput(
      display,
      frame,
      SubstructureRedirectMask | SubstructureNotifyMask);

  addClient(client_windows, (Client) {frame, w});
  XAddToSaveSet(display, w);

  XReparentWindow(
      display,
      w,
      frame,
      0, 0);  

  XMapWindow(display, frame);

  XGrabButton(
      display,
      Button1,
      Mod1Mask,
      w,
      false,
      ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
      GrabModeAsync,
      GrabModeAsync,
      None,
      None);

  XGrabButton(
      display,
      Button3,
      Mod1Mask,
      w,
      false,
      ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
      GrabModeAsync,
      GrabModeAsync,
      None,
      None);

  XGrabKey(
      display,
      XKeysymToKeycode(display, XK_F4),
      Mod1Mask,
      w,
      false,
      GrabModeAsync,
      GrabModeAsync);

  XGrabKey(
      display,
      XKeysymToKeycode(display, XK_Tab),
      Mod1Mask,
      w,
      false,
      GrabModeAsync,
      GrabModeAsync);
}

void OnMapRequest(const XMapRequestEvent* event)
{
  if (event == NULL)
  {
    printf("OnMapRequest: the event argument passed was null\n");
    return;
  }
  XMapRequestEvent e = *event;

  AddTitlebar(e.window);
  XMapWindow(display, e.window);
}

void OnUnmapNotify(const XUnmapEvent* event)
{
  if (event == NULL)
  {
    printf("OnUnmapNotify: the event argument passed was null\n");
    return;
  }
  XUnmapEvent e = *event;

  Client window_client;
  if (findClient(client_windows, e.window, &window_client) == 1)
  {
    printf("OnUnmapNotify: the window being unmapped did not have a frame\n");
    return;
  }

  if (e.event == root_window)
  {
    printf("OnUnmapNotify: tried unmapping the root window, good try\n");
    return;
  }

  RemoveTitlebar(window_client);
}

void OnConfigureRequest(const XConfigureRequestEvent* event)
{
  if (event == NULL)
  {
    printf("OnConfigureRequest: the event argument passed was null\n");
    return;
  }
  XConfigureRequestEvent e = *event;

  XWindowChanges changes;
  changes.x = e.x;
  changes.y = e.y;
  changes.width = e.width;
  changes.height = e.height;
  changes.border_width = e.border_width;
  changes.sibling = e.above;
  changes.stack_mode = e.detail;

  //Resize the frame
  Client window_client;
  if (findClient(client_windows, e.window, &window_client) == 0) 
  {
    XConfigureWindow(display, window_client.frame, e.value_mask, &changes);
  }

  //Resize the client area
  XConfigureWindow(display, e.window, e.value_mask, &changes);
}

int main()
{
  client_windows = createDynamicArray(10);
  display = XOpenDisplay(NULL);
  if (display == NULL)
  {
    printf("Failed to XOpenDisplay\n");
    return 1;
  }

  root_window = DefaultRootWindow(display);
  if (root_window == 0)
  {
    printf("Failed to get RootWindow\n");
    return 1;
  }

  utf8string = XInternAtom(display, "UTF8_STRING", False);
  atoms[WMName] = XInternAtom(display, "_NET_WM_NAME", False);
  atoms[WMCheck] = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);

  wmcheckwin = XCreateSimpleWindow(display, root_window, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(display, wmcheckwin, atoms[WMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(display, wmcheckwin, atoms[WMName], utf8string, 8, PropModeReplace, (unsigned char *) WM_NAME, 3);
	XChangeProperty(display, root_window, atoms[WMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &wmcheckwin, 1);

  XSelectInput(
      display,
      root_window,
      SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask);
  XSync(display, false);

  screen = DefaultScreen(display);
  XSetWindowBackground(display, root_window, WhitePixel(display, screen));
  XClearWindow(display, root_window);

  char running = 1;

  while (running)
  {
    XEvent event;
    XNextEvent(display, &event);

    switch (event.type)
    {
      case ConfigureRequest:
        OnConfigureRequest(&event.xconfigurerequest);
        break;
      case MapRequest:
        OnMapRequest(&event.xmaprequest);
        break;
      case UnmapNotify:
        OnUnmapNotify(&event.xunmap);
        break;
    }

  }

  XCloseDisplay(display);
  freeDynamicArray(client_windows);
  return 0;
}
