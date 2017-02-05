#include <LiquidCrystal.h>
#include <OneWire.h>
#include <SPI.h>
#include <SdFat.h>

#define MAX_SENSORS       8
#ifndef DEBUG
#define DEBUG             0
#endif

//Definimos las constantes de los botones
const int BTN_NONE =      0;
const int BTN_LEFT =      1;
const int BTN_RIGHT =     2;
const int BTN_UP =        3;
const int BTN_DOWN =      4;
const int BTN_SELECT =    5;
//Definimos las constantes de los simbolos
const int SYM_CANCEL =    0;
const int SYM_LEFT =      1;
const int SYM_RIGHT =     2;
const int SYM_UP =        3;
const int SYM_DOWN =      4;
const int SYM_SELECT =    5;
const int SYM_CLOCK =     6;
const int SYM_TEMP =      7;
//Definimos los pines
const int PIN_BTNS =      0;
const int PIN_SENSORS =   22;
const int PIN_SD_CS =     53;
//Temperatura base
const float BASE_TEMP =   -127;
//Definimos el tiempo entre mediciones
const long INTERVAL =     300000L;
//Definimos el tiempo entre jumps
const long JUMP_INTERVAL = 10000L;
//Definimos los pines de los relays
const int PIN_RELAY_0 =   23;
const int PIN_RELAY_1 =   25;
const int PIN_RELAY_2 =   27;
const int PIN_RELAY_3 =   29;
const int PIN_RELAY_4 =   31;
const int PIN_RELAY_5 =   33;
const int PIN_RELAY_6 =   35;
const int PIN_RELAY_7 =   37;
//Definimos el array de relays
int relays[8] = {PIN_RELAY_0, PIN_RELAY_1, PIN_RELAY_2, PIN_RELAY_3, PIN_RELAY_4, PIN_RELAY_5, PIN_RELAY_6, PIN_RELAY_7};
//Temperatura default de trigger
const float TEMP_TRIGGER = 25.00;

//ROM de los sensores
//S0=2816CF1E00801B
//S1=2843ED1E0080A5
//S2=28F5ED1E008010
//S3=2853D41E008089
//S4=28AFC11E008028
//S5=2883EB1E0080A6
//S6=28EEED1E0080B3
//S7=2866C21E00806F

struct sensor {
  int number;
  String rom;
  bool founded = false;
  bool manual = false;  //false = encendido por condicion - true = encendido por determinacion
  bool on = false;
  int relay;
  float last_value;
  float trigger_value = 25.00;
};
sensor sensors[MAX_SENSORS];

//Inicializamos el LCD
//PIN 8 -> RS
//PIN 9 -> ENABLE
//PIN 4 -> DATA4
//PIN 5 -> DATA5
//PIN 6 -> DATA6
//PIN 7 -> DATA7
//PIN 10 -> BACKLIGHT
//PIN A0 -> BUTTONS
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

//Inicializamos el bus OneWire
OneWire  ds(22);

//Declaramos la tarjeta sd
SdFat sd;

//Declaramos las variables globales
int flagOk = 0;
int contador = 0;
int contador_anterior = -1;
boolean on;
boolean on_anterior = false;
float trigger_value;
float trigger_value_anterior = -1;
boolean manual_value;
boolean manual_value_anterior = true;
boolean edit = 0;
boolean edit_anterior;
int firstLap = 1;
unsigned long last_sense = INTERVAL;
unsigned long last_jump = JUMP_INTERVAL;

