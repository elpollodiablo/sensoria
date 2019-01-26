#include "Arduino.h"
#include <Adafruit_Sensor.h>
#include "DHT.h"
#include <LiquidCrystal.h>

/* LCD screen */
#define LCD_COLS 20
#define LCD_ROWS 4
#define LCD_MAX_ITEMS 8
#define LCD_SPACING 10
#define LCD_LABEL_WIDTH 4

#define PIN_LCD_RS 9
#define PIN_LCD_EN 8
#define PIN_LCD_D4 4
#define PIN_LCD_D5 5
#define PIN_LCD_D6 6
#define PIN_LCD_D7 7

#ifdef PIN_LCD_D4
LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);
int lcd_item_counter;
int lcd_items[LCD_MAX_ITEMS];
int lcd_row_pos = 0;
int lcd_col_pos = 0;
#endif

/* MQ Sensors */
#define NAME_A0 "MQ2"
#define SAMPLES_A0 10
#define NAME_A1 "MQ3"
#define SAMPLES_A1 10
#define NAME_A2 "MQ4"
#define SAMPLES_A2 10
#define NAME_A3 "MQ5"
#define SAMPLES_A3 10
#define NAME_A4 "MQ9"
#define SAMPLES_A4 10
//#define NAME_A5 "MQ136"
//#define SAMPLES_A0 10
//#define NAME_A6 "MQ138"
//#define SAMPLES_A0 10

/* DHT22 */
#define PIN_DHT 12
#define DHTTYPE DHT22

#ifdef PIN_DHT
DHT dht(PIN_DHT, DHTTYPE);
int chk;
int val_hum;
#endif

// this is outside the ifdef because some sensors need it
// as a reference value
float val_temp = 20;
#define NAME_TEMP "C"
#define NAME_HUM "RH"

/* DSM501a */
#define PIN_DUST_PM10  11
#define PIN_DUST_PM25  10
#define SAMPLE_SLOTS 10
#define SAMPLES_DUST 20

#if defined(PIN_DUST_PM10) || defined(PIN_DUST_PM25)
unsigned long start_time;
unsigned long end_time;
int slot_counter = 0;
#endif

#ifdef PIN_DUST_PM10
long val_dust_pm10 = 0;
long val_ppmv_pm10 = 0;
long slots_low_duration_pm10[SAMPLE_SLOTS];
unsigned long slots_sample_time_pm10[SAMPLE_SLOTS];
#endif

#ifdef PIN_DUST_PM25
long val_dust_pm25 = 0;
long val_ppmv_pm25 = 0;
long slots_low_duration_pm25[SAMPLE_SLOTS];
unsigned long slots_sample_time_pm25[SAMPLE_SLOTS];
#endif

#ifdef NAME_A0
  int val_a0 = 0;
#endif
#ifdef NAME_A1
  int val_a1 = 0;
#endif
#ifdef NAME_A2
  int val_a2 = 0;
#endif
#ifdef NAME_A3
  int val_a3 = 0;
#endif
#ifdef NAME_A4
  int val_a4 = 0;
#endif
#ifdef NAME_A5
  int val_a5 = 0;
#endif
#ifdef NAME_A6
  int val_a6 = 0;
#endif

void setup()
{
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  Serial.begin(115200);
#ifdef PIN_DHT
  dht.begin();
#endif
#ifdef PIN_DUST_PM10
  pinMode(PIN_DUST_PM10, INPUT);
  for (int i = 0; i < SAMPLE_SLOTS; i++) {
    slots_low_duration_pm10[i] = 0;
    slots_sample_time_pm10[i] = 0;
  }
#endif
#ifdef PIN_DUST_PM25
  pinMode(PIN_DUST_PM25, INPUT);
  for (int i = 0; i < SAMPLE_SLOTS; i++) {
    slots_low_duration_pm25[i] = 0;
    slots_sample_time_pm25[i] = 0;
  }
#endif
#ifdef NAME_A0
  pinMode(A0, INPUT);
#endif
#ifdef NAME_A1
  pinMode(A1, INPUT);
#endif
#ifdef NAME_A2
  pinMode(A2, INPUT);
#endif
#ifdef NAME_A3
  pinMode(A3, INPUT);
#endif
#ifdef NAME_A4
  pinMode(A4, INPUT);
#endif
#ifdef NAME_A5
  pinMode(A5, INPUT);
#endif
#ifdef NAME_A6
  pinMode(A6, INPUT);
#endif
#ifdef PIN_LCD_D4
  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.print("hello, world!");
#endif
  digitalWrite(13, LOW);
  delay(1000);
  lcd.clear();
}

