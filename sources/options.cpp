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

#include "options.h"
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include "inputfactory.h"
#include "playeroptions.h"

Options::Options(int argc, char *argv[]) :
    _currentState(STATE_INPUT_FILE), // By default args are input files
    _mode(MODE_GUI),
    _error(false),
    _help(false),
    _sf3Quality(1),
    _sf3Codec("vorbis"),
    _sf3CodecOptionSeen(false),
    _sf3QualityOptionSeen(false),
    _sfzPresetPrefix(false),
    _sfzOneDirPerBank(false),
    _sfzGeneralMidi(false),
    _csvRawValues(false),
    _playerOptions(nullptr)
{
    _appPath = QFileInfo(QCoreApplication::applicationFilePath()).path();

    // Convert into QStringList
    QStringList args;
    for (int i = 1; i < argc; i++) // The first argument is rejected (executable path)
#ifdef Q_OS_WIN
        args << QString::fromLatin1(argv[i]);
#else
        args << QString(argv[i]);
#endif

    // Argument processing
    foreach (QString arg, args)
    {
        if (arg.isEmpty())
            continue;

        if (arg.startsWith("--") && _currentState != STATE_SF3_CODEC &&
            _currentState != STATE_SF3_QUALITY)
            processLongOption(arg);
        else if (arg[0] == '-' && _currentState != STATE_SF3_CODEC &&
                 _currentState != STATE_SF3_QUALITY)
            processType1(arg);
        else
            processType2(arg);

        if (_error)
            break;
    }

    // Check for errors
    if (!_error)
        checkErrors();

    // Post-treatment
    if (!_error)
        postTreatment();
}

Options::~Options()
{
    delete _playerOptions;
}

void Options::processType1(QString arg)
{
    if (arg.size() != 2)
    {
        _error = true;
        return;
    }

    switch (arg[1].toLatin1())
    {
    case '0':
#ifdef POLYPHONE_NO_GUI
        _error = true;
#else
        _mode = MODE_GUI;
#endif
        break;
    case '1':
        _mode = MODE_CONVERSION_TO_SF2;
        break;
    case '2':
        _mode = MODE_CONVERSION_TO_SF3;
        break;
    case '3':
        _mode = MODE_CONVERSION_TO_SFZ;
        break;
    case '4':
        _mode = MODE_CONVERSION_TO_CSV;
        break;
    case 'd':
        _currentState = STATE_OUTPUT_DIRECTORY;
        break;
    case 'i':
        _currentState = STATE_INPUT_FILE;
        break;
    case 'o':
        _currentState = STATE_OUTPUT_FILE;
        break;
    case 'c':
        _currentState = STATE_CONFIG;
        break;
    case 'C':
        _sf3CodecOptionSeen = true;
        _currentState = STATE_SF3_CODEC;
        break;
    case 'q':
        _sf3QualityOptionSeen = true;
        _currentState = STATE_SF3_QUALITY;
        break;
    case 'h':
        _help = true;
        break;
    case 'r':
        _mode = MODE_RESET_CONFIG;
        break;
    case 's':
#ifdef POLYPHONE_NO_GUI
        _error = true;
#else
        _mode = MODE_SYNTHESIZER;
#endif
        break;
    default:
        _error = true;
        break;
    }
}

void Options::processLongOption(QString arg)
{
    QString option = arg.mid(2);
    QString value;
    int separator = option.indexOf('=');
    if (separator >= 0)
    {
        value = option.mid(separator + 1);
        option = option.left(separator);
    }

    if (option == "codec" || option == "sf3-codec" || option == "sf3-encoding")
    {
        _sf3CodecOptionSeen = true;
        _currentState = STATE_SF3_CODEC;
    }
    else if (option == "quality" || option == "sf3-quality")
    {
        _sf3QualityOptionSeen = true;
        _currentState = STATE_SF3_QUALITY;
    }
    else
    {
        _error = true;
        return;
    }

    if (separator >= 0)
        processType2(value);
}

