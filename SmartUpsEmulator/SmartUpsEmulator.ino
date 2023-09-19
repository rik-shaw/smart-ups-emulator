#include <HIDPowerDevice.h>

#define MINUPDATEINTERVAL   26

int iIntTimer=0;

// For Sid, the project is located at: C:\Users\User\Documents\GitHub\smart-ups-emulator
/*
Todo: 
    Board voltage calibration. To EEPROM
    Via commands, Simulate battery voltage for easy PC comm testing
  Testing:
    See if need % left or just the shutdown cmd
    What kind of shut down does it do? (Sleep: Open app is open on start up)
    OS's tried so far: Win10 Home, Win10 Pro: Both do Sleep by default.
    Win10 Pro: I changed config and was able to set it to do a full shutdown.
    Appears the Shutdown requested/Immenent message is NOT what tells the PC to shut down, but instead the battery level(?)
    Try RPi Pico (but would need an external EEPROM for production version)
 
  Shutdown config on Win10 Pro: Settings, Power&Sleep, Additional Power Settings (green text in right side of window),
    For the power plan in use: click "Change plan settings", "Change advanced power settings", scroll down to Battery,
    Open "Critical battery action", "On battery:" use pulldown to select desired action.
*/

// String constants 
const char STRING_DEVICECHEMISTRY[] PROGMEM = "PbAc";
const char STRING_OEMVENDOR[] PROGMEM = "MyCoolUPS";
const char STRING_SERIAL[] PROGMEM = "UPS10"; 

const byte bDeviceChemistry = IDEVICECHEMISTRY;
const byte bOEMVendor = IOEMVENDOR;

uint16_t iPresentStatus = 0, iPreviousStatus = 0;
bool bForceShutdownPrior = false;

byte bRechargable = 1;
byte bCapacityMode = 2;  // units are in %%

// Physical parameters
const uint16_t iConfigVoltage = 1380;
uint16_t iVoltage =1300, iPrevVoltage = 0;
uint16_t iRunTimeToEmpty = 0, iPrevRunTimeToEmpty = 0;
uint16_t iAvgTimeToFull = 7200;
uint16_t iAvgTimeToEmpty = 7200;
uint16_t iRemainTimeLimit = 600;
int16_t  iDelayBe4Reboot = -1;
int16_t  iDelayBe4ShutDown = -1;

byte iAudibleAlarmCtrl = 2; // 1 - Disabled, 2 - Enabled, 3 - Muted


// Parameters for ACPI compliancy
const byte iDesignCapacity = 100;
byte iWarnCapacityLimit = 10; // warning at 10% 
byte iRemnCapacityLimit = 5; // low at 5% 
const byte bCapacityGranularity1 = 1;
const byte bCapacityGranularity2 = 1;
byte iFullChargeCapacity = 100;

byte iRemaining =0, iPrevRemaining=0;

int iRes=0;


