#include "StateMachine.h"

extern StateMachine stateMachine;
extern float loopTimeSec;

// States
void *initState() {
    stabilization.Idle();
    while (!stabilization.AreAttitudeOffsetsComputed())
        stabilization.AttitudeComputeOffsets();

    if (Rx.GetFlyingMode() != disarmed)
        return initState;
    else if (stabilization.AreAttitudeOffsetsComputed())
        return startingState;

    return initState;
}
void *startingState() {
    stabilization.Idle();
    stateMachine.ActivateBuzzer(500);
    int state = Rx.GetFlyingMode();
    delay(500);
    if (state != Rx.GetFlyingMode()) // Check it was not a transitory switch state
        return startingState;
    if ((state == angle) || (state == accro)) {
        Serial.println(F("stateMachine.state != disarmed MODE"));
        // PrintSettings();
        if (state == angle)
            return angleState;
        if (state == accro)
            return accroState;
    }
    return startingState;
}

void *angleState() {
    uint8_t throttle =
            Rx.GetThrottle(stabilization.GetESCsMinPower(), stabilization.GetESCsMaxThrottle());
    if (throttle > stabilization.GetESCIdleThreshold()) {
        stateMachine.throttleWasHigh = true;
        stabilization.Angle(loopTimeSec, Rx, throttle);

        // Allow to change flying mode during flight
        if (Rx.GetFlyingMode() == accro) {
            Serial.println(F("Flying mode changed from angle to accro"));
            return accroState;
        }
    } else {
        // after 20s without pwr
        stabilization.ResetPID(throttle);
        if (stateMachine.IsSafetyStateNeeded()) // Safety cut mngt: set safety cut
            return safetyState;
    }
    return angleState;
}

void *accroState() {
    uint8_t throttle =
            Rx.GetThrottle(stabilization.GetESCsMinPower(), stabilization.GetESCsMaxThrottle());
    if (throttle > stabilization.GetESCIdleThreshold()) {
        stateMachine.throttleWasHigh = true;

        stabilization.Accro(loopTimeSec, Rx, throttle);

        // Allow to change flying mode during flight
        if (Rx.GetFlyingMode() == angle) {
            Serial.println(F("Flying mode changed from accro to angle"));
            return angleState;
        }
    } else {
        // after 5s without pwr
        stabilization.ResetPID(throttle);
        if (stateMachine.IsSafetyStateNeeded()) // Safety cut mngt: set safety cut
            return safetyState;
    }
    return accroState;
}

void *safetyState() {
    stabilization.Idle();
    stateMachine.ActivateBuzzer(500);
    Rx.GetFlyingMode();
    if (Rx.GetFlyingMode() != disarmed) {
        stabilization.Idle();
        return safetyState;
    } else {
        return disarmedState;
    }
}

void *disarmedState() {
    stabilization.Idle();
    stateMachine.ActivateBuzzer(500);
    int state = Rx.GetFlyingMode();
    delay(500);
    // Check it was not a transitory switch state
    if (state != Rx.GetFlyingMode())
        return disarmedState;
    if (state != disarmed) {
        stateMachine.throttleWasHigh = true;
        if (state == angle) {
            Serial.println(F("ANGLE MODE"));
            return angleState;
        } else if (state == accro) {
            Serial.println(F("ACCRO MODE"));
            return accroState;
        }
    }
    return disarmedState;
}

void StateMachine::Init() {
    // Buzzer
    pinMode(BUZZER_PIN, OUTPUT);

    elapsedTime.Init(0);
    timeBuzzer.Init(0);
    setBuzzer = false;
}

// Activate buzzer after x minutes of power idle
void StateMachine::ActivateBuzzer(int _duration) {
    if (setBuzzer) {
        Time time;
        time.Init(0);
        while ((time.GetExecutionTimeMilliseconds(0)) < _duration) {
            digitalWrite(BUZZER_PIN, HIGH);
            delayMicroseconds(1800);
            digitalWrite(BUZZER_PIN, LOW);
            delay(10);
            wdt_reset();
            Serial.println(F("BUZZZZZ"));
        }
    } else if (timeBuzzer.GetExecutionTimeSeconds(0) > 120) { // Activate buzzer after 2 minutes
        setBuzzer = true;
    }
}

// Auto disarm when throttle is idle since a long period
bool StateMachine::IsSafetyStateNeeded() {
    if (throttleWasHigh) {
        Serial.println(F("Throttle just setted low!"));
        Init();
        throttleWasHigh = false;
    } else if (elapsedTime.GetExecutionTimeSeconds(0) > delayThresholdSec) {
        Serial.print(delayThresholdSec);
        Serial.println(F(" sec without power, system DISARMED!"));
        return true;
    }
    return false;
}
