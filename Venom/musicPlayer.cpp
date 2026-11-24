// SPDX-License-Identifier: GPL-3.0-or-later

#include "musicPlayer.h"
#include <string.h>
#include <dsound.h>
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <ntddcdrm.h>
#include <spdlog/spdlog.h>
#include <cmath>

MusicPlayer::MusicPlayer() {}
MusicPlayer::~MusicPlayer() { shutdown(); }

std::filesystem::path MusicPlayer::detectGameDirectory() {
    DWORD bufferSize = 1024;
    std::wstring pathBuffer(bufferSize, L'\0');
    DWORD copied = 0;

    while (true) {
        copied = GetModuleFileNameW(NULL, pathBuffer.data(), bufferSize);

        if (copied == 0) {
            spdlog::warn("detectGameDirectory: GetModuleFileNameW failed. Error code: {}", GetLastError());
            return std::filesystem::path();
        }

        if (copied >= bufferSize) {
            bufferSize *= 2;
            pathBuffer.resize(bufferSize);
        }
        else {
            pathBuffer.resize(copied);
            break;
        }
    }

    std::filesystem::path exePath(pathBuffer);
    m_gameDir = exePath.parent_path();
    m_gameDir.make_preferred();
    return m_gameDir;
}

void MusicPlayer::writeGameVolumeByte(int8_t v) { *m_volumeAddress = v; }

LONG MusicPlayer::dsVolumeFromGameVolume(int gameVolume) {
    if (gameVolume <= 0) { return DSBVOLUME_MIN; }
    if (gameVolume >= 8) { return DSBVOLUME_MAX; }
	
    /*
    constexpr LONG volumeTable[9] = {
        DSBVOLUME_MIN,   // 0
        -5000,           // 1
        -4000,           // 2
        -3000,           // 3
        -2000,           // 4
        -1000,           // 5
        -700,            // 6
        -400,            // 7
        DSBVOLUME_MAX    // 8
    };

    return volumeTable[gameVolume];
    */

    return LONG(DSBVOLUME_MIN * powf(1.0f - (gameVolume / 8.0f), 2.0f));
}

void MusicPlayer::setVolumeAddress(BYTE* addr) {
    if (addr) { m_volumeAddress = addr; }
	else { spdlog::warn("Unable to set music volume, required memory address is null."); }
}

bool MusicPlayer::init(HWND hwnd) {
    m_hwnd = hwnd;
    m_needReinit = false;

    HRESULT result = DirectSoundCreate(nullptr, &m_ds, nullptr);
    if (FAILED(result)) { spdlog::warn("DirectSoundCreate failed, hr=0x{:X}", result); return false; }

    result = m_ds->SetCooperativeLevel(hwnd, DSSCL_PRIORITY);
    if (FAILED(result)) { spdlog::warn("DirectSound SetCooperativeLevel failed, hr=0x{:X}", result); return false; }

    return true;
}

void MusicPlayer::shutdown() {
    if (m_track.buffer) {
        m_track.buffer->Stop();
        m_track.buffer->Release();
        m_track.buffer = nullptr;
    }
    m_track.pcm.clear();
    m_track.playing = false;
    m_track.paused = false;

    m_preloadedPcm.clear();
    m_preloadedId.clear();
    m_hasPreloaded = false;

    if (m_ds) {
        m_ds->Release();
        m_ds = nullptr;
    }
}

void MusicPlayer::stop(bool clearPlaylist) {
    if (m_track.buffer) {
        m_track.buffer->Stop();
    }
    m_track.playing = false;
    m_track.paused = false;

    if (clearPlaylist) {
        m_playlist.tracks.clear();
        m_playlist.currentIndex = -1;
        m_playlist.previousIndex = -1;
        m_hasPreloaded = false;
        m_preloadedPcm.clear();
        m_preloadedId.clear();
        m_nextIndexCached = -1;
    }
}

void MusicPlayer::pause() {
    if (m_track.buffer && m_track.playing) {
        m_track.buffer->Stop();
        m_track.playing = false;
        m_track.paused = true;
    }
}

