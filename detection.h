#pragma once
// =============================================================================
// DETECTION ALGORITHM FUNCTIONS
// =============================================================================

#include "mouse_device.h"

// Detection algorithms
bool DetectContinuousMovement(MouseDevice& device, DetectionResult& result);
bool DetectLargeRepetitiveMovement(MouseDevice& device, DetectionResult& result);
bool DetectConstantSpeed(MouseDevice& device, DetectionResult& result);
bool FitCircleToPoints(const POINT* points, size_t count, double& cx, double& cy, double& r);
bool DetectArcPattern(MouseDevice& device, DetectionResult& result);
bool DetectGeometricPattern(MouseDevice& device, DetectionResult& result);
bool DetectCircularPattern(MouseDevice& device, DetectionResult& result);
bool DetectOscillationPattern(MouseDevice& device, DetectionResult& result);
bool DetectConstantDelta(MouseDevice& device, DetectionResult& result);
bool DetectAlternatingPattern(MouseDevice& device, DetectionResult& result);

// Behavioral suspicion scoring
int CalculateBehaviorSuspicionScore(MouseDevice& device, DetectionResult& result);

// Primary mouse wheel-jiggler detection
bool DetectPrimaryMouseWheelPattern(MouseDevice& device, DetectionResult& result);

// Primary mouse zigzag (linear/sawtooth) jiggler detection
bool DetectPrimaryMouseZigzagPattern(MouseDevice& device, DetectionResult& result);

// Primary mouse repetitive micro-delta jiggler detection
bool DetectPrimaryMouseRepetitiveDelta(MouseDevice& device, DetectionResult& result);
