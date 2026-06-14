#include "views.h"

#include "imgui/imgui.h"

#include <array>
#include <algorithm>
#include <cmath>

namespace {

constexpr float kCubeHalf = 1.0f;
constexpr float kCameraDistanceMin = 2.5f;
constexpr float kCameraDistanceMax = 14.0f;
constexpr float kCameraWheelStep = 0.35f;
constexpr float kNearPlane = 0.15f;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3 operator+(const Vec3 & o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(const Vec3 & o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
};

struct Camera {
    Vec3 position { 0.0f, 0.0f, -5.0f };
    Vec3 target { 0.0f, 0.0f, 0.0f };
    Vec3 up { 0.0f, 1.0f, 0.0f };
};

struct ViewBasis {
    Vec3 right {};
    Vec3 up {};
    Vec3 forward {};
};

struct CubePose {
    float pitch = 0.0f;
    float yaw = 0.0f;
};

struct RaySource {
    Vec3 origin {};
    ImU32 color = 0;
};

struct RayHit {
    Vec3 worldPoint {};
    Vec3 worldNormal {};
    bool valid = false;
};

Vec3 rotateX(Vec3 v, float a) {
    const float c = std::cos(a);
    const float s = std::sin(a);
    return { v.x, v.y * c - v.z * s, v.y * s + v.z * c };
}

Vec3 rotateY(Vec3 v, float a) {
    const float c = std::cos(a);
    const float s = std::sin(a);
    return { v.x * c + v.z * s, v.y, -v.x * s + v.z * c };
}

Vec3 transform(Vec3 v, float pitch, float yaw) {
    return rotateY(rotateX(v, pitch), yaw);
}

Vec3 inverseTransform(Vec3 v, float pitch, float yaw) {
    return rotateX(rotateY(v, -yaw), -pitch);
}

float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float length(Vec3 v) {
    return std::sqrt(dot(v, v));
}

Vec3 normalize(Vec3 v) {
    const float len = length(v);
    if (len < 1e-6f) {
        return {};
    }
    return v * (1.0f / len);
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

ViewBasis makeViewBasis(const Camera & camera) {
    const Vec3 forward = normalize(camera.target - camera.position);
    const Vec3 right = normalize(cross(forward, camera.up));
    const Vec3 up = cross(right, forward);
    return { right, up, forward };
}

Vec3 worldToView(Vec3 world, const Camera & camera, const ViewBasis & basis) {
    const Vec3 offset = world - camera.position;
    return {
        dot(offset, basis.right),
        dot(offset, basis.up),
        dot(offset, basis.forward),
    };
}

ImVec2 projectViewPoint(Vec3 view, ImVec2 center, float scale) {
    if (view.z < kNearPlane) {
        return center;
    }
    const float inv = scale / view.z;
    return ImVec2(center.x + view.x * inv, center.y - view.y * inv);
}

bool clipViewSegment(Vec3 & a, Vec3 & b) {
    if (a.z >= kNearPlane && b.z >= kNearPlane) {
        return true;
    }
    if (a.z < kNearPlane && b.z < kNearPlane) {
        return false;
    }
    if (a.z < b.z) {
        std::swap(a, b);
    }
    const float t = (kNearPlane - a.z) / (b.z - a.z);
    b = a + (b - a) * t;
    b.z = kNearPlane;
    return a.z >= kNearPlane;
}

ImVec2 projectWorldPoint(Vec3 world, const Camera & camera, const ViewBasis & basis, ImVec2 center, float scale) {
    return projectViewPoint(worldToView(world, camera, basis), center, scale);
}

Camera makeCamera(float distance) {
    Camera camera {};
    camera.position.z = -distance;
    return camera;
}

Vec3 toWorld(Vec3 local, const CubePose & pose) {
    return transform(local, pose.pitch, pose.yaw);
}

Vec3 reflectDir(Vec3 incident, Vec3 normal) {
    Vec3 n = normal;
    if (dot(incident, n) > 0.0f) {
        n = n * -1.0f;
    }
    return normalize(incident - n * (2.0f * dot(incident, n)));
}

bool rayAabb(Vec3 origin, Vec3 dir, float half, float & tHit, Vec3 & normal) {
    float tEnter = -1e30f;
    float tExit = 1e30f;
    normal = {};

    const auto slab = [&](float o, float d, float bMin, float bMax, Vec3 nMin, Vec3 nMax) -> bool {
        if (std::fabs(d) < 1e-8f) {
            return o >= bMin && o <= bMax;
        }

        float t0 = (bMin - o) / d;
        float t1 = (bMax - o) / d;
        Vec3 n0 = nMin;
        Vec3 n1 = nMax;
        if (t0 > t1) {
            std::swap(t0, t1);
            std::swap(n0, n1);
        }
        if (t0 > tEnter) {
            tEnter = t0;
            normal = n0;
        }
        tExit = std::min(tExit, t1);
        return true;
    };

    if (!slab(origin.x, dir.x, -half, half, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f })) {
        return false;
    }
    if (!slab(origin.y, dir.y, -half, half, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f })) {
        return false;
    }
    if (!slab(origin.z, dir.z, -half, half, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f })) {
        return false;
    }

