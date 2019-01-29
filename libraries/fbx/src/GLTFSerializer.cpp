//
//  GLTFSerializer.cpp
//  libraries/fbx/src
//
//  Created by Luis Cuenca on 8/30/17.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "GLTFSerializer.h"

#include <QtCore/QBuffer>
#include <QtCore/QIODevice>
#include <QtCore/QEventLoop>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qpair.h>
#include <QtCore/qlist.h>


#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>

#include <qfile.h>
#include <qfileinfo.h>

#include <shared/NsightHelpers.h>
#include <NetworkAccessManager.h>
#include <ResourceManager.h>
#include <PathUtils.h>

#include "FBXSerializer.h"

bool GLTFSerializer::getStringVal(const QJsonObject& object, const QString& fieldname,
                              QString& value, QMap<QString, bool>&  defined) {
    bool _defined = (object.contains(fieldname) && object[fieldname].isString());
    if (_defined) {
        value = object[fieldname].toString();
    }
    defined.insert(fieldname, _defined);
    return _defined;
}

bool GLTFSerializer::getBoolVal(const QJsonObject& object, const QString& fieldname,
                            bool& value, QMap<QString, bool>&  defined) {
    bool _defined = (object.contains(fieldname) && object[fieldname].isBool());
    if (_defined) {
        value = object[fieldname].toBool();
    }
    defined.insert(fieldname, _defined);
    return _defined;
}

bool GLTFSerializer::getIntVal(const QJsonObject& object, const QString& fieldname,
                           int& value, QMap<QString, bool>&  defined) {
    bool _defined = (object.contains(fieldname) && !object[fieldname].isNull());
    if (_defined) {
        value = object[fieldname].toInt();
    }
    defined.insert(fieldname, _defined);
    return _defined;
}

bool GLTFSerializer::getDoubleVal(const QJsonObject& object, const QString& fieldname,
                              double& value, QMap<QString, bool>&  defined) {
    bool _defined = (object.contains(fieldname) && object[fieldname].isDouble());
    if (_defined) {
        value = object[fieldname].toDouble();
    }
    defined.insert(fieldname, _defined);
    return _defined;
}
bool GLTFSerializer::getObjectVal(const QJsonObject& object, const QString& fieldname,
                              QJsonObject& value, QMap<QString, bool>&  defined) {
    bool _defined = (object.contains(fieldname) && object[fieldname].isObject());
    if (_defined) {
        value = object[fieldname].toObject();
    }
    defined.insert(fieldname, _defined);
    return _defined;
}

bool GLTFSerializer::getIntArrayVal(const QJsonObject& object, const QString& fieldname,
                                QVector<int>& values, QMap<QString, bool>&  defined) {
    bool _defined = (object.contains(fieldname) && object[fieldname].isArray());
    if (_defined) {
        QJsonArray arr = object[fieldname].toArray();
        foreach(const QJsonValue & v, arr) {
            if (!v.isNull()) {
                values.push_back(v.toInt());
            }
        }
    }
    defined.insert(fieldname, _defined);
    return _defined;
}

bool GLTFSerializer::getDoubleArrayVal(const QJsonObject& object, const QString& fieldname,
                                   QVector<double>& values, QMap<QString, bool>&  defined) {
    bool _defined = (object.contains(fieldname) && object[fieldname].isArray());
    if (_defined) {
        QJsonArray arr = object[fieldname].toArray();
        foreach(const QJsonValue & v, arr) {
            if (v.isDouble()) {
                values.push_back(v.toDouble());
            }
        }
    }
    defined.insert(fieldname, _defined);
    return _defined;
}

bool GLTFSerializer::getObjectArrayVal(const QJsonObject& object, const QString& fieldname,
                                   QJsonArray& objects, QMap<QString, bool>& defined) {
    bool _defined = (object.contains(fieldname) && object[fieldname].isArray());
    if (_defined) {
        objects = object[fieldname].toArray();
    }
    defined.insert(fieldname, _defined);
    return _defined;
}

int GLTFSerializer::getMeshPrimitiveRenderingMode(const QString& type)
{
    if (type == "POINTS") {
        return GLTFMeshPrimitivesRenderingMode::POINTS;
    }
    if (type == "LINES") {
        return GLTFMeshPrimitivesRenderingMode::LINES;
    }
    if (type == "LINE_LOOP") {
        return GLTFMeshPrimitivesRenderingMode::LINE_LOOP;
    }
    if (type == "LINE_STRIP") {
        return GLTFMeshPrimitivesRenderingMode::LINE_STRIP;
    }
    if (type == "TRIANGLES") {
        return GLTFMeshPrimitivesRenderingMode::TRIANGLES;
    }
    if (type == "TRIANGLE_STRIP") {
        return GLTFMeshPrimitivesRenderingMode::TRIANGLE_STRIP;
    }
    if (type == "TRIANGLE_FAN") {
        return GLTFMeshPrimitivesRenderingMode::TRIANGLE_FAN;
    }
    return GLTFMeshPrimitivesRenderingMode::TRIANGLES;
}

int GLTFSerializer::getAccessorType(const QString& type)
{
    if (type == "SCALAR") {
        return GLTFAccessorType::SCALAR;
    }
    if (type == "VEC2") {
        return GLTFAccessorType::VEC2;
    }
    if (type == "VEC3") {
        return GLTFAccessorType::VEC3;
    }
    if (type == "VEC4") {
        return GLTFAccessorType::VEC4;
    }
    if (type == "MAT2") {
        return GLTFAccessorType::MAT2;
    }
    if (type == "MAT3") {
        return GLTFAccessorType::MAT3;
    }
    if (type == "MAT4") {
        return GLTFAccessorType::MAT4;
    }
    return GLTFAccessorType::SCALAR;
}

int GLTFSerializer::getMaterialAlphaMode(const QString& type)
{
    if (type == "OPAQUE") {
        return GLTFMaterialAlphaMode::OPAQUE;
    }
    if (type == "MASK") {
        return GLTFMaterialAlphaMode::MASK;
    }
    if (type == "BLEND") {
        return GLTFMaterialAlphaMode::BLEND;
    }
    return GLTFMaterialAlphaMode::OPAQUE;
}

int GLTFSerializer::getCameraType(const QString& type)
{
    if (type == "orthographic") {
        return GLTFCameraTypes::ORTHOGRAPHIC;
    }
    if (type == "perspective") {
        return GLTFCameraTypes::PERSPECTIVE;
    }
    return GLTFCameraTypes::PERSPECTIVE;
}

int GLTFSerializer::getImageMimeType(const QString& mime)
{
    if (mime == "image/jpeg") {
        return GLTFImageMimetype::JPEG;
    }
    if (mime == "image/png") {
        return GLTFImageMimetype::PNG;
    }
    return GLTFImageMimetype::JPEG;
}