void setup() {

  Serial.begin(57600);
    
  PowerDevice.begin();
  
  // Serial No is set in a special way as it forms Arduino port name
  PowerDevice.setSerial(STRING_SERIAL); 
  
  // Used for debugging purposes. 
  PowerDevice.setOutput(Serial);
  
  pinMode(4, INPUT_PULLUP); // ground this pin to simulate power failure. 
  pinMode(6, INPUT_PULLUP); // ground this pin to send only the shutdown command without changing bat charge
  pinMode(5, OUTPUT);  // output flushing 1 sec indicating that the arduino cycle is running. 
  pinMode(10, OUTPUT); // output is on once commuication is lost with the host, otherwise off.


  PowerDevice.setFeature(HID_PD_PRESENTSTATUS, &iPresentStatus, sizeof(iPresentStatus));
  
  PowerDevice.setFeature(HID_PD_RUNTIMETOEMPTY, &iRunTimeToEmpty, sizeof(iRunTimeToEmpty));
  PowerDevice.setFeature(HID_PD_AVERAGETIME2FULL, &iAvgTimeToFull, sizeof(iAvgTimeToFull));
  PowerDevice.setFeature(HID_PD_AVERAGETIME2EMPTY, &iAvgTimeToEmpty, sizeof(iAvgTimeToEmpty));
  PowerDevice.setFeature(HID_PD_REMAINTIMELIMIT, &iRemainTimeLimit, sizeof(iRemainTimeLimit));
  PowerDevice.setFeature(HID_PD_DELAYBE4REBOOT, &iDelayBe4Reboot, sizeof(iDelayBe4Reboot));
  PowerDevice.setFeature(HID_PD_DELAYBE4SHUTDOWN, &iDelayBe4ShutDown, sizeof(iDelayBe4ShutDown));
  
  PowerDevice.setFeature(HID_PD_RECHARGEABLE, &bRechargable, sizeof(bRechargable));
  PowerDevice.setFeature(HID_PD_CAPACITYMODE, &bCapacityMode, sizeof(bCapacityMode));
  PowerDevice.setFeature(HID_PD_CONFIGVOLTAGE, &iConfigVoltage, sizeof(iConfigVoltage));
  PowerDevice.setFeature(HID_PD_VOLTAGE, &iVoltage, sizeof(iVoltage));

  PowerDevice.setStringFeature(HID_PD_IDEVICECHEMISTRY, &bDeviceChemistry, STRING_DEVICECHEMISTRY);
  PowerDevice.setStringFeature(HID_PD_IOEMINFORMATION, &bOEMVendor, STRING_OEMVENDOR);

  PowerDevice.setFeature(HID_PD_AUDIBLEALARMCTRL, &iAudibleAlarmCtrl, sizeof(iAudibleAlarmCtrl));

  PowerDevice.setFeature(HID_PD_DESIGNCAPACITY, &iDesignCapacity, sizeof(iDesignCapacity));
  PowerDevice.setFeature(HID_PD_FULLCHRGECAPACITY, &iFullChargeCapacity, sizeof(iFullChargeCapacity));
  PowerDevice.setFeature(HID_PD_REMAININGCAPACITY, &iRemaining, sizeof(iRemaining));
  PowerDevice.setFeature(HID_PD_WARNCAPACITYLIMIT, &iWarnCapacityLimit, sizeof(iWarnCapacityLimit));
  PowerDevice.setFeature(HID_PD_REMNCAPACITYLIMIT, &iRemnCapacityLimit, sizeof(iRemnCapacityLimit));
  PowerDevice.setFeature(HID_PD_CPCTYGRANULARITY1, &bCapacityGranularity1, sizeof(bCapacityGranularity1));
  PowerDevice.setFeature(HID_PD_CPCTYGRANULARITY2, &bCapacityGranularity2, sizeof(bCapacityGranularity2));

}

