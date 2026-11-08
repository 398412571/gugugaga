/**
 * 任务二：识别视频中装甲板的红色灯条，用最大外接矩形框住灯条，
 *         显示四个角点，并连接装甲板的四个角点。
 *
 * 运行（在仓库根目录）：
 *   ./build/task2_armor_detection data/armor.mp4
 *   ./build/task2_armor_detection data/armor.mp4 results/armor_detection_raw.mp4 --show
 *
 * 依赖：OpenCV 4.x
 */
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MAKE_DIR(path) mkdir(path, 0755)
#endif

namespace {

const cv::Scalar kGreen(0, 255, 0);
const cv::Scalar kYellow(0, 255, 255);
const cv::Scalar kRed(0, 0, 255);
const cv::Scalar kWhite(255, 255, 255);

struct CornerTips {
    cv::Point tl;
    cv::Point tr;
    cv::Point br;
    cv::Point bl;
};

// 从红色灯条轮廓上取四个最外侧尖端（不是黄色外接矩形的顶角）。
CornerTips findCornerTips(const std::vector<cv::Point>& contour) {
    double minSum = 1e18, maxSum = -1e18;
    double minDiff = 1e18, maxDiff = -1e18;
    CornerTips tips;
    for (const auto& p : contour) {
        const double sum = static_cast<double>(p.x) + p.y;
        const double diff = static_cast<double>(p.x) - p.y;
        if (sum < minSum) { minSum = sum; tips.tl = p; }
        if (sum > maxSum) { maxSum = sum; tips.br = p; }
        if (diff < minDiff) { minDiff = diff; tips.bl = p; }
        if (diff > maxDiff) { maxDiff = diff; tips.tr = p; }
    }
    return tips;
}

struct Bar {
    std::vector<cv::Point> contour;
    cv::Rect box;
    cv::RotatedRect minRect;
    cv::Point2f center;
    double area = 0.0;
    double angle = 0.0;
};

double longAxisAngle(const cv::RotatedRect& rect) {
    // OpenCV 的 angle 在不同版本里含义略有差异，这里统一成长边的方向角。
    double angle = (rect.size.width >= rect.size.height) ? rect.angle : rect.angle + 90.0;
    angle = std::fmod(angle, 180.0);
    if (angle < 0.0) {
        angle += 180.0;
    }
    return angle;
}

cv::Mat buildRedMask(const cv::Mat& bgr) {
    cv::Mat hsv;
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);

    // 红色在色环上跨 0 度，所以用两段 inRange。
    cv::Mat lowRed, highRed, mask;
    cv::inRange(hsv, cv::Scalar(0, 90, 60), cv::Scalar(12, 255, 255), lowRed);
    cv::inRange(hsv, cv::Scalar(168, 90, 60), cv::Scalar(180, 255, 255), highRed);
    cv::bitwise_or(lowRed, highRed, mask);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 2);
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    return mask;
}

std::vector<Bar> findBars(const cv::Mat& mask) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<Bar> bars;
    for (const auto& contour : contours) {
        const double area = cv::contourArea(contour);
        if (area < 250.0) {
            continue;
        }

        const cv::RotatedRect rect = cv::minAreaRect(contour);
        const double longSide = std::max(rect.size.width, rect.size.height);
        const double shortSide = std::min(rect.size.width, rect.size.height);
        if (shortSide < 8.0 || longSide < 35.0 || longSide / std::max(shortSide, 1.0) < 1.6) {
            continue;
        }

        Bar bar;
        bar.contour = contour;
        bar.box = cv::boundingRect(contour);   // 最大外接矩形（轴对齐）
        bar.minRect = rect;
        bar.center = rect.center;
        bar.area = area;
        bar.angle = longAxisAngle(rect);
        bars.push_back(bar);
    }

    std::sort(bars.begin(), bars.end(), [](const Bar& a, const Bar& b) {
        return a.area > b.area;
    });
    return bars;
}

bool findBarPair(const std::vector<Bar>& bars, Bar& left, Bar& right) {
    double bestScore = -1e18;
    bool found = false;

    for (size_t i = 0; i < bars.size(); ++i) {
        for (size_t j = i + 1; j < bars.size(); ++j) {
            const Bar& a = bars[i];
            const Bar& b = bars[j];

            const double dx = std::abs(a.center.x - b.center.x);
            const double dy = std::abs(a.center.y - b.center.y);
            const double maxHeight = std::max(
                std::max(a.minRect.size.width, a.minRect.size.height),
                std::max(b.minRect.size.width, b.minRect.size.height));

            double dAngle = std::abs(a.angle - b.angle);
            dAngle = std::min(dAngle, 180.0 - dAngle);

            // 两条灯条必须横向分开、高度接近、方向接近。
            if (dx < 120.0 || dx < 1.1 * std::max(a.box.width, b.box.width)) {
                continue;
            }
            if (dy > 0.55 * maxHeight + 25.0) {
                continue;
            }
            if (dAngle > 18.0) {
                continue;
            }

            const double score = a.area + b.area - 3.0 * dy - 2.0 * dAngle;
            if (!found || score > bestScore) {
                found = true;
                bestScore = score;
                left = a;
                right = b;
            }
        }
    }

    if (found && left.center.x > right.center.x) {
        std::swap(left, right);
    }
    return found;
}

