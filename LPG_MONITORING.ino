#define BLYNK_TEMPLATE_ID "TMPL3NuI4s4zt"
#define BLYNK_TEMPLATE_NAME "LPG MONITORING"
#define BLYNK_AUTH_TOKEN "4hRbvLDD8NI9rvnrRPNzdWbNGpSXUPuT"

#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "HX711.h"

// =============================
// PIN DEFINITIONS
// =============================

const int MQ2_AO = 36;

const int HX711_DT = 34;
const int HX711_SCK = 32;

const int LCD_SDA = 21;
const int LCD_SCL = 22;

const int BUZZER_PIN = 25;
const int RED_LED_PIN = 26;
const int GREEN_LED_PIN = 27;

// =============================
// WIFI
// =============================

char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// =============================
// OBJECTS
// =============================

LiquidCrystal_I2C lcd(0x27, 16, 2);
HX711 scale;

// =============================
// LPG SETTINGS
// =============================

const float EMPTY_CYLINDER_WEIGHT = 25.0;
const float LPG_CAPACITY = 25.0;
const float FULL_CYLINDER_WEIGHT = 50.0;

const float DAILY_LPG_USAGE_KG = 1.0;

const float HX711_SCALE = 420.0;

// =============================
// GAS SENSOR SETTINGS
// =============================

const int GAS_LEAK_THRESHOLD_ADC = 3900;
const int GAS_SAFE_THRESHOLD_ADC = 3800;

// =============================
// VARIABLES
// =============================

float cylinderWeight = 0.0;
float lpgWeight = 0.0;
float lpgPercentage = 0.0;
float remainingDays = 0.0;

int gasValue = 0;

bool gasLeak = false;
bool redLedState = false;

bool lowLPGNotificationSent = false;

// =============================
// TIMERS
// =============================

unsigned long lastSensorRead = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastSerialPrint = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastBlynkUpdate = 0;

const unsigned long SENSOR_INTERVAL = 500;
const unsigned long LCD_INTERVAL = 500;
const unsigned long SERIAL_INTERVAL = 1000;
const unsigned long BLINK_INTERVAL = 300;
const unsigned long BLYNK_INTERVAL = 1000;

// =============================
// SETUP
// =============================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    pinMode(MQ2_AO, INPUT);

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);

    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);

    noTone(BUZZER_PIN);

    // =============================
    // LCD
    // =============================

    Wire.begin(LCD_SDA, LCD_SCL);

    lcd.init();
    lcd.backlight();

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("LPG MONITOR");

    lcd.setCursor(0, 1);
    lcd.print("Starting...");

    delay(2000);

    // =============================
    // HX711
    // =============================

    scale.begin(HX711_DT, HX711_SCK);
    scale.set_scale(HX711_SCALE);

    // =============================
    // SERIAL INFORMATION
    // =============================

    Serial.println();
    Serial.println("======================================");
    Serial.println("       LPG MONITORING SYSTEM");
    Serial.println("======================================");

    Serial.print("Empty cylinder : ");
    Serial.print(EMPTY_CYLINDER_WEIGHT);
    Serial.println(" kg");

    Serial.print("LPG capacity   : ");
    Serial.print(LPG_CAPACITY);
    Serial.println(" kg");

    Serial.print("Full cylinder  : ");
    Serial.print(FULL_CYLINDER_WEIGHT);
    Serial.println(" kg");

    Serial.print("Daily usage    : ");
    Serial.print(DAILY_LPG_USAGE_KG);
    Serial.println(" kg/day");

    Serial.print("HX711 scale    : ");
    Serial.println(HX711_SCALE);

    Serial.print("Gas leak ADC   : ");
    Serial.println(GAS_LEAK_THRESHOLD_ADC);

    Serial.print("Gas safe ADC   : ");
    Serial.println(GAS_SAFE_THRESHOLD_ADC);

    // =============================
    // BLYNK CONNECTION
    // =============================

    Serial.println();
    Serial.println("Connecting to Blynk...");

    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

    Serial.println("Blynk connected");

    Serial.println();
    Serial.println("SYSTEM READY");
    Serial.println("======================================");

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("SYSTEM READY");

    lcd.setCursor(0, 1);
    lcd.print("Monitoring...");

    delay(1500);

    lcd.clear();
}

// =============================
// READ LPG WEIGHT
// =============================

void readWeight()
{
    if (scale.wait_ready_timeout(100))
    {
        cylinderWeight = scale.get_units(5);

        if (cylinderWeight < 0)
        {
            cylinderWeight = 0;
        }

        if (cylinderWeight > FULL_CYLINDER_WEIGHT)
        {
            cylinderWeight = FULL_CYLINDER_WEIGHT;
        }

        // LPG = Total cylinder weight - empty cylinder weight
        lpgWeight = cylinderWeight - EMPTY_CYLINDER_WEIGHT;

        if (lpgWeight < 0)
        {
            lpgWeight = 0;
        }

        if (lpgWeight > LPG_CAPACITY)
        {
            lpgWeight = LPG_CAPACITY;
        }

        // LPG percentage
        lpgPercentage = (lpgWeight / LPG_CAPACITY) * 100.0;

        if (lpgPercentage < 0)
        {
            lpgPercentage = 0;
        }

        if (lpgPercentage > 100)
        {
            lpgPercentage = 100;
        }

        // Remaining days
        if (DAILY_LPG_USAGE_KG > 0)
        {
            remainingDays = lpgWeight / DAILY_LPG_USAGE_KG;
        }
        else
        {
            remainingDays = 0;
        }

        // =============================
        // LOW LPG EVENT
        // =============================

        if (remainingDays <= 2.0 && !lowLPGNotificationSent)
        {
            lowLPGNotificationSent = true;

            Blynk.logEvent(
                "lpg_low",
                String("LPG remaining: ") +
                String(lpgWeight, 1) +
                " kg. Approximately " +
                String(remainingDays, 1) +
                " days remaining."
            );

            Serial.println("BLYNK: LOW LPG NOTIFICATION SENT");
        }

        // Reset notification after LPG goes above 2 days
        if (remainingDays > 2.0)
        {
            lowLPGNotificationSent = false;
        }
    }
    else
    {
        Serial.println("ERROR: HX711 not ready!");
    }
}

