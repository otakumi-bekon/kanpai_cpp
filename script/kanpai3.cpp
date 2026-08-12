// コンパイルコマンド:
// g++ -std=c++11 kanpai3.cpp -o kanpai3 `pkg-config --cflags --libs opencv4`

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <opencv2/opencv.hpp>

// =================================================================
// ⚙️ パラメータ設定エリア（すべてここで一括管理）
// =================================================================

// 1. ディレクトリパス設定
const std::string INPUT_DIR  = "../imput_image/";
const std::string SOUSE_DIR  = "../souse_image/";
const std::string OUTPUT_DIR = "../output_image/";

// 2. 使用ファイル名設定 (背景素材・出力名・パネル名をインデックスで対応付け)
const std::vector<std::string> SOUSE_FILES  = { "normal_souse.png", "Ready_souse.png", "kanpai_souse.png", "good_souse.png", "miss_souse.png" };
const std::vector<std::string> OUTPUT_FILES = { "normal.png",       "ready.png",       "kanpai.png",       "good.png",       "miss.png" };
const std::vector<std::string> PANEL_FILES  = { "panel_souse4.png", "panel_souse4.png", "panel_souse4.png", "good_panel.png", "miss_panel.png" };

// 3. 各出力画像ごとの回転角度・横移動量設定 (全5種分)
// 必要に応じて good (4番目), miss (5番目) の角度や横位置を調整してください
const std::vector<double> ANGLE_LIST    = { 0.0, -13.0, 10.0,  0.0,  0.0 }; // 角度 (度)
const std::vector<int>    OFFSET_X_LIST = {   0,   200,    -20,    80,    80 }; // 横移動オフセット (px)

// 4. 顔切り抜きパラメータ
const double FACE_CUT_SCALE = 1.00; // 楕円のカット倍率
const double FACE_ASPECT_X  = 0.72; // 横幅の比率補正（正方形枠から縦長に絞る）
const double FACE_ASPECT_Y  = 0.95; // 縦幅の比率補正
const int    FACE_BLUR_SIZE = 7;    // 境界のぼかし量 (フェザリング)

// 5. カツラ・パネル処理パラメータ
const int    WHITE_THRESHOLD     = 230;  // 白地判定の閾値（これ以上を透明化）
const double PANEL_SCALE_MULT    = 0.90; // カツラの拡大倍率（顔に対する比率）
const int    UNSCALED_PANEL_LIFT = -51;  // カツラを上に引き上げる量 (px)

// 6. 下半身素材との合成パラメータ
const double HEAD_SCALE = 3.0; // 頭部全体の拡大倍率
const int    OFFSET_Y   = 150;  // 首元の高さ（縦方向）オフセット (px)

// =================================================================
// 画像処理関数
// =================================================================

/**
 * @brief 画像を指定角度（度）回転させる（透過アルファチャンネル維持）
 */
cv::Mat rotateImage(const cv::Mat& src, double angle) {
    if (angle == 0.0) return src.clone();

    cv::Point2f center(src.cols / 2.0f, src.rows / 2.0f);
    cv::Mat rotMat = cv::getRotationMatrix2D(center, angle, 1.0);

    cv::Rect2f bbox = cv::RotatedRect(cv::Point2f(), src.size(), angle).boundingRect2f();
    rotMat.at<double>(0, 2) += bbox.width / 2.0 - center.x;
    rotMat.at<double>(1, 2) += bbox.height / 2.0 - center.y;

    cv::Mat dst;
    cv::warpAffine(src, dst, rotMat, bbox.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0, 0));
    return dst;
}

/**
 * @brief 背景画像の上にアルファチャンネル付き画像を重ね合わせる
 */
cv::Mat overlayImage(const cv::Mat& baseImage, const cv::Mat& overlay, cv::Point position) {
    cv::Mat result = baseImage.clone();

    int startX = std::max(0, position.x);
    int startY = std::max(0, position.y);
    int endX   = std::min(result.cols, position.x + overlay.cols);
    int endY   = std::min(result.rows, position.y + overlay.rows);

    int overlayStartX = startX - position.x;
    int overlayStartY = startY - position.y;

    for (int y = startY, fy = overlayStartY; y < endY; ++y, ++fy) {
        const cv::Vec4b* overPtr = overlay.ptr<cv::Vec4b>(fy);
        
        if (result.channels() == 4) {
            cv::Vec4b* basePtr = result.ptr<cv::Vec4b>(y);
            for (int x = startX, fx = overlayStartX; x < endX; ++x, ++fx) {
                const cv::Vec4b& overPixel = overPtr[fx];
                uchar alpha = overPixel[3];

                if (alpha > 0) {
                    cv::Vec4b& basePixel = basePtr[x];
                    if (basePixel[3] == 0) {
                        basePixel = overPixel;
                    } else {
                        float a = alpha / 255.0f;
                        for (int c = 0; c < 3; ++c) {
                            basePixel[c] = static_cast<uchar>(overPixel[c] * a + basePixel[c] * (1.0f - a));
                        }
                        basePixel[3] = std::max(basePixel[3], alpha);
                    }
                }
            }
        } else if (result.channels() == 3) {
            cv::Vec3b* basePtr = result.ptr<cv::Vec3b>(y);
            for (int x = startX, fx = overlayStartX; x < endX; ++x, ++fx) {
                const cv::Vec4b& overPixel = overPtr[fx];
                uchar alpha = overPixel[3];

                if (alpha > 0) {
                    float a = alpha / 255.0f;
                    cv::Vec3b& basePixel = basePtr[x];
                    for (int c = 0; c < 3; ++c) {
                        basePixel[c] = static_cast<uchar>(overPixel[c] * a + basePixel[c] * (1.0f - a));
                    }
                }
            }
        }
    }
    return result;
}

