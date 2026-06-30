/*
 ** graphics.cpp
 **
 ** This file is part of mkxp.
 **
 ** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
 **
 ** mkxp is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** mkxp is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "graphics.h"

#include "alstream.h"
#include "audio.h"
#include "binding.h"
#include "bitmap.h"
#include "config.h"
#include "debugwriter.h"
#include "disposable.h"
#include "etc.h"
#include "etc-internal.h"
#include "eventthread.h"
#include "filesystem.h"
#include "gl-fun.h"
#include "gl-util.h"
#include "glstate.h"
#include "intrulist.h"
#include "quad.h"
#include "scene.h"
#include "shader.h"
#include "sharedstate.h"
#include "texpool.h"
#include "util.h"
#include "input.h"
#include "sprite.h"

#ifdef MKXPZ_RETRO
#  include <memory>
#  include "core.h"
#  include "mkxp-polyfill.h"
#  include "sandbox-serial-util.h"
#else
#  include <SDL.h>
#  include <SDL_image.h>
#  include <SDL_timer.h>
#  include <SDL_video.h>
#  include <SDL_mutex.h>
#  include <SDL_thread.h>
#endif // MKXPZ_RETRO

#ifdef MKXPZ_STEAM
#include "steamshim_child.h"
#endif

#include "theoraplay.h"

#include <algorithm>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <cmath>
#include <climits>


#define DEF_SCREEN_W (rgssVer == 1 ? 640 : 544)
#define DEF_SCREEN_H (rgssVer == 1 ? 480 : 416)

#define DEF_FRAMERATE (rgssVer == 1 ? 40 : 60)

#define DEF_MAX_VIDEO_FRAMES 30
#define VIDEO_DELAY 10
#define MOVIE_AUDIO_BUFFER_SIZE 2048
#define BUFFER_LEN_MS 200

#define GUARD_V(value, expression) do { expression; if (exception.is_error()) return value; } while (0)
#define GUARD(expression) GUARD_V(, expression)

#ifdef MKXPZ_RETRO
#  define scSize scRes
#  define winSize scRes
#endif // MKXPZ_RETRO

typedef struct AudioQueue
{
    const THEORAPLAY_AudioPacket *audio;
    int offset;
    struct AudioQueue *next;
} AudioQueue;


static long readMovie(THEORAPLAY_Io *io, void *buf, long buflen)
{
#ifdef MKXPZ_RETRO
    std::shared_ptr<struct FileSystem::File> *f = (std::shared_ptr<struct FileSystem::File> *) io->userdata;
    return (long) PHYSFS_readBytes((*f)->get_read(), buf, buflen);
#else
    SDL_RWops *f = (SDL_RWops *) io->userdata;
    return (long) SDL_RWread(f, buf, 1, buflen);
#endif // MKXPZ_RETRO
} // IoFopenRead


static void closeMovie(THEORAPLAY_Io *io)
{
#ifndef MKXPZ_RETRO
    SDL_RWops *f = (SDL_RWops *) io->userdata;
    SDL_RWclose(f);
#endif // MKXPZ_RETRO
    free(io);
} // IoFopenClose


static void movieSleep(uint32_t milliseconds)
{
#ifdef MKXPZ_RETRO
    mkxp_sleep_ms(milliseconds);
#else
    SDL_Delay(milliseconds);
#endif // MKXPZ_RETRO
}


static uint64_t movieTicks()
{
#ifdef MKXPZ_RETRO
    return mkxp_retro::get_ticks_ms();
#else
    return SDL_GetTicks64();
#endif // MKXPZ_RETRO
}


#ifdef MKXPZ_RETRO
template<class C, void (C::*func)()>
static void movieThreadFun(C *obj)
{
	(obj->*func)();
}
#endif // MKXPZ_RETRO


struct Movie
{
    THEORAPLAY_Decoder *decoder;
    const THEORAPLAY_AudioPacket *audio;
    const THEORAPLAY_VideoFrame *video;
    bool hasVideo;
    bool hasAudio;
    bool skippable;
    Bitmap *videoBitmap;
#ifdef MKXPZ_RETRO
    std::shared_ptr<struct FileSystem::File> srcOps;
#else
    SDL_RWops srcOps;
    SDL_Thread *audioThread;
#endif // MKXPZ_RETRO
    AtomicFlag audioThreadTermReq;
    volatile AudioQueue *audioQueueHead;
    volatile AudioQueue *audioQueueTail;
    ALuint audioSource;
    ALuint alBuffers[STREAM_BUFS];
    ALshort audioBuffer[MOVIE_AUDIO_BUFFER_SIZE];
    AudioMutex audioMutex;
    Sprite *movieSprite;
    Sprite *letterboxSprite;
    Bitmap *letterbox;
    uint64_t baseTicks;
    uint64_t currentTicks;
    float volume;
    bool openedAudio;

#ifdef MKXPZ_RETRO
    bool dupeFrame;
#endif // MKXPZ_RETRO

    // Variables used by streamMovieAudioProc
    ALint streamMovieAudioState;
    ALint procBufs;
    volatile AudioQueue *audioPacketAndOffset;
    int channels;
    int sampleRate;
    float *sourceSamples;
    ALuint samplesToProcess;
    ALshort *sampleBuffer;
    ALuint remainingSamples;

    Movie(float volume, bool skippable_)
    : decoder(0), audio(0), video(0), hasVideo(false), hasAudio(false), skippable(skippable_), videoBitmap(0),
#ifndef MKXPZ_RETRO
        audioThread(0),
#endif // MKXPZ_RETRO
        movieSprite(nullptr), letterboxSprite(nullptr), letterbox(nullptr), baseTicks(-1), currentTicks(0), volume(volume), openedAudio(false),
#ifdef MKXPZ_RETRO
        dupeFrame(true),
#endif // MKXPZ_RETRO
        streamMovieAudioState(0), procBufs(STREAM_BUFS)
    {
        audioThreadTermReq.set();
    }

    bool preparePlayback()
    {
        
        // https://theora.org/doc/libtheora-1.0/codec_8h.html
        // https://ffmpeg.org/doxygen/0.11/group__lavc__misc__pixfmt.html
        THEORAPLAY_Io *io = (THEORAPLAY_Io *) malloc(sizeof (THEORAPLAY_Io));
        if(!io) {
#ifndef MKXPZ_RETRO
            SDL_RWclose(&srcOps);
#endif // MKXPZ_RETRO
            return false;
        }
        
        io->read = readMovie;
        io->close = closeMovie;
        io->userdata = &srcOps;
#ifdef MKXPZ_NO_THREAD
        decoder = THEORAPLAY_startDecode(io, DEF_MAX_VIDEO_FRAMES, THEORAPLAY_VIDFMT_RGBA, nullptr, false);
#else
        decoder = THEORAPLAY_startDecode(io, DEF_MAX_VIDEO_FRAMES, THEORAPLAY_VIDFMT_RGBA, nullptr, true);
#endif // MKXPZ_NO_THREAD
        if (!decoder) {
#ifndef MKXPZ_RETRO
            SDL_RWclose(&srcOps);
#endif // MKXPZ_RETRO
            return false;
        }
        
        // Wait until the decoder has parsed out some basic truths from the file.
        while (!THEORAPLAY_isInitialized(decoder)) {
#ifdef MKXPZ_NO_THREAD
            THEORAPLAY_pumpDecode(decoder, DEF_MAX_VIDEO_FRAMES);
#else
            movieSleep(VIDEO_DELAY);
#endif // MKXPZ_NO_THREAD
        }
        
        // Once we're initialized, we can tell if this file has audio and/or video.
        hasAudio = THEORAPLAY_hasAudioStream(decoder);
        hasVideo = THEORAPLAY_hasVideoStream(decoder);
        
        // Queue up the audio
        if (hasAudio) {
            while ((audio = THEORAPLAY_getAudio(decoder)) == NULL) {
                if ((THEORAPLAY_availableVideo(decoder) >= DEF_MAX_VIDEO_FRAMES)) {
                    break;  // we'll never progress, there's no audio yet but we've prebuffered as much as we plan to.
                }
#ifdef MKXPZ_NO_THREAD
                THEORAPLAY_pumpDecode(decoder, DEF_MAX_VIDEO_FRAMES);
#else
                movieSleep(VIDEO_DELAY);
#endif // MKXPZ_NO_THREAD
            }
        }
        
        // No video, so no point in doing anything else
        if (!hasVideo) {
            THEORAPLAY_stopDecode(decoder);
            return false;
        }
        
        // Wait until we have video
        while ((video = THEORAPLAY_getVideo(decoder)) == NULL) {
#ifdef MKXPZ_NO_THREAD
            THEORAPLAY_pumpDecode(decoder, DEF_MAX_VIDEO_FRAMES);
#else
            movieSleep(VIDEO_DELAY);
#endif // MKXPZ_NO_THREAD
        }
        
        // Wait until we have audio, if applicable
        audio = NULL;
        if (hasAudio) {
            while ((audio = THEORAPLAY_getAudio(decoder)) == NULL && THEORAPLAY_availableVideo(decoder) < DEF_MAX_VIDEO_FRAMES) {
#ifdef MKXPZ_NO_THREAD
                THEORAPLAY_pumpDecode(decoder, DEF_MAX_VIDEO_FRAMES);
#else
                movieSleep(VIDEO_DELAY);
#endif // MKXPZ_NO_THREAD
            }
        }
        // Create this Bitmap without a hires replacement, because we don't
        // support hires replacement for Movies yet.
        Exception exception;
        videoBitmap = new Bitmap(exception, video->width, video->height, true, false);
        if (exception.is_error()) {
            delete videoBitmap;
            videoBitmap = NULL;
            return false;
        }
        audioQueueHead = NULL;
        audioQueueTail = NULL;
        
        return true;
    }
    
    void queueAudioPacket(const THEORAPLAY_AudioPacket *audio) {
        AudioQueue *item = NULL;
        
        if (!audio) {
            return;
        }
        
        item = (AudioQueue *) malloc(sizeof (AudioQueue));
        if (!item) {
            THEORAPLAY_freeAudio(audio);
            return;  // oh well.
        }
        
        item->audio = audio;
        item->offset = 0;
        item->next = NULL;
        
        AudioMutexGuard guard(audioMutex);
        if (audioQueueTail) {
            audioQueueTail->next = item;
        } else {
            audioQueueHead = item;
        }
        audioQueueTail = item;
    }
    
    void bufferMovieAudio(THEORAPLAY_Decoder *decoder, const uint64_t now) {
        const THEORAPLAY_AudioPacket *audio;
        while ((audio = THEORAPLAY_getAudio(decoder)) != NULL) {
            queueAudioPacket(audio);
            if (audio->playms >= now + BUFFER_LEN_MS) {  // don't let this get too far ahead.
                break;
            }
        }
    }

    bool streamMovieAudioProc() {
        // Quit if audio thread terminate request has been made
        if (audioThreadTermReq) return false;

        // Periodically check the buffers until one is available
        if (procBufs <= 0) {
            alGetSourcei(audioSource, AL_BUFFERS_PROCESSED, &procBufs);
            if(procBufs > 0) {
                alSourceUnqueueBuffers(audioSource, procBufs, alBuffers);
            } else {
#ifndef MKXPZ_RETRO
                movieSleep(AUDIO_SLEEP);
#endif // MKXPZ_RETRO
            }
        }

        while(procBufs > 0) {
            --procBufs;

            remainingSamples = MOVIE_AUDIO_BUFFER_SIZE;
            sampleBuffer = audioBuffer;

            {
                AudioMutexGuard guard(audioMutex);

                while(audioQueueHead && (remainingSamples > 0)) {
                    audioPacketAndOffset = audioQueueHead;

                    // Skip packets to catch up
                    if (currentTicks >= audioPacketAndOffset->audio->playms + BUFFER_LEN_MS) {
                        audioQueueHead = audioPacketAndOffset->next;
                        THEORAPLAY_freeAudio(audioPacketAndOffset->audio);
                        free((void *) audioPacketAndOffset);
                        continue;
                    }

                    channels = audioPacketAndOffset->audio->channels;
                    sampleRate = audioPacketAndOffset->audio->freq;
                    sourceSamples = audioPacketAndOffset->audio->samples + (audioPacketAndOffset->offset * channels);
                    samplesToProcess = (audioPacketAndOffset->audio->frames - audioPacketAndOffset->offset) * channels;

                    if (samplesToProcess > remainingSamples) samplesToProcess = remainingSamples;

                    for (ALuint i = 0; i < samplesToProcess; i++) {
                        const float val = (*(sourceSamples++));
                        if (val < -1.0f) {
                            *(sampleBuffer++) = SHRT_MIN;
                        } else if (val > 1.0f) {
                            *(sampleBuffer++) = SHRT_MAX;
                        } else {
                            *(sampleBuffer++) = (ALshort) (val * SHRT_MAX);
                        }
                    }

                    // Necessary to remember position between repeated iterations
                    audioPacketAndOffset->offset += (samplesToProcess / channels);
                    remainingSamples -= samplesToProcess;

                    // The current audio packet has been completed
                    if ((audioPacketAndOffset->offset) >= audioPacketAndOffset->audio->frames) {
                        audioQueueHead = audioPacketAndOffset->next;
                        THEORAPLAY_freeAudio(audioPacketAndOffset->audio);
                        free((void *) audioPacketAndOffset);
                    }
                }

                if(!audioQueueHead) audioQueueTail = NULL;
            }

            alBufferData(alBuffers[procBufs], channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16, audioBuffer,
                (MOVIE_AUDIO_BUFFER_SIZE - remainingSamples) * sizeof(ALshort), sampleRate);
            alSourceQueueBuffers(audioSource, 1, &alBuffers[procBufs]);     
            alGetSourcei(audioSource, AL_SOURCE_STATE, &streamMovieAudioState);
            if(streamMovieAudioState != AL_PLAYING) alSourcePlay(audioSource);
        }

        return true;
    }

    void streamMovieAudio() {
        while(streamMovieAudioProc());
    }
    
    bool startAudio(float volume)
    {
        alGenSources(1, &audioSource);
        alGenBuffers(STREAM_BUFS, alBuffers);
        alSourcef(audioSource, AL_GAIN, volume);

        audioThreadTermReq.clear();
        queueAudioPacket(audio);
        audio = NULL;
        bufferMovieAudio(decoder, 0);
#ifndef MKXPZ_RETRO
        audioThread = createSDLThread <Movie, &Movie::streamMovieAudio>(this, "movieaudio");
#endif // MKXPZ_RETRO

        return true;
    }
    
    bool play()
    {
#ifdef MKXPZ_RETRO
        dupeFrame = true;
#endif // MKXPZ_RETRO

        if (baseTicks == (uint64_t)-1) {
            baseTicks = movieTicks();
        }

        while (THEORAPLAY_isDecoding(decoder)) {
            // Check for reset/shutdown input
            if(shState->graphics().updateMovieInput(this)) break;
            
            // Check for attempted skip
            if (skippable) {
#ifdef MKXPZ_RETRO // TODO: move into shState
                mkxp_retro::input->update();
                if  (mkxp_retro::input->isTriggered(Input::C) || mkxp_retro::input->isTriggered(Input::B)) break;
#else
                shState->input().update();
                if  (shState->input().isTriggered(Input::C) || shState->input().isTriggered(Input::B)) break;
#endif // MKXPZ_RETRO
            }

#ifdef MKXPZ_NO_THREAD
            THEORAPLAY_pumpDecode(decoder, DEF_MAX_VIDEO_FRAMES);
#endif // MKXPZ_NO_THREAD

            uint64_t now = movieTicks() - baseTicks;
            
            if (!video) {
                video = THEORAPLAY_getVideo(decoder);
            }
            
            if (hasAudio) {
                if (!audio) {
                    audio = THEORAPLAY_getAudio(decoder);
                }
                
                if (audio && !openedAudio) {
                    if(!startAudio(volume)){
                        Debug() << "Error opening movie audio!";
                        break;
                    }
                    openedAudio = true;
                }
                
            }
            
            if (video && (video->playms <= now)) {
                if (now >= video->playms + BUFFER_LEN_MS)
                {
                    // Skip frames to catch up
                    const THEORAPLAY_VideoFrame *last = video;
                    while ((video = THEORAPLAY_getVideo(decoder)) != NULL)
                    {
                        THEORAPLAY_freeVideo(last);
                        last = video;
                        if (now < video->playms + BUFFER_LEN_MS)
                            break;
                    } 

                    if (!video)
                        video = last;

                    baseTicks = movieTicks() - video->playms;
                }

                // Got a video frame, now draw it
                {
                    // Ignore errors
                    Exception e;
                    videoBitmap->replaceRaw(e, video->pixels, video->width * video->height * 4);
                }
                {
                    // Ignore errors
                    Exception e;
                    shState->graphics().update(e, false);
                }
                {
                    AudioMutexGuard guard(audioMutex);
                    currentTicks = video->playms;
                }
                THEORAPLAY_freeVideo(video);
                video = NULL;
#ifdef MKXPZ_RETRO
                dupeFrame = false;
#endif // MKXPZ_RETRO
            } else {
#ifndef MKXPZ_RETRO
                // Next video frame not yet ready, let the CPU breathe
                movieSleep(VIDEO_DELAY);
#endif // MKXPZ_RETRO
            }
            
            if (openedAudio) {
                bufferMovieAudio(decoder, currentTicks);
            }

#ifdef MKXPZ_RETRO
            return true;
#endif // MKXPZ_RETRO
        }

        return false;
    }
    
    ~Movie()
    {
        if (letterbox) delete letterbox;
        if (letterboxSprite) delete letterboxSprite;
        if (movieSprite) delete movieSprite;
        if (hasAudio) {
            audioThreadTermReq.set();
#ifdef MKXPZ_RETRO
            {
                AudioMutexGuard guard(audioMutex);
            }
#else
            if(audioThread) {
                SDL_WaitThread(audioThread, 0);
                audioThread = 0;
            }
#endif // MKXPZ_RETRO

            if (audioQueueTail) {
                THEORAPLAY_freeAudio(audioQueueTail->audio);
            }
            audioQueueTail = NULL;
            
            if (audioQueueHead) {
                THEORAPLAY_freeAudio(audioQueueHead->audio);
            }
            audioQueueHead = NULL;
            alSourceStop(audioSource);
            alDeleteSources(1, &audioSource);
            alDeleteBuffers(STREAM_BUFS, alBuffers);
        }
        if (video) THEORAPLAY_freeVideo(video);
        if (audio) THEORAPLAY_freeAudio(audio);
        if (decoder) THEORAPLAY_stopDecode(decoder);
        delete videoBitmap;
    }

#ifdef MKXPZ_RETRO
    bool sandbox_serialize(void *&data, mkxp_sandbox::wasm_size_t &max_size) const;
#endif // MKXPZ_RETRO
};

struct MovieOpenHandler : FileSystem::OpenHandler
{
#ifdef MKXPZ_RETRO
    std::shared_ptr<struct FileSystem::File> *srcOps;
#else
    SDL_RWops *srcOps;
#endif // MKXPZ_RETRO
    
#ifdef MKXPZ_RETRO
    MovieOpenHandler(std::shared_ptr<struct FileSystem::File> &srcOps)
#else
    MovieOpenHandler(SDL_RWops &srcOps)
#endif // MKXPZ_RETRO
    :   srcOps(&srcOps)
    {}
    
#ifdef MKXPZ_RETRO
    bool tryRead(std::shared_ptr<struct FileSystem::File> ops, const char *ext)
#else
    bool tryRead(SDL_RWops &ops, const char *ext)
#endif // MKXPZ_RETRO
    {
        *srcOps = ops;
        return true;
    }
};

struct PingPong {
    TEXFBO rt[2];
    uint8_t srcInd, dstInd;
    int screenW, screenH;
    
    PingPong(int screenW, int screenH)
    : srcInd(0), dstInd(1), screenW(screenW), screenH(screenH) {
        reinit();
    }
    
    ~PingPong() {
        for (int i = 0; i < 2; ++i)
            TEXFBO::fini(rt[i]);
    }

    void reinit() {
        for (int i = 0; i < 2; ++i) {
            TEXFBO::init(rt[i]);
            TEXFBO::allocEmpty(rt[i], screenW, screenH);
            TEXFBO::linkFBO(rt[i]);
            gl.ClearColor(0, 0, 0, 1);
            FBO::clear();
        }
    }

    TEXFBO &backBuffer() { return rt[srcInd]; }
    
    TEXFBO &frontBuffer() { return rt[dstInd]; }
    
    /* Better not call this during render cycles */
    void resize(int width, int height) {
        screenW = width;
        screenH = height;
        
        for (int i = 0; i < 2; ++i)
            TEXFBO::allocEmpty(rt[i], width, height);
    }
    
    void startRender() { bind(); }
    
    void swapRender() {
        std::swap(srcInd, dstInd);
        
        bind();
    }
    
    void clearBuffers() {
        glState.clearColor.pushSet(Vec4(0, 0, 0, 1));
        
        for (int i = 0; i < 2; ++i) {
            FBO::bind(rt[i].fbo);
            FBO::clear();
        }
        
        glState.clearColor.pop();
    }
    
