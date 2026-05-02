  /****************************************************************
   * Neo-Smart Industrial Firmware v0.1.0
   * Target: ESP8266 (ESP-01 Compatible)
   * Features:
   *  - 2 Relay Control
   *  - TRIAC Fan Speed Control (Zero Cross + Timer)
   *  - Blynk IoT Integration
   *  - EEPROM Persistence
   *  - Non-blocking Architecture
   *  - DEBUG Serial Logs (Disable for ESP-01)
   *  - Blynk RTC Time Sync
   ****************************************************************/
  
  /************************************************************
   * Project       : <Home Automation Solutions>
   * File Name     : <HomifiBlynkIOT02Apr2026.ino>
   * Authorrrrr    : <Saad Ilyas>
   * Organization  : <Neo-Smart>
   *
   * Description   :
   *  <Brief technical description of what this file/module does.
   *   Mention key functionality, interfaces, or hardware used.>
   *
   * Hardware used :
   *   MCU used    : <ESP8266, ESP-01 CHIP with a custom made
   *                  circuit developed by Neo-Smart>
   *   Peripherals : <Double Relay Module with a TRIAC for fan>
   *
   * Version       : v0.1.0
   * Dateeee       : <02-Apr-2026>
   *
   * Revision History:
   *   ----------------------------------------------------------
   *   Version        Date           Author        Description
   *   ----------------------------------------------------------
   *   v0.1.0    <02-Apr-2026>    <Saad ilyas>    Initial release
   *   v0.2.0    <date>              <name>       <Changes made>
   *   v0.3.0    <date>              <name>       <Changes made>
   *   v0.4.0    <date>              <name>       <Changes made>
   *
   * Dependencies :
   *   - <EEPROM.h>
   *   - <TimeLib.h>
   *   - <WidgetRTC.h>
   *   - <BlynkEdgent.h>
   *
   * Important Notes :
   *   - <Important implementation notes>
   *   This firmware is specifically meant for 28BYJ MOTOR
   *   - <Limitations or assumptions>
   *   User must count limitations ULN2003 driver IC, which
   *   contains darlington pairs transistors
   *
   * *********************************************************/
  
  #define BLYNK_TEMPLATE_ID             "TMPL6iZ_PhY-Z"
  #define BLYNK_TEMPLATE_NAME      "IOT BLINKING LED LIGHTS"
  #define BLYNK_FIRMWARE_VERSION           "0.1.0"

  // #define BLYNK_PRINT Serial
  
  #include "BlynkEdgent.h"
  #include <WidgetRTC.h>
  #include "hw_timer.h"
  #include <TimeLib.h>
  #include <EEPROM.h>
  
  // #define APP_DEBUG
  // #define BLYNK_DEBUG
  // #define USE_NODE_MCU_BOARD
  
/////////// -------------------- Debug Control -------------------- ////////////
  
  #define DEBUG 0
  #if DEBUG
    #define DBG(x) Serial.print(x)
    #define DBGLN(x) Serial.println(x)
  #else
    #define DBG(x)
    #define DBGLN(x)
  #endif

///////////// -------------------- Debug Blynk -------------------- ////////////

  #define BLYNK_DEBUG 1

  #if BLYNK_DEBUG
    #define BLOG(x)    {terminal.print(x);}
    #define BLOGLN(x)  {terminal.println(x); terminal.flush();}
  #else
    #define BLOG(x)
    #define BLOGLN(x)
  #endif
  
////////// -------------------- Pins & addresses -------------------- //////////
  
  #define RELAY1_PIN    0   // GPIO0(D3)
  #define RELAY2_PIN    2   // GPIO2(D4)
  #define ZEROCR_PIN    1   // GPIO1(TX)
  #define TRIAC1_PIN    3   // GPIO3(RX)
  
  #define EEPROM_ADDR_FAN  901
  #define EEPROM_ADDR_RL1  902
  #define EEPROM_ADDR_RL2  903
  
////////// ------------------------ Blynk RTC ----------------------- //////////
  
  WidgetRTC rtc;
  WidgetTerminal terminal(V10);
  unsigned long lastRequest = 0;
  
