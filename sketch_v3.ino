// RFID reader ID-12 for Arduino
// 

#include <EEPROM.h>
#include <PLCTimer.h>
#include <Wire.h>
#include <LiquidTWI.h>

#define __DEBUGGING__
#define __CODE_SIZE__        5  // numero de bytes de cada codigo
#define __DURATION_LIGHT__   2  // duracion de la luz encendida [min]
#define __DURATION_CODE__    1./6.  // time-out para la identificacion [min]

#define __MAX_NUM_CODES__    7  // maximo numero de codigos almacenables
#define __POS_CODES_EEPROM__    2  // byte en el que comienza el almacenamiento de codigos en eeprom
#define __POS_NUMCODES_EEPROM__    0  // byte en el que se almacena el numero de codigos en eeprom
#define __POS_STATE_EEPROM__    1  // byte en el que se almacena el estado
#define __NUM_CYCLES_NO_COMM__  10000 // numero de ciclos que espera para volver a probar la comm con el LCD
#define __LCD_ADDR__  0 // direccion del LCD en el bus I2C
#define __RELAY_ON__  0
#define __RELAY_OFF__  1
// declaracion de variables globales
byte default_code[__CODE_SIZE__]  = {25, 00, 113, 93, 171};
byte accepted_codes[__MAX_NUM_CODES__][__CODE_SIZE__];
byte buffCode[__CODE_SIZE__];
byte *ptrbuffCode  = &buffCode[0];                  // puntero al buffer donde se almacena el codigo leido
volatile byte cuenta_minutos = 0, cuenta_segundos = 0; // variables actualizadas en la interrupcion
volatile static boolean actualizaLCD;
volatile static boolean tryComm;
struct CODES_TBL
{
  byte *code;
  byte num_cols;
  byte num_rows;
};

struct CODES_TBL _tabla_codigos  = {&accepted_codes[0][0], __CODE_SIZE__, 1};

const byte lightPin        = 6;            // OUT encender luz
const byte hornPin         = 7;            // OUT activar alarma
const byte NOTdoorClosedPin   = 2;            // IN puerta no cerrada
const byte keyDesactPin    = 3;            // IN llave desactivada
const byte beepPin         = 4;            // Beep pin
const byte codeMemPin      = 5;            // IN memorizar nuevo codigo

static byte estado         =  0;      // variable que recoge el estado actual del sistema
static byte estado_ant     = 255;

// Connect via i2c, default address #0 (A0-A2 not jumpered)
LiquidTWI lcd(__LCD_ADDR__);
static bool LCDinit=false;
PLCTimer TC1(TEMP_CON),TC2(TEMP_CON);