//Configuracion
void setup() {

#ifdef DEBUG
  Serial.begin(9600);
#endif

  //Inicializamos el LCD con 16x2
  lcd.begin(16, 2);
  //Inicializamos los caracteres
  createChars();
  lcd.print("Configuracion ");
  lcd.write(byte(SYM_SELECT));
  //Inicializamos los sensores
  if (initializeSensors()) {
#ifdef DEBUG
    for (int i = 0; i < MAX_SENSORS; i++) {
      Serial.println(sensors[i].number);
      Serial.println(sensors[i].rom);
      Serial.println(sensors[i].trigger_value);
      Serial.println(sensors[i].relay);
      Serial.println(sensors[i].manual);
      Serial.println("-------");
    }
#endif
    //Inicializamos los relays
    initializeRelays();
    resetLcd();
    lcd.print("Configuracion ");
    lcd.write(byte(SYM_SELECT));
    flagOk = 1;
    delay(500);
  } else {
#ifdef DEBUG
    Serial.println("Error en la configuracion de los sensores");
#endif
    delay(5000);
    resetLcd();
    lcd.print("Llamar al ");
    lcd.setCursor(0, 1);
    lcd.print("0294154371238");
  }

}

//Loop infinito
void loop() {
  if (!flagOk) {
    //We die
    while (1) { }
  }
  evaluateJump();
  evaluateTime();
  evaluateConditions();
  evaluateButtons();
  printSituation();
}

void printSituation() {
  if (edit) {
    if (trigger_value != trigger_value_anterior || manual_value != manual_value_anterior || edit != edit_anterior) {
      resetLcd();
      lcd.print("Editando S");
      lcd.print(contador);
      lcd.setCursor(0, 1);
      lcd.print("Tr: ");
      lcd.print(trigger_value, 2);
      lcd.setCursor(11, 1);
      lcd.print("Man:");
      if (manual_value) {
        lcd.write(byte(SYM_SELECT));
      } else {
        lcd.write(byte(SYM_CANCEL));
      }
    }
    trigger_value_anterior = trigger_value;
    manual_value_anterior = manual_value;
  } else {
    if (contador_anterior != contador || on != on_anterior || edit != edit_anterior) {
      resetLcd();
      lcd.print("S");
      lcd.print(contador);
      lcd.print(" ");
      lcd.write(byte(SYM_TEMP));
      lcd.print(sensors[contador].last_value, 2);
      lcd.setCursor(13, 0);
      if (sensors[contador].on) {
        lcd.print("On");
      } else {
        lcd.print("Off");
      }
      lcd.setCursor(0, 1);
      lcd.print("Tr: ");
      lcd.print(sensors[contador].trigger_value, 2);
      lcd.setCursor(11, 1);
      lcd.print("Man:");
      if (sensors[contador].manual) {
        lcd.write(byte(SYM_SELECT));
      } else {
        lcd.write(byte(SYM_CANCEL));
      }
      contador_anterior = contador;
      on_anterior = on;
    }
  }
  edit_anterior = edit;
  delay(100);
}

void evaluateConditions() {
  boolean flagOn = false;
  for (int i = 1; i < MAX_SENSORS; i++) {
    if ((sensors[i].founded && sensors[i].last_value > sensors[i].trigger_value) || sensors[i].manual) {
      if (i != 1) {
        flagOn = true;
      }
      if (digitalRead(relays[i]) != LOW) {
        digitalWrite(relays[i], LOW);
        sensors[i].on = true;
        on_anterior = false;
        on = true;
        delay(250);
      }
    } else {
      if (digitalRead(relays[i]) != HIGH) {
        digitalWrite(relays[i], HIGH);
        sensors[i].on = false;
        on_anterior = true;
        on = false;
      }
    }
  }
  if (flagOn) {
    if (digitalRead(relays[0]) != LOW) {
      digitalWrite(relays[0], LOW);
      sensors[0].on = true;
      delay(250);
    }
  } else {
    if (digitalRead(relays[0]) != HIGH) {
      digitalWrite(relays[0], HIGH);
      sensors[0].on = false;
    }
  }
}

