//
//  Material.h
//  libraries/graphics/src/graphics
//
//  Created by Sam Gateau on 12/10/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#ifndef hifi_model_Material_h
#define hifi_model_Material_h

#include <QMutex>

#include <bitset>
#include <map>
#include <unordered_map>
#include <queue>

#include <ColorUtils.h>

#include <gpu/Resource.h>
#include <gpu/TextureTable.h>

#include "MaterialMappingMode.h"

class Transform;

namespace graphics {

class TextureMap;
typedef std::shared_ptr< TextureMap > TextureMapPointer;

// Material Key is a coarse trait description of a material used to classify the materials
class MaterialKey {
public:
   enum FlagBit {
        EMISSIVE_VAL_BIT = 0,
        UNLIT_VAL_BIT,
        ALBEDO_VAL_BIT,
        METALLIC_VAL_BIT,
        GLOSSY_VAL_BIT,
        OPACITY_VAL_BIT,
        OPACITY_MASK_MAP_BIT,           // Opacity Map and Opacity MASK map are mutually exclusive
        OPACITY_TRANSLUCENT_MAP_BIT,
        SCATTERING_VAL_BIT,

        // THe map bits must be in the same sequence as the enum names for the map channels
        EMISSIVE_MAP_BIT,
        ALBEDO_MAP_BIT,
        METALLIC_MAP_BIT,
        ROUGHNESS_MAP_BIT,
        NORMAL_MAP_BIT,
        OCCLUSION_MAP_BIT,
        LIGHTMAP_MAP_BIT,
        SCATTERING_MAP_BIT,

        NUM_FLAGS,
    };
    typedef std::bitset<NUM_FLAGS> Flags;

    enum MapChannel {
        EMISSIVE_MAP = 0,
        ALBEDO_MAP,
        METALLIC_MAP,
        ROUGHNESS_MAP,
        NORMAL_MAP,
        OCCLUSION_MAP,
        LIGHTMAP_MAP,
        SCATTERING_MAP,

        NUM_MAP_CHANNELS,
    };

    // The signature is the Flags
    Flags _flags;

    MaterialKey() : _flags(0) {}
    MaterialKey(const Flags& flags) : _flags(flags) {}

    class Builder {
        Flags _flags{ 0 };
    public:
        Builder() {}

        MaterialKey build() const { return MaterialKey(_flags); }

        Builder& withEmissive() { _flags.set(EMISSIVE_VAL_BIT); return (*this); }
        Builder& withUnlit() { _flags.set(UNLIT_VAL_BIT); return (*this); }

        Builder& withAlbedo() { _flags.set(ALBEDO_VAL_BIT); return (*this); }
        Builder& withMetallic() { _flags.set(METALLIC_VAL_BIT); return (*this); }
        Builder& withGlossy() { _flags.set(GLOSSY_VAL_BIT); return (*this); }

        Builder& withTranslucentFactor() { _flags.set(OPACITY_VAL_BIT); return (*this); }

        Builder& withScattering() { _flags.set(SCATTERING_VAL_BIT); return (*this); }

        Builder& withEmissiveMap() { _flags.set(EMISSIVE_MAP_BIT); return (*this); }
        Builder& withAlbedoMap() { _flags.set(ALBEDO_MAP_BIT); return (*this); }
        Builder& withMetallicMap() { _flags.set(METALLIC_MAP_BIT); return (*this); }
        Builder& withRoughnessMap() { _flags.set(ROUGHNESS_MAP_BIT); return (*this); }

        Builder& withTranslucentMap() { _flags.set(OPACITY_TRANSLUCENT_MAP_BIT); return (*this); }
        Builder& withMaskMap() { _flags.set(OPACITY_MASK_MAP_BIT); return (*this); }

        Builder& withNormalMap() { _flags.set(NORMAL_MAP_BIT); return (*this); }
        Builder& withOcclusionMap() { _flags.set(OCCLUSION_MAP_BIT); return (*this); }
        Builder& withLightmapMap() { _flags.set(LIGHTMAP_MAP_BIT); return (*this); }
        Builder& withScatteringMap() { _flags.set(SCATTERING_MAP_BIT); return (*this); }

