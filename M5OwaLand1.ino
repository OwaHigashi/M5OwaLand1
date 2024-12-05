// Mini SMF(Standard Midi File) Sequencer Sample Program
//
// SDカード内に格納したSMFファイルを自動で演奏します。
//
// 以下のI/F/ライブラリを使用します
//  M5Stack用MIDIモジュール2 https://necobit.com/denshi/m5-midi-module2/
//  MSTimer2
//  LovyanGFX
//
// オリジナルは @catsin さんの https://bitbucket.org/kyoto-densouan/smfseq/src/m5stack/
// necobitでは画面描画部分と、起動時自動スタートの処理への変更をしています。
//
//　コメントやコメントアウトなど、取っ散らかっているところがありますがご了承ください。
//
// 尾和東@Pococha技術枠
// necobit版SMFプレーヤーをUNIT-SYNTHを装着したCore2で演奏するように改造
// さらに、任意のファイル名を受け付けてプレイリストを生成するように修正

#include <M5Core2.h>

#include "common.h"
#include "SmfSeq.h"
#include "IntervalCheck.h"
#include "IntervalCheckMicros.h"
#define LGFX_AUTODETECT
#include <LovyanGFX.hpp>
SMF_SEQ_TABLE *pseqTbl; // SMFシーケンサハンドル

// Create an instance of LGFX for LovyanGFX
LGFX lcd;

// 最大の曲数とファイル名の長さを定義
#define MAX_SONGS 100
#define MAX_FILENAME_LENGTH 64

char songFilenames[MAX_SONGS][MAX_FILENAME_LENGTH]; // 曲のファイル名を格納する配列
int songCount = 0;                                  // 曲数
char *currentFilename = NULL;                       // 現在の曲のファイル名

#include <M5UnitSynth.h>
M5UnitSynth synth;

// Define programs
#define PROG_MENU 0
#define PROG_1 1
#define PROG_2 2

int currentProgram = PROG_MENU;
bool prog1Init = false;
bool prog2Init = false;

// Forward declarations of program setup and loop functions
void prog1Setup();
void prog1Loop();
void prog2Setup();
void prog2Loop();
void dispMenu();

// Variables for touch input
TouchPoint_t touchPoint;

void setup() {
  Serial.begin(115200);
  M5.begin();
  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(255);
  lcd.fillScreen(TFT_BLACK);
  // Initialize M5.Touch
  M5.Touch.begin();
  // Display the menu
  displayMenu();
}

void loop() {
  M5.update();
  switch (currentProgram) {
    case PROG_MENU:
      // Check for touch input
      if (M5.Touch.ispressed()) {
        touchPoint = M5.Touch.getPressPoint();
        if (touchPoint.y > 60 && touchPoint.y < 100) {
          // User selected Program 1
          currentProgram = PROG_1;
          prog1Init = false;
        } else if (touchPoint.y > 100 && touchPoint.y < 140) {
          // User selected Program 2
          currentProgram = PROG_2;
          prog2Init = false;
        }
      }
      break;

    case PROG_1:
      if (!prog1Init) {
        // Initialize program 1
        prog1Setup();
        prog1Init = true;
      }
      prog1Loop();
      break;

    case PROG_2:
      if (!prog2Init) {
        // Initialize program 2
        prog2Setup();
        prog2Init = true;
      }
      prog2Loop();
      // Check for exit condition to return to menu
      break;
  }
}

// Function to display the menu
void displayMenu() {
  Serial.println("Menu");
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextSize(2);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setCursor(20, 20);
  lcd.print("Select Program:");
  lcd.setCursor(20, 60);
  lcd.print("1. MIDI Player");
  lcd.setCursor(20, 100);
  lcd.print("2. Koshi Chime");
}