int GLTFSerializer::getAnimationSamplerInterpolation(const QString& interpolation)
{
    if (interpolation == "LINEAR") {
        return GLTFAnimationSamplerInterpolation::LINEAR;
    }
    return GLTFAnimationSamplerInterpolation::LINEAR;
}

bool GLTFSerializer::setAsset(const QJsonObject& object) {
    QJsonObject jsAsset;
    bool isAssetDefined = getObjectVal(object, "asset", jsAsset, _file.defined);
    if (isAssetDefined) {
        if (!getStringVal(jsAsset, "version", _file.asset.version, 
                          _file.asset.defined) || _file.asset.version != "2.0") {
            return false;
        }
        getStringVal(jsAsset, "generator", _file.asset.generator, _file.asset.defined);
        getStringVal(jsAsset, "copyright", _file.asset.copyright, _file.asset.defined);
    }
    return isAssetDefined;
}

bool GLTFSerializer::addAccessor(const QJsonObject& object) {
    GLTFAccessor accessor;
    
    getIntVal(object, "bufferView", accessor.bufferView, accessor.defined);
    getIntVal(object, "byteOffset", accessor.byteOffset, accessor.defined);
    getIntVal(object, "componentType", accessor.componentType, accessor.defined);
    getIntVal(object, "count", accessor.count, accessor.defined);
    getBoolVal(object, "normalized", accessor.normalized, accessor.defined);
    QString type;
    if (getStringVal(object, "type", type, accessor.defined)) {
        accessor.type = getAccessorType(type);
    }
    getDoubleArrayVal(object, "max", accessor.max, accessor.defined);
    getDoubleArrayVal(object, "min", accessor.min, accessor.defined);

    _file.accessors.push_back(accessor);

    return true;
}

bool GLTFSerializer::addAnimation(const QJsonObject& object) {
    GLTFAnimation animation;
    
    QJsonArray channels;
    if (getObjectArrayVal(object, "channels", channels, animation.defined)) {
        foreach(const QJsonValue & v, channels) {
            if (v.isObject()) {
                GLTFChannel channel;
                getIntVal(v.toObject(), "sampler", channel.sampler, channel.defined);
                QJsonObject jsChannel;
                if (getObjectVal(v.toObject(), "target", jsChannel, channel.defined)) {
                    getIntVal(jsChannel, "node", channel.target.node, channel.target.defined);
                    getIntVal(jsChannel, "path", channel.target.path, channel.target.defined);
                }             
            }
        }
    }

    QJsonArray samplers;
    if (getObjectArrayVal(object, "samplers", samplers, animation.defined)) {
        foreach(const QJsonValue & v, samplers) {
            if (v.isObject()) {
                GLTFAnimationSampler sampler;
                getIntVal(v.toObject(), "input", sampler.input, sampler.defined);
                getIntVal(v.toObject(), "output", sampler.input, sampler.defined);
                QString interpolation;
                if (getStringVal(v.toObject(), "interpolation", interpolation, sampler.defined)) {
                    sampler.interpolation = getAnimationSamplerInterpolation(interpolation);
                }
            }
        }
    }
    
    _file.animations.push_back(animation);

    return true;
}

bool GLTFSerializer::addBufferView(const QJsonObject& object) {
    GLTFBufferView bufferview;
    
    getIntVal(object, "buffer", bufferview.buffer, bufferview.defined);
    getIntVal(object, "byteLength", bufferview.byteLength, bufferview.defined);
    getIntVal(object, "byteOffset", bufferview.byteOffset, bufferview.defined);
    getIntVal(object, "target", bufferview.target, bufferview.defined);
    
    _file.bufferviews.push_back(bufferview);
   
    return true;
}

bool GLTFSerializer::addBuffer(const QJsonObject& object) {
    GLTFBuffer buffer;
   
    getIntVal(object, "byteLength", buffer.byteLength, buffer.defined);
    if (getStringVal(object, "uri", buffer.uri, buffer.defined)) {
        if (!readBinary(buffer.uri, buffer.blob)) {
            return false;
        }
    }
    _file.buffers.push_back(buffer);
    
    return true;
}

bool GLTFSerializer::addCamera(const QJsonObject& object) {
    GLTFCamera camera;
    
    QJsonObject jsPerspective;
    QJsonObject jsOrthographic;
    QString type;
    getStringVal(object, "name", camera.name, camera.defined);
    if (getObjectVal(object, "perspective", jsPerspective, camera.defined)) {
        getDoubleVal(jsPerspective, "aspectRatio", camera.perspective.aspectRatio, camera.perspective.defined);
        getDoubleVal(jsPerspective, "yfov", camera.perspective.yfov, camera.perspective.defined);
        getDoubleVal(jsPerspective, "zfar", camera.perspective.zfar, camera.perspective.defined);
        getDoubleVal(jsPerspective, "znear", camera.perspective.znear, camera.perspective.defined);
        camera.type = GLTFCameraTypes::PERSPECTIVE;
    } else if (getObjectVal(object, "orthographic", jsOrthographic, camera.defined)) {
        getDoubleVal(jsOrthographic, "zfar", camera.orthographic.zfar, camera.orthographic.defined);
        getDoubleVal(jsOrthographic, "znear", camera.orthographic.znear, camera.orthographic.defined);
        getDoubleVal(jsOrthographic, "xmag", camera.orthographic.xmag, camera.orthographic.defined);
        getDoubleVal(jsOrthographic, "ymag", camera.orthographic.ymag, camera.orthographic.defined);
        camera.type = GLTFCameraTypes::ORTHOGRAPHIC;
    } else if (getStringVal(object, "type", type, camera.defined)) {
        camera.type = getCameraType(type);
    }
    
    _file.cameras.push_back(camera);
    
    return true;
}

bool GLTFSerializer::addImage(const QJsonObject& object) {
    GLTFImage image;
    
    QString mime;
    getStringVal(object, "uri", image.uri, image.defined);
    if (getStringVal(object, "mimeType", mime, image.defined)) {
        image.mimeType = getImageMimeType(mime);
    }
    getIntVal(object, "bufferView", image.bufferView, image.defined);
    
    _file.images.push_back(image);

    return true;
}

bool GLTFSerializer::getIndexFromObject(const QJsonObject& object, const QString& field,
                                    int& outidx, QMap<QString, bool>& defined) {
    QJsonObject subobject;
    if (getObjectVal(object, field, subobject, defined)) {
        QMap<QString, bool> tmpdefined = QMap<QString, bool>();
        return getIntVal(subobject, "index", outidx, tmpdefined);
    }
    return false;
}

