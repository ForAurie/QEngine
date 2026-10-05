#include <iostream>
#include <memory>
#include <climits>
#include <vector>
#include "QE.hpp"
#include "BMP.hpp"
#include "Camera.hpp"
#include <algorithm>
#include <random>
using namespace std;

Pixel toPixel(const std::optional<Vec3<float>>& colorOpt) {
    if (!colorOpt) return Pixel(0, 0, 0);
    const int mx = 150;
    return Pixel(std::min(255, (int)(colorOpt->x / mx * 255)),std::min(255, (int)(colorOpt->y / mx * 255)),std::min(255, (int)(colorOpt->z / mx * 255)));
}
int main() {
    Vec3<float> p0(0, 0, 0), p1(-1, 0, 0), p2(0, 0, -1), p3(-1, 0, -1), p4(-1, 1, 0), p5(-1, 1, -1), p6(0, 1, -1), p7(0, 1, 0);
    auto mat = make_unique<CookTorrancePBRMaterial<float>>(Vec3<float>(0.2, 0.3, 0.4), Vec3<float>(0.91, 0.92, 0.92), 0.35, 1, 0,
    nullptr, nullptr, nullptr, nullptr, nullptr);
    auto matv = make_unique<MaterialSet<float>>(vector<pair<Material<float>*, float>>(
        {
            make_pair(mat.get(), 1)
        }
    ), false);
    auto mesh = make_unique<TriangleMesh<float>>(vector<Vec3<float>>({p0, p1, p2, p3, p4, p5, p6, p7}));
    
    mesh->insertTriangle(0, 1, 3, matv.get());
    mesh->insertTriangle(0, 3, 2, matv.get());

    mesh->insertTriangle(1, 4, 5, matv.get());
    mesh->insertTriangle(1, 5, 3, matv.get());

    mesh->insertTriangle(3, 5, 6, matv.get());
    mesh->insertTriangle(3, 6, 2, matv.get());

    mesh->insertTriangle(0, 4, 1, matv.get());
    mesh->insertTriangle(0, 7, 4, matv.get());

    mesh->insertTriangle(7, 5, 4, matv.get());
    mesh->insertTriangle(7, 6, 5, matv.get());

    mesh->insertTriangle(0, 2, 6, matv.get());
    mesh->insertTriangle(0, 6, 7, matv.get());

    auto light1 = make_unique<PointLight<float>>(Vec3<float>(2, 2, 2), Vec3<float>(5000, 5000, 5000));
    auto light2 = make_unique<PointLight<float>>(Vec3<float>(-3, 2, -3), Vec3<float>(5000, 5000, 5000));
    auto light3 = make_unique<TriangleLight<float>>(Vec3<float>(2, 0, 2), Vec3<float>(0, 2, 3), Vec3<float>(3, 2, 0), Vec3<float>(5000, 5000, 5000));
    Engine<float> engine;
    mesh->init();
    engine.insertInstance(Instance<float>(mesh.get(), Vec3<float>(0, 0, 0)));
    engine.insertInstance(Instance<float>(mesh.get(), Vec3<float>(-0.5, 0.1, 0.5)));
    engine.insertLight(light1.get());
    engine.insertLight(light2.get());
    engine.insertLight(light3.get());
    const size_t width = 1920, height = 1080;
    Camera<float> camera(Vec3<float>(2, 2, 2), Vec3<float>(-0.5, 0.5, -0.5), Vec3<float>(0, 1, 0), 90.0f * acos(-1) / 180.0f, width, height);
    engine.init();
    vector<vector<Pixel>> image(height, vector<Pixel>(width));
    uint64_t seed = 99832;
    mt19937 rng(static_cast<std::mt19937::result_type>(seed));
    for (size_t i = 0; i < height; i++) {
        for (size_t j = 0; j < width; j++) {
            Ray<float> ray = camera.generateRay(i, j);
            auto colorOpt = engine.renderPixel(rng, ray, 0.05, 50);
            if (colorOpt)
                image[i][j] = toPixel(colorOpt);
        }
    }
    saveBMP("../output.bmp", image);
    return 0;
}