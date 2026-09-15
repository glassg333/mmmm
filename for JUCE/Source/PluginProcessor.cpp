#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MonomachineNovaAudioProcessor::MonomachineNovaAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

MonomachineNovaAudioProcessor::~MonomachineNovaAudioProcessor()
{
}

//==============================================================================
const juce::String MonomachineNovaAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MonomachineNovaAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MonomachineNovaAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MonomachineNovaAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MonomachineNovaAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MonomachineNovaAudioProcessor::getNumPrograms()
{
    return 1;
}

int MonomachineNovaAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MonomachineNovaAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String MonomachineNovaAudioProcessor::getProgramName (int index)
{
    return {};
}

void MonomachineNovaAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void MonomachineNovaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Инициализация всех модулей с частотой дискретизации.
    // Сигнатуры методов взяты из реального кода репозитория.
    voiceChain.reset(sampleRate);
    fmSynth.reset(sampleRate);
    arpeggiator.reset(sampleRate, 120.0); // 120 BPM по умолчанию
    modMatrix.reset();
}

void MonomachineNovaAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MonomachineNovaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void MonomachineNovaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Чистим лишние выходные каналы
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // === 1. Обработка MIDI ===
    // Методы noteOn/noteOff для каждого модуля взяты из реального кода:
    // MonomachineFmDynamic::noteOn(uint8_t midiNote, uint8_t velocity)
    // MonomachineVoiceChain::noteOn(uint8_t note, uint8_t velocity)
    // MonomachineArpeggiator::noteOn(uint8_t note, uint8_t velocity)
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            fmSynth.noteOn(msg.getNoteNumber(), msg.getVelocity());
            voiceChain.noteOn(msg.getNoteNumber(), msg.getVelocity());
            arpeggiator.noteOn(msg.getNoteNumber(), msg.getVelocity());
        }
        else if (msg.isNoteOff())
        {
            fmSynth.noteOff();
            voiceChain.noteOff();
            arpeggiator.noteOff(msg.getNoteNumber());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            fmSynth.noteOff();
            voiceChain.noteOff();
        }
    }

    // === 2. Генерация звука ===
    const int numSamples = buffer.getNumSamples();
    auto* channelLeft  = buffer.getWritePointer(0);
    auto* channelRight = (totalNumOutputChannels > 1) ? buffer.getWritePointer(1) : channelLeft;

    // MonomachineFmDynamic::processStereo(float* outL, float* outR, size_t numFrames)
    fmSynth.processStereo(channelLeft, channelRight, static_cast<size_t>(numSamples));

    // === 3. Цепочка обработки ===
    // MonomachineVoiceChain::processBlock(
    //     const float* inL, const float* inR,
    //     float* outL, float* outR, size_t numFrames)
    voiceChain.processBlock(channelLeft, channelRight,
                            channelLeft, channelRight,
                            static_cast<size_t>(numSamples));
}

//==============================================================================
bool MonomachineNovaAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* MonomachineNovaAudioProcessor::createEditor()
{
    return new MonomachineNovaAudioProcessorEditor (*this);
}

//==============================================================================
void MonomachineNovaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
}

void MonomachineNovaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MonomachineNovaAudioProcessor();
}