private:
    void bind() { FBO::bind(rt[dstInd].fbo); }
};

class ScreenScene : public Scene {
    friend class Graphics;

public:
    ScreenScene(int width, int height) : pp(width, height) {
        updateReso(width, height);
        
        brightEffect = false;
        brightnessQuad.setColor(Vec4());
    }
    
    void composite(Exception &exception) {
        const int w = geometry.rect.w;
        const int h = geometry.rect.h;
        
        shState->prepareDraw();
        
        pp.startRender();
        
        glState.viewport.set(IntRect(0, 0, w, h));
        
        FBO::clear();
        
        GUARD(Scene::composite(exception));
        
        if (brightEffect) {
            SimpleColorShader &shader = shState->shaders().simpleColor;
            shader.bind();
            shader.applyViewportProj();
            shader.setTranslation(Vec2i());
            
            brightnessQuad.draw();
        }
    }
    
    void requestViewportRender(const Vec4 &c, const Vec4 &f, const Vec4 &t) {
        const IntRect &viewpRect = glState.scissorBox.get();
        const IntRect &screenRect = geometry.rect;
        
        const bool toneRGBEffect = t.xyzNotNull();
        const bool toneGrayEffect = t.w != 0;
        const bool colorEffect = c.w > 0;
        const bool flashEffect = f.w > 0;
        
        if (toneGrayEffect) {
            pp.swapRender();
            
            if (!viewpRect.encloses(screenRect)) {
                /* Scissor test _does_ affect FBO blit operations,
                 * and since we're inside the draw cycle, it will
                 * be turned on, so turn it off temporarily */
                glState.scissorTest.pushSet(false);
                
                int scaleIsSpecial = GLMeta::blitScaleIsSpecial(pp.frontBuffer(), false, geometry.rect, pp.backBuffer(), geometry.rect);

                GLMeta::blitBegin(pp.frontBuffer(), false, scaleIsSpecial);
                GLMeta::blitSource(pp.backBuffer(), scaleIsSpecial);
                GLMeta::blitRectangle(geometry.rect, Vec2i());
                GLMeta::blitEnd();
                
                glState.scissorTest.pop();
            }
            
            GrayShader &shader = shState->shaders().gray;
            shader.bind();
            shader.setGray(t.w);
            shader.applyViewportProj();
            shader.setTexSize(screenRect.size());
            
            TEX::bind(pp.backBuffer().tex);
            
            glState.blend.pushSet(false);
            screenQuad.draw();
            glState.blend.pop();
        }
        
        if (!toneRGBEffect && !colorEffect && !flashEffect)
            return;
        
        FlatColorShader &shader = shState->shaders().flatColor;
        shader.bind();
        shader.applyViewportProj();
        
        if (toneRGBEffect) {
            /* First split up additive / substractive components */
            Vec4 add, sub;
            
            if (t.x > 0)
                add.x = t.x;
            if (t.y > 0)
                add.y = t.y;
            if (t.z > 0)
                add.z = t.z;
            
            if (t.x < 0)
                sub.x = -t.x;
            if (t.y < 0)
                sub.y = -t.y;
            if (t.z < 0)
                sub.z = -t.z;
            
            /* Then apply them using hardware blending */
            gl.BlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
            
            if (add.xyzNotNull()) {
                gl.BlendEquation(GL_FUNC_ADD);
                shader.setColor(add);
                
                screenQuad.draw();
            }
            
            if (sub.xyzNotNull()) {
                gl.BlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                shader.setColor(sub);
                
                screenQuad.draw();
            }
        }
        
        if (colorEffect || flashEffect) {
            gl.BlendEquation(GL_FUNC_ADD);
            gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO,
                                 GL_ONE);
        }
        
