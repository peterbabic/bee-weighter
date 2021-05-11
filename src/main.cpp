#include <Arduino.h>
#include <avr/sleep.h>

#include <Wire.h>
#include <RtcDS3231.h>

RtcDS3231<TwoWire> Rtc(Wire);

const int rtcPowerPin = 4;
const int wakeUpPin = 7;
const int ledPin = 17;

const int secondsTillNextWakeup = 5;
const long interval = 50;

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
    pinMode(wakeUpPin, INPUT_PULLUP);
    pinMode(ledPin, OUTPUT);

    pinMode(rtcPowerPin, OUTPUT);
    digitalWrite(rtcPowerPin, HIGH);

    // Serial.begin(9660);
    Rtc.Begin();

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

    if (!Rtc.IsDateTimeValid())
    {
        if (Rtc.LastError() != 0)
        {
            // we have a communications error
            // see https://www.arduino.cc/en/Reference/WireEndTransmission for
            // what the number means
            // Serial.print("RTC communications error = ");
            // Serial.println(Rtc.LastError());
        }
        else
        {
            // Serial.println("RTC lost confidence in the DateTime!");
            Rtc.SetDateTime(compiled);
        }
    }

    if (!Rtc.GetIsRunning())
    {
        // Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled)
    {
        // Serial.println("RTC is older than compile time!  (Updating DateTime)");
        Rtc.SetDateTime(compiled);
    }

    Rtc.Enable32kHzPin(false);
    Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmOne);
}

void loop()
{
    pinMode(rtcPowerPin, OUTPUT);
    digitalWrite(rtcPowerPin, HIGH);
    digitalWrite(ledPin, LOW);

    delay(interval);

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

    digitalWrite(ledPin, HIGH);
    digitalWrite(rtcPowerPin, LOW);
    pinMode(rtcPowerPin, INPUT);

    sleepNow();
}