////////// -------------------- Runtime Variables --------------------//////////

enum EffectMode 
 {MODE_OFFEEE,
  MODE_STATIC,
  // MODE_BLINKK,
  MODE_FBLINK,
  MODE_FADEEE,
  // MODE_BREATH,
  // MODE_DANCEE,
  // MODE_WAVEEE,
  // MODE_PULSEE,
  // MODE_FLICKER,
  MODE_RAMPUP,
  MODE_RAMPDN};

  volatile EffectMode currentMode = MODE_STATIC;

  const uint16_t ledDelay[30] = 
  {9000,   // OFF
   8700,
   8400,
   8100,
   7800,
   7500,
   7200,
   6900,
   6600,
   6300,
   6000,
   5700,
   5400,
   5100,
   4800,
   4500,
   4200,
   3900,
   3600,
   3300,
   3000,
   2700,
   2400,
   2100,
   1800,
   1500,
   1200,
   900,
   600,
   300};   // FULL
  
  String currentTime;
  String currentDate;

  int Slider_Value = 0;

  int brightness = 1;
  int directionn = 1;   // +1 = increasing, -1 = decreasing
   
  uint8_t Relay1State = LOW;
  uint8_t Relay2State = LOW;
  
  // static bool timerArmed = false;

  uint8_t effectSpeed = 50;                    // from Blynk (V8)
  
  uint8_t effectBrightness = 100;              // USED BY EFFECT ENGINE
  
  uint8_t targetBrightness = 255;              // from Blynk (V6)
  
  // uint8_t currentBrightness = 100;          // USED ONLY BY ISR

  // uint16_t interval;
  
  unsigned long lastRampTime = 0;

  uint32_t fadeStartTime = 0;
  bool fadeDirection = true;                   // true = rising, false = falling 

////////// ------------------ Volatile ISR Variables -----------------//////////

  volatile uint32_t zcCount = 0;

  // volatile uint32_t lastZC = 0;
  
  volatile uint32_t lastZCtime = 0;
  volatile uint32_t rejectCount = 0;

  volatile bool systemReady = false;

  volatile uint8_t currentBrightness = 0;    // USED ONLY BY ISR
   
  volatile uint32_t halfCycleTime = 10000;   // default 10ms 
  
////////////////////////////////////////////////////////////////////////////////
///////// -------------------- Function Prototypes --------------------/////////
////////////////////////////////////////////////////////////////////////////////
  
  void loadEEPROM();
  void saveEEPROM();
  
  void pushTimeBlynk();

  void updateEffects();
  void syncBrightness();
  
  // void updateBrightness();

  void ICACHE_RAM_ATTR dimTimerISR();
  void ICACHE_RAM_ATTR zeroCrossISR();
  
////////////////////////////////////////////////////////////////////////////////
/////////// -------------------- Blynk Functions --------------------///////////
////////////////////////////////////////////////////////////////////////////////
  
  BLYNK_CONNECTED()
   {// Blynk.syncAll();}
    
    rtc.begin();
    
    BLOGLN("[BLYNK] Connected");

    BLOGLN("[WIFI] Mode=" + String(WiFi.getMode()));
    BLOGLN("[WIFI] SSID=" + WiFi.SSID());
  
    Blynk.sendInternal("rtc", "sync");
    
    // Blynk.syncVirtual(V0);   // Date
    // Blynk.syncVirtual(V1);   // Time
    // Blynk.syncVirtual(V2);   // Temper
    // Blynk.syncVirtual(V3);   // Humidi 
    
    Blynk.syncVirtual(V4);      // Relay1
    Blynk.syncVirtual(V5);      // Relay2
    Blynk.syncVirtual(V6);      // Fan Rl

    systemReady = true;}        // ENABLE AFTER SYNC
  
BLYNK_WRITE(V4)
 {Relay1State = param.asInt();
  digitalWrite(RELAY1_PIN, Relay1State);
  BLOGLN("[BLYNK] Relay1: " + String(Relay1State));
  saveEEPROM();}
  
