#include <Arduino.h>
#include <avr/sleep.h>

#include <Wire.h>
#include <RtcDS3231.h>
#include <HX711.h>
#include <EepromAT24C32.h>

RtcDS3231<TwoWire> Rtc(Wire);
EepromAt24c32<TwoWire> RtcEeprom(Wire);HX711 scale;

// void printDateTime(const RtcDateTime &dt);


const int rtcPowerPin = 4;
const int wakeUpPin = 7;
const int ledPin = 17;

const int scaleDataPin = 21;
const int scaleSckPin = 20;
const int scalePowerPin = 19;

const int secondsTillNextWakeup = 3;
const long interval = 50;

const int memTotal = 4096;
const int pageSize = 16;
const int pageCount = (memTotal / pageSize) - 1;

void wake()
{
    sleep_disable();
    detachInterrupt(digitalPinToInterrupt(wakeUpPin));
}

void sleepNow()
{
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    noInterrupts();
    sleep_enable();
    attachInterrupt(digitalPinToInterrupt(wakeUpPin), wake, LOW);
    interrupts();
    sleep_cpu();
}

void setup()
{
    delay(3000);

    Serial.begin(9660);
    Serial.println("Initializing.");

    pinMode(wakeUpPin, INPUT_PULLUP);
    pinMode(ledPin, OUTPUT);
    pinMode(scalePowerPin, OUTPUT);

    pinMode(rtcPowerPin, OUTPUT);
    digitalWrite(rtcPowerPin, HIGH);
    digitalWrite(scalePowerPin, HIGH);

    Rtc.Begin();
    RtcEeprom.Begin();

    scale.begin(scaleDataPin, scaleSckPin);
    scale.set_scale(1000.f); // this value is obtained by calibrating the scale with known weights; see the README for details
    scale.tare();            // reset the scale to 0

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

    if (!Rtc.IsDateTimeValid())
    {
        if (Rtc.LastError() != 0)
        {
            // we have a communications error
            // see https://www.arduino.cc/en/Reference/WireEndTransmission for
            // what the number means
            Serial.print("RTC communications error = ");
            Serial.println(Rtc.LastError());
        }
        else
        {
            Serial.println("RTC lost confidence in the DateTime!");
            Rtc.SetDateTime(compiled);
        }
    }

    if (!Rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled)
    {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        Rtc.SetDateTime(compiled);
    }

    Rtc.Enable32kHzPin(false);
    Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmOne);
}

void loop()
{
    pinMode(rtcPowerPin, OUTPUT);
    pinMode(scalePowerPin, OUTPUT);
    digitalWrite(scalePowerPin, HIGH);
    digitalWrite(rtcPowerPin, HIGH);
    digitalWrite(ledPin, LOW);

    delay(interval);

    scale.power_up();

    RtcDateTime now = Rtc.GetDateTime();
    RtcDateTime alarmTime = now + secondsTillNextWakeup;
    DS3231AlarmOne alarm1(
        alarmTime.Day(),
        alarmTime.Hour(),
        alarmTime.Minute(),
        alarmTime.Second(),
        DS3231AlarmOneControl_HoursMinutesSecondsMatch);

    Rtc.SetAlarmOne(alarm1);
    Rtc.LatchAlarmsTriggeredFlags();

    RtcTemperature temp = Rtc.GetTemperature();

    int lastA = -1;
    int lastB = -1;

    for (int i = 0; i <= pageCount; i++)
    {
        uint8_t x = RtcEeprom.GetMemory(i * pageSize);

        if ((char)x == 'a')
        {
            lastA = i;
        }

        if ((char)x == 'b')
        {
            lastB = i;
        }
    }

    int head = -1;
    uint8_t next = 'c';

    if (lastA == pageCount)
    {
        head = pageCount;
        next = 'b';
    }

    if (lastB == pageCount)
    {
        head = pageCount;
        next = 'a';
    }

    if (lastA > lastB)
    {
        head = lastB;
        next = 'b';
    }

    if (lastB > lastA)
    {
        head = lastA;
        next = 'a';
    }

    head = (head + 1) % pageCount;

    // Serial.print("Last a: ");
    // Serial.print(lastA);
    // Serial.print(", Last b: ");
    // Serial.print(lastB);
    // Serial.print(", HEAD: ");
    // Serial.print(head);
    // Serial.print(", next: ");
    // Serial.println((char)next);

    String buffer;
    buffer += String((char)next);
    buffer += String(scale.get_units(1), 2);
    buffer += F(" kg, ");
    buffer += String(temp.AsFloatDegC(), 2);
    buffer += F(" °C");
    // Serial.println(buffer);
    char data[pageSize];
    buffer.toCharArray(data, pageSize);

    RtcEeprom.SetMemory(head * pageSize, (const uint8_t *)data, sizeof(data) - 1);

    for (int i = 0; i <= pageCount; i++)
    {
        uint8_t buff[pageSize + 1];
        uint8_t gotten = RtcEeprom.GetMemory(i * pageSize, buff, pageSize);
        Serial.print(i);
        Serial.print(":\t");
        Serial.print("data read (");
        Serial.print(gotten);
        Serial.print(") = \"");
        for (uint8_t ch = 0; ch < gotten; ch++)
        {
            Serial.print((char)buff[ch]);
        }
        Serial.println("\"");
    }

    Serial.flush();
    scale.power_down();

    digitalWrite(scalePowerPin, LOW);
    digitalWrite(ledPin, HIGH);
    digitalWrite(rtcPowerPin, LOW);
    pinMode(rtcPowerPin, INPUT);
    pinMode(scalePowerPin, INPUT);

    sleepNow();
}


/