        // Convenient standard keys that we will keep on using all over the place
        static MaterialKey opaqueAlbedo() { return Builder().withAlbedo().build(); }
    };

    void setEmissive(bool value) { _flags.set(EMISSIVE_VAL_BIT, value); }
    bool isEmissive() const { return _flags[EMISSIVE_VAL_BIT]; }

    void setUnlit(bool value) { _flags.set(UNLIT_VAL_BIT, value); }
    bool isUnlit() const { return _flags[UNLIT_VAL_BIT]; }

    void setEmissiveMap(bool value) { _flags.set(EMISSIVE_MAP_BIT, value); }
    bool isEmissiveMap() const { return _flags[EMISSIVE_MAP_BIT]; }
 
    void setAlbedo(bool value) { _flags.set(ALBEDO_VAL_BIT, value); }
    bool isAlbedo() const { return _flags[ALBEDO_VAL_BIT]; }

    void setAlbedoMap(bool value) { _flags.set(ALBEDO_MAP_BIT, value); }
    bool isAlbedoMap() const { return _flags[ALBEDO_MAP_BIT]; }

    void setMetallic(bool value) { _flags.set(METALLIC_VAL_BIT, value); }
    bool isMetallic() const { return _flags[METALLIC_VAL_BIT]; }

    void setMetallicMap(bool value) { _flags.set(METALLIC_MAP_BIT, value); }
    bool isMetallicMap() const { return _flags[METALLIC_MAP_BIT]; }

    void setGlossy(bool value) { _flags.set(GLOSSY_VAL_BIT, value); }
    bool isGlossy() const { return _flags[GLOSSY_VAL_BIT]; }
    bool isRough() const { return !_flags[GLOSSY_VAL_BIT]; }

    void setRoughnessMap(bool value) { _flags.set(ROUGHNESS_MAP_BIT, value); }
    bool isRoughnessMap() const { return _flags[ROUGHNESS_MAP_BIT]; }

    void setTranslucentFactor(bool value) { _flags.set(OPACITY_VAL_BIT, value); }
    bool isTranslucentFactor() const { return _flags[OPACITY_VAL_BIT]; }

    void setTranslucentMap(bool value) { _flags.set(OPACITY_TRANSLUCENT_MAP_BIT, value); }
    bool isTranslucentMap() const { return _flags[OPACITY_TRANSLUCENT_MAP_BIT]; }

    void setOpacityMaskMap(bool value) { _flags.set(OPACITY_MASK_MAP_BIT, value); }
    bool isOpacityMaskMap() const { return _flags[OPACITY_MASK_MAP_BIT]; }

    void setNormalMap(bool value) { _flags.set(NORMAL_MAP_BIT, value); }
    bool isNormalMap() const { return _flags[NORMAL_MAP_BIT]; }

    void setOcclusionMap(bool value) { _flags.set(OCCLUSION_MAP_BIT, value); }
    bool isOcclusionMap() const { return _flags[OCCLUSION_MAP_BIT]; }

    void setLightmapMap(bool value) { _flags.set(LIGHTMAP_MAP_BIT, value); }
    bool isLightmapMap() const { return _flags[LIGHTMAP_MAP_BIT]; }

    void setScattering(bool value) { _flags.set(SCATTERING_VAL_BIT, value); }
    bool isScattering() const { return _flags[SCATTERING_VAL_BIT]; }

    void setScatteringMap(bool value) { _flags.set(SCATTERING_MAP_BIT, value); }
    bool isScatteringMap() const { return _flags[SCATTERING_MAP_BIT]; }

    void setMapChannel(MapChannel channel, bool value) { _flags.set(EMISSIVE_MAP_BIT + channel, value); }
    bool isMapChannel(MapChannel channel) const { return _flags[EMISSIVE_MAP_BIT + channel]; }


    // Translucency and Opacity Heuristics are combining several flags:
    bool isTranslucent() const { return isTranslucentFactor() || isTranslucentMap(); }
    bool isOpaque() const { return !isTranslucent(); }
    bool isSurfaceOpaque() const { return isOpaque() && !isOpacityMaskMap(); }
    bool isTexelOpaque() const { return isOpaque() && isOpacityMaskMap(); }
};

class MaterialFilter {
public:
    MaterialKey::Flags _value{ 0 };
    MaterialKey::Flags _mask{ 0 };


