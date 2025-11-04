#include "BWFMetadata.h"
#include "../Audio/AudioFileManager.h"

BWFMetadata::BWFMetadata()
    : m_timeReference(0)
{
}

bool BWFMetadata::loadFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
    {
        juce::Logger::writeToLog("BWFMetadata::loadFromFile() - File does not exist: " + file.getFullPathName());
        return false;
    }

    // Create format manager and register WAV format
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    // Create reader for the file
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader == nullptr)
    {
        juce::Logger::writeToLog("BWFMetadata::loadFromFile() - Failed to create reader for: " + file.getFullPathName());
        return false;
    }

    // Load metadata from reader
    fromJUCEMetadata(reader->metadataValues);

    bool hasAnyMetadata = hasMetadata();
    if (hasAnyMetadata)
    {
        juce::Logger::writeToLog("BWFMetadata::loadFromFile() - Successfully loaded BWF metadata from: " + file.getFullPathName());
    }

    return hasAnyMetadata;
}

juce::StringPairArray BWFMetadata::toJUCEMetadata() const
{
    juce::StringPairArray metadata;

    // Only add non-empty fields to metadata
    if (m_description.isNotEmpty())
        metadata.set(juce::WavAudioFormat::bwavDescription, m_description);

    if (m_originator.isNotEmpty())
        metadata.set(juce::WavAudioFormat::bwavOriginator, m_originator);

    if (m_originatorRef.isNotEmpty())
        metadata.set(juce::WavAudioFormat::bwavOriginatorRef, m_originatorRef);

    if (m_originationDate.isNotEmpty())
        metadata.set(juce::WavAudioFormat::bwavOriginationDate, m_originationDate);

    if (m_originationTime.isNotEmpty())
        metadata.set(juce::WavAudioFormat::bwavOriginationTime, m_originationTime);

    // Time reference is always included (even if 0)
    metadata.set(juce::WavAudioFormat::bwavTimeReference, juce::String(m_timeReference));

    if (m_codingHistory.isNotEmpty())
        metadata.set(juce::WavAudioFormat::bwavCodingHistory, m_codingHistory);

    return metadata;
}

void BWFMetadata::fromJUCEMetadata(const juce::StringPairArray& metadata)
{
    // Clear existing metadata
    clear();

    // Load all BWF fields from metadata
    m_description = metadata.getValue(juce::WavAudioFormat::bwavDescription, "");
    m_originator = metadata.getValue(juce::WavAudioFormat::bwavOriginator, "");
    m_originatorRef = metadata.getValue(juce::WavAudioFormat::bwavOriginatorRef, "");
    m_originationDate = metadata.getValue(juce::WavAudioFormat::bwavOriginationDate, "");
    m_originationTime = metadata.getValue(juce::WavAudioFormat::bwavOriginationTime, "");

    juce::String timeRefStr = metadata.getValue(juce::WavAudioFormat::bwavTimeReference, "0");
    m_timeReference = timeRefStr.getLargeIntValue();

    m_codingHistory = metadata.getValue(juce::WavAudioFormat::bwavCodingHistory, "");
}

void BWFMetadata::setOriginationDateTime(const juce::Time& time)
{
    // Format date as yyyy-mm-dd
    m_originationDate = juce::String::formatted("%04d-%02d-%02d",
                                                  time.getYear(),
                                                  time.getMonth() + 1,  // JUCE months are 0-based
                                                  time.getDayOfMonth());

    // Format time as hh:mm:ss
    m_originationTime = juce::String::formatted("%02d:%02d:%02d",
                                                  time.getHours(),
                                                  time.getMinutes(),
                                                  time.getSeconds());
}

juce::Time BWFMetadata::getOriginationDateTime() const
{
    if (m_originationDate.isEmpty() || m_originationTime.isEmpty())
        return juce::Time();

    // Parse date (yyyy-mm-dd)
    juce::StringArray dateParts = juce::StringArray::fromTokens(m_originationDate, "-", "");
    if (dateParts.size() != 3)
        return juce::Time();

    int year = dateParts[0].getIntValue();
    int month = dateParts[1].getIntValue() - 1;  // JUCE months are 0-based
    int day = dateParts[2].getIntValue();

    // Parse time (hh:mm:ss)
    juce::StringArray timeParts = juce::StringArray::fromTokens(m_originationTime, ":", "");
    if (timeParts.size() != 3)
        return juce::Time();

    int hours = timeParts[0].getIntValue();
    int minutes = timeParts[1].getIntValue();
    int seconds = timeParts[2].getIntValue();

    // Create Time object (use local time to match getHours() behavior)
    return juce::Time(year, month, day, hours, minutes, seconds, 0, true);
}

bool BWFMetadata::hasMetadata() const
{
    return m_description.isNotEmpty() ||
           m_originator.isNotEmpty() ||
           m_originatorRef.isNotEmpty() ||
           m_originationDate.isNotEmpty() ||
           m_originationTime.isNotEmpty() ||
           m_timeReference != 0 ||
           m_codingHistory.isNotEmpty();
}

void BWFMetadata::clear()
{
    m_description.clear();
    m_originator.clear();
    m_originatorRef.clear();
    m_originationDate.clear();
    m_originationTime.clear();
    m_timeReference = 0;
    m_codingHistory.clear();
}

BWFMetadata BWFMetadata::createDefault(const juce::String& description)
{
    BWFMetadata metadata;

    // Set description
    if (description.isNotEmpty())
        metadata.setDescription(description);

    // Set originator to WaveEdit by ZQ SFX
    metadata.setOriginator("ZQ SFX WaveEdit");

    // Set current date/time
    metadata.setOriginationDateTime(juce::Time::getCurrentTime());

    // Add WaveEdit to coding history
    metadata.setCodingHistory("A=PCM,F=44100,W=16,M=stereo,T=ZQ SFX WaveEdit");

    return metadata;
}

juce::String BWFMetadata::formatDate(const juce::String& date)
{
    // Expected format: yyyy-mm-dd (10 characters)
    if (date.length() != 10)
        return "";

    // Validate format with regex or simple checks
    if (date[4] != '-' || date[7] != '-')
        return "";

    // Validate year, month, day are numeric
    juce::String year = date.substring(0, 4);
    juce::String month = date.substring(5, 7);
    juce::String day = date.substring(8, 10);

    if (!year.containsOnly("0123456789") ||
        !month.containsOnly("0123456789") ||
        !day.containsOnly("0123456789"))
        return "";

    // Validate ranges
    int monthInt = month.getIntValue();
    int dayInt = day.getIntValue();

    if (monthInt < 1 || monthInt > 12 || dayInt < 1 || dayInt > 31)
        return "";

    return date;
}

juce::String BWFMetadata::formatTime(const juce::String& time)
{
    // Expected format: hh:mm:ss (8 characters)
    if (time.length() != 8)
        return "";

    // Validate format with regex or simple checks
    if (time[2] != ':' || time[5] != ':')
        return "";

    // Validate hour, minute, second are numeric
    juce::String hour = time.substring(0, 2);
    juce::String minute = time.substring(3, 5);
    juce::String second = time.substring(6, 8);

    if (!hour.containsOnly("0123456789") ||
        !minute.containsOnly("0123456789") ||
        !second.containsOnly("0123456789"))
        return "";

    // Validate ranges
    int hourInt = hour.getIntValue();
    int minuteInt = minute.getIntValue();
    int secondInt = second.getIntValue();

    if (hourInt < 0 || hourInt > 23 ||
        minuteInt < 0 || minuteInt > 59 ||
        secondInt < 0 || secondInt > 59)
        return "";

    return time;
}