BLYNK_WRITE(V5)
 {Relay2State = param.asInt();
  digitalWrite(RELAY2_PIN, Relay2State);
  BLOGLN("[BLYNK] Relay2: " + String(Relay2State));
  saveEEPROM();}
  
BLYNK_WRITE(V6)
 /*{targetBrightness = param.asInt();
 BLOGLN("[BLYNK] Fan Slider: " + String(targetBrightness));
 saveEEPROM();}*/
 {targetBrightness = param.asInt();
  BLOGLN("[BLYNK] Brightness Slider: " + String(targetBrightness));
  saveEEPROM();}

 /*{Slider_Value = param.asInt(); 
  if (Slider_Value>0)
     {targetBrightness = Slider_Value;
      BLOGLN("[BLYNK] Fan Slider: " + String(targetBrightness));}}*/

BLYNK_WRITE(V7)
 {int mode = param.asInt();

  if (mode == 1)     // ONLY on press (ignore release)
     {currentMode = (EffectMode)((currentMode + 1) % 6);

      BLOGLN("[EFFECT] Mode Changed To: " + String(currentMode));}}

/*BLYNK_WRITE(V7)
  {int mode = param.asInt();   // Dropdown selected value

  currentMode = (EffectMode)mode;   // Direct assignment

  BLOGLN("[EFFECT] Mode Set To: " + String(currentMode));}*/

  /*switch(mode)
   
   {case 0: currentMode = MODE_OFF; break;
    case 1: currentMode = MODE_STATIC; break;
    case 2: currentMode = MODE_BLINK; break;
    case 3: currentMode = MODE_FADE; break;
    case 4: currentMode = MODE_BREATH; break;
    case 5: currentMode = MODE_DANCE; break;}
    
    BLOGLN("[EFFECT] Mode Changed: " + String(mode));}*/

BLYNK_WRITE(V8)
 {effectSpeed = param.asInt(); // 1–100
  BLOGLN("[EFFECT] Speed: " + String(effectSpeed));}

////////////////////////////////////////////////////////////////////////////////
/////////// ------------------ Push Time Function ------------------////////////
////////////////////////////////////////////////////////////////////////////////
 
void pushTimeBlynk()
 {currentTime = String(hour()) + ":" + minute() + ":" + second();
  currentDate = String(day()) + " " + month() + " " + year();
    
  Blynk.virtualWrite(V0, currentDate);
  Blynk.virtualWrite(V1, currentTime);}
  
////////////////////////////////////////////////////////////////////////////////
/////////// -------------------- Setup Function --------------------////////////
////////////////////////////////////////////////////////////////////////////////
  
void setup()
 {EEPROM.begin(1024);
  // Serial.begin(115200);
  
  BLOGLN("\n[BOOT] System Starting...");
  
  // pinMode(ZEROCR_PIN, INPUT_PULLUP);
  
  pinMode(ZEROCR_PIN, INPUT);
    
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
    
  pinMode(TRIAC1_PIN, OUTPUT);
  
  // digitalWrite(TRIAC1_PIN, 0);

  systemReady = false;
    
  loadEEPROM();
 
  digitalWrite(RELAY1_PIN, Relay1State);
  digitalWrite(RELAY2_PIN, Relay2State);
  
  currentBrightness = currentBrightness;

  delay(1000);

  BlynkEdgent.begin();

  delay(1000);

  // attachInterrupt(digitalPinToInterrupt(ZEROCR_PIN), zeroCrossISR, RISING);
  attachInterrupt(ZEROCR_PIN, zeroCrossISR, RISING);
  // attachInterrupt(ZEROCR_PIN, zeroCrossISR, FALLING);
  
  hw_timer_init(NMI_SOURCE, 0);
  hw_timer_set_func(dimTimerISR);

  // WiFi.setAutoReconnect(true);
  // WiFi.persistent(true);
  // WiFi.mode(WIFI_STA);
  
  // BlynkEdgent.begin();

  BLOGLN("Reset reason: " + String(ESP.getResetReason()));
  
  BLOGLN("[WIFI] Mode=" + String(WiFi.getMode()));
  BLOGLN("[WIFI] SSID=" + WiFi.SSID());
  BLOGLN("[WIFI] PSSK=" + WiFi.psk());
    
  BLOGLN("Relay1=" + String(Relay1State));
  BLOGLN("Relay2=" + String(Relay2State));
    
  BLOGLN("Dimmer=" + String(targetBrightness));}
  
