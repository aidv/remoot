/*********************************************************************
Based on the Adafruit SSD1306 example.

Written/modified by Aid Vllasaliu.
GPLv3
All text above, and the splash screen must be included in any redistribution
*********************************************************************/


#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>


#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

/* Non-Halting Microsecond Timer. START. STABLE! */

#define forever 0

typedef void (*onNHTimerExecuteCB)(bool);

class NHTimer {
    onNHTimerExecuteCB onExecute;
    unsigned long last       =   0; //value less than 1 (one) means disabled

  public:
    float         interval   = 1.0; //pause time
    unsigned long iterations =   0; //zero iterations means run forever. above zero specifies how many times the callback should be called
    unsigned long iteration  =   0; //how many times the callback has been called

    NHTimer(onNHTimerExecuteCB onExecuteFunc, float _interval, unsigned long _iterations){
      interval   = (_interval > 0.0) ? _interval : 1.0; //sets default interfal to 1 second if it's less than zero milliseconds
      onExecute  = onExecuteFunc ;
      iterations = _iterations   ;
     }

    void Reset(){
      last = (micros() > 0) ? micros(): 1;
      iteration = 0;
    }

    void Start(){
      iteration = 0;
      Reset();
     }

    void Stop(){ last = 0; }

    void Update(){
      if(last > 0 && micros() - last >= long(interval*1000000.0) ){
        ++iteration;

        bool isLastIteration = false;

        if (iterations > 0){
          if (iteration == iterations){
            isLastIteration = true;
            Stop();
          }
        }

        if (isLastIteration == false) last = (micros() > 0) ? micros(): 1;


        onExecute(isLastIteration);
      }
    }
};

/* Non-Halting Microsecond Timer Code. END */

/* Interface Management. START. */

typedef void(*onValChanged)(int, int, bool);

#define iftDigital 0
#define iftAnalog  1

struct interface{
  onValChanged onChanged;
  byte pin = 0;
  byte type = iftDigital;
  bool pullup = false;
  int lastVal = 0;
};

interface ifaces[4];

class InterfaceMonitor {
  public:
    InterfaceMonitor(){
      for (int i=0; i<4; i++){
        pinMode(ifaces[i].pin, INPUT);
        if (ifaces[i].pullup == true) digitalWrite(ifaces[i].pin, HIGH);
      }
    }

    void Update(){
      for (int i=0; i<4; i++){
        int raw = 0;

        if (ifaces[i].type == iftDigital){
          raw = digitalRead(ifaces[i].pin);
          notify(i, raw, raw, bool(raw));
        }
        else if (ifaces[i].type == iftAnalog){
          raw = analogRead(ifaces[i].pin);

          int result = map(long(raw), 0, 1024, -10, 10);

          //if (result < round(float(mapLow)/2.0) || result > round(float(mapHigh)/2.0)){
            notify(i, raw, result, 0);
          /*} else {
            notify(i, 0, 0, 0);
          }*/
        }
      }

    }

    void notify(int iface, int raw, int val, bool high){
      if (val != ifaces[iface].lastVal){
        ifaces[iface].onChanged(raw, val, bool(raw));
        ifaces[iface].lastVal = val;
      }
    }

};

/* Interface Manager. END */





int pwr_btn_P = 2;
int pwr_P     = 3;
int btn_P     = 4;
int photo_P   = 5;
int analogH_P = A7;
int analogV_P = A6;


NHTimer *gTimer = new NHTimer(&gTimerCB, 0.001, forever);

bool shutdownRun, selRun, stepRun, statusRun, shootRun = false;
long shutdownTicks, selTicks, stepTicks, statusTicks, shootTicks, rateTicks, shutterTicks = 0;
int shutdownTime = 10000;
int selTime      = 600;
int stepTime     = 400;
int statusTime   = 400;
int shootTime    = 1000;
int rateTime     = 1000;
int shutterTime  = 1000;