    MaterialFilter(const MaterialKey::Flags& value = MaterialKey::Flags(0), const MaterialKey::Flags& mask = MaterialKey::Flags(0)) : _value(value), _mask(mask) {}

    class Builder {
        MaterialKey::Flags _value{ 0 };
        MaterialKey::Flags _mask{ 0 };
    public:
        Builder() {}

        MaterialFilter build() const { return MaterialFilter(_value, _mask); }

        Builder& withoutEmissive()       { _value.reset(MaterialKey::EMISSIVE_VAL_BIT); _mask.set(MaterialKey::EMISSIVE_VAL_BIT); return (*this); }
        Builder& withEmissive()        { _value.set(MaterialKey::EMISSIVE_VAL_BIT);  _mask.set(MaterialKey::EMISSIVE_VAL_BIT); return (*this); }

        Builder& withoutEmissiveMap()       { _value.reset(MaterialKey::EMISSIVE_MAP_BIT); _mask.set(MaterialKey::EMISSIVE_MAP_BIT); return (*this); }
        Builder& withEmissiveMap()        { _value.set(MaterialKey::EMISSIVE_MAP_BIT);  _mask.set(MaterialKey::EMISSIVE_MAP_BIT); return (*this); }

        Builder& withoutUnlit()       { _value.reset(MaterialKey::UNLIT_VAL_BIT); _mask.set(MaterialKey::UNLIT_VAL_BIT); return (*this); }
        Builder& withUnlit()        { _value.set(MaterialKey::UNLIT_VAL_BIT);  _mask.set(MaterialKey::UNLIT_VAL_BIT); return (*this); }

        Builder& withoutAlbedo()       { _value.reset(MaterialKey::ALBEDO_VAL_BIT); _mask.set(MaterialKey::ALBEDO_VAL_BIT); return (*this); }
        Builder& withAlbedo()        { _value.set(MaterialKey::ALBEDO_VAL_BIT);  _mask.set(MaterialKey::ALBEDO_VAL_BIT); return (*this); }

        Builder& withoutAlbedoMap()       { _value.reset(MaterialKey::ALBEDO_MAP_BIT); _mask.set(MaterialKey::ALBEDO_MAP_BIT); return (*this); }
        Builder& withAlbedoMap()        { _value.set(MaterialKey::ALBEDO_MAP_BIT);  _mask.set(MaterialKey::ALBEDO_MAP_BIT); return (*this); }

        Builder& withoutMetallic()       { _value.reset(MaterialKey::METALLIC_VAL_BIT); _mask.set(MaterialKey::METALLIC_VAL_BIT); return (*this); }
        Builder& withMetallic()        { _value.set(MaterialKey::METALLIC_VAL_BIT);  _mask.set(MaterialKey::METALLIC_VAL_BIT); return (*this); }

        Builder& withoutMetallicMap()       { _value.reset(MaterialKey::METALLIC_MAP_BIT); _mask.set(MaterialKey::METALLIC_MAP_BIT); return (*this); }
        Builder& withMetallicMap()        { _value.set(MaterialKey::METALLIC_MAP_BIT);  _mask.set(MaterialKey::METALLIC_MAP_BIT); return (*this); }

        Builder& withoutGlossy()       { _value.reset(MaterialKey::GLOSSY_VAL_BIT); _mask.set(MaterialKey::GLOSSY_VAL_BIT); return (*this); }
        Builder& withGlossy()        { _value.set(MaterialKey::GLOSSY_VAL_BIT);  _mask.set(MaterialKey::GLOSSY_VAL_BIT); return (*this); }

        Builder& withoutRoughnessMap()       { _value.reset(MaterialKey::ROUGHNESS_MAP_BIT); _mask.set(MaterialKey::ROUGHNESS_MAP_BIT); return (*this); }
        Builder& withRoughnessMap()        { _value.set(MaterialKey::ROUGHNESS_MAP_BIT);  _mask.set(MaterialKey::ROUGHNESS_MAP_BIT); return (*this); }

