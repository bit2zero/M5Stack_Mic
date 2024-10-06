#include <M5Unified.h>
#include <M5GFX.h>

const int sampleCount = 320;   // 一度に取得するサンプル数
int16_t samples[sampleCount];  // サンプルを保存するバッファ

M5GFX DispBuff;
M5Canvas canvas(&DispBuff);

void setup() {
  auto cfg = M5.config();
  cfg.internal_mic = false;  // 内蔵マイクの使用を指定
  M5.begin(cfg);

  auto mic_cfg = M5.Mic.config();
  if(!cfg.internal_mic){
    // PORT A for Core S3
    mic_cfg.pin_ws = 1; 
    mic_cfg.pin_data_in = 2;
  }
  mic_cfg.sample_rate = 16000;
  M5.Mic.config(mic_cfg);

  // 画面の初期化
  DispBuff.begin();
  DispBuff.fillScreen(BLACK);
}

void loop() {
  M5.update();

  // マイクからデータを取得
  M5.Mic.record(samples, sampleCount);
  
  DispBuff.startWrite();
  canvas.createSprite(320, 240);
  // 波形を描画
  for (int i = 0; i < sampleCount - 1; i++) {
    int y1 = map(samples[i], -32768, 32767, 0, 240);    // サンプル値を画面のY座標にマッピング
    int y2 = map(samples[i + 1], -32768, 32767, 0, 240); // 次のサンプルのY座標

    // 波形を画面に描画
    canvas.drawLine(i, y1, i + 1, y2, GREEN);
  }
  canvas.pushSprite(0, 0);
  DispBuff.endWrite();

  delay(10); // ループの速度を調整
}