//
//  Image.cpp
//  image/src/image
//
//  Created by Clement Brisset on 4/5/2017.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Image.h"

#include <glm/gtc/packing.hpp>

#include <QtCore/QtGlobal>
#include <QUrl>
#include <QImage>
#include <QRgb>
#include <QBuffer>
#include <QImageReader>

#include <Finally.h>
#include <Profile.h>
#include <StatTracker.h>
#include <GLMHelpers.h>

#include "TGAReader.h"

#include "ImageLogging.h"

using namespace gpu;

#define CPU_MIPMAPS 1
#include <nvtt/nvtt.h>

#undef _CRT_SECURE_NO_WARNINGS
#include <Etc2/Etc.h>
#include <Etc2/EtcFilter.h>

static const glm::uvec2 SPARSE_PAGE_SIZE(128);
static const glm::uvec2 MAX_TEXTURE_SIZE_GLES(2048);
static const glm::uvec2 MAX_TEXTURE_SIZE_GL(4096);
bool DEV_DECIMATE_TEXTURES = false;
std::atomic<size_t> DECIMATED_TEXTURE_COUNT{ 0 };
std::atomic<size_t> RECTIFIED_TEXTURE_COUNT{ 0 };

// we use a ref here to work around static order initialization
// possibly causing the element not to be constructed yet
static const auto& HDR_FORMAT = gpu::Element::COLOR_R11G11B10;

uint rectifyDimension(const uint& dimension) {
    if (dimension == 0) {
        return 0;
    }
    if (dimension < SPARSE_PAGE_SIZE.x) {
        uint newSize = SPARSE_PAGE_SIZE.x;
        while (dimension <= newSize / 2) {
            newSize /= 2;
        }
        return newSize;
    } else {
        uint pages = (dimension / SPARSE_PAGE_SIZE.x) + (dimension % SPARSE_PAGE_SIZE.x == 0 ? 0 : 1);
        return pages * SPARSE_PAGE_SIZE.x;
    }
}

glm::uvec2 rectifySize(const glm::uvec2& size) {
    return { rectifyDimension(size.x), rectifyDimension(size.y) };
}


