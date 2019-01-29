//
//  FBXSerializer.cpp
//  libraries/fbx/src
//
//  Created by Andrzej Kapolka on 9/18/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "FBXSerializer.h"

#include <QBuffer>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include <FaceshiftConstants.h>

#include <hfm/ModelFormatLogging.h>

// TOOL: Uncomment the following line to enable the filtering of all the unkwnon fields of a node so we can break point easily while loading a model with problems...
//#define DEBUG_FBXSERIALIZER

using namespace std;

glm::vec3 parseVec3(const QString& string) {
    QStringList elements = string.split(',');
    if (elements.isEmpty()) {
        return glm::vec3();
    }
    glm::vec3 value;
    for (int i = 0; i < 3; i++) {
        // duplicate last value if there aren't three elements
        value[i] = elements.at(min(i, elements.size() - 1)).trimmed().toFloat();
    }
    return value;
}

enum RotationOrder {
    OrderXYZ = 0,
    OrderXZY,
    OrderYZX,
    OrderYXZ,
    OrderZXY,
    OrderZYX,
    OrderSphericXYZ
};

bool haveReportedUnhandledRotationOrder = false; // Report error only once per FBX file.

glm::vec3 convertRotationToXYZ(int rotationOrder, const glm::vec3& rotation) {
    // Convert rotation with given rotation order to have order XYZ.
    if (rotationOrder == OrderXYZ) {
        return rotation;
    }

    glm::quat xyzRotation;

    switch (rotationOrder) {
        case OrderXZY:
            xyzRotation = glm::quat(glm::radians(glm::vec3(0, rotation.y, 0)))
                * (glm::quat(glm::radians(glm::vec3(0, 0, rotation.z))) * glm::quat(glm::radians(glm::vec3(rotation.x, 0, 0))));
            break;
        case OrderYZX:
            xyzRotation = glm::quat(glm::radians(glm::vec3(rotation.x, 0, 0)))
                * (glm::quat(glm::radians(glm::vec3(0, 0, rotation.z))) * glm::quat(glm::radians(glm::vec3(0, rotation.y, 0))));
            break;
        case OrderYXZ:
            xyzRotation = glm::quat(glm::radians(glm::vec3(0, 0, rotation.z)))
                * (glm::quat(glm::radians(glm::vec3(rotation.x, 0, 0))) * glm::quat(glm::radians(glm::vec3(0, rotation.y, 0))));
            break;
        case OrderZXY:
            xyzRotation = glm::quat(glm::radians(glm::vec3(0, rotation.y, 0)))
                * (glm::quat(glm::radians(glm::vec3(rotation.x, 0, 0))) * glm::quat(glm::radians(glm::vec3(0, 0, rotation.z))));
            break;
        case OrderZYX:
            xyzRotation = glm::quat(glm::radians(glm::vec3(rotation.x, 0, 0)))
                * (glm::quat(glm::radians(glm::vec3(0, rotation.y, 0))) * glm::quat(glm::radians(glm::vec3(0, 0, rotation.z))));
            break;
        default:
            // FIXME: Handle OrderSphericXYZ.
            if (!haveReportedUnhandledRotationOrder) {
                qCDebug(modelformat) << "ERROR: Unhandled rotation order in FBX file:" << rotationOrder;
                haveReportedUnhandledRotationOrder = true;
            }
            return rotation;
    }

    return glm::degrees(safeEulerAngles(xyzRotation));
}

QString processID(const QString& id) {
    // Blender (at least) prepends a type to the ID, so strip it out
    return id.mid(id.lastIndexOf(':') + 1);
}

QString getName(const QVariantList& properties) {
    QString name;
    if (properties.size() == 3) {
        name = properties.at(1).toString();
        name = processID(name.left(name.indexOf(QChar('\0'))));
    } else {
        name = processID(properties.at(0).toString());
    }
    return name;
}

QString getID(const QVariantList& properties, int index = 0) {
    return processID(properties.at(index).toString());
}

class FBXModel {
public:
    QString name;

    int parentIndex;
    glm::vec3 translation;
    glm::mat4 preTransform;
    glm::quat preRotation;
    glm::quat rotation;
    glm::quat postRotation;
    glm::mat4 postTransform;

    glm::vec3 rotationMin;  // radians
    glm::vec3 rotationMax;  // radians

    bool hasGeometricOffset;
    glm::vec3 geometricTranslation;
    glm::quat geometricRotation;
    glm::vec3 geometricScaling;
};

glm::mat4 getGlobalTransform(const QMultiMap<QString, QString>& _connectionParentMap,
        const QHash<QString, FBXModel>& fbxModels, QString nodeID, bool mixamoHack, const QString& url) {
    glm::mat4 globalTransform;
    QVector<QString> visitedNodes; // Used to prevent following a cycle
    while (!nodeID.isNull()) {
        visitedNodes.append(nodeID); // Append each node we visit

        const FBXModel& fbxModel = fbxModels.value(nodeID);
        globalTransform = glm::translate(fbxModel.translation) * fbxModel.preTransform * glm::mat4_cast(fbxModel.preRotation *
            fbxModel.rotation * fbxModel.postRotation) * fbxModel.postTransform * globalTransform;
        if (fbxModel.hasGeometricOffset) {
            glm::mat4 geometricOffset = createMatFromScaleQuatAndPos(fbxModel.geometricScaling, fbxModel.geometricRotation, fbxModel.geometricTranslation);
            globalTransform = globalTransform * geometricOffset;
        }

        if (mixamoHack) {
            // there's something weird about the models from Mixamo Fuse; they don't skin right with the full transform
            return globalTransform;
        }
        QList<QString> parentIDs = _connectionParentMap.values(nodeID);
        nodeID = QString();
        foreach (const QString& parentID, parentIDs) {
            if (visitedNodes.contains(parentID)) {
                qCWarning(modelformat) << "Ignoring loop detected in FBX connection map for" << url;
                continue;
            }

            if (fbxModels.contains(parentID)) {
                nodeID = parentID;
                break;
            }
        }
    }

    return globalTransform;
}

class ExtractedBlendshape {
public:
    QString id;
    HFMBlendshape blendshape;
};

void printNode(const FBXNode& node, int indentLevel) {
    int indentLength = 2;
    QByteArray spaces(indentLevel * indentLength, ' ');
    QDebug nodeDebug = qDebug(modelformat);

    nodeDebug.nospace() << spaces.data() << node.name.data() << ": ";
    foreach (const QVariant& property, node.properties) {
        nodeDebug << property;
    }

    foreach (const FBXNode& child, node.children) {
        printNode(child, indentLevel + 1);
    }
}

class Cluster {
public:
    QVector<int> indices;
    QVector<double> weights;
    glm::mat4 transformLink;
};

void appendModelIDs(const QString& parentID, const QMultiMap<QString, QString>& connectionChildMap,
        QHash<QString, FBXModel>& fbxModels, QSet<QString>& remainingModels, QVector<QString>& modelIDs, bool isRootNode = false) {
    if (remainingModels.contains(parentID)) {
        modelIDs.append(parentID);
        remainingModels.remove(parentID);
    }
    int parentIndex = isRootNode ? -1 : modelIDs.size() - 1;
    foreach (const QString& childID, connectionChildMap.values(parentID)) {
        if (remainingModels.contains(childID)) {
            FBXModel& fbxModel = fbxModels[childID];
            if (fbxModel.parentIndex == -1) {
                fbxModel.parentIndex = parentIndex;
                appendModelIDs(childID, connectionChildMap, fbxModels, remainingModels, modelIDs);
            }
        }
    }
}

HFMBlendshape extractBlendshape(const FBXNode& object) {
    HFMBlendshape blendshape;
    foreach (const FBXNode& data, object.children) {
        if (data.name == "Indexes") {
            blendshape.indices = FBXSerializer::getIntVector(data);

        } else if (data.name == "Vertices") {
            blendshape.vertices = FBXSerializer::createVec3Vector(FBXSerializer::getDoubleVector(data));

        } else if (data.name == "Normals") {
            blendshape.normals = FBXSerializer::createVec3Vector(FBXSerializer::getDoubleVector(data));
        }
    }
    return blendshape;
}

QVector<int> getIndices(const QVector<QString> ids, QVector<QString> modelIDs) {
    QVector<int> indices;
    foreach (const QString& id, ids) {
        int index = modelIDs.indexOf(id);
        if (index != -1) {
            indices.append(index);
        }
    }
    return indices;
}

typedef QPair<int, float> WeightedIndex;

void addBlendshapes(const ExtractedBlendshape& extracted, const QList<WeightedIndex>& indices, ExtractedMesh& extractedMesh) {
    foreach (const WeightedIndex& index, indices) {
        extractedMesh.mesh.blendshapes.resize(max(extractedMesh.mesh.blendshapes.size(), index.first + 1));
        extractedMesh.blendshapeIndexMaps.resize(extractedMesh.mesh.blendshapes.size());
        HFMBlendshape& blendshape = extractedMesh.mesh.blendshapes[index.first];
        QHash<int, int>& blendshapeIndexMap = extractedMesh.blendshapeIndexMaps[index.first];
        for (int i = 0; i < extracted.blendshape.indices.size(); i++) {
            int oldIndex = extracted.blendshape.indices.at(i);
            for (QMultiHash<int, int>::const_iterator it = extractedMesh.newIndices.constFind(oldIndex);
                    it != extractedMesh.newIndices.constEnd() && it.key() == oldIndex; it++) {
                QHash<int, int>::iterator blendshapeIndex = blendshapeIndexMap.find(it.value());
                if (blendshapeIndex == blendshapeIndexMap.end()) {
                    blendshapeIndexMap.insert(it.value(), blendshape.indices.size());
                    blendshape.indices.append(it.value());
                    blendshape.vertices.append(extracted.blendshape.vertices.at(i) * index.second);
                    blendshape.normals.append(extracted.blendshape.normals.at(i) * index.second);
                } else {
                    blendshape.vertices[*blendshapeIndex] += extracted.blendshape.vertices.at(i) * index.second;
                    blendshape.normals[*blendshapeIndex] += extracted.blendshape.normals.at(i) * index.second;
                }
            }
        }
    }
}