        if (colorEffect) {
            shader.setColor(c);
            screenQuad.draw();
        }
        
        if (flashEffect) {
            shader.setColor(f);
            screenQuad.draw();
        }
        
        glState.blendMode.refresh();
    }
    
    void setBrightness(float norm) {
        brightnessQuad.setColor(Vec4(0, 0, 0, 1.0f - norm));
        
        brightEffect = norm < 1.0f;
    }
    
    void updateReso(int width, int height) {
        geometry.rect.w = width;
        geometry.rect.h = height;
        
        screenQuad.setTexPosRect(geometry.rect, geometry.rect);
        brightnessQuad.setTexPosRect(geometry.rect, geometry.rect);
        
        notifyGeometryChange();
    }
    
    void setResolution(int width, int height) {
        pp.resize(width, height);
        updateReso(width, height);
    }
    
    PingPong &getPP() { return pp; }
    
private:
    PingPong pp;
    Quad screenQuad;
    
    Quad brightnessQuad;
    bool brightEffect;
};

/* Nanoseconds per second */
#define NS_PER_S 1000000000

#ifndef MKXPZ_RETRO
struct FPSLimiter {
    uint64_t lastTickCount;
    
    /* ticks per frame */
    int64_t tpf;
    
    /* Ticks per second */
    const uint64_t tickFreq;
    
    /* Ticks per milisecond */
    const uint64_t tickFreqMS;
    
    /* Ticks per nanosecond */
    const double tickFreqNS;
    
    bool disabled;
    
    /* Data for frame timing adjustment */
    struct {
        /* Last tick count */
        uint64_t last;
        
        /* How far behind/in front we are for ideal frame timing */
        int64_t idealDiff;
        
        bool resetFlag;
    } adj;
    
    FPSLimiter(uint16_t desiredFPS)
    : lastTickCount(SDL_GetPerformanceCounter()),
    tickFreq(SDL_GetPerformanceFrequency()), tickFreqMS(tickFreq / 1000),
    tickFreqNS((double)tickFreq / NS_PER_S), disabled(false) {
        setDesiredFPS(desiredFPS);
        
        adj.last = SDL_GetPerformanceCounter();
        adj.idealDiff = 0;
        adj.resetFlag = false;
    }
    
    void setDesiredFPS(uint16_t value) { tpf = tickFreq / value; }
    
    void delay() {
        if (disabled)
            return;
        
        int64_t tickDelta = SDL_GetPerformanceCounter() - lastTickCount;
        int64_t toDelay = tpf - tickDelta;
        
        /* Compensate for the last delta
         * to the ideal timestep */
        toDelay -= adj.idealDiff;
        
        if (toDelay < 0)
            toDelay = 0;
        
        delayTicks(toDelay);
        
        uint64_t now = lastTickCount = SDL_GetPerformanceCounter();
        int64_t diff = now - adj.last;
        adj.last = now;
        
        /* Recalculate our temporal position
         * relative to the ideal timestep */
        adj.idealDiff = diff - tpf + adj.idealDiff;
        
        if (adj.resetFlag) {
            adj.idealDiff = 0;
            adj.resetFlag = false;
        }
    }
    
    void resetFrameAdjust() { adj.resetFlag = true; }
    
