//
//  ModelCache.h
//  libraries/model-networking
//
//  Created by Zach Pomerantz on 3/15/16.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ModelCache_h
#define hifi_ModelCache_h

#include <DependencyManager.h>
#include <ResourceCache.h>

#include <graphics/Material.h>
#include <graphics/Asset.h>

#include "FBXSerializer.h"
#include "TextureCache.h"
#include "ModelLoader.h"

// Alias instead of derive to avoid copying

class NetworkTexture;
class NetworkMaterial;
class MeshPart;

class GeometryMappingResource;

class Geometry {
public:
    using Pointer = std::shared_ptr<Geometry>;
    using WeakPointer = std::weak_ptr<Geometry>;

    Geometry() = default;
    Geometry(const Geometry& geometry);
    virtual ~Geometry() = default;

    // Immutable over lifetime
    using GeometryMeshes = std::vector<std::shared_ptr<const graphics::Mesh>>;
    using GeometryMeshParts = std::vector<std::shared_ptr<const MeshPart>>;

    // Mutable, but must retain structure of vector
    using NetworkMaterials = std::vector<std::shared_ptr<NetworkMaterial>>;

    bool isHFMModelLoaded() const { return (bool)_hfmModel; }

    const HFMModel& getHFMModel() const { return *_hfmModel; }
    const GeometryMeshes& getMeshes() const { return *_meshes; }
    const std::shared_ptr<NetworkMaterial> getShapeMaterial(int shapeID) const;

    const QVariantMap getTextures() const;
    void setTextures(const QVariantMap& textureMap);

    virtual bool areTexturesLoaded() const;
    const QUrl& getAnimGraphOverrideUrl() const { return _animGraphOverrideUrl; }
    const QVariantHash& getMapping() const { return _mapping; }

protected:
    friend class GeometryMappingResource;

    // Shared across all geometries, constant throughout lifetime
    std::shared_ptr<const HFMModel> _hfmModel;
    std::shared_ptr<const GeometryMeshes> _meshes;
    std::shared_ptr<const GeometryMeshParts> _meshParts;

    // Copied to each geometry, mutable throughout lifetime via setTextures
    NetworkMaterials _materials;

    QUrl _animGraphOverrideUrl;
    QVariantHash _mapping;  // parsed contents of FST file.

private:
    mutable bool _areTexturesLoaded { false };
};

/// A geometry loaded from the network.
class GeometryResource : public Resource, public Geometry {
public:
    using Pointer = QSharedPointer<GeometryResource>;

    GeometryResource(const QUrl& url, const QUrl& textureBaseUrl = QUrl()) :
        Resource(url), _textureBaseUrl(textureBaseUrl) {}

    virtual bool areTexturesLoaded() const override { return isLoaded() && Geometry::areTexturesLoaded(); }

    virtual void deleter() override;

protected:
    friend class ModelCache;
    friend class GeometryMappingResource;

    // Geometries may not hold onto textures while cached - that is for the texture cache
    // Instead, these methods clear and reset textures from the geometry when caching/loading
    bool shouldSetTextures() const { return _hfmModel && _materials.empty(); }
    void setTextures();
    void resetTextures();

    QUrl _textureBaseUrl;

    virtual bool isCacheable() const override { return _loaded && _isCacheable; }
    bool _isCacheable { true };
};

class GeometryResourceWatcher : public QObject {
    Q_OBJECT
public:
    using Pointer = std::shared_ptr<GeometryResourceWatcher>;

    GeometryResourceWatcher() = delete;
    GeometryResourceWatcher(Geometry::Pointer& geometryPtr) : _geometryRef(geometryPtr) {}

    void setResource(GeometryResource::Pointer resource);

    QUrl getURL() const { return (bool)_resource ? _resource->getURL() : QUrl(); }
    int getResourceDownloadAttempts() { return _resource ? _resource->getDownloadAttempts() : 0; }
    int getResourceDownloadAttemptsRemaining() { return _resource ? _resource->getDownloadAttemptsRemaining() : 0; }

private:
    void startWatching();
    void stopWatching();

signals:
    void finished(bool success);

private slots:
    void resourceFinished(bool success);
    void resourceRefreshed();

private:
    GeometryResource::Pointer _resource;
    Geometry::Pointer& _geometryRef;
};

/// Stores cached model geometries.
class ModelCache : public ResourceCache, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:

    GeometryResource::Pointer getGeometryResource(const QUrl& url,
                                                  const QVariantHash& mapping = QVariantHash(),
                                                  const QUrl& textureBaseUrl = QUrl());

    GeometryResource::Pointer getCollisionGeometryResource(const QUrl& url,
                                                           const QVariantHash& mapping = QVariantHash(),
                                                           const QUrl& textureBaseUrl = QUrl());

protected:
    friend class GeometryMappingResource;

    virtual QSharedPointer<Resource> createResource(const QUrl& url, const QSharedPointer<Resource>& fallback,
                                                    const void* extra) override;

private:
    ModelCache();
    virtual ~ModelCache() = default;
    ModelLoader _modelLoader;
};

class NetworkMaterial : public graphics::Material {
public:
    using MapChannel = graphics::Material::MapChannel;

    NetworkMaterial() : _textures(MapChannel::NUM_MAP_CHANNELS) {}
    NetworkMaterial(const HFMMaterial& material, const QUrl& textureBaseUrl);
    NetworkMaterial(const NetworkMaterial& material);

    void setAlbedoMap(const QUrl& url, bool useAlphaChannel);
    void setNormalMap(const QUrl& url, bool isBumpmap);
    void setRoughnessMap(const QUrl& url, bool isGloss);
    void setMetallicMap(const QUrl& url, bool isSpecular);
    void setOcclusionMap(const QUrl& url);
    void setEmissiveMap(const QUrl& url);
    void setScatteringMap(const QUrl& url);
    void setLightmapMap(const QUrl& url);

    bool isMissingTexture();
    void checkResetOpacityMap();

protected:
    friend class Geometry;

    class Texture {
    public:
        QString name;
        NetworkTexturePointer texture;
    };
    using Textures = std::vector<Texture>;

    Textures _textures;

    static const QString NO_TEXTURE;
    const QString& getTextureName(MapChannel channel);

    void setTextures(const QVariantMap& textureMap);

    const bool& isOriginal() const { return _isOriginal; }

private:
    // Helpers for the ctors
    QUrl getTextureUrl(const QUrl& baseUrl, const HFMTexture& hfmTexture);
    graphics::TextureMapPointer fetchTextureMap(const QUrl& baseUrl, const HFMTexture& hfmTexture,
                                             image::TextureUsage::Type type, MapChannel channel);
    graphics::TextureMapPointer fetchTextureMap(const QUrl& url, image::TextureUsage::Type type, MapChannel channel);

    Transform _albedoTransform;
    Transform _lightmapTransform;
    vec2 _lightmapParams;

    bool _isOriginal { true };
};

class MeshPart {
public:
    MeshPart(int mesh, int part, int material) : meshID { mesh }, partID { part }, materialID { material } {}
    int meshID { -1 };
    int partID { -1 };
    int materialID { -1 };
};

#endif // hifi_ModelCache_h
