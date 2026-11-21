#pragma once

#include "PluginProcessor.h"

// placeholder until the real UI is in place
class AuricMaster2Editor : public juce::GenericAudioProcessorEditor
{
public:
    explicit AuricMaster2Editor (AuricMaster2Processor& p) : juce::GenericAudioProcessorEditor (p) {}
};