//----------------------------------------------------------------------
#include "MidiPort.h"
HardwareSerial MIDI_SERIAL(2); // UART2 を使用
int MidiPort_open()
{
  MIDI_SERIAL.begin(D_MIDI_PORT_BPS, SERIAL_8N1, -1, 32); // Core2 MIDI 出力をピン32で初期化
  return (0);
}
void MidiPort_close()
{
  MIDI_SERIAL.end();
}
int MidiPort_write(UCHAR data)
{
#ifdef DUMPMIDI
  DPRINT("1:");
  int n = (int)data;
  DPRINTLN(n, HEX);
#else
  MIDI_SERIAL.write(data);
#endif
  return (1);
}
int MidiPort_writeBuffer(UCHAR *pData, ULONG Len)
{
#ifdef DUMPMIDI
  int n;
  int i;
  DPRINT.print(Len);
  DPRINT.print(":");
  for (i = 0; i < Len; i++)
  {
    n = (int)pData[i];
    DPRINT.print(n, HEX);
  }
  DPRINTLN.println("");
#else
  MIDI_SERIAL.write(pData, Len);
#endif
  return (Len);
}
//----------------------------------------------------------------------
#include "SmfFileAccess.h"

File s_FileHd;
bool SmfFileAccessOpen(UCHAR *Filename)
{
  bool result = false;

  if (Filename != NULL)
  {
    lcd.setFont(&fonts::Font4);
    lcd.setTextSize(1);
    lcd.setCursor(5, 0);
    lcd.println((const char *)Filename);
    //s_FileHd = SD.open((const char *)Filename);
    char filepath[256];  // 必要に応じてサイズを調整してください
    snprintf(filepath, sizeof(filepath), "/smf/%s", Filename);
    s_FileHd = SD.open(filepath);
    result = s_FileHd.available();
  }
  return (result);
}
void SmfFileAccessClose()
{
  s_FileHd.close();
}
bool SmfFileAccessRead(UCHAR *Buf, unsigned long Ptr)
{
  bool result = true;
  if (Buf != NULL)
  {
    if (s_FileHd.position() != Ptr)
    {
      s_FileHd.seek(Ptr);
    }
    int data = s_FileHd.read();
    if (data >= 0)
    {
      *Buf = (UCHAR)data;
    }
    else
    {
      result = false;
    }
  }
  return (result);
}
bool SmfFileAccessReadNext(UCHAR *Buf)
{
  bool result = true;
  if (Buf != NULL)
  {
    int data = s_FileHd.read();
    if (data >= 0)
    {
      *Buf = (UCHAR)data;
    }
    else
    {
      result = false;
    }
  }
  return (result);
}
int SmfFileAccessReadBuf(UCHAR *Buf, unsigned long Ptr, int Lng)
{
  int result = 0;
  if (Buf != NULL)
  {
    if (s_FileHd.position() != Ptr)
    {
      s_FileHd.seek(Ptr);
    }

    int i;
    int data;
    for (i = 0; i < Lng; i++)
    {
      data = s_FileHd.read();
      if (data >= 0)
      {
        Buf[i] = (UCHAR)data;
        result++;
      }
      else
      {
        break;
      }
    }
  }
  return (result);
}
unsigned int SmfFileAccessSize()
{
  unsigned int result = 0;
  result = s_FileHd.size();

  return (result);
}

//----------------------------------------------------------------------
// #define D_CH_OFFSET_PIN 3 // チャンネル番号オフセット（eVY1のGM音源としての演奏）
// #define D_STATUS_LED 10   // 状態表示LED

int playdataCnt = 0; // 選曲番号

IntervalCheck sButtonCheckInterval(100, true);
IntervalCheckMicros sTickProcInterval(ZTICK * 1000, true);
IntervalCheck sStatusLedCheckInterval(100, true);
unsigned int sLedPattern = 0x0f0f;
IntervalCheck sUpdateScreenInterval(500, true);

// SMFファイル名生成
char *makeFilename(int seq)
{
  if (songCount == 0) {
    return NULL; // 曲がない場合
  }
  playdataCnt += seq;
  if (playdataCnt >= songCount) {
    playdataCnt = 0;
  } else if (playdataCnt < 0) {
    playdataCnt = songCount-1;
  }
  char *filename = songFilenames[playdataCnt];
  return filename;
}

