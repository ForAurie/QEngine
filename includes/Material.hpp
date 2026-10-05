#ifndef MATERIAL_H
#define MATERIAL_H
#include "Vec3.hpp"
#include <vector>
#include <utility>
#include "Consts.hpp"
#include <climits>
#include "Consts.hpp"
enum class MaterialType {
    CookTorrance,
    CookTorrancePBR,
    SelfIllumination
};

template<typename T = float>
class Material {
public:
    virtual ~Material() = default;
    virtual MaterialType getType() const = 0;
    virtual Vec3<T> getColor(
        const Vec3<T>& lightColor,
        const Vec3<T>& l,       // 光源方向 (hitPos -> light)
        const Vec3<T>& v,       // 视线方向 (hitPos -> camera)
        const Vec3<T>& n,       // 法线
        T x, T y
    ) const = 0;
};

template<typename T = float>
class CookTorranceMaterial : public Material<T> {
public:
    Vec3<T> albedo;      // 漫反射颜色
    Vec3<T> F0;          // 基反射率 (non-metal: ~0.04, metal: 根据材质)
    T roughness;         // 粗糙度 [0,1]
    T metalness;         // 金属度 [0,1]
    T sigma;             // 材质浑浊度
    // IOR(__IOR), refractiveIndex(__refractiveIndex)
    CookTorranceMaterial(
        const Vec3<T>& __albedo = Vec3<float>(0.45f, 0.43f, 0.4f),
        const Vec3<T>& __F0 = Vec3<float>(0.04f, 0.04f, 0.04f),
        T __roughness = 0.8,
        T __metalness = 0,
        T __sigma = 0.1
    ) : albedo(__albedo), F0(__F0), roughness(__roughness), metalness(__metalness), sigma(__sigma) {}
    MaterialType getType() const override { return MaterialType::CookTorrance; }
    Vec3<T> getColor(const Vec3<T>& lightColor, const Vec3<T>& l, const Vec3<T>& v, const Vec3<T>& n,
                    T x, T y) const override {
        // 半程向量
        const Vec3<T> h = (l + v).normalized();
        // dot 值
        const T NdotL = n.dot(l);
        const T NdotV = n.dot(v);
        const T NdotH = std::max((T)0, n.dot(h));
        const T VdotH = std::max((T)0, v.dot(h));

        if (NdotL <= 0 || NdotV <= 0) return Vec3<T>(0, 0, 0); // 光线不在半球内，没贡献

        // === 1. 法线分布函数 D (GGX) ===
        T a = roughness * roughness;
        T a2 = a * a;
        T denom = (NdotH * NdotH * (a2 - 1) + 1);
        denom = T(PI) * denom * denom;
        T D = a2 / std::max((T)1e-6, denom);

        // === 2. 菲涅尔项 F (Schlick) ===
        Vec3<T> F = F0 + (Vec3<T>(1, 1, 1) - F0) * std::pow(1 - VdotH, 5);

        // === 3. 几何函数 G (Smith) ===
        auto G_SchlickGGX = [&](T NdotX) {
            T k = (roughness + 1.0);
            k = k * k / 8; // Smith's method
            return NdotX / (NdotX * (1.0 - k) + k);
        };
        T G = G_SchlickGGX(NdotL) * G_SchlickGGX(NdotV);

        // Cook–Torrance BRDF
        Vec3<T> specular = (D * G) * F / std::max(T(1e-6), T(4) * NdotL * NdotV);
        // 漫反射部分
        Vec3<T> diffuse = (Vec3<T>(1, 1, 1) - F) * (1 - metalness) * albedo / T(PI);

        // 最终结果
        return (diffuse + specular) * lightColor * NdotL;
    }
};

template<typename T = float>
class Texture {
public:
    virtual ~Texture() = default;
    // uv 坐标 [0,1] × [0,1]
    virtual Vec3<T> sample(T x, T y) const = 0;
};

template<typename T = float>
class ImageTexture : public Texture<T> {
private:
    size_t width, height;
    std::vector<Vec3<T>> data; // RGB存储
public:
    ImageTexture(size_t __width, size_t __height, const std::vector<Vec3<T>>& __data) 
        : width(__width), height(__height), data(__data) {}
    Vec3<T> sample(T x, T y) const override {
        x = std::clamp(x, 0, 1);
        y = std::clamp(y, 0, 1);
        return data[std::min(size_t(round(x * (height - 1))), height - 1) * width + std::min(size_t(round(y * (width - 1))), width - 1)];
    }
};