void loop() {
  
  
  //*********** Measurements Unit ****************************
  bool bCharging = digitalRead(4);
  bool bForceShutdown = digitalRead(6) ? false : true;
  bool bACPresent = bCharging;    // TODO - replace with sensor
  bool bDischarging = !bCharging; // TODO - replace with sensor
  int iA7 = analogRead(A0);       // TODO - this is for debug only. Replace with charge estimation

  static uint8_t charIndex = 0;   // For tracking input chars from serial port
  static char inLine[16];

  if (Serial.available() > 0)
  {
      char c = Serial.read();

      switch (toupper(c))
      {
      case 'H':
          Serial.println("Here's some Help!");
          break;

      default:
          Serial.println("Try 'H'");
          break;

      }
  }

      //if (charIndex < sizeof(inLine) - 1)
      //{
      //    inLine[charIndex] = Serial.read();
      //    inLine[charIndex + 1] = 0;
      //}



  iRemaining = (byte)(round((float)100*iA7/1024));
  iRunTimeToEmpty = (uint16_t)round((float)iAvgTimeToEmpty*iRemaining/100);
  
    // Charging
  if(bCharging) 
    bitSet(iPresentStatus,PRESENTSTATUS_CHARGING);
  else
    bitClear(iPresentStatus,PRESENTSTATUS_CHARGING);
  if(bACPresent) 
    bitSet(iPresentStatus,PRESENTSTATUS_ACPRESENT);
  else
    bitClear(iPresentStatus,PRESENTSTATUS_ACPRESENT);
  if(iRemaining == iFullChargeCapacity) 
    bitSet(iPresentStatus,PRESENTSTATUS_FULLCHARGE);
  else 
    bitClear(iPresentStatus,PRESENTSTATUS_FULLCHARGE);
    
  // Discharging
  if(bDischarging) {
    bitSet(iPresentStatus,PRESENTSTATUS_DISCHARGING);
    // if(iRemaining < iRemnCapacityLimit) bitSet(iPresentStatus,PRESENTSTATUS_BELOWRCL);
    
    if(iRunTimeToEmpty < iRemainTimeLimit) 
      bitSet(iPresentStatus, PRESENTSTATUS_RTLEXPIRED);
    else
      bitClear(iPresentStatus, PRESENTSTATUS_RTLEXPIRED);

  }
  else {
    bitClear(iPresentStatus,PRESENTSTATUS_DISCHARGING);
    bitClear(iPresentStatus, PRESENTSTATUS_RTLEXPIRED);
  }

  // Shutdown requested
  if(iDelayBe4ShutDown > 0 ) {
      bitSet(iPresentStatus, PRESENTSTATUS_SHUTDOWNREQ);
      Serial.println("shutdown requested");
  }
  else
    bitClear(iPresentStatus, PRESENTSTATUS_SHUTDOWNREQ);

  if (bForceShutdown)
  {
      bitSet(iPresentStatus, PRESENTSTATUS_SHUTDOWNREQ);
  }

  // Shutdown imminent
  if((iPresentStatus & (1 << PRESENTSTATUS_SHUTDOWNREQ)) || 
     (iPresentStatus & (1 << PRESENTSTATUS_RTLEXPIRED))) {
    bitSet(iPresentStatus, PRESENTSTATUS_SHUTDOWNIMNT);
    Serial.println("shutdown imminent");
  }
  else
    bitClear(iPresentStatus, PRESENTSTATUS_SHUTDOWNIMNT);


  
  bitSet(iPresentStatus ,PRESENTSTATUS_BATTPRESENT);

  

  //************ Delay ****************************************  
  delay(1000);
  iIntTimer++;
  digitalWrite(5, HIGH);   // turn the LED on (HIGH is the voltage level);
  delay(1000);
  iIntTimer++;
  digitalWrite(5, LOW);   // turn the LED off;

  //************ Check if we are still online ******************

  

  //************ Bulk send or interrupt ***********************

  if((iPresentStatus != iPreviousStatus) || (iRemaining != iPrevRemaining) || (iRunTimeToEmpty != iPrevRunTimeToEmpty) || (iIntTimer>MINUPDATEINTERVAL) 
     || (bForceShutdownPrior != bForceShutdown)) {

    PowerDevice.sendReport(HID_PD_REMAININGCAPACITY, &iRemaining, sizeof(iRemaining));
    if(bDischarging) PowerDevice.sendReport(HID_PD_RUNTIMETOEMPTY, &iRunTimeToEmpty, sizeof(iRunTimeToEmpty));
    iRes = PowerDevice.sendReport(HID_PD_PRESENTSTATUS, &iPresentStatus, sizeof(iPresentStatus));

    if(iRes <0 ) {
      digitalWrite(10, HIGH);
    }
    else
      digitalWrite(10, LOW);
        
    iIntTimer = 0;
    iPreviousStatus = iPresentStatus;
    iPrevRemaining = iRemaining;
    iPrevRunTimeToEmpty = iRunTimeToEmpty;
    bForceShutdownPrior = bForceShutdown;
  }
  

  Serial.print("Remaining: ");
  Serial.println(iRemaining);
  Serial.print("To Empty mm:ss ");
  Serial.print(iRunTimeToEmpty/60);
  Serial.print(":");
  Serial.println(iRunTimeToEmpty%60);
  Serial.print("Communications state with PC (negative=bad): ");
  Serial.println(iRes);
  
}
