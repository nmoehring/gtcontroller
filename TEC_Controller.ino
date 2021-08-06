#include <RTClib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

String padString(String s, int strLen = 16, bool back = true, String fillChar = " ");

const uint8_t smallAirFanSwitchPin = 9;
const uint8_t largeAirFanSwitchPin = 10;
const uint8_t airTECSwitchPin = 11;
const uint8_t lightSwitchPin = 8;
//const uint8_t waterTECSwitchPin = 7;
const uint8_t uvSwitchPin = 4;
const uint8_t tempSenseDataPin = 2;
const int buttonPin = 3;

uint8_t waterTempHigh = 68;
uint8_t waterTempTarget = 55;
uint8_t airTempTarget = 69;
uint8_t airTempHigh = 71;

uint8_t airTemp = 0;
uint8_t waterTemp = 0;

bool airCoolingActive = false;
bool waterCoolingActive = false;
bool airTECActive = false;
bool airFansActive = false;
bool waterLEDHigh = false;
bool waterTECActive = false;
bool uvActive = false;
bool lightActive = false;
bool isWaterChangeDay = false;
bool buttonActive = false;
int buttonState = 0;
unsigned long buttonPressTimer = 0;
unsigned long loopTimer = 0;
unsigned long updateDisplayTimer = 0;
unsigned long runScheduleTimer = 0;
unsigned long timeoutTimer = 0;
unsigned long blinkTimer = 0;
uint8_t menuPosition = 0;
uint8_t blinkState = 0;

OneWire tempSenseDataWire{tempSenseDataPin};
DallasTemperature sensors{&tempSenseDataWire};
RTC_DS3231 clock;
LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display

enum DisplayModes {timeDisplay, tempDisplay, scheduleDisplay};
enum ButtonPressType {noPress, shortPress, longPress, programPress};
ButtonPressType buttonPress = noPress;
DisplayModes displayMode = timeDisplay;
DisplayModes lastDisplay = scheduleDisplay;
bool onMainMenu = true;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
uint8_t airProbe = 0;
uint8_t waterProbe = 1;
enum ScheduleIdxNames {On1Hour, On1Min, Off1Hour, Off1Min, On2Hour, On2Min, Off2Hour, Off2Min, UVOnHour, UVOnMin, UVOffHour, UVOffMin, DayAirHi, DayAirTg, NiteAirHi, NiteAirTg, DayH2OHi, DayH2OTg, NiteH2OHi, NiteH2OTg};
int schedule[13][20] = {{4, 0, 20, 0, 2, 30, 2, 30, 7, 0, 7, 0, 68, 55, 66, 55, 68, 66, 70, 68}, //Week 1, Veg 1, Seed/Clone, 16-8
                      {4, 0, 16, 0, 21, 30, 22, 30, 7, 0, 7, 0, 68, 55, 66, 55, 68, 66, 70, 68}, //Week 2, Veg 2, Early Grow, 12-5.5-1-5.5
                      {4, 0, 16, 0, 21, 30, 22, 30, 7, 0, 7, 0, 68, 55, 66, 55, 68, 66, 70, 68}, //Week 3, Veg 3, Early Grow, 12-5.5-1-5.5
                      {4, 0, 16, 0, 21, 30, 22, 30, 7, 0, 7, 0, 68, 55, 66, 55, 68, 66, 70, 68}, //Week 4, Veg 4, Late Grow, 12-5.5-1-5.5
                      {4, 0, 16, 0, 21, 30, 21, 30, 7, 0, 7, 0, 68, 55, 66, 55, 68, 66, 70, 68}, //Week 5, Bloom 1, Early Bloom, 12-12
                      {4, 0, 16, 0, 21, 30, 21, 30, 7, 0, 7, 0, 68, 55, 66, 55, 68, 66, 70, 68}, //Week 6, Bloom 2, Early Bloom, 12-12
                      {4, 0, 15, 0, 21, 30, 21, 30, 7, 0, 7, 10, 68, 55, 66, 55, 68, 66, 70, 68}, //Week 7, Bloom 3, Mid Bloom, 11-13
                      {4, 0, 15, 0, 21, 30, 21, 30, 7, 0, 7, 20, 68, 55, 66, 55, 68, 66, 70, 68}, //Week 8, Bloom 4, Mid Bloom, 11-13
                      {4, 0, 15, 0, 21, 30, 21, 30, 7, 0, 7, 40, 68, 55, 66, 55, 68, 66, 70, 68}, //Week 9, Bloom 5, Mid Bloom, 11-13
                      //{9, 0, 20, 0, 2, 30, 2, 30, 7, 0, 7, 40, 68, 55, 66, 55, 68, 66, 70, 68}, //Extra week, Extra Bloom, Mid Bloom, 11-13
                      {4, 0, 14, 0, 21, 30, 21, 30, 7, 0, 8, 0, 68, 55, 66, 55, 70, 68, 68, 66}, //Week 10, Bloom 6, Late Bloom, 10-14, Low Nite temps when UV is high
                      {4, 0, 14, 0, 21, 30, 21, 30, 7, 0, 8, 0, 68, 55, 66, 55, 70, 68, 68, 66}, //Week 11, Bloom 7, Late Bloom, 10-14, Low Nite temps when UV is high
                      {4, 0, 13, 0, 21, 30, 21, 30, 7, 0, 7, 10, 68, 55, 66, 55, 70, 68, 68, 66}, //Week 12, Bloom 8, Ripen, 9-15, Low Nite temps @ late growth for colors
                      {4, 0, 12, 0, 21, 30, 21, 30, 7, 0, 7, 5, 68, 55, 66, 55, 68, 66, 68, 66}}; //Week 13, Bloom 9, Flush, 8-16

