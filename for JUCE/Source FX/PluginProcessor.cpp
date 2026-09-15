#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MonomachineNovaAudioProcessor::MonomachineNovaAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       )
#endif
{
}

MonomachineNovaAudioProcessor::~MonomachineNovaAudioProcessor() {}

//==============================================================================
const juce::String MonomachineNovaAudioProcessor::getName() const { return JucePlugin_Name; }
bool MonomachineNovaAudioProcessor::acceptsMidi() const { return false; } // FX не нужен MIDI
bool MonomachineNovaAudioProcessor::producesMidi() const { return false; }
bool MonomachineNovaAudioProcessor::isMidiEffect() const { return false; }
double MonomachineNovaAudioProcessor::getTailLengthSeconds() const { return 2.0; } // хвост delay

int MonomachineNovaAudioProcessor::getNumPrograms() { return 1; }
int MonomachineNovaAudioProcessor::getCurrentProgram() { return 0; }
void MonomachineNovaAudioProcessor::setCurrentProgram (int index) {}
const juce::String MonomachineNovaAudioProcessor::getProgramName (int index) { return {}; }
void MonomachineNovaAudioProcessor::changeProgramName (int index, const juce::String& newName) {}

//==============================================================================
void MonomachineNovaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    voiceChain.reset(sampleRate);

    // ⚠️ ВАЖНО: в FX-режиме огибающая усиления должна быть всегда открыта,
    // иначе звук не пройдёт через цепочку.
    // Проверь в monomachine_voice_chain.hpp, есть ли такой метод:
    // voiceChain.setAmpEnvelopeAlwaysOpen(true);
    //
    // Если такого метода нет — надо добавить его в voiceChain,
    // либо вызвать voiceChain.noteOn() один раз в конструкторе,
    // чтобы огибающая осталась в sustain.
}

void MonomachineNovaAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MonomachineNovaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // FX требует: вход стерео → выход стерео (или моно → моно)
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

//==============================================================================
void MonomachineNovaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages); // MIDI не используем

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels < 1 || numSamples == 0)
        return;

    auto* channelLeft  = buffer.getWritePointer(0);
    auto* channelRight = (numChannels > 1) ? buffer.getWritePointer(1) : channelLeft;

    // Пропускаем входящий звук через всю цепочку эффектов:
    // Overdrive → Filter → EQ → Amp → Delay → Chorus
    voiceChain.processBlock(channelLeft, channelRight,
                            channelLeft, channelRight,
                            static_cast<size_t>(numSamples));
}

//==============================================================================
bool MonomachineNovaAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* MonomachineNovaAudioProcessor::createEditor()
{
    return new MonomachineNovaAudioProcessorEditor (*this);
}

//==============================================================================
void MonomachineNovaAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {}
void MonomachineNovaAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MonomachineNovaAudioProcessor();
}