


enum esr31_rx_state {
  ERR,
  INIT,
  SYNC,
  SYNC_DONE,
  REC,
  DONE
};

enum rx_byte_pos {
  START,
  DATA,
  STOP
};



#define buf_bytes_size 64

const byte dataPin = 2;
const unsigned long bit_spacing_micros = 2049;
const unsigned long bit_spacing_tol_micros = 200;
const unsigned long bit_spacing_min_micros = bit_spacing_micros - bit_spacing_tol_micros;
const unsigned long bit_spacing_max_micros = bit_spacing_micros + bit_spacing_tol_micros;


volatile uint16_t hsync = 0;
volatile esr31_rx_state RSTATE = INIT;
volatile unsigned long last = 0;

volatile uint8_t rx_data[64];
volatile uint16_t rx_bits = 0;
volatile rx_byte_pos rx_pos = START;

void setup() {
  Serial.begin(115200);

}

void loop() {
  handle_decoding();
}

void handle_decoding()
{
  switch(RSTATE)
  {
    case INIT:
      detachInterrupt(digitalPinToInterrupt(dataPin));
      rx_bits = 0;
      rx_pos = START;
      hsync = 0;
      last = 0;
      for (uint16_t i = 0; i < buf_bytes_size; i++)
        rx_data[i] = 0x00;
      attachInterrupt(digitalPinToInterrupt(dataPin), pin_sync_received, FALLING);
      RSTATE = SYNC;
      return;
    case SYNC:
      return;
    case SYNC_DONE:
      detachInterrupt(digitalPinToInterrupt(dataPin));
      attachInterrupt(digitalPinToInterrupt(dataPin), pin_data_received, CHANGE);
      RSTATE = REC;
      return;
    case REC:
      return;
    case ERR:
      Serial.println("ERR");
      while(1);
      return;
     case DONE:
       detachInterrupt(digitalPinToInterrupt(dataPin));
       Serial.println("Done");
       for (uint16_t i = 0; i < buf_bytes_size; i++)
         Serial.println(rx_data[i], HEX);
        while(1);

  }
}

void process_bit(uint8_t val) {
  if (rx_pos == START) // Start bit
  {
    rx_pos = DATA;
    if (val != 0)
    {
      RSTATE = ERR;
    }
    return;
  }

  if (rx_pos == DATA)
  {
    rx_data[rx_bits / 8] |= (val != 0) << (rx_bits % 8);
    rx_bits++;

    if ((rx_bits % 8) == 7) // last data bit
    {
      rx_pos = STOP;
    }
    return;
  }

  if (rx_pos == STOP) // Stop bit
  {
    rx_pos = START;
    if (val != 1)
    {
      RSTATE = ERR;
    }
    return;
  }

  
  
}

void pin_data_received() {
  unsigned long now = micros();
  uint8_t val = !digitalRead(dataPin);
  //Serial.println(val);

  if (now < last)
  {
    // handle or simply ignore :D
    detachInterrupt(digitalPinToInterrupt(dataPin));
    RSTATE = INIT;
    return;
  }

  unsigned long dt = now - last;
  if (dt > bit_spacing_min_micros && dt < bit_spacing_max_micros)
  {
    process_bit(val);
    last = now;
    if (rx_bits >= 240)
      RSTATE = DONE;
    return;
  }
  
  if (dt > bit_spacing_max_micros)
  {
    // timeout!
    detachInterrupt(digitalPinToInterrupt(dataPin));
    Serial.print("timout: "); Serial.println(dt);
    Serial.print("at bit nr: "); Serial.println(rx_bits);
    Serial.print("value: "); Serial.println(val);
  }
  
}


void pin_sync_received() {
  unsigned long now = micros();
  if (last != 0UL && now > last)
  {
    unsigned long dt = now - last;
    if (dt > bit_spacing_min_micros && dt < bit_spacing_max_micros)
    {
      hsync++;
    }
    else 
    {
      hsync = 0;
    }
    //Serial.print("hsync: "); Serial.println(hsync);
  }
  else if (now < last)
  {
    // handle or just ignore :D
    RSTATE = INIT;
    return;
  }

  if (hsync >= 16)
  {
    Serial.println("SYNCED");
    RSTATE = SYNC_DONE;
  }
  last = now;

}
