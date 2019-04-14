//
//  RenderForwardTask.h
//  render-utils/src/
//
//  Created by Zach Pomerantz on 12/13/2016.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RenderForwardTask_h
#define hifi_RenderForwardTask_h

#include <gpu/Pipeline.h>
#include <render/RenderFetchCullSortTask.h>
#include "AssembleLightingStageTask.h"
#include "LightingModel.h"

class RenderForwardTask {
public:
    using Input = render::VaryingSet3<RenderFetchCullSortTask::Output, LightingModelPointer, AssembleLightingStageTask::Output>;
    using JobModel = render::Task::ModelI<RenderForwardTask, Input>;

    RenderForwardTask() {}

    void build(JobModel& task, const render::Varying& input, render::Varying& output);
};


class PrepareFramebufferConfig : public render::Job::Config {
    Q_OBJECT
    Q_PROPERTY(int numSamples WRITE setNumSamples READ getNumSamples NOTIFY dirty)
public:
    int getNumSamples() const { return numSamples; }
    void setNumSamples(int num) {
        numSamples = std::max(1, std::min(32, num));
        emit dirty();
    }

signals:
    void dirty();

protected:
    int numSamples{ 4 };
};

class PrepareFramebuffer {
public:
    using Inputs = gpu::FramebufferPointer;
    using Config = PrepareFramebufferConfig;
    using JobModel = render::Job::ModelO<PrepareFramebuffer, Inputs, Config>;

    void configure(const Config& config);
    void run(const render::RenderContextPointer& renderContext,
            gpu::FramebufferPointer& framebuffer);

private:
    gpu::FramebufferPointer _framebuffer;
    int _numSamples;
};

class PrepareForward {
public:
    using Inputs = LightStage::FramePointer;
    using JobModel = render::Job::ModelI<PrepareForward, Inputs>;

    void run(const render::RenderContextPointer& renderContext,
        const Inputs& inputs);

private:
};

class DrawForward{
public:
    using Inputs = render::VaryingSet2<render::ItemBounds, LightingModelPointer>;
    using JobModel = render::Job::ModelI<DrawForward, Inputs>;

    DrawForward(const render::ShapePlumberPointer& shapePlumber, bool opaquePass) : _shapePlumber(shapePlumber), _opaquePass(opaquePass) {}
    void run(const render::RenderContextPointer& renderContext,
            const Inputs& inputs);

private:
    render::ShapePlumberPointer _shapePlumber;
    bool _opaquePass;
};

#endif // hifi_RenderForwardTask_h