void fillRectAlpha(cv::Mat& image, const cv::Rect& rect, const cv::Scalar& color, double alpha) {
    const cv::Rect clipped = rect & cv::Rect(0, 0, image.cols, image.rows);
    if (clipped.area() <= 0) {
        return;
    }
    cv::Mat roi = image(clipped);
    cv::Mat overlay(roi.size(), roi.type(), color);
    cv::Mat blended;
    cv::addWeighted(overlay, alpha, roi, 1.0 - alpha, 0.0, blended);
    blended.copyTo(roi);
}

void putTextOutlined(cv::Mat& image, const std::string& text, const cv::Point& org,
                     double scale, const cv::Scalar& color, int thickness = 1) {
    cv::putText(image, text, org, cv::FONT_HERSHEY_SIMPLEX, scale,
                cv::Scalar(0, 0, 0), thickness + 3, cv::LINE_AA);
    cv::putText(image, text, org, cv::FONT_HERSHEY_SIMPLEX, scale,
                color, thickness, cv::LINE_AA);
}

void drawHud(cv::Mat& image, int frameIndex, double fps) {
    const cv::Rect panel(18, 18, 350, 118);
    fillRectAlpha(image, panel, cv::Scalar(10, 12, 16), 0.62);
    cv::rectangle(image, cv::Rect(18, 18, 5, 118), kGreen, cv::FILLED);
    cv::circle(image, cv::Point(44, 44), 6, kGreen, cv::FILLED, cv::LINE_AA);

    putTextOutlined(image, "ARMOR PLATE TRACKING", cv::Point(60, 50), 0.62, kWhite, 1);
    putTextOutlined(image, "LIGHT BARS 2/2    CORNERS 4", cv::Point(40, 84), 0.48, kGreen, 1);

    char line[128];
    std::snprintf(line, sizeof(line), "FRAME %04d    %.0f FPS", frameIndex, fps);
    putTextOutlined(image, line, cv::Point(40, 117), 0.48, cv::Scalar(190, 196, 205), 1);
}

void drawLegend(cv::Mat& image) {
    const int x = 18;
    const int y = std::max(20, image.rows - 108);
    const cv::Rect panel(x, y, 350, 90);
    fillRectAlpha(image, panel, cv::Scalar(10, 12, 16), 0.62);

    cv::rectangle(image, cv::Rect(x + 12, y + 14, 30, 6), kYellow, cv::FILLED);
    putTextOutlined(image, "light-bar max bounding rect", cv::Point(x + 52, y + 23), 0.42, kWhite, 1);

    cv::rectangle(image, cv::Rect(x + 12, y + 42, 30, 9), kGreen, cv::FILLED);
    putTextOutlined(image, "P1-P4 tip connection", cv::Point(x + 52, y + 51), 0.42, kWhite, 1);

    cv::circle(image, cv::Point(x + 27, y + 70), 5, kGreen, cv::FILLED, cv::LINE_AA);
    cv::circle(image, cv::Point(x + 27, y + 70), 7, kWhite, 1, cv::LINE_AA);
    putTextOutlined(image, "bar tip corners", cv::Point(x + 52, y + 78), 0.42, kWhite, 1);
}

