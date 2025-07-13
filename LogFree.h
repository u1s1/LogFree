#ifndef LOGFREE_H
#define LOGFREE_H

#include <iostream>
#include <string>
#include <queue>
#include <future>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <windows.h>
#include "ThreadPool.h"

enum LogLevel
{
    INFO,
    WARN,
    ERRO
};

struct LogTime
{
    std::tm localTime;
    int milliseconds;
    LogTime()
    {
        ResetTime();
    }
    ~LogTime(){}
    //重置当前时间
    void ResetTime()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        localTime = *std::localtime(&t);
        auto tNow = std::chrono::system_clock::now();
        auto tMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(tNow.time_since_epoch());
        auto tSeconds = std::chrono::duration_cast<std::chrono::seconds>(tNow.time_since_epoch());
        auto ms = tMilliseconds - tSeconds;
        milliseconds = ms.count();
    }
    //是否为同一天
    bool IsSameDay(const LogTime& otherTime)
    {
        return otherTime.localTime.tm_year - localTime.tm_year == 0 &&
               otherTime.localTime.tm_mon - localTime.tm_mon == 0 &&
               otherTime.localTime.tm_mday - localTime.tm_mday == 0;
    }
    //当前存的时间和现在实际时间是否为同一天
    bool IsToday()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm nowTime = *std::localtime(&t);
        return nowTime.tm_year - localTime.tm_year == 0 &&
               nowTime.tm_mon - localTime.tm_mon == 0 &&
               nowTime.tm_mday - localTime.tm_mday == 0;
    }
};

struct LogInfo
{
    char *logBuff;
    int logBuffSize;
    LogLevel level;
    bool showInCmd;
    LogTime logTimeStamp;

    LogInfo()
    {
        logBuff = nullptr;
        logBuffSize = 0;
        level = LogLevel::INFO;
        showInCmd = false;
        logTimeStamp.ResetTime();
    }
    //深克隆
    LogInfo(const LogInfo& logger)
    {
        logBuff = new char[logger.logBuffSize];
        logBuffSize = logger.logBuffSize;
        memcpy(logBuff, logger.logBuff, logBuffSize);
        level = logger.level;
        showInCmd = logger.showInCmd;
        logTimeStamp = logger.logTimeStamp;
    }
    LogInfo(const std::string& strLog, LogLevel logLevel = LogLevel::INFO, bool logShowInCmd = false)
    {
        logBuff = new char[strLog.size()];
        logBuffSize = strLog.size();
        memcpy(logBuff, strLog.data(), logBuffSize);
        level = logLevel;
        showInCmd = logShowInCmd;
        logTimeStamp.ResetTime();
    }
    ~LogInfo()
    {
        if (logBuff != nullptr)
        {
            delete[] logBuff;
        }
    }
};

class LogFree
{
public:
    static int Log(const std::string& strLog, LogLevel level = LogLevel::INFO, bool showInCmd = false);

private:
    static LogFree *getInstance()
    {
        static LogFree log;
        return &log;
    }

    LogFree()
    {
        m_fileLog = nullptr;
        ResetLogFile();
        m_bRunning = true;
        std::thread th(&LogFree::LogThread, this);
        th.detach();
    }
    ~LogFree()
    {
        {
            std::unique_lock<std::mutex> lock(m_mutexLog);
            m_bRunning = false;
        }
        m_Condition.notify_all();
        {
            std::unique_lock<std::mutex> lockQueue(m_mutexLog);
            std::queue<std::unique_ptr<LogInfo>> queueTemp;
            m_queLogInfo.swap(queueTemp);
        }
        if (m_fileLog != nullptr)
        {
            m_fileLog->close();
            delete m_fileLog;
            m_fileLog = nullptr;
        }
    }
    //创建一个今日日期的log文件
    void ResetLogFile();
    //log处理线程
    void LogThread();
    //log数据写入
    void LogHandle(std::unique_ptr<LogInfo> logger);

    std::queue<std::unique_ptr<LogInfo>> m_queLogInfo;
    std::mutex m_mutexLog;
    std::condition_variable m_Condition; // 线程等待锁
    bool m_bRunning;
    std::ofstream* m_fileLog;
    LogTime m_timeToday;
    ThreadPool m_logThreadPool;
};

inline int LogFree::Log(const std::string& strLog, LogLevel level, bool showInCmd)
{
    std::unique_ptr<LogInfo> logPtr = std::make_unique<LogInfo>(strLog, level, showInCmd);
    //限制锁的作用域，不影响notify
    {
        std::unique_lock<std::mutex> lock(getInstance()->m_mutexLog);
        getInstance()->m_queLogInfo.push(std::move(logPtr));
    }
    getInstance()->m_Condition.notify_all();
    return 0;
}

inline void LogFree::LogThread()
{
    std::unique_ptr<LogInfo> logger;
    while (m_bRunning)
    {
        logger.release();
        //线程锁范围限定
        {
            std::unique_lock<std::mutex> lock(m_mutexLog);
            m_Condition.wait_for(lock, std::chrono::milliseconds(5), [this]() { return !m_bRunning || m_queLogInfo.size() != 0; });
            if (!m_bRunning)
            {
                return;
            }
            if (m_queLogInfo.size() == 0)
            {
                continue;
            }
            logger = std::move(m_queLogInfo.front());
            m_queLogInfo.pop();
        }
        if (logger.get() == nullptr)
        {
            continue;
        }
        LogHandle(std::move(logger));
    }
}

inline void LogFree::LogHandle(std::unique_ptr<LogInfo> logger)
{
    if (!logger.get()->logTimeStamp.IsToday())
    {
        ResetLogFile();
    }
    std::string strLog = "";
    std::stringstream ss;
    ss << std::put_time(&logger.get()->logTimeStamp.localTime, "%Y-%m-%d %H:%M:%S")
       << "." << std::setfill('0') << std::setw(3) << logger.get()->logTimeStamp.milliseconds;
    strLog = ss.str();
    switch (logger.get()->level)
    {
    case LogLevel::INFO:
        strLog += " INFO:";
        break;
    case LogLevel::WARN:
        strLog += " WARN:";
        break;
    case LogLevel::ERRO:
        strLog += " ERRO:";
        break;
    default:
        strLog += " INFO:";
        break;
    }
    strLog.append(logger.get()->logBuff, logger.get()->logBuffSize);
    strLog += "\n";
    //写到文件里
    m_fileLog->write(strLog.data(), strLog.size());
    //显示到控制台
    if (logger.get()->showInCmd)
    {
        std::cout << strLog;
    }
}

inline void LogFree::ResetLogFile()
{
    m_timeToday.ResetTime();
    std::stringstream ss;
	ss << std::put_time(&m_timeToday.localTime, "%Y-%m-%d");
	std::string strTime = ss.str();
    strTime = "log-" + strTime + ".log";
    if (m_fileLog != nullptr)
    {
        m_fileLog->close();
        delete m_fileLog;
        m_fileLog = nullptr;
    }
    //获取执行文件路径
    char exeFullPath[256]; // Full path
    std::string strPath = "";
    //获取带有可执行文件名路径
    GetModuleFileName(NULL, exeFullPath, 256);
    strPath=(std::string)exeFullPath;
    int pos = strPath.find_last_of('\\', strPath.length()) + 1;
    //生成log绝对路径
    strPath = strPath.substr(0, pos) + strTime;

    m_fileLog = new std::ofstream(strPath.data(), std::ios::out | std::ios::app);
}

#endif