namespace image {

const QStringList getSupportedFormats() {
    auto formats = QImageReader::supportedImageFormats();
    QStringList stringFormats;
    std::transform(formats.begin(), formats.end(), std::back_inserter(stringFormats),
                   [](QByteArray& format) -> QString { return format; });
    return stringFormats;
}


// On GLES, we don't use HDR skyboxes
QImage::Format hdrFormatForTarget(BackendTarget target) {
    if (target == BackendTarget::GLES32) {
        return QImage::Format_RGB32;
    }
    return QImage::Format_RGB30;
}

TextureUsage::TextureLoader TextureUsage::getTextureLoaderForType(Type type, const QVariantMap& options) {
    switch (type) {
        case ALBEDO_TEXTURE:
            return image::TextureUsage::createAlbedoTextureFromImage;
        case EMISSIVE_TEXTURE:
            return image::TextureUsage::createEmissiveTextureFromImage;
        case LIGHTMAP_TEXTURE:
            return image::TextureUsage::createLightmapTextureFromImage;
        case CUBE_TEXTURE:
            if (options.value("generateIrradiance", true).toBool()) {
                return image::TextureUsage::createCubeTextureFromImage;
            } else {
                return image::TextureUsage::createCubeTextureFromImageWithoutIrradiance;
            }
        case BUMP_TEXTURE:
            return image::TextureUsage::createNormalTextureFromBumpImage;
        case NORMAL_TEXTURE:
            return image::TextureUsage::createNormalTextureFromNormalImage;
        case ROUGHNESS_TEXTURE:
            return image::TextureUsage::createRoughnessTextureFromImage;
        case GLOSS_TEXTURE:
            return image::TextureUsage::createRoughnessTextureFromGlossImage;
        case SPECULAR_TEXTURE:
            return image::TextureUsage::createMetallicTextureFromImage;
        case STRICT_TEXTURE:
            return image::TextureUsage::createStrict2DTextureFromImage;

        case DEFAULT_TEXTURE:
        default:
            return image::TextureUsage::create2DTextureFromImage;
    }
}

gpu::TexturePointer TextureUsage::createStrict2DTextureFromImage(QImage&& srcImage, const std::string& srcImageName,
                                                                 bool compress, BackendTarget target, const std::atomic<bool>& abortProcessing) {
    return process2DTextureColorFromImage(std::move(srcImage), srcImageName, compress, target, true, abortProcessing);
}

gpu::TexturePointer TextureUsage::create2DTextureFromImage(QImage&& srcImage, const std::string& srcImageName,
                                                           bool compress, BackendTarget target, const std::atomic<bool>& abortProcessing) {
    return process2DTextureColorFromImage(std::move(srcImage), srcImageName, compress, target, false, abortProcessing);
}

gpu::TexturePointer TextureUsage::createAlbedoTextureFromImage(QImage&& srcImage, const std::string& srcImageName,
                                                               bool compress, BackendTarget target, const std::atomic<bool>& abortProcessing) {
    return process2DTextureColorFromImage(std::move(srcImage), srcImageName, compress, target, false, abortProcessing);
}

gpu::TexturePointer TextureUsage::createEmissiveTextureFromImage(QImage&& srcImage, const std::string& srcImageName,
                                                                 bool compress, BackendTarget target, const std::atomic<bool>& abortProcessing) {
    return process2DTextureColorFromImage(std::move(srcImage), srcImageName, compress, target, false, abortProcessing);
}

gpu::TexturePointer TextureUsage::createLightmapTextureFromImage(QImage&& srcImage, const std::string& srcImageName,
                                                                 bool compress, BackendTarget target, const std::atomic<bool>& abortProcessing) {
    return process2DTextureColorFromImage(std::move(srcImage), srcImageName, compress, target, false, abortProcessing);
}

gpu::TexturePointer TextureUsage::createNormalTextureFromNormalImage(QImage&& srcImage, const std::string& srcImageName,
                                                                     bool compress, BackendTarget target, const std::atomic<bool>& abortProcessing) {
    return process2DTextureNormalMapFromImage(std::move(srcImage), srcImageName, compress, target, false, abortProcessing);
}

gpu::TexturePointer TextureUsage::createNormalTextureFromBumpImage(QImage&& srcImage, const std::string& srcImageName,
                                                                   bool compress, BackendTarget target, const std::atomic<bool>& abortProcessing) {
    return process2DTextureNormalMapFromImage(std::move(srcImage), srcImageName, compress, target, true, abortProcessing);
}

gpu::TexturePointer TextureUsage::createRoughnessTextureFromImage(QImage&& srcImage, const std::string& srcImageName,
                                                                  bool compress, BackendTarget target, const std::atomic<bool>& abortProcessing) {
    return process2DTextureGrayscaleFromImage(std::move(srcImage), srcImageName, compress, target, false, abortProcessing);
}

gpu::TexturePointer TextureUsage::createRoughnessTextureFromGlossImage(QImage&& srcImage, const std::string& srcImageName,
                                                                       bool compress, BackendTarget target, const std::atomic<bool>& abortProcessing) {
    return process2DTextureGrayscaleFromImage(std::move(srcImage), srcImageName, compress, target, true, abortProcessing);
}

gpu::TexturePointer TextureUsage::createMetallicTextureFromImage(QImage&& srcImage, const std::string& srcImageName,
                                                                 bool compress, BackendTarget target, const std::atomic<bool>& abortProcessing) {
    return process2DTextureGrayscaleFromImage(std::move(srcImage), srcImageName, compress, target, false, abortProcessing);
}

gpu::TexturePointer TextureUsage::createCubeTextureFromImage(QImage&& srcImage, const std::string& srcImageName,
                                                             bool compress, BackendTarget target, const std::atomic<bool>& abortProcessing) {
    return processCubeTextureColorFromImage(std::move(srcImage), srcImageName, compress, target, true, abortProcessing);
}

gpu::TexturePointer TextureUsage::createCubeTextureFromImageWithoutIrradiance(QImage&& srcImage, const std::string& srcImageName,
                                                                              bool compress, BackendTarget target, const std::atomic<bool>& abortProcessing) {
    return processCubeTextureColorFromImage(std::move(srcImage), srcImageName, compress, target, false, abortProcessing);
}

static float denormalize(float value, const float minValue) {
    return value < minValue ? 0.0f : value;
}

uint32 packR11G11B10F(const glm::vec3& color) {
    // Denormalize else unpacking gives high and incorrect values
    // See https://www.khronos.org/opengl/wiki/Small_Float_Formats for this min value
    static const auto minValue = 6.10e-5f;
    static const auto maxValue = 6.50e4f;
    glm::vec3 ucolor;
    ucolor.r = denormalize(color.r, minValue);
    ucolor.g = denormalize(color.g, minValue);
    ucolor.b = denormalize(color.b, minValue);
    ucolor.r = std::min(ucolor.r, maxValue);
    ucolor.g = std::min(ucolor.g, maxValue);
    ucolor.b = std::min(ucolor.b, maxValue);
    return glm::packF2x11_1x10(ucolor);
}

QImage processRawImageData(QIODevice& content, const std::string& filename) {
    // Help the QImage loader by extracting the image file format from the url filename ext.
    // Some tga are not created properly without it.
    auto filenameExtension = filename.substr(filename.find_last_of('.') + 1);
    content.open(QIODevice::ReadOnly);

    if (filenameExtension == "tga") {
        QImage image = image::readTGA(content);
        if (!image.isNull()) {
            return image;
        }
        content.reset();
    }

    QImageReader imageReader(&content, filenameExtension.c_str());

    if (imageReader.canRead()) {
        return imageReader.read();
    } else {
        // Extension could be incorrect, try to detect the format from the content
        QImageReader newImageReader;
        newImageReader.setDecideFormatFromContent(true);
        content.reset();
        newImageReader.setDevice(&content);

        if (newImageReader.canRead()) {
            return newImageReader.read();
        }
    }

    return QImage();
}

void mapToRedChannel(QImage& image, ColorChannel sourceChannel) {
    // Change format of image so we know exactly how to process it
    if (image.format() != QImage::Format_ARGB32) {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }

    for (int i = 0; i < image.height(); i++) {
        QRgb* pixel = reinterpret_cast<QRgb*>(image.scanLine(i));
        // Past end pointer
        QRgb* lineEnd = pixel + image.width();

        // Transfer channel data from source to target
        for (; pixel < lineEnd; pixel++) {
            int colorValue;
            switch (sourceChannel) {
            case ColorChannel::RED:
                colorValue = qRed(*pixel);
                break;
            case ColorChannel::GREEN:
                colorValue = qGreen(*pixel);
                break;
            case ColorChannel::BLUE:
                colorValue = qBlue(*pixel);
                break;
            case ColorChannel::ALPHA:
                colorValue = qAlpha(*pixel);
                break;
            default:
                colorValue = qRed(*pixel);
                break;
            }

            // Dump the color in the red channel, ignore the rest
            *pixel = qRgba(colorValue, 0, 0, 255);
        }
    }
}

gpu::TexturePointer processImage(std::shared_ptr<QIODevice> content, const std::string& filename, ColorChannel sourceChannel,
                                 int maxNumPixels, TextureUsage::Type textureType,
                                 bool compress, BackendTarget target, const std::atomic<bool>& abortProcessing) {

    QImage image = processRawImageData(*content.get(), filename);
    // Texture content can take up a lot of memory. Here we release our ownership of that content
    // in case it can be released.
    content.reset();

    int imageWidth = image.width();
    int imageHeight = image.height();

    // Validate that the image loaded
    if (imageWidth == 0 || imageHeight == 0 || image.format() == QImage::Format_Invalid) {
        QString reason(image.format() == QImage::Format_Invalid ? "(Invalid Format)" : "(Size is invalid)");
        qCWarning(imagelogging) << "Failed to load:" << qPrintable(reason);
        return nullptr;
    }

    // Validate the image is less than _maxNumPixels, and downscale if necessary
    if (imageWidth * imageHeight > maxNumPixels) {
        float scaleFactor = sqrtf(maxNumPixels / (float)(imageWidth * imageHeight));
        int originalWidth = imageWidth;
        int originalHeight = imageHeight;
        imageWidth = (int)(scaleFactor * (float)imageWidth + 0.5f);
        imageHeight = (int)(scaleFactor * (float)imageHeight + 0.5f);
        image = image.scaled(QSize(imageWidth, imageHeight), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        qCDebug(imagelogging).nospace() << "Downscaled " << " (" <<
            QSize(originalWidth, originalHeight) << " to " <<
            QSize(imageWidth, imageHeight) << ")";
    }

    // Re-map to image with single red channel texture if requested
    if (sourceChannel != ColorChannel::NONE) {
        mapToRedChannel(image, sourceChannel);
    }
    
    auto loader = TextureUsage::getTextureLoaderForType(textureType);
    auto texture = loader(std::move(image), filename, compress, target, abortProcessing);

    return texture;
}

QImage processSourceImage(QImage&& srcImage, bool cubemap, BackendTarget target) {
    PROFILE_RANGE(resource_parse, "processSourceImage");

    // Take a local copy to force move construction
    // https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#f18-for-consume-parameters-pass-by-x-and-stdmove-the-parameter
    QImage localCopy = std::move(srcImage);

    const glm::uvec2 srcImageSize = toGlm(localCopy.size());
    glm::uvec2 targetSize = srcImageSize;

    const auto maxTextureSize = target == BackendTarget::GLES32 ? MAX_TEXTURE_SIZE_GLES : MAX_TEXTURE_SIZE_GL;
    while (glm::any(glm::greaterThan(targetSize, maxTextureSize))) {
        targetSize /= 2;
    }
    if (targetSize != srcImageSize) {
        ++DECIMATED_TEXTURE_COUNT;
    }

    if (!cubemap) {
        auto rectifiedSize = rectifySize(targetSize);
        if (rectifiedSize != targetSize) {
            ++RECTIFIED_TEXTURE_COUNT;
            targetSize = rectifiedSize;
        }
    }

    if (DEV_DECIMATE_TEXTURES && glm::all(glm::greaterThanEqual(targetSize / SPARSE_PAGE_SIZE, glm::uvec2(2)))) {
        targetSize /= 2;
    }

    if (targetSize != srcImageSize) {
        PROFILE_RANGE(resource_parse, "processSourceImage Rectify");
        qCDebug(imagelogging) << "Resizing texture from " << srcImageSize.x << "x" << srcImageSize.y << " to " << targetSize.x << "x" << targetSize.y;
        return localCopy.scaled(fromGlm(targetSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    return localCopy;
}

#if defined(NVTT_API)
struct OutputHandler : public nvtt::OutputHandler {
    OutputHandler(gpu::Texture* texture, int face) : _texture(texture), _face(face) {}

    virtual void beginImage(int size, int width, int height, int depth, int face, int miplevel) override {
        _size = size;
        _miplevel = miplevel;

        _data = static_cast<gpu::Byte*>(malloc(size));
        _current = _data;
    }

    virtual bool writeData(const void* data, int size) override {
        assert(_current + size <= _data + _size);
        memcpy(_current, data, size);
        _current += size;
        return true;
    }

    virtual void endImage() override {
        if (_face >= 0) {
            _texture->assignStoredMipFace(_miplevel, _face, _size, static_cast<const gpu::Byte*>(_data));
        } else {
            _texture->assignStoredMip(_miplevel, _size, static_cast<const gpu::Byte*>(_data));
        }
        free(_data);
        _data = nullptr;
    }

    gpu::Byte* _data{ nullptr };
    gpu::Byte* _current{ nullptr };
    gpu::Texture* _texture{ nullptr };
    int _miplevel = 0;
    int _size = 0;
    int _face = -1;
};

struct PackedFloatOutputHandler : public OutputHandler {
    PackedFloatOutputHandler(gpu::Texture* texture, int face, gpu::Element format) : OutputHandler(texture, face) {
        if (format == gpu::Element::COLOR_RGB9E5) {
            _packFunc = glm::packF3x9_E1x5;
        } else if (format == gpu::Element::COLOR_R11G11B10) {
            _packFunc = packR11G11B10F;
        } else {
            qCWarning(imagelogging) << "Unknown handler format";
            Q_UNREACHABLE();
        }
    }

    virtual void beginImage(int size, int width, int height, int depth, int face, int miplevel) override {
        // Divide by 3 because we will compress from 3*floats to 1 uint32
        OutputHandler::beginImage(size / 3, width, height, depth, face, miplevel);
    }
    virtual bool writeData(const void* data, int size) override {
        // Expecting to write multiple of floats
        if (_packFunc) {
            assert((size % sizeof(float)) == 0);
            auto floatCount = size / sizeof(float);
            const float* floatBegin = (const float*)data;
            const float* floatEnd = floatBegin + floatCount;

            while (floatBegin < floatEnd) {
                _pixel[_coordIndex] = *floatBegin;
                floatBegin++;
                _coordIndex++;
                if (_coordIndex == 3) {
                    uint32 packedRGB = _packFunc(_pixel);
                    _coordIndex = 0;
                    OutputHandler::writeData(&packedRGB, sizeof(packedRGB));
                }
            }
            return true;
        }
        return false;
    }

    std::function<uint32(const glm::vec3&)> _packFunc;
    glm::vec3 _pixel;
    int _coordIndex{ 0 };
};

struct MyErrorHandler : public nvtt::ErrorHandler {
    virtual void error(nvtt::Error e) override {
        qCWarning(imagelogging) << "Texture compression error:" << nvtt::errorString(e);
    }
};

class SequentialTaskDispatcher : public nvtt::TaskDispatcher {
public:
    SequentialTaskDispatcher(const std::atomic<bool>& abortProcessing) : _abortProcessing(abortProcessing) {};

    const std::atomic<bool>& _abortProcessing;

    virtual void dispatch(nvtt::Task* task, void* context, int count) override {
        for (int i = 0; i < count; i++) {
            if (!_abortProcessing.load()) {
                task(context, i);
            } else {
                break;
            }
        }
    }
};

void generateHDRMips(gpu::Texture* texture, QImage&& image, BackendTarget target, const std::atomic<bool>& abortProcessing, int face) {
    // Take a local copy to force move construction
    // https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#f18-for-consume-parameters-pass-by-x-and-stdmove-the-parameter
    QImage localCopy = std::move(image);

    assert(localCopy.format() == hdrFormatForTarget(target));

    const int width = localCopy.width(), height = localCopy.height();
    std::vector<glm::vec4> data;
    std::vector<glm::vec4>::iterator dataIt;
    auto mipFormat = texture->getStoredMipFormat();
    std::function<glm::vec3(uint32)> unpackFunc;

    nvtt::InputFormat inputFormat = nvtt::InputFormat_RGBA_32F;
    nvtt::WrapMode wrapMode = nvtt::WrapMode_Mirror;
    nvtt::AlphaMode alphaMode = nvtt::AlphaMode_None;

    nvtt::CompressionOptions compressionOptions;
    compressionOptions.setQuality(nvtt::Quality_Production);

    // TODO: gles: generate ETC mips instead?
    if (mipFormat == gpu::Element::COLOR_COMPRESSED_BCX_HDR_RGB) {
        compressionOptions.setFormat(nvtt::Format_BC6);
    } else if (mipFormat == gpu::Element::COLOR_RGB9E5) {
        compressionOptions.setFormat(nvtt::Format_RGB);
        compressionOptions.setPixelType(nvtt::PixelType_Float);
        compressionOptions.setPixelFormat(32, 32, 32, 0);
    } else if (mipFormat == gpu::Element::COLOR_R11G11B10) {
        compressionOptions.setFormat(nvtt::Format_RGB);
        compressionOptions.setPixelType(nvtt::PixelType_Float);
        compressionOptions.setPixelFormat(32, 32, 32, 0);
    } else {
        qCWarning(imagelogging) << "Unknown mip format";
        Q_UNREACHABLE();
        return;
    }

    if (HDR_FORMAT == gpu::Element::COLOR_RGB9E5) {
        unpackFunc = glm::unpackF3x9_E1x5;
    } else if (HDR_FORMAT == gpu::Element::COLOR_R11G11B10) {
        unpackFunc = glm::unpackF2x11_1x10;
    } else {
        qCWarning(imagelogging) << "Unknown HDR encoding format in QImage";
        Q_UNREACHABLE();
        return;
    }

    data.resize(width * height);
    dataIt = data.begin();
    for (auto lineNb = 0; lineNb < height; lineNb++) {
        const uint32* srcPixelIt = reinterpret_cast<const uint32*>(localCopy.constScanLine(lineNb));
        const uint32* srcPixelEnd = srcPixelIt + width;

        while (srcPixelIt < srcPixelEnd) {
            *dataIt = glm::vec4(unpackFunc(*srcPixelIt), 1.0f);
            ++srcPixelIt;
            ++dataIt;
        }
    }
    assert(dataIt == data.end());

    // We're done with the localCopy, free up the memory to avoid bloating the heap
    localCopy = QImage(); // QImage doesn't have a clear function, so override it with an empty one.

    nvtt::OutputOptions outputOptions;
    outputOptions.setOutputHeader(false);
    std::unique_ptr<nvtt::OutputHandler> outputHandler;
    MyErrorHandler errorHandler;
    outputOptions.setErrorHandler(&errorHandler);
    nvtt::Context context;
    int mipLevel = 0;

    if (mipFormat == gpu::Element::COLOR_RGB9E5 || mipFormat == gpu::Element::COLOR_R11G11B10) {
        // Don't use NVTT (at least version 2.1) as it outputs wrong RGB9E5 and R11G11B10F values from floats
        outputHandler.reset(new PackedFloatOutputHandler(texture, face, mipFormat));
    } else {
        outputHandler.reset(new OutputHandler(texture, face));
    }

    outputOptions.setOutputHandler(outputHandler.get());

    nvtt::Surface surface;
    surface.setImage(inputFormat, width, height, 1, &(*data.begin()));
    surface.setAlphaMode(alphaMode);
    surface.setWrapMode(wrapMode);

    SequentialTaskDispatcher dispatcher(abortProcessing);
    nvtt::Compressor compressor;
    context.setTaskDispatcher(&dispatcher);

    context.compress(surface, face, mipLevel++, compressionOptions, outputOptions);
    while (surface.canMakeNextMipmap() && !abortProcessing.load()) {
        surface.buildNextMipmap(nvtt::MipmapFilter_Box);
        context.compress(surface, face, mipLevel++, compressionOptions, outputOptions);
    }
}

void generateLDRMips(gpu::Texture* texture, QImage&& image, BackendTarget target, const std::atomic<bool>& abortProcessing, int face) {
    // Take a local copy to force move construction
    // https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#f18-for-consume-parameters-pass-by-x-and-stdmove-the-parameter
    QImage localCopy = std::move(image);

    if (localCopy.format() != QImage::Format_ARGB32 && localCopy.format() != hdrFormatForTarget(target)) {
        localCopy = localCopy.convertToFormat(QImage::Format_ARGB32);
    }

    const int width = localCopy.width(), height = localCopy.height();
    auto mipFormat = texture->getStoredMipFormat();

    if (target != BackendTarget::GLES32) {
        const void* data = static_cast<const void*>(localCopy.constBits());
        nvtt::TextureType textureType = nvtt::TextureType_2D;
        nvtt::InputFormat inputFormat = nvtt::InputFormat_BGRA_8UB;
        nvtt::WrapMode wrapMode = nvtt::WrapMode_Mirror;
        nvtt::RoundMode roundMode = nvtt::RoundMode_None;
        nvtt::AlphaMode alphaMode = nvtt::AlphaMode_None;

        float inputGamma = 2.2f;
        float outputGamma = 2.2f;

        nvtt::InputOptions inputOptions;
        inputOptions.setTextureLayout(textureType, width, height);

        inputOptions.setMipmapData(data, width, height);
        // setMipmapData copies the memory, so free up the memory afterward to avoid bloating the heap
        data = nullptr;
        localCopy = QImage(); // QImage doesn't have a clear function, so override it with an empty one.

        inputOptions.setFormat(inputFormat);
        inputOptions.setGamma(inputGamma, outputGamma);
        inputOptions.setAlphaMode(alphaMode);
        inputOptions.setWrapMode(wrapMode);
        inputOptions.setRoundMode(roundMode);

        inputOptions.setMipmapGeneration(true);
        inputOptions.setMipmapFilter(nvtt::MipmapFilter_Box);

        nvtt::CompressionOptions compressionOptions;
        compressionOptions.setQuality(nvtt::Quality_Production);

        if (mipFormat == gpu::Element::COLOR_COMPRESSED_BCX_SRGB) {
            compressionOptions.setFormat(nvtt::Format_BC1);
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_BCX_SRGBA_MASK) {
            alphaMode = nvtt::AlphaMode_Transparency;
            compressionOptions.setFormat(nvtt::Format_BC1a);
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_BCX_SRGBA) {
            alphaMode = nvtt::AlphaMode_Transparency;
            compressionOptions.setFormat(nvtt::Format_BC3);
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_BCX_RED) {
            compressionOptions.setFormat(nvtt::Format_BC4);
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_BCX_XY) {
            compressionOptions.setFormat(nvtt::Format_BC5);
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_BCX_SRGBA_HIGH) {
            alphaMode = nvtt::AlphaMode_Transparency;
            compressionOptions.setFormat(nvtt::Format_BC7);
        } else if (mipFormat == gpu::Element::COLOR_RGBA_32) {
            compressionOptions.setFormat(nvtt::Format_RGBA);
            compressionOptions.setPixelType(nvtt::PixelType_UnsignedNorm);
            compressionOptions.setPitchAlignment(4);
            compressionOptions.setPixelFormat(32,
                                              0x000000FF,
                                              0x0000FF00,
                                              0x00FF0000,
                                              0xFF000000);
            inputGamma = 1.0f;
            outputGamma = 1.0f;
        } else if (mipFormat == gpu::Element::COLOR_BGRA_32) {
            compressionOptions.setFormat(nvtt::Format_RGBA);
            compressionOptions.setPixelType(nvtt::PixelType_UnsignedNorm);
            compressionOptions.setPitchAlignment(4);
            compressionOptions.setPixelFormat(32,
                                              0x00FF0000,
                                              0x0000FF00,
                                              0x000000FF,
                                              0xFF000000);
            inputGamma = 1.0f;
            outputGamma = 1.0f;
        } else if (mipFormat == gpu::Element::COLOR_SRGBA_32) {
            compressionOptions.setFormat(nvtt::Format_RGBA);
            compressionOptions.setPixelType(nvtt::PixelType_UnsignedNorm);
            compressionOptions.setPitchAlignment(4);
            compressionOptions.setPixelFormat(32,
                                              0x000000FF,
                                              0x0000FF00,
                                              0x00FF0000,
                                              0xFF000000);
        } else if (mipFormat == gpu::Element::COLOR_SBGRA_32) {
            compressionOptions.setFormat(nvtt::Format_RGBA);
            compressionOptions.setPixelType(nvtt::PixelType_UnsignedNorm);
            compressionOptions.setPitchAlignment(4);
            compressionOptions.setPixelFormat(32,
                                              0x00FF0000,
                                              0x0000FF00,
                                              0x000000FF,
                                              0xFF000000);
        } else if (mipFormat == gpu::Element::COLOR_R_8) {
            compressionOptions.setFormat(nvtt::Format_RGB);
            compressionOptions.setPixelType(nvtt::PixelType_UnsignedNorm);
            compressionOptions.setPitchAlignment(4);
            compressionOptions.setPixelFormat(8, 0, 0, 0);
        } else if (mipFormat == gpu::Element::VEC2NU8_XY) {
            inputOptions.setNormalMap(true);
            compressionOptions.setFormat(nvtt::Format_RGBA);
            compressionOptions.setPixelType(nvtt::PixelType_UnsignedNorm);
            compressionOptions.setPitchAlignment(4);
            compressionOptions.setPixelFormat(8, 8, 0, 0);
        } else {
            qCWarning(imagelogging) << "Unknown mip format";
            Q_UNREACHABLE();
            return;
        }

        nvtt::OutputOptions outputOptions;
        outputOptions.setOutputHeader(false);
        OutputHandler outputHandler(texture, face);
        outputOptions.setOutputHandler(&outputHandler);
        MyErrorHandler errorHandler;
        outputOptions.setErrorHandler(&errorHandler);

        SequentialTaskDispatcher dispatcher(abortProcessing);
        nvtt::Compressor compressor;
        compressor.setTaskDispatcher(&dispatcher);
        compressor.process(inputOptions, compressionOptions, outputOptions);
    } else {
        int numMips = 1 + (int)log2(std::max(width, height));
        Etc::RawImage *mipMaps = new Etc::RawImage[numMips];
        Etc::Image::Format etcFormat = Etc::Image::Format::DEFAULT;

        if (mipFormat == gpu::Element::COLOR_COMPRESSED_ETC2_RGB) {
            etcFormat = Etc::Image::Format::RGB8;
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_ETC2_SRGB) {
            etcFormat = Etc::Image::Format::SRGB8;
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_ETC2_RGB_PUNCHTHROUGH_ALPHA) {
            etcFormat = Etc::Image::Format::RGB8A1;
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_ETC2_SRGB_PUNCHTHROUGH_ALPHA) {
            etcFormat = Etc::Image::Format::SRGB8A1;
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_ETC2_RGBA) {
            etcFormat = Etc::Image::Format::RGBA8;
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_ETC2_SRGBA) {
            etcFormat = Etc::Image::Format::SRGBA8;
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_EAC_RED) {
            etcFormat = Etc::Image::Format::R11;
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_EAC_RED_SIGNED) {
            etcFormat = Etc::Image::Format::SIGNED_R11;
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_EAC_XY) {
            etcFormat = Etc::Image::Format::RG11;
        } else if (mipFormat == gpu::Element::COLOR_COMPRESSED_EAC_XY_SIGNED) {
            etcFormat = Etc::Image::Format::SIGNED_RG11;
        } else {
            qCWarning(imagelogging) << "Unknown mip format";
            Q_UNREACHABLE();
            return;
        }

        const Etc::ErrorMetric errorMetric = Etc::ErrorMetric::RGBA;
        const float effort = 1.0f;
        const int numEncodeThreads = 4;
        int encodingTime;
        const float MAX_COLOR = 255.0f;

        std::vector<vec4> floatData;
        floatData.resize(width * height);
        for (int y = 0; y < height; y++) {
            QRgb *line = (QRgb *)localCopy.scanLine(y);
            for (int x = 0; x < width; x++) {
                QRgb &pixel = line[x];
                floatData[x + y * width] = vec4(qRed(pixel), qGreen(pixel), qBlue(pixel), qAlpha(pixel)) / MAX_COLOR;
            }
        }

        // free up the memory afterward to avoid bloating the heap
        localCopy = QImage(); // QImage doesn't have a clear function, so override it with an empty one.

        Etc::EncodeMipmaps(
            (float *)floatData.data(), width, height,
            etcFormat, errorMetric, effort,
            numEncodeThreads, numEncodeThreads,
            numMips, Etc::FILTER_WRAP_NONE,
            mipMaps, &encodingTime
        );

        for (int i = 0; i < numMips; i++) {
            if (mipMaps[i].paucEncodingBits.get()) {
                if (face >= 0) {
                    texture->assignStoredMipFace(i, face, mipMaps[i].uiEncodingBitsBytes, static_cast<const gpu::Byte*>(mipMaps[i].paucEncodingBits.get()));
                } else {
                    texture->assignStoredMip(i, mipMaps[i].uiEncodingBitsBytes, static_cast<const gpu::Byte*>(mipMaps[i].paucEncodingBits.get()));
                }
            }
        }

        delete[] mipMaps;
    }
}

#endif

void generateMips(gpu::Texture* texture, QImage&& image, BackendTarget target, const std::atomic<bool>& abortProcessing = false, int face = -1) {
#if CPU_MIPMAPS
    PROFILE_RANGE(resource_parse, "generateMips");

    if (target == BackendTarget::GLES32) {
        generateLDRMips(texture, std::move(image), target, abortProcessing, face);
    } else {
        if (image.format() == hdrFormatForTarget(target)) {
            generateHDRMips(texture, std::move(image), target, abortProcessing, face);
        } else {
            generateLDRMips(texture, std::move(image), target, abortProcessing, face);
        }
    }
#else
    texture->setAutoGenerateMips(true);
#endif
}

void processTextureAlpha(const QImage& srcImage, bool& validAlpha, bool& alphaAsMask) {
    PROFILE_RANGE(resource_parse, "processTextureAlpha");
    validAlpha = false;
    alphaAsMask = true;
    const uint8 OPAQUE_ALPHA = 255;
    const uint8 TRANSPARENT_ALPHA = 0;

    // Figure out if we can use a mask for alpha or not
    int numOpaques = 0;
    int numTranslucents = 0;
    const int NUM_PIXELS = srcImage.width() * srcImage.height();
    const int MAX_TRANSLUCENT_PIXELS_FOR_ALPHAMASK = (int)(0.05f * (float)(NUM_PIXELS));
    const QRgb* data = reinterpret_cast<const QRgb*>(srcImage.constBits());
    for (int i = 0; i < NUM_PIXELS; ++i) {
        auto alpha = qAlpha(data[i]);
        if (alpha == OPAQUE_ALPHA) {
            numOpaques++;
        } else if (alpha != TRANSPARENT_ALPHA) {
            if (++numTranslucents > MAX_TRANSLUCENT_PIXELS_FOR_ALPHAMASK) {
                alphaAsMask = false;
                break;
            }
        }
    }
    validAlpha = (numOpaques != NUM_PIXELS);
}

gpu::TexturePointer TextureUsage::process2DTextureColorFromImage(QImage&& srcImage, const std::string& srcImageName, bool compress,
                                                                 BackendTarget target, bool isStrict, const std::atomic<bool>& abortProcessing) {
    PROFILE_RANGE(resource_parse, "process2DTextureColorFromImage");
    QImage image = processSourceImage(std::move(srcImage), false, target);

    bool validAlpha = image.hasAlphaChannel();
    bool alphaAsMask = false;

    if (image.format() != QImage::Format_ARGB32) {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }

    if (validAlpha) {
        processTextureAlpha(image, validAlpha, alphaAsMask);
    }

    gpu::TexturePointer theTexture = nullptr;

    if ((image.width() > 0) && (image.height() > 0)) {
        gpu::Element formatMip;
        gpu::Element formatGPU;
        if (compress) {
            if (target == BackendTarget::GLES32) {
                // GLES does not support GL_BGRA
                formatGPU = gpu::Element::COLOR_COMPRESSED_ETC2_SRGBA;
                formatMip = formatGPU;
            } else {
                if (validAlpha) {
                    // NOTE: This disables BC1a compression because it was producing odd artifacts on text textures
                    // for the tutorial. Instead we use BC3 (which is larger) but doesn't produce the same artifacts).
                    formatGPU = gpu::Element::COLOR_COMPRESSED_BCX_SRGBA;
                } else {
                    formatGPU = gpu::Element::COLOR_COMPRESSED_BCX_SRGB;
                }
                formatMip = formatGPU;
            }
        } else {
            if (target == BackendTarget::GLES32) {
            } else {
                formatGPU = gpu::Element::COLOR_SRGBA_32;
                formatMip = gpu::Element::COLOR_SBGRA_32;
            }
        }

        if (isStrict) {
            theTexture = gpu::Texture::createStrict(formatGPU, image.width(), image.height(), gpu::Texture::MAX_NUM_MIPS, gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_MIP_LINEAR));
        } else {
            theTexture = gpu::Texture::create2D(formatGPU, image.width(), image.height(), gpu::Texture::MAX_NUM_MIPS, gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_MIP_LINEAR));
        }
        theTexture->setSource(srcImageName);
        auto usage = gpu::Texture::Usage::Builder().withColor();
        if (validAlpha) {
            usage.withAlpha();
            if (alphaAsMask) {
                usage.withAlphaMask();
            }
        }
        theTexture->setUsage(usage.build());
        theTexture->setStoredMipFormat(formatMip);
        theTexture->assignStoredMip(0, image.byteCount(), image.constBits());
        generateMips(theTexture.get(), std::move(image), target, abortProcessing);
    }

    return theTexture;
}

int clampPixelCoordinate(int coordinate, int maxCoordinate) {
    return coordinate - ((int)(coordinate < 0) * coordinate) + ((int)(coordinate > maxCoordinate) * (maxCoordinate - coordinate));
}

const int RGBA_MAX = 255;

// transform -1 - 1 to 0 - 255 (from sobel value to rgb)
double mapComponent(double sobelValue) {
    const double factor = RGBA_MAX / 2.0;
    return (sobelValue + 1.0) * factor;
}

QImage processBumpMap(QImage&& image) {
    // Take a local copy to force move construction
    // https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#f18-for-consume-parameters-pass-by-x-and-stdmove-the-parameter
    QImage localCopy = std::move(image);

    if (localCopy.format() != QImage::Format_Grayscale8) {
        localCopy = localCopy.convertToFormat(QImage::Format_Grayscale8);
    }

    // PR 5540 by AlessandroSigna integrated here as a specialized TextureLoader for bumpmaps
    // The conversion is done using the Sobel Filter to calculate the derivatives from the grayscale image
    const double pStrength = 2.0;
    int width = localCopy.width();
    int height = localCopy.height();

    QImage result(width, height, QImage::Format_ARGB32);

    for (int i = 0; i < width; i++) {
        const int iNextClamped = clampPixelCoordinate(i + 1, width - 1);
        const int iPrevClamped = clampPixelCoordinate(i - 1, width - 1);

        for (int j = 0; j < height; j++) {
            const int jNextClamped = clampPixelCoordinate(j + 1, height - 1);
            const int jPrevClamped = clampPixelCoordinate(j - 1, height - 1);

            // surrounding pixels
            const QRgb topLeft = localCopy.pixel(iPrevClamped, jPrevClamped);
            const QRgb top = localCopy.pixel(iPrevClamped, j);
            const QRgb topRight = localCopy.pixel(iPrevClamped, jNextClamped);
            const QRgb right = localCopy.pixel(i, jNextClamped);
            const QRgb bottomRight = localCopy.pixel(iNextClamped, jNextClamped);
            const QRgb bottom = localCopy.pixel(iNextClamped, j);
            const QRgb bottomLeft = localCopy.pixel(iNextClamped, jPrevClamped);
            const QRgb left = localCopy.pixel(i, jPrevClamped);

            // take their gray intensities
            // since it's a grayscale image, the value of each component RGB is the same
            const double tl = qRed(topLeft);
            const double t = qRed(top);
            const double tr = qRed(topRight);
            const double r = qRed(right);
            const double br = qRed(bottomRight);
            const double b = qRed(bottom);
            const double bl = qRed(bottomLeft);
            const double l = qRed(left);

            // apply the sobel filter
            const double dX = (tr + pStrength * r + br) - (tl + pStrength * l + bl);
            const double dY = (bl + pStrength * b + br) - (tl + pStrength * t + tr);
            const double dZ = RGBA_MAX / pStrength;

            glm::vec3 v(dX, dY, dZ);
            glm::normalize(v);

            // convert to rgb from the value obtained computing the filter
            QRgb qRgbValue = qRgba(mapComponent(v.z), mapComponent(v.y), mapComponent(v.x), 1.0);
            result.setPixel(i, j, qRgbValue);
        }
    }

    return result;
}
gpu::TexturePointer TextureUsage::process2DTextureNormalMapFromImage(QImage&& srcImage, const std::string& srcImageName,
                                                                     bool compress, BackendTarget target, bool isBumpMap,
                                                                     const std::atomic<bool>& abortProcessing) {
    PROFILE_RANGE(resource_parse, "process2DTextureNormalMapFromImage");
    QImage image = processSourceImage(std::move(srcImage), false, target);

    if (isBumpMap) {
        image = processBumpMap(std::move(image));
    }

    // Make sure the normal map source image is ARGB32
    if (image.format() != QImage::Format_ARGB32) {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }

    gpu::TexturePointer theTexture = nullptr;
    if ((image.width() > 0) && (image.height() > 0)) {
        gpu::Element formatMip;
        gpu::Element formatGPU;
        if (compress) {
            if (target == BackendTarget::GLES32) {
                formatGPU = gpu::Element::COLOR_COMPRESSED_EAC_XY;
            } else {
                formatGPU = gpu::Element::COLOR_COMPRESSED_BCX_XY;
            }
        } else {
            formatGPU = gpu::Element::VEC2NU8_XY;
        }
        formatMip = formatGPU;

        theTexture = gpu::Texture::create2D(formatGPU, image.width(), image.height(), gpu::Texture::MAX_NUM_MIPS, gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_MIP_LINEAR));
        theTexture->setSource(srcImageName);
        theTexture->setStoredMipFormat(formatMip);
        theTexture->assignStoredMip(0, image.byteCount(), image.constBits());
        generateMips(theTexture.get(), std::move(image), target, abortProcessing);
    }

    return theTexture;
}

gpu::TexturePointer TextureUsage::process2DTextureGrayscaleFromImage(QImage&& srcImage, const std::string& srcImageName,
                                                                     bool compress, BackendTarget target, bool isInvertedPixels,
                                                                     const std::atomic<bool>& abortProcessing) {
    PROFILE_RANGE(resource_parse, "process2DTextureGrayscaleFromImage");
    QImage image = processSourceImage(std::move(srcImage), false, target);

    if (image.format() != QImage::Format_ARGB32) {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }

    if (isInvertedPixels) {
        // Gloss turned into Rough
        image.invertPixels(QImage::InvertRgba);
    }

    gpu::TexturePointer theTexture = nullptr;
    if ((image.width() > 0) && (image.height() > 0)) {
        gpu::Element formatMip;
        gpu::Element formatGPU;
        if (compress) {
            if (target == BackendTarget::GLES32) {
                formatGPU = gpu::Element::COLOR_COMPRESSED_EAC_RED;
            } else {
                formatGPU = gpu::Element::COLOR_COMPRESSED_BCX_RED;
            }
        } else {
            formatGPU = gpu::Element::COLOR_R_8;
        }
        formatMip = formatGPU;

        theTexture = gpu::Texture::create2D(formatGPU, image.width(), image.height(), gpu::Texture::MAX_NUM_MIPS, gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_MIP_LINEAR));
        theTexture->setSource(srcImageName);
        theTexture->setStoredMipFormat(formatMip);
        theTexture->assignStoredMip(0, image.byteCount(), image.constBits());
        generateMips(theTexture.get(), std::move(image), target, abortProcessing);
    }

    return theTexture;  
}

class CubeLayout {
public:

    enum SourceProjection {
        FLAT = 0,
        EQUIRECTANGULAR,
    };
    int _type = FLAT;
    int _widthRatio = 1;
    int _heightRatio = 1;

    class Face {
    public:
        int _x = 0;
        int _y = 0;
        bool _horizontalMirror = false;
        bool _verticalMirror = false;

        Face() {}
        Face(int x, int y, bool horizontalMirror, bool verticalMirror) : _x(x), _y(y), _horizontalMirror(horizontalMirror), _verticalMirror(verticalMirror) {}
    };

    Face _faceXPos;
    Face _faceXNeg;
    Face _faceYPos;
    Face _faceYNeg;
    Face _faceZPos;
    Face _faceZNeg;

    CubeLayout(int wr, int hr, Face fXP, Face fXN, Face fYP, Face fYN, Face fZP, Face fZN) :
        _type(FLAT),
        _widthRatio(wr),
        _heightRatio(hr),
        _faceXPos(fXP),
        _faceXNeg(fXN),
        _faceYPos(fYP),
        _faceYNeg(fYN),
        _faceZPos(fZP),
        _faceZNeg(fZN) {}

    CubeLayout(int wr, int hr) :
        _type(EQUIRECTANGULAR),
        _widthRatio(wr),
        _heightRatio(hr) {}