void MusicPlayer::resume() {
    if (m_track.buffer && m_track.paused) {
        HRESULT result = m_track.buffer->Play(0, 0, m_track.looping ? DSBPLAY_LOOPING : 0);
        if (FAILED(result)) { spdlog::warn("DirectSound Play failed on resume, hr=0x{:X}", result); }
        else {
            m_track.playing = true;
            m_track.paused = false;
        }
    }
}

void MusicPlayer::queueDAPlaylist(bool shuffle, bool autoAdvance, bool loop, int firstTrack, int lastTrack) {
    m_playlist.tracks.clear();
    m_playlist.shuffle = shuffle;
    m_playlist.autoAdvance = autoAdvance;
    m_playlist.currentIndex = -1;
    m_playlist.previousIndex = -1;

    if (lastTrack >= firstTrack) { m_playlist.tracks.reserve(static_cast<size_t>(lastTrack - firstTrack + 1)); }

    char buf[16];
    for (int i = firstTrack; i <= lastTrack; ++i) {
        std::snprintf(buf, sizeof(buf), "TRACK%02d", i);

        TrackEntry t;
        t.trackName = buf;
        t.isWav = false;
        t.loop = loop;
        t.autoAdvance = autoAdvance;
        m_playlist.tracks.push_back(std::move(t));
    }

    if (shuffle && !m_playlist.tracks.empty()) { std::shuffle(m_playlist.tracks.begin(), m_playlist.tracks.end(), m_rng); }

    m_hasPreloaded = false;
    m_preloadedPcm.clear();
    m_preloadedId.clear();
    m_nextIndexCached = -1;
}

void MusicPlayer::queueWAVPlaylist(const std::vector<std::string>& baseNames, bool shuffle, bool autoAdvance) {
    m_playlist.tracks.clear();
    m_playlist.shuffle = shuffle;
    m_playlist.autoAdvance = autoAdvance;
    m_playlist.currentIndex = -1;
    m_playlist.previousIndex = -1;

    m_playlist.tracks.reserve(baseNames.size());
    for (const auto &name : baseNames) {
        TrackEntry t;
        t.trackName = name;
        t.isWav = true;
        t.loop = false;
        t.autoAdvance = true;
        m_playlist.tracks.push_back(std::move(t));
    }

    if (shuffle && !m_playlist.tracks.empty()) { std::shuffle(m_playlist.tracks.begin(), m_playlist.tracks.end(), m_rng); }

    m_hasPreloaded = false;
    m_preloadedPcm.clear();
    m_preloadedId.clear();
    m_nextIndexCached = -1;
}

const char* ReservedWavNames[] = {
    "CREDITS.WAV",
    "DEATH.WAV",
    "FAIL.WAV",
    "MENU.WAV",
    "SUCCESS.WAV"
};

const int ReservedWavCount = sizeof(ReservedWavNames) / sizeof(const char*);

void MusicPlayer::queueWAVPlaylist(bool shuffle, bool autoAdvance) {
    namespace fs = std::filesystem;
    std::vector<std::string> discoveredTracks;

    const std::filesystem::path targetDir = std::filesystem::path(m_gameDir) / "music";

    if (!fs::exists(targetDir) || !fs::is_directory(targetDir)) { spdlog::error("queueWAVPlaylist fallback: {} does not exist!", targetDir.generic_string()); return; }

    auto isReserved = [](std::string filename) -> bool {
        std::transform(filename.begin(), filename.end(), filename.begin(), ::toupper);

        for (int i = 0; i < ReservedWavCount; ++i) {
            std::string reservedStr = ReservedWavNames[i];
            std::transform(reservedStr.begin(), reservedStr.end(), reservedStr.begin(), ::toupper);
            if (filename == reservedStr) { return true; }
        }
        return false;
     };

    for (const std::filesystem::directory_entry &entry : fs::directory_iterator(targetDir)) {
        if (entry.is_regular_file()) {
            const std::filesystem::path &path = entry.path();

            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);

            if (ext == ".WAV" && !isReserved(path.filename().string())) { discoveredTracks.push_back(path.stem().string()); }
        }
    }
    queueWAVPlaylist(discoveredTracks, shuffle, autoAdvance);
}

