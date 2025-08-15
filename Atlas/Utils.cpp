#include "Utils.h"

void Utils::InitConsole()
{
    /* Code to open a console window */
    AllocConsole();
    FILE* Dummy;
    freopen_s(&Dummy, "CONOUT$", "w", stdout);
    freopen_s(&Dummy, "CONIN$", "r", stdin);
    SetConsoleTitleW(L"Atlas | Loading MapInfo...");
}

void Utils::InitLogger()
{
    if (std::filesystem::exists(FileName))
    {
        std::ofstream ofs(FileName, std::ofstream::trunc);
        ofs.close();
    }
    else
    {
        std::ofstream ofs(FileName);
        ofs.close();
    }
}

void Utils::LOG(const std::string& Category, const std::string& Message, const std::string& Info)
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf;
    localtime_s(&tmBuf, &t);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::ostringstream timestamp;
    timestamp << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S") << "." << std::setw(3) << std::setfill('0') << ms.count();
    std::ofstream ofs(Utils::FileName, std::ios_base::app);
    ofs << "[" << timestamp.str() << "]" << " [" << Category << "]" << " [" << Info << "]: " << Message << std::endl;
    printf("[%s] [%s] [%s]: %s\n", timestamp.str().c_str(), Category.c_str(), Info.c_str(), Message.c_str());
}