bool wantShutdown = false;
void powerState(bool ON){
  if (ON == true){
    pinMode(pwr_P, OUTPUT);
    digitalWrite(pwr_P, HIGH);

    wantShutdown = false;
  } else {
    gTimer -> Stop();

    photoStop();

    display.clearDisplay();
    display.display();

    wantShutdown = true;

    Serial.println(F("Bye!"));
    delay(100);

    digitalWrite(pwr_P, LOW);
  }
}




struct visualsChanged {
  bool selector = true;
  bool rate     = true;
  bool shut     = true;
  bool status   = true;
  bool timerBar = true;

  void reset(){
    selector = false;
    rate     = false;
    shut     = false;
    status   = false;
    timerBar = false;
  }
};

visualsChanged visChanged;


InterfaceMonitor *ifaceMon;




byte objIndex = 1;

byte sliderValue[2][3] = {{1,10,1},{1,60,1}};
byte sliderValueToChange = 1;
bool changingSliderValue = false;

bool selectorHidden = false;
void selectorVisible(bool visible){
  selectorHidden = !visible;
  visChanged.selector = true;
}
void selectorReset(){
  selectorVisible(true);
  selTicks = 0;
}


String statusTxt = "PAUSED";
bool statusHidden = false;
void setStatusText(String txt){
  statusTxt = txt;
  visChanged.status = true;
}
void statusVisible(bool visible){
  statusHidden = !visible;
  visChanged.status = true;
}


void photoStart() {
  shutdownRun = false;

  statusTxt = "SHOOTING";
  shootTicks = 0;

  rateTime    = sliderValue[0][2]*1000;
  shutterTime = sliderValue[1][2]*1000;
  rateTicks = 0;
  shutterTicks = 0;

  shootRun = true;
  digitalWrite(photo_P, HIGH);
}
void photoStop() {
  digitalWrite(photo_P, LOW);
}




void setup()   {
  //latch power
  powerState(true);

  //initial serial communication
  Serial.begin(9600);
  Serial.println(F("Hello!"));



  //initialize display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  //initialize interfaces
  ifaces[0].onChanged = &pwrCB;
  ifaces[0].pin = 2;
  ifaces[0].type = iftDigital;
  ifaces[0].pullup = true;


  ifaces[1].onChanged = &selCB;
  ifaces[1].pin = 4;
  ifaces[1].type = iftDigital;
  ifaces[1].pullup = true;

  ifaces[2].onChanged = &horzCB;
  ifaces[2].pin = A7;
  ifaces[2].type = iftAnalog;
  ifaces[2].pullup = false;

  ifaces[3].onChanged = &vertCB;
  ifaces[3].pin = A6;
  ifaces[3].type = iftAnalog;
  ifaces[3].pullup = false;

  ifaceMon = new InterfaceMonitor();


  //initialize photo trigger
  pinMode(photo_P, OUTPUT); //photo trigger
  digitalWrite (photo_P, LOW);



  //start some timers
  gTimer -> Start();
  selRun = true;

}

void loop() {
  //update timers and interface monitors
  gTimer    -> Update();
  ifaceMon -> Update();
}



/***** Interface Monitors Callbacks ******/

void pwrCB(int raw, int value, bool high){
  shutdownRun = false;

  shutdownTicks = 0;

  if (high == true){
    shutdownTime = 10000;
    if (statusTxt == "PAUSED") shutdownRun = true;
  } else {
    shutdownTime = 1500;
    shutdownRun = true;
  }
}

void selCB(int raw, int value, bool high){
  shutdownTicks = 0;

  if (high == false){
    if (objIndex > 0 && objIndex < 3){
      changingSliderValue = !changingSliderValue;
    }
    else {
      if (statusTxt == "SHOOTING"){
        statusTxt = "PAUSED";
        statusVisible(true);
        photoStop();
        shootRun = false;
      }
      else {
        photoStart();
      }
    }
  } else {
    shutdownTicks = 0;
    if (statusTxt == "PAUSED") shutdownRun = true;
  }

  if (changingSliderValue == true){
    selTime = 100;
    selectorReset();
  } else {
    selTime = 600;
    selectorReset();
  }

}

