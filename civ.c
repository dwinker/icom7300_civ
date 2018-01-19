#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>  /* File Control Definitions          */
#include <termios.h>/* POSIX Terminal Control Definitions*/
#include <unistd.h> /* UNIX Standard Definitions         */
#include <sys/ioctl.h>

void init_serial(int fd, struct termios *ptio);
void send_on(int fd);
void send_off(int fd);
void send_vd(int fd);
void send_s(int fd);
void print_usage_exit(void);

int main(int argc, char **argv)
{
    struct termios tio_orig;
    struct termios tio;
    int fd;
    int c;
    int rc;
    int on_flag  = 0;
    int off_flag = 0;
    int vd_flag  = 0;
    int s_flag   = 0;

    static const struct option long_options[] =
    {
        {"device", required_argument, 0, 'd'},
        {0, 0, 0, 0}
    };

    char *devStr = "/dev/ttyUSB0";

    /* getopt_long stores the option index here. */
    int option_index = 0;

    while (-1 != (c = getopt_long(argc, argv, "d:", long_options, &option_index)))
    {
        switch (c)
        {
            case 'd':
              printf ("option -d with value `%s'\n", optarg);
              devStr = optarg;
              break;
            case '?':
              /* getopt_long already printed an error message. */
              break;
            default:
              abort ();
        }
    }

    /* No extra arguments means turn the radio on. */
    if(optind == argc)
        on_flag = 1;

    /* Print any remaining command line arguments (not options). */
    while (optind < argc) {
        if(0 == strcmp("on", argv[optind])) {
            on_flag  = 1;
            off_flag = 0;
        } else if(0 == strcmp("off", argv[optind])) {
            on_flag  = 0;
            off_flag = 1;
        } else if(0 == strcmp("vd", argv[optind])) {
            vd_flag  = 1;
        } else if(0 == strcmp("s", argv[optind])) {
            s_flag   = 1;
        } else {
            print_usage_exit();
        }

        optind++;
    }

    fd = open(devStr, O_RDWR | O_NOCTTY);
    if(fd == 1) {
        printf("Error Opening %s\n", devStr);
        return fd;
    }

    /* Get the current serial port configuration and save it off for
     * restoration later. */
    rc = tcgetattr(fd, &tio_orig);
    if(rc < 0) {
        printf("main: failed to get attr: %d, %s\n", rc, strerror(errno));
    }

    tio = tio_orig;

    /* Modify the current configuration for our use here. */
    init_serial(fd, &tio);

    if(on_flag) {
        send_on(fd);
    } else if(off_flag) {
        send_off(fd);
    } else {
        if(vd_flag)
            send_vd(fd);
        if(s_flag)
            send_s(fd);
    }

    rc = tcsetattr(fd, TCSADRAIN, &tio_orig);
    if(rc < 0) {
        printf("main: failed to restore attr: %d, %s\n", rc, strerror(errno));
    }

    close(fd);

    return 0;
}

void init_serial(int fd, struct termios *ptio)
{
    int serial;
    int rc;

    // Baudrate, fast
    cfsetispeed(ptio, B115200);
    cfsetospeed(ptio, B115200);

    // Sets up most c_[iolc]flag's the way we want them.
    cfmakeraw(ptio);

    // Number of Stop bits = 1, so we clear the CSTOPB bit.
    ptio->c_cflag &= ~CSTOPB; //Stop bits = 1 

    // Turn off hardware based flow control (RTS/CTS).
    ptio->c_cflag &= ~CRTSCTS;

    // Turn on the receiver of the serial port (CREAD).
    ptio->c_cflag |= (CREAD | CLOCAL);

    // Wait 1 second (10 tenths second) before returing read.  This makes a
    // purely timed read. read(fd...) will always take about a second.
    ptio->c_cc[VMIN]  =  0;
    ptio->c_cc[VTIME] = 10;

    rc = tcsetattr(fd, TCSANOW, ptio);
    if(rc < 0) {
        printf("init_serial: failed to set attr: %d, %s\n", rc, strerror(errno));
        exit(1);
    }
}

void send_on(int fd)
{
    unsigned char buf[256];
    int nbytes;
    int n;

    for(n = 0; n < 200; n++)
        buf[n] = 0xFE;

    buf[n++] = 0x94; 
    buf[n++] = 0xE0;
    buf[n++] = 0x18;
    buf[n++] = 0x01;
    buf[n++] = 0xFD;

    nbytes = write(fd, buf, n);
    printf("send_on: sent %d of %d bytes.\n", nbytes, n);

    nbytes = read(fd, buf, sizeof(buf));
    printf("send_on: read %d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');
}

void send_off(int fd)
{
    unsigned char buf[20];
    int nbytes;
    int n;

    n = 0;
    buf[n++] = 0xFE; 
    buf[n++] = 0x94; 
    buf[n++] = 0xE0;
    buf[n++] = 0x18;
    buf[n++] = 0x00;
    buf[n++] = 0xFD;

    nbytes = write(fd, buf, n);
    printf("send_off: sent %d of %d bytes.\n", nbytes, n);

    nbytes = read(fd, buf, sizeof(buf));
    printf("send_off: read %d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');
}

void send_vd(int fd)
{
    unsigned char buf[20];
    int nbytes;
    int n;

    n = 0;
    buf[n++] = 0xFE; 
    buf[n++] = 0x94; 
    buf[n++] = 0xE0;
    buf[n++] = 0x15;
    buf[n++] = 0x15;
    buf[n++] = 0xFD;

    nbytes = write(fd, buf, n);
    printf("send_vd: sent %d of %d bytes.\n", nbytes, n);

    nbytes = read(fd, buf, sizeof(buf));
    printf("send_vd: read %d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');
}

void send_s(int fd)
{
    unsigned char buf[20];
    int nbytes;
    int n;

    n = 0;
    buf[n++] = 0xFE; 
    buf[n++] = 0x94; 
    buf[n++] = 0xE0;
    buf[n++] = 0x15;
    buf[n++] = 0x02;
    buf[n++] = 0xFD;

    nbytes = write(fd, buf, n);
    printf("send_s: sent %d of %d bytes.\n", nbytes, n);

    nbytes = read(fd, buf, sizeof(buf));
    printf("send_s: read %d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');
}

void print_usage_exit(void)
{
    puts("usage: civ [-d|--device /dev/ttyUSBx] [on|off|vd|s]");
    puts("  default device is /dev/ttyUSB0, default command is on.");
    puts("  vd reads battery voltage, s reads the S-Meter.");
    puts("  Multiple commands can given, however only vd and s make sense together.");
    exit(1);
}