    /* If we're more than a full frame's worth
     * of ticks behind the ideal timestep,
     * there's no choice but to skip frame(s)
     * to catch up */
    bool frameSkipRequired() const {
        if (disabled)
            return false;
        
        return adj.idealDiff > tpf;
    }
    
private:
    void delayTicks(uint64_t ticks) {
#if defined(HAVE_NANOSLEEP)
        struct timespec req;
        uint64_t nsec = ticks / tickFreqNS;
        req.tv_sec = nsec / NS_PER_S;
        req.tv_nsec = nsec % NS_PER_S;
        errno = 0;
        
        while (nanosleep(&req, &req) == -1) {
            int err = errno;
            errno = 0;
            
            if (err == EINTR)
                continue;
            
            Debug() << "nanosleep failed. errno:" << err;
            SDL_Delay(ticks / tickFreqMS);
            break;
        }
#else
        SDL_Delay(ticks / tickFreqMS);
#endif
    }
};
#endif // MKXPZ_RETRO

struct GraphicsPrivate {
    /* Screen resolution, ie. the resolution at which
     * RGSS renders at (settable with Graphics.resize_screen).
     * Can only be changed from within RGSS */
    Vec2i scRes;
    Vec2i scResLores;
    
#ifndef MKXPZ_RETRO
    /* Screen size, to which the rendered frames are scaled up.
     * This can be smaller than the window size when fixed aspect
     * ratio is enforced */
    Vec2i scSize;
    
    /* Actual physical size of the game window */
    Vec2i winSize;
#endif // MKXPZ_RETRO
    
    /* Offset in the game window at which the scaled game screen
     * is blitted inside the game window */
    Vec2i scOffset;
    
    // Scaling factor, used to display the screen properly
    // on Retina displays
    int scalingFactor;
    
    ScreenScene screen;
    RGSSThreadData *threadData;
#ifndef MKXPZ_RETRO
    SDL_GLContext glCtx;
#endif // MKXPZ_RETRO
    
    int frameRate;
    int frameCount;
    int brightness;
    
    double last_update;
    
    
#ifndef MKXPZ_RETRO
    FPSLimiter fpsLimiter;
#endif // MKXPZ_RETRO
    
    // Can be set from Ruby. Takes priority over config setting.
    bool useFrameSkip;
    
    bool frozen;
    TEXFBO frozenScene;
    Quad screenQuad;
    
#ifndef MKXPZ_RETRO
    float backingScaleFactor;
#endif // MKXPZ_RETRO
    
    Vec2i integerScaleFactor;
#ifndef MKXPZ_RETRO
    TEXFBO integerScaleBuffer;
#endif // MKXPZ_RETRO
    bool integerScaleActive;
    bool integerLastMileScaling;
    
    std::vector<double> avgFPSData;
    double last_avg_update;
#ifndef MKXPZ_RETRO
    SDL_mutex *avgFPSLock;
    
    SDL_mutex *glResourceLock;
#endif // MKXPZ_RETRO
    bool multithreadedMode;
    
    /* Global list of all live Disposables
     * (disposed on reset) */
    IntruList<Disposable> dispList;

    GraphicsPrivate(RGSSThreadData *rtData)
    : scResLores(DEF_SCREEN_W, DEF_SCREEN_H),
    scRes(rtData->config.enableHires ? (int)lround(rtData->config.framebufferScalingFactor * DEF_SCREEN_W) : DEF_SCREEN_W,
        rtData->config.enableHires ? (int)lround(rtData->config.framebufferScalingFactor * DEF_SCREEN_H) : DEF_SCREEN_H),
#ifndef MKXPZ_RETRO
    scSize(scRes),
    winSize(rtData->config.defScreenW, rtData->config.defScreenH),
#endif // MKXPZ_RETRO
    screen(scRes.x, scRes.y), threadData(rtData),
#ifndef MKXPZ_RETRO
    glCtx(SDL_GL_GetCurrentContext()),
#endif // MKXPZ_RETRO
    multithreadedMode(true),
    frameRate(DEF_FRAMERATE), frameCount(0), brightness(255),
#ifndef MKXPZ_RETRO
    fpsLimiter(frameRate),
#endif // MKXPZ_RETRO
    useFrameSkip(rtData->config.frameSkip),
    frozen(false),
    last_update(0), last_avg_update(0),
#ifndef MKXPZ_RETRO
    backingScaleFactor(1),
#endif // MKXPZ_RETRO
    integerScaleFactor(0, 0),
    integerScaleActive(rtData->config.integerScaling.active),
    integerLastMileScaling(rtData->config.integerScaling.lastMileScaling)
{
        avgFPSData = std::vector<double>();
#ifndef MKXPZ_RETRO
        avgFPSLock = SDL_CreateMutex();
        glResourceLock = SDL_CreateMutex();
#endif // MKXPZ_RETRO
        
        if (integerScaleActive) {
            rebuildIntegerScaleBuffer();
        }
        
        recalculateScreenSize(rtData->config.fixedAspectRatio);
        updateScreenResoRatio(rtData);
        
        reinit();
        
        FloatRect screenRect(0, 0, scRes.x, scRes.y);
        screenQuad.setTexPosRect(screenRect, screenRect);
        
#ifndef MKXPZ_RETRO
        fpsLimiter.resetFrameAdjust();
#endif // MKXPZ_RETRO
    }
    
    ~GraphicsPrivate() {
        TEXFBO::fini(frozenScene);
#ifndef MKXPZ_RETRO
        TEXFBO::fini(integerScaleBuffer);
        SDL_DestroyMutex(avgFPSLock);
        SDL_DestroyMutex(glResourceLock);
#endif // MKXPZ_RETRO
    }

    void reinit() {
        TEXFBO::init(frozenScene);
        TEXFBO::allocEmpty(frozenScene, scRes.x, scRes.y);
        TEXFBO::linkFBO(frozenScene);
    }

    void updateScreenResoRatio(RGSSThreadData *rtData) {
        Vec2 &ratio = rtData->sizeResoRatio;

#ifdef MKXPZ_RETRO
        const float factor = 1.0f;
#else
        const float factor = backingScaleFactor;
#endif // MKXPZ_RETRO

        ratio.x = (float)scRes.x / scSize.x * factor;
        ratio.y = (float)scRes.y / scSize.y * factor;
        
        rtData->screenOffset = scOffset / factor;
    }
    
    /* Enforces fixed aspect ratio, if desired */
    void recalculateScreenSize(bool fixedAspectRatio) {
        scSize = winSize;
        
        if (!fixedAspectRatio) {
#ifndef MKXPZ_RETRO
            if (!integerScaleActive || (integerScaleActive && integerLastMileScaling))
#endif // MKXPZ_RETRO
            {
                scOffset = Vec2i(0, 0);
                return;
            }
        }
        
#ifndef MKXPZ_RETRO
        if (integerScaleActive && !integerLastMileScaling) {
            scOffset.x = ((winSize.x / 2) - (scRes.x / 2) * integerScaleFactor.x);
            scOffset.y = ((winSize.y / 2) - (scRes.y / 2) * integerScaleFactor.y);
            
            scSize = Vec2i(scRes.x * integerScaleFactor.x, scRes.y * integerScaleFactor.y);
            return;
        }
#endif // MKXPZ_RETRO
        
        float resRatio = (float)scRes.x / scRes.y;
        float winRatio = (float)winSize.x / winSize.y;
        
        if (resRatio > winRatio)
            scSize.y = scSize.x / resRatio;
        else if (resRatio < winRatio)
            scSize.x = scSize.y * resRatio;
        
        scOffset.x = (winSize.x - scSize.x) / 2.f;
        scOffset.y = (winSize.y - scSize.y) / 2.f;
    }
    
    static int findHighestFittingScale(int base, int target) {
        int scale = 1;
        
        while (base * scale <= target)
            scale++;
        
        return std::max(scale - 1, 1);
    }
    
    /* Returns whether a new scale was found */
    bool findHighestIntegerScale()
    {
        Vec2i newScale(findHighestFittingScale(scRes.x, winSize.x),
                       findHighestFittingScale(scRes.y, winSize.y));
        
        if (threadData->config.fixedAspectRatio)
        {
            /* Limit both factors to the smaller of the two */
            newScale.x = newScale.y = std::min(newScale.x, newScale.y);
        }
        
        if (newScale == integerScaleFactor)
            return false;
        
        integerScaleFactor = newScale;
        return true;
    }
    
    void rebuildIntegerScaleBuffer()
    {
#ifndef MKXPZ_RETRO
        TEXFBO::fini(integerScaleBuffer);
        TEXFBO::init(integerScaleBuffer);
        TEXFBO::allocEmpty(integerScaleBuffer, scRes.x * integerScaleFactor.x,
                           scRes.y * integerScaleFactor.y);
        TEXFBO::linkFBO(integerScaleBuffer);
#endif // MKXPZ_RETRO
    }
    
    bool integerScaleStepApplicable() const
    {
#ifdef MKXPZ_RETRO
        return false;
#else
        if (!integerScaleActive)
            return false;
        
        if (integerScaleFactor.x < 1 || integerScaleFactor.y < 1) // XXX should be < 2, this is for testing only
            return false;
        
        return true;
#endif // MKXPZ_RETRO
    }
    