void send_result_int(const char pin_name[], long val) {
  Serial.print(pin_name);
  Serial.print(":");
  Serial.println(val);
#ifdef PIN_LCD_D4
  if (lcd_row_pos == LCD_ROWS || lcd_item_counter >= LCD_MAX_ITEMS) {
    return;
  }
  if (lcd_col_pos >= LCD_COLS) {
    lcd_col_pos = 0;
    lcd_row_pos++;
  }
  lcd.setCursor(lcd_col_pos, lcd_row_pos);
  lcd.print(pin_name);
  lcd.print(":");
  lcd.setCursor(lcd_col_pos + LCD_LABEL_WIDTH, lcd_row_pos);
  lcd.print(val);
  lcd_col_pos += LCD_SPACING;
  lcd_item_counter++;
#endif
}

int get_analog_val(int pin_analog, int sample_no) {
  double analog_val = 0;

  // create an average from 100 samples
  for(int i = 0; i < sample_no; i++) { 
    analog_val = analog_val + (float) analogRead(pin_analog);
  }

  analog_val = analog_val / sample_no;

  // convert to millivolts
  analog_val = (ceil)(analog_val / 1025 * 5000.0);

  return ceil(analog_val);
}

void store_dust_timings(int pin_dust, long slots_low_duration[], unsigned long slots_sample_time[]) {
  long duration;
  long low_pulse_duration = 0;

  // as the total duration will be until a downslope, let's wait for an upslope first
  //pulseIn(pin_dust, LOW);

  // now let's gather the time of all the low flanks
  // + the total time it takes to find SAMPLES_DUST low flanks
  start_time = micros();
  for (int i = 0; i < SAMPLES_DUST; i++) {
    duration = pulseIn(pin_dust, LOW);
    low_pulse_duration += duration;
  }
  end_time = micros();

  // yes, with unsigned, this is correct:
  // https://www.giannistsakiris.com/2015/11/17/arduino-properly-measuring-time-intervals/
  // (see comments)
  slots_sample_time[slot_counter] = end_time - start_time;
  slots_low_duration[slot_counter] = low_pulse_duration;
}

