//
//  Created by Bradley Austin Davis on 2016/03/04
//  Copyright 2013-2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi__OculusControllerManager
#define hifi__OculusControllerManager

#include <QObject>
#include <unordered_set>
#include <map>

#include <GLMHelpers.h>

#include <controllers/InputDevice.h>
#include <plugins/InputPlugin.h>

std::shared_ptr<InputPlugin> getOculusMobileInputPlugin();

#endif // hifi__OculusControllerManager
