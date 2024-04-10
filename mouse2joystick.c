#if 0 // self-compiling code: chmod +x this file and run it like a script
gcc -Wall -Werror mouse2joystick.c -o mouse2joystick -I/usr/include/libevdev-1.0/libevdev/ -levdev  && ./mouse2joystick
exit 0
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <libevdev.h>
#include <linux/uinput.h>

#define msleep(ms) usleep((ms)*1000)

double clamp(double d, double min, double max) {
  const double t = d < min ? min : d;
  return t > max ? max : t;
}

static volatile unsigned char keepRunning = 1;

// handle Ctrl+C
static void sigintHandler(int x) {
  printf("Interrupted1\n");
  keepRunning = 0;
}

// enable and configure an absolute "position" analog channel
static void setup_abs(int fd2, unsigned chan, int min, int max) {
  if (ioctl(fd2, UI_SET_ABSBIT, chan))
    perror("UI_SET_ABSBIT");

  struct uinput_abs_setup s = {
      .code = chan,
      .absinfo = {.minimum = min, .maximum = max},
  };

  if (ioctl(fd2, UI_ABS_SETUP, &s))
    perror("UI_ABS_SETUP");
}

int main() {
  // register CTRL+C
  signal(SIGINT, &sigintHandler);

  // Initialise Evdev
  struct libevdev *dev = NULL;
  int fd1;
  int rc = 1;
  int grabbed = 0;

  fd1 = open("/dev/input/event25", O_RDONLY | O_NONBLOCK);
  rc = libevdev_new_from_fd(fd1, &dev);
  if (rc < 0) {
    fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
    exit(1);
  }
  printf("Input device name: \"%s\"\n", libevdev_get_name(dev));
  printf("Input device ID: bus %#x vendor %#x product %#x\n",
         libevdev_get_id_bustype(dev), libevdev_get_id_vendor(dev),
         libevdev_get_id_product(dev));
  if (!libevdev_has_event_type(dev, EV_REL) ||
      !libevdev_has_event_code(dev, EV_KEY, BTN_LEFT)) {
    printf("This device does not look like a mouse\n");
    exit(1);
  }

  //rc = ioctl(fd1, EVIOCGRAB, (void *)1);
  //grabbed = 1;

  if (rc) {
    printf("Failed to grab mouse\n");
    exit(1);
  }

  // Initialise Uinput
  int fd2 = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

  if (fd2 < 0) {
    perror("open /dev/uinput");
    return 1;
  }

  ioctl(fd2, UI_SET_EVBIT, EV_KEY); // enable button/key handling

  ioctl(fd2, UI_SET_KEYBIT, BTN_A);
  ioctl(fd2, UI_SET_KEYBIT, BTN_B);
  ioctl(fd2, UI_SET_KEYBIT, BTN_X);
  ioctl(fd2, UI_SET_KEYBIT, BTN_Y);
#if 0
    ioctl(fd2, UI_SET_KEYBIT, BTN_TL);
    ioctl(fd2, UI_SET_KEYBIT, BTN_TR);
    ioctl(fd2, UI_SET_KEYBIT, BTN_TL2);
    ioctl(fd2, UI_SET_KEYBIT, BTN_TR2);
    ioctl(fd2, UI_SET_KEYBIT, BTN_START);
    ioctl(fd2, UI_SET_KEYBIT, BTN_SELECT);
    ioctl(fd2, UI_SET_KEYBIT, BTN_THUMBL);
    ioctl(fd2, UI_SET_KEYBIT, BTN_THUMBR);
    ioctl(fd2, UI_SET_KEYBIT, BTN_DPAD_UP);
    ioctl(fd2, UI_SET_KEYBIT, BTN_DPAD_DOWN);
    ioctl(fd2, UI_SET_KEYBIT, BTN_DPAD_LEFT);
    ioctl(fd2, UI_SET_KEYBIT, BTN_DPAD_RIGHT);
#endif

  ioctl(fd2, UI_SET_EVBIT, EV_ABS); // enable analog absolute position handling

  setup_abs(fd2, ABS_X, -512, 512);
  setup_abs(fd2, ABS_Y, -512, 512);
  setup_abs(fd2, ABS_Z, -32767, 32767);
#if 0  
    setup_abs(fd2, ABS_RX, 0, 100);
    setup_abs(fd2, ABS_RY, 0, 100);
    setup_abs(fd2, ABS_RZ, 0, 100);
#endif
   /*ioctl(fd2, UI_SET_EVBIT, EV_REL);
   ioctl(fd2, UI_SET_RELBIT, REL_X);
   ioctl(fd2, UI_SET_RELBIT, REL_Y);*/

  struct uinput_setup setup = {.name = "Userspace Joystick",
                               .id = {
                                   .bustype = BUS_USB,
                                   .vendor = 0x3,
                                   .product = 0x3,
                                   .version = 2,
                               }};

  if (ioctl(fd2, UI_DEV_SETUP, &setup)) {
    perror("UI_DEV_SETUP");
    return 1;
  }

  if (ioctl(fd2, UI_DEV_CREATE)) {
    perror("UI_DEV_CREATE");
    return 1;
  }

  // you can write events one at a time, but to save overhead we'll
  // update all of them in a single write

  // x and y are triangle wave 90 degrees out of phase
  // z ramps slowly
  // A B X Y buttons toggle at four different frequencies

  float x = 0;
  int enabled = 0;
  int clear = 0;

  // cycle
  do {
    struct input_event ev_input;
    rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev_input);
    if (rc == 0) {
      /*printf("Event: %s %s %d\n",
             libevdev_event_type_get_name(ev_input.type),
             libevdev_event_code_get_name(ev_input.type, ev_input.code),
             ev_input.value);*/

      if (ev_input.code == BTN_LEFT) {
        // printf("enabled %d\n", enabled);
        enabled = ev_input.value;
        x *= enabled;
        clear = !enabled;
      }
      if (ev_input.code == BTN_SIDE || ev_input.code == BTN_EXTRA) {
        if (ev_input.value) {
          grabbed = !grabbed;
          //printf("grabbed %d\n", grabbed);
          ioctl(fd1, EVIOCGRAB, grabbed ? (void *)1 : (void *)0);
        }
      }
      if (enabled && ev_input.type == EV_REL && ev_input.code == REL_X) {
        x += ev_input.value * 5;
        // x = clamp(x, -32768, 32768);
        // x = clamp(x, -1024, 1024);
        x = clamp(x, -1280, 1280);
      }
    }

    // x -= ((x>0) - (x<0)) * 2;
    //x *= 0.997;
    // x *= 0.998;
    x *= 0.9985;

    if (fabs(x) > 0.5 || clear) {
      clear = 0;
      // printf("%f\n", x);
      struct input_event ev[2];
      memset(&ev, 0, sizeof ev);
      
      /*for (int i = -1024; i < 1024; i += 32) {
        putchar(i < x ? '|' : '-');
      }
      putchar('\n');*/

      ev[0].type = EV_ABS;
      ev[0].code = ABS_X;
      ev[0].value = x;

      /*ev[1].type = EV_REL;
      ev[1].code = REL_X;
      ev[1].value = x;*/

      // sync event tells input layer we're done with a "batch" of
      // updates
      ev[1].type = EV_SYN;
      ev[1].code = SYN_REPORT;
      ev[1].value = 0;

      if (write(fd2, &ev, sizeof ev) < 0) {
        perror("write");
        return 1;
      }
    }

    //msleep(1);
    usleep(500);
  } while (keepRunning && (rc == 1 || rc == 0 || rc == -EAGAIN));
  
  printf("Cleanup\n");
  if (grabbed) {
    ioctl(fd1, EVIOCGRAB, (void *)0);
    grabbed = 0;
  }
}