bool GLTFSerializer::addMaterial(const QJsonObject& object) {
    GLTFMaterial material;

    getStringVal(object, "name", material.name, material.defined);
    getDoubleArrayVal(object, "emissiveFactor", material.emissiveFactor, material.defined);
    getIndexFromObject(object, "emissiveTexture", material.emissiveTexture, material.defined);
    getIndexFromObject(object, "normalTexture", material.normalTexture, material.defined);
    getIndexFromObject(object, "occlusionTexture", material.occlusionTexture, material.defined);
    getBoolVal(object, "doubleSided", material.doubleSided, material.defined);
    QString alphamode;
    if (getStringVal(object, "alphaMode", alphamode, material.defined)) {
        material.alphaMode = getMaterialAlphaMode(alphamode);
    }
    getDoubleVal(object, "alphaCutoff", material.alphaCutoff, material.defined);
    QJsonObject jsMetallicRoughness;
    if (getObjectVal(object, "pbrMetallicRoughness", jsMetallicRoughness, material.defined)) {
        getDoubleArrayVal(jsMetallicRoughness, "baseColorFactor", 
                          material.pbrMetallicRoughness.baseColorFactor, 
                          material.pbrMetallicRoughness.defined);
        getIndexFromObject(jsMetallicRoughness, "baseColorTexture", 
                           material.pbrMetallicRoughness.baseColorTexture, 
                           material.pbrMetallicRoughness.defined);
        getDoubleVal(jsMetallicRoughness, "metallicFactor", 
                     material.pbrMetallicRoughness.metallicFactor, 
                     material.pbrMetallicRoughness.defined);
        getDoubleVal(jsMetallicRoughness, "roughnessFactor", 
                     material.pbrMetallicRoughness.roughnessFactor, 
                     material.pbrMetallicRoughness.defined);
        getIndexFromObject(jsMetallicRoughness, "metallicRoughnessTexture", 
                           material.pbrMetallicRoughness.metallicRoughnessTexture, 
                           material.pbrMetallicRoughness.defined);
    }
   _file.materials.push_back(material);
    return true;
}

bool GLTFSerializer::addMesh(const QJsonObject& object) {
    GLTFMesh mesh;

    getStringVal(object, "name", mesh.name, mesh.defined);
    getDoubleArrayVal(object, "weights", mesh.weights, mesh.defined);
    QJsonArray jsPrimitives;
    object.keys();
    if (getObjectArrayVal(object, "primitives", jsPrimitives, mesh.defined)) {
        foreach(const QJsonValue & prim, jsPrimitives) {
            if (prim.isObject()) {
                GLTFMeshPrimitive primitive;
                QJsonObject jsPrimitive = prim.toObject();
                getIntVal(jsPrimitive, "mode", primitive.mode, primitive.defined);
                getIntVal(jsPrimitive, "indices", primitive.indices, primitive.defined);
                getIntVal(jsPrimitive, "material", primitive.material, primitive.defined);
                
                QJsonObject jsAttributes;
                if (getObjectVal(jsPrimitive, "attributes", jsAttributes, primitive.defined)) {
                    QStringList attrKeys = jsAttributes.keys();
                    foreach(const QString & attrKey, attrKeys) {
                        int attrVal;
                        getIntVal(jsAttributes, attrKey, attrVal, primitive.attributes.defined);
                        primitive.attributes.values.insert(attrKey, attrVal);
                    }
                }

                QJsonArray jsTargets;
                if (getObjectArrayVal(jsPrimitive, "targets", jsTargets, primitive.defined))
                {
                    foreach(const QJsonValue & tar, jsTargets) {
                        if (tar.isObject()) {
                            QJsonObject jsTarget = tar.toObject();
                            QStringList tarKeys = jsTarget.keys();
                            GLTFMeshPrimitiveAttr target;
                            foreach(const QString & tarKey, tarKeys) {
                                int tarVal;
                                getIntVal(jsAttributes, tarKey, tarVal, target.defined);
                                target.values.insert(tarKey, tarVal);
                            }
                            primitive.targets.push_back(target);
                        }
                    }
                }                
                mesh.primitives.push_back(primitive);
            }
        }

    }

    _file.meshes.push_back(mesh);

    return true;
}

bool GLTFSerializer::addNode(const QJsonObject& object) {
    GLTFNode node;
    
    getStringVal(object, "name", node.name, node.defined);
    getIntVal(object, "camera", node.camera, node.defined);
    getIntVal(object, "mesh", node.mesh, node.defined);
    getIntArrayVal(object, "children", node.children, node.defined);
    getDoubleArrayVal(object, "translation", node.translation, node.defined);
    getDoubleArrayVal(object, "rotation", node.rotation, node.defined);
    getDoubleArrayVal(object, "scale", node.scale, node.defined);
    getDoubleArrayVal(object, "matrix", node.matrix, node.defined);
    getIntVal(object, "skin", node.skin, node.defined);
    getStringVal(object, "jointName", node.jointName, node.defined);
    getIntArrayVal(object, "skeletons", node.skeletons, node.defined);

    _file.nodes.push_back(node);

    return true;
}

bool GLTFSerializer::addSampler(const QJsonObject& object) {
    GLTFSampler sampler;

    getIntVal(object, "magFilter", sampler.magFilter, sampler.defined);
    getIntVal(object, "minFilter", sampler.minFilter, sampler.defined);
    getIntVal(object, "wrapS", sampler.wrapS, sampler.defined);
    getIntVal(object, "wrapT", sampler.wrapT, sampler.defined);

    _file.samplers.push_back(sampler);

    return true;

}

bool GLTFSerializer::addScene(const QJsonObject& object) {
    GLTFScene scene;

    getStringVal(object, "name", scene.name, scene.defined);
    getIntArrayVal(object, "nodes", scene.nodes, scene.defined);

    _file.scenes.push_back(scene);
    return true;
}

bool GLTFSerializer::addSkin(const QJsonObject& object) {
    GLTFSkin skin;

    getIntVal(object, "inverseBindMatrices", skin.inverseBindMatrices, skin.defined);
    getIntVal(object, "skeleton", skin.skeleton, skin.defined);
    getIntArrayVal(object, "joints", skin.joints, skin.defined);

    _file.skins.push_back(skin);

    return true;
}

bool GLTFSerializer::addTexture(const QJsonObject& object) {
    GLTFTexture texture; 
    getIntVal(object, "sampler", texture.sampler, texture.defined);
    getIntVal(object, "source", texture.source, texture.defined);
    
    _file.textures.push_back(texture);

    return true;
}