bool MusicPlayer::playNamed(const std::string &name, bool loop) {
    m_playlist.tracks.clear();
    m_playlist.shuffle = false;
    m_playlist.autoAdvance = false;
    m_playlist.currentIndex = 0;
    m_playlist.previousIndex = -1;

    TrackEntry t;
    t.trackName = name;
    t.isWav = true;
    t.loop = loop;
    t.autoAdvance = false;
    m_playlist.tracks.push_back(std::move(t));

    return startTrackAtIndex(0);
}

bool MusicPlayer::startTrackAtIndex(int index) {
    if (index < 0 || index >= static_cast<int>(m_playlist.tracks.size())) { spdlog::warn("startTrackAtIndex: index {} out of range", index); return false; }

    TrackEntry& t = m_playlist.tracks[index];

    if (m_track.buffer) {
        m_track.buffer->Stop();
        m_track.buffer->Release();
        m_track.buffer = nullptr;
    }
    m_track.pcm.clear();
    m_track.playing = false;
    m_track.paused = false;

    bool ok = false;

    {
        std::lock_guard<std::mutex> lock(m_preloadMutex);
        if (m_hasPreloaded && m_preloadedId == t.trackName) {
            m_track.pcm = std::move(m_preloadedPcm);
            m_hasPreloaded = false;
            m_preloadedId.clear();
            ok = createBufferFromPCM(m_track);
        }
    }

    // Synchronous fallback. May cause microstutters in-game.
    if (!ok) {
        std::vector<uint8_t> pcm;
        bool loadOk = t.isWav ? loadWavTrackPCM(t.trackName, pcm) : loadDATrackPCM(t.trackName, pcm);

        if (!loadOk) { spdlog::warn("startTrackAtIndex: failed to load track '{}'", t.trackName); return false; }
        m_track.pcm = std::move(pcm);
        ok = createBufferFromPCM(m_track);
    }

    if (!ok || !m_track.buffer) { spdlog::warn("startTrackAtIndex: buffer creation failed for '{}'", t.trackName); return false; }
    else {
        m_track.buffer->SetVolume(dsVolumeFromGameVolume(m_gameVolume));

        HRESULT result = m_track.buffer->Play(0, 0, t.loop ? DSBPLAY_LOOPING : 0);
        if (FAILED(result)) { spdlog::warn("DirectSound Play failed, hr=0x{:X}", result); return false; }

        m_track.playing = true;
        m_track.looping = t.loop;
        m_track.playCursor = 0;
        m_playlist.currentIndex = index;

        preloadNextTrack();
        return true;
    }
}

bool MusicPlayer::playNextTrack() {
    if (m_playlist.tracks.empty()) { return false; }

    m_playlist.previousIndex = m_playlist.currentIndex;
    advancePlaylistIndexNext();

    if (m_playlist.currentIndex < 0 || m_playlist.currentIndex >= static_cast<int>(m_playlist.tracks.size())) {
        spdlog::warn("playNextTrack: currentIndex {} invalid after advance. Falling back to current track.", m_playlist.currentIndex);

        if (m_playlist.previousIndex >= 0 && m_playlist.previousIndex < static_cast<int>(m_playlist.tracks.size())) {
            m_playlist.currentIndex = m_playlist.previousIndex;

            if (m_track.paused) { resume(); return true; }
            return startTrackAtIndex(m_playlist.currentIndex);
        }
        return false;
    }
    return startTrackAtIndex(m_playlist.currentIndex);
}

