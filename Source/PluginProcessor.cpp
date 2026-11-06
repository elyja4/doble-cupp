#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset { const char* name; float spill, amount, speed, fill, mix; };
    const Preset presets[] = {
        { "Init - Full Cup",   0.f,   100.f, 1.0f,  12.f, 100.f },
        { "Sip",               15.f,  40.f,  1.0f,  12.f, 100.f },
        { "Tipsy",             40.f,  70.f,  1.5f,  10.f, 100.f },
        { "Slow Drip",         25.f,  100.f, 0.4f,  8.f,  100.f },
        { "Wobble Juice",      55.f,  55.f,  3.0f,  6.f,  100.f },
        { "Muddy Puddle",      75.f,  100.f, 2.0f,  16.f, 85.f  },
        { "Full Spill",        100.f, 100.f, 1.0f,  12.f, 100.f }
    };
}

int DoblecuppProcessor::numPresets() { return (int) (sizeof(presets) / sizeof(presets[0])); }
juce::String DoblecuppProcessor::presetName(int i) { return presets[juce::jlimit(0, numPresets() - 1, i)].name; }

DoblecuppProcessor::DoblecuppProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createLayout())
{
    apvts.state.setProperty("program", 0, nullptr);
}

bool DoblecuppProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void DoblecuppProcessor::setCurrentProgram(int index)
{
    index = juce::jlimit(0, numPresets() - 1, index);
    const auto& p = presets[index];
    auto set = [this](const char* id, float v)
    {
        if (auto* prm = apvts.getParameter(id)) prm->setValueNotifyingHost(prm->convertTo0to1(v));
    };
    set(Param::spill, p.spill); set(Param::amount, p.amount); set(Param::speed, p.speed);
    set(Param::fill, p.fill);   set(Param::mix, p.mix);
    apvts.state.setProperty("program", index, nullptr);
}

void DoblecuppProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    svf.prepare(spec);
    svf.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    svf.reset();
    cutoffSm.reset(sampleRate, 0.03);
    cutoffSm.setCurrentAndTargetValue(20000.f);

    delaySize = (int) (sampleRate * 0.02) + 4;
    for (auto& d : delay) d.assign((size_t) delaySize, 0.f);
    writePos = 0; lfoPhase = 0.f;
    centreDelay = (int) (sampleRate * 0.004);
    setLatencySamples(centreDelay);     // la línea de retardo fija (4 ms) se informa al host
    spillSmooth = apvts.getRawParameterValue(Param::spill)->load() * 0.01f;
}

void DoblecuppProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nc = juce::jmin(buffer.getNumChannels(), 2);
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch) buffer.clear(ch, 0, n);

    auto P = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    inLevel.store(buffer.getMagnitude(0, n));

    // --- posición del control, suavizada según SPEED (la usan el audio y la animación)
    const float target = P(Param::spill) * 0.01f;
    const float tau = 0.18f / juce::jmax(0.1f, P(Param::speed));
    spillSmooth += (target - spillSmooth) * (1.f - std::exp(-(float) n / ((float) sr * tau)));
    const float amount = P(Param::amount) * 0.01f, mix = P(Param::mix) * 0.01f;
    const float eff = juce::jlimit(0.f, 1.f, spillSmooth * amount);   // fracción derramada

    visSpill.store(spillSmooth);
    visSpilled.store(eff);
    visIntensity.store(eff * mix);

    // --- efecto: cuanto más líquido se derrama, más "sumergido" suena:
    //     paso bajo que baja de ~20 kHz a ~350 Hz, vibrato líquido (profundidad y velocidad crecen)
    //     y pérdida de nivel de hasta -6 dB. MIX mezcla con la señal seca (misma latencia).
    const float fc = 20000.f * std::pow(350.f / 20000.f, std::pow(eff, 0.75f));
    cutoffSm.setTargetValue(juce::jlimit(60.f, 0.45f * (float) sr, fc));
    const float depth = eff * 0.0025f * (float) sr;       // ±2.5 ms a derrame total
    const float rate = 0.8f + eff * 3.2f;                  // Hz
    const float gain = juce::Decibels::decibelsToGain(-6.f * eff);
    svf.setResonance(0.7f + eff * 1.3f);

    for (int i = 0; i < n; ++i)
    {
        if ((i & 7) == 0) svf.setCutoffFrequency(cutoffSm.getNextValue());
        else cutoffSm.skip(1);

        for (int ch = 0; ch < nc; ++ch)
        {
            float* d = buffer.getWritePointer(ch);
            auto& line = delay[(size_t) ch];
            line[(size_t) writePos] = d[i];

            int dryIdx = writePos - centreDelay; if (dryIdx < 0) dryIdx += delaySize;
            const float dry = line[(size_t) dryIdx];

            const float lfo = std::sin(juce::MathConstants<float>::twoPi * (lfoPhase + (ch == 1 ? 0.25f : 0.f)));
            float rp = (float) writePos - ((float) centreDelay + lfo * depth);
            while (rp < 0.f) rp += (float) delaySize;
            const int i0 = (int) rp % delaySize, i1 = (i0 + 1) % delaySize;
            const float fr = rp - std::floor(rp);
            const float wetIn = line[(size_t) i0] * (1.f - fr) + line[(size_t) i1] * fr;

            const float wet = svf.processSample(ch, wetIn) * gain;
            d[i] = dry * (1.f - mix) + wet * mix;
        }
        writePos = (writePos + 1) % delaySize;
        lfoPhase += rate / (float) sr;
        if (lfoPhase >= 1.f) lfoPhase -= 1.f;
    }
    outLevel.store(buffer.getMagnitude(0, n));
}

juce::AudioProcessorEditor* DoblecuppProcessor::createEditor() { return new DoblecuppEditor(*this); }

void DoblecuppProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void DoblecuppProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DoblecuppProcessor(); }
