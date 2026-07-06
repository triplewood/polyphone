/***************************************************************************
**                                                                        **
**  Polyphone, a soundfont editor                                         **
**  Copyright (C) 2013-2024 Davy Triponney                                **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program. If not, see http://www.gnu.org/licenses/.    **
**                                                                        **
****************************************************************************
**           Author: Davy Triponney                                       **
**  Website/Contact: https://www.polyphone.io                             **
**             Date: 01.01.2013                                           **
***************************************************************************/

#include <QFileInfo>
#include <QItemSelection>
#include <QDir>
#include <QScreen>
#include <QCoreApplication>
#ifndef POLYPHONE_NO_GUI
#include <QApplication>
#include <QStyleFactory>
#endif
#include "soundfontmanager.h"
#include "inputfactory.h"
#include "abstractinputparser.h"
#include "outputfactory.h"
#include "abstractoutput.h"
#include "options.h"
#include "contextmanager.h"
#ifndef POLYPHONE_NO_GUI
#include "qtsingleapplication.h"
#include "mainwindow.h"
#endif
#include "translationmanager.h"
#include "modulatordata.h"
#include "fastmaths.h"
#include "synth.h"
#include "voice.h"
#include "runnablemerger.h"

#include <iostream>
#include <QTextStream>
#include <QDebug>

void writeLine(QString line)
{
    QTextStream out(stdout);
    out << line << Qt::endl;
}

#ifndef POLYPHONE_NO_GUI
int launchApplication(QtSingleApplication * app, Options &options)
{
    // Prepare arrays
    SFModulator::prepareConversionTables();
    FastMaths::initialize();
    Voice::prepareTables();

    // Display the main window
    MainWindow w(options.mode() == Options::MODE_SYNTHESIZER);
    w.show();

    // Open files passed as argument
    QStringList inputFiles = options.getInputFiles();
    foreach (QString file, inputFiles)
        w.openFiles(file);

#ifdef Q_OS_MAC
    QObject::connect(qApp, SIGNAL(openFile(QString)), &w, SLOT(openFiles(QString)));
#endif

    return app->exec();
}
#endif

/// Error codes
/// 1: input file does not exist, or output file already exists
/// 2: bad extension
/// 3: cannot open the input file (corrupted are not accessible)
/// 4: cannot save the output file (internal problem or write access denied)
int convert(Options &options)
{
    // Check the input file
    QFileInfo inputFile(options.getInputFiles()[0]);
    if (!inputFile.exists())
    {
        qWarning() << "The file" << inputFile.filePath() << "does not exist.";
        return 1;
    }

    // Check the output
    QFileInfo outputFile(options.getOutputFileFullPath());
    if (!QDir(options.getOutputDirectory()).exists())
    {
        qWarning() << "The directory" << options.getOutputDirectory() << "does not exist.";
        return 1;
    }
    if (outputFile.exists() && options.mode() != Options::MODE_CONVERSION_TO_SFZ)
    {
        qWarning() << "The file" << outputFile.filePath() << "already exists.";
        return 1;
    }

    // Load input file
    qInfo() << "Loading file" << inputFile.filePath() << "...";
    AbstractInputParser * input = InputFactory::getInput(inputFile.filePath());
    input->process(false);
    if (!input->isSuccess())
    {
        qWarning() << "Couldn't load" << inputFile.filePath() + ":" << input->getError();
        delete input;
        return 1;
    }
    int sf2Index = input->getSf2Index();
    delete input;
    qInfo() << "File loaded";

    // Prepare the output with the respective options
    AbstractOutput * output = OutputFactory::getOutput(outputFile.filePath());
    SoundfontManager * sm = SoundfontManager::getInstance();
    EltID sf2Id(elementSf2, sf2Index);
    AttributeValue value;
    switch (options.mode())
    {
    case Options::MODE_CONVERSION_TO_SF2:
        // The soundfont version drives the output format: version 2.04 means no compression
        if (sm->get(sf2Id, champ_IFIL).sfVerValue.wMajor >= 3)
        {
            value.sfVerValue.wMajor = 2;
            value.sfVerValue.wMinor = 4;
            sm->set(sf2Id, champ_IFIL, value);
        }
        break;
    case Options::MODE_CONVERSION_TO_SF3:
        // Compression requires soundfont version 3.00 and a 16-bit depth
        value.sfVerValue.wMajor = 3;
        value.sfVerValue.wMinor = 0;
        sm->set(sf2Id, champ_IFIL, value);
        value.wValue = 16;
        sm->set(sf2Id, champ_wBpsSave, value);
        output->setOption("quality", options.sf3Quality());
        break;
    case Options::MODE_CONVERSION_TO_SFZ: {
        output->setOption("prefix", options.sfzPresetPrefix());
        output->setOption("bankdir", options.sfzOneDirPerBank());
        output->setOption("gmsort", options.sfzGeneralMidi());
        qInfo() << "Exporting in directory" << options.getOutputDirectory() << "...";
    } break;
    default:
        qWarning() << "fail";
        return 1;
    }

    // Convert
    output->process(sf2Index, false);
    if (!output->isSuccess())
    {
        qWarning() << "Couldn't create" << outputFile.filePath() + ":" << output->getError();
        delete output;
        return 1;
    }
    delete output;
    qInfo() << "done";

    // Destroy a singleton that has been silently created
    SoundfontManager::kill();
    return 0;
}

