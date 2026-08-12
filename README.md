# kanpai_cpp

C++とOpenCVを使用した簡単な画像処理プログラム

<!-- 画像を表示する例（.png への相対パスを指定してください） -->


## 概要 (Overview)
送られてきた画像を読み込み、5つの画像データを出力する

normal.png　//通常時 <br>
![normal.png] <img width="4284" height="5712" alt="normal" src="https://github.com/user-attachments/assets/2eab5a89-d5fe-442d-a163-f482c02e5ef6" />
ready.png　//乾杯準備時 <br>
kanpai.png　//乾杯時 <br>
good.png　//乾杯成功時 <br>
miss.png　//乾杯失敗時 <br>

## 動作環境 (Requirements)
動作確認を行った環境や必要なライブラリを記載します。

* **依存ライブラリ**: OpenCV 4.x

## ビルド・実行方法 (Build & Run)
コンパイルコマンド (g++ -std=c++11 kanpai3.cpp -o kanpai3 `pkg-config --cflags --libs opencv4`)
実行コマンド (./kanpai3)