void scanSongs()
{
  songCount = 0;
  File root = SD.open("/smf");
  if (!root)
  {
    lcd.println("Failed to open /smf folder");
    return;
  }
  if (!root.isDirectory())
  {
    lcd.println("/smf is not a folder");
    root.close();
    return;
  }
  File entry = root.openNextFile();
  while (entry && songCount < MAX_SONGS)
  {
    if (!entry.isDirectory())
    {
      const char *filename = entry.name(); // フルパスを取得
      String filenameStr(filename);
      if (filenameStr.endsWith(".mid") || filenameStr.endsWith(".MID") || filenameStr.endsWith(".smf") || filenameStr.endsWith(".SMF"))
      {
        strncpy(songFilenames[songCount], filename, MAX_FILENAME_LENGTH);
        songFilenames[songCount][MAX_FILENAME_LENGTH - 1] = '\0'; // 文字列の終端を保証
        Serial.println(filename);
        songCount++;
      }
    }
    entry.close();
    entry = root.openNextFile();
  }
  root.close();
}

void updateScreen()
{
  static String last_filename = "";
  static int last_status = -1;
  static int last_chNoOffset = -1;

  int status = SmfSeqGetStatus(pseqTbl);

  int chNoOffset = 0;

  if ((last_filename != String(currentFilename)) || (last_status != status) || (last_chNoOffset != chNoOffset))
  {
    lcd.fillScreen(TFT_BLACK);
    backscreen();
    lcd.setFont(&fonts::Font4);
    lcd.setTextSize(1);

    lcd.setCursor(5, 0);
    lcd.println(currentFilename);
    lcd.setCursor(5, 27);
    lcd.print(F("Status:"));
    switch (status)
    {
    case SMF_STAT_FILENOTREAD: // SMFファイル未読み込み（演奏不能）
      lcd.println(F("File load failed."));
      break;
    case SMF_STAT_STOP: // 演奏停止
      lcd.println(F("stop."));
      lcd.fillRect(260, 5, 40, 40, TFT_WHITE);
      break;
    case SMF_STAT_PLAY: // 演奏中
      lcd.println(F("playing."));
      lcd.fillRect(280, 5, 310, 45, TFT_BLACK);
      lcd.fillTriangle(280, 5, 280, 45, 310, 25, TFT_YELLOW);
      backscreen();
      break;
    case SMF_STAT_PAUSE: // 演奏一時停止中
      lcd.println(F("pause."));
      break;
    case SMF_STAT_STOPWAIT: // 演奏停止待ち（演奏中）
      lcd.println(F("wait."));
      break;
    default:
      break;
    }

    last_filename = String(currentFilename);
    last_status = status;
    last_chNoOffset = chNoOffset;
  }
}

void backscreen()
{
  lcd.setFont(&fonts::Font0);
  lcd.setTextSize(1);
  for (int chd = 1; chd <= 16; chd++)
  {
    int y = 49 + chd * 10;
    lcd.drawNumber(chd, 4, y + 1);
    lcd.drawFastHLine(2, y - 1, 316, 0xF660);
    lcd.fillRect(18, y, 300, 9, TFT_DARKGREY);
    lcd.setColor(TFT_BLACK);
    for (int oct = 0; oct < 11; ++oct)
    {
      int x = 18 + oct * 28;
      for (int n = 0; n < 7; ++n)
      {
        lcd.drawFastVLine(x + n * 4 + 3, y, 9);
      }
      lcd.fillRect(x + 2, y, 3, 5);
      lcd.fillRect(x + 6, y, 3, 5);
      lcd.fillRect(x + 14, y, 3, 5);
      lcd.fillRect(x + 18, y, 3, 5);
      lcd.fillRect(x + 22, y, 3, 5);
    }
  }
  lcd.drawFastVLine(16, 58, 161, 0xF660);
  lcd.drawRect(1, 58, 318, 161, 0xF660);
}

