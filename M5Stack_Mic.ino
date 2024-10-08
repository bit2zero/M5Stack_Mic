#include <M5Unified.h>
#include <M5GFX.h>
#include <SPI.h>
#include <SD.h>

int screenWidth, screenHeight;

const int sampleCount = 1280;   // 一度に取得するサンプル数
int16_t samples[sampleCount];  // サンプルを保存するバッファ
uint32_t sample_rate = 16000;

bool isRecording = false;
bool isPlaying   = false;

File audioFile;
const char* audioFileName = "/recording.raw";  // 録音ファイル名（RAWファイル形式）
int sdMountTimeout = 5000;  // SDカードマウントのタイムアウト時間（ミリ秒）

// M5Stack Core S3
#define SD_SPI_SCK_PIN  36
#define SD_SPI_MISO_PIN 35
#define SD_SPI_MOSI_PIN 37
#define SD_SPI_CS_PIN   4

// M5Stack Core S3 POAT A
#define I2S_WS      1
#define I2S_DATA_IN 2

M5GFX DispBuff;
M5Canvas canvas(&DispBuff);

int waveformHeight;  // 波形の高さ

// SDカードを一定時間マウントする関数
bool mountSDCard() {

  // SD Card Initialization
  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  
  auto startTime = millis();
  while (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
    if (millis() - startTime > sdMountTimeout) {
      return false;  // タイムアウト
    }
    delay(100);  // 少し待機して再トライ
  }
  return true;  // マウント成功
}

void displayMessage(const char* message) {
  // ディスプレイをクリアし、メッセージを表示（UTF-8日本語対応）
  DispBuff.setFont(&fonts::lgfxJapanGothicP_20);  // 日本語フォントを使用
  DispBuff.fillRect(0, screenHeight - 40, screenWidth, 40, TFT_BLACK); // 下部を黒でクリア
  DispBuff.setCursor(10, screenHeight - 30);
  DispBuff.setTextSize(1);  // 日本語フォントに合わせてサイズ調整
  DispBuff.setTextColor(TFT_WHITE);
  DispBuff.println(message);
}

void setup() {
  auto cfg = M5.config();

  //cfg.internal_mic = false;  // 内蔵マイクの使用を指定

  M5.begin(cfg);

  Serial.begin();

  auto mic_cfg = M5.Mic.config();

  if(!cfg.internal_mic){
    // PORT A for Core S3
    mic_cfg.pin_ws = I2S_WS; 
    mic_cfg.pin_data_in = I2S_DATA_IN;
  }
  mic_cfg.sample_rate = sample_rate;
  
  mic_cfg.over_sampling = 2;
  mic_cfg.magnification = 16;
  mic_cfg.noise_filter_level = 0; // 1(弱)～3(強), 室内想定のため 0(負荷なし)
  mic_cfg.dma_buf_len = 320;
  mic_cfg.dma_buf_count = 8;
  mic_cfg.task_priority = 2;

  M5.Mic.config(mic_cfg);
  M5.Mic.begin();

  // 実行時にディスプレイの幅と高さを取得
  screenWidth = M5.Display.width();
  screenHeight = M5.Display.height();
  waveformHeight = (screenHeight * 5) / 6;  // 波形の高さ

  // 初期画面設定
  DispBuff.begin();
  DispBuff.fillScreen(BLACK);
  DispBuff.setFont(&fonts::lgfxJapanGothicP_20);  // 日本語フォントを使用
  DispBuff.setTextSize(1);  // 日本語フォントに合わせてサイズ調整

  // SDカードのマウント処理（GPIO10をCSピン、通信速度は25MHz）
  if (!mountSDCard()) {
    displayMessage("SDカードのマウントに失敗しました!");
    while (1);  // 失敗時は停止
  }

  displayMessage("A: 録音開始 / 停止, B: 再生");
}

unsigned long drawElapseTime;
void drawWaveform() {
  auto drawStartTime = millis();

  DispBuff.startWrite();

  canvas.createSprite(screenWidth, waveformHeight);
  // 波形を描画
  for (int i = 0; i < screenWidth - 1; i++) {
    int y1 = map(samples[i]*1.5, -32768, 32767, 0, waveformHeight);    // サンプル値を画面のY座標にマッピング
    int y2 = map(samples[i + 1]*1.5, -32768, 32767, 0, waveformHeight); // 次のサンプルのY座標

    // 波形を画面に描画
    canvas.drawLine(i, y1, i + 1, y2, GREEN);
  }
  canvas.pushSprite(0, 0);

  DispBuff.endWrite();
  drawElapseTime = millis() - drawStartTime;
}

void loop() {
  M5.update();

  // タッチ状態の確認
  auto t = M5.Touch.getDetail();
  if (t.wasClicked()) {

    // ボタン領域を動的に設定（ディスプレイ幅に基づく）
    int buttonA_X_End = screenWidth / 3;
    int buttonB_X_Start = screenWidth / 3;
    int buttonB_X_End = 2 * (screenWidth / 3);

    if (t.x < buttonA_X_End) {
      if (isPlaying) return; // 再生中時は、ボタンAを無視

      // ボタンA: 録音開始または停止
      if (!isRecording) {
        audioFile = SD.open(audioFileName, FILE_WRITE);
        if (audioFile) {
          isRecording = true;
          displayMessage("録音中...");
        }
        else {
          displayMessage("ファイル作成に失敗しました！");
          delay(3000);
          return;
        }
      }
      else {
        audioFile.close();
        displayMessage("A: 録音開始 / 停止, B: 再生");
        isRecording = false;
      }
    }
    else if (t.x >= buttonB_X_Start && t.x < buttonB_X_End) {
      if (isRecording) return; // 録音中時は、ボタンBを無視
      
      // ボタンB: 再生開始または停止
      if(!isPlaying) {
        // 録音ファイルが存在するか確認
        if (!SD.exists(audioFileName)) {
          displayMessage("録音ファイルが見つかりません！");
          delay(3000);
          return;
        }

        audioFile = SD.open(audioFileName);
        if (!audioFile) {
          displayMessage("ファイルの読み込みに失敗しました！");
          delay(3000);
          return;
        }

        M5.Mic.end();
        delay(100);
        M5.Speaker.begin();
        M5.Speaker.setVolume(128); // max:255

        displayMessage("再生中...");
        isPlaying = true;
      }
      else
      {
        audioFile.close();

        M5.Speaker.end();
        delay(100);
        M5.Mic.begin();

        displayMessage("A: 録音開始 / 停止, B: 再生");
        isPlaying = false;
      }
    }
  }

  if (isPlaying) {
    if (audioFile.available()) {
      // 再生
      int bytesRead = audioFile.read((uint8_t*)samples, sizeof(samples));
      if (bytesRead > 0) {
        M5.Speaker.playRaw((int16_t*)samples, bytesRead / sizeof(int16_t), sample_rate);
        delay((sampleCount * 1000 / sample_rate) - drawElapseTime);
      }
    }
    else {
      // 再生終了
      audioFile.close();

      M5.Speaker.end();
      delay(100);
      M5.Mic.begin();

      displayMessage("A: 録音開始 / 停止, B: 再生");
      isPlaying = false;
    }
  }
  else
  {
    // 再生時以外は常に録音状態
    if(M5.Mic.record(samples, sampleCount, sample_rate)){
      // ファイル書き込み
      if (isRecording) {
        audioFile.write((uint8_t*)samples, sizeof(samples));
      }
    }
  }

  drawWaveform();
}