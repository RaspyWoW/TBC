
#pragma once

#include <unordered_map>
#include <cstdint>
#include <ctime>

class WhisperTargetLimits
{
    const std::uint32_t account_id_;
    const std::uint32_t max_targets_;
    const std::uint32_t bypass_level_;
    const std::uint32_t decay_;
    std::unordered_map<std::uint32_t, std::uint64_t> targets_;

    void LoadTargets();
    void SaveTargets() const;
    void TargetDecay(const std::time_t& time);

public:
    WhisperTargetLimits(std::uint32_t account_id, std::uint32_t max_targets, std::uint32_t bypass_level, std::uint32_t decay);
    ~WhisperTargetLimits();

    bool CanWhisper(const std::uint32_t target_guid, const std::uint32_t level);
};