void prog1Setup()
{
//  M5.begin();
  // 最初に初期化関数を呼び出します。
//  lcd.init();
//  lcd.setRotation(1);
  // バックライトの輝度を 0～255 の範囲で設定します。
//  lcd.setBrightness(255);
//  lcd.clear(TFT_BLACK);
//  Serial.begin(115200);
//  pinMode(D_CH_OFFSET_PIN, INPUT_PULLUP);
  lcd.fillScreen(TFT_BLACK);
  lcd.println(F("Initializing SD card..."));
  if (!SD.begin())
  {
    lcd.println(F("Card failed, or not present"));
    delay(2000);
    return;
  }
  lcd.println(F("Card initialized."));
  delay(2000);

  lcd.println(F("Scanning SMF files..."));
  scanSongs();
  if (songCount == 0)
  {
    lcd.println("No SMF files found.");
    delay(2000);
    return;
  }

  int Ret;
  pseqTbl = SmfSeqInit(ZTICK);
  if (pseqTbl == NULL)
  {
    lcd.println(F("SmfSeqInit failed."));
    delay(2000);
    return;
  }

  int chNoOffset = 0;

  // SMFファイル読込
  currentFilename = makeFilename(0);
  if (currentFilename == NULL)
  {
    lcd.println("No SMF files to play.");
    delay(2000);
    return;
  }
  SmfSeqFileLoadWithChNoOffset(pseqTbl, currentFilename, chNoOffset);
  // 発音中全キーノートオフ
  Ret = SmfSeqAllNoteOff(pseqTbl);
  // トラックテーブルリセット
  SmfSeqPlayResetTrkTbl(pseqTbl);
}

