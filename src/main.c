#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include "client.h"
#include "config.h"
#include "util.h"

static Display* display;
static Window root_window;
static int screen_index;
static Screen* screen;
static DynamicArray* client_windows;

static Position drag_start_cursor_position;
static Position drag_start_window_position;
static Size     drag_start_window_size;


static Window wmcheckwin;

enum { 
  WMName,
  WMCheck,
  WMDeleteWindow,
  WMProtocols,
  WMNetSupported,
  WMNetActiveWindow,
  WMWindowType,
  WMWindowTypeDock
};
static Atom utf8string;
static Atom atoms[6];

bool IsDock(Window w) {
  Atom actualType;
  int actualFormat;
  unsigned long nitems, bytesAfter;
  Atom* atomsRet = NULL;

  if (XGetWindowProperty(display, w,
                           atoms[WMWindowType], 0, 1024,
                           False, XA_ATOM,
                           &actualType, &actualFormat,
                           &nitems, &bytesAfter,
                           (unsigned char**)&atomsRet) == Success) {

        for (unsigned long i = 0; i < nitems; i++) {
            if (atomsRet[i] == atoms[WMWindowTypeDock]) {
                XFree(atomsRet);
                return true;
            }
        }
        XFree(atomsRet);
    }
    return false;
}

void CloseWindow(Client w)
{
  Window win = w.child;
  Window frame = w.frame;

  XReparentWindow(display, win, root_window, 0, 0);
  XSync(display, false);
  
  if (frame != -1) // if frame wasn't passed, then probably it wasn't found in the client list in the first place
  {
    removeClient(client_windows, w.index);
  }

  Atom* supported_protocols;
  int num_supported_protocols;
  if (XGetWMProtocols(display,
        win,
        &supported_protocols,
        &num_supported_protocols)) {
    bool supports_deletwin = false;
    for (int i = 0; i < num_supported_protocols; i++) 
    {
      if (supported_protocols[i] == atoms[WMDeleteWindow]) 
      {
        supports_deletwin = true;
        break;
      }
    }
    if (supports_deletwin) 
    {
      printf("Trying to gracefully close thwin\n");
      XEvent msg;
      memset(&msg, 0, sizeof(msg));
      msg.xclient.type = ClientMessage;
      msg.xclient.message_type = atoms[WMProtocols];
      msg.xclient.window = win;
      msg.xclient.format = 32;
      msg.xclient.data.l[0] = atoms[WMDeleteWindow];
      if (XSendEvent(display, win, false, 0, &msg) == 0)
      {
        printf("Failed to send kill event\n");
      }
    } else
    {
      printf("Killing window\n");
      XKillClient(display, win);
      XDestroyWindow(display, win);
      if (frame != -1)
      {
        XDestroyWindow(display, frame);
      }
    }
  } else 
  {
    printf("Killing window\n");
    XKillClient(display, win);
    XDestroyWindow(display, win);
    if (frame != NULL)
    {
      XDestroyWindow(display, frame);
    }
  }

}

void FocusWindow(Client w, bool raise)
{
  XSetInputFocus(display, w.child, RevertToPointerRoot, CurrentTime);

  XChangeProperty(display, root_window,
      atoms[WMNetActiveWindow], XA_WINDOW, 32, PropModeReplace,
      (unsigned char *)&(w.child), 1);

  if (raise) {
    XRaiseWindow(display, w.frame);
    XRaiseWindow(display, w.child);
  }
}

void ResizeWindow(Client w, unsigned int width, unsigned int height)
{
    XResizeWindow(
        display,
        w.frame,
        width, height
    );
    XResizeWindow(
        display,
        w.child,
        width, height
    );
}

