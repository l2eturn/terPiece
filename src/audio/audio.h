#pragma once
// Background music + synthesized sound effects — port of audio.js (WebAudio tones -> a small miniaudio mixer)
#include <memory>
#include <string>

class Audio {
public:
    Audio();
    ~Audio();
    // start the device and the music (the web waits for the first click)
    void unlock();
    // returns the new muted state
    bool toggleMute();
    bool muted() const;

    void step();
    void jump();
    void land();
    void chime();
    void pop();
    void talk();
    void fanfare();
    void boom();
    // combat (new — not in audio.js)
    void swing();
    void hit(bool crit);
    void hurt();
    void slay();
    void levelUp();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