int prePlayButtonStatus = HIGH;
int preFfButtonStatus = HIGH;
int preBwButtonStatus = HIGH;
void prog1Loop()
{
  int Ret;

  // 定期起動処理
  if (sTickProcInterval.check() == true)
  {
    if (SmfSeqGetStatus(pseqTbl) != SMF_STAT_STOP)
    {
      // 状態が演奏停止中以外の場合
      // 定期処理を実行
      Ret = SmfSeqTickProc(pseqTbl);
      // 処理が間に合わない場合のリカバリ
      while (sTickProcInterval.check() == true)
      {
        // 定期処理を実行
        Ret = SmfSeqTickProc(pseqTbl);
      }
      if (SmfSeqGetStatus(pseqTbl) == SMF_STAT_STOP)
      {
        // 状態が演奏停止中になった場合
        // 発音中全キーノートオフ
        Ret = SmfSeqAllNoteOff(pseqTbl);
        // トラックテーブルリセット
        SmfSeqPlayResetTrkTbl(pseqTbl);
        // ファイルクローズ
        SmfSeqEnd(pseqTbl);
        lcd.fillRect(280, 5, 310, 45, TFT_BLACK);
        lcd.setFont(&fonts::Font4);
        lcd.setCursor(5, 27);
        lcd.setTextSize(1);
        lcd.print(F("Status:"));
        lcd.println(F("SEQ end.  "));

        int chNoOffset = 0;

        pseqTbl = SmfSeqInit(ZTICK);
        // SMFファイル読込
        currentFilename = makeFilename(1);
        if (currentFilename == NULL)
        {
          lcd.println("No more SMF files to play.");
          return;
        }
        SmfSeqFileLoadWithChNoOffset(pseqTbl, currentFilename, chNoOffset);
        // トラックテーブルリセット
        SmfSeqPlayResetTrkTbl(pseqTbl);
        // 演奏開始
        SmfSeqStart(pseqTbl);
      }
    }
  }

  // ボタン操作処理
  if (sButtonCheckInterval.check() == true)
  {
    M5.update();
    // スイッチ状態取得
    int buttonPlayStatus = M5.BtnB.wasPressed();
    if (prePlayButtonStatus != buttonPlayStatus)
    {
      // スイッチ状態が変化していた場合
      if (buttonPlayStatus == LOW)
      {
        // スイッチ状態がONの場合
        if (SmfSeqGetStatus(pseqTbl) == SMF_STAT_STOP)
        {
          // 演奏開始
          SmfSeqStart(pseqTbl);
        }
        else
        {
          // 演奏中なら演奏停止
          SmfSeqStop(pseqTbl);
          // 発音中全キーノートオフ
          Ret = SmfSeqAllNoteOff(pseqTbl);
        }
      }
    }
    // スイッチ状態保持
    prePlayButtonStatus = buttonPlayStatus;

    int buttonFfStatus = M5.BtnC.wasPressed();
    if (preFfButtonStatus != buttonFfStatus) {
      // スイッチ状態が変化していた場合
      if (preFfButtonStatus == LOW) {
        // スイッチ状態がONの場合
        bool playing = false;
        if (SmfSeqGetStatus(pseqTbl) != SMF_STAT_STOP) {
          // 演奏中なら演奏停止
          SmfSeqStop(pseqTbl);
          // 発音中全キーノートオフ
          Ret = SmfSeqAllNoteOff(pseqTbl);
          // トラックテーブルリセット
          SmfSeqPlayResetTrkTbl(pseqTbl);
          // ファイルクローズ
          SmfSeqEnd(pseqTbl);
          playing = true;
        } else {
          playing = false;
        }
        int chNoOffset = 0;
        pseqTbl = SmfSeqInit(ZTICK);
        // SMFファイル読込
        currentFilename = makeFilename(1);
        if (currentFilename == NULL) {
          lcd.println("No more SMF files to play.");
          return;
        }
        SmfSeqFileLoadWithChNoOffset(pseqTbl, currentFilename, chNoOffset);
        // 発音中全キーノートオフ
        Ret = SmfSeqAllNoteOff(pseqTbl);
        // トラックテーブルリセット
        SmfSeqPlayResetTrkTbl(pseqTbl);
        if (playing == true) {
          // 演奏開始
          SmfSeqStart(pseqTbl);
        }
      }
    }
    // スイッチ状態保持
    preFfButtonStatus = buttonFfStatus;

    int buttonBwStatus = M5.BtnA.wasPressed();
    if (preBwButtonStatus != buttonBwStatus) {
      // スイッチ状態が変化していた場合
      if (preBwButtonStatus == LOW) {
        // スイッチ状態がONの場合
        bool playing = false;
        if (SmfSeqGetStatus(pseqTbl) != SMF_STAT_STOP) {
          // 演奏中なら演奏停止
          SmfSeqStop(pseqTbl);
          // 発音中全キーノートオフ
          Ret = SmfSeqAllNoteOff(pseqTbl);
          // トラックテーブルリセット
          SmfSeqPlayResetTrkTbl(pseqTbl);
          // ファイルクローズ
          SmfSeqEnd(pseqTbl);
          playing = true;
        } else {
          playing = false;
        }
        int chNoOffset = 0;
        pseqTbl = SmfSeqInit(ZTICK);
        // SMFファイル読込
        currentFilename = makeFilename(-1);
        if (currentFilename == NULL) {
          lcd.println("No more SMF files to play.");
          return;
        }
        SmfSeqFileLoadWithChNoOffset(pseqTbl, currentFilename, chNoOffset);
        // 発音中全キーノートオフ
        Ret = SmfSeqAllNoteOff(pseqTbl);
        // トラックテーブルリセット
        SmfSeqPlayResetTrkTbl(pseqTbl);
        if (playing == true) {
          // 演奏開始
          SmfSeqStart(pseqTbl);
        }
      }
    }
    // スイッチ状態保持
    preBwButtonStatus = buttonBwStatus;
  }

  // 状態表示更新
  if (SmfSeqGetStatus(pseqTbl) != SMF_STAT_STOP) {
    if (sStatusLedCheckInterval.check() == true) {
      unsigned int led = sLedPattern & 0x0001;
      if (led > 0) {
        // digitalWrite( D_STATUS_LED, HIGH );
      } else {
        // digitalWrite( D_STATUS_LED, LOW );
      }
      sLedPattern = (sLedPattern >> 1) | (led << 15);
    }
  } else {
    // digitalWrite( D_STATUS_LED, LOW );
  }

  if (sUpdateScreenInterval.check() == true) {
    updateScreen();
  }
}