bool GLTFSerializer::parseGLTF(const QByteArray& data) {
    PROFILE_RANGE_EX(resource_parse, __FUNCTION__, 0xffff0000, nullptr);
    
    QJsonDocument d = QJsonDocument::fromJson(data);
    QJsonObject jsFile = d.object();
    bool success = setAsset(jsFile);
    if (success) {
        QJsonArray accessors;
        if (getObjectArrayVal(jsFile, "accessors", accessors, _file.defined)) {
            foreach(const QJsonValue & accVal, accessors) {
                if (accVal.isObject()) {
                    success = success && addAccessor(accVal.toObject());
                }
            }
        }

        QJsonArray animations;
        if (getObjectArrayVal(jsFile, "animations", animations, _file.defined)) {
            foreach(const QJsonValue & animVal, accessors) {
                if (animVal.isObject()) {
                    success = success && addAnimation(animVal.toObject());
                }
            }
        }

        QJsonArray bufferViews;
        if (getObjectArrayVal(jsFile, "bufferViews", bufferViews, _file.defined)) {
            foreach(const QJsonValue & bufviewVal, bufferViews) {
                if (bufviewVal.isObject()) {
                    success = success && addBufferView(bufviewVal.toObject());
                }
            }
        }

        QJsonArray buffers;
        if (getObjectArrayVal(jsFile, "buffers", buffers, _file.defined)) {
            foreach(const QJsonValue & bufVal, buffers) {
                if (bufVal.isObject()) {
                    success = success && addBuffer(bufVal.toObject());
                }
            }
        }

        QJsonArray cameras;
        if (getObjectArrayVal(jsFile, "cameras", cameras, _file.defined)) {
            foreach(const QJsonValue & camVal, cameras) {
                if (camVal.isObject()) {
                    success = success && addCamera(camVal.toObject());
                }
            }
        }

        QJsonArray images;
        if (getObjectArrayVal(jsFile, "images", images, _file.defined)) {
            foreach(const QJsonValue & imgVal, images) {
                if (imgVal.isObject()) {
                    success = success && addImage(imgVal.toObject());
                }
            }
        }

        QJsonArray materials;
        if (getObjectArrayVal(jsFile, "materials", materials, _file.defined)) {
            foreach(const QJsonValue & matVal, materials) {
                if (matVal.isObject()) {
                    success = success && addMaterial(matVal.toObject());
                }
            }
        }

        QJsonArray meshes;
        if (getObjectArrayVal(jsFile, "meshes", meshes, _file.defined)) {
            foreach(const QJsonValue & meshVal, meshes) {
                if (meshVal.isObject()) {
                    success = success && addMesh(meshVal.toObject());
                }
            }
        }

        QJsonArray nodes;
        if (getObjectArrayVal(jsFile, "nodes", nodes, _file.defined)) {
            foreach(const QJsonValue & nodeVal, nodes) {
                if (nodeVal.isObject()) {
                    success = success && addNode(nodeVal.toObject());
                }
            }
        }

        QJsonArray samplers;
        if (getObjectArrayVal(jsFile, "samplers", samplers, _file.defined)) {
            foreach(const QJsonValue & samVal, samplers) {
                if (samVal.isObject()) {
                    success = success && addSampler(samVal.toObject());
                }
            }
        }

        QJsonArray scenes;
        if (getObjectArrayVal(jsFile, "scenes", scenes, _file.defined)) {
            foreach(const QJsonValue & sceneVal, scenes) {
                if (sceneVal.isObject()) {
                    success = success && addScene(sceneVal.toObject());
                }
            }
        }

        QJsonArray skins;
        if (getObjectArrayVal(jsFile, "skins", skins, _file.defined)) {
            foreach(const QJsonValue & skinVal, skins) {
                if (skinVal.isObject()) {
                    success = success && addSkin(skinVal.toObject());
                }
            }
        }

        QJsonArray textures;
        if (getObjectArrayVal(jsFile, "textures", textures, _file.defined)) {
            foreach(const QJsonValue & texVal, textures) {
                if (texVal.isObject()) {
                    success = success && addTexture(texVal.toObject());
                }
            }
        }
    } 
    return success;
}

glm::mat4 GLTFSerializer::getModelTransform(const GLTFNode& node) {
    glm::mat4 tmat = glm::mat4(1.0);

    if (node.defined["matrix"] && node.matrix.size() == 16) {
        tmat = glm::mat4(node.matrix[0], node.matrix[1], node.matrix[2], node.matrix[3],
            node.matrix[4], node.matrix[5], node.matrix[6], node.matrix[7],
            node.matrix[8], node.matrix[9], node.matrix[10], node.matrix[11],
            node.matrix[12], node.matrix[13], node.matrix[14], node.matrix[15]);
    } else {

        if (node.defined["rotation"] && node.rotation.size() == 4) {
            //quat(x,y,z,w) to quat(w,x,y,z)
            glm::quat rotquat = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            tmat = glm::mat4_cast(rotquat) * tmat;
        }

        if (node.defined["scale"] && node.scale.size() == 3) {
            glm::vec3 scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
            glm::mat4 s = glm::mat4(1.0);
            s = glm::scale(s, scale);
            tmat = s * tmat;
        }

        if (node.defined["translation"] && node.translation.size() == 3) {
            glm::vec3 trans = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
            glm::mat4 t = glm::mat4(1.0);
            t = glm::translate(t, trans);
            tmat = t * tmat;
        }
    }
    return tmat;
}

