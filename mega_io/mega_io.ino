// Mega 網頁互動控制 v1
// 搭配 index.html 使用（Chrome / Edge，鮑率 115200）
//
// 指令（一行一個，網頁或序列埠監控視窗都能打）：
//   D9=1    9 號腳輸出 HIGH        D9=0  輸出 LOW
//   P9=128  9 號腳 PWM 輸出 0~255（只有 2~13、44~46 可以）
//   I22     22 號腳設成輸入（內建上拉），狀態改變時回報
//   AUTO=1  每 0.2 秒回報 A0~A15    AUTO=0  停止
//   ?       回報版本
//   RESET   全部腳位回到輸入
//
// 回報：
//   OK D9=1          指令成功
//   ERR 說明          指令有問題
//   D22:0            輸入腳的狀態
//   A:512,13,...     A0~A15 的數值（16 個）

const int FIRST_PIN = 2;      // 0、1 是 USB 通訊用，不給控制
const int LAST_PIN = 53;
// 類比值每 200 毫秒回報一次
const unsigned long AUTO_MS = 200;

bool watching[LAST_PIN + 1];  // 哪些腳是輸入
// 輸入腳上一次的狀態
int lastState[LAST_PIN + 1];
// 是否正在連續回報類比值
bool autoAnalog = false;
// 上一次回報類比值的時間
unsigned long lastAuto = 0;
// 累積還沒收完的一行指令
String line = "";

// 開機時執行一次
void setup() {
  Serial.begin(115200);
  for (int p = FIRST_PIN; p <= LAST_PIN; p++) watching[p] = false;
  // 告訴網頁板子已經準備好
  Serial.println("READY MEGA-IO v1");
}

// 不斷重複：收指令、監看輸入腳、回報類比值
void loop() {
  // 有資料就一次讀一個字元，遇到換行才算一句
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      // 一句話收完：去掉空白，交給 handle
      line.trim();
      if (line.length() > 0) handle(line);
      line = "";
    } else {
      line += c;
    }
  }

  // 輸入腳狀態有變就回報
  for (int p = FIRST_PIN; p <= LAST_PIN; p++) {
    if (!watching[p]) continue;
    int s = digitalRead(p);
    if (s != lastState[p]) {
      lastState[p] = s;
      reportInput(p);
    }
  }

  // 用 millis 計時，不用 delay，程式才不會卡住
  if (autoAnalog && millis() - lastAuto >= AUTO_MS) {
    lastAuto = millis();
    reportAnalog();
  }
}

// 處理一行指令：看開頭判斷種類
void handle(String cmd) {
  // 轉成大寫，打小寫也可以
  cmd.toUpperCase();

  if (cmd == "?") {
    // ?：回報版本
    Serial.println("MEGA-IO v1");
  } else if (cmd == "RESET") {
    // RESET：全部腳位改回輸入，停止監看和類比回報
    for (int p = FIRST_PIN; p <= LAST_PIN; p++) {
      pinMode(p, INPUT);
      watching[p] = false;
    }
    autoAnalog = false;
    Serial.println("OK RESET");
  } else if (cmd.startsWith("AUTO=")) {
    // AUTO=1 開始回報類比值，AUTO=0 停止
    autoAnalog = cmd.substring(5).toInt() == 1;
    Serial.println("OK " + cmd);
  } else if (cmd.startsWith("D")) {
    // D9=1：數位輸出，等號前是腳位，等號後是數值
    int eq = cmd.indexOf('=');
    int pin = cmd.substring(1, eq).toInt();
    int val = cmd.substring(eq + 1).toInt();
    if (eq < 0 || !validPin(pin)) return error(cmd);
    watching[pin] = false;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, val ? HIGH : LOW);
    Serial.println("OK " + cmd);
  } else if (cmd.startsWith("P")) {
    // P9=128：PWM 輸出，數值限制在 0～255
    int eq = cmd.indexOf('=');
    int pin = cmd.substring(1, eq).toInt();
    int val = constrain(cmd.substring(eq + 1).toInt(), 0, 255);
    if (eq < 0 || !validPin(pin)) return error(cmd);
    if (!digitalPinHasPWM(pin)) {
      Serial.println("ERR " + String(pin) + " 號腳不能 PWM");
      return;
    }
    watching[pin] = false;
    pinMode(pin, OUTPUT);
    analogWrite(pin, val);
    Serial.println("OK P" + String(pin) + "=" + String(val));
  } else if (cmd.startsWith("I")) {
    // I22：設成輸入並打開內建上拉，開始監看
    int pin = cmd.substring(1).toInt();
    if (!validPin(pin)) return error(cmd);
    pinMode(pin, INPUT_PULLUP);
    watching[pin] = true;
    lastState[pin] = digitalRead(pin);
    Serial.println("OK " + cmd);
    reportInput(pin);
  } else {
    error(cmd);
  }
}

// 檢查腳位是否在可控制的範圍
bool validPin(int pin) {
  return pin >= FIRST_PIN && pin <= LAST_PIN;
}

// 回報錯誤；0、1 號腳另外說明原因
void error(String cmd) {
  int pin = cmd.substring(1).toInt();
  if ((cmd.startsWith("D") || cmd.startsWith("P") || cmd.startsWith("I")) && cmd.charAt(1) <= '1' && pin <= 1)
    Serial.println("ERR 0、1 號腳是 USB 通訊用，不能控制");
  else
    Serial.println("ERR 看不懂 " + cmd);
}

// 回報輸入腳狀態，例如 D22:0
void reportInput(int pin) {
  Serial.println("D" + String(pin) + ":" + String(lastState[pin]));
}

// 回報 A0～A15，例如 A:512,13,...
void reportAnalog() {
  Serial.print("A:");
  for (int i = 0; i < 16; i++) {
    if (i > 0) Serial.print(',');
    Serial.print(analogRead(A0 + i));
  }
  Serial.println();
}
