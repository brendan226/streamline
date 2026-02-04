#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>

/* #define MINIAUDIO_IMPLEMENTATION */
#include <miniaudio.h>

typedef struct {
    float *pcm;
    float volume;
    float duration;

    bool loop;
    bool running;
    
    ma_uint64 total_frames;
    ma_uint64 cursor;
    ma_uint32 channels;

    ma_result result;
    ma_decoder decoder;
    ma_device device;
    ma_device_config device_config;
    ma_decoder_config decoder_config;
} playback_device_f;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frame_count)
{
    playback_device_f *state = (playback_device_f*)pDevice->pUserData;
    float* out = (float*)pOutput;

    ma_uint32 channels = state->channels;
    ma_uint32 frames_written = 0;

    while (frames_written < frame_count) {
        ma_uint64 frames_rem = state->total_frames - state->cursor;
        ma_uint32 frames_to_write = (ma_uint32)(frame_count - frames_written);
        
        if (frames_rem < frames_to_write)
            frames_to_write = (ma_uint32)frames_rem;

        memcpy(out, state->pcm + state->cursor * channels,
               frames_to_write * channels * sizeof(float));

        ma_uint32 samples = frames_to_write * channels;
        for (ma_uint32 i = 0; i < samples; ++i) {
            out[i] *= state->volume;
        }

        out += samples;
        frames_written += frames_to_write;
        state->cursor += frames_to_write;

        if (state->cursor >= state->total_frames) {
            if (state->loop) {
                state->cursor = 0;
            } else {
                state->running = false;
                ma_uint32 rem_samples = (frame_count - frames_written) * channels;
                memset(out, 0, rem_samples * sizeof(float));
                break;
            }
        }

    }
    
    (void)pInput;
}

void playback_device_init(playback_device_f *pbd, const char *filename,
                          float volume, bool loop)
{
    pbd->pcm = NULL;
    pbd->total_frames = 0;
    pbd->cursor = 0;
    pbd->channels = 0;
    pbd->loop = loop;
    if (volume == 0) pbd->volume = 1.0f;
    pbd->volume = volume;
    pbd->running = true;
    
    pbd->decoder_config = ma_decoder_config_init(ma_format_f32, 0, 0);    
    pbd->result = ma_decoder_init_file(filename, &pbd->decoder_config, &pbd->decoder);
    if (pbd->result != MA_SUCCESS) {
        printf("Could not load file: %s\n", filename);
        return;
    }
    
    pbd->device_config = ma_device_config_init(ma_device_type_playback);
    pbd->device_config.playback.format   = ma_format_f32;
    pbd->device_config.playback.channels = pbd->decoder.outputChannels;
    pbd->channels = pbd->device_config.playback.channels;
    pbd->device_config.sampleRate        = pbd->decoder.outputSampleRate;
    pbd->device_config.dataCallback      = data_callback;
    pbd->device_config.pUserData         = pbd;
    pbd->device_config.noClip            = true;

    ma_decoder_get_length_in_pcm_frames(&pbd->decoder, &pbd->total_frames);

    pbd->pcm = (float*)_aligned_malloc(pbd->total_frames * pbd->channels * sizeof(float), 64);

    if (ma_device_init(NULL, &pbd->device_config, &pbd->device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        ma_decoder_uninit(&pbd->decoder);
    }
}

void move_samples(playback_device_f *pbd)
{
    ma_uint64 frames_read_total = 0;

    while (frames_read_total < pbd->total_frames) {
        ma_uint64 frames_read;
        ma_uint64 frames_to_read = pbd->total_frames - frames_read_total;

        ma_decoder_read_pcm_frames(
                                   &pbd->decoder,
                                   pbd->pcm + frames_read_total * pbd->channels,
                                   frames_to_read,
                                   &frames_read
                                   );

        if (frames_read == 0) break;
        frames_read_total += frames_read;
    }
}

void play_file(playback_device_f *pbd)
{
    if (ma_device_start(&pbd->device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&pbd->device);
        ma_decoder_uninit(&pbd->decoder);
        return;
    }
}

void destory_playback_device(playback_device_f *pbd)
{
    ma_device_stop(&pbd->device);
    
    if (pbd->pcm != NULL) free(pbd->pcm);

    pbd->total_frames = 0;
    pbd->cursor = 0;
    pbd->channels = 0;

    ma_device_uninit(&pbd->device);
    ma_decoder_uninit(&pbd->decoder);
}

int main(int argc, char** argv)
{    
    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    const char *filename = argv[1];

    playback_device_f pbd;
    playback_device_init(&pbd, filename, 0.2f, false);
    move_samples(&pbd);

    printf("playing %s\n", filename);

    while (pbd.running) {
        play_file(&pbd);
    }

    destory_playback_device(&pbd);
    
    return 0;
}