// User can set the notes corresponding to each direction here
int notes[4][8] = {
  { NOTE_G3, NOTE_C4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_C5, NOTE_E5, NOTE_G5 },
  { NOTE_A3, NOTE_D4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_D5, NOTE_F5, NOTE_A5 },
  { NOTE_A3, NOTE_C4, NOTE_E4, NOTE_A4, NOTE_B4, NOTE_C5, NOTE_E5, NOTE_B5 },
  { NOTE_G3, NOTE_B4, NOTE_D4, NOTE_G4, NOTE_B4, NOTE_D5, NOTE_G5, NOTE_A5 }
};
const char* noteNames[4][8] = {
  { "G3", "C4", "E4", "F4", "G4", "C5", "E5", "G5" },
  { "A3", "D4", "F4", "G4", "A4", "D5", "F5", "A5" },
  { "A3", "C4", "E4", "A4", "B4", "C5", "E5", "B5" },
  { "G3", "B4", "D4", "G4", "B4", "D5", "G5", "A5" }
};
const char* cylinderNames[4] = { "Terra", "Aqua", "Aria", "Ignis"};

uint16_t noteColors[] = { RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, ORANGE, WHITE };

float lastAccelX = 0, lastAccelY = 0, lastAccelZ = 0;
float lastGyroX = 0, lastGyroY = 0, lastGyroZ = 0;

int prevNoteX = -1;
int prevNoteY = -1;

const int centerX = 160;
const int centerY = 120;

// Define structure to keep track of active notes
struct ActiveNote {
  int note;
  unsigned long startTime;
};

const int MAX_ACTIVE_NOTES = 10;
ActiveNote activeNotes[MAX_ACTIVE_NOTES];

void addActiveNote(int note, unsigned long startTime) {
  for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
    if (activeNotes[i].note == -1) {
      activeNotes[i].note = note;
      activeNotes[i].startTime = startTime;
      return;
    }
  }
  Serial.println("Error: Too many active notes");
}

void removeActiveNoteAt(int index) {
  activeNotes[index].note = -1;
  activeNotes[index].startTime = 0;
}

int preCylNextButtonStatus = HIGH;
int preCylPrevButtonStatus = HIGH;
int cylID = 0;
int prevcylID = -1;

void prog2Setup() {
  Serial.println("KoshiChime");

  // Initialize accelerometer and gyroscope
  M5.IMU.Init();

  // Initialize synth
  synth.begin(&Serial2, UNIT_SYNTH_BAUD, 33, 32);
  synth.setInstrument(0, 0, Glockenspiel);  // Set to Glockenspiel sound

  // Initialize active notes
  for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
    activeNotes[i].note = -1;
    activeNotes[i].startTime = 0;
  }

  // Initialize screen
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.drawCircle(centerX, centerY, 100, WHITE);
}

