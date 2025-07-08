#include "LogFree.h"

int main()
{
    LogFree::Log("test");
    Sleep(50);
    LogFree::Log("testShow", LogLevel::ERRO, true);
    while (true)
    {
        LogFree::Log("test", LogLevel::WARN, true);
        Sleep(1000);
    }
}