bool MusicPlayer::playPreviousTrack() {
    if (m_playlist.tracks.empty()) { return false; }

    if (m_playlist.previousIndex < 0 || m_playlist.previousIndex >= static_cast<int>(m_playlist.tracks.size())) {
        spdlog::warn("playPreviousTrack: no valid previous track. Handling current track fallback.");

        if (m_playlist.currentIndex >= 0 && m_playlist.currentIndex < static_cast<int>(m_playlist.tracks.size())) {
            if (m_track.paused) {
                resume();
                return true;
            }
            return startTrackAtIndex(m_playlist.currentIndex);
        }
        return false;
    }

    int idx = m_playlist.previousIndex;
    m_playlist.previousIndex = m_playlist.currentIndex;
    return startTrackAtIndex(idx);
}

void MusicPlayer::advancePlaylistIndexNext() {
    if (m_playlist.tracks.empty()) {
        m_playlist.currentIndex = -1;
        m_nextIndexCached = -1;
        return;
    }

    if (m_nextIndexCached >= 0 && m_nextIndexCached < static_cast<int>(m_playlist.tracks.size())) { m_playlist.currentIndex = m_nextIndexCached; }
    else { m_playlist.currentIndex = (m_playlist.currentIndex < 0) ? 0 : computeNextIndex(); }

    m_nextIndexCached = -1;
}

int MusicPlayer::computeNextIndex() const {
    if (m_playlist.tracks.size() <= 1) { return m_playlist.currentIndex >= 0 ? m_playlist.currentIndex : 0; }

    int base = (m_playlist.currentIndex >= 0 && m_playlist.currentIndex < static_cast<int>(m_playlist.tracks.size())) ? m_playlist.currentIndex : 0;
    int next = base;
    uint32_t tracksSize = static_cast<uint32_t>(m_playlist.tracks.size());

    while (next == base) { next = static_cast<int>(const_cast<MusicPlayer*>(this)->m_rng() % tracksSize); }
    return next;
}

void MusicPlayer::preloadNextTrack() {
    if (m_playlist.tracks.size() <= 1) {
        std::lock_guard<std::mutex> lock(m_preloadMutex);
        m_hasPreloaded = false;
        m_preloadedPcm.clear();
        m_preloadedId.clear();
        m_nextIndexCached = -1;
        return;
    }

    m_nextIndexCached = computeNextIndex();
    TrackEntry t = m_playlist.tracks[m_nextIndexCached];

    m_preloadFuture = std::async(std::launch::async, [this, t]() mutable {
        std::vector<uint8_t> localPcm;
        bool ok = t.isWav ? loadWavTrackPCM(t.trackName, localPcm) : loadDATrackPCM(t.trackName, localPcm);
        if (!ok) { return; }
            {
                std::lock_guard<std::mutex> lock(this->m_preloadMutex);
                this->m_preloadedId = std::move(t.trackName);
                this->m_preloadedPcm = std::move(localPcm);
                this->m_hasPreloaded = true;
            }
        });

}

void MusicPlayer::update() {
    HWND currentHWND = GetForegroundWindow();

    if (currentHWND != m_hwnd && currentHWND != nullptr && IsWindow(currentHWND)) {
        char className[256];
        if (GetClassNameA(currentHWND, className, sizeof(className)) && std::strcmp(className, "Windows.UI.Core.CoreWindow") != 0) {
            m_hwnd = currentHWND;

            if (!m_ds) {
                init(m_hwnd);

                if (!m_track.pcm.empty() && !m_track.buffer) {
                    createBufferFromPCM(m_track);
                    if (m_track.buffer) {
                        m_track.buffer->SetCurrentPosition(m_track.playCursor);
                        if (m_track.playing) {
                            m_track.buffer->Play(0, 0, m_track.looping ? DSBPLAY_LOOPING : 0);
                        }
                    }
                }
            }
            else {
                HRESULT result = m_ds->SetCooperativeLevel(m_hwnd, DSSCL_PRIORITY);
                if (FAILED(result)) { spdlog::warn("MusicPlayer: SetCooperativeLevel failed on handoff, hr=0x{:X}", result); }
            }
        }
    }

    if (m_track.playing && m_track.buffer) {
        DWORD status = 0;
        m_track.buffer->GetStatus(&status);

        if (m_volumeAddress) {
            int gv = *m_volumeAddress;
            if (gv != m_gameVolume) {
                m_gameVolume = gv;
                m_track.buffer->SetVolume(dsVolumeFromGameVolume(m_gameVolume));
            }
        }

        if (!(status & DSBSTATUS_PLAYING)) {
            m_track.playing = false;

            if (m_playlist.tracks.empty()) return;

            if (m_playlist.currentIndex < 0 || m_playlist.currentIndex >= static_cast<int>(m_playlist.tracks.size())) {
                m_playlist.currentIndex = 0;
                startTrackAtIndex(0);
                return;
            }

            TrackEntry& t = m_playlist.tracks[m_playlist.currentIndex];
            if (t.loop) { startTrackAtIndex(m_playlist.currentIndex); }
            else if (t.autoAdvance && m_playlist.autoAdvance) { playNextTrack(); }
            return;
        }

        DWORD playPos = 0, writePos = 0;
        m_track.buffer->GetCurrentPosition(&playPos, &writePos);

        if (playPos < m_track.playCursor) {
            if (m_track.looping) {
                m_track.buffer->SetCurrentPosition(0);
                m_track.buffer->Play(0, 0, DSBPLAY_LOOPING);
            }
        }
        m_track.playCursor = playPos;
    }
}