int lastHorztVal = 0;
int valueStepping = 0;
void horzCB(int raw, int value, bool high){
  shutdownTicks = 0;

  int val = map(value+2,-10,7,-1,1);

  if (val != lastHorztVal){
    lastHorztVal = val;
    if (val != 0){
      if (changingSliderValue == true){
        valueStepping = val;
        stepTicks = 250;
        stepTime  = 250;
        stepRun   = true;
      }
    } else {
      stepRun = false;
    }
  }
}


int lastVertVal = 0;
void vertCB(int raw, int value, bool high){
  shutdownTicks = 0;

  int val = map(value+2,-10,7,-1,1);

  if (val != lastVertVal && changingSliderValue == false && statusTxt == "PAUSED"){
    lastVertVal = val;
    if (val != 0){
      objIndex += val;

      selectorReset();

      if (objIndex < 1) objIndex = 1;
      if (objIndex > 3) objIndex = 3;

      visChanged.selector = true;
    }
  }
}

/* graphics callbacks */

int half(int val){ return round(float(val) / 2.0); }



int selectorLast_Y = 8;
void drawSelector(bool clear){
  int size = 4;

  int x = 122;
  int y = 16*objIndex - 8;
  if (objIndex == 3) y = 48;


  display.fillRect(x, selectorLast_Y - size, size, size*2, BLACK); //clear current selector area

  if (clear == false){
    for (int i = 0; i < size; i++) display.drawLine(x+i, y-i, x+i, y+i, WHITE); //draw new selector
    selectorLast_Y = y;
  }
}


int padding = 4;
int textWidth(String text){ return 6*text.length(); }
int textHeight(String text){ return 8; }

void drawSlider(byte top, String name, String unit, int min, int max, int value){

  int w   = 120;
  int h  = 16;
  int half_h = half(h);
  int half_txt = half(textHeight("a"));
  int txt_y =  top + half_h - half_txt;


  display.fillRect(0, top, w, h, BLACK); //clear current slider area

  display.setTextSize(0);
  display.setTextColor(WHITE);
  display.setCursor(0, txt_y);
  display.println(name);



  int tmp = w - textWidth(unit);
  display.setCursor(w - textWidth(unit), txt_y);
  display.println(unit);

  tmp = tmp - textWidth(String(value));
  display.setCursor(tmp, txt_y);
  display.println(String(value));

  int sliderPos    = textWidth(name) + padding - 1;
  int sliderLength = tmp - sliderPos - padding - 2;

  int sliderValPos = map(value, long(min), long(max), long(sliderPos), long(tmp - padding - 2)); //sliderPos + round((float(sliderLength) / float(aMax - aMin)) * aValue) - round((float(sliderLength) / float(aMax - aMin)));



  display.drawLine(sliderPos, top + half_h, sliderPos + sliderLength, top + half_h, WHITE);

  display.drawLine(sliderValPos - 1, top + 1, sliderValPos - 1,  top + h - 1, WHITE);
  display.drawLine(sliderValPos, top + 2, sliderValPos, top+h - 2, WHITE);
  display.drawLine(sliderValPos + 1, top + 1, sliderValPos + 1,  top + h  - 1, WHITE);
}


void drawStatus(bool clear){
  int x    = 0;
  int y    = 38;
  int size = 7;

  display.fillRect(0, y, 120, 32, BLACK); //clear current slider area

  if (clear == true || statusTxt == "") return;


  if (statusTxt == "PAUSED"){
    display.fillRect(x, y+3, 4, 16, WHITE);
    display.fillRect(x+8, y+3, 4, 16, WHITE);
  } else {
    for (int i = 0; i < size; i++) display.drawLine(x+i, y+i+3, x+i, y+size*2-i+3, WHITE); //draw new selector
  }

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(x + size + 16, y + 4);
  display.println(statusTxt);
}