/**
 * @brief パネル画像の白背景領域を透過（Alpha=0）に変換する
 */
cv::Mat createTransparentPanel(const cv::Mat& panelImage, int whiteThreshold) {
    cv::Mat panel = panelImage.clone();
    if (panel.channels() == 3) {
        cv::cvtColor(panel, panel, cv::COLOR_BGR2BGRA);
    }

    for (int y = 0; y < panel.rows; ++y) {
        cv::Vec4b* ptr = panel.ptr<cv::Vec4b>(y);
        for (int x = 0; x < panel.cols; ++x) {
            if (ptr[x][0] >= whiteThreshold && ptr[x][1] >= whiteThreshold && ptr[x][2] >= whiteThreshold) {
                ptr[x][3] = 0;
            }
        }
    }
    return panel;
}

/**
 * @brief 縦長楕円切り抜き関数
 */
cv::Mat extractFaceWithEllipse(const cv::Mat& inputImage, const cv::Rect& faceRect, double cutScale, int blurSize, double aspectX, double aspectY) {
    cv::Rect safeBox = faceRect & cv::Rect(0, 0, inputImage.cols, inputImage.rows);
    cv::Mat faceRoiBgr = inputImage(safeBox);
    
    cv::Mat faceRoi;
    cv::cvtColor(faceRoiBgr, faceRoi, cv::COLOR_BGR2BGRA);

    cv::Mat mask = cv::Mat::zeros(safeBox.size(), CV_8UC1);
    cv::Point center(safeBox.width / 2, safeBox.height / 2);

    cv::Size axes(
        static_cast<int>((safeBox.width / 2.0) * cutScale * aspectX),
        static_cast<int>((safeBox.height / 2.0) * cutScale * aspectY)
    );

    cv::ellipse(mask, center, axes, 0, 0, 360, cv::Scalar(255), -1);

    if (blurSize > 1) {
        if (blurSize % 2 == 0) blurSize++;
        cv::GaussianBlur(mask, mask, cv::Size(blurSize, blurSize), 0);
    }

    for (int y = 0; y < faceRoi.rows; ++y) {
        cv::Vec4b* pixelPtr = faceRoi.ptr<cv::Vec4b>(y);
        const uchar* maskPtr = mask.ptr<uchar>(y);

        for (int x = 0; x < faceRoi.cols; ++x) {
            pixelPtr[x][3] = maskPtr[x];
        }
    }

    return faceRoi;
}

/**
 * @brief Haar Cascadeによる顔位置検出
 */
bool detectPrimaryFace(const cv::Mat& inputImage, cv::Rect& outFaceRect) {
    cv::CascadeClassifier face_cascade;
    std::string cascade_path = cv::samples::findFile("haarcascades/haarcascade_frontalface_alt.xml", false);
    if (!face_cascade.load(cascade_path)) {
        std::cerr << "[エラー] 顔検出用カスケードファイルが見つかりません。" << std::endl;
        return false;
    }

    cv::Mat gray;
    cv::cvtColor(inputImage, gray, cv::COLOR_BGR2GRAY);
    std::vector<cv::Rect> faces;
    face_cascade.detectMultiScale(gray, faces, 1.1, 3, 0, cv::Size(100, 100));

    if (faces.empty()) return false;

    outFaceRect = faces[0];
    for (const auto& f : faces) {
        if (f.area() > outFaceRect.area()) outFaceRect = f;
    }
    return true;
}