bool GLTFSerializer::buildGeometry(HFMModel& hfmModel, const QUrl& url) {

    //Build dependencies
    QVector<QVector<int>> nodeDependencies(_file.nodes.size());
    int nodecount = 0;
    foreach(auto &node, _file.nodes) {
        //nodes_transforms.push_back(getModelTransform(node));
        foreach(int child, node.children) nodeDependencies[child].push_back(nodecount);
        nodecount++;
    }
    
    nodecount = 0;
    foreach(auto &node, _file.nodes) {
        // collect node transform
        _file.nodes[nodecount].transforms.push_back(getModelTransform(node)); 
        if (nodeDependencies[nodecount].size() == 1) {
            int parentidx = nodeDependencies[nodecount][0];
            while (true) { // iterate parents
                // collect parents transforms
                _file.nodes[nodecount].transforms.push_back(getModelTransform(_file.nodes[parentidx])); 
                if (nodeDependencies[parentidx].size() == 1) {
                    parentidx = nodeDependencies[parentidx][0];
                } else break;
            }
        }
        
        nodecount++;
    }
    
    //Build default joints
    hfmModel.joints.resize(1);
    hfmModel.joints[0].isFree = false;
    hfmModel.joints[0].parentIndex = -1;
    hfmModel.joints[0].distanceToParent = 0;
    hfmModel.joints[0].translation = glm::vec3(0, 0, 0);
    hfmModel.joints[0].rotationMin = glm::vec3(0, 0, 0);
    hfmModel.joints[0].rotationMax = glm::vec3(0, 0, 0);
    hfmModel.joints[0].name = "OBJ";
    hfmModel.joints[0].isSkeletonJoint = true;

    hfmModel.jointIndices["x"] = 1;

    //Build materials
    QVector<QString> materialIDs;
    QString unknown = "Default";
    int ukcount = 0;
    foreach(auto material, _file.materials) {
        QString mid = (material.defined["name"]) ? material.name : unknown + ukcount++;
        materialIDs.push_back(mid);
    }

    for (int i = 0; i < materialIDs.size(); i++) {
        QString& matid = materialIDs[i];
        hfmModel.materials[matid] = HFMMaterial();
        HFMMaterial& hfmMaterial = hfmModel.materials[matid];
        hfmMaterial._material = std::make_shared<graphics::Material>();
        setHFMMaterial(hfmMaterial, _file.materials[i]);
    }

    

    nodecount = 0;
    // Build meshes
    foreach(auto &node, _file.nodes) {

        if (node.defined["mesh"]) {
            qCDebug(modelformat) << "node_transforms" << node.transforms;
            foreach(auto &primitive, _file.meshes[node.mesh].primitives) {
                hfmModel.meshes.append(HFMMesh());
                HFMMesh& mesh = hfmModel.meshes[hfmModel.meshes.size() - 1];
                HFMCluster cluster;
                cluster.jointIndex = 0;
                cluster.inverseBindMatrix = glm::mat4(1, 0, 0, 0,
                    0, 1, 0, 0,
                    0, 0, 1, 0,
                    0, 0, 0, 1);
                mesh.clusters.append(cluster);

                HFMMeshPart part = HFMMeshPart();

                int indicesAccessorIdx = primitive.indices;

                GLTFAccessor& indicesAccessor = _file.accessors[indicesAccessorIdx];
                GLTFBufferView& indicesBufferview = _file.bufferviews[indicesAccessor.bufferView];
                GLTFBuffer& indicesBuffer = _file.buffers[indicesBufferview.buffer];

                int indicesAccBoffset = indicesAccessor.defined["byteOffset"] ? indicesAccessor.byteOffset : 0;

                QVector<int> raw_indices;
                QVector<glm::vec3> raw_vertices;
                QVector<glm::vec3> raw_normals;

                bool success = addArrayOfType(indicesBuffer.blob, 
                    indicesBufferview.byteOffset + indicesAccBoffset, 
                    indicesAccessor.count, 
                    part.triangleIndices, 
                    indicesAccessor.type, 
                    indicesAccessor.componentType);

                if (!success) {
                    qWarning(modelformat) << "There was a problem reading glTF INDICES data for model " << _url;
                    continue;
                }

                QList<QString> keys = primitive.attributes.values.keys();

                foreach(auto &key, keys) {
                    int accessorIdx = primitive.attributes.values[key];

                    GLTFAccessor& accessor = _file.accessors[accessorIdx];
                    GLTFBufferView& bufferview = _file.bufferviews[accessor.bufferView];
                    GLTFBuffer& buffer = _file.buffers[bufferview.buffer];

                    int accBoffset = accessor.defined["byteOffset"] ? accessor.byteOffset : 0;
                    if (key == "POSITION") {
                        QVector<float> vertices;
                        success = addArrayOfType(buffer.blob, 
                            bufferview.byteOffset + accBoffset, 
                            accessor.count, vertices, 
                            accessor.type, 
                            accessor.componentType);
                        if (!success) {
                            qWarning(modelformat) << "There was a problem reading glTF POSITION data for model " << _url;
                            continue;
                        }
                        for (int n = 0; n < vertices.size(); n = n + 3) {
                            mesh.vertices.push_back(glm::vec3(vertices[n], vertices[n + 1], vertices[n + 2]));
                        }
                    } else if (key == "NORMAL") {
                        QVector<float> normals;
                        success = addArrayOfType(buffer.blob, 
                            bufferview.byteOffset + accBoffset, 
                            accessor.count, 
                            normals, 
                            accessor.type, 
                            accessor.componentType);
                        if (!success) {
                            qWarning(modelformat) << "There was a problem reading glTF NORMAL data for model " << _url;
                            continue;
                        }
                        for (int n = 0; n < normals.size(); n = n + 3) {
                            mesh.normals.push_back(glm::vec3(normals[n], normals[n + 1], normals[n + 2]));
                        }
                    } else if (key == "TEXCOORD_0") {
                        QVector<float> texcoords;
                        success = addArrayOfType(buffer.blob, 
                            bufferview.byteOffset + accBoffset, 
                            accessor.count, 
                            texcoords, 
                            accessor.type, 
                            accessor.componentType);
                        if (!success) {
                            qWarning(modelformat) << "There was a problem reading glTF TEXCOORD_0 data for model " << _url;
                            continue;
                        }
                        for (int n = 0; n < texcoords.size(); n = n + 2) {
                            mesh.texCoords.push_back(glm::vec2(texcoords[n], texcoords[n + 1]));
                        }
                    } else if (key == "TEXCOORD_1") {
                        QVector<float> texcoords;
                        success = addArrayOfType(buffer.blob, 
                            bufferview.byteOffset + accBoffset, 
                            accessor.count, 
                            texcoords, 
                            accessor.type, 
                            accessor.componentType);
                        if (!success) {
                            qWarning(modelformat) << "There was a problem reading glTF TEXCOORD_1 data for model " << _url;
                            continue;
                        }
                        for (int n = 0; n < texcoords.size(); n = n + 2) {
                            mesh.texCoords1.push_back(glm::vec2(texcoords[n], texcoords[n + 1]));
                        }
                    }

                }

                if (primitive.defined["material"]) {
                    part.materialID = materialIDs[primitive.material];
                }
                mesh.parts.push_back(part);

                // populate the texture coordenates if they don't exist
                if (mesh.texCoords.size() == 0) {
                    for (int i = 0; i < part.triangleIndices.size(); i++) mesh.texCoords.push_back(glm::vec2(0.0, 1.0));
                }
                mesh.meshExtents.reset();
                foreach(const glm::vec3& vertex, mesh.vertices) {
                    mesh.meshExtents.addPoint(vertex);
                    hfmModel.meshExtents.addPoint(vertex);
                }
                
                // since mesh.modelTransform seems to not have any effect I apply the transformation the model 
                for (int h = 0; h < mesh.vertices.size(); h++) {
                    glm::vec4 ver = glm::vec4(mesh.vertices[h], 1);
                    if (node.transforms.size() > 0) {
                        ver = node.transforms[0] * ver; // for model dependency should multiply also by parents transforms?
                        mesh.vertices[h] = glm::vec3(ver[0], ver[1], ver[2]);
                    }
                }

                mesh.meshIndex = hfmModel.meshes.size();
            }
            
        }
        nodecount++;
    }

    
    return true;
}

MediaType GLTFSerializer::getMediaType() const {
    MediaType mediaType("gltf");
    mediaType.extensions.push_back("gltf");
    mediaType.webMediaTypes.push_back("model/gltf+json");
    return mediaType;
}

std::unique_ptr<hfm::Serializer::Factory> GLTFSerializer::getFactory() const {
    return std::make_unique<hfm::Serializer::SimpleFactory<GLTFSerializer>>();
}