void drawTimerBar(){
  byte y = 62;

  display.fillRect(0, y-1, 128, 3, BLACK);

  display.drawLine(0, y, 128, y, WHITE);
  if (statusTxt == "PAUSED"){
    bool black = false;
    for (byte i = 0; i < 128; i++){
      black = !black;
      if (black == true) display.drawLine(i, y, i, y, BLACK);
    }

    int v = map(millis() - shutdownTicks, 0, shutdownTime, 0, 128);
    display.fillRect(128 - v, y-1, v, 3, WHITE);
  } else {
    byte black = 0;
    for (byte i = 64; i < 128; i++){
      black++;
      if (black < 3){
        display.drawLine(i, y, i, y, BLACK);
      } else if (black > 4){
        black = 0;
      }
    }

    if (rateTicks == 0){
      int s = map(millis() - shutterTicks, 0, shutterTime, 0, 63);
      display.fillRect(0, y-1, s, 3, WHITE);
    } else {

      int r = map(millis() - rateTicks, 0, rateTime, 0, 63);
      display.drawRect(0, y-1, 63 + r, 3, WHITE);
    }
  }
  visChanged.timerBar = true;
}




byte stepIterations = 0;
void gTimerCB(bool isLastIteration){

  //==============
  if (shutdownRun == true){
    if (shutdownTicks == 0) shutdownTicks = millis();

    drawTimerBar();

    if (millis() - shutdownTicks >= shutdownTime){
      powerState(false);
    }
  } else { shutdownTicks = 0; }



  //==============
  if (selRun == true){
    if (selTicks == 0) selTicks = millis();
    if (millis() - selTicks >= selTime){
      selectorVisible(selectorHidden);
      if (statusTxt == "SHOOTING"){ statusVisible(statusHidden); }
      selTicks = 0;
    }
  } else { selTicks = 0; }


  //==============
  if (stepRun == true){
    if (stepTicks == 0) stepTicks = millis();

    if (millis() - stepTicks >= stepTime){
      if (stepIterations == 4){
          stepTime = half(stepTime);
          //if (stepIterations < 16) stepIterations = stepIterations*2;
          stepIterations = 0;
      }

      stepIterations++;

      if (objIndex == 1){
        visChanged.rate = true;
        if (sliderValue[0][2] + valueStepping >= sliderValue[0][0] && sliderValue[0][2] + valueStepping <= sliderValue[0][1]) sliderValue[0][2] += valueStepping;
      }
      else if (objIndex == 2){
        visChanged.shut = true;
        if (sliderValue[1][2] + valueStepping >= sliderValue[1][0] && sliderValue[1][2] + valueStepping <= sliderValue[1][1]) sliderValue[1][2] += valueStepping;
      }

      stepTicks = 0;
    }
  } else { stepTicks = 0; }

  //==============
  if (statusRun == true){
    statusTicks++;

  }

  //==============
  if (shootRun == true){
    if (shutterTicks == 0) shutterTicks = millis();
    if (millis() - shutterTicks >= shutterTime){
      photoStop();
      if (rateTicks == 0) rateTicks = millis();
      if (millis() - rateTicks >= rateTime){
        //flash the screen white once to indicate that photo has been taken!
        /*display.fillRect(0, 0, 128, 64, WHITE);
        display.display();
        delay(1);
        display.clearDisplay();
        visChanged.selector = true;
        visChanged.rate     = true;
        visChanged.shut     = true;
        visChanged.status   = true;
        visChanged.timerBar = true;*/


        photoStart();
      }
    } else { rateTicks = 0; }

    drawTimerBar();
  } else { shutterTicks = 0; }


  if (visChanged.selector == true || visChanged.rate == true || visChanged.shut == true || visChanged.status == true || visChanged.timerBar == true){
    if (visChanged.selector == true) drawSelector(selectorHidden);
    if (visChanged.rate     == true) drawSlider( 0, "Rate", "sec", sliderValue[0][0], sliderValue[0][1], sliderValue[0][2]);
    if (visChanged.shut     == true) drawSlider(16, "Shut", "sec", sliderValue[1][0], sliderValue[1][1], sliderValue[1][2]);
    if (visChanged.status   == true) drawStatus(statusHidden);
    if (visChanged.timerBar == true) drawTimerBar();

    visChanged.reset();

    display.display();
  }
}
