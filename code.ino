/*
Tested library versions:
Adafruit BusIO: 1.17.4
Adafruit GFX Library: 1.12.3
Adafruit ST7735 and ST7789 Library: 1.11.0
ArtronShop_SHT3x: 1.0.0
*/

#include <Arduino.h>

#include <Wire.h>
#include <ArtronShop_SHT3x.h>

#include <Adafruit_GFX.h> 
#include <Adafruit_ST7735.h>

ArtronShop_SHT3x sht3x(0x44, &Wire); 

#define TFT_CS        10
#define TFT_RST        9
#define TFT_DC         8
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST); // Set HW SPI pins

#define BACKGROUND_COLOUR ST77XX_BLACK
#define PRIMARY_FOREGROUND_COLOUR tft.color565(187, 0, 255)
#define SECONDARY_FOREGROUND_COLOUR ST77XX_WHITE

const int buttonPin = 6;

const int AMOUNT_DATAPOINTS = 30; // ~100 max on Arduino Nano (2kB SRAM)
const float WAIT_TIME = 60;

class DataStorage {
private:
  float data[AMOUNT_DATAPOINTS];
  int rIndex;
  int rCount;
  const int MAX_DATA_POINTS;
  const String UNIT;

public:
  DataStorage(String unit, int maxDataPoints)
    : UNIT(unit), MAX_DATA_POINTS(maxDataPoints), rIndex(0), rCount(0) {
    for (int i = 0; i < this->MAX_DATA_POINTS; i++) {
      this->data[i] = 0;
    }
  }

  void addData(float newData) {
    this->data[this->rIndex] = newData;
    this->rIndex = (this->rIndex + 1) % this->MAX_DATA_POINTS;
    if (this->rCount < this->MAX_DATA_POINTS) this->rCount++;
  }

  float getDataByIndex(int logicalIndex) {
    if(logicalIndex < 0 || logicalIndex >= this->rCount) return 0;
    int realIndex = (this->rIndex + this->MAX_DATA_POINTS - this->rCount + logicalIndex) % this->MAX_DATA_POINTS; // pain
    return this->data[realIndex];
  }

  int getCursor() const { return this->rCount; }

  float getMaxDataPoint() {
    if (this->rCount == 0) return 0;
    float maxY = this->data[0];
    for (int i = 1; i < this->rCount; i++) {
      float v = this->data[i];
      if (v > maxY) maxY = v;
    }
    return maxY;
  }

  float getMinDataPoint() {
    if (this->rCount == 0) return 0;
    float minY = this->data[0];
    for (int i = 1; i < this->rCount; i++) {
      float v = this->data[i];
      if (v < minY) minY = v;
    }
    return minY;
  }

  float getAvgDataPoint() {
    if (this->rCount == 0) return 0;
    float sum = 0;
    for (int i = 0; i < this->rCount; i++) {
      sum += this->data[i];
    }
    return sum / this->rCount;
  }

  String getUnit() { return this->UNIT; }
};

class Element {
protected:
  const int X;
  const int Y;
  const int WIDTH;
  const int HEIGHT;
  const bool DRAW_BOARDER;
  const DataStorage& data;

  void drawBoarder() {
    tft.drawRect(X, Y, WIDTH, HEIGHT, PRIMARY_FOREGROUND_COLOUR);
  }

  void eraseBoarderContent() {
    tft.fillRect(X + 1, Y + 1, WIDTH - 2, HEIGHT - 2, BACKGROUND_COLOUR);
  }

public:
   Element(int x, int y, int width, int height, DataStorage& data, bool drawBoarder)
    : X(x), Y(y), WIDTH(width), HEIGHT(height), data(data), DRAW_BOARDER(drawBoarder) {}

  virtual void render() = 0;
};

class MaxAvgMinElement : public Element {
  protected:
    const int textSize;
  public:
   MaxAvgMinElement(int x, int y, int width, int height, DataStorage& data, bool drawBoarder, int textSize)
    : Element(x, y, width, height, data, drawBoarder), textSize(textSize) {}
  void render() override {
    if (this->DRAW_BOARDER) this->drawBoarder();
    this->eraseBoarderContent();

    const String unit = this->data.getUnit();
    tft.setTextSize(this->textSize);
    tft.setTextColor(SECONDARY_FOREGROUND_COLOUR);
    tft.setCursor(this->X + 1, this->Y + 1);
    tft.print(F("Max: "));
    tft.print(this->data.getMaxDataPoint(), 1);
    tft.print(unit);
    tft.setCursor(this->X + 1, this->Y + this->HEIGHT / 2 - this->textSize * 3.5);
    tft.print(F("Avg: "));
    tft.print(this->data.getAvgDataPoint(), 1);
    tft.print(unit);
    tft.setCursor(this->X + 1, this->Y + this->HEIGHT - this->textSize * 7);
    tft.print(F("Min: "));
    tft.print(this->data.getMinDataPoint(), 1);
    tft.print(unit);

  };
};