        Builder& withoutTranslucentFactor()       { _value.reset(MaterialKey::OPACITY_VAL_BIT); _mask.set(MaterialKey::OPACITY_VAL_BIT); return (*this); }
        Builder& withTranslucentFactor()        { _value.set(MaterialKey::OPACITY_VAL_BIT);  _mask.set(MaterialKey::OPACITY_VAL_BIT); return (*this); }

        Builder& withoutTranslucentMap()       { _value.reset(MaterialKey::OPACITY_TRANSLUCENT_MAP_BIT); _mask.set(MaterialKey::OPACITY_TRANSLUCENT_MAP_BIT); return (*this); }
        Builder& withTranslucentMap()        { _value.set(MaterialKey::OPACITY_TRANSLUCENT_MAP_BIT);  _mask.set(MaterialKey::OPACITY_TRANSLUCENT_MAP_BIT); return (*this); }

        Builder& withoutMaskMap()       { _value.reset(MaterialKey::OPACITY_MASK_MAP_BIT); _mask.set(MaterialKey::OPACITY_MASK_MAP_BIT); return (*this); }
        Builder& withMaskMap()        { _value.set(MaterialKey::OPACITY_MASK_MAP_BIT);  _mask.set(MaterialKey::OPACITY_MASK_MAP_BIT); return (*this); }

        Builder& withoutNormalMap()       { _value.reset(MaterialKey::NORMAL_MAP_BIT); _mask.set(MaterialKey::NORMAL_MAP_BIT); return (*this); }
        Builder& withNormalMap()        { _value.set(MaterialKey::NORMAL_MAP_BIT);  _mask.set(MaterialKey::NORMAL_MAP_BIT); return (*this); }

        Builder& withoutOcclusionMap()       { _value.reset(MaterialKey::OCCLUSION_MAP_BIT); _mask.set(MaterialKey::OCCLUSION_MAP_BIT); return (*this); }
        Builder& withOcclusionMap()        { _value.set(MaterialKey::OCCLUSION_MAP_BIT);  _mask.set(MaterialKey::OCCLUSION_MAP_BIT); return (*this); }

        Builder& withoutLightmapMap()       { _value.reset(MaterialKey::LIGHTMAP_MAP_BIT); _mask.set(MaterialKey::LIGHTMAP_MAP_BIT); return (*this); }
        Builder& withLightmapMap()        { _value.set(MaterialKey::LIGHTMAP_MAP_BIT);  _mask.set(MaterialKey::LIGHTMAP_MAP_BIT); return (*this); }

        Builder& withoutScattering()       { _value.reset(MaterialKey::SCATTERING_VAL_BIT); _mask.set(MaterialKey::SCATTERING_VAL_BIT); return (*this); }
        Builder& withScattering()        { _value.set(MaterialKey::SCATTERING_VAL_BIT);  _mask.set(MaterialKey::SCATTERING_VAL_BIT); return (*this); }

        Builder& withoutScatteringMap()       { _value.reset(MaterialKey::SCATTERING_MAP_BIT); _mask.set(MaterialKey::SCATTERING_MAP_BIT); return (*this); }
        Builder& withScatteringMap()        { _value.set(MaterialKey::SCATTERING_MAP_BIT);  _mask.set(MaterialKey::SCATTERING_MAP_BIT); return (*this); }


        // Convenient standard keys that we will keep on using all over the place
        static MaterialFilter opaqueAlbedo() { return Builder().withAlbedo().withoutTranslucentFactor().build(); }
    };

    // Item Filter operator testing if a key pass the filter
    bool test(const MaterialKey& key) const { return (key._flags & _mask) == (_value & _mask); }

    class Less {
    public:
        bool operator() (const MaterialFilter& left, const MaterialFilter& right) const {
            if (left._value.to_ulong() == right._value.to_ulong()) {
                return left._mask.to_ulong() < right._mask.to_ulong();
            } else {
                return left._value.to_ulong() < right._value.to_ulong();
            }
        }
    };
};

class Material {
public:
    typedef MaterialKey::MapChannel MapChannel;
    typedef std::map<MapChannel, TextureMapPointer> TextureMaps;