DateTime startDay = DateTime(2021, 4, 25, schedule[0][On1Hour], schedule[0][On1Min], 0);
DateTime endDay = startDay + TimeSpan(7*13, 0, 0, 0);
DateTime scheduleRunTime = startDay - TimeSpan(100, 0, 0, 0);
uint8_t scheduleWeek = 0;

void setup() {
  lcd.init();  //initialize the lcd
  lcd.backlight();  //open the backlight
  
  sensors.begin(); //Init DallasTemperature library
  pinMode(uvSwitchPin, OUTPUT);
  //pinMode(waterTECSwitchPin, OUTPUT);
  pinMode(lightSwitchPin, OUTPUT);
  pinMode(buttonPin, INPUT);
  
  if (Serial)
    Serial.begin(9600);

  if (!clock.begin())
  {
    Serial.println("Couldn't find RTC");
    while(1)
      delay(100000); //Can't continue without RTC
  }
  //clock.adjust(DateTime(2021, 5, 31, 13, 37, 5));
  readStartDay();
  runSchedule(clock.now());
}

void loop() {
  //Calculate time passed on timers-----------------------
  DateTime now = clock.now();
  unsigned long nowMillis = millis();
  unsigned long updateDisplayTimeElapsed = abs(nowMillis-updateDisplayTimer);
  unsigned long loopTimeElapsed = abs(nowMillis-loopTimer);
  unsigned long runScheduleTimeElapsed = abs(nowMillis-loopTimer);
  unsigned long buttonPressTimeElapsed = abs(nowMillis-buttonPressTimer);
  unsigned long timeoutTimeElapsed = abs(nowMillis-timeoutTimer);
  unsigned long blinkTimeElapsed = abs(nowMillis-blinkTimer);
  bool menuSwitch = false;

  //Look for button presses-----------------------------
  buttonState = digitalRead(buttonPin);
  if (buttonState == HIGH and not buttonActive)
  {
    buttonPressTimer = nowMillis;
    buttonActive = true;
  }
  else if (buttonState == LOW and buttonActive)
  {
    buttonActive = false;
    buttonPress = (buttonPressTimeElapsed >= 600 ? longPress : shortPress);
  }

  //Handle button presses-------------------------------
  if (buttonPress == shortPress)
  {
    if (onMainMenu)
    {
      lcd.clear();
      displayMode = (displayMode < lastDisplay ? DisplayModes(int(displayMode)+1) : timeDisplay);
    }
  }
  else if (buttonPress == longPress)
  {
    if (onMainMenu)
    {
      onMainMenu = false;
      buttonPress = programPress;
      menuSwitch = true;
    }
  }  

  //Timeout display and go back to time display-----------------
  if (onMainMenu and displayMode != timeDisplay and not buttonActive and (buttonPress == noPress or buttonPress == programPress) and timeoutTimeElapsed >= 60000)
    displayMode = timeDisplay;
  else if (displayMode != timeDisplay and (buttonActive or (buttonPress != noPress and buttonPress != programPress)))
    timeoutTimer = nowMillis;

  //Update displays--------------------------------------
  if (displayMode == timeDisplay and (updateDisplayTimeElapsed >= 250 or buttonPress != noPress))
  {
    updateDisplayTimer = nowMillis;
    lcd.setCursor(0,0);
    String hourStr = String(now.hour()), minuteStr = String(now.minute()), secondStr = String(now.second());
    if (hourStr.length() == 1) {hourStr = "0" + hourStr;}
    if (minuteStr.length() == 1) {minuteStr = "0" + minuteStr;}
    if (secondStr.length() == 1) {secondStr = "0" + secondStr;}
    String timeStr = "Time: " + hourStr + ":" + minuteStr + ":" + secondStr;
    
    lcd.print(padString(timeStr));

    lcd.setCursor(0,1);
    if (isWaterChangeDay and not uvActive and lightActive)
      lcd.print(padString("Change h2o now!"));
    else if (lightActive and not uvActive)
      lcd.print(padString("Light:On, UV:Off"));
    else if (not lightActive and not uvActive)
      lcd.print(padString("PLANTS SLEEPING!"));
    else if (lightActive and uvActive)
      lcd.print(padString("UV ON, DONT LOOK"));
    else
      lcd.print(padString("ERR: UV AT NIGHT"));
  }
  else if (displayMode == tempDisplay and (updateDisplayTimeElapsed >= 5000 or buttonPress != noPress))
  {
    updateDisplayTimer = nowMillis;
    
    sensors.requestTemperatures();
    airTemp = sensors.getTempFByIndex(airProbe);
    waterTemp = sensors.getTempFByIndex(waterProbe);
    String Tw = String(waterTemp), Ta = String(airTemp);
    String Twr = "|"+String(waterTempTarget)+"-"+String(waterTempHigh);
    String Tar = "|"+String(airTempTarget)+"-"+String(airTempHigh);
    String wc = (waterCoolingActive ? "On" : "Off");
    String ac = (airCoolingActive ? "On" : "Off");
    if (onMainMenu)
    {
      lcd.setCursor(0,0);
      lcd.print(padString("W:" + Tw + Twr + " C:" + wc));
      lcd.setCursor(0,1);
      lcd.print(padString("A:" + Ta + Tar + " C:" + ac));
    }
    else
    {
      lcd.setCursor(0,0);
      lcd.print(padString("W:"+Tw+" A:"+Ta));
      lcd.setCursor(0,1);
      if (menuPosition == 0)
        lcd.print("*");
      else
        lcd.print(" ");
      lcd.print("SWAP ");
      if (menuPosition == 1)
        lcd.print("*");
      else
        lcd.print(" ");
      lcd.print("EXIT");

      if (buttonPress == shortPress)
      {
        if (menuPosition == 1)
          menuPosition = 0;
        else
          menuPosition++;
        buttonPress = programPress;
      }
      else if (buttonPress == longPress)
      {
        switch (menuPosition)
        {
          case 0: {int temp = airProbe; airProbe = waterProbe; waterProbe = temp; onMainMenu = true; menuPosition = 0; break;}
          case 1: {onMainMenu = true; menuPosition = 0; break;}
        }
        buttonPress = programPress;
      }
    }
  }
  else if (displayMode == scheduleDisplay and ((not onMainMenu and blinkTimeElapsed >= 375) or updateDisplayTimeElapsed >= 60000 or buttonPress != noPress))
  {
    if (buttonPress != noPress)
    {
      updateDisplayTimer = nowMillis;
      uint32_t nowUnix = now.unixtime();
      uint32_t startUnix = startDay.unixtime();
      double secDiff = (nowUnix > startUnix ? nowUnix - startUnix : 0);
      uint32_t week = ((secDiff / 604800L) < 13 ? secDiff / 604800L : 12);
      scheduleWeek = week;
    }
    
    lcd.setCursor(0,0);
    String line1 = "Wk";
    line1 += String(scheduleWeek+1);
    line1 += " ";
    line1 += "St:";
    if (not onMainMenu)
    {
      line1 += (blinkState == 0 and menuPosition == 0 ? "    " : String(startDay.year()));
      line1 += (blinkState == 0 and menuPosition == 1 ? "  " : padString(String(startDay.month()), 2, false, "0"));
      line1 += (blinkState == 0 and menuPosition == 2 ? "  " : padString(String(startDay.day()), 2, false, "0"));
      blinkState = (blinkState == 0 ? 1 : 0);
      blinkTimer = nowMillis;
      if (menuSwitch)
        startDay = DateTime(2021, 1, 1, startDay.hour(), startDay.minute(), startDay.second());
      if (buttonPress == shortPress)
      {
        if (menuPosition == 0)
          startDay = DateTime(startDay.year() < 2030 ? startDay.year()+1 : 2021, startDay.month(), startDay.day(), startDay.hour(), startDay.minute(), startDay.second());
        else if (menuPosition == 1)
          startDay = DateTime(startDay.year(), startDay.month() <= 12 ? startDay.month()+1 : 1, startDay.day(), startDay.hour(), startDay.minute(), startDay.second());
        else if (menuPosition == 2)
        {
          int sdmonth = startDay.month();
          startDay = startDay + TimeSpan(1,0,0,0);
          if (startDay.month() != sdmonth) 
            startDay = DateTime(startDay.year(), sdmonth, 1, startDay.hour(), startDay.minute(), startDay.second());
        }
      }
      else if (buttonPress == longPress)
      {
        if (menuPosition < 2)
          menuPosition++;
        else
        {
          onMainMenu = true;
          menuPosition = 0;
          writeStartDay();  
        }
      }
      if (buttonPress != programPress and buttonPress != noPress)
      {
        buttonPress = programPress;
        blinkState = 1;
      }
    }
    else
    {
      line1 += String(startDay.year());
      line1 += padString(String(startDay.month()), 2, false, "0");
      line1 += padString(String(startDay.day()), 2, false, "0");
    }

    lcd.setCursor(0,0);
    lcd.print(padString(line1));

    if (buttonPress != noPress)
    {
      float onHr1 = schedule[scheduleWeek][On1Hour] + (schedule[scheduleWeek][On1Min]/60.0);
      float offHr1 = schedule[scheduleWeek][Off1Hour] + (schedule[scheduleWeek][Off1Min]/60.0);
      float onHr2 = schedule[scheduleWeek][On2Hour] + (schedule[scheduleWeek][Off2Min]/60.0);
      float offHr2 = schedule[scheduleWeek][Off2Hour] + (schedule[scheduleWeek][Off2Min]/60.0);
      uint8_t uvOn = (schedule[scheduleWeek][UVOnHour]*60) + schedule[scheduleWeek][UVOnMin];
      uint8_t uvOff = (schedule[scheduleWeek][UVOffHour]*60) + schedule[scheduleWeek][UVOffMin];
      uint8_t uvMin = uvOff-uvOn;
      float lightCycle[4] = {offHr1-onHr1, onHr2-offHr1, offHr2-onHr2, (24-offHr2)+onHr1};
      if (lightCycle[2] == 0) lightCycle[1] += lightCycle[3];
      lcd.setCursor(0,1);
      String line2 = String(int(lightCycle[0]))+"-";
      line2 += (float(int(lightCycle[1])) == lightCycle[1] ? String(int(lightCycle[1])) : String(lightCycle[1]).substring(0,3));
      if (lightCycle[2] != 0) line2 += "-";
      if (lightCycle[2] != 0) line2 += (float(int(lightCycle[2])) == lightCycle[2] ? String(int(lightCycle[2])) : String(lightCycle[2]).substring(0,3))+"-";
      if (lightCycle[2] != 0) line2 += (float(int(lightCycle[3])) == lightCycle[3] ? String(int(lightCycle[3])) : String(lightCycle[3]).substring(0,3));
      line2 += " " + String(uvMin) + "m";
      lcd.print(padString(line2));
    }
  }

  //Execute the rest only every 30 seconds------------------------
  if (loopTimeElapsed < 30000 or buttonActive or buttonPress != noPress) 
  {
    buttonPress = noPress;
    return; 
  }
  else 
  {
    loopTimer = nowMillis; 
    lcd.setCursor(0,1);
    lcd.print(padString("Exec Loop"));
    buttonPress = programPress;
  }

  //Run schedule every 5 minutes (300,000 seconds)----------------
  if (runScheduleTimeElapsed >= 300000)
  {
    runScheduleTimer = nowMillis;
    runSchedule(now);
  }

  sensors.requestTemperatures();
  airTemp = sensors.getTempFByIndex(airProbe);
  waterTemp = sensors.getTempFByIndex(waterProbe);

  /*Blink builtin LED if water temp is above 68 deg F. Turn on water TEC and fan.
  if (!waterCoolingActive and waterTemp >= waterTempHigh)
  {
    switchWaterTEC(true);
    waterCoolingActive = true;
  }
  else if (waterCoolingActive and waterTemp <= waterTempTarget)
  {
    digitalWrite(LED_BUILTIN, LOW);
    switchWaterTEC(false);
    waterCoolingActive = false;
  }
  */

  //Check air temp and activate cooling, if necessary
  if (airCoolingActive and airTemp <= airTempTarget)
  {
    switchAirTEC(false);
    switchAirFans(false);
    airCoolingActive = false;
  }
  else if (!airCoolingActive and airTemp >= airTempHigh)
  {
    switchAirFans(true);
    switchAirTEC(true);
    airCoolingActive = true;
  }

  //Serial output, if available
  if (Serial)
    printTemps(airTemp, waterTemp, now);

  lcd.clear();
}