void Options::processType2(QString arg)
{
    switch (_currentState)
    {
    case STATE_INPUT_FILE:
        _inputFiles << arg;
        break;
    case STATE_OUTPUT_DIRECTORY:
        _outputDirectory = arg;
        _currentState = STATE_NONE; // no more output
        break;
    case STATE_OUTPUT_FILE:
        _outputFile = arg;
        _currentState = STATE_NONE; // no more output
        break;
    case STATE_SF3_CODEC: {
        QString codec = arg.trimmed().toLower();
        if (codec == "vorbis" || codec == "flac")
            _sf3Codec = codec;
        else
            _error = true;
        _currentState = STATE_NONE;
        break;
    }
    case STATE_SF3_QUALITY: {
        bool ok = false;
        int quality = arg.toInt(&ok);
        if (ok && quality >= 0 && quality <= 2)
            _sf3Quality = quality;
        else
            _error = true;
        _currentState = STATE_NONE;
        break;
    }
    case STATE_CONFIG: {
        QStringList configurations = arg.split('|');

        switch (_mode)
        {
        case MODE_CONVERSION_TO_SFZ:
            if (configurations.count() >= 1)
            {
                if (configurations[0] == "1")
                    _sfzPresetPrefix = true;
                else if (configurations[0] != "0")
                    _error = true;
            }

            if (configurations.count() >= 2)
            {
                if (configurations[1] == "1")
                    _sfzOneDirPerBank = true;
                else if (configurations[1] != "0")
                    _error = true;
            }

            if (configurations.count() >= 3)
            {
                if (configurations[2] == "1")
                    _sfzGeneralMidi = true;
                else if (configurations[2] != "0")
                    _error = true;
            }

            if (configurations.count() > 3)
                _error = true;
            break;
        case  MODE_CONVERSION_TO_SF2:
            if (configurations.count() > 0)
                _error = true;
            break;
        case MODE_CONVERSION_TO_SF3: {
            if (configurations.count() > 2)
            {
                _error = true;
                break;
            }

            bool qualitySeen = false;
            bool codecSeen = false;
            foreach (QString configuration, configurations)
            {
                QString value = configuration.trimmed().toLower();
                if (value == "vorbis" || value == "flac")
                {
                    if (codecSeen)
                    {
                        _error = true;
                        break;
                    }
                    _sf3Codec = value;
                    codecSeen = true;
                }
                else if (!qualitySeen && value == "0")
                    _sf3Quality = 0;
                else if (!qualitySeen && value == "1")
                    _sf3Quality = 1;
                else if (!qualitySeen && value == "2")
                    _sf3Quality = 2;
                else
                {
                    _error = true;
                    break;
                }
                if (value == "0" || value == "1" || value == "2")
                    qualitySeen = true;
            }
            break;
        }
        case MODE_CONVERSION_TO_CSV:
            if (configurations.count() > 1)
                _error = true;
            _csvRawValues = configurations[0] == "raw";
            break;
        case MODE_SYNTHESIZER:
            if (_playerOptions == nullptr)
            {
                _playerOptions = new PlayerOptions();
                _error = !_playerOptions->parse(configurations);
            }
            else
                _error = true;
            break;
        default:
            _error = true;
            break;
        }

    } break;
    default:
        _error = true;
        break;
    }
}

void Options::checkErrors()
{
    // Input files
    foreach (QString inputFile, _inputFiles)
    {
        if (QFileInfo(inputFile).isDir() && _mode == MODE_GUI)
            continue;
        if (!InputFactory::isSuffixSupported(QFileInfo(inputFile).suffix()))
        {
            _error = true;
            return;
        }
    }

    switch (_mode)
    {
    case MODE_RESET_CONFIG:
        _error = false;
        break;
    case MODE_GUI: case MODE_SYNTHESIZER:
        if (_outputDirectory != "" || _outputFile != "")
            _error = true;
        break;
    case MODE_CONVERSION_TO_SF2: case MODE_CONVERSION_TO_SF3:
    case MODE_CONVERSION_TO_SFZ: case MODE_CONVERSION_TO_CSV:
        if (_inputFiles.count() != 1)
            _error = true;
        break;
    }

    // Encoding-specific options are meaningful only for SF3 conversion.
    if ((_sf3CodecOptionSeen || _sf3QualityOptionSeen) && _mode != MODE_CONVERSION_TO_SF3)
        _error = true;
    if (_currentState == STATE_SF3_CODEC || _currentState == STATE_SF3_QUALITY)
        _error = true;
}

void Options::postTreatment()
{
    if (_mode == MODE_CONVERSION_TO_SF2 || _mode == MODE_CONVERSION_TO_SF3 ||
        _mode == MODE_CONVERSION_TO_SFZ || _mode == MODE_CONVERSION_TO_CSV)
    {
        // By default, the output directory is the same than the input file directory
        if (_outputDirectory == "")
            _outputDirectory = QFileInfo(_inputFiles[0]).dir().absolutePath();

        // By default, the output file name is the same than the input file name
        if (_outputFile == "")
            _outputFile = QFileInfo(_inputFiles[0]).completeBaseName();
    }
}

QString Options::getOutputFileFullPath()
{
    QString strTmp = _outputDirectory;
    if (!strTmp.endsWith('/'))
        strTmp += '/';

    // Extension
    QString extension = "";
    switch (_mode)
    {
    case MODE_CONVERSION_TO_SF2:
        extension = ".sf2";
        break;
    case MODE_CONVERSION_TO_SF3:
        extension = ".sf3";
        break;
    case MODE_CONVERSION_TO_SFZ:
        extension = ".sfz";
        break;
    case MODE_CONVERSION_TO_CSV:
        extension = ".csv";
        break;
    default:
        break;
    }

    return strTmp + _outputFile + extension;
}

QString Options::getInputFilesAsString()
{
    // All files
    QStringList filesToOpen = _inputFiles;
    filesToOpen.removeAll("");
    QString str = filesToOpen.join('|');

    // Possible add the player options
    if (_playerOptions != nullptr)
        str += "||" + _playerOptions->toString();

    return str;
}