    Material();
    Material(const Material& material);
    Material& operator= (const Material& material);

    const MaterialKey& getKey() const { return _key; }

    static const float DEFAULT_EMISSIVE;
    void setEmissive(const glm::vec3& emissive, bool isSRGB = true);
    glm::vec3 getEmissive(bool SRGB = true) const { return (SRGB ? ColorUtils::tosRGBVec3(_emissive) : _emissive); }

    static const float DEFAULT_OPACITY;
    void setOpacity(float opacity);
    float getOpacity() const { return _opacity; }

    void setUnlit(bool value);
    bool isUnlit() const { return _key.isUnlit(); }

    static const float DEFAULT_ALBEDO;
    void setAlbedo(const glm::vec3& albedo, bool isSRGB = true);
    glm::vec3 getAlbedo(bool SRGB = true) const { return (SRGB ? ColorUtils::tosRGBVec3(_albedo) : _albedo); }

    static const float DEFAULT_METALLIC;
    void setMetallic(float metallic);
    float getMetallic() const { return _metallic; }

    static const float DEFAULT_ROUGHNESS;
    void setRoughness(float roughness);
    float getRoughness() const { return _roughness; }

    static const float DEFAULT_SCATTERING;
    void setScattering(float scattering);
    float getScattering() const { return _scattering; }

    // The texture map to channel association
    static const int NUM_TEXCOORD_TRANSFORMS { 2 };
    void setTextureMap(MapChannel channel, const TextureMapPointer& textureMap);
    const TextureMaps& getTextureMaps() const { return _textureMaps; } // FIXME - not thread safe... 
    const TextureMapPointer getTextureMap(MapChannel channel) const;

    // Albedo maps cannot have opacity detected until they are loaded
    // This method allows const changing of the key/schemaBuffer without touching the map
    void resetOpacityMap() const;

    // conversion from legacy material properties to PBR equivalent
    static float shininessToRoughness(float shininess) { return 1.0f - shininess / 100.0f; }

    void setTextureTransforms(const Transform& transform, MaterialMappingMode mode, bool repeat);

    const std::string& getName() const { return _name; }

    const std::string& getModel() const { return _model; }
    void setModel(const std::string& model) { _model = model; }

    glm::mat4 getTexCoordTransform(uint i) const { return _texcoordTransforms[i]; }
    glm::vec2 getLightmapParams() const { return _lightmapParams; }
    glm::vec2 getMaterialParams() const { return _materialParams; }

    bool getDefaultFallthrough() const { return _defaultFallthrough; }
    void setDefaultFallthrough(bool defaultFallthrough) { _defaultFallthrough = defaultFallthrough; }

    enum ExtraFlagBit {
        TEXCOORDTRANSFORM0 = MaterialKey::NUM_FLAGS,
        TEXCOORDTRANSFORM1,
        LIGHTMAP_PARAMS,
        MATERIAL_PARAMS,

        NUM_TOTAL_FLAGS
    };
    std::unordered_map<uint, bool> getPropertyFallthroughs() { return _propertyFallthroughs; }
    bool getPropertyFallthrough(uint property) { return _propertyFallthroughs[property]; }
    void setPropertyDoesFallthrough(uint property) { _propertyFallthroughs[property] = true; }

protected:
    std::string _name { "" };

private:
    std::string _model { "hifi_pbr" };
    mutable MaterialKey _key { 0 };

    // Material properties
    glm::vec3 _emissive { DEFAULT_EMISSIVE };
    float _opacity { DEFAULT_OPACITY };
    glm::vec3 _albedo { DEFAULT_ALBEDO };
    float _roughness { DEFAULT_ROUGHNESS };
    float _metallic { DEFAULT_METALLIC };
    float _scattering { DEFAULT_SCATTERING };
    std::array<glm::mat4, NUM_TEXCOORD_TRANSFORMS> _texcoordTransforms;
    glm::vec2 _lightmapParams { 0.0, 1.0 };
    glm::vec2 _materialParams { 0.0, 1.0 };
    TextureMaps _textureMaps;

    bool _defaultFallthrough { false };
    std::unordered_map<uint, bool> _propertyFallthroughs { NUM_TOTAL_FLAGS };

