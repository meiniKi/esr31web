/*************************************************************
* esr31web.ino
* 
* Usage: *Simple* firmware file to decode the DL-Bus signal 
*        from an ESR31 solar controller and forward the data
*        bytes via serial. The file provides a quick test 
*        script and does not inlcude any error handling etc.
*
* Author: Meinhard Kissich
*/

/* main state machine status */
enum esr31_rx_state {
  ERR,
  INIT,
  SYNC,
  SYNC_DONE,
  REC,
  DONE
};

/* position within one data byte */
enum rx_byte_pos {
  START,
  DATA,
  STOP
};

/* uncomment to enable debug information to be printed */
// #define DEBUG

#define NR_DATA_BYTES 31    /* number of data bytes in one frame */
const uint8_t din_pin = 2;  /* arduino pin with attached bus signal, make sure
                               to use one that can handle interrupts */

/* sampling period in micro seconds and granted tolerance in micro seconds */
const unsigned long bit_spacing_micros = 2049;
const unsigned long bit_spacing_tol_micros = 200;

/* cumputed barriers from tolerance */
const unsigned long bit_spacing_min_micros = bit_spacing_micros - bit_spacing_tol_micros;
const unsigned long bit_spacing_max_micros = bit_spacing_micros + bit_spacing_tol_micros;

volatile uint16_t hsync = 0;            /* nr of sync edges received */
volatile esr31_rx_state RSTATE = INIT;  /* state of main state machine */
volatile unsigned long last = 0;        /* micros at last sampling point */

volatile uint8_t rx_data[NR_DATA_BYTES]; /* rx buffer */
volatile uint16_t rx_bits = 0;           /* number bits received since frame start */
volatile rx_byte_pos rx_pos = START;     /* position in one receive byte */

void setup() {
  Serial.begin(115200);
}

void loop() {
  handle_decoding();
  /* handler other stuff here */
}

void handle_decoding()
{
  switch(RSTATE)
  {
    case INIT:
      /* init all values to be ready to receive a frame */
      detachInterrupt(digitalPinToInterrupt(din_pin));
      rx_bits = 0;
      rx_pos = START;
      hsync = 0;
      last = 0;
      for (uint16_t i = 0; i < NR_DATA_BYTES; i++)
        rx_data[i] = 0x00;
      attachInterrupt(digitalPinToInterrupt(din_pin), pin_sync_received, FALLING);
      RSTATE = SYNC;
      return;

    case SYNC:
      /* wait for sync to be received */
      return;

    case SYNC_DONE:
      /* sync received, prepare receiving data */
      detachInterrupt(digitalPinToInterrupt(din_pin));
      attachInterrupt(digitalPinToInterrupt(din_pin), pin_data_received, CHANGE);
      RSTATE = REC;
      return;

    case REC:
      /* wait for data to be received */
      return;

    case ERR:
      /* there was some error */
      #ifdef DEBUG
        Serial.println(">ERR");
        while(1);
      #else
        Serial.println("[ERR]");
        delay(10000);
        RSTATE = INIT;
      #endif
      return;

     case DONE:
      /* full frame received, encode for raspberry */
      detachInterrupt(digitalPinToInterrupt(din_pin));
      #ifdef DEBUG
        Serial.println(">Done");
        for (uint16_t i = 0; i < NR_DATA_BYTES; i++)
          {
            Serial.print(">"); 
            Serial.println(rx_data[i], HEX);
          }
          while(1);
      #else
        Serial.print("* ");
        for (uint16_t i = 0; i < NR_DATA_BYTES; i++)
        {
          Serial.print(rx_data[i], HEX);
          Serial.print(" ");
        }
        Serial.println("*");
        delay(10000);
        RSTATE = INIT;
      #endif
  }
}

void process_bit(uint8_t val) {
  if (rx_pos == START) // Start bit
  {
    rx_pos = DATA;
    if (val != 0)
    {
      #ifdef DEBUG
        Serial.println(">ERR: start bit wrong!");
      #endif
      RSTATE = ERR;
    }
    return;
  }

  if (rx_pos == DATA)
  {
    rx_data[rx_bits / 8] |= (val != 0) << (rx_bits % 8);

    if ((rx_bits % 8) == 7) // last data bit
    {
      rx_pos = STOP;
    }
    rx_bits++;
    return;
  }

  if (rx_pos == STOP) // Stop bit
  {
    rx_pos = START;
    if (val != 1)
    {
      #ifdef DEBUG
        Serial.println(">ERR: stop bit wrong!");
      #endif
      RSTATE = ERR;
    }
    return;
  }
}

void pin_data_received() {
  unsigned long now = micros();
  uint8_t val = !digitalRead(din_pin);

  if (now < last)
  {
    /* micros() overflowed, we will simply ignore the whole frame */
    detachInterrupt(digitalPinToInterrupt(din_pin));
    RSTATE = INIT;
    return;
  }

  unsigned long dt = now - last;
  if (dt > bit_spacing_min_micros && dt < bit_spacing_max_micros)
  {
    /* edge within sampling region, take sample */
    process_bit(val);
    last = now;
    if (rx_bits >= (NR_DATA_BYTES << 3))
      RSTATE = DONE;
    return;
  }
  
  if (dt > bit_spacing_max_micros)
  {
    /* timeout! there must have been an edge */
    #ifdef DEBUG
      Serial.println("timout!");
      Serial.print("at bit: "); Serial.println(rx_bits);
    #endif    
    detachInterrupt(digitalPinToInterrupt(din_pin));
    RSTATE = ERR;
  }
  
}


void pin_sync_received() {
  unsigned long now = micros();
  if (last != 0UL && now > last)
  {
    unsigned long dt = now - last;
    if (dt > bit_spacing_min_micros && dt < bit_spacing_max_micros)
    {
      /* another edge of sync-type */
      hsync++;
    }
    else 
    {
      /* not another ONE - maybe not the sync block? */
      hsync = 0;
    }
  }
  else if (now < last)
  {
    /* micros() overflowed, we will simply ignore the whole frame */
    RSTATE = INIT;
    return;
  }

  if (hsync >= 16)
  {
    #ifdef DEBUG
      Serial.println(">SYNCED");
    #endif
    RSTATE = SYNC_DONE;
  }
  last = now;

}