// =============================
// READ MQ2 GAS SENSOR
// =============================

void readGasSensor()
{
    gasValue = analogRead(MQ2_AO);

    // =============================
    // GAS LEAK DETECTED
    // =============================

    if (!gasLeak &&
        gasValue >= GAS_LEAK_THRESHOLD_ADC)
    {
        gasLeak = true;

        Blynk.logEvent(
            "gas_leak",
            String("LPG leakage detected! MQ2 ADC: ") +
            String(gasValue)
        );

        Serial.println();
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        Serial.println("!!! LPG LEAKAGE DETECTED !!!");
        Serial.println("!!! BLYNK NOTIFICATION SENT!");
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        Serial.println();
    }

    // =============================
    // GAS LEAK CLEARED
    // =============================

    if (gasLeak &&
        gasValue <= GAS_SAFE_THRESHOLD_ADC)
    {
        gasLeak = false;
        redLedState = false;

        Serial.println();
        Serial.println("--------------------------------");
        Serial.println("LPG LEAKAGE CLEARED");
        Serial.println("NO LEAK DETECTED");
        Serial.println("--------------------------------");
        Serial.println();
    }
}

// =============================
// LED + BUZZER
// =============================

void updateAlarmOutputs()
{
    if (gasLeak)
    {
        // Green LED OFF
        digitalWrite(GREEN_LED_PIN, LOW);

        // Red LED blinking
        if (millis() - lastBlinkTime >= BLINK_INTERVAL)
        {
            lastBlinkTime = millis();

            redLedState = !redLedState;

            digitalWrite(RED_LED_PIN, redLedState);
        }

        // Buzzer ON
        tone(BUZZER_PIN, 2000);
    }
    else
    {
        redLedState = false;

        // Red LED OFF
        digitalWrite(RED_LED_PIN, LOW);

        // Green LED ON
        digitalWrite(GREEN_LED_PIN, HIGH);

        // Buzzer OFF
        noTone(BUZZER_PIN);
    }
}

// =============================
// LCD
// =============================

void updateLCD()
{
    lcd.clear();

    // Leakage screen
    if (gasLeak)
    {
        lcd.setCursor(0, 0);
        lcd.print("LEAKAGE");

        lcd.setCursor(0, 1);
        lcd.print("DETECTED!");

        return;
    }

    // Normal LPG screen
    lcd.setCursor(0, 0);
    lcd.print("LPG:");
    lcd.print(lpgWeight, 1);
    lcd.print("kg");

    lcd.setCursor(0, 1);
    lcd.print("Days:");
    lcd.print(remainingDays, 1);
}

// =============================
// SEND ONLY 3 VALUES TO BLYNK
// =============================

void updateBlynk()
{
    // V0 = LPG level in kg
    Blynk.virtualWrite(V0, lpgWeight);

    // V1 = Estimated remaining days
    Blynk.virtualWrite(V1, remainingDays);

    // V2 = Gas status
    // 0 = SAFE
    // 1 = LEAKAGE
    Blynk.virtualWrite(V2, gasLeak ? 1 : 0);
}

// =============================
// SERIAL MONITOR
// =============================

void printSerialData()
{
    Serial.println();
    Serial.println("--------------------------------------");

    Serial.print("MQ2 ADC            : ");
    Serial.println(gasValue);

    Serial.print("Cylinder weight    : ");
    Serial.print(cylinderWeight, 2);
    Serial.println(" kg");

    Serial.print("LPG weight         : ");
    Serial.print(lpgWeight, 2);
    Serial.println(" kg");

    Serial.print("LPG percentage     : ");
    Serial.print(lpgPercentage, 1);
    Serial.println("%");

    Serial.print("Remaining days     : ");
    Serial.print(remainingDays, 1);
    Serial.println(" days");

    Serial.print("Leak status        : ");

    if (gasLeak)
    {
        Serial.println("LEAKAGE DETECTED");
    }
    else
    {
        Serial.println("NO LEAK DETECTED");
    }

    Serial.println("--------------------------------------");
}

// =============================
// MAIN LOOP
// =============================

void loop()
{
    // Keep Blynk connection alive
    Blynk.run();

    unsigned long currentMillis = millis();

    // =============================
    // SENSOR UPDATE
    // =============================

    if (currentMillis - lastSensorRead >= SENSOR_INTERVAL)
    {
        lastSensorRead = currentMillis;

        readWeight();
        readGasSensor();
        updateAlarmOutputs();
    }

    // =============================
    // LCD UPDATE
    // =============================

    if (currentMillis - lastLCDUpdate >= LCD_INTERVAL)
    {
        lastLCDUpdate = currentMillis;

        updateLCD();
    }

    // =============================
    // BLYNK UPDATE
    // =============================

    if (currentMillis - lastBlynkUpdate >= BLYNK_INTERVAL)
    {
        lastBlynkUpdate = currentMillis;

        updateBlynk();
    }

    // =============================
    // SERIAL UPDATE
    // =============================

    if (currentMillis - lastSerialPrint >= SERIAL_INTERVAL)
    {
        lastSerialPrint = currentMillis;

        printSerialData();
    }
}