HFMModel::Pointer GLTFSerializer::read(const QByteArray& data, const QVariantHash& mapping, const QUrl& url) {
    
    _url = url;

    // Normalize url for local files
    QUrl normalizeUrl = DependencyManager::get<ResourceManager>()->normalizeURL(_url);
    if (normalizeUrl.scheme().isEmpty() || (normalizeUrl.scheme() == "file")) {
        QString localFileName = PathUtils::expandToLocalDataAbsolutePath(normalizeUrl).toLocalFile();
        _url = QUrl(QFileInfo(localFileName).absoluteFilePath());
    }

    if (parseGLTF(data)) {
        //_file.dump();
        auto hfmModelPtr = std::make_shared<HFMModel>();
        HFMModel& hfmModel = *hfmModelPtr;

        buildGeometry(hfmModel, _url);

        //hfmDebugDump(data);
        return hfmModelPtr;
    } else {
        qCDebug(modelformat) << "Error parsing GLTF file.";
    }

    return nullptr;
}

bool GLTFSerializer::readBinary(const QString& url, QByteArray& outdata) {
    QUrl binaryUrl = _url.resolved(url);

    bool success;
    std::tie<bool, QByteArray>(success, outdata) = requestData(binaryUrl);
    
    return success;
}

bool GLTFSerializer::doesResourceExist(const QString& url) {
    if (_url.isEmpty()) {
        return false;
    }
    QUrl candidateUrl = _url.resolved(url);
    return DependencyManager::get<ResourceManager>()->resourceExists(candidateUrl);
}

std::tuple<bool, QByteArray> GLTFSerializer::requestData(QUrl& url) {
    auto request = DependencyManager::get<ResourceManager>()->createResourceRequest(
        nullptr, url, true, -1, "GLTFSerializer::requestData");

    if (!request) {
        return std::make_tuple(false, QByteArray());
    }

    QEventLoop loop;
    QObject::connect(request, &ResourceRequest::finished, &loop, &QEventLoop::quit);
    request->send();
    loop.exec();

    if (request->getResult() == ResourceRequest::Success) {
        return std::make_tuple(true, request->getData());
    } else {
        return std::make_tuple(false, QByteArray());
    }
}


QNetworkReply* GLTFSerializer::request(QUrl& url, bool isTest) {
    if (!qApp) {
        return nullptr;
    }
    bool aboutToQuit{ false };
    auto connection = QObject::connect(qApp, &QCoreApplication::aboutToQuit, [&] {
        aboutToQuit = true;
    });
    QNetworkAccessManager& networkAccessManager = NetworkAccessManager::getInstance();
    QNetworkRequest netRequest(url);
    netRequest.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    QNetworkReply* netReply = isTest ? networkAccessManager.head(netRequest) : networkAccessManager.get(netRequest);
    if (!qApp || aboutToQuit) {
        netReply->deleteLater();
        return nullptr;
    }
    QEventLoop loop; // Create an event loop that will quit when we get the finished signal
    QObject::connect(netReply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();                    // Nothing is going to happen on this whole run thread until we get this

    QObject::disconnect(connection);
    return netReply;                // trying to sync later on.
}

HFMTexture GLTFSerializer::getHFMTexture(const GLTFTexture& texture) {
    HFMTexture fbxtex = HFMTexture();
    fbxtex.texcoordSet = 0;
    
    if (texture.defined["source"]) {
        QString url = _file.images[texture.source].uri;
        QString fname = QUrl(url).fileName();
        QUrl textureUrl = _url.resolved(url);
        qCDebug(modelformat) << "fname: " << fname;
        fbxtex.name = fname;
        fbxtex.filename = textureUrl.toEncoded();
    }
    return fbxtex;
}

void GLTFSerializer::setHFMMaterial(HFMMaterial& fbxmat, const GLTFMaterial& material) {


    if (material.defined["name"]) {
        fbxmat.name = fbxmat.materialID = material.name;
    }
    
    if (material.defined["emissiveFactor"] && material.emissiveFactor.size() == 3) {
        glm::vec3 emissive = glm::vec3(material.emissiveFactor[0], 
                                       material.emissiveFactor[1], 
                                       material.emissiveFactor[2]);
        fbxmat._material->setEmissive(emissive);
    }

    if (material.defined["emissiveTexture"]) {
        fbxmat.emissiveTexture = getHFMTexture(_file.textures[material.emissiveTexture]);
        fbxmat.useEmissiveMap = true;
    }
    
    if (material.defined["normalTexture"]) {
        fbxmat.normalTexture = getHFMTexture(_file.textures[material.normalTexture]);
        fbxmat.useNormalMap = true;
    }
    
    if (material.defined["occlusionTexture"]) {
        fbxmat.occlusionTexture = getHFMTexture(_file.textures[material.occlusionTexture]);
        fbxmat.useOcclusionMap = true;
    }

    if (material.defined["pbrMetallicRoughness"]) {
        fbxmat.isPBSMaterial = true;
        
        if (material.pbrMetallicRoughness.defined["metallicFactor"]) {
            fbxmat.metallic = material.pbrMetallicRoughness.metallicFactor;
        }
        if (material.pbrMetallicRoughness.defined["baseColorTexture"]) {
            fbxmat.opacityTexture = getHFMTexture(_file.textures[material.pbrMetallicRoughness.baseColorTexture]);
            fbxmat.albedoTexture = getHFMTexture(_file.textures[material.pbrMetallicRoughness.baseColorTexture]);
            fbxmat.useAlbedoMap = true;
        }
        if (material.pbrMetallicRoughness.defined["metallicRoughnessTexture"]) {
            fbxmat.roughnessTexture = getHFMTexture(_file.textures[material.pbrMetallicRoughness.metallicRoughnessTexture]);
            fbxmat.useRoughnessMap = true;
            fbxmat.metallicTexture = getHFMTexture(_file.textures[material.pbrMetallicRoughness.metallicRoughnessTexture]);
            fbxmat.useMetallicMap = true;
        }
        if (material.pbrMetallicRoughness.defined["roughnessFactor"]) {
            fbxmat._material->setRoughness(material.pbrMetallicRoughness.roughnessFactor);
        }
        if (material.pbrMetallicRoughness.defined["baseColorFactor"] && 
            material.pbrMetallicRoughness.baseColorFactor.size() == 4) {
            glm::vec3 dcolor =  glm::vec3(material.pbrMetallicRoughness.baseColorFactor[0], 
                                          material.pbrMetallicRoughness.baseColorFactor[1], 
                                          material.pbrMetallicRoughness.baseColorFactor[2]);
            fbxmat.diffuseColor = dcolor;
            fbxmat._material->setAlbedo(dcolor);
            fbxmat._material->setOpacity(material.pbrMetallicRoughness.baseColorFactor[3]);
        }   
    }

}

template<typename T, typename L>
bool GLTFSerializer::readArray(const QByteArray& bin, int byteOffset, int count,
                           QVector<L>& outarray, int accessorType) {
    
    QDataStream blobstream(bin);
    blobstream.setByteOrder(QDataStream::LittleEndian);
    blobstream.setVersion(QDataStream::Qt_5_9);
    blobstream.setFloatingPointPrecision(QDataStream::FloatingPointPrecision::SinglePrecision);

    qCDebug(modelformat) << "size1: " << count;
    int dataskipped = blobstream.skipRawData(byteOffset);
    qCDebug(modelformat) << "dataskipped: " << dataskipped;

    int bufferCount = 0;
    switch (accessorType) {
    case GLTFAccessorType::SCALAR:
        bufferCount = 1;
        break;
    case GLTFAccessorType::VEC2:
        bufferCount = 2;
        break;
    case GLTFAccessorType::VEC3:
        bufferCount = 3;
        break;
    case GLTFAccessorType::VEC4:
        bufferCount = 4;
        break;
    case GLTFAccessorType::MAT2:
        bufferCount = 4;
        break;
    case GLTFAccessorType::MAT3:
        bufferCount = 9;
        break;
    case GLTFAccessorType::MAT4:
        bufferCount = 16;
        break;
    default:
        qWarning(modelformat) << "Unknown accessorType: " << accessorType;
        blobstream.unsetDevice();
        return false;
    }
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < bufferCount; j++) {
            if (!blobstream.atEnd()) {
                T value;
                blobstream >> value;
                outarray.push_back(value);
            } else {
                blobstream.unsetDevice();
                return false;
            }
        }
    }

    blobstream.unsetDevice();
    return true;
}
template<typename T>
bool GLTFSerializer::addArrayOfType(const QByteArray& bin, int byteOffset, int count,
                                QVector<T>& outarray, int accessorType, int componentType) {
    
    switch (componentType) {
    case GLTFAccessorComponentType::BYTE: {}
    case GLTFAccessorComponentType::UNSIGNED_BYTE: {
        return readArray<uchar>(bin, byteOffset, count, outarray, accessorType);
    }
    case GLTFAccessorComponentType::SHORT: {
        return readArray<short>(bin, byteOffset, count, outarray, accessorType);
    }
    case GLTFAccessorComponentType::UNSIGNED_INT: {
        return readArray<uint>(bin, byteOffset, count, outarray, accessorType);
    }
    case GLTFAccessorComponentType::UNSIGNED_SHORT: {
        return readArray<ushort>(bin, byteOffset, count, outarray, accessorType);
    }
    case GLTFAccessorComponentType::FLOAT: {
        return readArray<float>(bin, byteOffset, count, outarray, accessorType);
    }
    }
    return false;
}