void evaluateButtons() {
  //int simbolo = 0;
  int button = recognizeButton();
  if (button) {
    switch (button) {
      case BTN_RIGHT:
        if (edit) {
          manual_value = !manual_value;
        }
        //simbolo = SYM_RIGHT;
        delay(100);
        break;
      case BTN_UP:
        //simbolo = SYM_UP;
        if (!edit) {
          if (contador < MAX_SENSORS - 1) {
            contador++;
          }
        } else {
          trigger_value += 1;
        }
        delay(100);
        break;
      case BTN_DOWN:
        //simbolo = SYM_DOWN;
        if (!edit) {
          if (contador > 0) {
            contador--;
          }
        } else {
          trigger_value -= 1;
        }
        delay(100);
        break;
      case BTN_LEFT:
        //simbolo = SYM_LEFT;
        if(!edit){
          evaluateSensors();  
        }
        delay(100);
        break;
      case BTN_SELECT:
        if (edit) {
          contador_anterior = -1;
          sensors[contador].trigger_value = trigger_value;
          sensors[contador].manual = manual_value;
          saveSensorData();
        } else {
          trigger_value = sensors[contador].trigger_value;
          manual_value = sensors[contador].manual;
        }
        edit_anterior = edit;
        edit = !edit;
        delay(100);
        //simbolo = SYM_SELECT;
        break;
    }
    last_jump = millis();
  }
}

void saveSensorData() {
  resetLcd();
  lcd.print("Buscando SD...");
  if(sd.begin()){
    resetLcd();
    lcd.print("Guardando S" + contador);
    String ttempFileName = "s" + String(contador) + "ttemp.txt";
    char ttempFileNameChar[15];
    ttempFileName.toCharArray(ttempFileNameChar, 15); 
    File ttempDataFile = sd.open(ttempFileNameChar, FILE_WRITE);
    lcd.setCursor(0, 1);
    if(ttempDataFile){ 
      ttempDataFile.seek(0);
      ttempDataFile.print("       ");
      ttempDataFile.seek(0);
      ttempDataFile.print(sensors[contador].trigger_value);
      lcd.print("Exito ");
      lcd.write(byte(SYM_SELECT)); 
    } else {
      lcd.print("Error ");
      lcd.write(byte(SYM_CANCEL));
    }
    delay(150);
  } else {
    resetLcd();
    lcd.print("No hay SD!");
    delay(1000);
  }
}

int recognizeButton() {
  int analog = analogRead(PIN_BTNS);
#ifdef DEBUG
  Serial.print("Lectura boton: ");
  Serial.print(analog);
  Serial.print("\n");
#endif
  if (analog < 50) {
    //0
    return BTN_RIGHT;
  } else if (analog < 177) {
    //97
    return BTN_UP;
  } else if (analog < 332) {
    //252
    return BTN_DOWN;
  } else if (analog < 525) {
    //405
    return BTN_LEFT;
  } else if (analog < 750) {
    //637
    return BTN_SELECT;
  } else {
    //1023
    return BTN_NONE;
  }
}

void evaluateJump() {
  unsigned long time;
  time = millis();
  if (time - last_jump >= JUMP_INTERVAL && !edit) {
    contador = (contador == MAX_SENSORS - 1) ? 0 : contador + 1;
    last_jump = time;
  }
}

void evaluateTime() {
  unsigned long time;
  time = millis();
  if (time - last_sense >= INTERVAL || firstLap) {
    evaluateSensors();
  }
}


void evaluateSensors() {
  readSensors();
  writeSd();
  last_sense = millis();
  contador_anterior = -1;
  trigger_value_anterior = -1;
  firstLap = false;
}

