// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <dsound.h>
#include <mutex>
#include <random>
#include <cstdint>
#include <filesystem>
#include <future>

class MusicPlayer {
public:
    MusicPlayer();
    ~MusicPlayer();

    bool init(HWND hwnd);
    void shutdown();
    void update();
    void stop(bool clearPlaylist);
    void pause();
    void resume();

    bool playNamed(const std::string &trackName, bool loop);

    void queueDAPlaylist(bool shuffle, bool autoAdvance, bool loop, int firstTrack, int lastTrack);
    void queueWAVPlaylist(const std::vector<std::string> &baseNames, bool shuffle = true, bool autoAdvance = true);
    void queueWAVPlaylist(bool shuffle, bool autoAdvance);

    bool playNextTrack();
    bool playPreviousTrack();

    std::vector<std::string> getWavFilesWithPrefix(const std::string &prefix) const;

    std::string resolveCaseInsensitive(const std::string &relative) const;

    std::filesystem::path detectGameDirectory();
    bool detectCDDrive();

    bool isPlaying() const { return m_track.playing; }
    bool isPaused()  const { return m_track.paused; }
	std::string getCDDrive() const { return m_cdDrive; }

    void setVolumeAddress(BYTE* addr);
    void writeGameVolumeByte(int8_t v);

private:
    struct TrackEntry {
        std::string trackName;
        bool isWav = false;
        bool loop = false;
        bool autoAdvance = true;
    };

    struct Playlist {
        std::vector<TrackEntry> tracks;
        bool shuffle = false;
        bool autoAdvance = true;
        int currentIndex = -1;
        int previousIndex = -1;
    };

    struct RuntimeTrack {
        LPDIRECTSOUNDBUFFER buffer = nullptr;
        std::vector<uint8_t> pcm;
        bool playing = false;
        bool paused = false;
        bool looping = false;
        DWORD playCursor = 0;
    };

    bool startTrackAtIndex(int index);
    void advancePlaylistIndexNext();
    int  computeNextIndex() const;

    bool loadWavTrackPCM(const std::string &id, std::vector<uint8_t> &outPcm);
    bool loadDATrackPCM(const std::string &id, std::vector<uint8_t> &outPcm);
    bool createBufferFromPCM(RuntimeTrack &rt);
    bool readFileToBuffer(const std::string &path, std::vector<uint8_t> &outBuf);
    void preloadNextTrack();
    
    std::future<void> m_preloadFuture;
    std::mutex m_preloadMutex;
    
    LONG dsVolumeFromGameVolume(int volume);

    HWND m_hwnd = nullptr;
    bool m_needReinit = false;

    Playlist     m_playlist;
    RuntimeTrack m_track;

    std::string          m_preloadedId;
    std::vector<uint8_t> m_preloadedPcm;
    bool                 m_hasPreloaded = false;
    int                  m_nextIndexCached = -1;

    std::filesystem::path m_gameDir;
    std::string m_cdDrive;

    LPDIRECTSOUND m_ds = nullptr;

    std::mt19937 m_rng { std::random_device{}() };

    int m_gameVolume = 8;
    BYTE* m_volumeAddress = nullptr;
};
