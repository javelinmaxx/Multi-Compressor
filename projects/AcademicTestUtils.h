#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace academic
{
inline int checks = 0;
inline void check (bool condition, const char* message)
{
    ++checks;
    if (! condition) throw std::runtime_error (message);
}
inline std::ofstream csv (const std::filesystem::path& directory, const char* name, const char* header)
{
    std::filesystem::create_directories (directory);
    std::ofstream stream (directory/name);
    stream.exceptions (std::ios::badbit | std::ios::failbit);
    stream.precision (10); stream << header << '\n'; return stream;
}
inline double db (double ratio) { return 20*std::log10 (std::max (1e-15, ratio)); }
inline double rms (const std::vector<float>& x)
{
    double sum = 0; for (auto v : x) sum += v*v;
    return std::sqrt (sum/std::max (size_t (1), x.size()));
}
inline double peak (const std::vector<float>& x)
{
    double maximum = 0; for (auto v : x) maximum = std::max (maximum, std::abs (double (v)));
    return maximum;
}
inline double amplitude (const std::vector<float>& x, double hz, double rate)
{
    std::complex<double> sum {};
    for (size_t i = 0; i < x.size(); ++i)
        sum += double (x[i])*std::polar (1., -2*juce::MathConstants<double>::pi*hz*i/rate);
    return 2*std::abs (sum)/x.size();
}
inline void manifest (const std::filesystem::path& directory, const char* project)
{
    auto file = csv (directory, "manifest.csv", "project,version,execution,normalization");
    file << project << ',' << JucePlugin_VersionString << ",deterministic_offline,none\n";
}
}