void prog2Loop() {
  float accelX, accelY, accelZ;
  float gyroX, gyroY, gyroZ;

  int cylNextbuttonStatus = M5.BtnC.wasPressed();
  if (preCylNextButtonStatus != cylNextbuttonStatus) {
    // スイッチ状態が変化していた場合
    if (cylNextbuttonStatus == LOW) {
      cylID++;
      if(cylID > 3) cylID = 0;
    }
  }
  int cylPrevbuttonStatus = M5.BtnA.wasPressed();
  if (preCylPrevButtonStatus != cylPrevbuttonStatus) {
    // スイッチ状態が変化していた場合
    if (cylPrevbuttonStatus == LOW) {
      cylID--;
      if(cylID < 0) cylID = 3;
    }
  }
  M5.update();

  // 先に背景を塗りつぶしてからテキストを描画
  if(cylID != prevcylID){
    M5.Lcd.fillRect(0, 0, 100, 30, BLACK);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.drawCentreString(cylinderNames[cylID], 50, 0, 2); // X座標を50に調整
    prevcylID = cylID;
  }

  M5.IMU.getAccelData(&accelX, &accelY, &accelZ);
  M5.IMU.getGyroData(&gyroX, &gyroY, &gyroZ);

  unsigned long currentTime = millis();

  // Calculate tilt angle in the XY plane
  float angle = atan2(accelY, accelX) * 180 / PI; // Convert radians to degrees
  if (angle < 0) angle += 360;
  int direction = (int)((angle + 22.5) / 45.0) % 8; // Divide into 8 sectors

  // Compute angular velocity magnitude
  float angularVelocity = sqrt(gyroX * gyroX + gyroY * gyroY + gyroZ * gyroZ);

  // Map angularVelocity to volume (adjust scale factor as needed)
  int volume = (int)(angularVelocity * 4) - 20;

  if (volume > 127) volume = 127;
  if (volume < 0) volume = 0;

  // Get the note corresponding to the current direction
  int note = notes[cylID][direction];

  // Play note if volume exceeds threshold
  if (volume > 10) {
    synth.setNoteOn(0, note, volume);
    addActiveNote(note, currentTime);
    Serial.printf("Vol: %d, Note %d\n", volume, note);

    // Erase previous note symbol
    if (prevNoteX != -1 && prevNoteY != -1) {
      // Define area to clear
      int clearSize = 60; // Adjust this size as needed to cover the text and circle
      M5.Lcd.fillRect(prevNoteX - clearSize / 2, prevNoteY - clearSize / 2, clearSize, clearSize, BLACK);

      // Redraw circle border if overlapped
      float distance = sqrt(pow(prevNoteX - centerX, 2) + pow(prevNoteY - centerY, 2)) + clearSize / 2;
      if (distance >= 100 - clearSize / 2 && distance <= 100 + clearSize / 2) {
        M5.Lcd.drawCircle(centerX, centerY, 100, WHITE);
      }
    }

    // Compute the position to draw the new note symbol
    // Distance from center proportional to volume
    float distanceFromCenter = (volume / 127.0) * 80; // max distance 80 pixels

    // Angle in radians
    float angle_rad = angle * PI / 180.0;

    // x and y offsets
    int x_offset = (int)(cos(angle_rad) * distanceFromCenter);
    int y_offset = (int)(sin(angle_rad) * distanceFromCenter);

    // Position to draw the note symbol
    int noteX = centerX + x_offset;
    int noteY = centerY + y_offset;

    // Get the note name
    const char* noteName = noteNames[cylID][direction];

    // Set the text color
    uint16_t color = noteColors[direction % 8];
    M5.Lcd.setTextColor(color, BLACK);
    M5.Lcd.setTextSize(2);

    // Draw the note symbol
    M5.Lcd.drawCentreString(noteName, noteX, noteY - 8, 2); // Adjust Y position
    // Draw a colored circle around the note
    M5.Lcd.drawCircle(noteX, noteY, 15, color);

    // Update prevNoteX and prevNoteY
    prevNoteX = noteX;
    prevNoteY = noteY;
  }

  // Check for notes that should be turned off after a certain duration
  unsigned long noteDuration = 1000; // Note duration in ms
  for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
    if (activeNotes[i].note != -1) {
      if (currentTime - activeNotes[i].startTime >= noteDuration) {
        // Send Note Off
        synth.setNoteOff(0, activeNotes[i].note, 0);
        // Remove note from activeNotes
        removeActiveNoteAt(i);
      }
    }
  }

  // Update last values
  lastAccelX = accelX;
  lastAccelY = accelY;
  lastAccelZ = accelZ;
  lastGyroX = gyroX;
  lastGyroY = gyroY;
  lastGyroZ = gyroZ;

  // CPU使用率を下げるためにディレイを追加
  delay(volume*0.8+70);
}