void writeStartDay()
{
  short sdNums[6] = {startDay.year(),startDay.month(),startDay.day(),startDay.hour(),startDay.minute(),startDay.second()};
  for (int i = 0; i < 6; ++i)
    EEPROM.put(i*sizeof(short), sdNums[i]);
}

void readStartDay()
{
  short sdNums[6];
  for (short i = 0; i < 6; ++i)
    EEPROM.get(i*sizeof(short), sdNums[i]);
  startDay = DateTime(sdNums[0],sdNums[1],sdNums[2],sdNums[3],sdNums[4],sdNums[5]);
}

String padString(String s, int strLen, bool back, String fillChar)
{
  String newStr = String(s);
  int len = s.length();
  if (len < strLen)
    for (int x = strLen-len; x > 0; x--) 
      newStr = (back ? newStr+fillChar : fillChar+newStr);
  return newStr;
}

//Turn tent air cooling fans on/off
void switchAirFans(bool on)
{
  if (!airFansActive and on)
  {
    analogWrite(smallAirFanSwitchPin, 255);
    analogWrite(largeAirFanSwitchPin, 255);
    airFansActive = true;
  }
  else if (airFansActive and !on)
  {
    analogWrite(smallAirFanSwitchPin, 0);
    analogWrite(largeAirFanSwitchPin, 0);
    airFansActive = false;
  }
}

