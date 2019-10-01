//
//  FileTypeProfile.cpp
//  interface/src/networking
//
//  Created by Kunal Gosar on 2017-03-10.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "FileTypeProfile.h"

#include <QtQml/QQmlContext>

#include "RequestFilters.h"

#if !defined(Q_OS_ANDROID)
static const QString QML_WEB_ENGINE_STORAGE_NAME = "qmlWebEngine";

FileTypeProfile::FileTypeProfile(QQmlContext* parent) :
    ContextAwareProfile(parent)
{
    static const QString WEB_ENGINE_USER_AGENT = "Chrome/48.0 (HighFidelityInterface)";
    setHttpUserAgent(WEB_ENGINE_USER_AGENT);

    auto requestInterceptor = new RequestInterceptor(this);
    setRequestInterceptor(requestInterceptor);
}

void FileTypeProfile::RequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo& info) {
    RequestFilters::interceptHFWebEngineRequest(info, isRestricted());
    RequestFilters::interceptFileType(info);
}

void FileTypeProfile::registerWithContext(QQmlContext* context) {
    context->setContextProperty("FileTypeProfile", new FileTypeProfile(context));
}


#endif