int resetConfig(Options &options)
{
    Q_UNUSED(options)

    QSettings settings;
    settings.clear();
    qInfo() << "Previous configuration is now cleared.";

    return 0;
}

int displayHelp(Options &options)
{
    Q_UNUSED(options)
    QString exe = QFileInfo(QCoreApplication::arguments().value(0)).fileName();
    if (exe.isEmpty())
        exe = "polyphone";

    writeLine("Polyphone command line usage:");
    writeLine("  " + exe + " [FILE1] [FILE2] ...");
#ifndef POLYPHONE_NO_GUI
    writeLine("  " + exe + " -0 [-i INPUT_FILE]");
#endif
    writeLine("  " + exe + " -1 -i INPUT_FILE [-d OUTPUT_DIR] [-o OUTPUT_NAME]");
    writeLine("  " + exe + " -2 -i INPUT_FILE [-d OUTPUT_DIR] [-o OUTPUT_NAME] [-c QUALITY]");
    writeLine("  " + exe + " -3 -i INPUT_FILE [-d OUTPUT_DIR] [-o OUTPUT_NAME] [-c SFZ_OPTIONS]");
#ifndef POLYPHONE_NO_GUI
    writeLine("  " + exe + " -s -i INPUT_FILE [-c SYNTH_OPTIONS]");
#endif
    writeLine("  " + exe + " -r");
    writeLine("  " + exe + " -h");
    writeLine("");
    writeLine("Options:");
    writeLine("  -i INPUT_FILE      Input file (sf2, sf3, sfz, sfArk, organ)");
    writeLine("  -d OUTPUT_DIR      Output directory (default: input file directory)");
    writeLine("  -o OUTPUT_NAME     Output base name (default: input file base name)");
    writeLine("  -c CONFIG          Extra mode-specific config:");
    writeLine("                     sf3 quality: 0 (low), 1 (medium), 2 (high)");
    writeLine("                     sfz options: presetPrefix|oneDirPerBank|gmSort (e.g. 0|1|1)");
#ifndef POLYPHONE_NO_GUI
    writeLine("                     synth options: midiChannel|multiPreset|toggleByLowKeys");
#endif
    writeLine("  -1                 Convert input to .sf2");
    writeLine("  -2                 Convert input to .sf3");
    writeLine("  -3                 Convert input to .sfz");
#ifndef POLYPHONE_NO_GUI
    writeLine("  -0                 Open GUI mode");
    writeLine("  -s                 Open synthesizer mode");
#endif
    writeLine("  -r                 Reset Polyphone configuration");
    writeLine("  -h                 Show this help");
    writeLine("");
    writeLine("Examples:");
    writeLine("  " + exe + " -1 -i file.sfArk");
    writeLine("  " + exe + " -2 -i file.sf2 -c 2");
    writeLine("  " + exe + " -3 -i file.sf3 -c 0|1|1");
#ifndef POLYPHONE_NO_GUI
    writeLine("  " + exe + " -s -i file.sf2 -c all|off|toggle");
#endif
    writeLine("");
#ifdef _WIN32
    writeLine(QObject::tr("See \"%1\" for more information.").arg("https://www.polyphone.io/documentation/manual/annexes/command-line"));
#else
    writeLine(QObject::tr("Write \"%1\" to show usage.").arg("man polyphone"));
#endif
    return 0;
}

