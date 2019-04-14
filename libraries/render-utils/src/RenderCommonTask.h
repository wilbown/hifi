//
//  Created by Bradley Austin Davis on 2018/01/09
//  Copyright 2013-2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RenderCommonTask_h
#define hifi_RenderCommonTask_h

#include <gpu/Pipeline.h>
#include "LightStage.h"
#include "LightingModel.h"

class BeginGPURangeTimer {
public:
    using JobModel = render::Job::ModelO<BeginGPURangeTimer, gpu::RangeTimerPointer>;

    BeginGPURangeTimer(const std::string& name) : _gpuTimer(std::make_shared<gpu::RangeTimer>(name)) {}

    void run(const render::RenderContextPointer& renderContext, gpu::RangeTimerPointer& timer);

protected:
    gpu::RangeTimerPointer _gpuTimer;
};

using GPURangeTimerConfig = render::GPUJobConfig;

class EndGPURangeTimer {
public:
    using Config = GPURangeTimerConfig;
    using JobModel = render::Job::ModelI<EndGPURangeTimer, gpu::RangeTimerPointer, Config>;

    EndGPURangeTimer() {}

    void configure(const Config& config) {}
    void run(const render::RenderContextPointer& renderContext, const gpu::RangeTimerPointer& timer);

protected:
};

class DrawLayered3DConfig : public render::Job::Config {
    Q_OBJECT
        Q_PROPERTY(int numDrawn READ getNumDrawn NOTIFY numDrawnChanged)
        Q_PROPERTY(int maxDrawn MEMBER maxDrawn NOTIFY dirty)
public:
    int getNumDrawn() { return numDrawn; }
    void setNumDrawn(int num) { numDrawn = num; emit numDrawnChanged(); }

    int maxDrawn{ -1 };

signals:
    void numDrawnChanged();
    void dirty();

protected:
    int numDrawn{ 0 };
};

class DrawLayered3D {
public:
    using Inputs = render::VaryingSet3<render::ItemBounds, LightingModelPointer, glm::vec2>;
    using Config = DrawLayered3DConfig;
    using JobModel = render::Job::ModelI<DrawLayered3D, Inputs, Config>;

    DrawLayered3D(bool opaque);

    void configure(const Config& config) { _maxDrawn = config.maxDrawn; }
    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs);

protected:
    render::ShapePlumberPointer _shapePlumber;
    int _maxDrawn; // initialized by Config
    bool _opaquePass { true };
};

class CompositeHUD {
public:
    // IF specified the input Framebuffer is actively set by the batch of this job before calling the HUDOperator.
    // If not, the current Framebuffer is left unchanged.
    //using Inputs = gpu::FramebufferPointer;
    using JobModel = render::Job::ModelI<CompositeHUD, gpu::FramebufferPointer>;

    void run(const render::RenderContextPointer& renderContext, const gpu::FramebufferPointer& inputs);
};

class Blit {
public:
    using JobModel = render::Job::ModelI<Blit, gpu::FramebufferPointer>;

    void run(const render::RenderContextPointer& renderContext, const gpu::FramebufferPointer& srcFramebuffer);
};


class ResolveFramebuffer {
public:
    using Inputs = render::VaryingSet2<gpu::FramebufferPointer, gpu::FramebufferPointer>;
    using Outputs = gpu::FramebufferPointer;
    using JobModel = render::Job::ModelIO<ResolveFramebuffer, Inputs, Outputs>;

    void run(const render::RenderContextPointer& renderContext, const Inputs& source, Outputs& dest);
};

class ResolveNewFramebuffer {
public:
    using Inputs = gpu::FramebufferPointer;
    using Outputs = gpu::FramebufferPointer;
    using JobModel = render::Job::ModelIO<ResolveNewFramebuffer, Inputs, Outputs>;

    void run(const render::RenderContextPointer& renderContext, const Inputs& source, Outputs& dest);
private:
    gpu::FramebufferPointer _outputFramebuffer;
};


class ExtractFrustums {
public:

    enum Frustum {
        SHADOW_CASCADE0_FRUSTUM = 0,
        SHADOW_CASCADE1_FRUSTUM,
        SHADOW_CASCADE2_FRUSTUM,
        SHADOW_CASCADE3_FRUSTUM,

        SHADOW_CASCADE_FRUSTUM_COUNT,

        VIEW_FRUSTUM = SHADOW_CASCADE_FRUSTUM_COUNT,

        FRUSTUM_COUNT
    };

    using Inputs = LightStage::ShadowFramePointer;
    using Outputs = render::VaryingArray<ViewFrustumPointer, FRUSTUM_COUNT>;
    using JobModel = render::Job::ModelIO<ExtractFrustums, Inputs, Outputs>;

    void run(const render::RenderContextPointer& renderContext, const Inputs& inputs, Outputs& output);
};

#endif // hifi_RenderDeferredTask_h