//Turn tent air cooling TEC on/off
void switchAirTEC(bool on)
{
  if (!airTECActive and on)
  {
    analogWrite(airTECSwitchPin, 255);
    airTECActive = true;
  }
  else if (airTECActive and !on)
  {
    analogWrite(airTECSwitchPin, 0);
    airTECActive = false;
  }
}

/*Turn hydroponic nutrient water reservoir cooling TEC on/off.
void switchWaterTEC(bool on)
{
  if (!waterTECActive and on)
  {
    digitalWrite(waterTECSwitchPin, HIGH);
    waterTECActive = true;
  }
  else if (waterTECActive and !on)
  {
    digitalWrite(waterTECSwitchPin, LOW);
    waterTECActive = false;
  }
}
*/

//Print temperature data to serial output
void printTemps(int airTemp, int waterTemp, DateTime now)
{
  uint32_t nowUnix = now.unixtime();
  uint32_t startUnix = startDay.unixtime();
  double secDiff = (nowUnix > startUnix ? nowUnix - startUnix : 0);
  uint32_t week = ((secDiff / 604800L) < 13 ? secDiff / 604800L : 12);
  scheduleWeek = week;
  
  Serial.println("");
  Serial.println("--------------------------");
  Serial.print("Light: ");
  Serial.println(lightActive);
  Serial.print("UV: ");
  Serial.println(uvActive);
  Serial.print("Air Temp: ");
  Serial.print(airTemp);
  Serial.println(" degrees F");
  Serial.print("Water Temp: ");
  Serial.print(waterTemp);
  Serial.println(" degrees F");
  Serial.print("Air cooling on: ");
  Serial.println(airCoolingActive);
  Serial.print("Water cooling on: ");
  Serial.println(waterCoolingActive);

  Serial.print("Week ");
  Serial.println(week+1);
  Serial.print("High air temp: ");
  Serial.println(airTempHigh);
  Serial.print("Target air temp: ");
  Serial.println(airTempTarget);
  //Serial.print("High water temp: ");
  //Serial.println(waterTempHigh);
  //Serial.print("Target water temp: ");
  //Serial.println(waterTempTarget);
    
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

   /*
    Serial.print(" since midnight 1/1/1970 = ");
    Serial.print(now.unixtime());
    Serial.print("s = ");
    Serial.print(now.unixtime() / 86400L);
    Serial.println("d");

    // calculate a date which is 7 days, 12 hours, 30 minutes, 6 seconds into the future
    DateTime future (now + TimeSpan(7,12,30,6));

    Serial.print(" now + 7d + 12h + 30m + 6s: ");
    Serial.print(future.year(), DEC);
    Serial.print('/');
    Serial.print(future.month(), DEC);
    Serial.print('/'); 
    */
}