QString getTopModelID(const QMultiMap<QString, QString>& connectionParentMap,
        const QHash<QString, FBXModel>& fbxModels, const QString& modelID, const QString& url) {
    QString topID = modelID;
    QVector<QString> visitedNodes; // Used to prevent following a cycle
    forever {
        visitedNodes.append(topID); // Append each node we visit

        foreach (const QString& parentID, connectionParentMap.values(topID)) {
            if (visitedNodes.contains(parentID)) {
                qCWarning(modelformat) << "Ignoring loop detected in FBX connection map for" << url;
                continue;
            }

            if (fbxModels.contains(parentID)) {
                topID = parentID;
                goto outerContinue;
            }
        }
        return topID;

        outerContinue: ;
    }
}

QString getString(const QVariant& value) {
    // if it's a list, return the first entry
    QVariantList list = value.toList();
    return list.isEmpty() ? value.toString() : list.at(0).toString();
}

typedef std::vector<glm::vec3> ShapeVertices;

class AnimationCurve {
public:
    QVector<float> values;
};

bool checkMaterialsHaveTextures(const QHash<QString, HFMMaterial>& materials,
        const QHash<QString, QByteArray>& textureFilenames, const QMultiMap<QString, QString>& _connectionChildMap) {
    foreach (const QString& materialID, materials.keys()) {
        foreach (const QString& childID, _connectionChildMap.values(materialID)) {
            if (textureFilenames.contains(childID)) {
                return true;
            }
        }
    }
    return false;
}

int matchTextureUVSetToAttributeChannel(const QString& texUVSetName, const QHash<QString, int>& texcoordChannels) {
    if (texUVSetName.isEmpty()) {
        return 0;
    } else {
        QHash<QString, int>::const_iterator tcUnit = texcoordChannels.find(texUVSetName);
        if (tcUnit != texcoordChannels.end()) {
            int channel = (*tcUnit);
            if (channel >= 2) {
                channel = 0;
            }
            return channel;
        } else {
            return 0;
        }
    }
}


HFMLight extractLight(const FBXNode& object) {
    HFMLight light;
    foreach (const FBXNode& subobject, object.children) {
        QString childname = QString(subobject.name);
        if (subobject.name == "Properties70") {
            foreach (const FBXNode& property, subobject.children) {
                int valIndex = 4;
                QString propName = QString(property.name);
                if (property.name == "P") {
                    QString propname = property.properties.at(0).toString();
                    if (propname == "Intensity") {
                        light.intensity = 0.01f * property.properties.at(valIndex).value<float>();
                    } else if (propname == "Color") {
                        light.color = FBXSerializer::getVec3(property.properties, valIndex);
                    }
                }
            }
        } else if ( subobject.name == "GeometryVersion"
                   || subobject.name == "TypeFlags") {
        }
    }
#if defined(DEBUG_FBXSERIALIZER)

    QString type = object.properties.at(0).toString();
    type = object.properties.at(1).toString();
    type = object.properties.at(2).toString();

    foreach (const QVariant& prop, object.properties) {
        QString proptype = prop.typeName();
        QString propval = prop.toString();
        if (proptype == "Properties70") {
        }
    }
#endif

    return light;
}

QByteArray fileOnUrl(const QByteArray& filepath, const QString& url) {
    // in order to match the behaviour when loading models from remote URLs
    // we assume that all external textures are right beside the loaded model
    // ignoring any relative paths or absolute paths inside of models

    return filepath.mid(filepath.lastIndexOf('/') + 1);
}

QMap<QString, QString> getJointNameMapping(const QVariantHash& mapping) {
    static const QString JOINT_NAME_MAPPING_FIELD = "jointMap";
    QMap<QString, QString> hfmToHifiJointNameMap;
    if (!mapping.isEmpty() && mapping.contains(JOINT_NAME_MAPPING_FIELD) && mapping[JOINT_NAME_MAPPING_FIELD].type() == QVariant::Hash) {
        auto jointNames = mapping[JOINT_NAME_MAPPING_FIELD].toHash();
        for (auto itr = jointNames.begin(); itr != jointNames.end(); itr++) {
            hfmToHifiJointNameMap.insert(itr.key(), itr.value().toString());
            qCDebug(modelformat) << "the mapped key " << itr.key() << " has a value of " << hfmToHifiJointNameMap[itr.key()];
        }
    }
    return hfmToHifiJointNameMap;
}

QMap<QString, glm::quat> getJointRotationOffsets(const QVariantHash& mapping) {
    QMap<QString, glm::quat> jointRotationOffsets;
    static const QString JOINT_ROTATION_OFFSET_FIELD = "jointRotationOffset";
    if (!mapping.isEmpty() && mapping.contains(JOINT_ROTATION_OFFSET_FIELD) && mapping[JOINT_ROTATION_OFFSET_FIELD].type() == QVariant::Hash) {
        auto offsets = mapping[JOINT_ROTATION_OFFSET_FIELD].toHash();
        for (auto itr = offsets.begin(); itr != offsets.end(); itr++) {
            QString jointName = itr.key();
            QString line = itr.value().toString();
            auto quatCoords = line.split(',');
            if (quatCoords.size() == 4) {
                float quatX = quatCoords[0].mid(1).toFloat();
                float quatY = quatCoords[1].toFloat();
                float quatZ = quatCoords[2].toFloat();
                float quatW = quatCoords[3].mid(0, quatCoords[3].size() - 1).toFloat();
                if (!isNaN(quatX) && !isNaN(quatY) && !isNaN(quatZ) && !isNaN(quatW)) {
                    glm::quat rotationOffset = glm::quat(quatW, quatX, quatY, quatZ);
                    jointRotationOffsets.insert(jointName, rotationOffset);
                }
            }
        }
    }
    return jointRotationOffsets;
}