bool MusicPlayer::readFileToBuffer(const std::string &path, std::vector<uint8_t> &outBuf) {
    std::ifstream fStream(path, std::ios::binary | std::ios::ate);
    if (!fStream) { spdlog::warn("readFileToBuffer: Failed to open {}", path); return false; }

    std::streamsize size = fStream.tellg();
    if (size <= 0) { outBuf.clear(); return true; }

    fStream.seekg(0, std::ios::beg);

    outBuf.resize(static_cast<size_t>(size));

    if (!fStream.read(reinterpret_cast<char*>(outBuf.data()), size)) {
        spdlog::warn("readFileToBuffer: Failed to read complete file data for {}", path);
        outBuf.clear();
        return false;
    }

    return true;
}

bool MusicPlayer::loadWavTrackPCM(const std::string &id, std::vector<uint8_t> &outPcm) {
    std::string rel = "music/" + id + ".wav";
    std::string path = resolveCaseInsensitive(rel);
    if (path.empty()) { spdlog::warn("loadWavTrackPCM: Unable to resolve {}", rel); return false; }

    return readFileToBuffer(path, outPcm);
}


constexpr int RAW_SECTOR_SIZE = 2352;

typedef struct CDROM_READ_AUDIO {
    LARGE_INTEGER StartingMsn;
    ULONG NumberOfBlocks;
    TRACK_MODE_TYPE TrackMode;
    PUCHAR Buffer;
} CDROM_READ_AUDIO, * PCDROM_READ_AUDIO;