void annotateFrame(const cv::Mat& frame, const Bar* left, const Bar* right,
                   int frameIndex, double fps, cv::Mat& output) {
    output = frame.clone();

    if (left == nullptr || right == nullptr) {
        putTextOutlined(output, "LIGHT BARS NOT FOUND", cv::Point(30, 60), 1.0, kRed, 2);
        drawHud(output, frameIndex, fps);
        return;
    }

    // 两条灯条：画最大外接矩形（黄色），不画矩形角点。
    const Bar* barList[2] = {left, right};
    for (const Bar* bar : barList) {
        cv::rectangle(output, bar->box, kYellow, 2, cv::LINE_AA);
    }

    // 装甲板：P1~P4 取红色灯条轮廓的四个最外侧尖端，
    // 不是黄色外接矩形的顶角；大四边形由这四个点依次连接。
    const CornerTips lt = findCornerTips(left->contour);
    const CornerTips rt = findCornerTips(right->contour);
    const cv::Point armor[4] = {
        lt.tl,   // P1 左灯条左上尖端
        rt.tr,   // P2 右灯条右上尖端
        rt.br,   // P3 右灯条右下尖端
        lt.bl    // P4 左灯条左下尖端
    };
    const std::vector<std::vector<cv::Point>> armorContours = {
        std::vector<cv::Point>(armor, armor + 4)
    };
    cv::polylines(output, armorContours, true, kGreen, 3, cv::LINE_AA);

    const char* names[4] = {"P1", "P2", "P3", "P4"};
    const cv::Point offsets[4] = {
        cv::Point(-52, -14), cv::Point(12, -14),
        cv::Point(12, 30), cv::Point(-52, 30)
    };
    for (int i = 0; i < 4; ++i) {
        cv::circle(output, armor[i], 11, cv::Scalar(0, 0, 0), cv::FILLED, cv::LINE_AA);
        cv::circle(output, armor[i], 9, kGreen, cv::FILLED, cv::LINE_AA);
        cv::circle(output, armor[i], 9, kWhite, 2, cv::LINE_AA);

        int lx = armor[i].x + offsets[i].x;
        int ly = armor[i].y + offsets[i].y;
        lx = std::max(6, std::min(lx, frame.cols - 44));
        ly = std::max(24, std::min(ly, frame.rows - 8));
        putTextOutlined(output, names[i], cv::Point(lx, ly), 0.62, kWhite, 2);
    }

    drawHud(output, frameIndex, fps);
    drawLegend(output);
}

}  // namespace

int main(int argc, char** argv) {
    std::string videoPath = "data/armor.mp4";
    std::string outputPath = "results/armor_detection_raw.mp4";
    bool show = false;

    if (argc > 1 && std::string(argv[1]) != "--show") {
        videoPath = argv[1];
    }
    if (argc > 2 && std::string(argv[2]) != "--show") {
        outputPath = argv[2];
    }
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--show") {
            show = true;
        }
    }

    MAKE_DIR("results");
    MAKE_DIR("results/samples");

    cv::VideoCapture capture(videoPath);
    if (!capture.isOpened()) {
        std::cerr << "Cannot open video: " << videoPath << std::endl;
        std::cerr << "Please run this program from the repository root, "
                     "or pass the video path as the first argument." << std::endl;
        return -1;
    }

    double fps = capture.get(cv::CAP_PROP_FPS);
    if (fps <= 1.0) {
        fps = 30.0;
    }
    const int width = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));
    const int height = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    const int totalFrames = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_COUNT));

    cv::VideoWriter writer;
    writer.open(outputPath, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps,
                cv::Size(width, height));
    if (!writer.isOpened()) {
        writer.open(outputPath, cv::VideoWriter::fourcc('a', 'v', 'c', '1'), fps,
                    cv::Size(width, height));
    }
    if (!writer.isOpened()) {
        std::cerr << "Cannot open VideoWriter for: " << outputPath << std::endl;
        return -1;
    }

    std::cout << "Video: " << videoPath << "  (" << width << " x " << height
              << ", " << fps << " fps, " << totalFrames << " frames)" << std::endl;

    int frameIndex = 0;
    int detectedFrames = 0;
    const int64_t startTick = static_cast<int64_t>(cv::getTickCount());

    cv::Mat frame;
    while (capture.read(frame)) {
        if (frame.empty()) {
            break;
        }

        const cv::Mat mask = buildRedMask(frame);
        const std::vector<Bar> bars = findBars(mask);

        Bar left, right;
        const bool found = findBarPair(bars, left, right);
        if (found) {
            ++detectedFrames;
        }

        cv::Mat annotated;
        annotateFrame(frame, found ? &left : nullptr, found ? &right : nullptr,
                      frameIndex, fps, annotated);
        writer.write(annotated);

        if (frameIndex % 90 == 0) {
            char samplePath[256];
            std::snprintf(samplePath, sizeof(samplePath), "results/samples/t%02d.jpg",
                          frameIndex / 30);
            std::vector<int> jpegParams = {cv::IMWRITE_JPEG_QUALITY, 94};
            cv::imwrite(samplePath, annotated, jpegParams);
        }

        if (show) {
            cv::imshow("Armor plate detection", annotated);
            const int key = cv::waitKey(1);
            if (key == 27) {
                break;
            }
        }

        if (frameIndex % 60 == 0) {
            std::cout << "\rProcessing frame " << frameIndex << " / " << totalFrames << std::flush;
        }
        ++frameIndex;
    }

    capture.release();
    writer.release();
    cv::destroyAllWindows();

    const double elapsedMs = (cv::getTickCount() - startTick) * 1000.0 / cv::getTickFrequency();
    std::cout << "\nDone. " << frameIndex << " frames, " << detectedFrames
              << " frames with 2 bars (" << (100.0 * detectedFrames / std::max(frameIndex, 1))
              << "%), " << elapsedMs << " ms." << std::endl;
    std::cout << "Saved video: " << outputPath << std::endl;
    return 0;
}