    if (tExit < 0.0f || tEnter > tExit) {
        return false;
    }

    tHit = tEnter >= 0.0f ? tEnter : tExit;
    return tHit >= 0.0f;
}

bool intersectCubeTowardCenter(
    Vec3 worldOrigin,
    const CubePose & pose,
    Vec3 & worldHit,
    Vec3 & worldNormal) {
    const Vec3 cubeCenter {};
    const Vec3 worldDir = normalize(cubeCenter - worldOrigin);
    const Vec3 localOrigin = inverseTransform(worldOrigin, pose.pitch, pose.yaw);
    const Vec3 localDir = inverseTransform(worldDir, pose.pitch, pose.yaw);

    float tHit = 0.0f;
    Vec3 localNormal {};
    if (!rayAabb(localOrigin, localDir, kCubeHalf, tHit, localNormal)) {
        return false;
    }

    worldHit = toWorld(localOrigin + localDir * tHit, pose);
    worldNormal = normalize(toWorld(localNormal, pose));
    return true;
}

RayHit computeRayHit(const RaySource & source, const CubePose & pose) {
    RayHit hit {};
    hit.valid = intersectCubeTowardCenter(
        source.origin,
        pose,
        hit.worldPoint,
        hit.worldNormal);
    return hit;
}

void drawSegment(
    ImDrawList * dl,
    Vec3 a,
    Vec3 b,
    const Camera & camera,
    const ViewBasis & basis,
    ImVec2 center,
    float scale,
    ImU32 color,
    float thickness) {
    Vec3 viewA = worldToView(a, camera, basis);
    Vec3 viewB = worldToView(b, camera, basis);
    if (!clipViewSegment(viewA, viewB)) {
        return;
    }

    dl->AddLine(
        projectViewPoint(viewA, center, scale),
        projectViewPoint(viewB, center, scale),
        color,
        thickness);
}

void drawCube(
    ImDrawList * dl,
    const CubePose & pose,
    const Camera & camera,
    const ViewBasis & basis,
    ImVec2 center,
    float scale) {
    static constexpr std::array<Vec3, 8> kLocal = {
        Vec3 { -1.0f, -1.0f, -1.0f },
        Vec3 { 1.0f, -1.0f, -1.0f },
        Vec3 { 1.0f, 1.0f, -1.0f },
        Vec3 { -1.0f, 1.0f, -1.0f },
        Vec3 { -1.0f, -1.0f, 1.0f },
        Vec3 { 1.0f, -1.0f, 1.0f },
        Vec3 { 1.0f, 1.0f, 1.0f },
        Vec3 { -1.0f, 1.0f, 1.0f },
    };

    static constexpr std::array<std::pair<int, int>, 12> kEdges = { {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
    } };

    std::array<Vec3, 8> world {};
    std::array<Vec3, 8> view {};
    for (size_t i = 0; i < kLocal.size(); ++i) {
        world[i] = toWorld(kLocal[i] * kCubeHalf, pose);
        view[i] = worldToView(world[i], camera, basis);
    }

    for (const auto & [a, b] : kEdges) {
        const Vec3 & v0 = view[(size_t) a];
        const Vec3 & v1 = view[(size_t) b];
        const float depth = (v0.z + v1.z) * 0.5f;
        const int alpha = (int) (90.0f + (1.0f - depth / 5.5f) * 130.0f);
        drawSegment(dl, world[(size_t) a], world[(size_t) b], camera, basis, center, scale, IM_COL32(220, 224, 230, alpha), 2.0f);
    }

    for (size_t i = 0; i < view.size(); ++i) {
        const float depth = view[i].z;
        const int alpha = (int) (70.0f + (1.0f - depth / 5.5f) * 120.0f);
        dl->AddCircleFilled(
            projectViewPoint(view[i], center, scale),
            2.2f,
            IM_COL32(232, 236, 230, alpha),
            8);
    }
}