    void checkResize(bool skipIntScaleBuffer = false) {
#ifndef MKXPZ_RETRO
        if (threadData->windowSizeMsg.poll(winSize)) {
            /* Query the actual size in pixels, not units */
            Vec2i drawableSize(winSize);
            threadData->drawableSizeMsg.poll(drawableSize);
            
            backingScaleFactor = drawableSize.x / winSize.x;
            winSize = drawableSize;
            
            /* Make sure integer buffers are rebuilt before screen offsets are
             * calculated so we have the final allocated buffer size ready */
            if (integerScaleActive && findHighestIntegerScale() && !skipIntScaleBuffer)
                rebuildIntegerScaleBuffer();
            
            /* some GL drivers change the viewport on window resize */
            glState.viewport.refresh();
            recalculateScreenSize(threadData->config.fixedAspectRatio);
            updateScreenResoRatio(threadData);
            
            SDL_Rect screen = {scOffset.x, scOffset.y, scSize.x, scSize.y};
            threadData->ethread->notifyGameScreenChange(screen);
        }
#endif // MKXPZ_RETRO
    }
    
    void checkShutDownReset() {
        shState->checkShutdown();
        shState->checkReset();
    }
    
    void shutdown() {
        threadData->rqTermAck.set();
        shState->texPool().disable();
        
        scriptBinding->terminate();
    }
    
    void swapGLBuffer() {
#ifndef MKXPZ_RETRO
        fpsLimiter.delay();
        SDL_GL_SwapWindow(threadData->window);
#endif // MKXPZ_RETRO
        
        ++frameCount;
        
#ifndef MKXPZ_RETRO
        threadData->ethread->notifyFrame();
#endif // MKXPZ_RETRO
    }
    
    void compositeToBuffer(Exception &exception, TEXFBO &buffer) {
        GUARD(compositeToBufferScaled(exception, buffer, scRes.x, scRes.y));
    }

    void compositeToBufferScaled(Exception &exception, TEXFBO &buffer, int destWidth, int destHeight) {
        GUARD(screen.composite(exception));
        
        int scaleIsSpecial = GLMeta::blitScaleIsSpecial(buffer, false, IntRect(0, 0, destWidth, destHeight), screen.getPP().frontBuffer(), IntRect(0, 0, scRes.x, scRes.y));

        GLMeta::blitBegin(buffer, false, scaleIsSpecial);
        GLMeta::blitSource(screen.getPP().frontBuffer(), scaleIsSpecial);
        GLMeta::blitRectangle(IntRect(0, 0, scRes.x, scRes.y), IntRect(0, 0, destWidth, destHeight));
        GLMeta::blitEnd();
    }
    
    void metaBlitBufferFlippedScaled(int scaleIsSpecial) {
        metaBlitBufferFlippedScaled(scRes, scaleIsSpecial);
        GLMeta::blitRectangle(
                              IntRect(0, 0, scRes.x, scRes.y),
#ifdef MKXPZ_RETRO
                              // Don't need to vertically flip the screen in libretro builds because the libretro frontend will do it for us
                              IntRect(scOffset.x, scOffset.y, scSize.x, scSize.y),
#else
                              IntRect(scOffset.x,
                                      (scSize.y + scOffset.y),
                                      scSize.x,
                                      -scSize.y),
#endif // MKXPZ_RETRO
                              GLMeta::smoothScalingMethod(scaleIsSpecial) == Bilinear);
    }
    
    void metaBlitBufferFlippedScaled(const Vec2i &sourceSize, int scaleIsSpecial, bool forceNearestNeighbor=false) {
        GLMeta::blitRectangle(IntRect(0, 0, sourceSize.x, sourceSize.y),
#ifdef MKXPZ_RETRO
                              // Don't need to vertically flip the screen in libretro builds because the libretro frontend will do it for us
                              IntRect(scOffset.x, scOffset.y, scSize.x, scSize.y),
#else
                              IntRect(scOffset.x, scSize.y+scOffset.y, scSize.x, -scSize.y),
#endif // MKXPZ_RETRO
                              !forceNearestNeighbor && GLMeta::smoothScalingMethod(scaleIsSpecial) == Bilinear);
    }
    
    void redrawScreen(Exception &exception) {
        GUARD(screen.composite(exception));
        
#ifndef MKXPZ_RETRO
        // maybe unspaghetti this later
        if (integerScaleStepApplicable() && !integerLastMileScaling)
        {
            int scaleIsSpecial = GLMeta::blitScaleIsSpecial(integerScaleBuffer, false, IntRect(0, 0, scSize.x, scSize.y), screen.getPP().frontBuffer(), IntRect(0, 0, scRes.x, scRes.y));

            GLMeta::blitBeginScreen(winSize, scaleIsSpecial);
            GLMeta::blitSource(screen.getPP().frontBuffer(), scaleIsSpecial);
            
            FBO::clear();
            metaBlitBufferFlippedScaled(scRes, scaleIsSpecial, true);
            GLMeta::blitEnd();
            
            swapGLBuffer();
            updateAvgFPS();
            return;
        }
        
        if (integerScaleStepApplicable())
        {
            int scaleIsSpecial = GLMeta::blitScaleIsSpecial(integerScaleBuffer, false, IntRect(0, 0, integerScaleBuffer.width, integerScaleBuffer.height), screen.getPP().frontBuffer(), IntRect(0, 0, scRes.x, scRes.y));

            assert(integerScaleBuffer.tex != TEX::ID(0));
            GLMeta::blitBegin(integerScaleBuffer, false, scaleIsSpecial);
            GLMeta::blitSource(screen.getPP().frontBuffer(), scaleIsSpecial);
            
            GLMeta::blitRectangle(IntRect(0, 0, scRes.x, scRes.y),
                                  IntRect(0, 0, integerScaleBuffer.width, integerScaleBuffer.height),
                                  false);
            
            GLMeta::blitEnd();
        }
#endif // MKXPZ_RETRO

        Vec2i sourceSize;

#ifndef MKXPZ_RETRO
        if (integerScaleActive)
        {
            sourceSize = Vec2i(integerScaleBuffer.width, integerScaleBuffer.height);
        }
        else
#endif // MKXPZ_RETRO
        {
            sourceSize = scRes;
        }

#ifdef MKXPZ_RETRO
        int scaleIsSpecial = 0;
#else
        int scaleIsSpecial = GLMeta::blitScaleIsSpecial(integerScaleBuffer, false, IntRect(0, 0, scSize.x, scSize.y), integerScaleActive ? integerScaleBuffer : screen.getPP().frontBuffer(), IntRect(0, 0, sourceSize.x, sourceSize.y));
#endif // MKXPZ_RETRO

        GLMeta::blitBeginScreen(winSize, scaleIsSpecial);
        //GLMeta::blitSource(screen.getPP().frontBuffer(), scaleIsSpecial);

#ifndef MKXPZ_RETRO
        if (integerScaleActive)
        {
            GLMeta::blitSource(integerScaleBuffer, scaleIsSpecial);
        }
        else
#endif // MKXPZ_RETRO
        {
            GLMeta::blitSource(screen.getPP().frontBuffer(), scaleIsSpecial);
        }
        
        FBO::clear();
        metaBlitBufferFlippedScaled(sourceSize, scaleIsSpecial);
        
        GLMeta::blitEnd();
        
        swapGLBuffer();
        
        updateAvgFPS();
    }
    
    void checkSyncLock() {
#ifndef MKXPZ_RETRO
        if (!threadData->syncPoint.mainSyncLocked())
            return;
        
        /* Releasing the GL context before sleeping and making it
         * current again on wakeup seems to avoid the context loss
         * when the app moves into the background on Android */
        SDL_GL_MakeCurrent(threadData->window, 0);
        threadData->syncPoint.waitMainSync();
        SDL_GL_MakeCurrent(threadData->window, glCtx);
        
        fpsLimiter.resetFrameAdjust();
#endif // MKXPZ_RETRO
    }
    
    double averageFPS() {
        double ret = 0;
#ifndef MKXPZ_RETRO
        SDL_LockMutex(avgFPSLock);
#endif // MKXPZ_RETRO
        for (double times : avgFPSData)
            ret += times;
        
        ret = 1 / (ret / avgFPSData.size());
#ifndef MKXPZ_RETRO
        SDL_UnlockMutex(avgFPSLock);
#endif // MKXPZ_RETRO
        return ret;
    }
    
    void setLock(bool force = false) {
        if (!(force || multithreadedMode)) return;
        
#ifndef MKXPZ_RETRO
        SDL_LockMutex(glResourceLock);
        SDL_GL_MakeCurrent(threadData->window, threadData->glContext);
#endif // MKXPZ_RETRO
    }
    
    void releaseLock(bool force = false) {
        if (!(force || multithreadedMode)) return;
        
#ifndef MKXPZ_RETRO
        SDL_UnlockMutex(glResourceLock);
#endif // MKXPZ_RETRO
    }

    void updateAvgFPS() {
#ifndef MKXPZ_RETRO
        SDL_LockMutex(avgFPSLock);
#endif // MKXPZ_RETRO
        if (avgFPSData.size() > 40)
            avgFPSData.erase(avgFPSData.begin());
        
        double time = shState->runTime();
        avgFPSData.push_back(time - last_avg_update);
        last_avg_update = time;
#ifndef MKXPZ_RETRO
        SDL_UnlockMutex(avgFPSLock);
#endif // MKXPZ_RETRO
    }
};

