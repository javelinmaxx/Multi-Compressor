#include "PluginEditor.h"
#include "../../EditorScaling.h"
#include "../../EditorTooltips.h"
using namespace stageStyle;
namespace
{
const auto nianPanel = juce::Colour (0xff29445a);
const auto nianAccent = juce::Colour (0xff8cd8ff);
const auto nianLabel = juce::Colour (0xffc9ddec);
const auto nianGrid = juce::Colour (0xff42627b);
}
NianSaturatorEditor::NianSaturatorEditor (NianSaturatorProcessor& p) : AudioProcessorEditor (&p), signalDisplay (p)
{
    look.accentColour = nianAccent;
    look.markColour = nianLabel;
    look.trackColour = nianGrid;
    look.dialTopColour = juce::Colour (0xff41657d);
    look.dialBottomColour = juce::Colour (0xff1d3448);
    bypass.accentColour = nianAccent;
    bypass.panelColour = nianPanel;
    addAndMakeVisible (titleHint);
    addAndMakeVisible (signalDisplay);
    signalDisplay.setTooltip ("Stacks the generated-delta spectrum above the input spectrum and shows the actual output around the crossover.");
    setOpaque (true); setLookAndFeel (&look);
    look.setColour (juce::TooltipWindow::backgroundColourId, nianPanel);
    look.setColour (juce::TooltipWindow::textColourId, foreground);
    look.setColour (juce::TooltipWindow::outlineColourId, nianGrid);
    addAndMakeVisible (bypass);
    bypass.setTooltip ("Bypasses processing while preserving the plugin's latency.");
    bypassAttachment = std::make_unique<State::ButtonAttachment> (p.parameters, "bypass", bypass);
    constexpr std::array<const char*, 5> ids { "crossover", "lowDrive", "highDrive", "mix", "output" };
    constexpr std::array<const char*, 5> names { "CROSSOVER", "LOW", "HIGH", "MIX", "OUTPUT" };
    constexpr std::array<const char*, 5> units { " Hz", " %", " %", " %", " dB" };
    constexpr std::array<const char*, 5> hints {
        "Sets the frequency where the low and high bands meet.",
        "Sets the low-band peak enhancement or drive.",
        "Sets the high-band peak enhancement or drive.",
        "Blends the processed sound with the latency-aligned original.",
        "Adjusts the final stereo output level." };
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        auto& slider = sliders[i];
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxTextColourId, foreground);
        slider.setTextValueSuffix (units[i]); slider.setName (names[i]); slider.setTitle (names[i]);
        slider.setDoubleClickReturnValue (true, p.parameters.getParameter (ids[i])->convertFrom0to1 (
            p.parameters.getParameter (ids[i])->getDefaultValue()));
        labels[i].setText (names[i], juce::dontSendNotification);
        labels[i].setJustificationType (juce::Justification::centred);
        labels[i].setFont (juce::FontOptions (10.0f));
        labels[i].setColour (juce::Label::textColourId, nianLabel);
        addAndMakeVisible (slider); addAndMakeVisible (labels[i]);
        attachments[i] = std::make_unique<State::SliderAttachment> (p.parameters, ids[i], slider);
        editorTooltips::setHint (slider, labels[i], hints[i]);
    }
    mode.setColour (juce::ComboBox::backgroundColourId, nianPanel);
    mode.setColour (juce::ComboBox::textColourId, foreground);
    mode.setColour (juce::ComboBox::outlineColourId, nianGrid);
    mode.setColour (juce::ComboBox::arrowColourId, nianAccent);
    mode.addItem ("F / PEAK",1); mode.addItem ("T / DRIVE",2);
    mode.setTooltip ("Selects short twelve-band transient enhancement or dynamically softened tube-style drive.");
    addAndMakeVisible (mode); modeAttachment = std::make_unique<State::ComboBoxAttachment> (p.parameters,"mode",mode);
    editorScaling::enable (*this, designWidth, designHeight);
    gpu.attach (*this);
    signalDisplay.refresh();
}
void NianSaturatorEditor::resized()
{
    titleHint.setBounds (16, 30, 310, 36);
    bypass.setBounds (346, 30, 82, 28);
    mode.setBounds (260, 94, 150, 26);
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        const auto x = 24 + static_cast<int> (i) * 80;
        labels[i].setBounds (x, 140, 76, 16);
        sliders[i].setBounds (x, 156, 76, 76);
    }
    signalDisplay.setBounds (16, 254, 412, 162);
    const auto scale = editorScaling::transform (*this, designWidth, designHeight);
    signalDisplay.setTransform (scale);
    titleHint.setTransform (scale);
    bypass.setTransform (scale);
    mode.setTransform (scale);
    for (auto& slider : sliders) slider.setTransform (scale);
    for (auto& label : labels) label.setTransform (scale);
}
void NianSaturatorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    g.saveState();
    g.addTransform (editorScaling::transform (*this, designWidth, designHeight));
    g.setGradientFill (juce::ColourGradient (background.brighter (0.035f), 0, 0,
                                            background.darker (0.13f), 0, designHeight, false));
    g.fillRect (0, 0, designWidth, designHeight);
    g.setColour (secondary); g.setFont (juce::FontOptions (10.0f));
    g.drawText ("J-SONIC / ADAPTIVE SATURATION", 16, 12, 310, 16, juce::Justification::centredLeft);
    g.setColour (foreground); g.setFont (juce::FontOptions (25.0f));
    g.drawText ("NIAN SATURATOR", 16, 30, 310, 36, juce::Justification::centredLeft);
    drawPanel (g, { 16, 82, 412, 162 }, nianPanel);
    g.setColour (foreground); g.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
    g.drawText ("SATURATION", 28, 94, 380, 20, juce::Justification::centredLeft);
    g.setColour (nianLabel); g.setFont (juce::FontOptions (9.0f));
    g.drawText ("Twelve-band transient lift or gentle tube-style drive", 28, 116, 380, 16, juce::Justification::centredLeft);
    g.restoreState();
}
NianSaturatorEditor::SignalDisplay::SignalDisplay (NianSaturatorProcessor& p) : processor (p)
{
    for(size_t i=0;i<window.size();++i)
        window[i]=.5f-.5f*std::cos(juce::MathConstants<float>::twoPi*float(i)/float(window.size()-1));
    startTimerHz (30);
}
void NianSaturatorEditor::SignalDisplay::refresh()
{
    std::array<NianSaturatorProcessor::DisplaySample, 2048> incoming {};
    int count=0;
    for(int chunk=0;chunk<8;++chunk)
    {
        const auto read=processor.readDisplay(incoming.data(),int(incoming.size()));
        for(int i=0;i<read;++i)
        {
            history[size_t(position)]=incoming[size_t(i)];
            position=(position+1)%int(history.size());
        }
        count+=read;
        if(read<int(incoming.size())) break;
    }
    const auto frequency=processor.parameters.getRawParameterValue("crossover")->load();
    if(count>0)
    {
        for(int trace=0;trace<3;++trace)
        {
            std::array<float,4096> transform {};
            for(size_t i=0;i<history.size();++i)
            {
                const auto& point=history[size_t((position+int(i))%int(history.size()))];
                const auto value=trace==0 ? point.input : trace==1 ? point.delta : point.input+point.delta;
                transform[i]=value*window[i];
            }
            fft.performFrequencyOnlyForwardTransform(transform.data());
            for(size_t bin=1;bin<magnitudes[size_t(trace)].size();++bin)
            {
                const auto value=transform[bin];
                auto& displayed=magnitudes[size_t(trace)][bin];
                const auto weight=value>displayed ? .28f : .11f;
                displayed=received==0 ? value : displayed+weight*(value-displayed);
            }
        }
        received=std::min(received+count,int(history.size()));
    }
    if(count>0 || std::abs(frequency-lastCrossover)>.5f)
    {
        lastCrossover=frequency;
        repaint();
    }
}
void NianSaturatorEditor::SignalDisplay::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), nianPanel);
    g.setColour (foreground); g.setFont (juce::FontOptions (13.f).withStyle ("bold"));
    g.drawText ("SPECTRUM",12,8,150,19,juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (9.f));
    g.setColour (nianAccent); g.drawText ("IN",235,12,25,13,juce::Justification::centredLeft);
    g.setColour (amber); g.drawText ("DELTA",268,12,45,13,juce::Justification::centredLeft);
    g.setColour (foreground); g.drawText ("OUT",328,12,44,13,juce::Justification::centredLeft);
    const auto rate=processor.getDisplaySampleRate();
    const auto maxHz=std::min(20000.f,rate*.48f);
    const auto crossover=std::clamp(processor.parameters.getRawParameterValue("crossover")->load(),80.f,8000.f);
    const auto left=20.f,right=float(getWidth()-20),top=52.f,bottom=132.f,width=right-left;
    const auto xFor=[&](float hz){return left+width*std::clamp(std::log(hz/40.f)/std::log(maxHz/40.f),0.f,1.f);};
    const auto marker=xFor(crossover);
    g.setColour (amber.withAlpha(.045f)); g.fillRect(left,top,marker-left,bottom-top);
    g.setColour (foreground.withAlpha(.025f)); g.fillRect(marker,top,right-marker,bottom-top);
    for(int db=-96;db<=0;db+=24)
    {
        const auto y=bottom-float(db+96)/96.f*(bottom-top);
        g.setColour(nianGrid.withAlpha(.55f)); g.drawLine(left,y,right,y,.7f);
    }
    for(const float hz : {100.f,1000.f,10000.f})
        if(hz<maxHz) { const auto x=xFor(hz); g.setColour(nianGrid.withAlpha(.35f)); g.drawLine(x,top,x,bottom,.7f); }
    const auto magnitudeAt=[&](int trace,float center)
    {
        const auto nearest=int(std::floor(center));
        float weighted=0, total=0;
        for(int offset=-3;offset<=3;++offset)
        {
            const auto bin=nearest+offset;
            if(bin<1 || bin>1024) continue;
            const auto distance=std::abs(float(bin)-center);
            const auto taper=std::max(0.f,3.f-distance);
            const auto weight=taper*taper;
            weighted+=weight*magnitudes[size_t(trace)][size_t(bin)];
            total+=weight;
        }
        return total>0 ? weighted/total : 0.f;
    };
    const auto yFor=[&](float value)
    {
        const auto db=20.f*std::log10(std::max(value/512.f,1.e-6f));
        return bottom-std::clamp((db+96.f)/96.f,0.f,1.f)*(bottom-top);
    };
    juce::Path inputPath,stackPath,outputPath,deltaArea;
    std::vector<float> inputY;
    inputY.reserve(size_t(width)+1);
    for(int pixel=0;pixel<=int(width);++pixel)
    {
        const auto x=left+float(pixel);
        const auto hz=40.f*std::pow(maxHz/40.f,float(pixel)/width);
        const auto center=hz*2048.f/rate;
        const auto input=magnitudeAt(0,center), delta=magnitudeAt(1,center);
        const auto inY=yFor(input), stackedY=yFor(input+delta), outY=yFor(magnitudeAt(2,center));
        inputY.push_back(inY);
        if(pixel==0)
        { inputPath.startNewSubPath(x,inY); stackPath.startNewSubPath(x,stackedY);
          outputPath.startNewSubPath(x,outY); deltaArea.startNewSubPath(x,stackedY); }
        else
        { inputPath.lineTo(x,inY); stackPath.lineTo(x,stackedY);
          outputPath.lineTo(x,outY); deltaArea.lineTo(x,stackedY); }
    }
    for(int pixel=int(width);pixel>=0;--pixel)
        deltaArea.lineTo(left+float(pixel),inputY[size_t(pixel)]);
    deltaArea.closeSubPath();
    g.setColour(amber.withAlpha(.18f)); g.fillPath(deltaArea);
    g.setColour(nianAccent.withAlpha(.85f)); g.strokePath(inputPath,juce::PathStrokeType(1.f));
    g.setColour(amber.withAlpha(.95f)); g.strokePath(stackPath,juce::PathStrokeType(1.3f));
    g.setColour(foreground.withAlpha(.95f)); g.strokePath(outputPath,juce::PathStrokeType(1.25f));
    g.setColour(amber); g.drawLine(marker,top,marker,bottom,1.f);
    g.setFont (juce::FontOptions (9.f));
    g.setColour (nianLabel); g.drawText ("LOW / 40 Hz",20,138,85,13,juce::Justification::centredLeft);
    g.drawText ("HIGH / "+juce::String(maxHz/1000.f,1)+" kHz",int(right-103),138,103,13,juce::Justification::centredRight);
    g.setColour (amber); g.drawText (juce::String(int(crossover))+" Hz",int(std::clamp(marker-28,left+85,right-153)),138,56,13,juce::Justification::centred);
}