////////////////////////////////////////////////////////////////////////////////
//////////// -------------------- Loop Function --------------------////////////
////////////////////////////////////////////////////////////////////////////////
  
void loop()
 {BlynkEdgent.run();

 ////// ------ Run multiple AC cycles at current brightness ------ //////
 
 updateEffects();    // generate pattern
 syncBrightness();   // send to ISR safely
 
 // yield();
 
 static uint32_t lastZC = 0;
 static uint32_t lastPrint = 0;

 ///////// ---------- Print zcCount every 1 second ---------- /////////
  
 // -- if (millis() % 2000 < 50) // modulo use a bit sloppy method -- //
 
 if (millis() - lastPrint > 1000)
    {lastPrint = millis();
     pushTimeBlynk();         // included pushTime function in this also
     
     // BLOGLN("ZC Count: " + String(zcCount));
     // BLOGLN("ZC: " + String(zcCount) + " Reject: " + String(rejectCount));

     BLOGLN("Mode=" + String(currentMode));
     BLOGLN("Brightness=" + String(currentBrightness));
     BLOGLN("ZC Count / Sec: " + String(zcCount - lastZC));
     BLOGLN("HalfCycleTime=" + String(halfCycleTime));
     
     lastZC = zcCount;}

 static uint32_t lastReconnectAttempt = 0;
     
 /*if (!Blynk.connected())
    {if (millis() - lastReconnectAttempt > 10000)
        {lastReconnectAttempt = millis();

        // Step 1: ensure WiFi is alive
        if (WiFi.status() != WL_CONNECTED)
           {WiFi.disconnect();
            WiFi.begin();}

        // Step 2: reconnect Blynk
        Blynk.connect(3000);}}*/
        }   // timeout 1 sec
  
////////////////////////////////////////////////////////////////////////////////
//////////// -------------------- ISR Functions --------------------////////////
////////////////////////////////////////////////////////////////////////////////
  
void ICACHE_RAM_ATTR zeroCrossISR()

 {zcCount++;
 
  uint32_t now = micros();
  // uint32_t dt = now - lastZCtime;
  // dt = now - lastZCtime;
  halfCycleTime = now - lastZCtime;
  lastZCtime = now;

  // static uint8_t stableCounter = 0;
  // static uint32_t stableDelay = 5000;
  
  
  //// ---- validate AC half cycle (50Hz safe window) ---- ////
  
  if (halfCycleTime < 8000)   // reject only very fast noise
     {// rejectCount++;
      return;}

  /*if (halfCycleTime > 20000)  // reject only very fast noise
       {hw_timer_arm(1000);}    // force minimal firing to keep TRIAC alive*/
       
  /*if (dt > 8000 || dt < 12000)
     
       {stableCounter++;

        if (stableCounter > 50)   // update every ~0.5 sec
           {halfCycleTime = dt;
            stableCounter = 0;}}*/
  
  /////// ---------- simple low-pass filter ---------- ///////

  // halfCycleTime = (halfCycleTime * 7 + dt) / 8;
  // halfCycleTime = (halfCycleTime * 15 + dt) / 16;

  /// ------ Adaptive delay based on real AC timing ------ ///

  // uint8_t quantized = (currentBrightness / 8) * 8;  // 32 levels
  
  // uint32_t delayMicros = 30 * (255 - currentBrightness) + 400; // old formula
  
  // uint32_t delayMicros = (halfCycleTime * (255 - currentBrightness)) / 255;

  // uint32_t delayMicros = (halfCycleTime * (255 - quantized)) / 255;

  int index = map(currentBrightness, 0, 255, 0, 29);
  uint16_t delayMicros = ledDelay[index];

  //// --------- Non-linear curve (gamma ≈ 2.2) --------- ////
  
  // float normalized = currentBrightness / 255.0;   // gamma ≈ 2.0
  // float gammaCorrected = normalized * normalized;   // simple gamma
  // return gamma * 255;

  // uint16_t delayMicros = (1.0 - gammaCorrected) * halfCycleTime;

  //// ------ only update if change is significant ------ ////

  // if (abs((int)delayMicros - (int)stableDelay) > 50)
     // {stableDelay = delayMicros;}

  ////////// ------------- smooth it -------------- //////////

  // stableDelay = (stableDelay * 7 + delayMicros) / 8;
  // stableDelay = (stableDelay * 15 + delayMicros) / 16;

  //////////// ---------- safety clamp ---------- ////////////
  
  if (delayMicros < 300) delayMicros = 300;
  if (delayMicros > (halfCycleTime - 900))   // (delayMicros > (halfCycleTime - 300))
      delayMicros = halfCycleTime - 900;     // delayMicros = halfCycleTime - 300;

  ////////// ---------- ARM THE TIMER NOW ---------- /////////
   
  // if (!timerArmed)
     // {timerArmed = true;
      // hw_timer_arm(delayMicros + zcOffset);
      hw_timer_arm(delayMicros);
      // hw_timer_arm(stableDelay);
   
  }