void RemoveTitlebar(Client w)
{
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
      MODKEY,
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
      MODKEY,
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
      MODKEY,
      w,
      false,
      GrabModeAsync,
      GrabModeAsync);

  XGrabKey(
      display,
      XKeysymToKeycode(display, XK_Tab),
      MODKEY,
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

  if (!IsDock(e.window))
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

  if (IsDock(e.window)) return;

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

void OnKeyPress(const XKeyEvent* event)
{
  if (event == NULL)
  {
    printf("OnKeyPress: the event argument passed was null\n");
    return;
  }
  XKeyEvent e = *event;

  if ((e.state & MODKEY) &&
      (e.keycode == XKeysymToKeycode(display, XK_F4))) 
  {
    Client c;
    if (findClient(client_windows, e.window, &c) == 0)
    {
      CloseWindow(c);
    } else
    {
      //No frame found, so gotta improvise
      c.child = e.window;
      c.index = 0;
      c.frame = -1;
      CloseWindow(c);
    }
  }    

}

void OnKeyRelease(const XKeyEvent* e)
{}

void OnButtonPress(const XButtonEvent* event)
{
  printf("Button press\n");
  if (event == NULL)
  {
    printf("OnButtonPress: event argument passed was null\n");
    return;
  }

  XButtonEvent e = *event; 
  
  Client window_client;
  if (findClient(client_windows, e.window, &window_client) == 1)
  {
    printf("OnButtonPress: the window being unmapped did not have a frame\n");
    return;
  }

  drag_start_cursor_position = (Position) { e.x_root, e.y_root };
  Window returned_root;
  int x, y;
  unsigned width, height, border_width, depth;
  if (!XGetGeometry(
      display,
      window_client.frame,
      &returned_root,
      &x, &y,
      &width, &height,
      &border_width,
      &depth)) {
    printf("Could not get window's position and size");  
  }
  drag_start_window_position = (Position){x, y};
  drag_start_window_size = (Size){width, height};


  printf("Drag started on %d %d\n", drag_start_window_position.x, drag_start_window_position.y);
  FocusWindow(window_client, true);
}

void OnMotionNotify(const XMotionEvent* e) {
  Client window_client;
  if (findClient(client_windows, e->window, &window_client) == 1)
  {
    printf("OnMotionNotify: movement object window not found\n");
    return;
  } 

  Position deltaPosition = (Position){e->x_root - drag_start_cursor_position.x, e->y_root - drag_start_cursor_position.y};

  if (e->state & Button1Mask)
  {
    XMoveWindow(
        display,
        window_client.frame,
        drag_start_window_position.x + deltaPosition.x,
        drag_start_window_position.y + deltaPosition.y
    );
  } else if (e->state & Button3Mask)
  {
    Size newSize = (Size) {
        drag_start_window_size.width + deltaPosition.x,
        drag_start_window_size.height + deltaPosition.y
    };
    if (newSize.width < MIN_WINDOW_WIDTH) newSize.width = MIN_WINDOW_WIDTH;
    if (newSize.height < MIN_WINDOW_HEIGHT) newSize.height = MIN_WINDOW_HEIGHT;

    if (newSize.width > screen->width) newSize.width = screen->width;
    if (newSize.height > screen->height) newSize.height = screen->height;

    ResizeWindow(window_client, newSize.width, newSize.height);
  }
}

void reap_child(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
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
  atoms[WMDeleteWindow] = XInternAtom(display, "WM_DELETE_WINDOW", False);
  atoms[WMProtocols] = XInternAtom(display, "WM_PROTOCOLS", False);
  atoms[WMNetSupported] = XInternAtom(display, "_NET_SUPPORTED", False);
  atoms[WMNetActiveWindow] = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
  atoms[WMWindowType] = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
  atoms[WMWindowTypeDock] = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);

  Atom supported_atoms[] = {
    atoms[WMNetActiveWindow],
    atoms[WMWindowType],
    atoms[WMWindowTypeDock],
    atoms[WMProtocols],
    atoms[WMDeleteWindow]
  };

  wmcheckwin = XCreateSimpleWindow(display, root_window, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(display, wmcheckwin, atoms[WMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(display, wmcheckwin, atoms[WMName], utf8string, 8, PropModeReplace, (unsigned char *) WM_NAME, 3);
	XChangeProperty(display, root_window, atoms[WMCheck], XA_WINDOW, 32, PropModeReplace, (unsigned char *) &wmcheckwin, 1);
  XChangeProperty(
      display,
      root_window,
      atoms[WMNetSupported],
      XA_ATOM,
      32,
      PropModeReplace,
      (unsigned char *)supported_atoms,
      sizeof(supported_atoms) / sizeof(supported_atoms[0])
      );

  XSelectInput(
      display,
      root_window,
      SubstructureRedirectMask | 
      SubstructureNotifyMask |
      KeyPressMask |
      ButtonPressMask |
      ExposureMask
      );
  XSync(display, false);

  screen = DefaultScreenOfDisplay(display);
  screen_index = DefaultScreen(display);
  XSetWindowBackground(display, root_window, BlackPixel(display, screen_index));
  XClearWindow(display, root_window);


  pid_t pid = fork();

  if (pid == 0) {
    execlp("picom", "picom", "--backend", "xrender", (char *)NULL);
    printf("Launching startup script failed: (failed to exec)");
    exit(1);
  } else if (pid > 0) {
    printf("Launched startup script (PID: %d)\n", pid);
  } else {
    printf("Launching startup script failed: (failed to fork)");
  }

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
      case KeyPress:
        OnKeyPress(&event.xkey);
        break;
      case KeyRelease:
        OnKeyRelease(&event.xkey);
        break;
      case ButtonPress:
        OnButtonPress(&event.xbutton);
        break;
      case MotionNotify:
        while (XCheckTypedWindowEvent(
            display, event.xmotion.window, MotionNotify, &event)) {}
        OnMotionNotify(&event.xmotion);
        break;
      case Expose:
        XFlush(display);
        break;
      default:
        //printf("Ignored event\n");
        break;
    }

  }

  signal(SIGCHLD, reap_child);

  for (size_t i = 0; i < client_windows->size; i++)
  {
    Client client = client_windows->clients[i];

    XDestroyWindow(display, client.child);
    XDestroyWindow(display, client.frame);
    removeClient(client_windows, i);
  }

  XDestroyWindow(display, wmcheckwin);
  XCloseDisplay(display);
  freeDynamicArray(client_windows);
  return 0;
}
