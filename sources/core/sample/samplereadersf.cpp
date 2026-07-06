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

#include "samplereadersf.h"
#include <vorbis/vorbisenc.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <sndfile.h>
#include <limits>
#include <vector>

static size_t ovRead(void* ptr, size_t size, size_t nmemb, void* datasource);
static int ovSeek(void* datasource, ogg_int64_t offset, int whence);
static long ovTell(void* datasource);
static ov_callbacks ovCallbacks = { ovRead, ovSeek, nullptr, ovTell };

struct FlacData
{
    sf_count_t pos;
    sf_count_t length;
    const char* data;
};

static sf_count_t sfFlacGetFileLen(void* userData);
static sf_count_t sfFlacSeek(sf_count_t offset, int whence, void* userData);
static sf_count_t sfFlacRead(void* ptr, sf_count_t count, void* userData);
static sf_count_t sfFlacTell(void* userData);
static SF_VIRTUAL_IO sfFlacVirtualIo = { sfFlacGetFileLen, sfFlacSeek, sfFlacRead, nullptr, sfFlacTell };

static size_t ovRead(void* ptr, size_t size, size_t nmemb, void* datasource)
{
    SampleReaderSf::VorbisData* vd = (SampleReaderSf::VorbisData*)datasource;
    size_t n = size * nmemb;
    if (vd->length < vd->pos + n)
        n = vd->length - vd->pos;
    if (n)
    {
        const char* src = vd->data + vd->pos;
        memcpy(ptr, src, n);
        vd->pos += n;
    }

    return n;
}

static int ovSeek(void* datasource, ogg_int64_t offset, int whence)
{
    SampleReaderSf::VorbisData* vd = (SampleReaderSf::VorbisData*)datasource;
    switch(whence) {
    case SEEK_SET:
        vd->pos = offset;
        break;
    case SEEK_CUR:
        vd->pos += offset;
        break;
    case SEEK_END:
        vd->pos = vd->length - offset;
        break;
    }
    return 0;
}

static long ovTell(void* datasource)
{
    SampleReaderSf::VorbisData* vd = (SampleReaderSf::VorbisData*)datasource;
    return vd->pos;
}

static sf_count_t sfFlacGetFileLen(void* userData)
{
    FlacData* fd = (FlacData*)userData;
    return fd->length;
}

static sf_count_t sfFlacSeek(sf_count_t offset, int whence, void* userData)
{
    FlacData* fd = (FlacData*)userData;
    sf_count_t newPos = fd->pos;

    switch (whence) {
    case SEEK_SET:
        newPos = offset;
        break;
    case SEEK_CUR:
        newPos += offset;
        break;
    case SEEK_END:
        newPos = fd->length + offset;
        break;
    default:
        return -1;
    }

    if (newPos < 0 || newPos > fd->length)
        return -1;

    fd->pos = newPos;
    return fd->pos;
}

static sf_count_t sfFlacRead(void* ptr, sf_count_t count, void* userData)
{
    FlacData* fd = (FlacData*)userData;
    if (fd->pos >= fd->length)
        return 0;

    sf_count_t available = fd->length - fd->pos;
    sf_count_t n = count < available ? count : available;
    if (n > 0)
    {
        memcpy(ptr, fd->data + fd->pos, static_cast<size_t>(n));
        fd->pos += n;
    }

    return n;
}

static sf_count_t sfFlacTell(void* userData)
{
    FlacData* fd = (FlacData*)userData;
    return fd->pos;
}

SampleReaderSf::SampleReaderSf(QString filename) :
    SampleReader(filename),
    _info(nullptr)
{

}

SampleReader::SampleReaderResult SampleReaderSf::getInfo(QFile &fi, InfoSound* info)
{
    Q_UNUSED(fi)

    // Info completed outside for an sf2: we keep the pointer
    _info = info;

    // Extra info
    info->wChannel = 0;
    info->wChannels = 1;
    info->pitchDefined = true; // So that we don't try to find the key based on the filename

    return FILE_OK;
}

SampleReader::SampleReaderResult SampleReaderSf::getRawData(QFile &fi, char* &rawData, quint32 &length)
{
    // Read a compressed sample in a sf3 soundfont
    rawData = nullptr;
    length = 0;
    if (_info->dwLength == 0)
        return FILE_OK;

    fi.seek(_info->dwStart);
    rawData = new char[_info->dwLength];
    qint64 nb = fi.read(rawData, _info->dwLength);
    if (nb != (qint64)(_info->dwLength))
    {
        delete [] rawData;
        rawData = nullptr;
        return nb == -1 ? FILE_NOT_READABLE : FILE_CORRUPT;
    }
    length = _info->dwLength;

    return FILE_OK;
}