bool MusicPlayer::loadDATrackPCM(const std::string &id, std::vector<uint8_t> &outPcm) {
    if (m_cdDrive.empty()) { spdlog::warn("loadDaTrackPCM: Unable to load DA music, no CD drive detected"); return false; }

    if (id.size() < 6) { spdlog::warn("loadDaTrackPCM: Invalid track ID format '{}'", id); return false; }

    int num = 0;
    std::string_view numPart(id.data() + 5, id.size() - 5);
    auto [ptr, errCode] = std::from_chars(numPart.data(), numPart.data() + numPart.size(), num);

    if (errCode != std::errc{}) { spdlog::warn("loadDaTrackPCM: Failed to parse track number from '{}'", id); return false; }

    char fileName[32];
    std::snprintf(fileName, sizeof(fileName), "TRACK%02d.DA", num);
    std::string path = m_cdDrive + fileName;

    if (readFileToBuffer(path, outPcm)) { return true; }

    /*
        The above works in most cases.
        Below is a fallback attempt to extract the audio track directly from the CD device using Windows IOCTLs, which works sometimes.

        If none of it works, we are possiby dealing with a virtual drive or with quirky hardware.
        Virtual drives tend to vary in how faithfully they emulate hardware behavior; Some will not allow raw audio extraction at all.
        Virtual disc formats also vary in how they expose data and how accurately they preserve the original CD layout/track data.
        eg. ISO destroys tracks completely, split formats (CUE + BIN or CCD + IMG) do a better job.
    */

    if (m_cdDrive.size() < 2 || m_cdDrive[1] != ':') { spdlog::trace("loadDaTrackPCM: Invalid CD drive root format: {}", m_cdDrive); return false; }

    char volumeDevicePath[16];
    std::snprintf(volumeDevicePath, sizeof(volumeDevicePath), "\\\\.\\%c:", m_cdDrive[0]);

    HANDLE hDevice = CreateFileA(volumeDevicePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hDevice == INVALID_HANDLE_VALUE) { spdlog::trace("loadDaTrackPCM: Could not open device volume handle for extraction."); return false; }

    CDROM_TOC toc = {};
    DWORD bytesReturned = 0;
    if (!DeviceIoControl(hDevice, IOCTL_CDROM_READ_TOC, nullptr, 0, &toc, sizeof(toc), &bytesReturned, nullptr)) {
        spdlog::warn("loadDaTrackPCM: Failed to read Table of Contents (TOC).");
        CloseHandle(hDevice);
        return false;
    }

    int trackIndex = num - toc.FirstTrack;
    if (trackIndex < 0 || num > toc.LastTrack) {
        spdlog::warn("loadDaTrackPCM: Track out of bounds for current media layout.");
        CloseHandle(hDevice);
        return false;
    }

    auto msfToLba = [](const UCHAR msf[4]) -> ULONG { return (msf[1] * 60 * 75) + (msf[2] * 75) + msf[3]; };

    ULONG startLba = msfToLba(toc.TrackData[trackIndex].Address);
    ULONG endLba = msfToLba(toc.TrackData[trackIndex + 1].Address);

    if (endLba <= startLba) { CloseHandle(hDevice); return false; }

    ULONG totalSectors = endLba - startLba;
    size_t totalBytes = static_cast<size_t>(totalSectors) * RAW_SECTOR_SIZE;
    outPcm.resize(totalBytes);

    RAW_READ_INFO readInfo = {};
    readInfo.DiskOffset.QuadPart = static_cast<LONGLONG>(startLba) * 2048;
    readInfo.SectorCount = totalSectors;
    readInfo.TrackMode = CDDA;

    if (!DeviceIoControl(hDevice, IOCTL_CDROM_RAW_READ, &readInfo, sizeof(readInfo), outPcm.data(), static_cast<DWORD>(totalBytes), &bytesReturned, nullptr)) {
        DWORD err = GetLastError();

        if (err == ERROR_INVALID_PARAMETER) {
            readInfo.DiskOffset.QuadPart = static_cast<LONGLONG>(startLba) * RAW_SECTOR_SIZE;

            if (DeviceIoControl(hDevice, IOCTL_CDROM_RAW_READ, &readInfo, sizeof(readInfo), outPcm.data(), static_cast<DWORD>(totalBytes), &bytesReturned, nullptr)) {
                CloseHandle(hDevice);
                return true;
            }
        }
    }
    else {
        CloseHandle(hDevice);
        return true;
    }

    CDROM_READ_AUDIO readAudio = {};
    readAudio.StartingMsn.QuadPart = static_cast<LONGLONG>(startLba);
    readAudio.NumberOfBlocks = totalSectors;
    readAudio.TrackMode = CDDA;
    readAudio.Buffer = outPcm.data();

    if (DeviceIoControl(hDevice, CTL_CODE(FILE_DEVICE_CD_ROM, 0x003E, METHOD_BUFFERED, FILE_READ_ACCESS), &readAudio, sizeof(readAudio), outPcm.data(), static_cast<DWORD>(totalBytes), &bytesReturned, nullptr)) {
        CloseHandle(hDevice);
        return true;
    }

    // Alternative attempt, modifies the byte offset request calculation directly at the device boundary.
    readAudio.StartingMsn.QuadPart = static_cast<LONGLONG>(startLba) * 2048;
    if (DeviceIoControl(hDevice, CTL_CODE(FILE_DEVICE_CD_ROM, 0x003E, METHOD_BUFFERED, FILE_READ_ACCESS), &readAudio, sizeof(readAudio), outPcm.data(), static_cast<DWORD>(totalBytes), &bytesReturned, nullptr)) {
        CloseHandle(hDevice);
        return true;
    }

    spdlog::warn("loadDaTrackPCM: Could not get audio from CD. If using a virtual drive, try a different drive emulator.");
    outPcm.clear();
    CloseHandle(hDevice);
    return false;
}

