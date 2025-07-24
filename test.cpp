#include "LogFree.h"

void fun()
{
    LogFree::LogCodeInfo("fun1", LogLevel::INFO, true);
    LogFree::LogCodeInfo("fun2");
    LogFree::LogCodeInfo("fun3", LogLevel::INFO);
    LogFree::LogCodeInfo("fun4", LogLevel::INFO, true);
}

int main()
{
    LogFree::LogCodeInfo("Code", LogLevel::ERRO, true);
    LogFree::Log("test");
    fun();
    Sleep(50);
    LogFree::Log("testShow", LogLevel::ERRO, true);
    LogFree::LogCodeInfo("info", LogLevel::ERRO, true);
    while (true)
    {
        LogFree::Log("test", LogLevel::WARN, true);
        Sleep(1000);
    }
}