#include <Arduino.h>
#include <avr/sleep.h>

#include <Wire.h>
#include <RtcDS3231.h>
#include <HX711.h>
#include <EepromAT24C32.h>

#define countof(a) (sizeof(a) / sizeof(a[0]))

RtcDS3231<TwoWire> Rtc(Wire);
EepromAt24c32<TwoWire> RtcEeprom(Wire);
HX711 scale;

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

    // const char data[] = "0               ";
    // for (int i = 0; i <= pageCount; i++)
    // {
    //     RtcEeprom.SetMemory(i * pageSize, (const uint8_t *)data, sizeof(data) - 1); // remove the null terminator strings add
    //     Serial.print('.');
    // }
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

    uint16_t highest = 0;
    uint32_t highestEpoch = 0;
    uint8_t epochBuff[40];

    for (int i = 0; i <= pageCount; i++)
    {
        uint16_t epochGotten = RtcEeprom.GetMemory(i * pageSize, epochBuff, 10);
        uint32_t epoch = strtol((const char *)epochBuff, NULL, 0);

        if (epoch > highestEpoch)
        {
            highest = i;
            highestEpoch = epoch;
        }

        // Serial.print("gotten: ");
        // Serial.print(epochGotten);
        // Serial.print(", buff: ");
        // for (uint8_t ch = 0; ch < epochGotten; ch++)
        // {
        //     Serial.print((char)epochBuff[ch]);
        // }
        // Serial.print(", i: ");
        // Serial.print(i);
        // Serial.print(", epoch: ");
        // Serial.println(epoch);
    }

    char memString[pageSize + 1];
    int16_t weight = scale.get_units(1) * 100.00f;
    // b674250562+12744
    snprintf_P(memString,
               countof(memString),
               PSTR("%10ld%+6d"),
               now.TotalSeconds(),
               weight);
    Serial.print("[");
    Serial.print(memString);
    Serial.println("]");

    uint16_t head = (highest + 1) % (pageCount + 1);

    Serial.print("Highest found is at position ");
    Serial.print(highest);
    Serial.print(" having an epoch ");
    Serial.println(highestEpoch);
    Serial.print("Head is now at ");
    Serial.println(head);

    RtcEeprom.SetMemory(head * pageSize, (const uint8_t *)memString, pageSize);

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
