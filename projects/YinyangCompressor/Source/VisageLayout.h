#pragma once

#include <array>
#include <cstddef>

namespace yinyangVisage
{
struct Theme
{
    unsigned int background, panel, meterPanel, text, secondary, control, meterColor, grid, gaugeTrack;
};
inline constexpr std::array<Theme, 4> yangThemes {{
    { 0xff252b31, 0xff303840, 0xff151a1e, 0xfff0f1ee, 0xffb7c0c6, 0xffa9b1b9, 0xff80549b, 0xff354048, 0xff414b53 },
    { 0xffe7ecef, 0xfff8fafb, 0xff0d1d2b, 0xff25343c, 0xff586b74, 0xff4389b3, 0xfff7fbff, 0xffcbd5da, 0xff3c5262 },
    { 0xffe4f0df, 0xffd1e5c9, 0xff1d392b, 0xff253b29, 0xff526b52, 0xff70a963, 0xfff8fff7, 0xff49624f, 0xff526b59 },
    { 0xfff5f7f6, 0xffe8edeb, 0xff252a30, 0xff283138, 0xff56646b, 0xfff8faf9, 0xffdc4a55, 0xff525d64, 0xff56616a }
}};
inline constexpr std::array<Theme, 4> yinThemes {{
    { 0xff1a1d21, 0xff242a30, 0xff101316, 0xffffffff, 0xffffffff, 0xff78578f, 0xff694881, 0xff302d35, 0xff343a40 },
    { 0xff0e1e2a, 0xff192f40, 0xff09151f, 0xfff7fbff, 0xff93aebd, 0xff3b82a8, 0xff93b4c8, 0xff293f4e, 0xff304654 },
    { 0xff355a46, 0xff406b52, 0xff182e25, 0xfff8fff7, 0xff91a990, 0xff5e8b5e, 0xff8dbd8c, 0xff354b3a, 0xff3b5140 },
    { 0xff281a1e, 0xff382126, 0xff151518, 0xfffff7f6, 0xffbda6a9, 0xff9d3d48, 0xffa83e49, 0xff403238, 0xff42383d }
}};
inline constexpr const Theme& themeFor (int model, bool yin) noexcept
{
    return yin ? yinThemes[static_cast<std::size_t> (model)] : yangThemes[static_cast<std::size_t> (model)];
}
enum class ControlKind { model, knob, toggle };
struct Control
{
    const char* id;
    const char* caption;
    const char* hint;
    float x, y, width, height;
    ControlKind kind;
};

inline constexpr std::array<Control, 19> controls {{
    { "model", "MODEL", "Selects the compressor model used for detection and gain reduction.", 24, 68, 500, 32, ControlKind::model },
    { "input", "INPUT", "Adjusts the level entering the compressor and its detector.", 265, 110, 53, 54, ControlKind::knob },
    { "threshold", "THRESHOLD", "Sets the level above which Yang compresses and below which Yin boosts.", 38, 174, 128, 108, ControlKind::knob },
    { "ratio", "RATIO", "Sets compression strength from 1:1 to 20:1 in Yang or 1:1 to 2:1 in Yin.", 168, 174, 156, 108, ControlKind::knob },
    { "attack", "ATTACK", "Sets attack timing in Yang and release timing in Yin.", 62, 300, 80, 60, ControlKind::knob },
    { "release", "RELEASE", "Sets release timing in Yang and attack timing in Yin.", 204, 300, 80, 60, ControlKind::knob },
    { "knee", "KNEE", "Softens the transition around the compression threshold.", 62, 380, 80, 60, ControlKind::knob },
    { "output", "OUTPUT", "Adjusts output gain after the wet and dry mix.", 344, 638, 60, 66, ControlKind::knob },
    { "mix", "MIX", "Blends the processed signal with the original input.", 404, 638, 60, 66, ControlKind::knob },
    { "sidechain", "DETECTOR HPF", "Removes low frequencies from the detector, with 0 Hz disabling the filter.", 30, 510, 80, 66, ControlKind::knob },
    { "drive", "CORE DRIVE", "Increases saturation in the enabled output transformer.", 30, 638, 80, 66, ControlKind::knob },
    { "transformer", "TRANSFORMER", "Saves CPU when Off; adds transformer color when On.", 241, 633, 58, 64, ControlKind::toggle },
    { "bypass", "BYPASS", "Bypasses compression and coloration for a direct comparison.", 450, 33, 74, 28, ControlKind::toggle },
    { "upward", "YIN / YANG", "Switches between Yang compression above threshold and Yin boosting below threshold.", 32, 116, 112, 28, ControlKind::toggle },
    { "stereoLink", "ST LINK", "Blends each channel's detector toward the louder channel's detector.", 464, 638, 60, 66, ControlKind::knob },
    { "midSide", "MID / SIDE", "Processes stereo as mid and side before decoding it back to left and right.", 241, 505, 58, 64, ControlKind::toggle },
    { "evenHarmonics", "DC BIAS", "Adds transformer asymmetry to increase even harmonics.", 130, 638, 80, 66, ControlKind::knob },
    { "variTransient", "VARITRANSIENT", "Slows attack and speeds release as Yang gain reduction increases.", 204, 380, 80, 60, ControlKind::knob },
    { "scHighTilt", "SC HIGH TILT", "Boosts high frequencies in the sidechain to increase their compression sensitivity.", 130, 510, 80, 66, ControlKind::knob }
}};
}