class showCurrentValue : public Element {
  public:
  using Element::Element;
  void render() override {
    if (this->DRAW_BOARDER) this->drawBoarder();
    this->eraseBoarderContent();

    const String unit = this->data.getUnit();
    tft.setTextSize(5);
    tft.setTextColor(PRIMARY_FOREGROUND_COLOUR);
    tft.setCursor(this->X + 1, this->Y + 1);
    tft.print(this->data.getDataByIndex(data.getCursor() - 1), 1);
    tft.print(this->data.getUnit());
  };
};

class GraphElement : public Element {
private:
  const int AMOUNT_DATAPOINTS;
  const int MARGIN;

  int getScaledX(int i) {
    return X + 2 + (int)(i * (this->WIDTH - 3) / (this->AMOUNT_DATAPOINTS - 1));
  }

  int getScaledY(float value) { return this->getScaledY(value, this->data.getMinDataPoint(), this->data.getMaxDataPoint()); }; // OOP is soo cool

  int getScaledY(float value, float minY, float maxY) {
    if (maxY - minY == 0) return this->Y + this->HEIGHT / 2;
    float norm = (value - minY) / (maxY - minY);
    const int scaledHeight = this->HEIGHT - 2 * this->MARGIN;
    return this->Y + this->MARGIN + (scaledHeight - 1) - (int)(norm * (scaledHeight - 1));
  }

  void drawMarker(float value, bool left, bool right) {
    int y = this->getScaledY(value, this->data.getMinDataPoint(), this->data.getMaxDataPoint());
    if (left) tft.drawLine(this->X, y, this->X + 3, y, SECONDARY_FOREGROUND_COLOUR);
    if (right) tft.drawLine(this->X + this->WIDTH - 1, y, this->X + this->WIDTH - 4, y, SECONDARY_FOREGROUND_COLOUR);
  }

public:
  GraphElement(int x, int y, int width, int height, DataStorage& data, bool drawBoarder, int amountDataPoints, int margin)
    : Element(x, y, width, height, data, drawBoarder), AMOUNT_DATAPOINTS(amountDataPoints), MARGIN(margin) {}
  void render() override {
    if (this->DRAW_BOARDER) this->drawBoarder();
    this->eraseBoarderContent();

    int count = this->data.getCursor();
    if (count == 0) return;
    
    const float minY = this->data.getMinDataPoint();
    const float maxY = this->data.getMaxDataPoint();

    for (int i = 0; i < count - 1; i++) {
      int x0 = this->getScaledX(i);
      int y0 = this->getScaledY(this->data.getDataByIndex(i), minY, maxY);
      int x1 = this->getScaledX(i + 1);
      int y1 = this->getScaledY(this->data.getDataByIndex(i + 1), minY, maxY);
      tft.drawLine(x0, y0, x1, y1, PRIMARY_FOREGROUND_COLOUR);
    }

    if (count == 1) {
      int x = this->getScaledX(0);
      int y = this->getScaledY(this->data.getDataByIndex(0));
      tft.drawPixel(x, y, PRIMARY_FOREGROUND_COLOUR);
    }
    this->drawMarker(minY, false, true);
    this->drawMarker(maxY, false, true);
    this->drawMarker(this->data.getAvgDataPoint(), false, true);
  }
};

class Screen {
protected:
  Element** elements;
public:
  Screen(Element** elems) : elements(elems){}; 
  void draw(){
    for(int i = 0; this->elements[i] != nullptr; i++) {
      this->elements[i]->render();
    };
  };
};

class DisplayConfig {
  private:
  Screen** screens;
  int screenIndex;
  int screenCount;
  
  public:
  DisplayConfig(Screen** screens) : screens(screens), screenIndex(0), screenCount(0){
    while (this->screens[this->screenCount] != nullptr) this->screenCount++;
    tft.initR(INITR_MINI160x80);
    tft.setRotation(3);
    tft.fillScreen(BACKGROUND_COLOUR);
  };

  void setScreen(int i) {
    this->screenIndex = i;
  }

  void applyScreen(int i){
    this->setScreen(i);
    this->drawScreen(i);    
  };

  void fillScreen(int c){
    tft.fillScreen(c);
  }

  void cycleScreen() {this->screenIndex = (this->screenIndex + 1) % this->screenCount;}

  // Screen* getCurrentScreen() {return this->screens[this->screenIndex];}
  int getCurrentScreenIndex() {return this->screenIndex;}

  void drawScreen(int i) {
    tft.fillScreen(BACKGROUND_COLOUR);
    this->screens[i]->draw();
  }

  void drawCurrentScreen() {
    this->drawScreen(this->screenIndex);
  }
};




const byte DEBOUNCE_DELAY_MILLY = 250;
const int SLEEP_DELAY_SECONDS = 30;
unsigned long previousMillis = -1000000000;
unsigned long lastPressTime = millis();
bool buttonPressedLastCycle = false;
bool sht3xErrorLastCycle = false;
bool isSleeping = false;


