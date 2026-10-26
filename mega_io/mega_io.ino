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
const unsigned long AUTO_MS = 200;

bool watching[LAST_PIN + 1];  // 哪些腳是輸入
int lastState[LAST_PIN + 1];
bool autoAnalog = false;
unsigned long lastAuto = 0;
String line = "";

void setup() {
  Serial.begin(115200);
  for (int p = FIRST_PIN; p <= LAST_PIN; p++) watching[p] = false;
  Serial.println("READY MEGA-IO v1");
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
  for (int p = FIRST_PIN; p <= LAST_PIN; p++) {
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

  if (cmd == "?") {
    Serial.println("MEGA-IO v1");
  } else if (cmd == "RESET") {
    for (int p = FIRST_PIN; p <= LAST_PIN; p++) {
      pinMode(p, INPUT);
      watching[p] = false;
    }
    autoAnalog = false;
    Serial.println("OK RESET");
  } else if (cmd.startsWith("AUTO=")) {
    autoAnalog = cmd.substring(5).toInt() == 1;
    Serial.println("OK " + cmd);
  } else if (cmd.startsWith("D")) {
    int eq = cmd.indexOf('=');
    int pin = cmd.substring(1, eq).toInt();
    int val = cmd.substring(eq + 1).toInt();
    if (eq < 0 || !validPin(pin)) return error(cmd);
    watching[pin] = false;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, val ? HIGH : LOW);
    Serial.println("OK " + cmd);
  } else if (cmd.startsWith("P")) {
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

bool validPin(int pin) {
  return pin >= FIRST_PIN && pin <= LAST_PIN;
}

void error(String cmd) {
  int pin = cmd.substring(1).toInt();
  if ((cmd.startsWith("D") || cmd.startsWith("P") || cmd.startsWith("I")) && cmd.charAt(1) <= '1' && pin <= 1)
    Serial.println("ERR 0、1 號腳是 USB 通訊用，不能控制");
  else
    Serial.println("ERR 看不懂 " + cmd);
}

void reportInput(int pin) {
  Serial.println("D" + String(pin) + ":" + String(lastState[pin]));
}

void reportAnalog() {
  Serial.print("A:");
  for (int i = 0; i < 16; i++) {
    if (i > 0) Serial.print(',');
    Serial.print(analogRead(A0 + i));
  }
  Serial.println();
}
