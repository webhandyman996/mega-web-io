// ESP32 網頁互動控制 v1
// 搭配 index.html?board=esp32 使用（Chrome / Edge，鮑率 115200）
// 開發板選 ESP32 Dev Module
//
// 指令（一行一個，網頁或序列埠監控視窗都能打）：
//   D2=1    2 號腳輸出 HIGH         D2=0  輸出 LOW
//   P2=128  2 號腳 PWM 輸出 0~255（輸出腳都可以）
//   I4      4 號腳設成輸入（內建上拉），狀態改變時回報
//   R32     32 號腳改成類比讀取（32、33 才需要；34、35、36、39 本來就是）
//   AUTO=1  每 0.2 秒回報類比值     AUTO=0  停止
//   ?       回報版本
//   RESET   全部腳位回到輸入
//
// 回報：
//   OK D2=1            指令成功
//   ERR 說明            指令有問題
//   D4:0               輸入腳的狀態
//   A:1234,0,...       36、39、34、35、32、33 的類比值（0~4095），沒開類比的腳是 -

const int OUT_PINS[] = {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
const int ADC_PINS[] = {36, 39, 34, 35, 32, 33};
const unsigned long AUTO_MS = 200;

bool watching[40];   // 哪些腳是數位輸入
bool pwmOn[40];      // 哪些腳正在 PWM
bool adcOn[40];      // 32、33 是否切成類比
int lastState[40];
bool autoAnalog = false;
unsigned long lastAuto = 0;
String line = "";

void setup() {
  Serial.begin(115200);
  for (int p = 0; p < 40; p++) watching[p] = pwmOn[p] = adcOn[p] = false;
  adcOn[34] = adcOn[35] = adcOn[36] = adcOn[39] = true;
  Serial.println();
  Serial.println("READY ESP32-IO v1");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      line.trim();
      if (line.length() > 0) handle(line);
      line = "";
    } else {
      line += c;
    }
  }

  // 輸入腳狀態有變就回報
  for (int p : OUT_PINS) {
    if (!watching[p]) continue;
    int s = digitalRead(p);
    if (s != lastState[p]) {
      lastState[p] = s;
      reportInput(p);
    }
  }

  if (autoAnalog && millis() - lastAuto >= AUTO_MS) {
    lastAuto = millis();
    reportAnalog();
  }
}

void handle(String cmd) {
  cmd.toUpperCase();
  char kind = cmd.charAt(0);
  int eq = cmd.indexOf('=');
  int pin = cmd.substring(1, eq < 0 ? cmd.length() : eq).toInt();
  int val = eq < 0 ? 0 : cmd.substring(eq + 1).toInt();

  if (cmd == "?") {
    Serial.println("ESP32-IO v1");
  } else if (cmd == "RESET") {
    for (int p : OUT_PINS) release(p), pinMode(p, INPUT);
    adcOn[32] = adcOn[33] = false;
    autoAnalog = false;
    Serial.println("OK RESET");
  } else if (cmd.startsWith("AUTO=")) {
    autoAnalog = val == 1;
    Serial.println("OK " + cmd);
  } else if ((kind == 'D' || kind == 'P' || kind == 'I' || kind == 'R') && (pin == 1 || pin == 3)) {
    Serial.println("ERR TX0、RX0 是 USB 通訊用，不能控制");
  } else if ((kind == 'D' || kind == 'P' || kind == 'I') && isInputOnly(pin)) {
    Serial.println("ERR " + String(pin) + " 號腳只能當類比輸入");
  } else if ((kind == 'D' || kind == 'P' || kind == 'I') && !isOutPin(pin)) {
    Serial.println("ERR 沒有 " + String(pin) + " 號腳");
  } else if (kind == 'D' && eq > 0) {
    release(pin);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, val ? HIGH : LOW);
    Serial.println("OK " + cmd);
  } else if (kind == 'P' && eq > 0) {
    val = constrain(val, 0, 255);
    release(pin);
    analogWrite(pin, val);
    pwmOn[pin] = true;
    Serial.println("OK P" + String(pin) + "=" + String(val));
  } else if (kind == 'I') {
    release(pin);
    pinMode(pin, INPUT_PULLUP);
    watching[pin] = true;
    lastState[pin] = digitalRead(pin);
    Serial.println("OK " + cmd);
    reportInput(pin);
  } else if (kind == 'R' && (pin == 32 || pin == 33)) {
    release(pin);
    pinMode(pin, INPUT);
    adcOn[pin] = true;
    Serial.println("OK " + cmd);
  } else {
    Serial.println("ERR 看不懂 " + cmd);
  }
}

// 換用法前，先把這根腳原本的用途解除
void release(int pin) {
  if (pwmOn[pin]) ledcDetach(pin);
  pwmOn[pin] = false;
  watching[pin] = false;
  if (pin == 32 || pin == 33) adcOn[pin] = false;
}

bool isOutPin(int pin) {
  for (int p : OUT_PINS) if (p == pin) return true;
  return false;
}

bool isInputOnly(int pin) {
  return pin == 34 || pin == 35 || pin == 36 || pin == 39;
}

void reportInput(int pin) {
  Serial.println("D" + String(pin) + ":" + String(lastState[pin]));
}

void reportAnalog() {
  Serial.print("A:");
  for (int i = 0; i < 6; i++) {
    if (i > 0) Serial.print(',');
    int p = ADC_PINS[i];
    if (adcOn[p]) Serial.print(analogRead(p));
    else Serial.print('-');
  }
  Serial.println();
}