void setup() {
  byte error,n, i, j;
  
  // set up the LCD's number of rows and columns:
  #ifdef __DEBUGGING__
    Serial.begin(9600);                                 // connect to the serial port
    Serial.print("Initializing LCD");
    Serial.println();
  #endif
  
  lcd.begin(16, 2);
  if (!lcd.NoComm)
  {
    lcd.print("Inicializando");
    LCDinit=true;
  } else
  {
    #ifdef __DEBUGGING__
    Serial.print("LCD is not responding");
    Serial.println();
    #endif
  }
  n  = EEPROM.read(__POS_NUMCODES_EEPROM__);          // retrieve the number of accepted codes
  if (n  == 255)                                         // it is the first time to write in EEPROM
  {
    EEPROM.write(__POS_NUMCODES_EEPROM__, 1);
    for (i = 0; i  < __CODE_SIZE__; i++)
    {
      EEPROM.write(i + __POS_CODES_EEPROM__, default_code[i]);
      delay(100);
    }
    n  = 1;
    #ifdef __DEBUGGING__
    Serial.print("First initialization. Default access code");
    Serial.println();
    #endif
  }
  else
  {
    _tabla_codigos.num_rows  = n;
  }
  
  code_load_EEPROM(&_tabla_codigos);
  
  #ifdef __DEBUGGING__
  Serial.print("Registered codes: ");
  Serial.print(n, DEC);
  Serial.println();
    
  for (i = 0; i  < n; i++)
  {
    Serial.print("Code ");
    Serial.print(i, DEC);
    Serial.print(": ");
    for (j = 0; j  < __CODE_SIZE__; j++)
    {
      Serial.print(*(_tabla_codigos.code + _tabla_codigos.num_cols * i + j), DEC);
      Serial.print(",");
    }
    Serial.println();
  }
  #endif
  
  digitalWrite(lightPin, __RELAY_OFF__);   // to avoid initial energization
  pinMode(lightPin, OUTPUT);      // sets the digital pin as output
  digitalWrite(hornPin, __RELAY_OFF__);   // to avoid initial energization
  pinMode(hornPin, OUTPUT);       // sets the digital pin as output
  pinMode(NOTdoorClosedPin, INPUT);       // sets the digital pin as input
  digitalWrite(NOTdoorClosedPin, HIGH);   // turn on pullup resistors
  pinMode(keyDesactPin, INPUT);       // sets the digital pin as input
  digitalWrite(keyDesactPin, HIGH);   // turn on pullup resistors
  pinMode(codeMemPin, INPUT);       // sets the digital pin as input
  digitalWrite(codeMemPin, HIGH);   // turn on pullup resistors
  digitalWrite(beepPin, 0);   // to avoid initial energization
  pinMode(beepPin, OUTPUT);      // sets the digital pin as output

  cli();//stop interrupts
  // CONFIGURACION TIMER 1 for INTERRUPTING EACH SECOND (1Hz)
  //set timer1 interrupt at 1Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 1hz increments
  OCR1A = 15624;// = (16*10^6) / (1*1024) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);
  sei();//allow interrupts

  if (!lcd.NoComm)
  {
    delay(1000);
    lcd.setCursor(0, 1);
    lcd.print("Inicializado OK");
    delay(1000);
    lcd.setBacklight(LOW);
  }
  
  TC1.Delay  = 2;
  TC2.Delay  = 100;
}
void enable_T1_interrupt()
{
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
}
void disable_T1_interrupt()
{
  // disables timer compare interrupt
  TIMSK1 &= (0 << OCIE1A);
  cuenta_minutos  = 0;
  cuenta_segundos = 0;
}

ISR(TIMER1_COMPA_vect)    // interrupcion del timer 1
{
  cuenta_segundos++;
  if (cuenta_segundos > 60)
  {
    cuenta_segundos  -= 60;
    cuenta_minutos++;
    if (cuenta_minutos  > 60)
    {
      cuenta_minutos  -= 60;
    }
  }
  actualizaLCD = true;
  TC1.Execute();
}