Graphics::Graphics(RGSSThreadData *data) {
    p = new GraphicsPrivate(data);
#ifndef MKXPZ_RETRO
    if (data->config.syncToRefreshrate) {
        p->frameRate = data->refreshRate;
        p->fpsLimiter.disabled = true;
    } else if (data->config.fixedFramerate > 0) {
        p->fpsLimiter.setDesiredFPS(data->config.fixedFramerate);
    } else if (data->config.fixedFramerate < 0) {
        p->fpsLimiter.disabled = true;
    }
#endif // MKXPZ_RETRO
}

Graphics::~Graphics() { delete p; }

double Graphics::getDelta() {
    return shState->runTime() - p->last_update;
}

double Graphics::lastUpdate() {
    return p->last_update;
}

bool Graphics::update(Exception &exception, bool checkForShutdown) {
    p->threadData->rqWindowAdjust.wait();
    p->last_update = shState->runTime();
    
    // update Input.repeat timing, rounding the framerate to the nearest 2
    {
        static const double mult = 2.0;
        double afr = std::abs(averageFrameRate()); // abs shouldn't be necessary but that's ok
        afr += mult / 2;
        afr -= std::fmod(afr, mult);
#ifdef MKXPZ_RETRO // TODO: merge into shState
        mkxp_retro::input->recalcRepeat(std::floor(afr));
#else
        shState->input().recalcRepeat(std::floor(afr));
#endif // MKXPZ_RETRO
    }
    
    if (checkForShutdown)
        p->checkShutDownReset();
    
    p->checkSyncLock();
    
    
#ifdef MKXPZ_STEAM
    if (STEAMSHIM_alive())
        STEAMSHIM_pump();
#endif // MKXPZ_STEAM
    
    if (p->frozen)
        return false;
    
#ifndef MKXPZ_RETRO
    if (p->fpsLimiter.frameSkipRequired()) {
        if (p->useFrameSkip) {
            /* Skip frame */
            p->fpsLimiter.delay();
            ++p->frameCount;
            p->threadData->ethread->notifyFrame();
            
            return true;
        } else {
            /* Just reset frame adjust counter */
            p->fpsLimiter.resetFrameAdjust();
        }
    }
#endif // MKXPZ_RETRO
    
    p->checkResize();
    GUARD_V(false, p->redrawScreen(exception));

    return true;
}

void Graphics::freeze(Exception &exception) {
    p->frozen = true;
    
    p->checkShutDownReset();
    p->checkResize();
    
    /* Capture scene into frozen buffer */
    GUARD(p->compositeToBuffer(exception, p->frozenScene));

#ifdef MKXPZ_RETRO
    if (frozenPixels.size() != (size_t)p->frozenScene.width * (size_t)p->frozenScene.height) {
        frozenPixels.clear();
        frozenPixels.resize((size_t)p->frozenScene.width * (size_t)p->frozenScene.height);
    }
    FBO::bind(p->frozenScene.fbo);
    gl.ReadPixels(0, 0, p->frozenScene.width, p->frozenScene.height, GL_RGBA, GL_UNSIGNED_BYTE, frozenPixels.data());
#endif // MKXPZ_RETRO
}

bool &Graphics::frozen() {
    return p->frozen;
}

#ifdef MKXPZ_RETRO
void Graphics::uploadFrozenPixels() {
    if (p->frozen) {
        TEX::bind(p->frozenScene.tex);
        TEX::uploadImage(p->frozenScene.width, p->frozenScene.height, frozenPixels.data(), GL_RGBA);
    }
}
#endif // MKXPZ_RETRO

void Graphics::transition(Exception &exception, int duration, Bitmap *transMap, int vague, int start, int stop) {
    p->checkSyncLock();
    
    if (!p->frozen)
        return;
    
    vague = clamp(vague, 1, 256);
    
    if (start <= 0) {
        setBrightness(255);
    }
    
    /* Capture new scene */
    GUARD(p->screen.composite(exception));
    
    /* The PP frontbuffer will hold the current scene after the
     * composition step. Since the backbuffer is unused during
     * the transition, we can reuse it as the target buffer for
     * the final rendered image. */
    TEXFBO &currentScene = p->screen.getPP().frontBuffer();
    TEXFBO &transBuffer = p->screen.getPP().backBuffer();
    
    /* If no transition bitmap is provided,
     * we can use a simplified shader */
    TransShader &transShader = shState->shaders().trans;
    SimpleTransShader &simpleShader = shState->shaders().simpleTrans;
    
    // Handle high-res.
    Vec2i transSize(p->scResLores.x, p->scResLores.y);

    if (transMap) {
        TransShader &shader = transShader;
        shader.bind();
        shader.applyViewportProj();
        shader.setFrozenScene(p->frozenScene.tex);
        shader.setCurrentScene(currentScene.tex);
        Bitmap *hires;
        GUARD(hires = transMap->getHires(exception));
        if (hires) {
            Debug() << "BUG: High-res Graphics transMap not implemented";
        }
        shader.setTransMap(transMap->getGLTypes().tex);
        shader.setVague(vague / 256.0f);
        shader.setTexSize(transSize);
    } else {
        SimpleTransShader &shader = simpleShader;
        shader.bind();
        shader.applyViewportProj();
        shader.setFrozenScene(p->frozenScene.tex);
        shader.setCurrentScene(currentScene.tex);
        shader.setTexSize(transSize);
    }
    
    glState.blend.pushSet(false);
    
    for (int i = start; i <= stop && i < duration; ++i) {
#ifndef MKXPZ_RETRO
        /* We need to clean up transMap properly before
         * a possible longjmp, so we manually test for
         * shutdown/reset here */
        if (p->threadData->rqTerm) {
            glState.blend.pop();
            delete transMap;
            p->shutdown();
            return;
        }
        
        if (p->threadData->rqReset) {
            glState.blend.pop();
            delete transMap;
            scriptBinding->reset();
            return;
        }
#endif // MKXPZ_RETRO
        
        p->checkSyncLock();
        
        const float prog = i * (1.0f / duration);
        
        if (transMap) {
            transShader.bind();
            transShader.setProg(prog);
        } else {
            simpleShader.bind();
            simpleShader.setProg(prog);
        }
        
        /* Draw the composed frame to a buffer first
         * (we need this because we're skipping PingPong) */
        FBO::bind(transBuffer.fbo);
        FBO::clear();
        p->screenQuad.draw();
        
        p->checkResize();
        
        /* Then blit it flipped and scaled to the screen */
        FBO::unbind();
        FBO::clear();
        
#ifdef MKXPZ_RETRO
        int scaleIsSpecial = 0;
#else
        int scaleIsSpecial = GLMeta::blitScaleIsSpecial(p->integerScaleBuffer, false, IntRect(0, 0, p->scSize.x, p->scSize.y), transBuffer, IntRect(0, 0, p->scRes.x, p->scRes.y));
#endif // MKXPZ_RETRO

        GLMeta::blitBeginScreen(Vec2i(p->winSize), scaleIsSpecial);
        GLMeta::blitSource(transBuffer, scaleIsSpecial);
        p->metaBlitBufferFlippedScaled(scaleIsSpecial);
        GLMeta::blitEnd();
        
        p->swapGLBuffer();
        /* Call this manually, as redrawScreen() is not called during this loop. */
        p->updateAvgFPS();
    }
    
    glState.blend.pop();
    
#ifndef MKXPZ_RETRO
    delete transMap;
#endif // MKXPZ_RETRO
    
    if (stop == INT_MAX || stop + 1 >= duration) {
        p->frozen = false;
    }
}

void Graphics::frameReset() {
#ifndef MKXPZ_RETRO
    p->fpsLimiter.resetFrameAdjust();
#endif // MKXPZ_RETRO
}

DEF_ATTR_NOEXCEPT_RD_SIMPLE(Graphics, FrameRate, int, p->frameRate)

DEF_ATTR_NOEXCEPT_SIMPLE(Graphics, FrameCount, int, p->frameCount)

void Graphics::setFrameRate(int value) {
    p->frameRate = std::max(value, 1);
    
    if (p->threadData->config.syncToRefreshrate)
        return;
    
    if (p->threadData->config.fixedFramerate > 0)
        return;
    
#ifndef MKXPZ_RETRO
    p->fpsLimiter.setDesiredFPS(p->frameRate);
#endif // MKXPZ_RETRO
    //shState->input().recalcRepeat((unsigned int)p->frameRate);
}

double Graphics::averageFrameRate() {
    return p->averageFPS();
}

void Graphics::wait(Exception &exception, int duration, int start, int stop) {
    for (int i = start; i <= stop && i < duration; ++i) {
        p->checkShutDownReset();
        GUARD(p->redrawScreen(exception));
    }
}

void Graphics::fadeout(Exception &exception, int duration, int start, int stop, int brightness) {
    FBO::unbind();
    
    float curr = brightness >= 0 && brightness <= 255 ? brightness : p->brightness;
    float diff = 255.0f - curr;
    
    for (int i = start; i <= stop && i < duration;) {
        setBrightness(diff + (curr / duration) * (duration - ++i));
        
        if (p->frozen) {
#ifdef MKXPZ_RETRO
            int scaleIsSpecial = 0;
#else
            int scaleIsSpecial = GLMeta::blitScaleIsSpecial(p->integerScaleBuffer, false, IntRect(0, 0, p->scSize.x, p->scSize.y), p->frozenScene, IntRect(0, 0, p->scRes.x, p->scRes.y));
#endif // MKXPZ_RETRO

            GLMeta::blitBeginScreen(p->scSize, scaleIsSpecial);
            GLMeta::blitSource(p->frozenScene, scaleIsSpecial);
            
            FBO::clear();
            p->metaBlitBufferFlippedScaled(scaleIsSpecial);
            
            GLMeta::blitEnd();
            
            p->swapGLBuffer();
        } else {
            GUARD(update(exception));
        }
    }
}

