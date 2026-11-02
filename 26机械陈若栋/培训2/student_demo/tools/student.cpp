#include "student.hpp"
Student::Student(std::string n, int i, double s) : name(n), id(i), score(s) {}
void Student::printInfo() const {
    std::cout << "学生姓名: " << name << "\n学号: " << id << "\n成绩: " << score << "\n" << std::endl;
}
void Student::modifyScore(double newScore) {
    score = newScore; std::cout << "成绩已修改为: " << score << std::endl;
}
