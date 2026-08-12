# kanpai_cpp

C++とOpenCVを使用した簡単な画像処理プログラム

<!-- 画像を表示する例（.png への相対パスを指定してください） -->
![デモ画面]()

## 概要 (Overview)
送られてきた画像を読み込み、5つの画像データを出力する

normal.png　//通常時. 
ready.png　//乾杯準備時. 
kanpai.png　//乾杯時. 
good.png　//乾杯成功時. 
miss.png　//乾杯失敗時. 

## 動作環境 (Requirements)
動作確認を行った環境や必要なライブラリを記載します。

* **依存ライブラリ**: OpenCV 4.x

## ビルド・実行方法 (Build & Run)
コンパイルコマンド (g++ -std=c++11 kanpai3.cpp -o kanpai3 `pkg-config --cflags --libs opencv4`)
実行コマンド (./kanpai3)