bool MusicPlayer::createBufferFromPCM(RuntimeTrack& rt) {
    if (!m_ds) { spdlog::warn("createBufferFromPCM: DirectSound device not initialized"); return false; }

    if (rt.pcm.empty()) { spdlog::warn("createBufferFromPCM: PCM buffer empty"); return false; }

    WAVEFORMATEX wfx = {};
    bool hasHeader = false;
    const uint8_t* dataPtr = rt.pcm.data();
    size_t dataSize = rt.pcm.size();

    auto read32 = [](const uint8_t* p) -> uint32_t {
        uint32_t val;
        std::memcpy(&val, p, 4);
        return val;
        };

    auto read16 = [](const uint8_t* p) -> uint16_t {
        uint16_t val;
        std::memcpy(&val, p, 2);
        return val;
        };

    if (dataSize >= 12 && dataPtr[0] == 'R' && dataPtr[1] == 'I' && dataPtr[2] == 'F' && dataPtr[3] == 'F' && dataPtr[8] == 'W' && dataPtr[9] == 'A' && dataPtr[10] == 'V' && dataPtr[11] == 'E') {
        size_t offset = 12;
        bool fmtFound = false;
        bool dataFound = false;
        size_t audioDataOffset = 0;
        size_t audioDataSize = 0;

        while (offset + 8 <= dataSize) {
            uint32_t chunkId = read32(dataPtr + offset);
            uint32_t chunkSize = read32(dataPtr + offset + 4);
            offset += 8;

            if (offset + chunkSize > dataSize) { spdlog::warn("createBufferFromPCM: Malformed WAV, chunk size exceeds file boundaries"); break; }

            if (std::memcmp(dataPtr + offset - 8, "fmt ", 4) == 0) {
                if (chunkSize >= 16) {
                    wfx.wFormatTag = read16(dataPtr + offset);
                    wfx.nChannels = read16(dataPtr + offset + 2);
                    wfx.nSamplesPerSec = read32(dataPtr + offset + 4);
                    wfx.nAvgBytesPerSec = read32(dataPtr + offset + 8);
                    wfx.nBlockAlign = read16(dataPtr + offset + 12);
                    wfx.wBitsPerSample = read16(dataPtr + offset + 14);
                    wfx.cbSize = 0;
                    fmtFound = true;
                }
            }
            else if (std::memcmp(dataPtr + offset - 8, "data", 4) == 0) {
                audioDataOffset = offset;
                audioDataSize = chunkSize;
                dataFound = true;
            }
            offset += (chunkSize + 1) & ~1;
        }

        if (fmtFound && dataFound) {
            std::vector<uint8_t> actualAudio(rt.pcm.begin() + audioDataOffset, rt.pcm.begin() + audioDataOffset + audioDataSize);
            rt.pcm.swap(actualAudio);
            hasHeader = true;
        }
        else { spdlog::warn("createBufferFromPCM: RIFF header found but missing fmt or data chunks"); }
    }

    if (!hasHeader) {
        // Fallback: Raw PCM / Redbook CDDA defaults
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = 2;
        wfx.nSamplesPerSec = 44100;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

        if (rt.pcm.size() % 4 != 0) {
            wfx.nChannels = 1;
            wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
            wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        }
    }

    DSBUFFERDESC desc = {};
    desc.dwSize = sizeof(desc);
	desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME; // | DSBCAPS_GLOBALFOCUS;
    desc.dwBufferBytes = static_cast<DWORD>(rt.pcm.size());
    desc.lpwfxFormat = &wfx;

    if (rt.buffer) {
        rt.buffer->Stop();
        rt.buffer->Release();
        rt.buffer = nullptr;
    }

    LPDIRECTSOUNDBUFFER buf = nullptr;
    HRESULT result = m_ds->CreateSoundBuffer(&desc, &buf, nullptr);
    if (FAILED(result)) { spdlog::warn("createBufferFromPCM: CreateSoundBuffer failed hr=0x{:X}", result); return false; }

    void* p1 = nullptr;
    void* p2 = nullptr;
    DWORD b1 = 0;
    DWORD b2 = 0;

    result = buf->Lock(0, desc.dwBufferBytes, &p1, &b1, &p2, &b2, 0);
    if (FAILED(result)) {
        spdlog::warn("createBufferFromPCM: Lock failed hr=0x{:X}", result);
        buf->Release();
        return false;
    }

    std::memcpy(p1, rt.pcm.data(), b1);
    if (p2 && b2) { std::memcpy(p2, rt.pcm.data() + b1, b2); }

    buf->Unlock(p1, b1, p2, b2);
    rt.buffer = buf;

    return true;
}