void writeSd() {
  resetLcd();
#ifdef DEBUG
  Serial.println("Buscando SD");
#endif
  lcd.print("Buscando SD...");
  lcd.write(byte(SYM_CLOCK));
  if (sd.begin(PIN_SD_CS)) {
    File dataFile = sd.open("csv.txt", FILE_WRITE);
    String data;
    if (dataFile) {
      data = toCsv();
#ifdef DEBUG
      Serial.println(data);
#endif
      dataFile.println(toCsv());
      dataFile.close();
      resetLcd();
      lcd.print("Guardando csv ");
      lcd.write(byte(SYM_SELECT));
    } else {
      resetLcd();
      lcd.print("Error ");
      lcd.write(byte(SYM_CANCEL));
    }
    delay(1000);
    dataFile = sd.open("json.txt", FILE_WRITE);
    if (dataFile) {
      data = toJson();
#ifdef DEBUG
      Serial.println(data);
#endif
      dataFile.println(toJson());
      dataFile.close();
      resetLcd();
      lcd.print("Guardando json ");
      lcd.write(byte(SYM_SELECT));
    } else {
      resetLcd();
      lcd.print("Error ");
      lcd.write(byte(SYM_CANCEL));
    }
  } else {
    resetLcd();
    lcd.print("No hay SD!");
#ifdef DEBUG
    Serial.println("No encontrada");
#endif
  }
  delay(1000);
}

String toJson() {
  String data = "[";
  for (int i = 0; i < MAX_SENSORS; i++) {
    data += "{";
    data += "\"s\":";
    data += i;
    data += ",\"m\":";
    data += sensors[i].last_value;
    data += (i == MAX_SENSORS - 1) ? "}" : "},";
  }
  data += "]";
  return data;
}

String toCsv() {
  String data = "";
  for (int i = 0; i < MAX_SENSORS; i++) {
    data += sensors[i].last_value;
    data += (i == MAX_SENSORS - 1) ? "" : ",";
  }
  return data;
}

void readSensors() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    sensors[i].founded = false;
  }

  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];

#ifdef DEBUG
  Serial.println("Sensando");
#endif

  resetLcd();
  lcd.print("Sensando ");
  lcd.write(byte(SYM_CLOCK));
  ds.reset();
  ds.skip();
  ds.write(0x44);
  delay(1000);

#ifdef DEBUG
  Serial.println("Buscando");
#endif

  while (ds.search(addr)) {
    ds.reset();
    ds.select(addr);
    ds.write(0xBE);

    for ( i = 0; i < 9; i++) {
      data[i] = ds.read();
    }

    int16_t raw = (data[1] << 8) | data[0];
    if (type_s) {
      raw = raw << 3;
      if (data[7] == 0x10) {
        raw = (raw & 0xFFF0) + 12 - data[6];
      }
    } else {
      byte cfg = (data[4] & 0x60);
      if (cfg == 0x00) raw = raw & ~7;
      else if (cfg == 0x20) raw = raw & ~3;
      else if (cfg == 0x40) raw = raw & ~1;
    }

    String rom;
    for ( i = 0; i < 8; i++) {
      rom += String(addr[i], HEX);
    }
    rom.toUpperCase();

#ifdef DEBUG
    Serial.println(rom);
#endif

    int i = 0;
    boolean founded = false;
    while (i < MAX_SENSORS && !founded) {
      if (sensors[i].rom == rom) {
        founded = true;
      } else {
        i++;
      }
    }
    sensors[i].last_value = (float)raw / 16.0;
    sensors[i].founded = true;
  }

  for (int i = 0; i < MAX_SENSORS; i++) {
    sensors[i].last_value = (sensors[i].founded) ? sensors[i].last_value : -127;
  }
  ds.reset_search();

#ifdef DEBUG
  Serial.println("Fin busqueda");
#endif

}

