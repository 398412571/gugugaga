/**
 * 任务一：读取装甲板图片，分离 HSV 三个通道，并用 inRange 提取红色灯条掩膜。
 *
 * 运行（在仓库根目录）：
 *   ./build/task1_hsv_mask assets/armor.png
 *
 * 输出：
 *   results/hsv_h.png / hsv_s.png / hsv_v.png
 *   results/red_mask.png
 *
 * 依赖：OpenCV 4.x
 */
#include <opencv2/opencv.hpp>

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

int main(int argc, char** argv) {
    const std::string imagePath = (argc > 1) ? argv[1] : "assets/armor.png";

    MAKE_DIR("results");

    cv::Mat src = cv::imread(imagePath, cv::IMREAD_COLOR);
    if (src.empty()) {
        std::cerr << "Cannot read image: " << imagePath << std::endl;
        std::cerr << "Please run this program from the repository root, "
                     "or pass the image path as the first argument." << std::endl;
        return -1;
    }
    std::cout << "Image: " << imagePath << "  (" << src.cols << " x " << src.rows << ")" << std::endl;

    // 1. BGR -> HSV，并分离 H / S / V 三个通道。
    cv::Mat hsv;
    cv::cvtColor(src, hsv, cv::COLOR_BGR2HSV);
    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);
    const cv::Mat& hChannel = channels[0];
    const cv::Mat& sChannel = channels[1];
    const cv::Mat& vChannel = channels[2];

    // 2. 红色在 HSV 色环上跨越 0 度，需要两段阈值。
    //    OpenCV 中 H 的范围是 0~179，S/V 是 0~255。
    cv::Mat lowRed, highRed, mask;
    cv::inRange(hsv, cv::Scalar(0, 90, 60), cv::Scalar(12, 255, 255), lowRed);
    cv::inRange(hsv, cv::Scalar(168, 90, 60), cv::Scalar(180, 255, 255), highRed);
    cv::bitwise_or(lowRed, highRed, mask);

    // 3. 形态学去噪：闭运算连接灯条内部空洞，开运算去掉零散噪点。
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 2);
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

    // 4. 去掉面积很小的连通域，只保留两条灯条。
    cv::Mat labels, stats, centroids;
    int componentCount = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);
    cv::Mat cleanMask = cv::Mat::zeros(mask.size(), CV_8UC1);
    for (int i = 1; i < componentCount; ++i) {
        if (stats.at<int>(i, cv::CC_STAT_AREA) >= 100) {
            cleanMask.setTo(255, labels == i);
        }
    }
    mask = cleanMask;

    // 5. 保存结果；H/S/V 也保存一份，方便放进 README 或实验报告。
    bool ok = true;
    ok &= cv::imwrite("results/hsv_h.png", hChannel);
    ok &= cv::imwrite("results/hsv_s.png", sChannel);
    ok &= cv::imwrite("results/hsv_v.png", vChannel);
    ok &= cv::imwrite("results/red_mask.png", mask);
    if (!ok) {
        std::cerr << "Some files could not be written. Is the results/ directory writable?" << std::endl;
        return -1;
    }

    // 6. 分别弹出窗口展示。按任意键关闭全部窗口。
    cv::imshow("1. Original", src);
    cv::imshow("2. H channel", hChannel);
    cv::imshow("3. S channel", sChannel);
    cv::imshow("4. V channel", vChannel);
    cv::imshow("5. Red mask (inRange)", mask);
    std::cout << "Press any key in an OpenCV window to exit..." << std::endl;
    cv::waitKey(0);
    cv::destroyAllWindows();

    std::cout << "Saved: results/hsv_h.png, hsv_s.png, hsv_v.png, red_mask.png" << std::endl;
    return 0;
}
