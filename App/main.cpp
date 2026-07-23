#include "Application.h"
#include "Core/Common/VersionInfo.h"
#include <iostream>
#include <string_view>

static bool checkVersionArg(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--version" || arg == "-v") {
            std::cout << ht::VersionInfo::version_full << std::endl;
            return true;
        }
    }
    return false;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    if (checkVersionArg(__argc, __argv)) return 0;
    ht::Application app(__argc, __argv);
    return app.run();
}
#else
int main(int argc, char** argv) {
    if (checkVersionArg(argc, argv)) return 0;
    ht::Application app(argc, argv);
    return app.run();
}
#endif