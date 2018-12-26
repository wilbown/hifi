//
//  GymEvent.cpp
//  libraries/script-engine/src
//
//  Created by Wil Bown on 2018-12-20
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "GymEvent.h"

void registerGymMetaTypes(QScriptEngine* engine) {
    qScriptRegisterMetaType(engine, gymEventToScriptValue, gymEventFromScriptValue);
}

const QString GYM_ACTION_PROP_NAME = "action";

QScriptValue gymEventToScriptValue(QScriptEngine* engine, const GymEvent& event) {
    QScriptValue obj = engine->newObject();
    obj.setProperty(GYM_ACTION_PROP_NAME, event.action);
    return obj;
}

void gymEventFromScriptValue(const QScriptValue &object, GymEvent& event) {
    event.action = object.property(GYM_ACTION_PROP_NAME).toVariant().toUInt();
}