    mutable QMutex _textureMapsMutex { QMutex::Recursive };
};
typedef std::shared_ptr<Material> MaterialPointer;

class MaterialLayer {
public:
    MaterialLayer(MaterialPointer material, quint16 priority) : material(material), priority(priority) {}

    MaterialPointer material { nullptr };
    quint16 priority { 0 };
};

class MaterialLayerCompare {
public:
    bool operator() (MaterialLayer left, MaterialLayer right) {
        return left.priority < right.priority;
    }
};
typedef std::priority_queue<MaterialLayer, std::vector<MaterialLayer>, MaterialLayerCompare> MaterialLayerQueue;

class MultiMaterial : public MaterialLayerQueue {
public:
    MultiMaterial();

    void push(const MaterialLayer& value) {
        MaterialLayerQueue::push(value);
        _hasCalculatedTextureInfo = false;
        _needsUpdate = true;
    }

    bool remove(const MaterialPointer& value) {
        auto it = c.begin();
        while (it != c.end()) {
            if (it->material == value) {
                break;
            }
            it++;
        }
        if (it != c.end()) {
            c.erase(it);
            std::make_heap(c.begin(), c.end(), comp);
            _hasCalculatedTextureInfo = false;
            _needsUpdate = true;
            return true;
        } else {
            return false;
        }
    }

    // Schema to access the attribute values of the material
    class Schema {
    public:
        glm::vec3 _emissive { Material::DEFAULT_EMISSIVE }; // No Emissive
        float _opacity { Material::DEFAULT_OPACITY }; // Opacity = 1 => Not Transparent

        glm::vec3 _albedo { Material::DEFAULT_ALBEDO }; // Grey albedo => isAlbedo
        float _roughness { Material::DEFAULT_ROUGHNESS }; // Roughness = 1 => Not Glossy

        float _metallic { Material::DEFAULT_METALLIC }; // Not Metallic
        float _scattering { Material::DEFAULT_SCATTERING }; // Scattering info
#if defined(__clang__)
        __attribute__((unused))
#endif
        glm::vec2 _spare { 0.0f }; // Padding

        uint32_t _key { 0 }; // a copy of the materialKey
#if defined(__clang__)
        __attribute__((unused))
#endif
        glm::vec3 _spare2 { 0.0f };

        // for alignment beauty, Material size == Mat4x4

        // Texture Coord Transform Array
        glm::mat4 _texcoordTransforms[Material::NUM_TEXCOORD_TRANSFORMS];

        glm::vec2 _lightmapParams { 0.0, 1.0 };

        // x: material mode (0 for UV, 1 for PROJECTED)
        // y: 1 for texture repeat, 0 for discard outside of 0 - 1
        glm::vec2 _materialParams { 0.0, 1.0 };

        Schema() {
            for (auto& transform : _texcoordTransforms) {
                transform = glm::mat4();
            }
        }
    };

    gpu::BufferView& getSchemaBuffer() { return _schemaBuffer; }
    graphics::MaterialKey getMaterialKey() const { return graphics::MaterialKey(_schemaBuffer.get<graphics::MultiMaterial::Schema>()._key); }
    const gpu::TextureTablePointer& getTextureTable() const { return _textureTable; }

    bool needsUpdate() const { return _needsUpdate; }
    void setNeedsUpdate(bool needsUpdate) { _needsUpdate = needsUpdate; }

    void setTexturesLoading(bool value) { _texturesLoading = value; }
    bool areTexturesLoading() const { return _texturesLoading; }

    int getTextureCount() const { calculateMaterialInfo(); return _textureCount; }
    size_t getTextureSize()  const { calculateMaterialInfo(); return _textureSize; }
    bool hasTextureInfo() const { return _hasCalculatedTextureInfo; }

private:
    gpu::BufferView _schemaBuffer;
    gpu::TextureTablePointer _textureTable { std::make_shared<gpu::TextureTable>() };
    bool _needsUpdate { false };
    bool _texturesLoading { false };

    mutable size_t _textureSize { 0 };
    mutable int _textureCount { 0 };
    mutable bool _hasCalculatedTextureInfo { false };
    void calculateMaterialInfo() const;
};

};

#endif
