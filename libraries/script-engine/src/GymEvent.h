//
//  GymEvent.h
//  libraries/script-engine/src
//
//  Created by Wil Bown on 2018-12-20
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <QtScript/QScriptEngine>

#ifndef hifi_GymEvent_h
#define hifi_GymEvent_h

class GymEvent {
public:
    unsigned int action;
};

Q_DECLARE_METATYPE(GymEvent)

void registerGymMetaTypes(QScriptEngine* engine);

QScriptValue gymEventToScriptValue(QScriptEngine* engine, const GymEvent& event);
void gymEventFromScriptValue(const QScriptValue &object, GymEvent& event);

#endif // hifi_GymEvent_h