SampleReader::SampleReaderResult SampleReaderSf::getData(QFile &fi, qint16* &data16, quint8* &data24, const char* rawData, quint32 rawDataLength)
{
    data16 = nullptr;
    data24 = nullptr;
    if (_info->dwLength == 0)
        return FILE_OK;

    if (rawData == nullptr)
    {
        // Load the smpl part of an sf2
        fi.seek(_info->dwStart);
        data16 = new qint16[_info->dwLength];
        qint64 nb = fi.read((char *)data16, _info->dwLength * sizeof(qint16));
        if (nb != (qint64)(_info->dwLength * sizeof(qint16)))
        {
            delete [] data16;
            data16 = nullptr;
            return nb == -1 ? FILE_NOT_READABLE : FILE_CORRUPT;
        }

        // Possibly load the sm24 part of an sf2
        if (_info->wBpsFile >= 24)
        {
            data24 = new quint8[_info->dwLength];
            fi.seek(_info->dwStart2);
            qint64 nb = fi.read((char *)data24, _info->dwLength);
            if (nb != (qint64)_info->dwLength)
            {
                delete [] data16;
                delete [] data24;
                data16 = nullptr;
                data24 = nullptr;
                return nb == -1 ? FILE_NOT_READABLE : FILE_CORRUPT;
            }
        }
    }
    else
    {
        if (rawDataLength < 4)
            return FILE_CORRUPT;

        if (memcmp(rawData, "OggS", 4) == 0)
        {
            // Extract wave data from a compressed OGG sample
            _vorbisData.pos  = 0;
            _vorbisData.data = rawData;
            _vorbisData.length = rawDataLength;

            OggVorbis_File vf;
            if (ov_open_callbacks(&_vorbisData, &vf, nullptr, 0, ovCallbacks) == 0)
            {
                qint64 length = ov_pcm_total(&vf, -1);
                if (length <= 0)
                    return FILE_CORRUPT;
                data16 = new qint16[length];

                int numberRead = 0;
                qint64 totalRead = 0;
                do {
                    numberRead = ov_read(&vf, &((char *)data16)[totalRead], 2048, 0, 2, 1, nullptr);
                    totalRead += numberRead;
                } while (numberRead > 0);
                ov_clear(&vf);

                if (totalRead != length * (qint64)sizeof(short))
                {
                    delete [] data16;
                    data16 = nullptr;
                    return numberRead == 0 ? FILE_NOT_READABLE : FILE_CORRUPT;
                }

                // Update the field "length" with the uncompressed data length
                _info->dwLength = (quint32)length;
            }
            else
                return FILE_CORRUPT;
        }
        else if (memcmp(rawData, "fLaC", 4) == 0)
        {
            FlacData flacData;
            flacData.pos = 0;
            flacData.data = rawData;
            flacData.length = rawDataLength;

            SF_INFO sfInfo;
            memset(&sfInfo, 0, sizeof(SF_INFO));
            SNDFILE* sndFile = sf_open_virtual(&sfFlacVirtualIo, SFM_READ, &sfInfo, &flacData);
            if (sndFile == nullptr)
                return FILE_CORRUPT;

            if (sfInfo.frames <= 0 || sfInfo.channels <= 0 ||
                sfInfo.frames > std::numeric_limits<quint32>::max())
            {
                sf_close(sndFile);
                return FILE_CORRUPT;
            }

            data16 = new qint16[static_cast<quint32>(sfInfo.frames)];
            std::vector<int> interleavedData(static_cast<size_t>(sfInfo.frames) * static_cast<size_t>(sfInfo.channels));
            sf_count_t framesRead = sf_readf_int(sndFile, interleavedData.data(), sfInfo.frames);
            sf_close(sndFile);

            if (framesRead != sfInfo.frames)
            {
                delete [] data16;
                data16 = nullptr;
                return FILE_CORRUPT;
            }

            int channel = qMin(static_cast<int>(_info->wChannel), sfInfo.channels - 1);
            for (sf_count_t i = 0; i < sfInfo.frames; i++)
                data16[i] = interleavedData[static_cast<size_t>(i) * static_cast<size_t>(sfInfo.channels) + static_cast<size_t>(channel)] >> 16;

            // Update the field "length" with the uncompressed data length
            _info->dwLength = static_cast<quint32>(sfInfo.frames);
        }
        else
            return FILE_CORRUPT;
    }

    return FILE_OK;
}