/// This test, using the merge tool, can be called for profiling the sound generation in voice.cpp
int testVoice()
{
    ContextManager::initializeNoAudioMidi();

    QString filePath = "/home/davy/Téléchargements/GO - Plein jeu V.sf2";
    AbstractInputParser * input = InputFactory::getInput(filePath);
    input->process(false);
    if (!input->isSuccess())
    {
        qWarning() << "Couldn't load" << filePath + ":" << input->getError();
        delete input;
        return 1;
    }
    int sf2Index = input->getSf2Index();
    delete input;

    // The first preset is used
    EltID elementPrstID = EltID(elementPrst, sf2Index, 0);

    for (int key = 36; key <= 40; key++)
    {
        writeLine("Key: " + QString::number(key));
        RunnableMerger merger(
            nullptr,
            elementPrstID,  // The first preset is used
            key, key,
            false, // No loop
            true, // Stereo enabled
            4, // Create samples lasting 4s (sustained phase)
            2  // With a 2s releaase
        );
        merger.run();
        writeLine("ok");
    }

    return 0;
}

/// This test can be used for testing the synth parallel computing mechanisms
int testSynth()
{
    SFModulator::prepareConversionTables();
    FastMaths::initialize();
    Voice::prepareTables();

    int bufferLength = 512;
    int sampleRate = 48000;
    int duration = 2; // in seconds
    int sf2Index = -1;

    ContextManager::initializeNoAudioMidi();
    QString filePath = qEnvironmentVariable("POLYPHONE_SYNTH_SELFTEST_FILE");
    if (!filePath.isEmpty())
    {
        AbstractInputParser * input = InputFactory::getInput(filePath);
        input->process(false);
        if (!input->isSuccess())
        {
            qWarning() << "Synth selftest couldn't load" << filePath + ":" << input->getError();
            delete input;
            return 1;
        }
        sf2Index = input->getSf2Index();
        delete input;
    }

    Synth * synth = new Synth(SoundfontManager::getInstance()->getSoundfonts(), SoundfontManager::getInstance()->getMutex());
    synth->configure(ContextManager::configuration()->getSynthConfig());
    synth->setSampleRateAndBufferSize(sampleRate, bufferLength);

    EltID presetId = EltID(elementPrst, sf2Index, 0);
    float* output = new float[bufferLength * 2];
    float maxAbs = 0.f;
    for (int i = 0; i < duration * sampleRate / bufferLength; i++)
    {
        if (i == 0)
            synth->play(presetId, -1, 60, 100);
        if (i == sampleRate / bufferLength / 2)
            synth->play(presetId, -1, 60, 0); // Note off

        synth->readData(&output[0], &output[bufferLength], bufferLength);
        for (int j = 0; j < bufferLength * 2; ++j)
            maxAbs = qMax(maxAbs, qAbs(output[j]));
    }

    qInfo() << "Synth selftest maxAbs" << maxAbs << "file" << filePath << "preset" << presetId.toString();

    delete [] output;
    delete synth;

    return maxAbs > 0.f ? 0 : 2;
}

int main(int argc, char *argv[])
{
    //return testVoice();
    //return testSynth();

    if (qEnvironmentVariableIsSet("POLYPHONE_SYNTH_SELFTEST"))
    {
        if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "offscreen");
        QApplication app(argc, argv);
        QCoreApplication::setApplicationName("Polyphone");
        QCoreApplication::setOrganizationName("polyphone");
        return testSynth();
    }

#ifdef Q_OS_LINUX
#ifndef POLYPHONE_NO_GUI
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::RoundPreferFloor);
#endif
#endif

#if QT_VERSION < 0x060000
#ifndef POLYPHONE_NO_GUI
    // Dpi scaling
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
#endif
#endif
#ifndef POLYPHONE_NO_GUI
    QtSingleApplication app("polyphone", argc, argv);
#else
    QCoreApplication app(argc, argv);
#endif
    QCoreApplication::setApplicationName("Polyphone");
    QCoreApplication::setOrganizationName("polyphone");
#ifndef POLYPHONE_NO_GUI
#ifdef _WIN32
    QFont f = app.font(); // Global font size so that it scales
    f.setPointSize(9);
    app.setFont(f);
#endif
#endif

    Options options(argc, argv);
    int valRet = 0;

    // Possibly launch the application
#ifndef POLYPHONE_NO_GUI
    if (!options.error() && !options.help() && (options.mode() == Options::MODE_GUI || options.mode() == Options::MODE_SYNTHESIZER))
    {
        QSettings settings;
        if (app.sendMessage(options.getInputFiles().join("|")))
            return 0;

        // Preparation
        SFModulator::prepareConversionTables();
        FastMaths::initialize();
        Voice::prepareTables();

        // Or launch the application as a unique instance
        return launchApplication(&app, options);
    }
#endif

    // Otherwise, console mode
#ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif

    if (options.error() || options.help())
        valRet = displayHelp(options);
    else if (options.mode() == Options::MODE_RESET_CONFIG)
        valRet = resetConfig(options);
    else
        valRet = convert(options);

    return valRet;
}