template<typename T = float>
class CookTorrancePBRMaterial : public Material<T> {
public:
    Texture<T>* albedoMap = nullptr;
    Texture<T>* F0Map = nullptr;
    Texture<T>* roughnessMap = nullptr;
    Texture<T>* metalnessMap = nullptr;
    Texture<T>* normalMap = nullptr; // 新增

    Vec3<T> albedo;
    Vec3<T> F0;
    T roughness;
    T metalness;
    T sigma;
    CookTorrancePBRMaterial(
        const Vec3<T>& __albedo,
        const Vec3<T>& __F0,
        T __roughness,
        T __metalness,
        T __sigma,
        Texture<T>* __albedoMap = nullptr,
        Texture<T>* __F0Map = nullptr,
        Texture<T>* __roughnessMap = nullptr,
        Texture<T>* __metalnessMap = nullptr,
        Texture<T>* __normalMap = nullptr
    ) : albedoMap(__albedoMap),
        F0Map(__F0Map),
        roughnessMap(__roughnessMap),
        metalnessMap(__metalnessMap),
        normalMap(__normalMap),
        albedo(__albedo),
        F0(__F0),
        roughness(__roughness),
        metalness(__metalness),
        sigma(__sigma) {}
    MaterialType getType() const override { return MaterialType::CookTorrancePBR; }

    Vec3<T> getColor(const Vec3<T>& lightColor, const Vec3<T>& l,
                     const Vec3<T>& v, const Vec3<T>& n,
                     T x, T y) const override {
        Vec3<T> __albedo = albedo;
        Vec3<T> __F0 = F0;
        T __roughness = roughness;
        T __metalness = metalness;
        Vec3<T> __n = n;

        if(albedoMap) __albedo = albedoMap->sample(x, y);
        if(F0Map) __F0 = F0Map->sample(x, y);
        if(roughnessMap) __roughness = roughnessMap->sample(x, y).x;
        if(metalnessMap) __metalness = metalnessMap->sample(x, y).x;
        if(normalMap) __n = normalMap->sample(x, y);

        Vec3<T> h = (l + v).normalized();
        T NdotL = __n.dot(l);
        T NdotV = __n.dot(v);
        T NdotH = std::max((T)0, __n.dot(h));
        T VdotH = std::max((T)0, v.dot(h));

        if(NdotL <= 0 || NdotV <= 0) return Vec3<T>(0, 0, 0);

        // D - GGX
        T a = __roughness * __roughness;
        T a2 = a * a;
        T denom = (NdotH * NdotH * (a2 - 1) + 1);
        denom = T(PI) * denom * denom;
        T D = a2 / std::max((T)1e-6, denom);

        // F - Schlick
        Vec3<T> F = __F0 + (Vec3<T>(1, 1, 1) - __F0) * std::pow(1 - VdotH, 5);

        // G - Smith
        auto G_SchlickGGX = [&](T NdotX){
            T k = __roughness + 1;
            k = k * k / 8;
            return NdotX / (NdotX * (1 - k) + k);
        };
        T G = G_SchlickGGX(NdotL) * G_SchlickGGX(NdotV);

        Vec3<T> specular = (D * G) * F / std::max((T)1e-6, T(4) * NdotL * NdotV);
        Vec3<T> diffuse = (Vec3<T>(1, 1, 1) - F) * (1 - __metalness) * __albedo / T(PI);

        return (diffuse + specular) * lightColor * NdotL;
    }
};

template<typename T = float>
class SelfIlluminationMaterial : public Material<T> {
public:
    Vec3<T> Color;
    SelfIlluminationMaterial(
        const Vec3<T>& __Color
    ) : Color(__Color) {}
    MaterialType getType() const override { return MaterialType::SelfIllumination; }
    // 为了统一化接口冗余设计，希望 -O3 优化掉
    Vec3<T> getColor(const Vec3<T>& lightColor, const Vec3<T>& l, const Vec3<T>& v, const Vec3<T>& n, T x, T y) const override {
        return Color;
    }
};

template<typename T>
class MaterialSet : public std::vector<std::pair<Material<T>*, T>> {
public:
    bool doubleSided;
    MaterialSet(
        const std::vector<std::pair<Material<T>*, T>>& __materialSet,
        bool __doubleSided = false
    ) : std::vector<std::pair<Material<T>*, T>>(__materialSet), doubleSided(__doubleSided) {}
};
// 光线与物体的交点信息
template<typename T = float>
struct HitInfo {
    T t = 0;          // 交点距离
    Vec3<T> position; // 交点坐标
    Vec3<T> normal;   // 法线
    MaterialSet<T>* materialSet = nullptr; // 材质信息
    bool isBack = false;
};
#endif