long calculate_dust_mg(long slots_low_duration[], unsigned long slots_sample_time[]) {
  double total_low_duration = 0;
  double total_sample_time = 0;
  double ratio;

  for (int slot = 0; slot < SAMPLE_SLOTS; slot++) {
    //Serial.print("slot ");
    //Serial.print(slot);
    //Serial.print(" low_duration ");
    //Serial.print(slots_low_duration[slot]);
    //Serial.print(" sample_time ");
    //Serial.println(slots_sample_time[slot]);
    total_low_duration += slots_low_duration[slot];
    total_sample_time += slots_sample_time[slot];
  }
  //Serial.print("total_low ");
  //Serial.print(total_low_duration);
  //Serial.print(" total_sample_time ");
  //Serial.print(total_sample_time);
  // from https://diyprojects.io/calculate-air-quality-index-iaq-iqa-dsm501-arduino-esp8266/#.XDkor8tKjM0
  ratio = (total_low_duration / total_sample_time) * 100;
  // ratio = (low_pulse_duration - end_time + start_time)/(sample_time_ms * 10.0); // apparently doesn't work
  //Serial.print(" ratio ");
  //Serial.println(ratio);
  // apparently, that's the spec sheet curve. I doubt it, but let's just use it for now.
  // long concentration = ceil( 1.1 * pow(ratio, 3) - 3.8 * pow(ratio, 2) + 520 * ratio + 0.62);
  // let's try this curve instead:
  long concentration = ceil (0.1776*pow(ratio,3) - 0.24*pow(ratio,2) + 94.003*ratio);
  return(concentration);
}
int get_aqi(int p25_weight) {
  long aqi = 0;

  if (p25_weight>= 0 && p25_weight <= 35) {
    aqi = 0   + (50.0 / 35 * p25_weight);
  } 
  else if (p25_weight > 35 && p25_weight <= 75) {
    aqi = 50  + (50.0 / 40 * (p25_weight - 35));
  } 
  else if (p25_weight > 75 && p25_weight <= 115) {
    aqi = 100 + (50.0 / 40 * (p25_weight - 75));
  } 
  else if (p25_weight > 115 && p25_weight <= 150) {
    aqi = 150 + (50.0 / 35 * (p25_weight - 115));
  } 
  else if (p25_weight > 150 && p25_weight <= 250) {
    aqi = 200 + (100.0 / 100.0 * (p25_weight - 150));
  } 
  else if (p25_weight > 250 && p25_weight <= 500) {
    aqi = 300 + (200.0 / 250.0 * (p25_weight - 250));
  } 
  else if (p25_weight > 500.0) {
    aqi = 500 + (500.0 / 500.0 * (p25_weight - 500.0)); // Extension
  } 
  else {
    aqi = 0;
  }
  return aqi;
}
void loop()
{
#ifdef PIN_LCD_D4
  lcd_item_counter = 0;
  lcd_row_pos = 0;
  lcd_col_pos = 0;
#endif
#ifdef PIN_DHT
  digitalWrite(13, HIGH);
  val_hum = (int) dht.readHumidity();
  send_result_int(NAME_TEMP, val_temp);
  val_temp = dht.readTemperature();
  send_result_int(NAME_HUM, val_hum);
#endif
#ifdef NAME_A0
  digitalWrite(13, LOW);
  val_a0 = get_analog_val(A0, SAMPLES_A0);
  send_result_int(NAME_A0, val_a0);
#endif
#ifdef NAME_A1
  digitalWrite(13, HIGH);
  val_a1 = get_analog_val(A1, SAMPLES_A1);
  send_result_int(NAME_A1, val_a1);
#endif
#ifdef NAME_A2
  digitalWrite(13, LOW);
  val_a2 = get_analog_val(A2, SAMPLES_A2);
  send_result_int(NAME_A2, val_a2);
#endif
#ifdef NAME_A3
  digitalWrite(13, HIGH);
  val_a3 = get_analog_val(A3, SAMPLES_A3);
  send_result_int(NAME_A3, val_a3);
#endif
#ifdef NAME_A4
  digitalWrite(13, LOW);
  val_a4 = get_analog_val(A4, SAMPLES_A4);
  send_result_int(NAME_A4, val_a4);
#endif
#ifdef NAME_A5
  digitalWrite(13, HIGH);
  val_a5 = get_analog_val(A5, SAMPLES_A5);
  send_result_int(NAME_A5, val_a5);
#endif
#ifdef NAME_A6
  digitalWrite(13, LOW);
  val_a6 = get_analog_val(A6, SAMPLES_A6);
  send_result_int(NAME_A6, val_a6);
#endif
#ifdef PIN_DUST_PM25
  digitalWrite(13, HIGH);
  store_dust_timings(PIN_DUST_PM25, slots_low_duration_pm25, slots_sample_time_pm25);
  val_dust_pm25 = calculate_dust_mg(slots_low_duration_pm25, slots_sample_time_pm25);
  send_result_int("P25", val_dust_pm25);
#endif
#ifdef PIN_DUST_PM10
  digitalWrite(13, LOW);
  store_dust_timings(PIN_DUST_PM10, slots_low_duration_pm10, slots_sample_time_pm10);
  val_dust_pm10 = calculate_dust_mg(slots_low_duration_pm10, slots_sample_time_pm10);
  send_result_int("P10", val_dust_pm10);
#endif
#if defined(PIN_DUST_PM10) || defined(PIN_DUST_PM25)
  send_result_int("AQI", get_aqi(val_dust_pm10 - val_dust_pm25));
  // increase or reset slot counter
  slot_counter = (++slot_counter == SAMPLE_SLOTS) ? 0 : slot_counter;
  //Serial.print("slot counter ");
  //Serial.println(slot_counter);
#endif
}

