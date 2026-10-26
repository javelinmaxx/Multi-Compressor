#include "../Source/PluginProcessor.h"
#include "../Measurements/SynthPad.h"
#include "../../AcademicAblations.h"
#include "../../AcademicTestUtils.h"
#include <chrono>
#include <random>

// Only this harness can call the real search synchronously. The background
// thread is stopped before supplying snapshots, removing scheduler variance.
struct HaasAblationAccess
{
    using Worker = KuoStereoProcessor::HaasSearchWorker;
    static void prepare (Worker& w, float rate)
    { w.prepare (rate, size_t (2048+std::ceil (.031*rate)+4)); w.stopThread (-1); w.setAdaptiveMode(); }
    static void process (Worker& w, Worker::SearchJob& job)
    {
        job.generation = w.generation.load();
        job.sequence = w.latestSequence.fetch_add (1)+1;
        w.process (job);
    }
    static int selected (const Worker& w) { return w.selectedCandidate; }
};
namespace
{
using namespace academic;
using Access = HaasAblationAccess;
struct Variant { const char* name; HaasOptions options; };
const std::array<Variant, 19> variants {{
    {"current", {}}, {"mid_only", {true,false,true,true,false}},
    {"side_only", {false,true,true,true,false}}, {"no_averaging", {true,true,false,true,false}},
    {"no_margin", {true,true,true,false,false}},
    {"ratio_0.10", {true,true,true,true,false,.10f}}, {"ratio_0.30", {true,true,true,true,false,.30f}},
    {"penalty_0.05", {true,true,true,true,false,.20f,.05f}},
    {"penalty_0.10", {true,true,true,true,false,.20f,.10f}},
    {"penalty_0.50", {true,true,true,true,false,.20f,.50f}},
    {"penalty_1.00", {true,true,true,true,false,.20f,1.00f}},
    {"penalty_2.00", {true,true,true,true,false,.20f,2.00f}},
    {"penalty_0.175", {true,true,true,true,false,.20f,.175f}}, {"penalty_0.70", {true,true,true,true,false,.20f,.70f}},
    {"average_125ms", {true,true,true,true,false,.20f,.35f,.125f}}, {"average_500ms", {true,true,true,true,false,.20f,.35f,.50f}},
    {"margin_0.96", {true,true,true,true,false,.20f,.35f,.25f,.96f}}, {"margin_0.99", {true,true,true,true,false,.20f,.35f,.25f,.99f}},
    {"exhaustive", {true,true,false,false,true}}
}};
float signal (int sample, float rate, int corpus, int channel)
{
    if (sample < 0) return 0;
    const auto time = sample/double (rate);
    const auto pad = kuoTestSignal::padSample (int (time*48000));
    if (corpus == 0) return pad;
    if (corpus == 1) return .9f*pad+.05f*float (std::sin (2*juce::MathConstants<double>::pi*(channel == 0 ? 523.25 : 554.37)*time));
    if (corpus == 2) return float (.12*std::sin (2*juce::MathConstants<double>::pi*(channel == 0 ? 277.18 : 369.99)*time)+.5*pad);
    // Deterministic broadband clicks over the same pad, shared between channels.
    const auto since = std::fmod (time,.4);
    return pad+float (.4*std::exp (-since/.002)*std::cos (2*juce::MathConstants<double>::pi*6300*since));
}
}
int main (int argc, char** argv)
{
    try
    {
        const std::filesystem::path directory = argc > 1 ? argv[1] : "academic-results";
        manifest (directory, "KuoStereo");
        auto out = csv (directory, "haas.csv", "rate,corpus,center_ms,range_ms,variant,frame,time_s,delay_ms,changed,mid_log_error,side_mid_rms,combined_score,oracle_score,regret,search_us");
        constexpr std::array<const char*,4> names {"C_to_Am_pad","correlated_stereo","wide_stereo","transient_pad"};
        for (float rate : {44100.f,48000.f,96000.f})
            for (int corpus = 0; corpus < 4; ++corpus)
                for (const auto range : std::array<std::array<float,2>,3> {{{0,6},{12,6},{-28,2}}})
                {
                    std::array<std::unique_ptr<Access::Worker>, variants.size()> workers;
                    for (auto& w : workers) { w = std::make_unique<Access::Worker>(); Access::prepare (*w,rate); }
                    Access::Worker::SearchJob job;
                    job.leftAgo.resize (size_t (2048+std::ceil (.031*rate)+4)); job.rightAgo.resize (job.leftAgo.size());
                    job.centerMs = range[0]; job.variabilityMs = range[1];
                    const int frames = int (6*rate/2048);
                    std::array<int,variants.size()> previous; previous.fill (-1);
                    for (int frame = 1; frame <= frames; ++frame)
                    {
                        for (size_t ago = 0; ago < job.leftAgo.size(); ++ago)
                        {
                            job.leftAgo[ago] = signal (frame*2048-1-int (ago),rate,corpus,0);
                            job.rightAgo[ago] = signal (frame*2048-1-int (ago),rate,corpus,1);
                        }
                        // Exhaustive current-score oracle on the very same frame.
                        const auto oracleStart = std::chrono::steady_clock::now();
                        haas = variants.back().options; Access::process (*workers.back(),job);
                        const auto oracleUs = std::chrono::duration<double,std::micro> (std::chrono::steady_clock::now()-oracleStart).count();
                        const auto metrics = haasMetrics;
                        const auto oracleIndex = Access::selected (*workers.back());
                        check (oracleIndex >= 0, "non-silent snapshot yields an oracle candidate");
                        const auto oracle = metrics[size_t (oracleIndex)].rawScore;
                        const auto first = int (std::ceil (std::max (-30.f,range[0]-range[1])*5-.0001f));
                        for (size_t v = 0; v < variants.size(); ++v)
                        {
                            haas = variants[v].options;
                            const auto start = std::chrono::steady_clock::now();
                            if (v+1 < variants.size()) Access::process (*workers[v],job);
                            const auto us = std::chrono::duration<double,std::micro> (std::chrono::steady_clock::now()-start).count();
                            const auto selected = Access::selected (*workers[v]);
                            check (selected >= 0 && selected < int (metrics.size()), "selected index is bounded");
                            const auto& metric = metrics[size_t (selected)];
                            check (std::isfinite (metric.rawScore) && metric.rawScore+1e-6 >= oracle, "exhaustive oracle bounds instantaneous combined score");
                            const auto delay = .2f*(first+selected);
                            check (delay >= std::max (-30.f,range[0]-range[1])-.001f && delay <= std::min (30.f,range[0]+range[1])+.001f,"delay respects signed search bounds");
                            out << rate << ',' << names[size_t (corpus)] << ',' << range[0] << ',' << range[1] << ',' << variants[v].name << ',' << frame << ',' << frame*2048/rate << ',' << delay << ',' << (previous[v] >= 0 && previous[v] != selected) << ',' << metric.spectrum << ',' << metric.sideRatio << ',' << metric.rawScore << ',' << oracle << ',' << std::max (0.f,metric.rawScore-oracle) << ',';
                            out << (v+1 < variants.size() ? us : oracleUs);
                            out << '\n'; previous[v] = selected;
                        }
                        for (const auto& fixed : {std::pair<const char*,float> {"fixed_center",range[0]}, {"fixed_zero",0.f}})
                        {
                            const auto index = juce::roundToInt (fixed.second*5)-first;
                            const auto last = int (std::floor (std::min(30.f,range[0]+range[1])*5+.0001f));
                            if (index < 0 || index > last-first) continue;
                            const auto& metric = metrics[size_t(index)];
                            out << rate << ',' << names[size_t(corpus)] << ',' << range[0] << ',' << range[1] << ',' << fixed.first << ',' << frame << ',' << frame*2048/rate << ',' << fixed.second << ",0," << metric.spectrum << ',' << metric.sideRatio << ',' << metric.rawScore << ',' << oracle << ',' << std::max(0.f,metric.rawScore-oracle) << ",0\n";
                        }
                    }
                }
        // Exact mono sign ties: compare the actual scoring of mirrored delays.
        Access::Worker tie; Access::prepare (tie,48000);
        Access::Worker::SearchJob job; job.leftAgo.resize (3540); job.rightAgo.resize (3540);
        job.centerMs = 0; job.variabilityMs = 6;
        for (size_t i = 0; i < job.leftAgo.size(); ++i) job.leftAgo[i] = job.rightAgo[i] = signal (24000-int(i),48000,0,0);
        haas = variants.back().options; Access::process (tie,job);
        for (int i = 0; i < 61; ++i) check (std::abs (haasMetrics[size_t(i)].rawScore-haasMetrics[size_t(60-i)].rawScore) < 1e-6,"mono score cannot distinguish delay sign");
        // Reference-mid silence deliberately falls back, rather than optimizing a
        // ratio with a zero denominator. No metrics from this path are reported.
        for (size_t i = 0; i < job.leftAgo.size(); ++i) job.rightAgo[i] = -job.leftAgo[i];
        Access::process (tie,job); check (juce::exactlyEqual(tie.getSelectedDelayMs(),0.f),"anti-phase mid-silence fallback selects center");
        // Full audio path with fixed Haas: no search-thread scheduling enters
        // this separate render. The snapshot experiments above cover search.
        haas = {};
        for (int variant = 0; variant < 2; ++variant)
        {
            KuoStereoProcessor processor;
            const auto set = [&] (const char* id, float value)
            {
                auto* p = processor.parameters.getParameter(id);
                p->setValueNotifyingHost(p->convertTo0to1(value));
            };
            set("haasCenter", variant == 0 ? 0.f : 12.f); set("haasRange",0);
            set("width",variant == 0 ? 0.f : 100.f);
            set("haasKeepTransient",0); set("transientProtect",80);
            processor.prepareToPlay(48000,128);
            const int n = 6*48000, count = n+4800;
            std::vector<float> input(size_t(n),0), left(size_t(count),0), right(size_t(count),0);
            for (int i = 0; i < n; ++i) input[size_t(i)] = signal(i,48000,3,0);
            juce::MidiBuffer midi;
            for (int at = 0; at < count; at += 128)
            {
                juce::AudioBuffer<float> block(2,std::min(128,count-at)); block.clear();
                for (int i = 0; i < block.getNumSamples(); ++i)
                    if (at+i < n) for (int ch = 0; ch < 2; ++ch) block.setSample(ch,i,input[size_t(at+i)]);
                processor.processBlock(block,midi);
                for (int i = 0; i < block.getNumSamples(); ++i)
                { left[size_t(at+i)] = block.getSample(0,i); right[size_t(at+i)] = block.getSample(1,i); }
            }
            double side = 0, error = 0;
            for (int i = 0; i < n; ++i)
            {
                const auto at = size_t(i+processor.getLatencySamples());
                side += std::pow(.5*(left[at]-right[at]),2);
                error = std::max(error,std::abs(double(left[at]-input[size_t(i)])));
            }
            check(variant == 0 ? side < 1e-16 && error < 1e-5 : side > .01,
                  "neutral render preserves mono; widening generates measured side energy");
            audioCapture(directory,variant == 0 ? "audio-neutral.csv" : "audio-wide.csv",
                         input,input,left,right,48000,processor.getLatencySamples());
            processor.releaseResources();
        }
        std::cout << checks << " Haas ablation checks passed\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