// CONFIG START
DataStorage* tempData;
DataStorage* humData;
GraphElement* graphTemp;
GraphElement* graphHum;
MaxAvgMinElement* bigMaxAvgMinTemp;
MaxAvgMinElement* bigMaxAvgMinHum;
MaxAvgMinElement* maxAvgMinTemp;
MaxAvgMinElement* maxAvgMinHum;
showCurrentValue* showCurrentTemp;
showCurrentValue* showCurrentHum;
Screen* screenTemp;
Screen* screenHum;
Screen* screenBigTemp;
Screen* screenBigHum;
Screen* screenCurrent;

DisplayConfig* config;

void setup() {

  // Datapoint storage. Arguments: Unit, amount of datapoints to be saved.
  tempData = new DataStorage("C", AMOUNT_DATAPOINTS);
  humData = new DataStorage("%", AMOUNT_DATAPOINTS);

  // Screen elements. Base arguments: X Pos, Y Pos, Width, Lenght, Datasource, Border true/false.
  bigMaxAvgMinTemp = new MaxAvgMinElement(5, 5, 65, 70, *tempData, false, 2);
  bigMaxAvgMinHum = new MaxAvgMinElement(5, 5, 65, 70, *humData, false, 2);
  graphTemp = new GraphElement(5, 5, 80, 70, *tempData, true, AMOUNT_DATAPOINTS, 10);
  maxAvgMinTemp = new MaxAvgMinElement(90, 5, 65, 70, *tempData, false, 1);
  graphHum = new GraphElement(5, 5, 80, 70, *humData, true, AMOUNT_DATAPOINTS, 10);
  maxAvgMinHum = new MaxAvgMinElement(90, 5, 65, 70, *humData, false, 1);
  showCurrentTemp = new showCurrentValue(0, 0, 40, 80, *tempData, false); 
  showCurrentHum = new showCurrentValue(0, 40, 40, 80, *humData, false);

  // Element 'collections' / arrays. Arguments: Array containing pointers to elements, nullptr must be at the end.
  static Element* elemArrTemp[] = { graphTemp, maxAvgMinTemp, nullptr };
  static Element* elemArrHum[] = { graphHum, maxAvgMinHum, nullptr };
  static Element* elemBigArrTemp[] = { bigMaxAvgMinTemp, nullptr };
  static Element* elemBigArrHum[] = { bigMaxAvgMinHum, nullptr };
  static Element* currentArr[] = { showCurrentTemp, showCurrentHum, nullptr };

  // Screens. Arguments: Element arrays.
  screenTemp = new Screen(elemArrTemp);
  screenHum = new Screen(elemArrHum);
  screenBigTemp = new Screen(elemBigArrTemp);
  screenBigHum = new Screen(elemBigArrHum);
  screenCurrent = new Screen(currentArr);


  // Screen 'collections' / arrays: Arguments: Array containing pointers to screens, nullptr must be at the end.
  static Screen* screenArr[] = { screenCurrent, screenTemp, screenHum, screenBigTemp, screenBigHum, nullptr};

  // Config. Arguments: A screen array.
  config = new DisplayConfig(screenArr);
  // CONFIG END


  Serial.begin(115200);
  Serial.println(F("Serial started."));

  pinMode(buttonPin, INPUT);

  Wire.begin();

  while (!sht3x.begin()) {
    Serial.println(F("SHT3x not found"));
    delay(500);
  }
  delay(1000);
}


void loop() {
  unsigned long currentMillis = millis();
  if (sht3xErrorLastCycle) {
    config->drawCurrentScreen();
    sht3xErrorLastCycle = false;
  }
  if (currentMillis - lastPressTime >= SLEEP_DELAY_SECONDS * 1000) {
    isSleeping = true;
  }

  if (isSleeping) {
    config->fillScreen(BACKGROUND_COLOUR);
  }

  if (currentMillis - previousMillis >= WAIT_TIME * 1000) {
    previousMillis = currentMillis;
    if (sht3x.measure()) {
      float temp = sht3x.temperature();
      float hum = sht3x.humidity();

      tempData->addData(temp);
      humData->addData(hum);
      

      config->drawCurrentScreen();

      Serial.print(F("Temperature: "));
      Serial.print(temp);
      Serial.print(F("°C, Humidity: "));
      Serial.print(hum);
      Serial.println(F("%"));
    } else {
      Serial.println(F("SHT3x read error"));
      config->fillScreen(0xF000);
      tft.setCursor(0, 0);
      tft.setTextColor(SECONDARY_FOREGROUND_COLOUR);
      tft.setTextSize(3);
      tft.print(F("SENSOR\nREAD\nERROR"));
      delay(500);
      Wire.begin();
      sht3x.begin(); 
      sht3xErrorLastCycle = true;
    }
  }
  if (digitalRead(buttonPin)){
      if (!buttonPressedLastCycle && millis() - lastPressTime > DEBOUNCE_DELAY_MILLY){
        buttonPressedLastCycle = true;
        lastPressTime = millis();
        if (isSleeping) {
          isSleeping = false;
        } else {
          config->cycleScreen();
        }
        config->drawCurrentScreen();
      }
  } else {
    buttonPressedLastCycle = false;
  }
}


