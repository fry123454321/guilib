/*! \file imgui-ws.h
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#pragma once

#include <functional>
#include <vector>
#include <queue>
#include <string>
#include <memory>

class ImGuiWS {
    public:
        using TextureId = uint32_t;

        using THandler = std::function<void()>;
        using TPath = std::string;
        using TIdxs = std::vector<int>;
        using TGetter = std::function<std::string_view(const TIdxs & idxs)>;

        struct Texture {
            enum class Type : int32_t {
                Alpha8 = 0,
                Gray8  = 1,
                RGB24  = 2,
                RGBA32 = 3,
            };

            int revision = 0;
            std::vector<char> data;
        };

        struct Event {
            enum Type {
                Unknown = 0,
                Connected,
                Disconnected,
                MouseMove,
                MouseDown,
                MouseUp,
                MouseWheel,
                KeyPress,
                KeyDown,
                KeyUp,
                Resize,
                TakeControl,
                PasteFromBrowser,
            };

            Type type = Unknown;

            int32_t clientId = -1;

            float mouse_x = 0.0f;
            float mouse_y = 0.0f;

            float wheel_x = 0.0f;
            float wheel_y = 0.0f;

            int32_t mouse_but = 0;

            int32_t key = 0;

            int32_t client_width = 1920;
            int32_t client_height = 1080;
            float client_pixel_ratio = 1.0f;

            std::string ip;

            std::string clipboard_text;
            std::string input_text;
        };

        struct InitOptions {
            std::string hostListen = "0.0.0.0";
            std::string staticRoot;
            bool useSsl = false;
            std::string sslKey = "key.pem";
            std::string sslCert = "cert.pem";
        };

        ImGuiWS();
        ~ImGuiWS();

        bool init(int32_t port, std::string pathHttp, std::vector<std::string> resources);
        bool init(int32_t port, std::string pathHttp, std::vector<std::string> resources, const InitOptions & options);
        bool init(int32_t port, std::string pathHttp, std::vector<std::string> resources, THandler && connect_handler, THandler && disconnect_handler);
        bool init(int32_t port, std::string pathHttp, std::vector<std::string> resources, THandler && connect_handler, THandler && disconnect_handler, const InitOptions & options);
        bool setTexture(TextureId textureId, Texture::Type textureType, int32_t width, int32_t height, const char * data);
        bool setDrawData(const struct ImDrawData * drawData);
        bool addVar(const TPath & path, TGetter && getter);
        void addResource(const std::string & url, const std::string & content);


        int32_t nConnected() const;

        std::vector<std::string> varPaths() const;
        std::vector<char> sampleVar(const std::string & path, const std::vector<int> & idxs = {}) const;

        struct TextureInfo {
            TextureId id = 0;
            Texture::Type type = Texture::Type::Alpha8;
            int32_t width = 0;
            int32_t height = 0;
            int32_t revision = 0;
            std::size_t byteSize = 0;
            bool valid = false;
        };

        struct DrawDataEncodeInfo {
            double encodeMs = 0.0;
            std::size_t payloadBytes = 0;
        };

        TextureInfo textureInfo(TextureId textureId) const;
        DrawDataEncodeInfo lastDrawDataEncodeInfo() const;
        std::vector<std::string> embeddedResources() const;
        std::vector<std::string> httpResourceRoutes() const;
        std::string httpRoot() const;
        std::string staticRoot() const;

        std::deque<Event> takeEvents();

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
};