void loop () {

  boolean code_OK = false;
  boolean read_OK = false;
  static int counterNoComm=0;
  int i;
  byte kk;

  
//  if (lcd.NoComm)
//  {
//    counterNoComm++;
//    if (counterNoComm >=  __NUM_CYCLES_NO_COMM__)
//    {
//      tryComm  = true;
//      counterNoComm=0;
//      #ifdef __DEBUGGING__
//        Serial.print("LCD comm variable is ");
//        Serial.println(lcd.NoComm);
//      #endif
//    }
//    
//  }
//  
//  if ((tryComm) && (lcd.NoComm))
//  {
//    // The i2c_scanner uses the return value of
//    // the Write.endTransmisstion to see if
//    // a device did acknowledge to the address.
//    byte error;
//    tryComm  = false;
//    Wire.beginTransmission(__LCD_ADDR__);
//    error = Wire.endTransmission();
//    if (error == 0) //found LCD response
//    {
//      lcd.begin(16, 2);
//      #ifdef __DEBUGGING__
//        Serial.print("LCD comm variable is ");
//        Serial.println(lcd.NoComm);
//      #endif
//      if (!lcd.NoComm)
//      {
//        lcd.print("Inicializando");
//      } else
//      {
//        #ifdef __DEBUGGING__
//          Serial.print("LCD is not responding");
//          Serial.println();
//        #endif
//      }
//      tryComm  = false;
//    }
//  }
  TC2.In     = (!digitalRead(keyDesactPin));
  TC2.Execute();
  if (TC2.OUT)    // se activa la llave
  {
    estado  = 200;
  }
  switch (estado)
  {
    case 0:  // IDLE STATE
      enable_T1_interrupt();
      digitalWrite(lightPin, __RELAY_OFF__);
      digitalWrite(hornPin, __RELAY_OFF__);
      read_code(&_tabla_codigos, &code_OK, ptrbuffCode, &read_OK);
      TC1.In     = (digitalRead(NOTdoorClosedPin)==HIGH);
      if (TC1.OUT)    // se abre la puerta
      {
        disable_T1_interrupt();
        #ifdef __DEBUGGING__
        Serial.print("Puerta abierta");
        Serial.println();
        #endif
        estado   = 1;
      }
      if (code_OK)
      {
        disable_T1_interrupt();
        estado   = 10;
        code_OK  = false;
      }
      break;
    case 1:  // PUERTA ABIERTA DETECTADA
      enable_T1_interrupt();
      digitalWrite(lightPin, __RELAY_OFF__);
      digitalWrite(hornPin, __RELAY_OFF__);

      if ((actualizaLCD) && (!lcd.NoComm))  
      {
        actualizaLCD  = false;
        kk  = __DURATION_CODE__ * 60 - (cuenta_minutos * 60 + cuenta_segundos);
        if (kk < 10)
        {
          lcd.print(" ");
        }
        lcd.print(kk);
        lcd.setCursor(10, 1);
      }

      if ((cuenta_segundos  % 2 == 0) && (!lcd.NoComm))  // modulo, blinks the backlight each second
      {
        lcd.setBacklight(HIGH);
        digitalWrite(beepPin, 1);
      }
      else if (!lcd.NoComm)
      {
        lcd.setBacklight(LOW);
        digitalWrite(beepPin, 0);
      }

      read_code(&_tabla_codigos, &code_OK, ptrbuffCode, &read_OK);
      
      if (cuenta_minutos*60+cuenta_segundos  >= __DURATION_CODE__*60)
      {
        #ifdef __DEBUGGING__
        Serial.print("Activar alarma");
        Serial.println();
        #endif
        digitalWrite(beepPin, 0);
        estado          = 100;
      }
      if (code_OK)
      {
        disable_T1_interrupt();
        estado   = 10;
        digitalWrite(beepPin, 0);
        code_OK  = false;
      }
      break;
    case 10:  // IDENTIFICACION OK, TEMPORIZANDO LA LUZ
      digitalWrite(lightPin, __RELAY_ON__);
      digitalWrite(hornPin, __RELAY_OFF__);
      enable_T1_interrupt();
      if ((actualizaLCD) && (!lcd.NoComm))   
      {
        kk  = __DURATION_LIGHT__ - cuenta_minutos;
        actualizaLCD  = false;
        lcd.setBacklight(HIGH);
        if (kk  <= 1)   
        {
          kk  = __DURATION_LIGHT__ * 60 - (cuenta_minutos * 60 + cuenta_segundos);          
          lcd.print(kk);
          lcd.print(" seg  ");
          if ((kk  < 10) && (kk  % 2 == 0))
          {
            digitalWrite(beepPin, 1);  
          }else
          {
            digitalWrite(beepPin, 0);  
          }
        }
        else
        {
          lcd.print(kk);
        }
        lcd.setCursor(10, 1);
      }


      if (cuenta_minutos  >= __DURATION_LIGHT__)
      {
        #ifdef __DEBUGGING__
        Serial.print("Apagar luz");
        Serial.println();
        #endif
        digitalWrite(beepPin, 0);
        estado          = 0;
      }
           
      if (!digitalRead(codeMemPin))    // new mem code button pushed
      {
         estado          = 250;
      }
      break;
    case 100:  // TIMEOUT PARA LA IDENTIFICACION, ACTIVAR ALARMA
      disable_T1_interrupt();
      digitalWrite(lightPin, __RELAY_OFF__);
      digitalWrite(hornPin, __RELAY_ON__);
      read_code(&_tabla_codigos, &code_OK, ptrbuffCode, &read_OK);
      if (code_OK)
      {
        #ifdef __DEBUGGING__
        Serial.print("Alarma desactivada");
        Serial.println();
        #endif
        estado   = 10;
        code_OK  = false;
      }
      break;
    case 200:  // se activa el modo manual mediante la llave
      disable_T1_interrupt();
      digitalWrite(lightPin, __RELAY_ON__);
      digitalWrite(hornPin, __RELAY_OFF__);
      if (!digitalRead(codeMemPin))    // se pulsa boton de memorizar nuevo codigo
      {
        estado          = 250;
      }
      if (digitalRead(keyDesactPin))    // se desactiva la llave
      {
        estado  = 10;
        #ifdef __DEBUGGING__
        Serial.print("Llave desactivada. Modo auto");
        Serial.println();
        #endif
      }
      break;
    case 250:  // se activa el modo de reconocer nuevo codigo
      enable_T1_interrupt();
      digitalWrite(lightPin, __RELAY_ON__);
      digitalWrite(hornPin, __RELAY_OFF__);
      read_code(&_tabla_codigos, &code_OK, ptrbuffCode, &read_OK);
      if (read_OK)
      {
        read_OK  = false;
        lcd.print("Detectado codigo");
        #ifdef __DEBUGGING__
        Serial.print("Nuevo codigo almacenado: ");
        #endif
        for (i = 0; i < _tabla_codigos.num_cols; i++)
        {
          EEPROM.write(_tabla_codigos.num_rows * _tabla_codigos.num_cols + i + __POS_CODES_EEPROM__, *(ptrbuffCode + i));
          #ifdef __DEBUGGING__
          Serial.print(*(ptrbuffCode + i), DEC);
          Serial.print(",");
          #endif
          delay(100);
        }
        if (_tabla_codigos.num_rows < __MAX_NUM_CODES__)
        {
          _tabla_codigos.num_rows++;
        }
        EEPROM.write(__POS_NUMCODES_EEPROM__, _tabla_codigos.num_rows);
        #ifdef __DEBUGGING__
        Serial.println();
        #endif
        estado  = 10;
      }
      if (cuenta_minutos  >= __DURATION_CODE__)
      {
        #ifdef __DEBUGGING__
        Serial.print("Codigo no registrado");
        Serial.println();
        #endif
        estado          = 0;
      }
      break;
  }

  if (estado  != estado_ant)
  {
    #ifdef __DEBUGGING__
    Serial.print("Estado: ");
    Serial.print(estado, DEC);
    Serial.println();
    #endif
    TC1.OUT  = false;            // to keep TC1.OUT= LOW
    estado_ant  = estado;
    if ((estado == 0) && (!lcd.NoComm))
    { lcd.noDisplay();
      lcd.setBacklight(LOW);
    }
    else if (!lcd.NoComm)
    {
      lcd.display();
      lcd.setCursor(0, 0);
      text2lcd(estado);
    }
  }
}
void text2lcd(byte estado)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  switch (estado)
  {
    case 0:  // ESPERANDO IDENTIFICACION
      lcd.setBacklight(LOW);
      break;
    case 1:  // PUERTA ABIERTA DETECTADA
      lcd.print("Puerta abierta");
      lcd.setCursor(0, 1);
      lcd.print("Alarma en:   seg");
      lcd.setCursor(10, 1);
      break;
    case 10:  // IDENTIFICACION OK, TEMPORIZANDO LA LUZ
      lcd.setBacklight(HIGH);
      lcd.print("Identif. OK");
      lcd.setCursor(0, 1);
      lcd.print("Luz off:    min");
      lcd.setCursor(10, 1);
      break;
    case 100:  // TIMEOUT PARA LA IDENTIFICACION, ACTIVAR ALARMA
      lcd.setBacklight(HIGH);
      lcd.print("Alarma activa");
      lcd.setCursor(0, 1);
      lcd.print("SMS 061 enviado");
      break;
    case 200:  // se activa el modo manual mediante la llave
      lcd.setBacklight(HIGH);
      lcd.print("Modo manual");
      break;
    case 250:  // se activa el modo de reconocer nuevo codigo
      lcd.setBacklight(HIGH);
      lcd.print("Nuevo codigo");
      lcd.setCursor(0, 1);
      lcd.print("Pasar pastilla");
      lcd.setCursor(0, 1);
      break;
  }


}
void read_code(struct CODES_TBL *tabla_codigos, boolean *code_OK, byte *codeRead, boolean *read_OK)
{
  byte i = 0;
  byte val = 0;
  byte code[6];
  byte checksum = 0;
  byte bytesread = 0;
  byte tempbyte = 0;

  if (Serial.available() >= 12) {
    if ((val = Serial.read()) == 2) {                 // check for header
      bytesread = 0;
      while (bytesread < 12) {                        // read 10 digit code + 2 digit checksum
        if ( Serial.available() > 0) {
          val = Serial.read();
          if ((val == 0x0D) || (val == 0x0A) || (val == 0x03) || (val == 0x02)) { // if header or stop bytes before the 10 digit reading
            break;                                    // stop reading
          }

          // Do Ascii/Hex conversion:
          if ((val >= '0') && (val <= '9')) {
            val = val - '0';
          }
          else if ((val >= 'A') && (val <= 'F')) {
            val = 10 + val - 'A';
          }

          // Every two hex-digits, add byte to code:
          if (bytesread & 1 == 1) {
            // make some space for this hex-digit by
            // shifting the previous hex-digit with 4 bits to the left:
            code[bytesread >> 1] = (val | (tempbyte << 4));

            if (bytesread >> 1 != __CODE_SIZE__) {                // If we're at the checksum byte,
              checksum ^= code[bytesread >> 1];       // Calculate the checksum... (XOR)
            };
          }
          else {
            tempbyte = val;                           // Store the first hex digit first...
          };
          bytesread++;                                // ready to read next digit
        }
      }

      // Output to Serial:

      if (bytesread == 12) {                          // if 12 digit read is complete
        *read_OK  = true;
        code_check(tabla_codigos, &code[0], code_OK);
        #ifdef __DEBUGGING__
        Serial.print("5-byte code: ");
        #endif
        for (i = 0; i < __CODE_SIZE__; i++)
        {
          *(codeRead + i)  = code[i];
          #ifdef __DEBUGGING__
          if (code[i] < 16) Serial.print("0");
          Serial.print(code[i], DEC);
          Serial.print(",");
          #endif
        }
        #ifdef __DEBUGGING__
        Serial.println();
        
        if (*code_OK)
        {
          Serial.print("Access granted");
          Serial.println();
        }
        else
        {
          Serial.print("Access denied");
          Serial.println();
        }
        #endif
      }

      bytesread = 0;
    }
  }
}

void code_load_EEPROM(struct CODES_TBL *tabla_codigos)
{
  int i, j;
  for (i = 0; i < tabla_codigos->num_rows; i++)
  {
    for (j = 0; j < tabla_codigos->num_cols; j++)
    {
      *(tabla_codigos->code + tabla_codigos->num_cols * i + j)  = EEPROM.read(tabla_codigos->num_cols * i + j + __POS_CODES_EEPROM__);
    }
  }
}

void code_check(struct CODES_TBL *tabla_codigos, byte *codigo, boolean *code_check)
{
  int i, j;

  for (i = 0; i < tabla_codigos->num_rows; i++)
  {
    *code_check  = true;
    for (j = 0; j < tabla_codigos->num_cols; j++)
    {
      if (*(tabla_codigos->code + tabla_codigos->num_cols * i + j) != *(codigo + j))
      {
        *code_check  = false;
        break;
      }
    }
    if (*code_check)
    {
      break;
    }
  }

}