void Graphics::fadein(Exception &exception, int duration, int start, int stop, int brightness) {
    FBO::unbind();
    
    float curr = brightness >= 0 && brightness <= 255 ? brightness : p->brightness;
    float diff = 255.0f - curr;
    
    for (int i = start; i <= stop && i < duration;) {
        setBrightness(curr + (diff / duration) * ++i);
        
        if (p->frozen) {
#ifdef MKXPZ_RETRO
            int scaleIsSpecial = 0;
#else
            int scaleIsSpecial = GLMeta::blitScaleIsSpecial(p->integerScaleBuffer, false, IntRect(0, 0, p->scSize.x, p->scSize.y), p->frozenScene, IntRect(0, 0, p->scRes.x, p->scRes.y));
#endif // MKXPZ_RETRO

            GLMeta::blitBeginScreen(p->scSize, scaleIsSpecial);
            GLMeta::blitSource(p->frozenScene, scaleIsSpecial);
            
            FBO::clear();
            p->metaBlitBufferFlippedScaled(scaleIsSpecial);
            
            GLMeta::blitEnd();
            
            p->swapGLBuffer();
        } else {
            GUARD(update(exception));
        }
    }
}

Bitmap *Graphics::snapToBitmap(Exception &exception) {
    GUARD_V(nullptr, p->screen.composite(exception));

    if (shState->config().enableHires) {
        // TODO: Maybe don't reconstruct this struct every time?
        TEXFBO tf;
        tf.width = width();
        tf.height = height();
        tf.selfHires = &p->screen.getPP().frontBuffer();

        Bitmap *bitmap = new Bitmap(exception, tf);
        if (exception.is_error()) {
            delete bitmap;
            return nullptr;
        }
        return bitmap;
    }

    Bitmap *bitmap = new Bitmap(exception, p->screen.getPP().frontBuffer());
    if (exception.is_error()) {
        delete bitmap;
        return nullptr;
    }
    return bitmap;
}

int Graphics::width() const { return p->scResLores.x; }

int Graphics::height() const { return p->scResLores.y; }

int Graphics::widthHires() const { return p->scRes.x; }

int Graphics::heightHires() const { return p->scRes.y; }

bool Graphics::isPingPongFramebufferActive() const {
    return p->screen.getPP().frontBuffer().fbo == FBO::boundFramebufferID || p->screen.getPP().backBuffer().fbo == FBO::boundFramebufferID;
}

int Graphics::displayContentWidth() const {
    return p->scSize.x;
}

int Graphics::displayContentHeight() const {
    return p->scSize.y;
}

int Graphics::displayWidth() const {
#ifdef MKXPZ_RETRO
    return Graphics::displayContentWidth();
#else
    SDL_DisplayMode dm{};
    SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(shState->sdlWindow()), &dm);
    return dm.w / p->backingScaleFactor;
#endif // MKXPZ_RETRO
}

int Graphics::displayHeight() const {
#ifdef MKXPZ_RETRO
    return Graphics::displayContentHeight();
#else
    SDL_DisplayMode dm{};
    SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(shState->sdlWindow()), &dm);
    return dm.h / p->backingScaleFactor;
#endif // MKXPZ_RETRO
}

void Graphics::resizeScreen(int width, int height, bool resizeWindow) {
#ifndef MKXPZ_RETRO
    p->threadData->rqWindowAdjust.wait();
#endif // MKXPZ_RETRO
    p->checkResize(true);
    
    Vec2i sizeLores(width, height);

    if (shState->config().enableHires) {
        double framebufferScalingFactor = shState->config().framebufferScalingFactor;
        width = (int)lround(framebufferScalingFactor * width);
        height = (int)lround(framebufferScalingFactor * height);
    }

    Vec2i size(width, height);
    
    if (p->scRes == size && p->scResLores == sizeLores)
        return;
    
    p->scRes = size;
    p->scResLores = sizeLores;
    
    p->screen.setResolution(width, height);
    
    if (p->integerScaleActive)
        p->rebuildIntegerScaleBuffer();
    
    TEXFBO::allocEmpty(p->frozenScene, width, height);
    
    FloatRect screenRect(0, 0, width, height);
    p->screenQuad.setTexPosRect(screenRect, screenRect);
    
    glState.scissorBox.set(IntRect(0, 0, p->scRes.x, p->scRes.y));
    
    if (resizeWindow)
    {
#ifdef MKXPZ_RETRO
        p->winSize = Vec2i(width, height);
        p->recalculateScreenSize(p->threadData->config.fixedAspectRatio);
        p->updateScreenResoRatio(p->threadData);
        mkxp_retro::request_resize(width, height);
#else
        shState->eThread().requestWindowResize(width, height);
#endif // MKXPZ_RETRO
    }
}

void Graphics::resizeWindow(int width, int height, bool center) {
#ifndef MKXPZ_RETRO
    p->threadData->rqWindowAdjust.wait();
#endif // MKXPZ_RETRO
    p->checkResize();

#ifdef MKXPZ_RETRO
    const float factor = 1.0f;
#else
    const float factor = p->backingScaleFactor;
#endif // MKXPZ_RETRO
    if (width == p->winSize.x / factor &&
        height == p->winSize.y / factor)
            return;

#ifdef MKXPZ_RETRO
    p->winSize = Vec2i(width, height);
    p->recalculateScreenSize(p->threadData->config.fixedAspectRatio);
    p->updateScreenResoRatio(p->threadData);
    mkxp_retro::request_resize(width, height);
#else
    shState->eThread().requestWindowResize(width, height);
#endif // MKXPZ_RETRO
    
    if (center)
        this->center();
}

bool Graphics::updateMovieInput(Movie *movie) {
    return  p->threadData->rqTerm || p->threadData->rqReset;
}

Movie *Graphics::playMovie(Exception &exception, const char *filename, int volume_, bool skippable) {
    if (shState->config().enableHires) {
        Debug() << "BUG: High-res Graphics playMovie not implemented";
    }

    float volume = volume_ * 0.01f;
    Movie *movie = new Movie(volume, skippable);
    MovieOpenHandler handler(movie->srcOps);
#ifdef MKXPZ_RETRO // TODO: move into shState
    mkxp_retro::fs->openRead(handler, filename);
#else
    shState->fileSystem().openRead(handler, filename);
#endif // MKXPZ_RETRO
    if (handler.exception.is_error()) {
        delete movie;
        exception = handler.exception;
        return nullptr;
    }
    
    if (movie->preparePlayback()) {        
        movie->movieSprite = new Sprite;
        
        // Currently this stretches to fit the screen. VX Ace behavior is to center it and let the edges run off
        movie->movieSprite->setBitmap(exception, movie->videoBitmap);
        if (exception.is_error()) {
            delete movie;
            return nullptr;
        }
        double ratio = std::min((double)width() / movie->video->width, (double)height() / movie->video->height);
        movie->movieSprite->setZoomX(exception, ratio);
        if (exception.is_error()) {
            delete movie;
            return nullptr;
        }
        movie->movieSprite->setZoomY(exception, ratio);
        if (exception.is_error()) {
            delete movie;
            return nullptr;
        }
        movie->movieSprite->setX(exception, (width() / 2) - (movie->video->width * ratio / 2));
        if (exception.is_error()) {
            delete movie;
            return nullptr;
        }
        movie->movieSprite->setY(exception, (height() / 2) - (movie->video->height * ratio / 2));
        if (exception.is_error()) {
            delete movie;
            return nullptr;
        }
        
        movie->letterboxSprite = new Sprite;
        movie->letterbox = new Bitmap(exception, width(), height(), false, false);
        if (exception.is_error()) {
            delete movie;
            return nullptr;
        }
        movie->letterbox->fillRect(exception, 0, 0, width(), height(), Vec4(0,0,0,255));
        if (exception.is_error()) {
            delete movie;
            return nullptr;
        }
        movie->letterboxSprite->setBitmap(exception, movie->letterbox);
        if (exception.is_error()) {
            delete movie;
            return nullptr;
        }
        
        movie->letterboxSprite->setZ(exception, 4999);
        if (exception.is_error()) {
            delete movie;
            return nullptr;
        }
        movie->movieSprite->setZ(exception, 5001);
        if (exception.is_error()) {
            delete movie;
            return nullptr;
        }
        
        if (movie->play()) {
            return movie;
        }
    }
    
#ifndef MKXPZ_RETRO
    delete movie;
#endif // MKXPZ_RETRO
    return nullptr;
}

Movie *Graphics::playMovie(Movie *movie) {
    if (movie == nullptr) {
        return nullptr;
    }

    if (movie->play()) {
        return movie;
    }

#ifndef MKXPZ_RETRO
    delete movie;
#endif // MKXPZ_RETRO
    return nullptr;
}

void Graphics::stopMovie(Movie *movie) {
    if (movie != nullptr) {
        delete movie;
    }
}

bool Graphics::streamMovieAudioProc(Movie *movie) {
    return movie->streamMovieAudioProc();
}

#ifdef MKXPZ_RETRO
bool Graphics::getMovieDupeFrame(Movie *movie) {
    return movie->dupeFrame;
}