    static const CubeLayout CUBEMAP_LAYOUTS[];
    static const int NUM_CUBEMAP_LAYOUTS;

    static int findLayout(int width, int height) {
        // Find the layout of the cubemap in the 2D image
        int foundLayout = -1;
        for (int i = 0; i < NUM_CUBEMAP_LAYOUTS; i++) {
            if ((height * CUBEMAP_LAYOUTS[i]._widthRatio) == (width * CUBEMAP_LAYOUTS[i]._heightRatio)) {
                foundLayout = i;
                break;
            }
        }
        return foundLayout;
    }

    static QImage extractEquirectangularFace(const QImage& source, gpu::Texture::CubeFace face, int faceWidth) {
        QImage image(faceWidth, faceWidth, source.format());

        glm::vec2 dstInvSize(1.0f / faceWidth);

        struct CubeToXYZ {
            gpu::Texture::CubeFace _face;
            CubeToXYZ(gpu::Texture::CubeFace face) : _face(face) {}

            glm::vec3 xyzFrom(const glm::vec2& uv) {
                auto faceDir = glm::normalize(glm::vec3(-1.0f + 2.0f * uv.x, -1.0f + 2.0f * uv.y, 1.0f));

                switch (_face) {
                    case gpu::Texture::CubeFace::CUBE_FACE_BACK_POS_Z:
                        return glm::vec3(-faceDir.x, faceDir.y, faceDir.z);
                    case gpu::Texture::CubeFace::CUBE_FACE_FRONT_NEG_Z:
                        return glm::vec3(faceDir.x, faceDir.y, -faceDir.z);
                    case gpu::Texture::CubeFace::CUBE_FACE_LEFT_NEG_X:
                        return glm::vec3(faceDir.z, faceDir.y, faceDir.x);
                    case gpu::Texture::CubeFace::CUBE_FACE_RIGHT_POS_X:
                        return glm::vec3(-faceDir.z, faceDir.y, -faceDir.x);
                    case gpu::Texture::CubeFace::CUBE_FACE_BOTTOM_NEG_Y:
                        return glm::vec3(-faceDir.x, -faceDir.z, faceDir.y);
                    case gpu::Texture::CubeFace::CUBE_FACE_TOP_POS_Y:
                    default:
                        return glm::vec3(-faceDir.x, faceDir.z, -faceDir.y);
                }
            }
        };
        CubeToXYZ cubeToXYZ(face);

        struct RectToXYZ {
            RectToXYZ() {}

            glm::vec2 uvFrom(const glm::vec3& xyz) {
                auto flatDir = glm::normalize(glm::vec2(xyz.x, xyz.z));
                auto uvRad = glm::vec2(atan2(flatDir.x, flatDir.y), asin(xyz.y));

                const float LON_TO_RECT_U = 1.0f / (glm::pi<float>());
                const float LAT_TO_RECT_V = 2.0f / glm::pi<float>();
                return glm::vec2(0.5f * uvRad.x * LON_TO_RECT_U + 0.5f, 0.5f * uvRad.y * LAT_TO_RECT_V + 0.5f);
            }
        };
        RectToXYZ rectToXYZ;

        int srcFaceHeight = source.height();
        int srcFaceWidth = source.width();

        glm::vec2 dstCoord;
        glm::ivec2 srcPixel;
        for (int y = 0; y < faceWidth; ++y) {
            QRgb* destScanLineBegin = reinterpret_cast<QRgb*>( image.scanLine(y) );
            QRgb* destPixelIterator = destScanLineBegin;

            dstCoord.y = 1.0f - (y + 0.5f) * dstInvSize.y; // Fill cube face images from top to bottom
            for (int x = 0; x < faceWidth; ++x) {
                dstCoord.x = (x + 0.5f) * dstInvSize.x;

                auto xyzDir = cubeToXYZ.xyzFrom(dstCoord);
                auto srcCoord = rectToXYZ.uvFrom(xyzDir);

                srcPixel.x = floor(srcCoord.x * srcFaceWidth);
                // Flip the vertical axis to QImage going top to bottom
                srcPixel.y = floor((1.0f - srcCoord.y) * srcFaceHeight);

                if (((uint32)srcPixel.x < (uint32)source.width()) && ((uint32)srcPixel.y < (uint32)source.height())) {
                    // We can't directly use the pixel() method because that launches a pixel color conversion to output
                    // a correct RGBA8 color. But in our case we may have stored HDR values encoded in a RGB30 format which
                    // are not convertible by Qt. The same goes with the setPixel method, by the way.
                    const QRgb* sourcePixelIterator = reinterpret_cast<const QRgb*>(source.scanLine(srcPixel.y));
                    sourcePixelIterator += srcPixel.x;
                    *destPixelIterator = *sourcePixelIterator;

                    // Keep for debug, this is showing the dir as a color
                    //  glm::u8vec4 rgba((xyzDir.x + 1.0)*0.5 * 256, (xyzDir.y + 1.0)*0.5 * 256, (xyzDir.z + 1.0)*0.5 * 256, 256);
                    //  unsigned int val = 0xff000000 | (rgba.r) | (rgba.g << 8) | (rgba.b << 16);
                    //  *destPixelIterator = val;
                }
                ++destPixelIterator;
            }
        }
        return image;
    }
};

const CubeLayout CubeLayout::CUBEMAP_LAYOUTS[] = {

    // Here is the expected layout for the faces in an image with the 2/1 aspect ratio:
    // THis is detected as an Equirectangular projection
    //                   WIDTH
    //       <--------------------------->
    //    ^  +------+------+------+------+
    //    H  |      |      |      |      |
    //    E  |      |      |      |      |
    //    I  |      |      |      |      |
    //    G  +------+------+------+------+
    //    H  |      |      |      |      |
    //    T  |      |      |      |      |
    //    |  |      |      |      |      |
    //    v  +------+------+------+------+
    //
    //    FaceWidth = width = height / 6
    { 2, 1 },

    // Here is the expected layout for the faces in an image with the 1/6 aspect ratio:
    //
    //         WIDTH
    //       <------>
    //    ^  +------+
    //    |  |      |
    //    |  |  +X  |
    //    |  |      |
    //    H  +------+
    //    E  |      |
    //    I  |  -X  |
    //    G  |      |
    //    H  +------+
    //    T  |      |
    //    |  |  +Y  |
    //    |  |      |
    //    |  +------+
    //    |  |      |
    //    |  |  -Y  |
    //    |  |      |
    //    H  +------+
    //    E  |      |
    //    I  |  +Z  |
    //    G  |      |
    //    H  +------+
    //    T  |      |
    //    |  |  -Z  |
    //    |  |      |
    //    V  +------+
    //
    //    FaceWidth = width = height / 6
    { 1, 6,
    { 0, 0, true, false },
    { 0, 1, true, false },
    { 0, 2, false, true },
    { 0, 3, false, true },
    { 0, 4, true, false },
    { 0, 5, true, false }
    },

    // Here is the expected layout for the faces in an image with the 3/4 aspect ratio:
    //
    //       <-----------WIDTH----------->
    //    ^  +------+------+------+------+
    //    |  |      |      |      |      |
    //    |  |      |  +Y  |      |      |
    //    |  |      |      |      |      |
    //    H  +------+------+------+------+
    //    E  |      |      |      |      |
    //    I  |  -X  |  -Z  |  +X  |  +Z  |
    //    G  |      |      |      |      |
    //    H  +------+------+------+------+
    //    T  |      |      |      |      |
    //    |  |      |  -Y  |      |      |
    //    |  |      |      |      |      |
    //    V  +------+------+------+------+
    //
    //    FaceWidth = width / 4 = height / 3
    { 4, 3,
    { 2, 1, true, false },
    { 0, 1, true, false },
    { 1, 0, false, true },
    { 1, 2, false, true },
    { 3, 1, true, false },
    { 1, 1, true, false }
    },

    // Here is the expected layout for the faces in an image with the 4/3 aspect ratio:
    //
    //       <-------WIDTH-------->
    //    ^  +------+------+------+
    //    |  |      |      |      |
    //    |  |      |  +Y  |      |
    //    |  |      |      |      |
    //    H  +------+------+------+
    //    E  |      |      |      |
    //    I  |  -X  |  -Z  |  +X  |
    //    G  |      |      |      |
    //    H  +------+------+------+
    //    T  |      |      |      |
    //    |  |      |  -Y  |      |
    //    |  |      |      |      |
    //    |  +------+------+------+
    //    |  |      |      |      |
    //    |  |      |  +Z! |      | <+Z is upside down!
    //    |  |      |      |      |
    //    V  +------+------+------+
    //
    //    FaceWidth = width / 3 = height / 4
    { 3, 4,
    { 2, 1, true, false },
    { 0, 1, true, false },
    { 1, 0, false, true },
    { 1, 2, false, true },
    { 1, 3, false, true },
    { 1, 1, true, false }
    }
};
const int CubeLayout::NUM_CUBEMAP_LAYOUTS = sizeof(CubeLayout::CUBEMAP_LAYOUTS) / sizeof(CubeLayout);

//#define DEBUG_COLOR_PACKING

QImage convertToHDRFormat(QImage&& srcImage, gpu::Element format, BackendTarget target) {
    // Take a local copy to force move construction
    // https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#f18-for-consume-parameters-pass-by-x-and-stdmove-the-parameter
    QImage localCopy = std::move(srcImage);

    QImage hdrImage(localCopy.width(), localCopy.height(), hdrFormatForTarget(target));
    std::function<uint32(const glm::vec3&)> packFunc;
#ifdef DEBUG_COLOR_PACKING
    std::function<glm::vec3(uint32)> unpackFunc;
#endif

    switch (format.getSemantic()) {
        case gpu::R11G11B10:
            packFunc = packR11G11B10F;
#ifdef DEBUG_COLOR_PACKING
            unpackFunc = glm::unpackF2x11_1x10;
#endif
            break;
        case gpu::RGB9E5:
            packFunc = glm::packF3x9_E1x5;
#ifdef DEBUG_COLOR_PACKING
            unpackFunc = glm::unpackF3x9_E1x5;
#endif
            break;
        default:
            qCWarning(imagelogging) << "Unsupported HDR format";
            Q_UNREACHABLE();
            return localCopy;
    }

    localCopy = localCopy.convertToFormat(QImage::Format_ARGB32);
    for (auto y = 0; y < localCopy.height(); y++) {
        const QRgb* srcLineIt = reinterpret_cast<const QRgb*>( localCopy.constScanLine(y) );
        const QRgb* srcLineEnd = srcLineIt + localCopy.width();
        uint32* hdrLineIt = reinterpret_cast<uint32*>( hdrImage.scanLine(y) );
        glm::vec3 color;

        while (srcLineIt < srcLineEnd) {
            color.r = qRed(*srcLineIt);
            color.g = qGreen(*srcLineIt);
            color.b = qBlue(*srcLineIt);
            // Normalize and apply gamma
            color /= 255.0f;
            color.r = powf(color.r, 2.2f);
            color.g = powf(color.g, 2.2f);
            color.b = powf(color.b, 2.2f);
            *hdrLineIt = packFunc(color);
#ifdef DEBUG_COLOR_PACKING
            glm::vec3 ucolor = unpackFunc(*hdrLineIt);
            assert(glm::distance(color, ucolor) <= 5e-2);
#endif
            ++srcLineIt;
            ++hdrLineIt;
        }
    }
    return hdrImage;
}

gpu::TexturePointer TextureUsage::processCubeTextureColorFromImage(QImage&& srcImage, const std::string& srcImageName,
                                                                   bool compress, BackendTarget target, bool generateIrradiance,
                                                                   const std::atomic<bool>& abortProcessing) {
    PROFILE_RANGE(resource_parse, "processCubeTextureColorFromImage");

    // Take a local copy to force move construction
    // https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#f18-for-consume-parameters-pass-by-x-and-stdmove-the-parameter
    QImage localCopy = std::move(srcImage);

    int originalWidth = localCopy.width();
    int originalHeight = localCopy.height();
    if ((originalWidth <= 0) && (originalHeight <= 0)) {
        return nullptr;
    }

    gpu::TexturePointer theTexture = nullptr;

    QImage image = processSourceImage(std::move(localCopy), true, target);

    if (image.format() != hdrFormatForTarget(target)) {
        if (target == BackendTarget::GLES32) {
            image = image.convertToFormat(QImage::Format_RGB32);
        } else {
            image = convertToHDRFormat(std::move(image), HDR_FORMAT, target);
        }
    }

    gpu::Element formatMip;
    gpu::Element formatGPU;
    if (compress) {
        if (target == BackendTarget::GLES32) {
            formatGPU = gpu::Element::COLOR_COMPRESSED_ETC2_SRGB;
        } else {
            formatGPU = gpu::Element::COLOR_COMPRESSED_BCX_HDR_RGB;
        }
    } else {
        formatGPU = HDR_FORMAT;
    }

    formatMip = formatGPU;

    // Find the layout of the cubemap in the 2D image
    // Use the original image size since processSourceImage may have altered the size / aspect ratio
    int foundLayout = CubeLayout::findLayout(originalWidth, originalHeight);

    if (foundLayout < 0) {
        qCDebug(imagelogging) << "Failed to find a known cube map layout from this image:" << QString(srcImageName.c_str());
        return nullptr;
    }

    std::vector<QImage> faces;

    // If found, go extract the faces as separate images
    auto& layout = CubeLayout::CUBEMAP_LAYOUTS[foundLayout];
    if (layout._type == CubeLayout::FLAT) {
        int faceWidth = image.width() / layout._widthRatio;

        faces.push_back(image.copy(QRect(layout._faceXPos._x * faceWidth, layout._faceXPos._y * faceWidth, faceWidth, faceWidth)).mirrored(layout._faceXPos._horizontalMirror, layout._faceXPos._verticalMirror));
        faces.push_back(image.copy(QRect(layout._faceXNeg._x * faceWidth, layout._faceXNeg._y * faceWidth, faceWidth, faceWidth)).mirrored(layout._faceXNeg._horizontalMirror, layout._faceXNeg._verticalMirror));
        faces.push_back(image.copy(QRect(layout._faceYPos._x * faceWidth, layout._faceYPos._y * faceWidth, faceWidth, faceWidth)).mirrored(layout._faceYPos._horizontalMirror, layout._faceYPos._verticalMirror));
        faces.push_back(image.copy(QRect(layout._faceYNeg._x * faceWidth, layout._faceYNeg._y * faceWidth, faceWidth, faceWidth)).mirrored(layout._faceYNeg._horizontalMirror, layout._faceYNeg._verticalMirror));
        faces.push_back(image.copy(QRect(layout._faceZPos._x * faceWidth, layout._faceZPos._y * faceWidth, faceWidth, faceWidth)).mirrored(layout._faceZPos._horizontalMirror, layout._faceZPos._verticalMirror));
        faces.push_back(image.copy(QRect(layout._faceZNeg._x * faceWidth, layout._faceZNeg._y * faceWidth, faceWidth, faceWidth)).mirrored(layout._faceZNeg._horizontalMirror, layout._faceZNeg._verticalMirror));
    } else if (layout._type == CubeLayout::EQUIRECTANGULAR) {
        // THe face width is estimated from the input image
        const int EQUIRECT_FACE_RATIO_TO_WIDTH = 4;
        const int EQUIRECT_MAX_FACE_WIDTH = 2048;
        int faceWidth = std::min(image.width() / EQUIRECT_FACE_RATIO_TO_WIDTH, EQUIRECT_MAX_FACE_WIDTH);
        for (int face = gpu::Texture::CUBE_FACE_RIGHT_POS_X; face < gpu::Texture::NUM_CUBE_FACES; face++) {
            QImage faceImage = CubeLayout::extractEquirectangularFace(std::move(image), (gpu::Texture::CubeFace) face, faceWidth);
            faces.push_back(std::move(faceImage));
        }
    }

    // free up the memory afterward to avoid bloating the heap
    image = QImage(); // QImage doesn't have a clear function, so override it with an empty one.

    // If the 6 faces have been created go on and define the true Texture
    if (faces.size() == gpu::Texture::NUM_FACES_PER_TYPE[gpu::Texture::TEX_CUBE]) {
        theTexture = gpu::Texture::createCube(formatGPU, faces[0].width(), gpu::Texture::MAX_NUM_MIPS, gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_MIP_LINEAR, gpu::Sampler::WRAP_CLAMP));
        theTexture->setSource(srcImageName);
        theTexture->setStoredMipFormat(formatMip);

        // Generate irradiance while we are at it
        if (generateIrradiance) {
            PROFILE_RANGE(resource_parse, "generateIrradiance");
            gpu::Element irradianceFormat;
            // TODO: we could locally compress the irradiance texture on Android, but we don't need to
            if (target == BackendTarget::GLES32) {
                irradianceFormat = gpu::Element::COLOR_SRGBA_32;
            } else {
                irradianceFormat = HDR_FORMAT;
            }

            auto irradianceTexture = gpu::Texture::createCube(irradianceFormat, faces[0].width(), gpu::Texture::MAX_NUM_MIPS, gpu::Sampler(gpu::Sampler::FILTER_MIN_MAG_MIP_LINEAR, gpu::Sampler::WRAP_CLAMP));
            irradianceTexture->setSource(srcImageName);
            irradianceTexture->setStoredMipFormat(irradianceFormat);
            for (uint8 face = 0; face < faces.size(); ++face) {
                irradianceTexture->assignStoredMipFace(0, face, faces[face].byteCount(), faces[face].constBits());
            }

            irradianceTexture->generateIrradiance(target);

            auto irradiance = irradianceTexture->getIrradiance();
            theTexture->overrideIrradiance(irradiance);
        }

        for (uint8 face = 0; face < faces.size(); ++face) {
            generateMips(theTexture.get(), std::move(faces[face]), target, abortProcessing, face);
        }
    }

    return theTexture;
}

} // namespace image