void drawRaySystem(
    ImDrawList * dl,
    const RaySource & source,
    const RayHit & hit,
    const Camera & camera,
    const ViewBasis & basis,
    ImVec2 center,
    float scale,
    float reflectLen) {
    if (!hit.valid) {
        return;
    }

    const Vec3 incident = normalize(hit.worldPoint - source.origin);
    if (length(incident) < 1e-5f) {
        return;
    }

    const Vec3 reflected = reflectDir(incident, hit.worldNormal);
    const ImU32 faint = (source.color & 0x00FFFFFF) | 0x55000000;

    drawSegment(dl, source.origin, hit.worldPoint, camera, basis, center, scale, source.color, 2.0f);
    drawSegment(dl, hit.worldPoint, hit.worldPoint + reflected * reflectLen, camera, basis, center, scale, faint, 1.5f);

    const ImVec2 screenHit = projectWorldPoint(hit.worldPoint, camera, basis, center, scale);
    dl->AddCircleFilled(screenHit, 3.5f, source.color, 14);
    dl->AddCircle(screenHit, 6.0f, IM_COL32(255, 255, 255, 70), 16, 1.0f);
}

} // namespace

void viewGfx() {
    static float time = 0.0f;
    static float cameraDistance = 5.0f;

    ImGui::TextDisabled("Колёсико — приближение камеры");

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float height = avail.y > 100.0f ? avail.y - 4.0f : 220.0f;

    ImGui::InvisibleButton("gfx_canvas", ImVec2(avail.x, height));
    if (ImGui::IsItemHovered()) {
        cameraDistance -= ImGui::GetIO().MouseWheel * kCameraWheelStep;
        cameraDistance = std::clamp(cameraDistance, kCameraDistanceMin, kCameraDistanceMax);
    }

    const ImVec2 pmin = ImGui::GetItemRectMin();
    const ImVec2 pmax = ImGui::GetItemRectMax();
    const ImVec2 center((pmin.x + pmax.x) * 0.5f, (pmin.y + pmax.y) * 0.5f);
    const float scale = (pmax.x - pmin.x) < (pmax.y - pmin.y)
        ? (pmax.x - pmin.x) * 0.36f
        : (pmax.y - pmin.y) * 0.36f;

    time += ImGui::GetIO().DeltaTime * 0.5f;
    const CubePose pose {
        0.35f,
        time * 0.7f,
    };

    static const std::array<RaySource, 3> kRays = { {
        // За кубом (z > 0): всегда перед камерой при любом зуме, лучи летят в центр.
        RaySource { { -2.8f, 1.4f, 1.2f }, IM_COL32(230, 90, 90, 255) },
        RaySource { { -2.5f, -1.8f, 1.0f }, IM_COL32(96, 210, 118, 255) },
        RaySource { { 2.9f, -0.2f, 1.1f }, IM_COL32(98, 148, 235, 255) },
    } };

    std::array<RayHit, kRays.size()> rayHits {};
    for (size_t i = 0; i < kRays.size(); ++i) {
        rayHits[i] = computeRayHit(kRays[i], pose);
    }

    const Camera camera = makeCamera(cameraDistance);
    const ViewBasis viewBasis = makeViewBasis(camera);

    ImDrawList * dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pmin, pmax, IM_COL32(20, 22, 28, 255), 8.0f);

    drawCube(dl, pose, camera, viewBasis, center, scale);

    for (size_t i = 0; i < kRays.size(); ++i) {
        drawRaySystem(dl, kRays[i], rayHits[i], camera, viewBasis, center, scale, 1.8f);
    }

    for (const auto & ray : kRays) {
        const Vec3 viewOrigin = worldToView(ray.origin, camera, viewBasis);
        if (viewOrigin.z < kNearPlane) {
            continue;
        }
        const ImVec2 src = projectViewPoint(viewOrigin, center, scale);
        dl->AddCircleFilled(src, 4.0f, ray.color, 14);
        dl->AddCircle(src, 7.0f, IM_COL32(255, 255, 255, 55), 16, 1.0f);
    }

    dl->AddRect(pmin, pmax, IM_COL32(255, 255, 255, 30), 8.0f, 0, 1.0f);
}
