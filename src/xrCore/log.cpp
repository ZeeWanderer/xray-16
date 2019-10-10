#include "stdafx.h"
#pragma hdrstop

#include <time.h>
#include "resource.h"
#include "log.h"
#include "xrCore/Threading/Lock.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/fmt/ostr.h"


BOOL LogExecCB = TRUE;
string_path logFName = "engine.log";
string_path log_file_name = "engine.log";
BOOL no_log = TRUE;
#ifdef CONFIG_PROFILE_LOCKS
Lock logCS(MUTEX_PROFILE_ID(log));
#else // CONFIG_PROFILE_LOCKS
Lock logCS;
#endif // CONFIG_PROFILE_LOCKS
xr_vector<xr_string> LogFile;
LogCallback LogCB = 0;

bool ForceFlushLog = false;
std::shared_ptr<spdlog::logger> LogWriter = nullptr;
size_t CachedLog = 0;

#ifdef WIN32
const bool b_is_debugger_attached = IsDebuggerPresent(); // TODO: linux
#endif // WIN32

inline void FlushLog()
{
    if (!no_log)
    {
        if (LogWriter)
            LogWriter->flush();
        CachedLog = 0;
    }
}

inline void AddOne(const char* split)
{
    logCS.Enter();

#ifdef WIN32
    if (b_is_debugger_attached)
    {
        OutputDebugString(split);
        OutputDebugString("\n");
    }
#else // !WIN32
    OutputDebugString(split);
    OutputDebugString("\n");
#endif // WIN32

    LogFile.emplace_back(split);

    // exec CallBack
    if (LogExecCB && LogCB)
        LogCB(split); 

    logCS.Leave();
}

void SplitLog(const char* s)
{
    int i, j;

    u32 length = xr_strlen(s);
#ifndef _EDITOR
    PSTR split = (PSTR)xr_alloca((length + 1) * sizeof(char));
#else
    PSTR split = (PSTR)alloca((length + 1) * sizeof(char));
#endif
    for (i = 0, j = 0; s[i] != 0; i++)
    {
        if (s[i] == '\n')
        {
            split[j] = 0; // end of line
            if (split[0] == 0)
            {
                split[0] = ' ';
                split[1] = 0;
            }
            AddOne(split);
            j = 0;
        }
        else
        {
            split[j++] = s[i];
        }
    }
    split[j] = 0;
    AddOne(split);
}

inline void Log(const char* s)
{
    if (LogWriter)
    {
        LogWriter->info(s);
        if (ForceFlushLog)
            LogWriter->flush();
    }
    SplitLog(s);
}

inline void __cdecl Msg(const char* format, ...)
{
    va_list mark;
    string2048 buf;
    va_start(mark, format);
    int sz = std::vsnprintf(buf, sizeof(buf) - 1, format, mark);
    buf[sizeof(buf) - 1] = 0;
    va_end(mark);
    if (sz)
        Log(buf);
}

inline void Log(const char* msg, const char* dop)
{
    if (!dop)
    {
        Log(msg);
        return;
    }

    fmt::memory_buffer buf;
    fmt::format_to(buf, "{} {}", msg, dop);

    Log(buf.data());
}

inline void Log(const char* msg, u32 dop)
{

    fmt::memory_buffer buf;
    fmt::format_to(buf, "{} {}", msg, dop);

    Log(buf.data());
}

inline void Log(const char* msg, u64 dop)
{
    fmt::memory_buffer buf;
    fmt::format_to(buf, "{} {}", msg, dop);

    Log(buf.data());
}

inline void Log(const char* msg, int dop)
{
    fmt::memory_buffer buf;
    fmt::format_to(buf, "{} {}", msg, dop);

    Log(buf.data());
}

inline void Log(const char* msg, float dop)
{
    fmt::memory_buffer buf;
    fmt::format_to(buf, "{} {}", msg, dop);

    Log(buf.data());
}

inline void Log(const char* msg, const Fvector& dop)
{
    fmt::memory_buffer buf;
    fmt::format_to(buf, "{} ({})", msg, dop);

    Log(buf.data());
}

inline void Log(const char* msg, const Fmatrix& dop)
{
    fmt::memory_buffer buf;
    fmt::format_to(buf, "{}:\n{}", msg, dop);

    Log(buf.data());
}

void LogWinErr(const char* msg, long err_code) { Msg("%s: %s", msg, xrDebug::ErrorToString(err_code)); }
LogCallback SetLogCB(const LogCallback& cb)
{
    LogCallback result = LogCB;
    LogCB = cb;
    return (result);
}

LPCSTR log_name() { return (log_file_name); }

void CreateLog(BOOL nl)
{

    no_log = nl;
    strconcat(sizeof(log_file_name), log_file_name, Core.ApplicationName, "_", Core.UserName, ".log");
    if (FS.path_exist("$logs$"))
        FS.update_path(logFName, "$logs$", log_file_name);
    if (!no_log)
    {
        // Alun: Backup existing log
        xr_string backup_logFName = EFS.ChangeFileExt(logFName, ".bkp");
        FS.file_rename(logFName, backup_logFName.c_str(), true);
        //-Alun

        LogWriter = spdlog::basic_logger_mt<spdlog::async_factory>("engine", logFName);
        
        if (LogWriter == nullptr)
        {
#if defined(WINDOWS)
            MessageBox(NULL, "Can't create log file.", "Error", MB_ICONERROR);
#endif
            abort();
        }
        
#ifdef USE_LOG_TIMING
        LogWriter->set_pattern("[%H:%M:%S %e] %v");
#else
        LogWriter->set_pattern("%v");
#endif

        for (u32 it = 0; it < LogFile.size(); it++)
        {
            LPCSTR s = LogFile[it].c_str();

            LogWriter->info(s ? s : "");
        }
        LogWriter->flush();
    }

    if (strstr(Core.Params, "-force_flushlog"))
        ForceFlushLog = true;
}

void CloseLog(void)
{
    FlushLog();
    spdlog::drop_all();
    spdlog::shutdown();
    
    LogFile.clear();
}
