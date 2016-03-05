#include "scrser3.h"

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
{
     .Config =
     {
	  .ControlInterfaceNumber   = INTERFACE_ID_CDC_CCI,
	  .DataINEndpoint           =
	  {
	       .Address          = CDC_TX_EPADDR,
	       .Size             = CDC_TXRX_EPSIZE,
	       .Banks            = 1,
	  },
	  .DataOUTEndpoint =
	  {
	       .Address          = CDC_RX_EPADDR,
	       .Size             = CDC_TXRX_EPSIZE,
	       .Banks            = 1,
	  },
	  .NotificationEndpoint =
	  {
	       .Address          = CDC_NOTIFICATION_EPADDR,
	       .Size             = CDC_NOTIFICATION_EPSIZE,
	       .Banks            = 1,
	  },
     },
};

/** Standard file stream for the CDC interface when set up, so that the virtual CDC COM port can be
 *  used like any regular character stream in the C APIs.
 */
static FILE USBSerialStream;

#define NROWS 7
#define NPX 120

#define FRAMESIZE (NPX*NROWS/8)
#define ROWSIZE (NPX/8)
#define MAXSUBFRAMES 60

struct buf {
     uint16_t time;
     uint8_t nsf;
     struct sf {
	  uint8_t data[ROWSIZE];
	  uint16_t time;
	  uint8_t rowsel;
     } sf[MAXSUBFRAMES];
} bufs[2],
     *thisbuf = &bufs[0], /* read */
     *thatbuf = &bufs[1]; /* write */

void flip(void) {
     struct buf *buf;
     buf = thisbuf;
     thisbuf = thatbuf;
     thatbuf = buf;
}

#define NPORTS 6
volatile uint8_t *inreg[] = { 0, &PINB, &PINC, &PIND, &PINE, &PINF };
volatile uint8_t *outreg[] = { 0, &PORTB, &PORTC, &PORTD, &PORTE, &PORTF };
volatile uint8_t *dirreg[] = { 0, &DDRB, &DDRC, &DDRD, &DDRE, &DDRF };

struct {
     uint8_t regno;
     uint8_t shift;
} pins[] = {
     {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6}, {1,7}, // portb
     {2,6}, {2,7}, // portc
     {3,0}, {3,1}, {3,2}, {3,3}, {3,4}, {3,5}, {3,6}, {3,7}, // portd
     /*{4,2},*/ {4,6}, // porte
     {5,0}, {5,1}, {5,4}, {5,5}, {5,6}, {5,7}, // portf
};

#define NPINS (sizeof(pins)/sizeof(pins[0]))

static inline void reg_write(uint8_t pin, bool val) {
     if (val)
	  *outreg[pins[pin].regno] |= 1<<pins[pin].shift;
     else
	  *outreg[pins[pin].regno] &= ~(1<<pins[pin].shift);
}

static inline bool reg_read(uint8_t pin) {
     return !!(*inreg[pins[pin].regno] & (1<<pins[pin].shift));
}

static inline void reg_dir(uint8_t pin, bool val) {
     if (val) {
	  *dirreg[pins[pin].regno] |= 1<<pins[pin].shift;
     } else {
	  *dirreg[pins[pin].regno] &= ~(1<<pins[pin].shift);
	  // enable pullup
	  *outreg[pins[pin].regno] |= 1<<pins[pin].shift;
     }
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
     /* Disable watchdog if enabled by bootloader/fuses */
     MCUSR &= ~(1 << WDRF);
     wdt_disable();

     /* Disable clock division */
     clock_prescale_set(clock_div_1);

     /* Hardware Initialization */
     LEDs_Init();
     USB_Init();
}


/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
     LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
     LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
     bool ConfigSuccess = true;

     ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);

     LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
     CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

void idle(void);

void delay_tu(uint16_t t) {
  //     TCCR1B = ((1 << CS11) | (1 << CS10)); // Set up timer at Fcpu/64
     TCCR1B = ((1 << CS11) | (0 << CS10)); // Set up timer at Fcpu/8
     TCNT1 = 0;
     int16_t togo;
     do {
	  togo = t - TCNT1;
	  if (togo > 50) 
	       idle();
	  /* nothing */
     } while (togo > 0);
}

char datapin = 'c';
char clockpin = 'b';
char resetpin = 'g';
char rowpins[] = "klmnvwx";

static inline void setrow(int rowsel) {
     PORTD = PORTF = rowsel;
}

void blat_row(uint8_t rowsel, uint8_t *ptr, uint16_t delay) {
     uint8_t n;
     //  PORTB &= ~(1<<6);

     for (n=ROWSIZE; n; n--) {
	  /* Start transmission */
	  SPDR = *(ptr++);
	  //    idle();
	  /* Wait for transmission complete */
	  while(!(SPSR & (1<<SPIF)))
	       ;
     }

     setrow(rowsel);
     delay_tu(delay); // row enable
     setrow(0);
}