void ICACHE_RAM_ATTR dimTimerISR()
 
 {// uint8_t b = currentBrightness;
  if (!systemReady)
     {currentBrightness = 100;}
        
  if (currentBrightness <= 10) // || (!systemReady)) // FULL OFF → do nothing
     {digitalWrite(TRIAC1_PIN, 0);}
      // return;}

  else if (currentBrightness >= 245)           // FULL ONN → immediate firing

          {digitalWrite(TRIAC1_PIN, 1);}
           // return;}
           /*GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
           os_delay_us(150); // 100 for bulb 150 for fan
           GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));
           return;}*/
  else
          {GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
           os_delay_us(100); // 100 for bulb 200 for fan
           GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));}}

  ////////// ---------- second reinforcement pulse ---------- //////////
  
  /*os_delay_us(200);
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1 << TRIAC1_PIN));
  os_delay_us(100);
  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << TRIAC1_PIN));}*/

////////////////////////////////////////////////////////////////////////////////
//////////// ------------- Led Effects Control Function -------------///////////
////////////////////////////////////////////////////////////////////////////////

/*void updateEffects()
   {
         case MODE_DANCEE:

         {static int target = 200;

         if (abs(effectBrightness - target) < 5)
             target = random(20, targetBrightness);
             
             effectBrightness += (target - effectBrightness) * 0.1;
         
             // if (effectBrightness < target) effectBrightness += 10;
             // else if (effectBrightness > target) effectBrightness -= 10;
         
             break;}

         
         
          case MODE_WAVEEE:
            {static float x = 0;
             x += 0.15 * (effectSpeed / 50.0);

             float y = (sin(x) + 1.0) * 0.5;
             effectBrightness = y * targetBrightness;
             break;}

          

          case MODE_PULSEE:
            {static bool state = false;
             state = !state;
             if (pulseActive)
                 effectBrightness = targettBrightness;
             else
                 effectBrightness *= 0.7;
             effectBrightness = state ? targetBrightness : targetBrightness / 5;
             break;}

          

          case MODE_FLICKER:
            {int noise = random(-20, 20);
             int val = targetBrightness + noise;
             effectBrightness += (target - effectBrightness) * 0.2;
             // effectBrightness = constrain(val, 10, targetBrightness);
             break;}*/
    
////////////////////////////////////////////////////////////////////////////////
//////////// ------------- Led Effects Control Function -------------///////////
////////////////////////////////////////////////////////////////////////////////

