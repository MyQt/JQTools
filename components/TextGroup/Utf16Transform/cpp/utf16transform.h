﻿/*
    This file is part of JQTools

    Project introduce: https://github.com/188080501/JQTools

    Copyright: Jason

    Contact email: Jason@JasonServer.com

    GitHub: https://github.com/188080501/
*/

#ifndef __GROUP_TEXTGROUP_UTF16TRANSFORM_CPP_UTF16TRANSFORM_H__
#define __GROUP_TEXTGROUP_UTF16TRANSFORM_CPP_UTF16TRANSFORM_H__

// JQToolsLibrary import
#include "JQToolsLibrary.h"

#define UTF16TRANSFORM_INITIALIZA                                                               \
{                                                                                               \
    qmlRegisterType<Utf16Transform::Manage>("Utf16Transform", 1, 0, "Utf16TransformManage");    \
}

namespace Utf16Transform
{

class Manage: public AbstractTool
{
    Q_OBJECT
    Q_DISABLE_COPY(Manage)

public:
    Manage() = default;

    ~Manage() = default;

public slots:
    QString toUtf16(const QString &string);

    QString fromUtf16(const QString &string);
};

}

#endif//__GROUP_TEXTGROUP_UTF16TRANSFORM_CPP_UTF16TRANSFORM_H__