std::vector<std::string> MusicPlayer::getWavFilesWithPrefix(const std::string &prefix) const {
    std::vector<std::string> result;

    std::filesystem::path dir = std::filesystem::path(m_gameDir) / "music";

    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        spdlog::warn("getWavFilesWithPrefix: Directory does not exist: {}", dir.string());
        return result;
    }

    std::string upPrefix = prefix;
    std::transform(upPrefix.begin(), upPrefix.end(), upPrefix.begin(), [](unsigned char c) { return std::toupper(c); });

    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) { continue; }

        const std::filesystem::path &path = entry.path();

        if (path.extension() == ".wav" || path.extension() == ".WAV" || path.extension().string().size() == 4) {

            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
            if (ext != ".wav") continue;

            std::string stem = path.stem().string();

            if (stem.size() >= upPrefix.size()) {
                bool match = std::equal(upPrefix.begin(), upPrefix.end(), stem.begin(),
                    [](char prefChar, char stemChar) {
                        return prefChar == std::toupper(static_cast<unsigned char>(stemChar));
                    });

                if (match) { result.push_back(std::move(stem)); }
            }
        }
    }
    return result;
}

std::string MusicPlayer::resolveCaseInsensitive(const std::string &relative) const {
    std::filesystem::path relPath(relative);

    if (relPath.is_absolute()) { return relative; }

    std::filesystem::path base = std::filesystem::path(m_gameDir) / relPath;

    if (std::filesystem::exists(base)) { return base.string(); }

    std::filesystem::path parentDir = base.parent_path();
    std::string targetFilename = base.filename().string();

    std::transform(targetFilename.begin(), targetFilename.end(), targetFilename.begin(), [](unsigned char c) { return std::tolower(c); });

    if (std::filesystem::exists(parentDir) && std::filesystem::is_directory(parentDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(parentDir)) {
            std::string currentFilename = entry.path().filename().string();

            std::string currentLower = currentFilename;
            std::transform(currentLower.begin(), currentLower.end(), currentLower.begin(), [](unsigned char c) { return std::tolower(c); });

            if (currentLower == targetFilename) { return entry.path().string(); }
        }
    }

    spdlog::warn("Unable to resolve {}", base.string());
    return "";
}

bool MusicPlayer::detectCDDrive() {
    m_cdDrive.clear();
    DWORD drives = GetLogicalDrives();

    for (char letter = 'A'; letter <= 'Z'; ++letter) {
        if (!(drives & (1 << (letter - 'A')))) { continue; }

        std::string root = std::string(1, letter) + ":\\";
        if (GetDriveTypeA(root.c_str()) != DRIVE_CDROM) { continue; }
		std::string sig = root + "HORTA.MPG";
        DWORD attributes = GetFileAttributesA(sig.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) { m_cdDrive = root; return true; }
    }
    return false;
}
