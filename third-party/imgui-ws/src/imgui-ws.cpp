/*! \file imgui-ws.cpp
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#include "imgui-ws/imgui-ws.h"
#include "imgui-ws/imgui-draw-data-compressor.h"

#include "common.h"

#include "incppect/incppect.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <optional>
#include <thread>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>

namespace {

bool parseClipboardText(std::string_view data, std::string & outText) {
    const std::string s(data);
    const auto sp = s.find(' ');
    if (sp == std::string::npos) {
        return false;
    }

    const auto sp2 = s.find(' ', sp + 1);
    if (sp2 == std::string::npos) {
        return false;
    }

    const int len = std::atoi(s.c_str() + sp + 1);
    if (len > 0 && sp2 + 1 + (size_t) len <= s.size()) {
        outText = s.substr(sp2 + 1, (size_t) len);
    } else if (sp2 + 1 < s.size()) {
        outText = s.substr(sp2 + 1);
    } else {
        outText.clear();
    }

    return true;
}

}

struct ImGuiWS::Impl {
    struct Events {
        std::deque<Event> data;

        std::mutex mutex;
        std::condition_variable cv;

        void push(Event && event) {
            std::lock_guard<std::mutex> lock(mutex);
            data.push_back(std::move(event));
            cv.notify_one();
        }
    };

    struct Data {
        std::map<int, TextureId> textureIdMap;
        std::map<TextureId, Texture> textures;

        ImDrawDataCompressor::Interface::DrawLists drawLists;
        ImDrawDataCompressor::Interface::DrawListsDiff drawListsDiff;
    };

    Impl() : compressorDrawData(new ImDrawDataCompressor::XorRlePerDrawListWithVtxOffset()) {}

    std::atomic<int32_t> nConnected = 0;

    std::thread worker;
    mutable std::shared_mutex mutex;

    Data dataWrite;
    Data dataRead;

    Events events;

    Incppect<false> incppPlain;
#ifdef IMGUI_WS_HAVE_SSL
    std::optional<Incppect<true>> incppSsl;
    bool useSsl = false;
#endif

    THandler handlerConnect;
    THandler handlerDisconnect;

    std::unique_ptr<ImDrawDataCompressor::Interface> compressorDrawData;

    double lastEncodeMs = 0.0;
    std::size_t lastEncodePayloadBytes = 0;

    template<typename F>
    decltype(auto) dispatch(F && f) {
#ifdef IMGUI_WS_HAVE_SSL
        if (useSsl) {
            return f(*incppSsl);
        }
#endif
        return f(incppPlain);
    }

    template<typename F>
    decltype(auto) dispatch(F && f) const {
#ifdef IMGUI_WS_HAVE_SSL
        if (useSsl) {
            return f(*incppSsl);
        }
#endif
        return f(incppPlain);
    }

    template<bool SSL>
    bool setupServer(
            Incppect<SSL> & incpp,
            int32_t port,
            std::string pathHttp,
            std::vector<std::string> resources,
            const ImGuiWS::InitOptions & options);
};

template<bool SSL>
bool ImGuiWS::Impl::setupServer(
        Incppect<SSL> & incpp,
        int32_t port,
        std::string pathHttp,
        std::vector<std::string> resources,
        const ImGuiWS::InitOptions & options) {
    using IC = Incppect<SSL>;

    incpp.var("my_id[%d]", [](const auto & idxs) {
        static int32_t id;
        id = idxs[0];
        return IC::view(id);
    });

    incpp.var("imgui.n_textures", [this](const auto & ) {
        std::shared_lock lock(mutex);

        return IC::view(dataRead.textures.size());
    });

    incpp.var("imgui.texture_id[%d]", [this](const auto & idxs) {
        std::shared_lock lock(mutex);

        if (dataRead.textureIdMap.find(idxs[0]) == dataRead.textureIdMap.end()) {
            return std::string_view { };
        }

        return IC::view(dataRead.textureIdMap[idxs[0]]);
    });

    incpp.var("imgui.texture_revision[%d]", [this](const auto & idxs) {
        std::shared_lock lock(mutex);

        if (dataRead.textures.find(idxs[0]) == dataRead.textures.end()) {
            return std::string_view { };
        }

        return IC::view(dataRead.textures[idxs[0]].revision);
    });

    incpp.var("imgui.texture_data[%d]", [this](const auto & idxs) {
        static std::vector<char> data;
        {
            std::shared_lock lock(mutex);
            if (dataRead.textures.find(idxs[0]) == dataRead.textures.end()) {
                return std::string_view { 0, 0 };
            }
            data.clear();
            std::copy(dataRead.textures[idxs[0]].data.data(),
                      dataRead.textures[idxs[0]].data.data() + dataRead.textures[idxs[0]].data.size(),
                      std::back_inserter(data));
        }

        return std::string_view { data.data(), data.size() };
    });

    incpp.var("imgui.n_draw_lists", [this](const auto & ) {
        std::shared_lock lock(mutex);

        return IC::view(dataRead.drawLists.size());
    });

    incpp.var("imgui.draw_list[%d]", [this](const auto & idxs) {
        static thread_local std::vector<char> data;
        {
            std::shared_lock lock(mutex);

            if (idxs[0] >= (int) dataRead.drawLists.size()) {
                return std::string_view {};
            }

            const auto & list = dataRead.drawLists[idxs[0]];
            data.assign(list.begin(), list.end());
        }

        return std::string_view { data.data(), data.size() };
    });

    incpp.handler([this](int clientId, typename IC::EventType etype, std::string_view data) {
        ImGuiWS::Event event;

        event.clientId = clientId;

        switch (etype) {
            case IC::Connect:
                {
                    ++nConnected;
                    event.type = ImGuiWS::Event::Connected;
                    std::stringstream ss;
                    { int a = data[0]; if (a < 0) a += 256; ss << a << "."; }
                    { int a = data[1]; if (a < 0) a += 256; ss << a << "."; }
                    { int a = data[2]; if (a < 0) a += 256; ss << a << "."; }
                    { int a = data[3]; if (a < 0) a += 256; ss << a; }
                    event.ip = ss.str();
                    if (handlerConnect) {
                        handlerConnect();
                    }
                }
                break;
            case IC::Disconnect:
                {
                    --nConnected;
                    event.type = ImGuiWS::Event::Disconnected;
                    if (handlerDisconnect) {
                        handlerDisconnect();
                    }
                }
                break;
            case IC::Custom:
                {
                    std::stringstream ss;
                    ss << data;

                    int type = -1;
                    ss >> type;

                    switch (type) {
                        case 0:
                            event.type = ImGuiWS::Event::MouseMove;
                            ss >> event.mouse_x >> event.mouse_y;
                            break;
                        case 1:
                            event.type = ImGuiWS::Event::MouseDown;
                            ss >> event.mouse_but >> event.mouse_x >> event.mouse_y;
                            break;
                        case 2:
                            event.type = ImGuiWS::Event::MouseUp;
                            ss >> event.mouse_but >> event.mouse_x >> event.mouse_y;
                            break;
                        case 3:
                            event.type = ImGuiWS::Event::MouseWheel;
                            ss >> event.wheel_x >> event.wheel_y;
                            break;
                        case 4:
                            event.type = ImGuiWS::Event::KeyPress;
                            if (!::parseClipboardText(data, event.input_text)) {
                                ss >> event.key;
                                if (event.key > 0 && event.key < 256) {
                                    event.input_text.assign(1, static_cast<char>(event.key));
                                }
                            }
                            break;
                        case 5:
                            event.type = ImGuiWS::Event::KeyDown;
                            ss >> event.key;
                            break;
                        case 6:
                            event.type = ImGuiWS::Event::KeyUp;
                            ss >> event.key;
                            break;
                        case 7:
                            event.type = ImGuiWS::Event::Resize;
                            ss >> event.client_width >> event.client_height;
                            if (!(ss >> event.client_pixel_ratio)) {
                                event.client_pixel_ratio = 1.0f;
                            }
                            break;
                        case 8:
                            event.type = ImGuiWS::Event::TakeControl;
                            break;
                        case 9:
                            event.type = ImGuiWS::Event::PasteFromBrowser;
                            ::parseClipboardText(data, event.clipboard_text);
                            break;
                        default:
                            printf("Unknown input received from client: id = %d, type = %d\n", clientId, type);
                            return;
                    };
                }
                break;
        };

        events.push(std::move(event));
    });

    resources.push_back("imgui-ws.js");
    incpp.setResource("/imgui-ws.js", kImGuiWS_js);

    typename IC::Parameters parameters;
    parameters.hostListen = options.hostListen;
    parameters.portListen = port;
    parameters.maxPayloadLength_bytes = 1024*1024;
    parameters.tLastRequestTimeout_ms = -1;
    parameters.httpRoot = std::move(pathHttp);
    parameters.staticRoot = options.staticRoot;
    parameters.resources = std::move(resources);
    parameters.sslKey = options.sslKey;
    parameters.sslCert = options.sslCert;
    worker = incpp.runAsync(parameters);

    return worker.joinable();
}

ImGuiWS::ImGuiWS() : m_impl(new Impl()) {
}

ImGuiWS::~ImGuiWS() {
    m_impl->dispatch([](auto & incpp) { incpp.stop(); });
    if (m_impl->worker.joinable()) {
        m_impl->worker.join();
    }
}

bool ImGuiWS::addVar(const TPath & path, TGetter && getter) {
    return m_impl->dispatch([&](auto & incpp) {
        return incpp.var(path, std::move(getter));
    });
}

void ImGuiWS::addResource(const std::string & url, const std::string & content) {
    m_impl->dispatch([&](auto & incpp) {
        incpp.setResource(url, content);
    });
}

bool ImGuiWS::init(int32_t port, std::string pathHttp, std::vector<std::string> resources) {
    return init(port, std::move(pathHttp), std::move(resources), InitOptions());
}

bool ImGuiWS::init(int32_t port, std::string pathHttp, std::vector<std::string> resources, const InitOptions & options) {
    if (options.useSsl) {
#ifdef IMGUI_WS_HAVE_SSL
        m_impl->useSsl = true;
        m_impl->incppSsl.emplace();
        return m_impl->setupServer(
            *m_impl->incppSsl,
            port,
            std::move(pathHttp),
            std::move(resources),
            options);
#else
        fprintf(stderr, "[imgui-ws] --ssl requested but WSS support was not compiled in (rebuild with -DLIBGUI_BUILD_SSL=ON)\n");
        return false;
#endif
    }

    return m_impl->setupServer(
        m_impl->incppPlain,
        port,
        std::move(pathHttp),
        std::move(resources),
        options);
}

bool ImGuiWS::setTexture(TextureId textureId, Texture::Type textureType, int32_t width, int32_t height, const char * data) {
    int bpp = 1; // bytes per pixel
    switch (textureType) {
        case Texture::Type::Alpha8: bpp = 1; break;
        case Texture::Type::Gray8:  bpp = 1; break;
        case Texture::Type::RGB24:  bpp = 3; break;
        case Texture::Type::RGBA32: bpp = 4; break;
    };

    if (m_impl->dataWrite.textures.find(textureId) == m_impl->dataWrite.textures.end()) {
        m_impl->dataWrite.textures[textureId].revision = 0;
        m_impl->dataWrite.textures[textureId].data.clear();
        m_impl->dataWrite.textureIdMap.clear();

        int idx = 0;
        for (const auto & t : m_impl->dataWrite.textures) {
            m_impl->dataWrite.textureIdMap[idx++] = t.first;
        }
    }

    m_impl->dataWrite.textures[textureId].revision++;
    m_impl->dataWrite.textures[textureId].data.resize(sizeof(TextureId) + sizeof(Texture::Type) + 3*sizeof(int32_t) + bpp*width*height);

    int revision = m_impl->dataWrite.textures[textureId].revision;

    size_t offset = 0;
    std::memcpy(m_impl->dataWrite.textures[textureId].data.data() + offset, &textureId, sizeof(textureId)); offset += sizeof(textureId);
    std::memcpy(m_impl->dataWrite.textures[textureId].data.data() + offset, &textureType, sizeof(textureType)); offset += sizeof(textureType);
    std::memcpy(m_impl->dataWrite.textures[textureId].data.data() + offset, &width, sizeof(width)); offset += sizeof(width);
    std::memcpy(m_impl->dataWrite.textures[textureId].data.data() + offset, &height, sizeof(height)); offset += sizeof(height);
    std::memcpy(m_impl->dataWrite.textures[textureId].data.data() + offset, &revision, sizeof(revision)); offset += sizeof(revision);
    std::memcpy(m_impl->dataWrite.textures[textureId].data.data() + offset, data, bpp*width*height);

    {
        std::unique_lock lock(m_impl->mutex);
        m_impl->dataRead.textures[textureId] = m_impl->dataWrite.textures[textureId];
        m_impl->dataRead.textureIdMap = m_impl->dataWrite.textureIdMap;
    }

    return true;
}

bool ImGuiWS::init(int32_t port, std::string pathHttp, std::vector<std::string> resources, THandler && handlerConnect, THandler && handlerDisconnect) {
    return init(port, std::move(pathHttp), std::move(resources), std::move(handlerConnect), std::move(handlerDisconnect), InitOptions());
}

bool ImGuiWS::init(int32_t port, std::string pathHttp, std::vector<std::string> resources, THandler && handlerConnect, THandler && handlerDisconnect, const InitOptions & options) {
    m_impl->handlerConnect = std::move(handlerConnect);
    m_impl->handlerDisconnect = std::move(handlerDisconnect);

    return init(port, std::move(pathHttp), std::move(resources), options);
}

bool ImGuiWS::setDrawData(const ImDrawData * drawData) {
    bool result = true;

    const auto t0 = std::chrono::steady_clock::now();
    result &= m_impl->compressorDrawData->setDrawData(drawData);
    const auto t1 = std::chrono::steady_clock::now();

    m_impl->lastEncodeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    m_impl->lastEncodePayloadBytes = m_impl->compressorDrawData->drawListsSize();

    auto & drawLists = m_impl->compressorDrawData->getDrawLists();
    auto & drawListsDiff = m_impl->compressorDrawData->getDrawListsDiff();

    // make the draw lists available to incppect clients
    {
        std::unique_lock lock(m_impl->mutex);

        m_impl->dataRead.drawLists = std::move(drawLists);
        m_impl->dataRead.drawListsDiff = std::move(drawListsDiff);
    }

    return result;
}

int32_t ImGuiWS::nConnected() const {
    return m_impl->nConnected;
}

std::vector<std::string> ImGuiWS::varPaths() const {
    return m_impl->dispatch([&](const auto & incpp) {
        return incpp.varPaths();
    });
}

std::vector<char> ImGuiWS::sampleVar(const std::string & path, const std::vector<int> & idxs) const {
    return m_impl->dispatch([&](const auto & incpp) {
        return incpp.sampleVar(path, idxs);
    });
}

ImGuiWS::DrawDataEncodeInfo ImGuiWS::lastDrawDataEncodeInfo() const {
    DrawDataEncodeInfo info;
    info.encodeMs = m_impl->lastEncodeMs;
    info.payloadBytes = m_impl->lastEncodePayloadBytes;
    return info;
}

ImGuiWS::TextureInfo ImGuiWS::textureInfo(TextureId textureId) const {
    TextureInfo info;
    info.id = textureId;

    std::shared_lock lock(m_impl->mutex);

    const auto it = m_impl->dataRead.textures.find(textureId);
    if (it == m_impl->dataRead.textures.end()) {
        return info;
    }

    const auto & data = it->second.data;
    if (data.size() < 20) {
        return info;
    }

    int32_t type = 0;
    std::memcpy(&type, data.data() + 4, sizeof(type));
    std::memcpy(&info.width, data.data() + 8, sizeof(info.width));
    std::memcpy(&info.height, data.data() + 12, sizeof(info.height));
    std::memcpy(&info.revision, data.data() + 16, sizeof(info.revision));

    if (info.width <= 0 || info.height <= 0 || type < 0 || type > 3) {
        return info;
    }

    info.type = static_cast<Texture::Type>(type);
    info.byteSize = data.size();
    info.valid = true;
    return info;
}

std::vector<std::string> ImGuiWS::embeddedResources() const {
    return m_impl->dispatch([&](const auto & incpp) {
        return incpp.embeddedResources();
    });
}

std::vector<std::string> ImGuiWS::httpResourceRoutes() const {
    return m_impl->dispatch([&](const auto & incpp) {
        return incpp.httpResourceRoutes();
    });
}

std::string ImGuiWS::httpRoot() const {
    return m_impl->dispatch([&](const auto & incpp) {
        return incpp.httpRoot();
    });
}

std::string ImGuiWS::staticRoot() const {
    return m_impl->dispatch([&](const auto & incpp) {
        return incpp.staticRoot();
    });
}

std::deque<ImGuiWS::Event> ImGuiWS::takeEvents() {
    std::lock_guard<std::mutex> lock(m_impl->events.mutex);
    auto res = std::move(m_impl->events.data);
    return res;
}