//Check time/date, set lighting schedule and temperature based on schedule
void runSchedule(DateTime now)
{ 
  for (int i = 0; i < 13; i++)
  {
    DateTime schedDay = startDay + TimeSpan(7*i,0,0,0);
    if (now.year() == schedDay.year() and now.month() == schedDay.month() and now.day() == schedDay.day() and now.hour() >= 7)
      isWaterChangeDay = true;
  }
  
  lcd.setCursor(0,1);
  lcd.print(padString("Exec Schedule"));
  uint32_t startUnix = startDay.unixtime();
  uint32_t endUnix = endDay.unixtime();
  uint32_t nowUnix = now.unixtime();
  
  double secDiff = (nowUnix > startUnix ? nowUnix - startUnix : 0);
  uint32_t week = (secDiff / 604800L < 13 ? secDiff / 604800L : 12);
  scheduleWeek = week;
  
  //Light hours from schedule
  DateTime tomorrow(now + TimeSpan(1, 0, 0, 0));
  uint32_t on1 = DateTime(now.year(), now.month(), now.day(), schedule[week][On1Hour], schedule[week][On1Min], 0).unixtime();
  uint32_t off1 = DateTime(now.year(), now.month(), now.day(), schedule[week][Off1Hour], schedule[week][Off1Min], 0).unixtime();
  uint32_t on2 = DateTime(now.year(), now.month(), now.day(), schedule[week][On2Hour], schedule[week][On2Min], 0).unixtime();
  uint32_t off2 = DateTime(now.year(), now.month(), now.day(), schedule[week][Off2Hour], schedule[week][Off2Min], 0).unixtime();
  uint32_t on3 = DateTime(tomorrow.year(), tomorrow.month(), tomorrow.day(), schedule[week][On1Hour], schedule[week][On1Min], 0).unixtime();
  uint32_t uvOn = DateTime(now.year(), now.month(), now.day(), schedule[week][UVOnHour], schedule[week][UVOnMin], 0).unixtime();
  uint32_t uvOff = DateTime(now.year(), now.month(), now.day(), schedule[week][UVOffHour], schedule[week][UVOffMin], 0).unixtime();

  //Temps from schedule
  if (nowUnix >= off1 and nowUnix < on3)
  {
    //-------Night-----------
    airTempHigh = schedule[week][NiteAirHi];
    airTempTarget = schedule[week][NiteAirTg];
    waterTempHigh = schedule[week][NiteH2OHi];
    waterTempTarget = schedule[week][NiteH2OTg];
    if (lightActive and (nowUnix < on2 or nowUnix >= off2))
      switchLight(false);
    else if (!lightActive and nowUnix >= on2 and nowUnix < off2)
      switchLight(true);
  } 
  else if (nowUnix >= on1 and nowUnix < off1)
  {
    //-------Day-------------
    if (nowUnix >= on1 and nowUnix < off1)
    {
      airTempHigh = schedule[week][DayAirHi];
      airTempTarget = schedule[week][DayAirTg];
      waterTempHigh = schedule[week][DayH2OHi];
      waterTempTarget = schedule[week][DayH2OTg];
      if (!lightActive)
        switchLight(true);
    }
    //--------UV-----------(only happens during day)
    if (nowUnix >= uvOn and nowUnix < uvOff)
      switchUV(true);
    else if (nowUnix < uvOn or nowUnix >= uvOff)
      switchUV(false);
  }
}

void switchUV(bool on)
{
  if (!uvActive and on)
  {
    digitalWrite(uvSwitchPin, HIGH);
    uvActive = true;
  }
  else if (uvActive and !on)
  {
    digitalWrite(uvSwitchPin, LOW);
    uvActive = false;
  }
}

void switchLight(bool on)
{
  if (!lightActive and on)
  {
    digitalWrite(lightSwitchPin, HIGH);
    lightActive = true;
  }
  else if (lightActive and !on)
  {
    digitalWrite(lightSwitchPin, LOW);
    lightActive = false;
  }
}
