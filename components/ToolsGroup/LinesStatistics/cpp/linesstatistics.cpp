﻿/*
    This file is part of JQTools

    Project introduce: https://github.com/188080501/JQTools

    Copyright: Jason

    Contact email: Jason@JasonServer.com

    GitHub: https://github.com/188080501/
*/

#include "linesstatistics.h"

// Qt lib import
#include <QSet>
#include <QFileDialog>
#include <QStandardPaths>
#include <QJsonArray>
#include <QEventLoop>
#include <QtConcurrent>

// JQLibrary import
#include "JQFile.h"

using namespace LinesStatistics;

QJsonObject Manage::statisticsLines(const QJsonArray &suffixs)
{
    auto fileCount = 0;
    auto lineCount = 0;

    auto currentPath = QFileDialog::getExistingDirectory( nullptr,
                                                          "Please choose code dir",
                                                          QStandardPaths::writableLocation( QStandardPaths::DesktopLocation ) );
    if ( currentPath.isEmpty() )
    {
        return
            { {
                { "cancel", true }
            } };
    }

    QSet< QString > availableSuffixs;

    for ( const auto suffix: suffixs )
    {
        availableSuffixs.insert( suffix.toString().toLower() );
    }

    QEventLoop eventLoop;

    QtConcurrent::run( [ & ]()
    {
        static QSet< QString > imageSuffixs;
        if ( imageSuffixs.isEmpty() )
        {
            imageSuffixs.insert( "png" );
            imageSuffixs.insert( "jpg" );
            imageSuffixs.insert( "jpeg" );
            imageSuffixs.insert( "bmp" );
            imageSuffixs.insert( "gif" );
            imageSuffixs.insert( "svg" );
            imageSuffixs.insert( "psd" );
            imageSuffixs.insert( "ai" );
        }

        JQFile::foreachFileFromDirectory( { currentPath }, [ & ](const QFileInfo &info)
        {
            if ( !availableSuffixs.contains( info.suffix().toLower() ) ) { return; }

            QFile file( info.filePath() );
            if ( !file.open(QIODevice::ReadOnly) ) { return; }

            fileCount++;

            const auto &&fileAllData = file.readAll();

            if ( fileAllData.isEmpty() ) { return; }

            if ( imageSuffixs.contains( info.suffix().toLower() ) ) { return; }

            lineCount += fileAllData.count('\n') + 1;
        }, true);

        eventLoop.quit();
    } );

    eventLoop.exec();

    return
        { {
            { "fileCount", fileCount },
            { "lineCount", lineCount }
        } };
}