void updateEffects()
 
 {static uint32_t lastUpdate = 0;

  static bool blinkState = false;
  static int fadeValue = 0;
  static int direction = 1;

  // static float phase = 0;
  // static float wavePhase = 0;

  static int rampValue = 0;
  // static int flickerTarget = 0;

  // uint8_t base = targetBrightness;

  uint16_t interval = map(effectSpeed, 1, 100, 200, 10);
  
  if (millis() - lastUpdate < interval) return;  // 10 FPS
      lastUpdate = millis();

  switch(currentMode)
    {
     //----------------------------------//
     
     case MODE_OFFEEE:
       {effectBrightness = 0;
        break;}

     //----------------------------------//
     
     case MODE_STATIC:
       {effectBrightness = targetBrightness;
        // keep currentBrightness from slider
        break;}

     //------------CLEAN BLINK-----------//

     /*case MODE_BLINKK:
       {blinkState = !blinkState;
        effectBrightness = blinkState ? targetBrightness : 0;
        break;}*/
        
     //------------FAST BLINK------------//
     
     case MODE_FBLINK:
     
       {static uint32_t lastToggle = 0;
        static bool state = false;

        // Map speed → frequency (1 Hz to 10 Hz)
        int freq = map(effectSpeed, 1, 100, 1, 10);

        // Convert frequency → toggle interval
        uint32_t interval = 1000 / (2 * freq);

        // uint32_t interval = 125; // 125ms → 4 Hz (ON+OFF = 250ms)
        
        /*uint16_t period = 1000 / freq; // total cycle time (ms)
          uint16_t onTime = period / 2;

        if (millis() % period < onTime)
            effectBrightness = targetBrightness;
        else
            effectBrightness = 0;*/

        /*if (millis() % 250 < 125)
              effectBrightness = targetBrightness;
          else
              effectBrightness = 0;*/

        if (millis() - lastToggle >= interval)
           {lastToggle = millis();
            // lastToggle += interval;
            state = !state;}

        effectBrightness = state ? targetBrightness : 0;

        break;}

     //------------FADE STEP BASED-------------//

       case MODE_FADEEE:
         {int riseStep = map(effectSpeed, 1, 100, 1, 50);
          int fallStep = map(effectSpeed, 1, 100, 1, 50);

          int step = (direction > 0) ? riseStep : fallStep;

          // if (direction > 0) step = 30;
             //  else step = 60;

          fadeValue += direction * step;

          if (fadeValue >= targetBrightness)
              {fadeValue = targetBrightness;
               direction *= -1;}

          else if (fadeValue <= 0)
                  {fadeValue = 0;
                   direction = 1;}
           
          effectBrightness = fadeValue;
          
          break;}

     //------------FADE TIME BASED-------------//
     
       /*case MODE_FADEEE:
      
         {uint32_t now = millis();

          // Total time for one half-cycle (rise or fall)
          uint32_t duration = map(effectSpeed, 1, 100, 3000, 300);  
          // slow → 3s, fast → 0.3s

          uint32_t elapsed = now - fadeStartTime;

           // If phase complete → flip direction
           
           if (elapsed >= duration)
              {fadeStartTime = now;
               fadeDirection = !fadeDirection;
               elapsed = 0;}

          // Linear interpolation
          uint16_t value = (elapsed * targetBrightness) / duration;

           if (fadeDirection)
               effectBrightness = value;                      // rising
           else
               effectBrightness = targetBrightness - value;   // falling

           break;}*/   

    //---------------FINE BREATH---------------//
    
    /*case MODE_BREATH:
      {phase += 0.05 + (effectSpeed * 0.002);

       float x = millis() / 500.0;
       static float x = 0;
       x += 0.1;

       // effectBrightness = (exp(sin(x)) - 0.367) * 108.0;

       float y = (sin(phase) + 1.0) * 0.5;

       float gamma = y * y;   // gamma + smoothing

       // slight randomness for "alive" feel
       float noise = random(-5, 5) / 255.0;

       effectBrightness = (gamma + noise) * targetBrightness;

       break;}*/
   
    //--------------SMOOTH WAVE---------------//

    /*case MODE_WAVEEE:
      {wavePhase += 0.03 + (effectSpeed * 0.001);

       float y1 = (sin(wavePhase) + 1.0) * 0.5;
       float y2 = (sin(wavePhase * 0.5) + 1.0) * 0.5;

       // layered wave = premium look
       float mix = (y1 * 0.7 + y2 * 0.3);

       effectBrightness = mix * base;

       break;}*/

    //-------------SPARKY FLICKER-------------//

    /*case MODE_FLICKER:
      {if (abs(effectBrightness - flickerTarget) < 5)
          {if (random(0, 10) > 7)   // rare spikes + normal flicker
               flickerTarget = random(base * 0.6, base);  // spark
           else
               flickerTarget = random(base * 0.3, base * 0.8);}

      // smooth toward target
      effectBrightness += (flickerTarget - effectBrightness) * 0.25;

      break;}*/

    //-----------------RAMP-ONN-----------------//

    case MODE_RAMPUP:
      {int riseStep = map(effectSpeed, 1, 100, 1, 20);
       int fallStep = map(effectSpeed, 1, 100, 1, 99);

       int step = (direction > 0) ? riseStep : fallStep;

       rampValue += direction * step;
        
       // rampVal += map(effectSpeed, 1, 100, 1, 50);

       /*if (rampValue >= targetBrightness)
            {rampValue = targetBrightness;
             direction *= -1;}*/

       if (rampValue >= targetBrightness)
          {rampValue = 0;
           direction *= -1;}
           
       else if (rampValue <= 0)
               {rampValue = 0;
                direction = 1;}

       effectBrightness = rampValue;

       break;}
       
    //---------------RAMP-OFF----------------//

    case MODE_RAMPDN:
      {int riseStep = map(effectSpeed, 1, 100, 1, 99);
       int fallStep = map(effectSpeed, 1, 100, 1, 20);

       int step = (direction > 0) ? riseStep : fallStep;

       rampValue += direction * step;
        
       // rampVal += map(effectSpeed, 1, 100, 1, 50);

       /*if (rampValue >= targetBrightness)
            {rampValue = targetBrightness;
             direction *= -1;}*/

       if (rampValue >= targetBrightness)
          {rampValue = targetBrightness;
           direction *= -1;}
           
       else if (rampValue <= 0)
               {rampValue = targetBrightness;
                direction = 1;}

       effectBrightness = rampValue;

       break;}}

  //------FINAL FILTER (VERY IMPORTANT)------//
  
  /*static float smooth = 0;
  smooth += (effectBrightness - smooth) * 0.2;

  effectBrightness = smooth;*/
  
  }

  /*BLOGLN("[EFFECT] Mode=" + String(currentMode) +
          " Out=" + String(effectBrightness));}*/