HFMModel* FBXSerializer::extractHFMModel(const QVariantHash& mapping, const QString& url) {
    const FBXNode& node = _rootNode;
    QMap<QString, ExtractedMesh> meshes;
    QHash<QString, QString> modelIDsToNames;
    QHash<QString, int> meshIDsToMeshIndices;
    QHash<QString, QString> ooChildToParent;

    QVector<ExtractedBlendshape> blendshapes;

    QHash<QString, FBXModel> fbxModels;
    QHash<QString, Cluster> clusters; 
    QHash<QString, AnimationCurve> animationCurves;

    QHash<QString, QString> typeFlags;

    QHash<QString, QString> localRotations;
    QHash<QString, QString> localTranslations;
    QHash<QString, QString> xComponents;
    QHash<QString, QString> yComponents;
    QHash<QString, QString> zComponents;

    std::map<QString, HFMLight> lights;

    QVariantHash joints = mapping.value("joint").toHash();

    QVariantHash blendshapeMappings = mapping.value("bs").toHash();

    QMultiHash<QByteArray, WeightedIndex> blendshapeIndices;
    for (int i = 0;; i++) {
        QByteArray blendshapeName = FACESHIFT_BLENDSHAPES[i];
        if (blendshapeName.isEmpty()) {
            break;
        }
        QList<QVariant> mappings = blendshapeMappings.values(blendshapeName);
        if (mappings.isEmpty()) {
            blendshapeIndices.insert(blendshapeName, WeightedIndex(i, 1.0f));
        } else {
            foreach (const QVariant& mapping, mappings) {
                QVariantList blendshapeMapping = mapping.toList();
                blendshapeIndices.insert(blendshapeMapping.at(0).toByteArray(),
                   WeightedIndex(i, blendshapeMapping.at(1).toFloat()));
            }
        }
    }
    QMultiHash<QString, WeightedIndex> blendshapeChannelIndices;
#if defined(DEBUG_FBXSERIALIZER)
    int unknown = 0;
#endif
    HFMModel* hfmModelPtr = new HFMModel;
    HFMModel& hfmModel = *hfmModelPtr;

    hfmModel.originalURL = url;
    hfmModel.hfmToHifiJointNameMapping.clear();
    hfmModel.hfmToHifiJointNameMapping = getJointNameMapping(mapping);

    float unitScaleFactor = 1.0f;
    glm::vec3 ambientColor;
    QString hifiGlobalNodeID;
    unsigned int meshIndex = 0;
    haveReportedUnhandledRotationOrder = false;
    foreach (const FBXNode& child, node.children) {

        if (child.name == "FBXHeaderExtension") {
            foreach (const FBXNode& object, child.children) {
                if (object.name == "SceneInfo") {
                    foreach (const FBXNode& subobject, object.children) {
                        if (subobject.name == "MetaData") {
                            foreach (const FBXNode& subsubobject, subobject.children) {
                                if (subsubobject.name == "Author") {
                                    hfmModel.author = subsubobject.properties.at(0).toString();
                                }
                            }
                        } else if (subobject.name == "Properties70") {
                            foreach (const FBXNode& subsubobject, subobject.children) {
                                static const QVariant APPLICATION_NAME = QVariant(QByteArray("Original|ApplicationName"));
                                if (subsubobject.name == "P" && subsubobject.properties.size() >= 5 &&
                                        subsubobject.properties.at(0) == APPLICATION_NAME) {
                                    hfmModel.applicationName = subsubobject.properties.at(4).toString();
                                }
                            }
                        }
                    }
                }
            }
        } else if (child.name == "GlobalSettings") {
            foreach (const FBXNode& object, child.children) {
                if (object.name == "Properties70") {
                    QString propertyName = "P";
                    int index = 4;
                    foreach (const FBXNode& subobject, object.children) {
                        if (subobject.name == propertyName) {
                            static const QVariant UNIT_SCALE_FACTOR = QByteArray("UnitScaleFactor");
                            static const QVariant AMBIENT_COLOR = QByteArray("AmbientColor");
                            const auto& subpropName = subobject.properties.at(0);
                            if (subpropName == UNIT_SCALE_FACTOR) {
                                unitScaleFactor = subobject.properties.at(index).toFloat();
                            } else if (subpropName == AMBIENT_COLOR) {
                                ambientColor = getVec3(subobject.properties, index);
                            }
                        }
                    }
                }
            }
        } else if (child.name == "Objects") {
            foreach (const FBXNode& object, child.children) {
                if (object.name == "Geometry") {
                    if (object.properties.at(2) == "Mesh") {
                        meshes.insert(getID(object.properties), extractMesh(object, meshIndex));
                    } else { // object.properties.at(2) == "Shape"
                        ExtractedBlendshape extracted = { getID(object.properties), extractBlendshape(object) };
                        blendshapes.append(extracted);
                    }
                } else if (object.name == "Model") {
                    QString name = getName(object.properties);
                    QString id = getID(object.properties);
                    modelIDsToNames.insert(id, name);

                    QString modelname = name.toLower();
                    if (modelname.startsWith("hifi")) {
                        hifiGlobalNodeID = id;
                    }

                    glm::vec3 translation;
                    // NOTE: the euler angles as supplied by the FBX file are in degrees
                    glm::vec3 rotationOffset;
                    int rotationOrder = OrderXYZ;  // Default rotation order set in "Definitions" node is assumed to be XYZ.
                    glm::vec3 preRotation, rotation, postRotation;
                    glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
                    glm::vec3 scalePivot, rotationPivot, scaleOffset;
                    bool rotationMinX = false, rotationMinY = false, rotationMinZ = false;
                    bool rotationMaxX = false, rotationMaxY = false, rotationMaxZ = false;

                    // local offset transforms from 3ds max
                    bool hasGeometricOffset = false;
                    glm::vec3 geometricTranslation;
                    glm::vec3 geometricScaling(1.0f, 1.0f, 1.0f);
                    glm::vec3 geometricRotation;

                    glm::vec3 rotationMin, rotationMax;
                    FBXModel fbxModel = { name, -1, glm::vec3(), glm::mat4(), glm::quat(), glm::quat(), glm::quat(),
                                       glm::mat4(), glm::vec3(), glm::vec3(),
                                       false, glm::vec3(), glm::quat(), glm::vec3(1.0f) };
                    ExtractedMesh* mesh = NULL;
                    QVector<ExtractedBlendshape> blendshapes;
                    foreach (const FBXNode& subobject, object.children) {
                        bool properties = false;
                        QByteArray propertyName;
                        int index;
                        if (subobject.name == "Properties60") {
                            properties = true;
                            propertyName = "Property";
                            index = 3;

                        } else if (subobject.name == "Properties70") {
                            properties = true;
                            propertyName = "P";
                            index = 4;
                        }
                        if (properties) {
                            static const QVariant ROTATION_ORDER = QByteArray("RotationOrder");
                            static const QVariant GEOMETRIC_TRANSLATION = QByteArray("GeometricTranslation");
                            static const QVariant GEOMETRIC_ROTATION = QByteArray("GeometricRotation");
                            static const QVariant GEOMETRIC_SCALING = QByteArray("GeometricScaling");
                            static const QVariant LCL_TRANSLATION = QByteArray("Lcl Translation");
                            static const QVariant LCL_ROTATION = QByteArray("Lcl Rotation");
                            static const QVariant LCL_SCALING = QByteArray("Lcl Scaling");
                            static const QVariant ROTATION_MAX = QByteArray("RotationMax");
                            static const QVariant ROTATION_MAX_X = QByteArray("RotationMaxX");
                            static const QVariant ROTATION_MAX_Y = QByteArray("RotationMaxY");
                            static const QVariant ROTATION_MAX_Z = QByteArray("RotationMaxZ");
                            static const QVariant ROTATION_MIN = QByteArray("RotationMin");
                            static const QVariant ROTATION_MIN_X = QByteArray("RotationMinX");
                            static const QVariant ROTATION_MIN_Y = QByteArray("RotationMinY");
                            static const QVariant ROTATION_MIN_Z = QByteArray("RotationMinZ");
                            static const QVariant ROTATION_OFFSET = QByteArray("RotationOffset");
                            static const QVariant ROTATION_PIVOT = QByteArray("RotationPivot");
                            static const QVariant SCALING_OFFSET = QByteArray("ScalingOffset");
                            static const QVariant SCALING_PIVOT = QByteArray("ScalingPivot");
                            static const QVariant PRE_ROTATION = QByteArray("PreRotation");
                            static const QVariant POST_ROTATION = QByteArray("PostRotation");
                            foreach(const FBXNode& property, subobject.children) {
                                const auto& childProperty = property.properties.at(0);
                                if (property.name == propertyName) {
                                    if (childProperty == LCL_TRANSLATION) {
                                        translation = getVec3(property.properties, index);

                                    } else if (childProperty == ROTATION_ORDER) {
                                        rotationOrder = property.properties.at(index).toInt();

                                    } else if (childProperty == ROTATION_OFFSET) {
                                        rotationOffset = getVec3(property.properties, index);

                                    } else if (childProperty == ROTATION_PIVOT) {
                                        rotationPivot = getVec3(property.properties, index);

                                    } else if (childProperty == PRE_ROTATION) {
                                        preRotation = convertRotationToXYZ(rotationOrder, getVec3(property.properties, index));

                                    } else if (childProperty == LCL_ROTATION) {
                                        rotation = convertRotationToXYZ(rotationOrder, getVec3(property.properties, index));

                                    } else if (childProperty == POST_ROTATION) {
                                        postRotation = convertRotationToXYZ(rotationOrder, getVec3(property.properties, index));

                                    } else if (childProperty == SCALING_PIVOT) {
                                        scalePivot = getVec3(property.properties, index);

                                    } else if (childProperty == LCL_SCALING) {
                                        scale = getVec3(property.properties, index);

                                    } else if (childProperty == SCALING_OFFSET) {
                                        scaleOffset = getVec3(property.properties, index);

                                    // NOTE: these rotation limits are stored in degrees (NOT radians)
                                    } else if (childProperty == ROTATION_MIN) {
                                        rotationMin = getVec3(property.properties, index);

                                    } else if (childProperty == ROTATION_MAX) {
                                        rotationMax = getVec3(property.properties, index);

                                    } else if (childProperty == ROTATION_MIN_X) {
                                        rotationMinX = property.properties.at(index).toBool();

                                    } else if (childProperty == ROTATION_MIN_Y) {
                                        rotationMinY = property.properties.at(index).toBool();

                                    } else if (childProperty == ROTATION_MIN_Z) {
                                        rotationMinZ = property.properties.at(index).toBool();

                                    } else if (childProperty == ROTATION_MAX_X) {
                                        rotationMaxX = property.properties.at(index).toBool();

                                    } else if (childProperty == ROTATION_MAX_Y) {
                                        rotationMaxY = property.properties.at(index).toBool();

                                    } else if (childProperty == ROTATION_MAX_Z) {
                                        rotationMaxZ = property.properties.at(index).toBool();
                                    } else if (childProperty == GEOMETRIC_TRANSLATION) {
                                        geometricTranslation = getVec3(property.properties, index);
                                        hasGeometricOffset = true;
                                    } else if (childProperty == GEOMETRIC_ROTATION) {
                                        geometricRotation = getVec3(property.properties, index);
                                        hasGeometricOffset = true;
                                    } else if (childProperty == GEOMETRIC_SCALING) {
                                        geometricScaling = getVec3(property.properties, index);
                                        hasGeometricOffset = true;
                                    }
                                }
                            }
                        } else if (subobject.name == "Vertices") {
                            // it's a mesh as well as a model
                            mesh = &meshes[getID(object.properties)];
                            *mesh = extractMesh(object, meshIndex);

                        } else if (subobject.name == "Shape") {
                            ExtractedBlendshape blendshape =  { subobject.properties.at(0).toString(),
                                extractBlendshape(subobject) };
                            blendshapes.append(blendshape);
                        }
#if defined(DEBUG_FBXSERIALIZER)
                        else if (subobject.name == "TypeFlags") {
                            QString attributetype = subobject.properties.at(0).toString();
                            if (!attributetype.empty()) {
                                if (attributetype == "Light") {
                                    QString lightprop;
                                    foreach (const QVariant& vprop, subobject.properties) {
                                        lightprop = vprop.toString();
                                    }

                                    HFMLight light = extractLight(object);
                                }
                            }
                        } else {
                            QString whatisthat = subobject.name;
                            if (whatisthat == "Shape") {
                            }
                        }
#endif
                    }

                    // add the blendshapes included in the model, if any
                    if (mesh) {
                        foreach (const ExtractedBlendshape& extracted, blendshapes) {
                            addBlendshapes(extracted, blendshapeIndices.values(extracted.id.toLatin1()), *mesh);
                        }
                    }

                    // see FBX documentation, http://download.autodesk.com/us/fbx/20112/FBX_SDK_HELP/index.html
                    fbxModel.translation = translation;

                    fbxModel.preTransform = glm::translate(rotationOffset) * glm::translate(rotationPivot);
                    fbxModel.preRotation = glm::quat(glm::radians(preRotation));
                    fbxModel.rotation = glm::quat(glm::radians(rotation));
                    fbxModel.postRotation = glm::inverse(glm::quat(glm::radians(postRotation)));
                    fbxModel.postTransform = glm::translate(-rotationPivot) * glm::translate(scaleOffset) *
                        glm::translate(scalePivot) * glm::scale(scale) * glm::translate(-scalePivot);
                    // NOTE: angles from the FBX file are in degrees
                    // so we convert them to radians for the FBXModel class
                    fbxModel.rotationMin = glm::radians(glm::vec3(rotationMinX ? rotationMin.x : -180.0f,
                        rotationMinY ? rotationMin.y : -180.0f, rotationMinZ ? rotationMin.z : -180.0f));
                    fbxModel.rotationMax = glm::radians(glm::vec3(rotationMaxX ? rotationMax.x : 180.0f,
                        rotationMaxY ? rotationMax.y : 180.0f, rotationMaxZ ? rotationMax.z : 180.0f));

                    fbxModel.hasGeometricOffset = hasGeometricOffset;
                    fbxModel.geometricTranslation = geometricTranslation;
                    fbxModel.geometricRotation = glm::quat(glm::radians(geometricRotation));
                    fbxModel.geometricScaling = geometricScaling;

                    fbxModels.insert(getID(object.properties), fbxModel);
                } else if (object.name == "Texture") {
                    TextureParam tex;
                    foreach (const FBXNode& subobject, object.children) {
                        const int RELATIVE_FILENAME_MIN_SIZE = 1;
                        const int TEXTURE_NAME_MIN_SIZE = 1;
                        const int TEXTURE_ALPHA_SOURCE_MIN_SIZE = 1;
                        const int MODEL_UV_TRANSLATION_MIN_SIZE = 2;
                        const int MODEL_UV_SCALING_MIN_SIZE = 2;
                        const int CROPPING_MIN_SIZE = 4;
                        if (subobject.name == "RelativeFilename" && subobject.properties.length() >= RELATIVE_FILENAME_MIN_SIZE) {
                            QByteArray filename = subobject.properties.at(0).toByteArray();
                            QByteArray filepath = filename.replace('\\', '/');
                            filename = fileOnUrl(filepath, url);
                            _textureFilepaths.insert(getID(object.properties), filepath);
                            _textureFilenames.insert(getID(object.properties), filename);
                        } else if (subobject.name == "TextureName" && subobject.properties.length() >= TEXTURE_NAME_MIN_SIZE) {
                            // trim the name from the timestamp
                            QString name = QString(subobject.properties.at(0).toByteArray());
                            name = name.left(name.indexOf('['));
                            _textureNames.insert(getID(object.properties), name);
                        } else if (subobject.name == "Texture_Alpha_Source" && subobject.properties.length() >= TEXTURE_ALPHA_SOURCE_MIN_SIZE) {
                            tex.assign<uint8_t>(tex.alphaSource, subobject.properties.at(0).value<int>());
                        } else if (subobject.name == "ModelUVTranslation" && subobject.properties.length() >= MODEL_UV_TRANSLATION_MIN_SIZE) {
                            tex.assign(tex.UVTranslation, glm::vec2(subobject.properties.at(0).value<double>(),
                                                                    subobject.properties.at(1).value<double>()));
                        } else if (subobject.name == "ModelUVScaling" && subobject.properties.length() >= MODEL_UV_SCALING_MIN_SIZE) {
                            tex.assign(tex.UVScaling, glm::vec2(subobject.properties.at(0).value<double>(),
                                                                subobject.properties.at(1).value<double>()));
                            if (tex.UVScaling.x == 0.0f) {
                                tex.UVScaling.x = 1.0f;
                            }
                            if (tex.UVScaling.y == 0.0f) {
                                tex.UVScaling.y = 1.0f;
                            }
                        } else if (subobject.name == "Cropping" && subobject.properties.length() >= CROPPING_MIN_SIZE) {
                            tex.assign(tex.cropping, glm::vec4(subobject.properties.at(0).value<int>(),
                                                                subobject.properties.at(1).value<int>(),
                                                                subobject.properties.at(2).value<int>(),
                                                                subobject.properties.at(3).value<int>()));
                        } else if (subobject.name == "Properties70") {
                            QByteArray propertyName;
                            int index;
                                propertyName = "P";
                                index = 4;
                                foreach (const FBXNode& property, subobject.children) {
                                    static const QVariant UV_SET = QByteArray("UVSet");
                                    static const QVariant CURRENT_TEXTURE_BLEND_MODE = QByteArray("CurrentTextureBlendMode");
                                    static const QVariant USE_MATERIAL = QByteArray("UseMaterial");
                                    static const QVariant TRANSLATION = QByteArray("Translation");
                                    static const QVariant ROTATION = QByteArray("Rotation");
                                    static const QVariant SCALING = QByteArray("Scaling");
                                    if (property.name == propertyName) {
                                        QString v = property.properties.at(0).toString();
                                        if (property.properties.at(0) == UV_SET) {
                                            std::string uvName = property.properties.at(index).toString().toStdString();
                                            tex.assign(tex.UVSet, property.properties.at(index).toString());
                                        } else if (property.properties.at(0) == CURRENT_TEXTURE_BLEND_MODE) {
                                            tex.assign<uint8_t>(tex.currentTextureBlendMode, property.properties.at(index).value<int>());
                                        } else if (property.properties.at(0) == USE_MATERIAL) {
                                            tex.assign<bool>(tex.useMaterial, property.properties.at(index).value<int>());
                                        } else if (property.properties.at(0) == TRANSLATION) {
                                            tex.assign(tex.translation, getVec3(property.properties, index));
                                        } else if (property.properties.at(0) == ROTATION) {
                                            tex.assign(tex.rotation, getVec3(property.properties, index));
                                        } else if (property.properties.at(0) == SCALING) {
                                            tex.assign(tex.scaling, getVec3(property.properties, index));
                                            if (tex.scaling.x == 0.0f) {
                                                tex.scaling.x = 1.0f;
                                            }
                                            if (tex.scaling.y == 0.0f) {
                                                tex.scaling.y = 1.0f;
                                            }
                                            if (tex.scaling.z == 0.0f) {
                                                tex.scaling.z = 1.0f;
                                            }
                                        }
#if defined(DEBUG_FBXSERIALIZER)
                                        else {
                                            QString propName = v;
                                            unknown++;
                                        }
#endif
                                    }
                                }
                        }
#if defined(DEBUG_FBXSERIALIZER)
                        else {
                            if (subobject.name == "Type") {
                            } else if (subobject.name == "Version") {
                            } else if (subobject.name == "FileName") {
                            } else if (subobject.name == "Media") {
                            } else {
                                QString subname = subobject.name.data();
                                unknown++;
                            }
                        }
#endif
                    }

                    if (!tex.isDefault) {
                        _textureParams.insert(getID(object.properties), tex);
                    }
                } else if (object.name == "Video") {
                    QByteArray filepath;
                    QByteArray content;
                    foreach (const FBXNode& subobject, object.children) {
                        if (subobject.name == "RelativeFilename") {
                            filepath = subobject.properties.at(0).toByteArray();
                            filepath = filepath.replace('\\', '/');

                        } else if (subobject.name == "Content" && !subobject.properties.isEmpty()) {
                            content = subobject.properties.at(0).toByteArray();
                        }
                    }
                    if (!content.isEmpty()) {
                        _textureContent.insert(filepath, content);
                    }
                } else if (object.name == "Material") {
                    HFMMaterial material;
                    material.name = (object.properties.at(1).toString());
                    foreach (const FBXNode& subobject, object.children) {
                        bool properties = false;

                        QByteArray propertyName;
                        int index;
                        if (subobject.name == "Properties60") {
                            properties = true;
                            propertyName = "Property";
                            index = 3;

                        } else if (subobject.name == "Properties70") {
                            properties = true;
                            propertyName = "P";
                            index = 4;
                        } else if (subobject.name == "ShadingModel") {
                            material.shadingModel = subobject.properties.at(0).toString();
                        }

                        if (properties) {
                            std::vector<std::string> unknowns;
                            static const QVariant DIFFUSE_COLOR = QByteArray("DiffuseColor");
                            static const QVariant DIFFUSE_FACTOR = QByteArray("DiffuseFactor");
                            static const QVariant DIFFUSE = QByteArray("Diffuse");
                            static const QVariant SPECULAR_COLOR = QByteArray("SpecularColor");
                            static const QVariant SPECULAR_FACTOR = QByteArray("SpecularFactor");
                            static const QVariant SPECULAR = QByteArray("Specular");
                            static const QVariant EMISSIVE_COLOR = QByteArray("EmissiveColor");
                            static const QVariant EMISSIVE_FACTOR = QByteArray("EmissiveFactor");
                            static const QVariant EMISSIVE = QByteArray("Emissive");
                            static const QVariant AMBIENT_FACTOR = QByteArray("AmbientFactor");
                            static const QVariant SHININESS = QByteArray("Shininess");
                            static const QVariant OPACITY = QByteArray("Opacity");
                            static const QVariant MAYA_USE_NORMAL_MAP = QByteArray("Maya|use_normal_map");
                            static const QVariant MAYA_BASE_COLOR = QByteArray("Maya|base_color");
                            static const QVariant MAYA_USE_COLOR_MAP = QByteArray("Maya|use_color_map");
                            static const QVariant MAYA_ROUGHNESS = QByteArray("Maya|roughness");
                            static const QVariant MAYA_USE_ROUGHNESS_MAP = QByteArray("Maya|use_roughness_map");
                            static const QVariant MAYA_METALLIC = QByteArray("Maya|metallic");
                            static const QVariant MAYA_USE_METALLIC_MAP = QByteArray("Maya|use_metallic_map");
                            static const QVariant MAYA_EMISSIVE = QByteArray("Maya|emissive");
                            static const QVariant MAYA_EMISSIVE_INTENSITY = QByteArray("Maya|emissive_intensity");
                            static const QVariant MAYA_USE_EMISSIVE_MAP = QByteArray("Maya|use_emissive_map");
                            static const QVariant MAYA_USE_AO_MAP = QByteArray("Maya|use_ao_map");




                            foreach(const FBXNode& property, subobject.children) {
                                if (property.name == propertyName) {
                                    if (property.properties.at(0) == DIFFUSE_COLOR) {
                                        material.diffuseColor = getVec3(property.properties, index);
                                    } else if (property.properties.at(0) == DIFFUSE_FACTOR) {
                                        material.diffuseFactor = property.properties.at(index).value<double>();
                                    } else if (property.properties.at(0) == DIFFUSE) {
                                        // NOTE: this is uneeded but keep it for now for debug
                                        //  material.diffuseColor = getVec3(property.properties, index);
                                        //  material.diffuseFactor = 1.0;

                                    } else if (property.properties.at(0) == SPECULAR_COLOR) {
                                        material.specularColor = getVec3(property.properties, index);
                                    } else if (property.properties.at(0) == SPECULAR_FACTOR) {
                                        material.specularFactor = property.properties.at(index).value<double>();
                                    } else if (property.properties.at(0) == SPECULAR) {
                                        // NOTE: this is uneeded but keep it for now for debug
                                        //  material.specularColor = getVec3(property.properties, index);
                                        //  material.specularFactor = 1.0;

                                    } else if (property.properties.at(0) == EMISSIVE_COLOR) {
                                        material.emissiveColor = getVec3(property.properties, index);
                                    } else if (property.properties.at(0) == EMISSIVE_FACTOR) {
                                        material.emissiveFactor = property.properties.at(index).value<double>();
                                    } else if (property.properties.at(0) == EMISSIVE) {
                                        // NOTE: this is uneeded but keep it for now for debug
                                        //  material.emissiveColor = getVec3(property.properties, index);
                                        //  material.emissiveFactor = 1.0;

                                    } else if (property.properties.at(0) == AMBIENT_FACTOR) {
                                        material.ambientFactor = property.properties.at(index).value<double>();
                                        // Detected just for BLender AO vs lightmap
                                    } else if (property.properties.at(0) == SHININESS) {
                                        material.shininess = property.properties.at(index).value<double>();

                                    } else if (property.properties.at(0) == OPACITY) {
                                        material.opacity = property.properties.at(index).value<double>();
                                    }

                                    // Sting Ray Material Properties!!!!
                                    else if (property.properties.at(0) == MAYA_USE_NORMAL_MAP) {
                                        material.isPBSMaterial = true;
                                        material.useNormalMap = (bool)property.properties.at(index).value<double>();

                                    } else if (property.properties.at(0) == MAYA_BASE_COLOR) {
                                        material.isPBSMaterial = true;
                                        material.diffuseColor = getVec3(property.properties, index);

                                    } else if (property.properties.at(0) == MAYA_USE_COLOR_MAP) {
                                        material.isPBSMaterial = true;
                                        material.useAlbedoMap = (bool) property.properties.at(index).value<double>();

                                    } else if (property.properties.at(0) == MAYA_ROUGHNESS) {
                                        material.isPBSMaterial = true;
                                        material.roughness = property.properties.at(index).value<double>();

                                    } else if (property.properties.at(0) == MAYA_USE_ROUGHNESS_MAP) {
                                        material.isPBSMaterial = true;
                                        material.useRoughnessMap = (bool)property.properties.at(index).value<double>();

                                    } else if (property.properties.at(0) == MAYA_METALLIC) {
                                        material.isPBSMaterial = true;
                                        material.metallic = property.properties.at(index).value<double>();

                                    } else if (property.properties.at(0) == MAYA_USE_METALLIC_MAP) {
                                        material.isPBSMaterial = true;
                                        material.useMetallicMap = (bool)property.properties.at(index).value<double>();

                                    } else if (property.properties.at(0) == MAYA_EMISSIVE) {
                                        material.isPBSMaterial = true;
                                        material.emissiveColor = getVec3(property.properties, index);

                                    } else if (property.properties.at(0) == MAYA_EMISSIVE_INTENSITY) {
                                        material.isPBSMaterial = true;
                                        material.emissiveIntensity = property.properties.at(index).value<double>();

                                    } else if (property.properties.at(0) == MAYA_USE_EMISSIVE_MAP) {
                                        material.isPBSMaterial = true;
                                        material.useEmissiveMap = (bool)property.properties.at(index).value<double>();

                                    } else if (property.properties.at(0) == MAYA_USE_AO_MAP) {
                                        material.isPBSMaterial = true;
                                        material.useOcclusionMap = (bool)property.properties.at(index).value<double>();

                                    } else {
                                        const QString propname = property.properties.at(0).toString();
                                        unknowns.push_back(propname.toStdString());
                                    }
                                }
                            }
                        }
#if defined(DEBUG_FBXSERIALIZER)
                        else {
                            QString propname = subobject.name.data();
                            int unknown = 0;
                            if ( (propname == "Version")
                                ||(propname == "Multilayer")) {
                            } else {
                                unknown++;
                            }
                        }
#endif
                    }
                    material.materialID = getID(object.properties);
                    _hfmMaterials.insert(material.materialID, material);


                } else if (object.name == "NodeAttribute") {
#if defined(DEBUG_FBXSERIALIZER)
                    std::vector<QString> properties;
                    foreach(const QVariant& v, object.properties) {
                        properties.push_back(v.toString());
                    }
#endif
                    QString attribID = getID(object.properties);
                    QString attributetype;
                    foreach (const FBXNode& subobject, object.children) {
                        if (subobject.name == "TypeFlags") {
                            typeFlags.insert(getID(object.properties), subobject.properties.at(0).toString());
                            attributetype = subobject.properties.at(0).toString();
                        }
                    }

                    if (!attributetype.isEmpty()) {
                        if (attributetype == "Light") {
                            HFMLight light = extractLight(object);
                            lights[attribID] = light;
                        }
                    }

                } else if (object.name == "Deformer") {
                    if (object.properties.last() == "Cluster") {
                        Cluster cluster;
                        foreach (const FBXNode& subobject, object.children) {
                            if (subobject.name == "Indexes") {
                                cluster.indices = getIntVector(subobject);

                            } else if (subobject.name == "Weights") {
                                cluster.weights = getDoubleVector(subobject);

                            } else if (subobject.name == "TransformLink") {
                                QVector<double> values = getDoubleVector(subobject);
                                cluster.transformLink = createMat4(values);
                            }
                        }
                        clusters.insert(getID(object.properties), cluster);

                    } else if (object.properties.last() == "BlendShapeChannel") {
                        QByteArray name = object.properties.at(1).toByteArray();

                        name = name.left(name.indexOf('\0'));
                        if (!blendshapeIndices.contains(name)) {
                            // try everything after the dot
                            name = name.mid(name.lastIndexOf('.') + 1);
                        }
                        QString id = getID(object.properties);
                        hfmModel.blendshapeChannelNames << name;
                        foreach (const WeightedIndex& index, blendshapeIndices.values(name)) {
                            blendshapeChannelIndices.insert(id, index);
                        }
                    }
                } else if (object.name == "AnimationCurve") {
                    AnimationCurve curve;
                    foreach (const FBXNode& subobject, object.children) {
                        if (subobject.name == "KeyValueFloat") {
                            curve.values = getFloatVector(subobject);
                        }
                    }
                    animationCurves.insert(getID(object.properties), curve);

                }
#if defined(DEBUG_FBXSERIALIZER)
                 else {
                    QString objectname = object.name.data();
                    if ( objectname == "Pose"
                        || objectname == "AnimationStack"
                        || objectname == "AnimationLayer"
                        || objectname == "AnimationCurveNode") {
                    } else {
                        unknown++;
                    }
                }
#endif
            }
        } else if (child.name == "Connections") {
            static const QVariant OO = QByteArray("OO");
            static const QVariant OP = QByteArray("OP");
            foreach (const FBXNode& connection, child.children) {
                if (connection.name == "C" || connection.name == "Connect") {
                    if (connection.properties.at(0) == OO) {
                        QString childID = getID(connection.properties, 1);
                        QString parentID = getID(connection.properties, 2);
                        ooChildToParent.insert(childID, parentID);
                        if (!hifiGlobalNodeID.isEmpty() && (parentID == hifiGlobalNodeID)) {
                            std::map< QString, HFMLight >::iterator lightIt = lights.find(childID);
                            if (lightIt != lights.end()) {
                                _lightmapLevel = (*lightIt).second.intensity;
                                if (_lightmapLevel <= 0.0f) {
                                    _loadLightmaps = false;
                                }
                                _lightmapOffset = glm::clamp((*lightIt).second.color.x, 0.f, 1.f);
                            }
                        }
                    } else if (connection.properties.at(0) == OP) {
                        int counter = 0;
                        QByteArray type = connection.properties.at(3).toByteArray().toLower();
                        if (type.contains("DiffuseFactor")) {
                            diffuseFactorTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if ((type.contains("diffuse") && !type.contains("tex_global_diffuse"))) {
                            diffuseTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("tex_color_map")) {
                            diffuseTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("transparentcolor")) { // Maya way of passing TransparentMap
                            transparentTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("transparencyfactor")) { // Blender way of passing TransparentMap
                            transparentTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("bump")) {
                            bumpTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("normal")) {
                            normalTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("tex_normal_map")) {
                            normalTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if ((type.contains("specular") && !type.contains("tex_global_specular")) || type.contains("reflection")) {
                            specularTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("tex_metallic_map")) {
                            metallicTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("shininess")) {
                            shininessTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("tex_roughness_map")) {
                            roughnessTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("emissive")) {
                            emissiveTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("tex_emissive_map")) {
                            emissiveTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("ambientcolor")) {
                            ambientTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("ambientfactor")) {
                            ambientFactorTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type.contains("tex_ao_map")) {
                            occlusionTextures.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type == "lcl rotation") {
                            localRotations.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type == "lcl translation") {
                            localTranslations.insert(getID(connection.properties, 2), getID(connection.properties, 1));

                        } else if (type == "d|x") {
                            xComponents.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type == "d|y") {
                            yComponents.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                        } else if (type == "d|z") {
                            zComponents.insert(getID(connection.properties, 2), getID(connection.properties, 1));

                        } else {
                            QString typenam = type.data();
                            counter++;
                        }
                    }
                    _connectionParentMap.insert(getID(connection.properties, 1), getID(connection.properties, 2));
                    _connectionChildMap.insert(getID(connection.properties, 2), getID(connection.properties, 1));
                }
            }
        }
#if defined(DEBUG_FBXSERIALIZER)
        else {
            QString objectname = child.name.data();
            if ( objectname == "Pose"
                || objectname == "CreationTime"
                || objectname == "FileId"
                || objectname == "Creator"
                || objectname == "Documents"
                || objectname == "References"
                || objectname == "Definitions"
                || objectname == "Takes"
                || objectname == "AnimationStack"
                || objectname == "AnimationLayer"
                || objectname == "AnimationCurveNode") {
            } else {
                unknown++;
            }
        }
#endif
    }

    // TODO: check if is code is needed
    if (!lights.empty()) {
        if (hifiGlobalNodeID.isEmpty()) {
            auto light = lights.begin();
            _lightmapLevel = (*light).second.intensity;
        }
    }

    // assign the blendshapes to their corresponding meshes
    foreach (const ExtractedBlendshape& extracted, blendshapes) {
        QString blendshapeChannelID = _connectionParentMap.value(extracted.id);
        QString blendshapeID = _connectionParentMap.value(blendshapeChannelID);
        QString meshID = _connectionParentMap.value(blendshapeID);
        addBlendshapes(extracted, blendshapeChannelIndices.values(blendshapeChannelID), meshes[meshID]);
    }

    // get offset transform from mapping
    float offsetScale = mapping.value("scale", 1.0f).toFloat() * unitScaleFactor * METERS_PER_CENTIMETER;
    glm::quat offsetRotation = glm::quat(glm::radians(glm::vec3(mapping.value("rx").toFloat(),
            mapping.value("ry").toFloat(), mapping.value("rz").toFloat())));
    hfmModel.offset = glm::translate(glm::vec3(mapping.value("tx").toFloat(), mapping.value("ty").toFloat(),
        mapping.value("tz").toFloat())) * glm::mat4_cast(offsetRotation) *
            glm::scale(glm::vec3(offsetScale, offsetScale, offsetScale));

    // get the list of models in depth-first traversal order
    QVector<QString> modelIDs;
    QSet<QString> remainingFBXModels;
    for (QHash<QString, FBXModel>::const_iterator fbxModel = fbxModels.constBegin(); fbxModel != fbxModels.constEnd(); fbxModel++) {
        // models with clusters must be parented to the cluster top
        // Unless the model is a root node.
        bool isARootNode = !modelIDs.contains(_connectionParentMap.value(fbxModel.key()));
        if (!isARootNode) {  
            foreach(const QString& deformerID, _connectionChildMap.values(fbxModel.key())) {
                foreach(const QString& clusterID, _connectionChildMap.values(deformerID)) {
                    if (!clusters.contains(clusterID)) {
                        continue;
                    }
                    QString topID = getTopModelID(_connectionParentMap, fbxModels, _connectionChildMap.value(clusterID), url);
                    _connectionChildMap.remove(_connectionParentMap.take(fbxModel.key()), fbxModel.key());
                    _connectionParentMap.insert(fbxModel.key(), topID);
                    goto outerBreak;
                }
            }
            outerBreak: ;
        }

        // make sure the parent is in the child map
        QString parent = _connectionParentMap.value(fbxModel.key());
        if (!_connectionChildMap.contains(parent, fbxModel.key())) {
            _connectionChildMap.insert(parent, fbxModel.key());
        }
        remainingFBXModels.insert(fbxModel.key());
    }
    while (!remainingFBXModels.isEmpty()) {
        QString first = *remainingFBXModels.constBegin();
        foreach (const QString& id, remainingFBXModels) {
            if (id < first) {
                first = id;
            }
        }
        QString topID = getTopModelID(_connectionParentMap, fbxModels, first, url);
        appendModelIDs(_connectionParentMap.value(topID), _connectionChildMap, fbxModels, remainingFBXModels, modelIDs, true);
    }

    // figure the number of animation frames from the curves
    int frameCount = 1;
    foreach (const AnimationCurve& curve, animationCurves) {
        frameCount = qMax(frameCount, curve.values.size());
    }
    for (int i = 0; i < frameCount; i++) {
        HFMAnimationFrame frame;
        frame.rotations.resize(modelIDs.size());
        frame.translations.resize(modelIDs.size());
        hfmModel.animationFrames.append(frame);
    }

    // convert the models to joints
    QVariantList freeJoints = mapping.values("freeJoint");
    hfmModel.hasSkeletonJoints = false;
    foreach (const QString& modelID, modelIDs) {
        const FBXModel& fbxModel = fbxModels[modelID];
        HFMJoint joint;
        joint.isFree = freeJoints.contains(fbxModel.name);
        joint.parentIndex = fbxModel.parentIndex;

        // get the indices of all ancestors starting with the first free one (if any)
        int jointIndex = hfmModel.joints.size();
        joint.freeLineage.append(jointIndex);
        int lastFreeIndex = joint.isFree ? 0 : -1;
        for (int index = joint.parentIndex; index != -1; index = hfmModel.joints.at(index).parentIndex) {
            if (hfmModel.joints.at(index).isFree) {
                lastFreeIndex = joint.freeLineage.size();
            }
            joint.freeLineage.append(index);
        }
        joint.freeLineage.remove(lastFreeIndex + 1, joint.freeLineage.size() - lastFreeIndex - 1);
        joint.translation = fbxModel.translation; // these are usually in centimeters
        joint.preTransform = fbxModel.preTransform;
        joint.preRotation = fbxModel.preRotation;
        joint.rotation = fbxModel.rotation;
        joint.postRotation = fbxModel.postRotation;
        joint.postTransform = fbxModel.postTransform;
        joint.rotationMin = fbxModel.rotationMin;
        joint.rotationMax = fbxModel.rotationMax;

        joint.hasGeometricOffset = fbxModel.hasGeometricOffset;
        joint.geometricTranslation = fbxModel.geometricTranslation;
        joint.geometricRotation = fbxModel.geometricRotation;
        joint.geometricScaling = fbxModel.geometricScaling;

        glm::quat combinedRotation = joint.preRotation * joint.rotation * joint.postRotation;

        if (joint.parentIndex == -1) {
            joint.transform = hfmModel.offset * glm::translate(joint.translation) * joint.preTransform *
                glm::mat4_cast(combinedRotation) * joint.postTransform;
            joint.inverseDefaultRotation = glm::inverse(combinedRotation);
            joint.distanceToParent = 0.0f;

        } else {
            const HFMJoint& parentJoint = hfmModel.joints.at(joint.parentIndex);
            joint.transform = parentJoint.transform * glm::translate(joint.translation) *
                joint.preTransform * glm::mat4_cast(combinedRotation) * joint.postTransform;
            joint.inverseDefaultRotation = glm::inverse(combinedRotation) * parentJoint.inverseDefaultRotation;
            joint.distanceToParent = glm::distance(extractTranslation(parentJoint.transform),
                extractTranslation(joint.transform));
        }
        joint.inverseBindRotation = joint.inverseDefaultRotation;
        joint.name = fbxModel.name;
        if (hfmModel.hfmToHifiJointNameMapping.contains(hfmModel.hfmToHifiJointNameMapping.key(joint.name))) {
            joint.name = hfmModel.hfmToHifiJointNameMapping.key(fbxModel.name);
        }

        foreach (const QString& childID, _connectionChildMap.values(modelID)) {
            QString type = typeFlags.value(childID);
            if (!type.isEmpty()) {
                hfmModel.hasSkeletonJoints |= (joint.isSkeletonJoint = type.toLower().contains("Skeleton"));
                break;
            }
        }

        joint.bindTransformFoundInCluster = false;

        hfmModel.joints.append(joint);
        hfmModel.jointIndices.insert(joint.name, hfmModel.joints.size());

        QString rotationID = localRotations.value(modelID);
        AnimationCurve xRotCurve = animationCurves.value(xComponents.value(rotationID));
        AnimationCurve yRotCurve = animationCurves.value(yComponents.value(rotationID));
        AnimationCurve zRotCurve = animationCurves.value(zComponents.value(rotationID));

        QString translationID = localTranslations.value(modelID);
        AnimationCurve xPosCurve = animationCurves.value(xComponents.value(translationID));
        AnimationCurve yPosCurve = animationCurves.value(yComponents.value(translationID));
        AnimationCurve zPosCurve = animationCurves.value(zComponents.value(translationID));

        glm::vec3 defaultRotValues = glm::degrees(safeEulerAngles(joint.rotation));
        glm::vec3 defaultPosValues = joint.translation;

        for (int i = 0; i < frameCount; i++) {
            hfmModel.animationFrames[i].rotations[jointIndex] = glm::quat(glm::radians(glm::vec3(
                xRotCurve.values.isEmpty() ? defaultRotValues.x : xRotCurve.values.at(i % xRotCurve.values.size()),
                yRotCurve.values.isEmpty() ? defaultRotValues.y : yRotCurve.values.at(i % yRotCurve.values.size()),
                zRotCurve.values.isEmpty() ? defaultRotValues.z : zRotCurve.values.at(i % zRotCurve.values.size()))));
            hfmModel.animationFrames[i].translations[jointIndex] = glm::vec3(
                xPosCurve.values.isEmpty() ? defaultPosValues.x : xPosCurve.values.at(i % xPosCurve.values.size()),
                yPosCurve.values.isEmpty() ? defaultPosValues.y : yPosCurve.values.at(i % yPosCurve.values.size()),
                zPosCurve.values.isEmpty() ? defaultPosValues.z : zPosCurve.values.at(i % zPosCurve.values.size()));
        }
    }

    // NOTE: shapeVertices are in joint-frame
    std::vector<ShapeVertices> shapeVertices;
    shapeVertices.resize(std::max(1, hfmModel.joints.size()) );

    hfmModel.bindExtents.reset();
    hfmModel.meshExtents.reset();

    // Create the Material Library
    consolidateHFMMaterials(mapping);

    // We can't allow the scaling of a given image to different sizes, because the hash used for the KTX cache is based on the original image
    // Allowing scaling of the same image to different sizes would cause different KTX files to target the same cache key
#if 0
    // HACK: until we get proper LOD management we're going to cap model textures
    // according to how many unique textures the model uses:
    //   1 - 8 textures --> 2048
    //   8 - 32 textures --> 1024
    //   33 - 128 textures --> 512
    // etc...
    QSet<QString> uniqueTextures;
    for (auto& material : _hfmMaterials) {
        material.getTextureNames(uniqueTextures);
    }
    int numTextures = uniqueTextures.size();
    const int MAX_NUM_TEXTURES_AT_MAX_RESOLUTION = 8;
    int maxWidth = sqrt(MAX_NUM_PIXELS_FOR_FBX_TEXTURE);

    if (numTextures > MAX_NUM_TEXTURES_AT_MAX_RESOLUTION) {
        int numTextureThreshold = MAX_NUM_TEXTURES_AT_MAX_RESOLUTION;
        const int MIN_MIP_TEXTURE_WIDTH = 64;
        do {
            maxWidth /= 2;
            numTextureThreshold *= 4;
        } while (numTextureThreshold < numTextures && maxWidth > MIN_MIP_TEXTURE_WIDTH);

        qCDebug(modelformat) << "Capped square texture width =" << maxWidth << "for model" << url << "with" << numTextures << "textures";
        for (auto& material : _hfmMaterials) {
            material.setMaxNumPixelsPerTexture(maxWidth * maxWidth);
        }
    }
#endif
    hfmModel.materials = _hfmMaterials;

    // see if any materials have texture children
    bool materialsHaveTextures = checkMaterialsHaveTextures(_hfmMaterials, _textureFilenames, _connectionChildMap);

    for (QMap<QString, ExtractedMesh>::iterator it = meshes.begin(); it != meshes.end(); it++) {
        ExtractedMesh& extracted = it.value();

        extracted.mesh.meshExtents.reset();

        // accumulate local transforms
        QString modelID = fbxModels.contains(it.key()) ? it.key() : _connectionParentMap.value(it.key());
        glm::mat4 modelTransform = getGlobalTransform(_connectionParentMap, fbxModels, modelID, hfmModel.applicationName == "mixamo.com", url);

        // compute the mesh extents from the transformed vertices
        foreach (const glm::vec3& vertex, extracted.mesh.vertices) {
            glm::vec3 transformedVertex = glm::vec3(modelTransform * glm::vec4(vertex, 1.0f));
            hfmModel.meshExtents.minimum = glm::min(hfmModel.meshExtents.minimum, transformedVertex);
            hfmModel.meshExtents.maximum = glm::max(hfmModel.meshExtents.maximum, transformedVertex);

            extracted.mesh.meshExtents.minimum = glm::min(extracted.mesh.meshExtents.minimum, transformedVertex);
            extracted.mesh.meshExtents.maximum = glm::max(extracted.mesh.meshExtents.maximum, transformedVertex);
            extracted.mesh.modelTransform = modelTransform;
        }

        // look for textures, material properties
        // allocate the Part material library
        int materialIndex = 0;
        int textureIndex = 0;
        bool generateTangents = false;
        QList<QString> children = _connectionChildMap.values(modelID);
        for (int i = children.size() - 1; i >= 0; i--) {

            const QString& childID = children.at(i);
            if (_hfmMaterials.contains(childID)) {
                // the pure material associated with this part
                HFMMaterial material = _hfmMaterials.value(childID);

                for (int j = 0; j < extracted.partMaterialTextures.size(); j++) {
                    if (extracted.partMaterialTextures.at(j).first == materialIndex) {
                        HFMMeshPart& part = extracted.mesh.parts[j];
                        part.materialID = material.materialID;
                        generateTangents |= material.needTangentSpace();
                    }
                }

                materialIndex++;

            } else if (_textureFilenames.contains(childID)) {
                HFMTexture texture = getTexture(childID);
                for (int j = 0; j < extracted.partMaterialTextures.size(); j++) {
                    int partTexture = extracted.partMaterialTextures.at(j).second;
                    if (partTexture == textureIndex && !(partTexture == 0 && materialsHaveTextures)) {
                        // TODO: DO something here that replaces this legacy code
                        // Maybe create a material just for this part with the correct textures?
                        // extracted.mesh.parts[j].diffuseTexture = texture;
                    }
                }
                textureIndex++;
            }
        }

        extracted.mesh.createMeshTangents(generateTangents);
        extracted.mesh.createBlendShapeTangents(generateTangents);

        // find the clusters with which the mesh is associated
        QVector<QString> clusterIDs;
        foreach (const QString& childID, _connectionChildMap.values(it.key())) {
            foreach (const QString& clusterID, _connectionChildMap.values(childID)) {
                if (!clusters.contains(clusterID)) {
                    continue;
                }
                HFMCluster hfmCluster;
                const Cluster& cluster = clusters[clusterID];
                clusterIDs.append(clusterID);

                // see http://stackoverflow.com/questions/13566608/loading-skinning-information-from-fbx for a discussion
                // of skinning information in FBX
                QString jointID = _connectionChildMap.value(clusterID);
                hfmCluster.jointIndex = modelIDs.indexOf(jointID);
                if (hfmCluster.jointIndex == -1) {
                    qCDebug(modelformat) << "Joint not in model list: " << jointID;
                    hfmCluster.jointIndex = 0;
                }

                hfmCluster.inverseBindMatrix = glm::inverse(cluster.transformLink) * modelTransform;

                // slam bottom row to (0, 0, 0, 1), we KNOW this is not a perspective matrix and
                // sometimes floating point fuzz can be introduced after the inverse.
                hfmCluster.inverseBindMatrix[0][3] = 0.0f;
                hfmCluster.inverseBindMatrix[1][3] = 0.0f;
                hfmCluster.inverseBindMatrix[2][3] = 0.0f;
                hfmCluster.inverseBindMatrix[3][3] = 1.0f;

                hfmCluster.inverseBindTransform = Transform(hfmCluster.inverseBindMatrix);

                extracted.mesh.clusters.append(hfmCluster);

                // override the bind rotation with the transform link
                HFMJoint& joint = hfmModel.joints[hfmCluster.jointIndex];
                joint.inverseBindRotation = glm::inverse(extractRotation(cluster.transformLink));
                joint.bindTransform = cluster.transformLink;
                joint.bindTransformFoundInCluster = true;

                // update the bind pose extents
                glm::vec3 bindTranslation = extractTranslation(hfmModel.offset * joint.bindTransform);
                hfmModel.bindExtents.addPoint(bindTranslation);
            }
        }

        // if we don't have a skinned joint, parent to the model itself
        if (extracted.mesh.clusters.isEmpty()) {
            HFMCluster cluster;
            cluster.jointIndex = modelIDs.indexOf(modelID);
            if (cluster.jointIndex == -1) {
                qCDebug(modelformat) << "Model not in model list: " << modelID;
                cluster.jointIndex = 0;
            }
            extracted.mesh.clusters.append(cluster);
        }

        // whether we're skinned depends on how many clusters are attached
        const HFMCluster& firstHFMCluster = extracted.mesh.clusters.at(0);
        glm::mat4 inverseModelTransform = glm::inverse(modelTransform);
        if (clusterIDs.size() > 1) {
            // this is a multi-mesh joint
            const int WEIGHTS_PER_VERTEX = 4;
            int numClusterIndices = extracted.mesh.vertices.size() * WEIGHTS_PER_VERTEX;
            extracted.mesh.clusterIndices.fill(0, numClusterIndices);
            QVector<float> weightAccumulators;
            weightAccumulators.fill(0.0f, numClusterIndices);

            for (int i = 0; i < clusterIDs.size(); i++) {
                QString clusterID = clusterIDs.at(i);
                const Cluster& cluster = clusters[clusterID];
                const HFMCluster& hfmCluster = extracted.mesh.clusters.at(i);
                int jointIndex = hfmCluster.jointIndex;
                HFMJoint& joint = hfmModel.joints[jointIndex];
                glm::mat4 transformJointToMesh = inverseModelTransform * joint.bindTransform;
                glm::vec3 boneEnd = extractTranslation(transformJointToMesh);
                glm::vec3 boneBegin = boneEnd;
                glm::vec3 boneDirection;
                float boneLength = 0.0f;
                if (joint.parentIndex != -1) {
                    boneBegin = extractTranslation(inverseModelTransform * hfmModel.joints[joint.parentIndex].bindTransform);
                    boneDirection = boneEnd - boneBegin;
                    boneLength = glm::length(boneDirection);
                    if (boneLength > EPSILON) {
                        boneDirection /= boneLength;
                    }
                }

                glm::mat4 meshToJoint = glm::inverse(joint.bindTransform) * modelTransform;
                ShapeVertices& points = shapeVertices.at(jointIndex);

                for (int j = 0; j < cluster.indices.size(); j++) {
                    int oldIndex = cluster.indices.at(j);
                    float weight = cluster.weights.at(j);
                    for (QMultiHash<int, int>::const_iterator it = extracted.newIndices.constFind(oldIndex);
                            it != extracted.newIndices.end() && it.key() == oldIndex; it++) {
                        int newIndex = it.value();

                        // remember vertices with at least 1/4 weight
                        const float EXPANSION_WEIGHT_THRESHOLD = 0.25f;
                        if (weight >= EXPANSION_WEIGHT_THRESHOLD) {
                            // transform to joint-frame and save for later
                            const glm::mat4 vertexTransform = meshToJoint * glm::translate(extracted.mesh.vertices.at(newIndex));
                            points.push_back(extractTranslation(vertexTransform));
                        }

                        // look for an unused slot in the weights vector
                        int weightIndex = newIndex * WEIGHTS_PER_VERTEX;
                        int lowestIndex = -1;
                        float lowestWeight = FLT_MAX;
                        int k = 0;
                        for (; k < WEIGHTS_PER_VERTEX; k++) {
                            if (weightAccumulators[weightIndex + k] == 0.0f) {
                                extracted.mesh.clusterIndices[weightIndex + k] = i;
                                weightAccumulators[weightIndex + k] = weight;
                                break;
                            }
                            if (weightAccumulators[weightIndex + k] < lowestWeight) {
                                lowestIndex = k;
                                lowestWeight = weightAccumulators[weightIndex + k];
                            }
                        }
                        if (k == WEIGHTS_PER_VERTEX && weight > lowestWeight) {
                            // no space for an additional weight; we must replace the lowest
                            weightAccumulators[weightIndex + lowestIndex] = weight;
                            extracted.mesh.clusterIndices[weightIndex + lowestIndex] = i;
                        }
                    }
                }
            }

            // now that we've accumulated the most relevant weights for each vertex
            // normalize and compress to 16-bits
            extracted.mesh.clusterWeights.fill(0, numClusterIndices);
            int numVertices = extracted.mesh.vertices.size();
            for (int i = 0; i < numVertices; ++i) {
                int j = i * WEIGHTS_PER_VERTEX;

                // normalize weights into uint16_t
                float totalWeight = weightAccumulators[j];
                for (int k = j + 1; k < j + WEIGHTS_PER_VERTEX; ++k) {
                    totalWeight += weightAccumulators[k];
                }
                if (totalWeight > 0.0f) {
                    const float ALMOST_HALF = 0.499f;
                    float weightScalingFactor = (float)(UINT16_MAX) / totalWeight;
                    for (int k = j; k < j + WEIGHTS_PER_VERTEX; ++k) {
                        extracted.mesh.clusterWeights[k] = (uint16_t)(weightScalingFactor * weightAccumulators[k] + ALMOST_HALF);
                    }
                }
            }
        } else {
            // this is a single-mesh joint
            int jointIndex = firstHFMCluster.jointIndex;
            HFMJoint& joint = hfmModel.joints[jointIndex];

            // transform cluster vertices to joint-frame and save for later
            glm::mat4 meshToJoint = glm::inverse(joint.bindTransform) * modelTransform;
            ShapeVertices& points = shapeVertices.at(jointIndex);
            foreach (const glm::vec3& vertex, extracted.mesh.vertices) {
                const glm::mat4 vertexTransform = meshToJoint * glm::translate(vertex);
                points.push_back(extractTranslation(vertexTransform));
            }

            // Apply geometric offset, if present, by transforming the vertices directly
            if (joint.hasGeometricOffset) {
                glm::mat4 geometricOffset = createMatFromScaleQuatAndPos(joint.geometricScaling, joint.geometricRotation, joint.geometricTranslation);
                for (int i = 0; i < extracted.mesh.vertices.size(); i++) {
                    extracted.mesh.vertices[i] = transformPoint(geometricOffset, extracted.mesh.vertices[i]);
                }
            }
        }

        hfmModel.meshes.append(extracted.mesh);
        int meshIndex = hfmModel.meshes.size() - 1;
        meshIDsToMeshIndices.insert(it.key(), meshIndex);
    }

    const float INV_SQRT_3 = 0.57735026918f;
    ShapeVertices cardinalDirections = {
        Vectors::UNIT_X,
        Vectors::UNIT_Y,
        Vectors::UNIT_Z,
        glm::vec3(INV_SQRT_3,  INV_SQRT_3,  INV_SQRT_3),
        glm::vec3(INV_SQRT_3, -INV_SQRT_3,  INV_SQRT_3),
        glm::vec3(INV_SQRT_3,  INV_SQRT_3, -INV_SQRT_3),
        glm::vec3(INV_SQRT_3, -INV_SQRT_3, -INV_SQRT_3)
    };

    // now that all joints have been scanned compute a k-Dop bounding volume of mesh
    for (int i = 0; i < hfmModel.joints.size(); ++i) {
        HFMJoint& joint = hfmModel.joints[i];

        // NOTE: points are in joint-frame
        ShapeVertices& points = shapeVertices.at(i);
        if (points.size() > 0) {
            // compute average point
            glm::vec3 avgPoint = glm::vec3(0.0f);
            for (uint32_t j = 0; j < points.size(); ++j) {
                avgPoint += points[j];
            }
            avgPoint /= (float)points.size();
            joint.shapeInfo.avgPoint = avgPoint;

            // compute a k-Dop bounding volume
            for (uint32_t j = 0; j < cardinalDirections.size(); ++j) {
                float maxDot = -FLT_MAX;
                float minDot = FLT_MIN;
                for (uint32_t k = 0; k < points.size(); ++k) {
                    float kDot = glm::dot(cardinalDirections[j], points[k] - avgPoint);
                    if (kDot > maxDot) {
                        maxDot = kDot;
                    }
                    if (kDot < minDot) {
                        minDot = kDot;
                    }
                }
                joint.shapeInfo.points.push_back(avgPoint + maxDot * cardinalDirections[j]);
                joint.shapeInfo.dots.push_back(maxDot);
                joint.shapeInfo.points.push_back(avgPoint + minDot * cardinalDirections[j]);
                joint.shapeInfo.dots.push_back(-minDot);
            }
            generateBoundryLinesForDop14(joint.shapeInfo.dots, joint.shapeInfo.avgPoint, joint.shapeInfo.debugLines);
        }
    }
    hfmModel.palmDirection = parseVec3(mapping.value("palmDirection", "0, -1, 0").toString());

    // attempt to map any meshes to a named model
    for (QHash<QString, int>::const_iterator m = meshIDsToMeshIndices.constBegin();
            m != meshIDsToMeshIndices.constEnd(); m++) {

        const QString& meshID = m.key();
        int meshIndex = m.value();

        if (ooChildToParent.contains(meshID)) {
            const QString& modelID = ooChildToParent.value(meshID);
            if (modelIDsToNames.contains(modelID)) {
                const QString& modelName = modelIDsToNames.value(modelID);
                hfmModel.meshIndicesToModelNames.insert(meshIndex, modelName);
            }
        }
    }

    auto offsets = getJointRotationOffsets(mapping);
    hfmModel.jointRotationOffsets.clear();
    for (auto itr = offsets.begin(); itr != offsets.end(); itr++) {
        QString jointName = itr.key();
        glm::quat rotationOffset = itr.value();
        int jointIndex = hfmModel.getJointIndex(jointName);
        if (hfmModel.hfmToHifiJointNameMapping.contains(jointName)) {
            jointIndex = hfmModel.getJointIndex(jointName);
        }
        if (jointIndex != -1) {
            hfmModel.jointRotationOffsets.insert(jointIndex, rotationOffset);
        }
        qCDebug(modelformat) << "Joint Rotation Offset added to Rig._jointRotationOffsets : " << " jointName: " << jointName << " jointIndex: " << jointIndex << " rotation offset: " << rotationOffset;
    }

    return hfmModelPtr;
}

MediaType FBXSerializer::getMediaType() const {
    MediaType mediaType("fbx");
    mediaType.extensions.push_back("fbx");
    mediaType.fileSignatures.emplace_back("Kaydara FBX Binary  \x00", 0);
    return mediaType;
}

std::unique_ptr<hfm::Serializer::Factory> FBXSerializer::getFactory() const {
    return std::make_unique<hfm::Serializer::SimpleFactory<FBXSerializer>>();
}

HFMModel::Pointer FBXSerializer::read(const QByteArray& data, const QVariantHash& mapping, const QUrl& url) {
    QBuffer buffer(const_cast<QByteArray*>(&data));
    buffer.open(QIODevice::ReadOnly);

    _rootNode = parseFBX(&buffer);

    return HFMModel::Pointer(extractHFMModel(mapping, url.toString()));
}
