#include <iostream>
#include "student.hpp"
int main() {
    Student stu("张三", 1001, 85.5); stu.printInfo();
    stu.modifyScore(95.0); stu.printInfo();
    return 0;
}
