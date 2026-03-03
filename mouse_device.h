#pragma once
// =============================================================================
// MOUSE DEVICE DATA STRUCTURES
// =============================================================================

#define NOMINMAX
#include <windows.h>
#include <string>
#include <deque>
#include <chrono>
#include <cmath>

#include "device_ids.h"
#include "detection_constants.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Forward declaration (defined in device_utils.cpp)
void ExtractVidPid(const std::string& deviceName, std::string& vid, std::string& pid);

// Detection types
enum class DetectionType {
    NONE,
    SMALL_MOVEMENT,
    LARGE_REPETITIVE_MOVEMENT,
    CONTINUOUS_MOVEMENT,
    CONSTANT_SPEED,
    CIRCULAR_PATTERN,
    CIRCULAR_ARC_PATTERN,
    OSCILLATION_PATTERN,
    GEOMETRIC_PATTERN,
    CONSTANT_DELTA,
    ALTERNATING_PATTERN,
    ZIGZAG_PATTERN,
    REPETITIVE_DELTA
};

struct DetectionResult {
    DetectionType type = DetectionType::NONE;
    std::string reason;
    double value = 0.0; // for storing specific values like duration, speed variance, etc.
};

// Represents a detected arc segment from circle-fitting a sliding window
struct ArcSegment {
    double radius;      // Fitted circle radius
    double centerX;     // Fitted circle center X
    double centerY;     // Fitted circle center Y
    double arcAngle;    // Angular span of the arc (degrees)
    double radiusCV;    // Coefficient of variation of radii in this window
};

class MouseDevice {
public:
    std::string name;
    std::string manufacturer;
    std::string product;
    std::string vid;  // Extracted VID
    std::string pid;  // Extracted PID
    std::deque<POINT> movements;
    bool isTouchpad = false;
    bool usageTouchpadHint = false;
    bool nameTouchpadHint = false;
    int absoluteMoveCount = 0;

    // VID-based trust scoring (NEW)
    VIDTrustLevel vidTrustLevel = VIDTrustLevel::UNKNOWN;
    int vidSuspicionScore = 30;  // Default to moderate suspicion for unknown VIDs

    // Behavioral detection scoring (NEW)
    int behaviorSuspicionScore = 0;  // 0-100 based on movement patterns
    int combinedSuspicionScore = 0;  // Combined VID + behavior score

    // Detection state (NEW)
    bool isJiggler = false;
    std::string detectionReason;
    std::string verdictLabel;  // "Trusted", "Suspicious", "Jiggler Detected"
    DetectionType detectionType = DetectionType::NONE;  // Pattern that triggered detection

    // New fields for advanced detection
    std::deque<double> speedHistory;  // Rolling window of speeds
    std::chrono::steady_clock::time_point lastMovementTime;
    std::chrono::steady_clock::time_point firstMovementTime;  // When tracking started for this device
    double continuousDuration = 0.0;  // in seconds
    std::deque<POINT> patternHistory;  // For circular/oscillation pattern detection
    std::deque<double> angleHistory;   // For circular pattern detection
    std::deque<int> directionXHistory; // For oscillation detection
    std::deque<int> directionYHistory; // For oscillation detection
    int reversalCountX = 0;
    int reversalCountY = 0;

    // Arc-based detection (partial arc / wheel jiggler)
    std::deque<ArcSegment> arcHistory;  // Rolling history of detected arc segments

    // Primary mouse sustained-pattern confirmation
    int primaryConsecutiveHighScores = 0;  // Consecutive cycles above threshold
    int primaryZigzagConsecutiveHighScores = 0;  // Consecutive cycles qualifying for zigzag
    int primaryRepetitiveConsecutiveScores = 0;  // Consecutive cycles qualifying for repetitive micro-delta
    std::chrono::steady_clock::time_point lastSeenMovementTime;  // Last lastMovementTime seen by detection loop

    MouseDevice() = default;
    MouseDevice(const std::string& name, bool isTouchpad = false) : name(name), isTouchpad(isTouchpad) {
        lastMovementTime = std::chrono::steady_clock::now();
        // Extract VID/PID and calculate trust level
        ExtractVidPid(name, vid, pid);
        if (!vid.empty()) {
            vidTrustLevel = GetVIDTrustLevel(vid);
            vidSuspicionScore = CalculateVIDSuspicionScore(vid);
        }
    }

    // Update the combined suspicion score
    void UpdateCombinedScore() {
        // VID contributes 40%, behavior contributes 60%
        combinedSuspicionScore = (vidSuspicionScore * 40 + behaviorSuspicionScore * 60) / 100;
    }

    // Get a human-readable trust label
    std::string GetTrustLabel() const {
        if (isJiggler) return "Jiggler Detected";
        if (combinedSuspicionScore >= 70) return "Highly Suspicious";
        if (combinedSuspicionScore >= 50) return "Suspicious";
        if (vidTrustLevel == VIDTrustLevel::ALLOWLIST && behaviorSuspicionScore < 30) return "Trusted Device";
        if (vidTrustLevel == VIDTrustLevel::UNKNOWN) return "Unknown Device";
        return "Analyzing...";
    }
};
