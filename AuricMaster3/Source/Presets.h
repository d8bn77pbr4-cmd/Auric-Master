#pragma once

#include <juce_core/juce_core.h>
#include <utility>
#include <vector>

// Factory presets: values on top of the default settings (everything not listed stays at its default).
// Thresholds assume a mix around -18 to -14 LUFS; after loading, LERNEN sets the loudness.
namespace FactoryPresets
{
    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };

    inline const std::vector<Preset>& all()
    {
        static const std::vector<Preset> presets = {
            { "Neutral (alles aus)", {} },
            { "Master - Streaming sauber", {
                { "dyn_on", 1 }, { "mb_low_thr", -24 }, { "mb_mid_thr", -26 }, { "mb_high_thr", -32 },
                { "mb_low_ratio", 1.8f }, { "mb_mid_ratio", 1.8f }, { "mb_high_ratio", 1.8f },
                { "glue_on", 1 }, { "glue_thr", -20 }, { "glue_ratio", 2 }, { "glue_att", 30 },
                { "width_high", 110 }, { "mono_freq", 100 },
                { "lim_on", 1 }, { "lim_mode", 0 }, { "clip", 15 }, { "target", 1 }, { "ceiling", -1 } } },
            { "Master - Warm & breit", {
                { "tone_low", 1 }, { "tone_high", 1.5f },
                { "dyn_on", 1 }, { "mb_low_thr", -24 }, { "mb_mid_thr", -26 }, { "mb_high_thr", -32 },
                { "sat_on", 1 }, { "sat_low", 30 }, { "sat_high", 20 },
                { "width_mid", 110 }, { "width_high", 125 }, { "mono_freq", 110 },
                { "glue_on", 1 }, { "glue_thr", -20 }, { "glue_att", 30 },
                { "lim_on", 1 }, { "lim_mode", 1 }, { "clip", 20 }, { "target", 1 } } },
            { "Master - Laut (Club -7)", {
                { "dyn_on", 1 }, { "mb_low_thr", -26 }, { "mb_mid_thr", -28 }, { "mb_high_thr", -34 },
                { "mb_low_ratio", 2.5f }, { "mb_mid_ratio", 2.5f }, { "mb_high_ratio", 2.5f },
                { "sat_on", 1 }, { "sat_low", 35 }, { "sat_high", 25 }, { "mono_freq", 120 },
                { "glue_on", 1 }, { "glue_thr", -22 }, { "glue_ratio", 3 }, { "glue_att", 10 },
                { "lim_on", 1 }, { "lim_mode", 2 }, { "clip", 50 }, { "target", 5 } } },
            { "Master - Hip-Hop", {
                { "tone_low", 1.5f }, { "dyn_on", 1 }, { "mb_low_thr", -24 }, { "mb_mid_thr", -26 }, { "mb_high_thr", -32 },
                { "mb_low_att", 40 }, { "sat_on", 1 }, { "sat_low", 30 }, { "sat_high", 10 }, { "mono_freq", 120 },
                { "glue_on", 1 }, { "glue_thr", -20 }, { "glue_att", 30 },
                { "lim_on", 1 }, { "lim_mode", 1 }, { "clip", 35 }, { "target", 3 } } },
            { "Master - Akustik dynamisch", {
                { "glue_on", 1 }, { "glue_thr", -24 }, { "glue_ratio", 1.5f }, { "glue_att", 30 }, { "glue_mix", 70 },
                { "lim_on", 1 }, { "lim_mode", 0 }, { "clip", 0 }, { "target", 2 } } },
            { "Mix-Bus - Glue 2 dB", {
                { "glue_on", 1 }, { "glue_thr", -18 }, { "glue_ratio", 2 }, { "glue_att", 10 }, { "glue_auto", 1 } } },
            { "Drum-Bus - Punch (parallel)", {
                { "glue_on", 1 }, { "glue_thr", -24 }, { "glue_ratio", 4 }, { "glue_att", 30 }, { "glue_rel", 120 },
                { "glue_auto", 0 }, { "glue_makeup", 4 }, { "glue_mix", 50 }, { "glue_hpf", 0 },
                { "sat_on", 1 }, { "sat_low", 30 }, { "sat_high", 0 } } },
            { "Bus - Resonanzen glätten", {
                { "res_on", 1 }, { "res_depth", 30 } } },
            { "Nur Limiter (-1 dBTP)", {
                { "eq_on", 0 }, { "st_on", 0 }, { "lim_on", 1 }, { "lim_mode", 0 }, { "target", 1 } } },
        };
        return presets;
    }
}
