#pragma once

#include <string>

class LibguiEnv {
    public:
        static LibguiEnv & instance();

        const std::string & rootDir() const { return rootDir_; }
        const std::string & dataDir() const { return dataDir_; }
        const std::string & fontsDir() const { return fontsDir_; }
        const std::string & staticDir() const { return staticDir_; }

        LibguiEnv(const LibguiEnv &) = delete;
        LibguiEnv & operator=(const LibguiEnv &) = delete;

    private:
        LibguiEnv();

        std::string rootDir_;
        std::string dataDir_;
        std::string fontsDir_;
        std::string staticDir_;
};