// =================================================================
// メイン処理
// =================================================================
int main() {
    std::cout << "=== 乾杯・顔切り抜き＆自動合成システム (kanpai3) 起動 ===" << std::endl;

    // 1. 入力画像の読み込み
    std::vector<cv::String> input_files;
    cv::glob(INPUT_DIR + "*.*", input_files, false);
    if (input_files.empty()) {
        std::cerr << "[エラー] " << INPUT_DIR << " 内に入力画像が見つかりません。" << std::endl;
        return -1;
    }

    std::string input_path = input_files[0];
    std::cout << "[読込] 入力画像: " << input_path << std::endl;
    cv::Mat input_image = cv::imread(input_path, cv::IMREAD_COLOR);
    if (input_image.empty()) {
        std::cerr << "[エラー] 入力画像の読み込みに失敗しました。" << std::endl;
        return -1;
    }

    // 2. 顔位置の検出
    cv::Rect face_rect;
    if (!detectPrimaryFace(input_image, face_rect)) {
        std::cerr << "[エラー] 画像から顔が検出されませんでした。" << std::endl;
        return -1;
    }

    // 3. 【ステップ①】顔の切り抜き実行
    cv::Mat transparent_face = extractFaceWithEllipse(
        input_image, face_rect, FACE_CUT_SCALE, FACE_BLUR_SIZE, FACE_ASPECT_X, FACE_ASPECT_Y
    );

    // 4. 【ステップ②＆③】素材ごとのパネル結合 ＆ 背景素材への合成ループ
    for (size_t i = 0; i < SOUSE_FILES.size(); ++i) {
        // A. 今回の素材に対応するパネル画像を読み込み & 白地透過化
        std::string panel_path = SOUSE_DIR + PANEL_FILES[i];
        cv::Mat rawPanel = cv::imread(panel_path, cv::IMREAD_UNCHANGED);
        if (rawPanel.empty()) {
            std::cerr << "[エラー] パネル画像の読み込みに失敗しました: " << panel_path << std::endl;
            continue;
        }
        cv::Mat transparent_panel = createTransparentPanel(rawPanel, WHITE_THRESHOLD);

        // B. 顔と今回のパネルを結合（頭部画像の作成）
        cv::Mat scaled_panel;
        double panel_scale_ratio = (double)transparent_face.cols / (double)transparent_panel.cols * PANEL_SCALE_MULT;
        cv::resize(transparent_panel, scaled_panel, cv::Size(), panel_scale_ratio, panel_scale_ratio, cv::INTER_LINEAR);

        int rel_panel_x = (transparent_face.cols - scaled_panel.cols) / 2;
        int rel_panel_y = (transparent_face.rows - scaled_panel.rows) / 2 + UNSCALED_PANEL_LIFT;

        int min_x = std::min(0, rel_panel_x);
        int min_y = std::min(0, rel_panel_y);
        int max_x = std::max(transparent_face.cols, rel_panel_x + scaled_panel.cols);
        int max_y = std::max(transparent_face.rows, rel_panel_y + scaled_panel.rows);

        int canvas_w = max_x - min_x;
        int canvas_h = max_y - min_y;

        cv::Mat combined_head = cv::Mat::zeros(canvas_h, canvas_w, CV_8UC4);
        combined_head = overlayImage(combined_head, transparent_face, cv::Point(-min_x, -min_y));
        combined_head = overlayImage(combined_head, scaled_panel, cv::Point(rel_panel_x - min_x, rel_panel_y - min_y));

        // C. 背景・首下素材の読み込み
        std::string souse_path = SOUSE_DIR + SOUSE_FILES[i];
        cv::Mat bg_souse = cv::imread(souse_path, cv::IMREAD_UNCHANGED);
        if (bg_souse.empty()) {
            std::cerr << "[警告] 素材スキップ: " << souse_path << std::endl;
            continue;
        }

        // D. 頭部を拡大 & 回転
        cv::Mat resized_head;
        cv::resize(combined_head, resized_head, cv::Size(), HEAD_SCALE, HEAD_SCALE, cv::INTER_LINEAR);
        cv::Mat rotated_head = rotateImage(resized_head, ANGLE_LIST[i]);

        // E. 配置位置の決定（OFFSET_X_LIST, OFFSET_Y を適用）
        int head_x = (bg_souse.cols - rotated_head.cols) / 2 + OFFSET_X_LIST[i];
        int head_y = (bg_souse.rows - rotated_head.rows) / 3 + OFFSET_Y;
        cv::Point head_pos(head_x, head_y);

        // F. 合成および画像保存
        cv::Mat final_result = overlayImage(bg_souse, rotated_head, head_pos);

        std::string out_path = OUTPUT_DIR + OUTPUT_FILES[i];
        if (cv::imwrite(out_path, final_result)) {
            std::cout << " -> 保存完了: " << out_path << " (パネル: " << PANEL_FILES[i] << ")" << std::endl;
        } else {
            std::cerr << "[エラー] 保存失敗: " << out_path << std::endl;
        }
    }

    std::cout << "=== すべての画像処理が正常に完了しました ===" << std::endl;
    return 0;
}