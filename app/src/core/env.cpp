#include "env.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

constexpr const char * kEnvRootDir = "LIBGUI_ROOT_DIR";
constexpr const char * kDefaultRootDir = ".";

[[noreturn]] void envFatal(const char * message) {
    fprintf(stderr, "%s", message);
    std::exit(1);
}

fs::path executableDir() {
    std::error_code ec;
    const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (ec) {
        return {};
    }
    return fs::weakly_canonical(exe, ec).parent_path();
}

std::string canonicalPathString(const fs::path & path) {
    std::error_code ec;
    return fs::weakly_canonical(path, ec).string();
}

fs::path resolveRootDirectory() {
    const char * envValue = std::getenv(kEnvRootDir);
    const char * pathStr = (envValue && envValue[0] != '\0') ? envValue : kDefaultRootDir;
    const char * source = (envValue && envValue[0] != '\0') ? kEnvRootDir : "default";

    fs::path path(pathStr);
    if (path.is_relative()) {
        const fs::path exeDir = executableDir();
        if (exeDir.empty()) {
            envFatal("error: cannot resolve executable directory\n");
        }
        path = exeDir / path;
    }

    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) {
        fprintf(stderr,
                "error: %s is not a directory: %s (%s=%s)\n",
                kEnvRootDir,
                path.string().c_str(),
                source,
                pathStr);
        std::exit(1);
    }

    return fs::weakly_canonical(path, ec);
}

void validateDirectory(const fs::path & dir, const char * label) {
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        fprintf(stderr, "error: %s is not a directory: %s\n", label, dir.string().c_str());
        std::exit(1);
    }
}

void validateStaticRoot(const fs::path & dir) {
    validateDirectory(dir, "static");

    std::error_code ec;
    const fs::path index = dir / "index.html";
    if (!fs::is_regular_file(index, ec)) {
        fprintf(stderr, "error: index.html not found in %s\n", dir.string().c_str());
        std::exit(1);
    }
}

} // namespace

LibguiEnv::LibguiEnv() {
    const fs::path rootPath = resolveRootDirectory();
    const fs::path dataPath = rootPath / "data";
    const fs::path staticPath = rootPath / "static";

    validateDirectory(dataPath, "data");
    validateStaticRoot(staticPath);

    rootDir_ = canonicalPathString(rootPath);
    dataDir_ = canonicalPathString(dataPath);
    fontsDir_ = canonicalPathString(dataPath / "fonts");
    staticDir_ = canonicalPathString(staticPath);
}

LibguiEnv & LibguiEnv::instance() {
    static LibguiEnv env;
    return env;
}
