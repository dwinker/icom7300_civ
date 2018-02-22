#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#include <fcntl.h>  /* File Control Definitions          */
#include <termios.h>/* POSIX Terminal Control Definitions*/
#include <unistd.h> /* UNIX Standard Definitions         */
#include <sys/ioctl.h>

void init_serial(int fd, struct termios *ptio);
void send_on(int fd);
void send_off(int fd);
void send_vd(int fd);
void send_s(int fd);
void send_date_time(int fd);
void send_scope_limits(
        int                fd,
        const char * const band,
        const char * const edge,
        const char * const low_edge,
        const char * const high_edge);
void get_scope_limits(
        int                fd,
        const char * const band,
        const char * const edge);
void print_usage_exit(void);

/* These are per the "Data format" section of the Full Manual. */
const unsigned char PREAMBLE    = 0xFE;
const unsigned char XCVR_ADDR   = 0x94; /* Transceiver's default address */
const unsigned char CONT_ADDR   = 0xE0; /* Controller's default address */
const unsigned char OK_CODE     = 0xFB;
const unsigned char NG_CODE     = 0xFA;
const unsigned char END_MESSAGE = 0xFD;

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
    int dt_flag  = 0;
    int sl_flag  = 0;
    const char *band      = NULL;
    const char *edge      = NULL;
    const char *low_edge  = NULL;
    const char *high_edge = NULL;

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
        } else if(0 == strcmp("s",  argv[optind])) {
            s_flag   = 1;
        } else if(0 == strcmp("dt", argv[optind])) {
            dt_flag   = 1;
        } else if(0 == strcmp("sl", argv[optind])) {
            sl_flag = 1;
            if(5 == (argc - optind)) {
                band      = argv[++optind];
                edge      = argv[++optind];
                low_edge  = argv[++optind];
                high_edge = argv[++optind];
            } else if(3 == (argc - optind)) {
                band      = argv[++optind];
                edge      = argv[++optind];
            } else {
                print_usage_exit();
            }
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
        if(dt_flag)
            send_date_time(fd);
        if(sl_flag)
            if(band && edge && low_edge && high_edge)
                send_scope_limits(fd, band, edge, low_edge, high_edge);
            else
                get_scope_limits(fd, band, edge);
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
        buf[n] = PREAMBLE;

    buf[n++] = XCVR_ADDR;
    buf[n++] = CONT_ADDR;
    buf[n++] = 0x18;
    buf[n++] = 0x01;
    buf[n++] = END_MESSAGE;

    nbytes = write(fd, buf, n);
    printf("send_on: sent %d of %d bytes.\n", nbytes, n);

    nbytes = read(fd, buf, sizeof(buf));
    printf("send_on: read %d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');

    // Check for correct response.
    puts( (CONT_ADDR   == buf[nbytes - 4] &&
           XCVR_ADDR   == buf[nbytes - 3] &&
           OK_CODE     == buf[nbytes - 2] &&
           END_MESSAGE == buf[nbytes - 1]) ? "OK" : "No Good");
}

void send_off(int fd)
{
    unsigned char buf[20];
    int nbytes;
    int n;

    n = 0;
    buf[n++] = PREAMBLE;
    buf[n++] = XCVR_ADDR;
    buf[n++] = CONT_ADDR;
    buf[n++] = 0x18;
    buf[n++] = 0x00;
    buf[n++] = END_MESSAGE;

    nbytes = write(fd, buf, n);
    printf("send_off: sent %d of %d bytes.\n", nbytes, n);

    nbytes = read(fd, buf, sizeof(buf));
    printf("send_off: read %d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');

    // Check for correct response.
    puts( (CONT_ADDR   == buf[nbytes - 4] &&
           XCVR_ADDR   == buf[nbytes - 3] &&
           OK_CODE     == buf[nbytes - 2] &&
           END_MESSAGE == buf[nbytes - 1]) ? "OK" : "No Good");
}

void send_vd(int fd)
{
    unsigned char buf[20];
    int nbytes;
    int n;
    int counts;
    float volts;

    n = 0;
    buf[n++] = PREAMBLE;
    buf[n++] = XCVR_ADDR;
    buf[n++] = CONT_ADDR;
    buf[n++] = 0x15;
    buf[n++] = 0x15;
    buf[n++] = END_MESSAGE;

    nbytes = write(fd, buf, n);
    printf("send_vd: sent %d of %d bytes.\n", nbytes, n);

    nbytes = read(fd, buf, sizeof(buf));
    printf("send_vd: read %d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');

    // Check for correctly formatted response.
    if(CONT_ADDR   == buf[nbytes - 7] &&
       XCVR_ADDR   == buf[nbytes - 6] &&
       0x15        == buf[nbytes - 5] &&
       0x15        == buf[nbytes - 4] &&
       END_MESSAGE == buf[nbytes - 1])
    {
        // Response appears correct. counts is two bytes preceeding END_MESSAGE.
        // It is BCD.
        unsigned char bh, bl;

        bh = buf[nbytes - 3];
        bl = buf[nbytes - 2];

        // Convert from BCD.
        counts = (bh >> 4) * 1000 + (bh & 0x0F) * 100 + (bl >> 4) * 10 + (bl & 0x0F);

        // Per IC-7300 Full Manual, there seems to be a discontinuity in scaling.
        // Read Vd meter level *(0000=0 V, 0013=10 V, 0241=16 V)
        if(counts < 13)
            volts = 10.0 * counts / 13.0;
        else
            volts = ((16.0 - 10.0) * counts / (241.0 - 13.0)) + 10.0;

        printf("counts = %d, volts = %.2f\n", counts, volts);
    }
}

void send_s(int fd)
{
    unsigned char buf[20];
    int nbytes;
    int n;
    int counts;

    n = 0;
    buf[n++] = PREAMBLE;
    buf[n++] = XCVR_ADDR;
    buf[n++] = CONT_ADDR;
    buf[n++] = 0x15;
    buf[n++] = 0x02;
    buf[n++] = END_MESSAGE;

    nbytes = write(fd, buf, n);
    printf("send_s: sent %d of %d bytes.\n", nbytes, n);

    nbytes = read(fd, buf, sizeof(buf));
    printf("send_s: read %d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');

    // Check for correctly formatted response.
    if(CONT_ADDR   == buf[nbytes - 7] &&
       XCVR_ADDR   == buf[nbytes - 6] &&
       0x15        == buf[nbytes - 5] &&
       0x02        == buf[nbytes - 4] &&
       END_MESSAGE == buf[nbytes - 1])
    {
        // Response appears correct. counts is two bytes preceeding END_MESSAGE.
        // It is BCD.
        unsigned char bh, bl;
        unsigned char s, db;

        bh = buf[nbytes - 3];
        bl = buf[nbytes - 2];

        // Convert from BCD.
        counts = (bh >> 4) * 1000 + (bh & 0x0F) * 100 + (bl >> 4) * 10 + (bl & 0x0F);

        // Per IC-7300 Full Manual, there seems to be a discontinuity in scaling.
        // Read S-meter level *(0000=S0, 0120=S9, 0241=S9+60dB)
        if(counts < 120) {
            s = (unsigned char)(0.5 + 9.0 * counts / 120.0);
            db = 0;
        } else {
            s = 9;
            db = (unsigned char)(0.5 + 60.0 * (counts - 120) / (241.0 - 120.0));
        }

        if(db)
            printf("counts = %d, S = S%d+%ddB\n", counts, s, db);
        else
            printf("counts = %d, S = S%d\n", counts, s);
    }
}

void send_date_time(int fd)
{
    unsigned char buf[20];
    int nbytes;
    int n;
    int counts;
    time_t t;
    struct tm *ptm;
    char date_str[9];
    char time_str[20];

    time(&t);
    ptm = gmtime(&t);
    printf("UTC: %s", asctime(ptm));

    if(0 == strftime(date_str, sizeof date_str, "%Y%m%d", ptm)) {
        puts("send_date_time: error formatting date.");
        exit(1);
    }

    if(0 == strftime(time_str, sizeof time_str, "%H%M%d", ptm)) {
        puts("send_date_time: error formatting time.");
        exit(1);
    }

    /* Send the date to the radio. */
    n = 0;
    buf[n++] = PREAMBLE;
    buf[n++] = XCVR_ADDR;
    buf[n++] = CONT_ADDR;
    buf[n++] = 0x1A;
    buf[n++] = 0x05;
    buf[n++] = 0x00;
    buf[n++] = 0x94;
    /* Convert string to BCD. */
    buf[n++] = (date_str[0] << 4) + (0x0F & date_str[1]);
    buf[n++] = (date_str[2] << 4) + (0x0F & date_str[3]);
    buf[n++] = (date_str[4] << 4) + (0x0F & date_str[5]);
    buf[n++] = (date_str[6] << 4) + (0x0F & date_str[7]);
    buf[n++] = END_MESSAGE;

    nbytes = write(fd, buf, n);
    printf("send_date_time: send %2d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');

    nbytes = read(fd, buf, sizeof(buf));
    printf("send_date_time: read %2d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');

    // Check for correct response.
    puts( (CONT_ADDR   == buf[nbytes - 4] &&
           XCVR_ADDR   == buf[nbytes - 3] &&
           OK_CODE     == buf[nbytes - 2] &&
           END_MESSAGE == buf[nbytes - 1]) ? "OK" : "No Good");

    /* Send the time to the radio. */
    n = 0;
    buf[n++] = PREAMBLE;
    buf[n++] = XCVR_ADDR;
    buf[n++] = CONT_ADDR;
    buf[n++] = 0x1A;
    buf[n++] = 0x05;
    buf[n++] = 0x00;
    buf[n++] = 0x95;
    /* Convert string to BCD. */
    buf[n++] = (time_str[0] << 4) + (0x0F & time_str[1]);
    buf[n++] = (time_str[2] << 4) + (0x0F & time_str[3]);
    buf[n++] = END_MESSAGE;

    nbytes = write(fd, buf, n);
    printf("send_date_time: send %2d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');

    nbytes = read(fd, buf, sizeof(buf));
    printf("send_date_time: read %2d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');

    // Check for correct response.
    puts( (CONT_ADDR   == buf[nbytes - 4] &&
           XCVR_ADDR   == buf[nbytes - 3] &&
           OK_CODE     == buf[nbytes - 2] &&
           END_MESSAGE == buf[nbytes - 1]) ? "OK" : "No Good");

}

const struct band_to_subcmd_lookup_tag {
    const char * const band;
    unsigned char      edge_1_cmd;
} BAND_TO_SUBCMD_LOOKUP[] = {
    {".03", 112}, // edge 1 for  0.03 to  1.60 MHz
    {"1.6", 115}, // edge 1 for  1.60 to  2.00 MHz
    {"2",   118}, // edge 1 for  2.00 to  6.00 MHz
    {"6",   121}, // edge 1 for  6.00 to  8.00 MHz
    {"8",   124}, // edge 1 for  8.00 to 11.00 MHz
    {"11",  127}, // edge 1 for 11.00 to 15.00 MHz
    {"15",  130}, // edge 1 for 15.00 to 20.00 MHz
    {"20",  133}, // edge 1 for 20.00 to 22.00 MHz
    {"22",  136}, // edge 1 for 22.00 to 26.00 MHz
    {"26",  139}, // edge 1 for 26.00 to 30.00 MHz
    {"30",  142}, // edge 1 for 30.00 to 45.00 MHz
    {"45",  145}, // edge 1 for 45.00 to 60.00 MHz
    {"60",  148}  // edge 1 for 60.00 to 74.80 MHz
};

// From page 19-8 of IC-7300 FULL MANUAL. PARSE_BANDSCOPE_EDGE_FREQUENCY() will
// convert the BCD encoded frequency into an unsigned long of frequency in Hz.
#define BCD2UCHAR(x) (((x >> 4) * 10) + (x & 0x0F))
#define PARSE_BANDSCOPE_EDGE_FREQUENCY(x1, x2, x3) \
    (100ul*BCD2UCHAR(x1) + 10000ul*BCD2UCHAR(x2) + 1000000ul*BCD2UCHAR(x3))

// No error checking.
#define UCHAR2BCD(x) (((x / 10) << 4) + (x % 10))

void send_scope_limits(
        int                fd,
        const char * const band,
        const char * const edge,
        const char * const low_edge,
        const char * const high_edge)
{
    double low_f_mhz, high_f_mhz;
    unsigned long low_100hz, high_100hz;
    unsigned char buf[20];
    unsigned char subcmd = 0;
    unsigned char edgei  = 0;
    int nbytes;
    int n;

    for(n = 0; n < sizeof(BAND_TO_SUBCMD_LOOKUP) / sizeof(BAND_TO_SUBCMD_LOOKUP[0]); n++) {
        if(0 == strcmp(band, BAND_TO_SUBCMD_LOOKUP[n].band)) {
            subcmd = BAND_TO_SUBCMD_LOOKUP[n].edge_1_cmd;
            edgei = atoi(edge);
            if(1 != edgei && 2 != edgei && 3 != edgei) {
                printf("send_scope_limits: edge was %s, must be 1, 2 or 3.\n", edge);
                print_usage_exit();
            }
            subcmd += (edgei - 1);
            break;
        }
    }

    if(!subcmd) {
        printf("send_scope_limits: band %s not found.\n", band);
        print_usage_exit();
    }

    low_f_mhz  = atof(low_edge);
    high_f_mhz = atof(high_edge);
    low_100hz  = (unsigned long)(low_f_mhz  * 1000000.0 / 100.0);
    high_100hz = (unsigned long)(high_f_mhz * 1000000.0 / 100.0);

    printf("send_scope_limits strings: \"%s\" \"%s\" \"%s\" \"%s\"\n", band, edge, low_edge, high_edge);
    printf("send_scope_limits vars: %.4f %.4f %lu %lu\n", low_f_mhz, high_f_mhz, low_100hz, high_100hz);

    /* Send the date to the radio. */
    n = 0;
    buf[n++] = PREAMBLE;
    buf[n++] = XCVR_ADDR;
    buf[n++] = CONT_ADDR;
    buf[n++] = 0x1A;
    buf[n++] = 0x05;
    buf[n++] = UCHAR2BCD(subcmd / 100);
    buf[n++] = UCHAR2BCD(subcmd % 100);
    buf[n++] = UCHAR2BCD( low_100hz % 100);  low_100hz /= 100;
    buf[n++] = UCHAR2BCD( low_100hz % 100);  low_100hz /= 100;
    buf[n++] = UCHAR2BCD( low_100hz % 100);
    buf[n++] = UCHAR2BCD(high_100hz % 100); high_100hz /= 100;
    buf[n++] = UCHAR2BCD(high_100hz % 100); high_100hz /= 100;
    buf[n++] = UCHAR2BCD(high_100hz % 100);
    buf[n++] = END_MESSAGE;

    nbytes = write(fd, buf, n);
    printf("send_scope_limits: send %2d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');

    nbytes = read(fd, buf, sizeof(buf));
    printf("send_scope_limits: read %2d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');

    // Check for correct response.
    puts( (CONT_ADDR   == buf[nbytes - 4] &&
           XCVR_ADDR   == buf[nbytes - 3] &&
           OK_CODE     == buf[nbytes - 2] &&
           END_MESSAGE == buf[nbytes - 1]) ? "OK" : "No Good");
}

void get_scope_limits(
        int                fd,
        const char * const band,
        const char * const edge)
{
    unsigned char buf[20];
    unsigned char subcmd = 0;
    unsigned char edgei  = 0;
    int nbytes;
    int n;

    for(n = 0; n < sizeof(BAND_TO_SUBCMD_LOOKUP) / sizeof(BAND_TO_SUBCMD_LOOKUP[0]); n++) {
        if(0 == strcmp(band, BAND_TO_SUBCMD_LOOKUP[n].band)) {
            subcmd = BAND_TO_SUBCMD_LOOKUP[n].edge_1_cmd;
            edgei = atoi(edge);
            if(1 != edgei && 2 != edgei && 3 != edgei) {
                printf("get_scope_limits: edge was %s, must be 1, 2 or 3.\n", edge);
                print_usage_exit();
            }
            subcmd += (edgei - 1);
            break;
        }
    }

    /* Send the date to the radio. */
    n = 0;
    buf[n++] = PREAMBLE;
    buf[n++] = XCVR_ADDR;
    buf[n++] = CONT_ADDR;
    buf[n++] = 0x1A;
    buf[n++] = 0x05;
    buf[n++] = UCHAR2BCD(subcmd / 100);
    buf[n++] = UCHAR2BCD(subcmd % 100);
    buf[n++] = END_MESSAGE;

    nbytes = write(fd, buf, n);
    printf("get_scope_limits: send %2d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');

    nbytes = read(fd, buf, sizeof(buf));
    printf("get_scope_limits: read %2d bytes:", nbytes);
    for(n = 0; n < nbytes; n++) {
        printf(" %02X", buf[n]);
    }
    putchar('\n');

    unsigned long hz100;
    double        mhz_low, mhz_high;
    // Decode response.
    if(7 <= nbytes &&
       //CONT_ADDR   == buf[nbytes - 4] &&
       //XCVR_ADDR   == buf[nbytes - 3] &&
       //OK_CODE     == buf[nbytes - 2] &&
       END_MESSAGE == buf[nbytes - 1]) {
       hz100 = BCD2UCHAR(buf[nbytes - 2]) * 10000ul + BCD2UCHAR(buf[nbytes - 3]) * 100ul + BCD2UCHAR(buf[nbytes - 4]); 
       mhz_high = hz100 * 100.0 / 1000000.0;
       hz100 = BCD2UCHAR(buf[nbytes - 5]) * 10000ul + BCD2UCHAR(buf[nbytes - 6]) * 100ul + BCD2UCHAR(buf[nbytes - 7]); 
       mhz_low = hz100 * 100.0 / 1000000.0;
       printf("mhz_lo = %.4f, mhz_high = %.4f\n", mhz_low, mhz_high);
    } else {
        puts("XXX ERROR XXX");
    }
}

void print_usage_exit(void)
{
    puts("usage: civ [-d|--device /dev/ttyUSBx] [on|off|vd|s|dt|sl [band edge [low_edge high_edge]]]");
    puts("  default device is /dev/ttyUSB0, default command is on.");
    puts("  vd reads battery voltage, s reads the S-Meter, dt sets UTC date and time.");
    puts("  Multiple commands can given, however only vd, s, and dt make sense together.");
    puts("  sl with just band and edge args will retrieve Bandscope edge frequencies.");
    printf("    Bands for sl are: ");
    for(int i = 0; i < sizeof(BAND_TO_SUBCMD_LOOKUP) / sizeof(BAND_TO_SUBCMD_LOOKUP[0]) ; i++) {
        printf(" %s", BAND_TO_SUBCMD_LOOKUP[i].band);
    }
    puts(".\n    low_edge and high_edge are in MHz.");
    exit(1);
}