#define DDR_SPI DDRB
#define DD_MOSI 2
#define DD_SCK  1

void SPI_MasterInit(void)
{
     /* Set MOSI and SCK output, all others input */
     DDR_SPI = (1<<DD_MOSI)|(1<<DD_SCK);
     /* Enable SPI, Master, set clock rate fck/4 */
#if 0
     // 2MHz
     SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0);
     SPSR |= 1 <<SPI2X; // 2x speed
#else
     // 4MHz
     SPCR = (1<<SPE)|(1<<MSTR)|(0<<SPR0);
     SPSR &= ~(1 <<SPI2X); // !2x speed
#endif
}
 
static inline void inbyte(uint8_t b) {
     static uint8_t s=0; /* FSM */
     static uint8_t left=0; /* subframes left to read */
     static uint8_t lo=0; /* low byte temp */
     static struct sf *sf; /* subframe ptr */
     static uint8_t *ptr; /* data ptr */
     //     fprintf(&USBSerialStream, "in %x, s=%d, left=%d, ptr=%x\n", b, s, left, ptr);
     
     switch (s) {
case 0: if (b==0xa9) {
	  flip();
	  s++;
	  sf = &thatbuf->sf[0];
     }
     break;
case 1:
     if (b && b <= MAXSUBFRAMES) {
	  thatbuf->nsf = left = b;
	  s++;
     }
     break;
case 2: /* 2 bytes: frametime, TU*8 */
     lo = b;
     s++;
     break;
case 3:
     thatbuf->time = lo + (b<<8);
     s++;
     break;
case 4: /* start subframe */
     sf->rowsel = b & 127;
     ptr = sf->data;
     if (!(b & 128))
	  s++;
     else
	  s = 20;
     break;
     case 5: case 6: case 7: case 8: case 9:
     case 10: case 11: case 12: case 13: case 14:
     case 15: case 16: case 17: case 18: case 19: /* pixel data */
     *ptr++ = b;
     s++;
     break;
case 20: /* 2 bytes: subframetime, TU */
     lo = b;
     s++;
     break;
case 21:
     sf->time = lo + (b<<8);
     sf++;
     if (!left--)
	  s = 0;
     else
	  s = 4; /* loop */
     break;

     }
}

const PROGMEM uint8_t logo[]=
    "\x7c\x7e\x18\x78\x66\x00"
    "\x66\x60\x3c\x6c\x66\x00"
    "\x66\x60\x66\x66\x66\x00"
    "\x7c\x78\x7e\x66\x3c\x00"
    "\x78\x60\x66\x66\x18\x00"
    "\x6c\x60\x66\x6c\x18\x18"
    "\x66\x7e\x66\x78\x18\x18";

void initbuf(struct buf *buf) {
     uint8_t i;
     buf->nsf = 7;
     for (i=0; i<MAXSUBFRAMES; i++) {
	  buf->sf[i].time = 500;
	  buf->sf[i].rowsel = 1<<(i % 7);
	  memset(buf->sf[i].data, 0x0, ROWSIZE);
	  memcpy_P(buf->sf[i].data,logo+i*6,6);
     }
}

void dispinit(void) {
     int i;
     SPI_MasterInit();

     reg_dir (datapin-'a', 1);
     reg_dir (clockpin-'a', 1);
     reg_dir (resetpin-'a', 1);
     for (i=0; i<NROWS; i++) {
	  reg_dir (rowpins[i]-'a', 1);
	  reg_write(rowpins[i]-'a', 0);
     }
     reg_write(resetpin-'a', 1);
     //PORTB |= 1<<6; // pull reset high

     //  memset(framebuf, 0x55, FRAMESIZE*FRAMES);
     initbuf(thisbuf);
     initbuf(thatbuf);

}

void idle() {
     int16_t len = CDC_Device_BytesReceived (&VirtualSerial_CDC_Interface);
     while (len--) {
	  inbyte(CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface));
	  //	    CDC_Device_SendByte(&VirtualSerial_CDC_Interface, i^1);
     }

     CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
     USB_USBTask();
}  

void draw(void) {
     uint8_t i;
     struct buf *buf = thisbuf;

     for (i=0; i<buf->nsf; i++) {
	  struct sf *sf = &buf->sf[i];
	  blat_row(sf->rowsel, sf->data, sf->time);
	  idle();
     }
}

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
     SetupHardware();

     /* Create a regular character stream for the interface so that it can be used with the stdio.h functions */
     CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

     LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
     GlobalInterruptEnable();
     dispinit();
     //initdirs();
     for (;;)
     {
	  draw();
     }
}
