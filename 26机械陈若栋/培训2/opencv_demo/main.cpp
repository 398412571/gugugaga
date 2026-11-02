#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    std::cout << "--- 任务2：OpenCV测试 ---" << std::endl;
    
    // 读取当前目录下的 test.jpg 图片
    cv::Mat img = cv::imread("test.jpg");
    
    if (img.empty()) {
        std::cerr << "错误：无法读取图片，请确保当前目录下有 test.jpg 文件！" << std::endl;
        return -1;
    }

    // 显示图片
    cv::imshow("OpenCV Window", img);
    std::cout << "请在弹出的窗口中按键盘任意键关闭..." << std::endl;
    
    // 等待按键
    cv::waitKey(0);
    return 0;
}