void GLTFSerializer::retriangulate(const QVector<int>& inIndices, const QVector<glm::vec3>& in_vertices,
                               const QVector<glm::vec3>& in_normals, QVector<int>& outIndices, 
                               QVector<glm::vec3>& out_vertices, QVector<glm::vec3>& out_normals) {
    for (int i = 0; i < inIndices.size(); i = i + 3) {

        int idx1 = inIndices[i];
        int idx2 = inIndices[i+1];
        int idx3 = inIndices[i+2];

        out_vertices.push_back(in_vertices[idx1]);
        out_vertices.push_back(in_vertices[idx2]);
        out_vertices.push_back(in_vertices[idx3]);

        out_normals.push_back(in_normals[idx1]);
        out_normals.push_back(in_normals[idx2]);
        out_normals.push_back(in_normals[idx3]);

        outIndices.push_back(i);
        outIndices.push_back(i+1);
        outIndices.push_back(i+2);
    }
}

void GLTFSerializer::hfmDebugDump(const HFMModel& hfmModel) {
    qCDebug(modelformat) << "---------------- hfmModel ----------------";
    qCDebug(modelformat) << "  hasSkeletonJoints =" << hfmModel.hasSkeletonJoints;
    qCDebug(modelformat) << "  offset =" << hfmModel.offset;

    qCDebug(modelformat) << "  palmDirection = " << hfmModel.palmDirection;

    qCDebug(modelformat) << "  neckPivot = " << hfmModel.neckPivot;

    qCDebug(modelformat) << "  bindExtents.size() = " << hfmModel.bindExtents.size();
    qCDebug(modelformat) << "  meshExtents.size() = " << hfmModel.meshExtents.size();

    qCDebug(modelformat) << "  jointIndices.size() =" << hfmModel.jointIndices.size();
    qCDebug(modelformat) << "  joints.count() =" << hfmModel.joints.count();
    qCDebug(modelformat) << "---------------- Meshes ----------------";
    qCDebug(modelformat) << "  meshes.count() =" << hfmModel.meshes.count();
    qCDebug(modelformat) << "  blendshapeChannelNames = " << hfmModel.blendshapeChannelNames;
    foreach(HFMMesh mesh, hfmModel.meshes) {
        qCDebug(modelformat) << "\n";
        qCDebug(modelformat) << "    meshpointer =" << mesh._mesh.get();
        qCDebug(modelformat) << "    meshindex =" << mesh.meshIndex;
        qCDebug(modelformat) << "    vertices.count() =" << mesh.vertices.size();
        qCDebug(modelformat) << "    colors.count() =" << mesh.colors.count();
        qCDebug(modelformat) << "    normals.count() =" << mesh.normals.size();
        qCDebug(modelformat) << "    tangents.count() =" << mesh.tangents.size();
        qCDebug(modelformat) << "    colors.count() =" << mesh.colors.count();
        qCDebug(modelformat) << "    texCoords.count() =" << mesh.texCoords.count();
        qCDebug(modelformat) << "    texCoords1.count() =" << mesh.texCoords1.count();
        qCDebug(modelformat) << "    clusterIndices.count() =" << mesh.clusterIndices.count();
        qCDebug(modelformat) << "    clusterWeights.count() =" << mesh.clusterWeights.count();
        qCDebug(modelformat) << "    modelTransform =" << mesh.modelTransform;
        qCDebug(modelformat) << "    parts.count() =" << mesh.parts.count();
        qCDebug(modelformat) << "---------------- Meshes (blendshapes)--------";
        foreach(HFMBlendshape bshape, mesh.blendshapes) {
            qCDebug(modelformat) << "\n";
            qCDebug(modelformat) << "    bshape.indices.count() =" << bshape.indices.count();
            qCDebug(modelformat) << "    bshape.vertices.count() =" << bshape.vertices.count();
            qCDebug(modelformat) << "    bshape.normals.count() =" << bshape.normals.count();
            qCDebug(modelformat) << "\n";
        }
        qCDebug(modelformat) << "---------------- Meshes (meshparts)--------";
        foreach(HFMMeshPart meshPart, mesh.parts) {
            qCDebug(modelformat) << "\n";
            qCDebug(modelformat) << "        quadIndices.count() =" << meshPart.quadIndices.count();
            qCDebug(modelformat) << "        triangleIndices.count() =" << meshPart.triangleIndices.count();
            qCDebug(modelformat) << "        materialID =" << meshPart.materialID;
            qCDebug(modelformat) << "\n";

        }
        qCDebug(modelformat) << "---------------- Meshes (clusters)--------";
        qCDebug(modelformat) << "    clusters.count() =" << mesh.clusters.count();
        foreach(HFMCluster cluster, mesh.clusters) {
            qCDebug(modelformat) << "\n";
            qCDebug(modelformat) << "        jointIndex =" << cluster.jointIndex;
            qCDebug(modelformat) << "        inverseBindMatrix =" << cluster.inverseBindMatrix;
            qCDebug(modelformat) << "\n";
        }
        qCDebug(modelformat) << "\n";
    }
    qCDebug(modelformat) << "---------------- AnimationFrames ----------------";
    foreach(HFMAnimationFrame anim, hfmModel.animationFrames) {
        qCDebug(modelformat) << "  anim.translations = " << anim.translations;
        qCDebug(modelformat) << "  anim.rotations = " << anim.rotations;
    }
    QList<int> mitomona_keys = hfmModel.meshIndicesToModelNames.keys();
    foreach(int key, mitomona_keys) {
        qCDebug(modelformat) << "    meshIndicesToModelNames key =" << key << "  val =" << hfmModel.meshIndicesToModelNames[key];
    }

    qCDebug(modelformat) << "---------------- Materials ----------------";

    foreach(HFMMaterial mat, hfmModel.materials) {
        qCDebug(modelformat) << "\n";
        qCDebug(modelformat) << "  mat.materialID =" << mat.materialID;
        qCDebug(modelformat) << "  diffuseColor =" << mat.diffuseColor;
        qCDebug(modelformat) << "  diffuseFactor =" << mat.diffuseFactor;
        qCDebug(modelformat) << "  specularColor =" << mat.specularColor;
        qCDebug(modelformat) << "  specularFactor =" << mat.specularFactor;
        qCDebug(modelformat) << "  emissiveColor =" << mat.emissiveColor;
        qCDebug(modelformat) << "  emissiveFactor =" << mat.emissiveFactor;
        qCDebug(modelformat) << "  shininess =" << mat.shininess;
        qCDebug(modelformat) << "  opacity =" << mat.opacity;
        qCDebug(modelformat) << "  metallic =" << mat.metallic;
        qCDebug(modelformat) << "  roughness =" << mat.roughness;
        qCDebug(modelformat) << "  emissiveIntensity =" << mat.emissiveIntensity;
        qCDebug(modelformat) << "  ambientFactor =" << mat.ambientFactor;

        qCDebug(modelformat) << "  materialID =" << mat.materialID;
        qCDebug(modelformat) << "  name =" << mat.name;
        qCDebug(modelformat) << "  shadingModel =" << mat.shadingModel;
        qCDebug(modelformat) << "  _material =" << mat._material.get();

        qCDebug(modelformat) << "  normalTexture =" << mat.normalTexture.filename;
        qCDebug(modelformat) << "  albedoTexture =" << mat.albedoTexture.filename;
        qCDebug(modelformat) << "  opacityTexture =" << mat.opacityTexture.filename;

        qCDebug(modelformat) << "  lightmapParams =" << mat.lightmapParams;

        qCDebug(modelformat) << "  isPBSMaterial =" << mat.isPBSMaterial;
        qCDebug(modelformat) << "  useNormalMap =" << mat.useNormalMap;
        qCDebug(modelformat) << "  useAlbedoMap =" << mat.useAlbedoMap;
        qCDebug(modelformat) << "  useOpacityMap =" << mat.useOpacityMap;
        qCDebug(modelformat) << "  useRoughnessMap =" << mat.useRoughnessMap;
        qCDebug(modelformat) << "  useSpecularMap =" << mat.useSpecularMap;
        qCDebug(modelformat) << "  useMetallicMap =" << mat.useMetallicMap;
        qCDebug(modelformat) << "  useEmissiveMap =" << mat.useEmissiveMap;
        qCDebug(modelformat) << "  useOcclusionMap =" << mat.useOcclusionMap;
        qCDebug(modelformat) << "\n";
    }

    qCDebug(modelformat) << "---------------- Joints ----------------";

    foreach(HFMJoint joint, hfmModel.joints) {
        qCDebug(modelformat) << "\n";
        qCDebug(modelformat) << "    shapeInfo.avgPoint =" << joint.shapeInfo.avgPoint;
        qCDebug(modelformat) << "    shapeInfo.debugLines =" << joint.shapeInfo.debugLines;
        qCDebug(modelformat) << "    shapeInfo.dots =" << joint.shapeInfo.dots;
        qCDebug(modelformat) << "    shapeInfo.points =" << joint.shapeInfo.points;

        qCDebug(modelformat) << "    isFree =" << joint.isFree;
        qCDebug(modelformat) << "    freeLineage" << joint.freeLineage;
        qCDebug(modelformat) << "    parentIndex" << joint.parentIndex;
        qCDebug(modelformat) << "    distanceToParent" << joint.distanceToParent;
        qCDebug(modelformat) << "    translation" << joint.translation;
        qCDebug(modelformat) << "    preTransform" << joint.preTransform;
        qCDebug(modelformat) << "    preRotation" << joint.preRotation;
        qCDebug(modelformat) << "    rotation" << joint.rotation;
        qCDebug(modelformat) << "    postRotation" << joint.postRotation;
        qCDebug(modelformat) << "    postTransform" << joint.postTransform;
        qCDebug(modelformat) << "    transform" << joint.transform;
        qCDebug(modelformat) << "    rotationMin" << joint.rotationMin;
        qCDebug(modelformat) << "    rotationMax" << joint.rotationMax;
        qCDebug(modelformat) << "    inverseDefaultRotation" << joint.inverseDefaultRotation;
        qCDebug(modelformat) << "    inverseBindRotation" << joint.inverseBindRotation;
        qCDebug(modelformat) << "    bindTransform" << joint.bindTransform;
        qCDebug(modelformat) << "    name" << joint.name;
        qCDebug(modelformat) << "    isSkeletonJoint" << joint.isSkeletonJoint;
        qCDebug(modelformat) << "    bindTransformFoundInCluster" << joint.hasGeometricOffset;
        qCDebug(modelformat) << "    bindTransformFoundInCluster" << joint.geometricTranslation;
        qCDebug(modelformat) << "    bindTransformFoundInCluster" << joint.geometricRotation;
        qCDebug(modelformat) << "    bindTransformFoundInCluster" << joint.geometricScaling;
        qCDebug(modelformat) << "\n";
    }

    qCDebug(modelformat) << "\n";
}