void Graphics::sandbox_reinit() {
    p->reinit();
    p->screenQuad.reinit();
    p->screen.screenQuad.reinit();
    p->screen.brightnessQuad.reinit();
    p->screen.pp.reinit();
    uploadFrozenPixels();
}
#endif // MKXPZ_RETRO

void Graphics::screenshot(Exception &exception, const char *filename) {
#ifndef MKXPZ_RETRO
    p->threadData->rqWindowAdjust.wait();
#endif // MKXPZ_RETRO
    Bitmap *ss = snapToBitmap(exception);
    if (exception.is_error()) {
        delete ss;
        return;
    }
    ss->saveToFile(exception, filename);
    if (exception.is_error()) {
        delete ss;
        return;
    }
    ss->dispose();
    delete ss;
}

DEF_ATTR_NOEXCEPT_RD_SIMPLE(Graphics, Brightness, int, p->brightness)

void Graphics::setBrightness(int value) {
    value = clamp(value, 0, 255);
    
    if (p->brightness == value)
        return;
    
    p->brightness = value;
    p->screen.setBrightness(value / 255.0);
}

void Graphics::reset(Exception &exception) {
    /* Dispose all live Disposables */
    IntruListLink<Disposable> *iter;
    
    for (iter = p->dispList.begin(); iter != p->dispList.end();
         iter = iter->next) {
        iter->data->dispose();
    }
    
    p->dispList.clear();
    
    /* Reset attributes (frame count not included) */
#ifndef MKXPZ_RETRO
    p->fpsLimiter.resetFrameAdjust();
#endif // MKXPZ_RETRO
    p->frozen = false;
    p->screen.getPP().clearBuffers();
    
    setFrameRate(DEF_FRAMERATE);
    setBrightness(255);
    
#ifndef MKXPZ_RETRO
    // Always update at least once to clear the screen
    if (p->threadData->rqResetFinish)
#endif // MKXPZ_RETRO
        GUARD(update(exception));
#ifndef MKXPZ_RETRO
    else
        repaintWait(p->threadData->rqResetFinish, false);
    p->threadData->rqReset.clear();
#endif // MKXPZ_RETRO
}

void Graphics::center() {
#ifndef MKXPZ_RETRO
    p->threadData->rqWindowAdjust.wait();
    if (getFullscreen())
        return;
    
    p->threadData->ethread->requestWindowCenter();
#endif // MKXPZ_RETRO
}

bool Graphics::getFullscreen() const {
#ifdef MKXPZ_RETRO
    return false; // TODO
#else
    return p->threadData->ethread->getFullscreen();
#endif // MKXPZ_RETRO
}

void Graphics::setFullscreen(bool value) {
#ifndef MKXPZ_RETRO
    p->threadData->ethread->requestFullscreenMode(value);
#endif // MKXPZ_RETRO
}

bool Graphics::getShowCursor() const {
#ifdef MKXPZ_RETRO
    return true; // TODO
#else
    return p->threadData->ethread->getShowCursor();
#endif // MKXPZ_RETRO
}

void Graphics::setShowCursor(bool value) {
#ifndef MKXPZ_RETRO
    p->threadData->ethread->requestShowCursor(value);
#endif // MKXPZ_RETRO
}

bool Graphics::getFixedAspectRatio() const
{
    // It's a bit hacky to expose config values as a Graphics
    // attribute, but there's really no point in state duplication
    return shState->config().fixedAspectRatio;
}

void Graphics::setFixedAspectRatio(bool value)
{
    shState->config().fixedAspectRatio = value;
    p->recalculateScreenSize(p->threadData->config.fixedAspectRatio);
    p->findHighestIntegerScale();
    p->recalculateScreenSize(p->threadData->config.fixedAspectRatio);
    p->updateScreenResoRatio(p->threadData);
}

int Graphics::getSmoothScaling() const
{
    // Same deal as with fixed aspect ratio
    return shState->config().smoothScaling;
}

void Graphics::setSmoothScaling(int value)
{
    shState->config().smoothScaling = value;
}

bool Graphics::getIntegerScaling() const
{
    return p->integerScaleActive;
}

void Graphics::setIntegerScaling(bool value)
{
    p->integerScaleActive = value;
    p->findHighestIntegerScale();
    p->rebuildIntegerScaleBuffer();
    
    p->recalculateScreenSize(p->threadData->config.fixedAspectRatio);
    p->updateScreenResoRatio(p->threadData);
}

bool Graphics::getLastMileScaling() const
{
    return p->integerLastMileScaling;
}

void Graphics::setLastMileScaling(bool value)
{
    p->integerLastMileScaling = value;
    p->recalculateScreenSize(p->threadData->config.fixedAspectRatio);
    p->updateScreenResoRatio(p->threadData);
}

bool Graphics::getThreadsafe() const
{
    return p->multithreadedMode;
}

void Graphics::setThreadsafe(bool value)
{
    p->multithreadedMode = value;
}

double Graphics::getScale() const {
    p->checkResize();
#ifdef MKXPZ_RETRO
        const float factor = 1.0f;
#else
        const float factor = p->backingScaleFactor;
#endif // MKXPZ_RETRO
    return (double)(p->winSize.y / factor) / p->scRes.y;
    
}

void Graphics::setScale(double factor) {
#ifndef MKXPZ_RETRO
    p->threadData->rqWindowAdjust.wait();
#endif // MKXPZ_RETRO
    factor = clamp(factor, 0.5, 4.0);
    
    if (factor == getScale())
        return;
    
    int widthpx = p->scRes.x * factor;
    int heightpx = p->scRes.y * factor;
    
#ifdef MKXPZ_RETRO
    p->winSize = Vec2i(widthpx, heightpx);
    p->recalculateScreenSize(p->threadData->config.fixedAspectRatio);
    p->updateScreenResoRatio(p->threadData);
    mkxp_retro::request_resize(widthpx, heightpx);
#else
    shState->eThread().requestWindowResize(widthpx, heightpx);
#endif // MKXPZ_RETRO
}

bool Graphics::getFrameskip() const { return p->useFrameSkip; }

void Graphics::setFrameskip(bool value) { p->useFrameSkip = value; }

Scene *Graphics::getScreen() const { return &p->screen; }

void Graphics::repaint(bool useBackBuffer) {
    /* Repaint the screen with the last good frame we drew */
    TEXFBO &lastFrame = useBackBuffer ? p->screen.getPP().backBuffer() : p->screen.getPP().frontBuffer();

#ifdef MKXPZ_RETRO
    int scaleIsSpecial = 0;
#else
    int scaleIsSpecial = GLMeta::blitScaleIsSpecial(p->integerScaleBuffer, false, IntRect(0, 0, p->scSize.x, p->scSize.y), lastFrame, IntRect(0, 0, p->scRes.x, p->scRes.y));
#endif // MKXPZ_RETRO

    GLMeta::blitBeginScreen(p->winSize, scaleIsSpecial);
    GLMeta::blitSource(lastFrame, scaleIsSpecial);

    FBO::clear();
    p->metaBlitBufferFlippedScaled(scaleIsSpecial);
#ifndef MKXPZ_RETRO
    SDL_GL_SwapWindow(p->threadData->window);
    p->fpsLimiter.delay();

    p->threadData->ethread->notifyFrame();
#endif // MKXPZ_RETRO

    GLMeta::blitEnd();
}

void Graphics::repaintWait(const AtomicFlag &exitCond, bool checkReset) {
    if (exitCond)
        return;
    
    /* Repaint the screen with the last good frame we drew */
    TEXFBO &lastFrame = p->screen.getPP().frontBuffer();

#ifdef MKXPZ_RETRO
    int scaleIsSpecial = 0;
#else
    int scaleIsSpecial = GLMeta::blitScaleIsSpecial(p->integerScaleBuffer, false, IntRect(0, 0, p->scSize.x, p->scSize.y), lastFrame, IntRect(0, 0, p->scRes.x, p->scRes.y));
#endif // MKXPZ_RETRO

    GLMeta::blitBeginScreen(p->winSize, scaleIsSpecial);
    GLMeta::blitSource(lastFrame, scaleIsSpecial);
    
    while (!exitCond) {
        shState->checkShutdown();
        
        if (checkReset)
            shState->checkReset();
        
        FBO::clear();
        p->metaBlitBufferFlippedScaled(scaleIsSpecial);
#ifndef MKXPZ_RETRO
        SDL_GL_SwapWindow(p->threadData->window);
        p->fpsLimiter.delay();
        
        p->threadData->ethread->notifyFrame();
#endif // MKXPZ_RETRO
    }
    
    GLMeta::blitEnd();
}

void Graphics::lock(bool force) {
    p->setLock(force);
}

void Graphics::unlock(bool force) {
    p->releaseLock(force);
}

void Graphics::addDisposable(Disposable *d) { p->dispList.append(d->link); }

void Graphics::remDisposable(Disposable *d) { p->dispList.remove(d->link); }

#ifdef MKXPZ_RETRO
#ifndef MKXPZ_SANDBOX_SERIAL_GRAPHICS_H
#define MKXPZ_SANDBOX_SERIAL_GRAPHICS_H
#include "sandbox-serial-graphics.h"
#endif // MKXPZ_SANDBOX_SERIAL_GRAPHICS_H
#endif // MKXPZ_RETRO

#undef GRAPHICS_THREAD_LOCK