////////////////////////////////////////////////////////////////////////////////
//////////// ------------- Brightness Control Function -------------////////////
////////////////////////////////////////////////////////////////////////////////

void syncBrightness()
 {static uint32_t last = 0;

  if (millis() - last < 10) return;
      last = millis();

  // noInterrupts();
  currentBrightness = effectBrightness;}
  // interrupts();}
  
  /*if (currentBrightness < targetBrightness)
        currentBrightness++;
    else if (currentBrightness > targetBrightness)
             currentBrightness--;}*/

////////////////////////////////////////////////////////////////////////////////
///////////// -------------------- Load EEPROM --------------------/////////////
////////////////////////////////////////////////////////////////////////////////
  
void loadEEPROM()
  
 {Relay1State = EEPROM.read(EEPROM_ADDR_RL1);
  Relay2State = EEPROM.read(EEPROM_ADDR_RL2);

  EEPROM.get(EEPROM_ADDR_FAN, currentBrightness);}
  
////////////////////////////////////////////////////////////////////////////////
///////////// -------------------- Save EEPROM --------------------/////////////
////////////////////////////////////////////////////////////////////////////////
  
void saveEEPROM()
  
 {EEPROM.write(EEPROM_ADDR_RL1, Relay1State);
  EEPROM.write(EEPROM_ADDR_RL2, Relay2State);
    
  EEPROM.put(EEPROM_ADDR_FAN, currentBrightness);
    
  EEPROM.commit();
    
  BLOGLN("[EEPROM] Saved State");}