int initializeSensors() {
  resetLcd();
  lcd.print("Buscando SD...");
  lcd.write(byte(SYM_CLOCK));
  if (sd.begin(PIN_SD_CS)) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      String number = String(i);
      
      String configFileName = "s" + number + "config.txt";
      char configFileNameChar[15];
      configFileName.toCharArray(configFileNameChar, 15);
      
      String ttempFileName = "s" + number + "ttemp.txt";
      char ttempFileNameChar[15];
      ttempFileName.toCharArray(ttempFileNameChar, 15);

      if(!sd.exists(configFileNameChar)){
        resetLcd();
        lcd.print("Falta el archivo");
        lcd.setCursor(0, 1);
        lcd.print("de conf. del S" + number);
        return 0;
      }

      if(!sd.exists(ttempFileNameChar)){
        resetLcd();
        lcd.print("Falta el archivo");
        lcd.setCursor(0, 1);
        lcd.print("de temp. del S" + number);
        return 0;
      }

      resetLcd();
      lcd.print("Configurando S" + number);
      File configDataFile = sd.open(configFileNameChar);
      File ttempDataFile = sd.open(ttempFileNameChar);

      if(!configDataFile){
        lcd.setCursor(0, 1);
        lcd.print("Error conf.");
        lcd.write(byte(SYM_CANCEL));
        return 0;
      }

      if(!ttempDataFile){
        lcd.setCursor(0, 1);
        lcd.print("Error temp.");
        lcd.write(byte(SYM_CANCEL));
        return 0;
      }
      
      sensors[i].number = i;
      sensors[i].rom = configDataFile.readString();
      sensors[i].trigger_value = ttempDataFile.readString().toFloat();
      sensors[i].last_value = 0;
      sensors[i].relay = relays[i];

      configDataFile.close();
      ttempDataFile.close();
      
      lcd.setCursor(0, 1);
      lcd.print("Exito ");
      lcd.write(byte(SYM_SELECT));
      delay(150);
    }
  } else {
    resetLcd();
    lcd.print("No hay SD!");
    return 0;
  }
  return 1;
}

void initializeRelays() {
  pinMode(PIN_RELAY_0, OUTPUT);
  pinMode(PIN_RELAY_1, OUTPUT);
  pinMode(PIN_RELAY_2, OUTPUT);
  pinMode(PIN_RELAY_3, OUTPUT);
  pinMode(PIN_RELAY_4, OUTPUT);
  pinMode(PIN_RELAY_5, OUTPUT);
  pinMode(PIN_RELAY_6, OUTPUT);
  pinMode(PIN_RELAY_7, OUTPUT);
  digitalWrite(PIN_RELAY_0, HIGH);
  digitalWrite(PIN_RELAY_1, HIGH);
  digitalWrite(PIN_RELAY_2, HIGH);
  digitalWrite(PIN_RELAY_3, HIGH);
  digitalWrite(PIN_RELAY_4, HIGH);
  digitalWrite(PIN_RELAY_5, HIGH);
  digitalWrite(PIN_RELAY_6, HIGH);
  digitalWrite(PIN_RELAY_7, HIGH);
}

void resetLcd() {
  lcd.clear();
  lcd.setCursor(0, 0);
}

void createChars() {
  byte sym_left[8] =    {B00000, B00100, B01000, B11111, B01000, B00100, B00000, B00000};
  byte sym_right[8] =   {B00000, B00100, B00010, B11111, B00010, B00100, B00000, B00000};
  byte sym_up[8] =      {B00000, B00100, B01110, B10101, B00100, B00100, B00000, B00000};
  byte sym_down[8] =    {B00000, B00100, B00100, B10101, B01110, B00100, B00000, B00000};
  byte sym_select[8] =  {B00000, B00000, B00001, B00010, B10100, B01000, B00000, B00000};
  byte sym_clock[8] =   {B11111, B10001, B01110, B00100, B01010, B10001, B11111, B00000};
  byte sym_temp[8] =    {B00100, B01010, B01010, B01110, B11111, B11111, B01110, B00000};
  byte sym_cancel[8] =  {B00000, B10001, B01010, B00100, B01010, B10001, B00000, B00000};
  lcd.createChar(SYM_LEFT, sym_left);
  lcd.createChar(SYM_RIGHT, sym_right);
  lcd.createChar(SYM_UP, sym_up);
  lcd.createChar(SYM_DOWN, sym_down);
  lcd.createChar(SYM_SELECT, sym_select);
  lcd.createChar(SYM_CLOCK, sym_clock);
  lcd.createChar(SYM_TEMP, sym_temp);
  lcd.createChar(SYM_CANCEL, sym_cancel);
}


