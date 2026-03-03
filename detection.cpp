// =============================================================================
// DETECTION ALGORITHM FUNCTIONS - Implementation
// =============================================================================

#define NOMINMAX
#include <windows.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

#include "detection.h"
#include "detection_constants.h"

// 1. Continuous Movement Detection
bool DetectContinuousMovement(MouseDevice& device, DetectionResult& result) {
    if (device.continuousDuration >= CONTINUOUS_THRESHOLD_SECONDS) {
        result.type = DetectionType::CONTINUOUS_MOVEMENT;
        result.value = device.continuousDuration;
        int minutes = (int)(device.continuousDuration / 60.0);
        std::ostringstream oss;
        oss << "Jiggler Detected: Continuous movement for " << minutes << " minute" << (minutes > 1 ? "s" : "");
        result.reason = oss.str();
        return true;
    }
    return false;
}

// 2. Large Repetitive Movement Detection
bool DetectLargeRepetitiveMovement(MouseDevice& device, DetectionResult& result) {
    if (device.movements.size() < MIN_REPETITIVE_SAMPLES) {
        return false;
    }
    
    // Check for repetitive patterns in larger movements
    // Count how many movements are similar to each other
    std::vector<std::pair<LONG, LONG>> deltas;
    for (size_t i = 1; i < device.movements.size() && i < MAX_REPETITIVE_CHECK_SAMPLES; ++i) {
        LONG dx = device.movements[i].x - device.movements[i-1].x;
        LONG dy = device.movements[i].y - device.movements[i-1].y;
        
        // Only consider movements larger than small threshold
        if (abs(dx) > LARGE_MOVEMENT_THRESHOLD || abs(dy) > LARGE_MOVEMENT_THRESHOLD) {
            deltas.push_back({dx, dy});
        }
    }
    
    if (deltas.size() < MIN_REPETITIVE_SAMPLES) {
        return false;
    }
    
    // Check if many deltas are similar to each other (repetitive pattern)
    int similarCount = 0;
    for (size_t i = 0; i < deltas.size(); ++i) {
        for (size_t j = i + 1; j < deltas.size(); ++j) {
            double dist = sqrt(pow(deltas[i].first - deltas[j].first, 2.0) + 
                             pow(deltas[i].second - deltas[j].second, 2.0));
            double avgMagnitude = sqrt(pow(deltas[i].first, 2.0) + pow(deltas[i].second, 2.0) +
                                      pow(deltas[j].first, 2.0) + pow(deltas[j].second, 2.0)) / 2.0;
            
            if (avgMagnitude > 0 && (dist / avgMagnitude) < REPETITIVE_TOLERANCE) {
                similarCount++;
            }
        }
    }
    
    // If we found many similar pairs, it's likely a jiggler with larger movements
    int totalPairs = (deltas.size() * (deltas.size() - 1)) / 2;
    double similarityRatio = totalPairs > 0 ? (double)similarCount / totalPairs : 0.0;
    
    if (similarityRatio > REPETITIVE_SIMILARITY_THRESHOLD) {
        result.type = DetectionType::LARGE_REPETITIVE_MOVEMENT;
        result.value = similarityRatio;
        result.reason = "Jiggler Detected: Repetitive movement pattern";
        return true;
    }
    
    return false;
}

// 3. Constant Speed Detection
bool DetectConstantSpeed(MouseDevice& device, DetectionResult& result) {
    if (device.speedHistory.size() < MIN_SPEED_SAMPLES) {
        return false;
    }
    
    // Calculate mean
    double sum = 0.0;
    for (double speed : device.speedHistory) {
        sum += speed;
    }
    double mean = sum / device.speedHistory.size();
    
    if (mean < 0.1) return false; // Ignore if speed is too low
    
    // Calculate standard deviation
    double variance = 0.0;
    for (double speed : device.speedHistory) {
        variance += (speed - mean) * (speed - mean);
    }
    variance /= device.speedHistory.size();
    double stddev = sqrt(variance);
    
    // Calculate coefficient of variation
    double cv = stddev / mean;
    
    if (cv < SPEED_CV_THRESHOLD) {
        result.type = DetectionType::CONSTANT_SPEED;
        result.value = cv;
        std::ostringstream oss;
        oss << "Jiggler Detected: Constant speed anomaly (CV: " << std::fixed << std::setprecision(2) << (cv * 100) << "%)";
        result.reason = oss.str();
        return true;
    }
    return false;
}

// Fit a circle through a set of points using least-squares algebraic fit.
// Returns true if a valid circle was found. Outputs center (cx, cy) and radius r.
// Uses the Kasa method: minimize sum of (x^2 + y^2 - 2*cx*x - 2*cy*y - (r^2 - cx^2 - cy^2))^2
bool FitCircleToPoints(const POINT* points, size_t count, double& cx, double& cy, double& r) {
    if (count < 3) return false;

    double sumX = 0, sumY = 0, sumX2 = 0, sumY2 = 0, sumXY = 0;
    double sumX3 = 0, sumY3 = 0, sumX2Y = 0, sumXY2 = 0;

    for (size_t i = 0; i < count; i++) {
        double x = (double)points[i].x;
        double y = (double)points[i].y;
        double x2 = x * x;
        double y2 = y * y;
        sumX += x;
        sumY += y;
        sumX2 += x2;
        sumY2 += y2;
        sumXY += x * y;
        sumX3 += x2 * x;
        sumY3 += y2 * y;
        sumX2Y += x2 * y;
        sumXY2 += x * y2;
    }

    double n = (double)count;
    double A = n * sumX2 - sumX * sumX;
    double B = n * sumXY - sumX * sumY;
    double C = n * sumY2 - sumY * sumY;
    double D = 0.5 * (n * (sumX3 + sumXY2) - sumX * (sumX2 + sumY2));
    double E = 0.5 * (n * (sumX2Y + sumY3) - sumY * (sumX2 + sumY2));

    double denom = A * C - B * B;
    if (fabs(denom) < 1e-10) return false; // Points are collinear

    cx = (D * C - B * E) / denom;
    cy = (A * E - B * D) / denom;
    r = 0.0;
    for (size_t i = 0; i < count; i++) {
        double dx = (double)points[i].x - cx;
        double dy = (double)points[i].y - cy;
        r += sqrt(dx * dx + dy * dy);
    }
    r /= n;

    return r > 0.0;
}

// 4a. Arc-Based Circular Pattern Detection (partial arcs / wheel jigglers)
// Uses sliding-window circle fitting to detect repeated arcs with consistent radius,
// without requiring a full 360-degree loop.
bool DetectArcPattern(MouseDevice& device, DetectionResult& result) {
    if (device.patternHistory.size() < MIN_ARC_POINTS + ARC_SLIDING_WINDOW) {
        return false;
    }

    // Convert deque to contiguous vector for indexed access
    std::vector<POINT> pts(device.patternHistory.begin(), device.patternHistory.end());
    size_t n = pts.size();

    // Slide a window across the pattern history and fit circles
    std::vector<ArcSegment> currentArcs;

    for (size_t start = 0; start + ARC_SLIDING_WINDOW <= n; start += ARC_SLIDING_WINDOW / 2) {
        size_t windowSize = ARC_SLIDING_WINDOW;
        if (start + windowSize > n) windowSize = n - start;
        if (windowSize < MIN_ARC_POINTS) break;

        double cx, cy, r;
        if (!FitCircleToPoints(&pts[start], windowSize, cx, cy, r)) {
            continue;
        }

        // Filter: radius must be in reasonable range
        if (r < MIN_ARC_RADIUS || r > MAX_ARC_RADIUS) {
            continue;
        }

        // Calculate per-point radii to check fit quality (CV)
        double sumR = 0.0, sumR2 = 0.0;
        for (size_t i = start; i < start + windowSize; i++) {
            double dx = (double)pts[i].x - cx;
            double dy = (double)pts[i].y - cy;
            double ri = sqrt(dx * dx + dy * dy);
            sumR += ri;
            sumR2 += ri * ri;
        }
        double meanR = sumR / windowSize;
        double varR = (sumR2 / windowSize) - (meanR * meanR);
        if (varR < 0.0) varR = 0.0;
        double cvR = (meanR > 0.0) ? sqrt(varR) / meanR : 1.0;

        if (cvR > ARC_RADIUS_CV_THRESHOLD) {
            continue; // Points don't fit a circle well enough
        }

        // Calculate angular span of this arc using cumulative sweep
        // (sums consecutive point-to-point angles around the fitted center,
        //  so arcs beyond 180° and direction reversals are measured correctly)
        double cumulativeAngle = 0.0;
        for (size_t i = start + 1; i < start + windowSize; i++) {
            double a1 = atan2((double)pts[i-1].y - cy, (double)pts[i-1].x - cx);
            double a2 = atan2((double)pts[i].y   - cy, (double)pts[i].x   - cx);
            double diff = a2 - a1;
            while (diff >  M_PI) diff -= 2.0 * M_PI;
            while (diff < -M_PI) diff += 2.0 * M_PI;
            cumulativeAngle += diff;
        }
        double arcAngle = fabs(cumulativeAngle) * 180.0 / M_PI;

        if (arcAngle < MIN_ARC_ANGLE) {
            continue; // Arc is too narrow to be meaningful
        }

        ArcSegment seg;
        seg.radius = r;
        seg.centerX = cx;
        seg.centerY = cy;
        seg.arcAngle = arcAngle;
        seg.radiusCV = cvR;
        currentArcs.push_back(seg);
    }

    // Store detected arcs in device history
    for (const auto& arc : currentArcs) {
        device.arcHistory.push_back(arc);
        if (device.arcHistory.size() > MAX_ARC_HISTORY_SIZE) {
            device.arcHistory.pop_front();
        }
    }

    // Check if enough arcs share a consistent radius (wheel signature)
    if (device.arcHistory.size() < MIN_CONSISTENT_ARCS) {
        return false;
    }

    // Group arcs by radius similarity: for each arc, count how many others match
    size_t bestMatchCount = 0;
    double bestRadius = 0.0;
    double bestTotalArcAngle = 0.0;

    for (size_t i = 0; i < device.arcHistory.size(); i++) {
        size_t matchCount = 1;
        double totalAngle = device.arcHistory[i].arcAngle;
        double refRadius = device.arcHistory[i].radius;

        for (size_t j = 0; j < device.arcHistory.size(); j++) {
            if (i == j) continue;
            double radiusDiff = fabs(device.arcHistory[j].radius - refRadius) / refRadius;
            if (radiusDiff <= ARC_RADIUS_MATCH_TOLERANCE) {
                matchCount++;
                totalAngle += device.arcHistory[j].arcAngle;
            }
        }

        if (matchCount > bestMatchCount) {
            bestMatchCount = matchCount;
            bestRadius = refRadius;
            bestTotalArcAngle = totalAngle;
        }
    }

    if (bestMatchCount >= MIN_CONSISTENT_ARCS) {
        result.type = DetectionType::CIRCULAR_ARC_PATTERN;
        result.value = bestRadius;
        std::ostringstream oss;
        oss << "Jiggler Detected: Repeated arc pattern (radius: " << std::fixed << std::setprecision(1)
            << bestRadius << "px, " << bestMatchCount << " consistent arcs, ~"
            << std::setprecision(0) << bestTotalArcAngle << " deg total)";
        result.reason = oss.str();
        return true;
    }

    return false;
}

// 4c. Geometric Pattern Detection (square, rectangle, triangle, etc.)
// Uses autocorrelation of direction angles to detect repeating geometric shapes.
bool DetectGeometricPattern(MouseDevice& device, DetectionResult& result) {
    if (device.patternHistory.size() < MIN_GEOMETRIC_HISTORY) {
        return false;
    }

    // Compute direction angles from consecutive points
    std::vector<POINT> pts(device.patternHistory.begin(), device.patternHistory.end());
    std::vector<double> angles;
    for (size_t i = 1; i < pts.size(); i++) {
        double dx = (double)(pts[i].x - pts[i-1].x);
        double dy = (double)(pts[i].y - pts[i-1].y);
        if (fabs(dx) < MIN_MOVEMENT_THRESHOLD && fabs(dy) < MIN_MOVEMENT_THRESHOLD) continue; // skip zero-movement
        angles.push_back(atan2(dy, dx));
    }

    if ((int)angles.size() < MIN_GEOMETRIC_PERIOD * 3) {
        return false;
    }

    // Compute circular mean of angles (correct for circular data)
    double sumCos = 0.0, sumSin = 0.0;
    for (double a : angles) {
        sumCos += cos(a);
        sumSin += sin(a);
    }
    double meanAngle = atan2(sumSin / angles.size(), sumCos / angles.size());

    // Compute autocorrelation at various lags
    // Note: Using simplified variance/autocorrelation (not full circular statistics).
    // This is adequate for detecting periodicity in direction sequences, which is our goal.
    // Proper circular variance would use: var = 1 - R where R = sqrt(sumCos² + sumSin²) / n
    size_t n = angles.size();
    double var = 0.0;
    for (double a : angles) var += (a - meanAngle) * (a - meanAngle);
    var /= n;
    if (var < MIN_ANGLE_VARIANCE) return false; // constant angle = straight line, not geometric

    int bestPeriod = -1;
    double bestCorr = 0.0;

    for (int lag = MIN_GEOMETRIC_PERIOD; lag <= MAX_GEOMETRIC_PERIOD && lag < (int)n / 2; lag++) {
        double corr = 0.0;
        int count = 0;
        for (size_t i = 0; i + lag < n; i++) {
            corr += (angles[i] - meanAngle) * (angles[i + lag] - meanAngle);
            count++;
        }
        if (count > 0) {
            corr /= (count * var);
            if (corr > bestCorr) {
                bestCorr = corr;
                bestPeriod = lag;
            }
        }
    }

    if (bestCorr >= GEOMETRIC_AUTOCORR_THRESHOLD && bestPeriod > 0) {
        result.type = DetectionType::GEOMETRIC_PATTERN;
        result.value = bestCorr;
        std::ostringstream oss;
        oss << "Jiggler Detected: Geometric movement pattern (period: " << bestPeriod
            << " samples, correlation: " << std::fixed << std::setprecision(2) << bestCorr << ")";
        result.reason = oss.str();
        return true;
    }
    return false;
}

// 4b. Full Circular Pattern Detection (legacy: requires near-complete loop)
bool DetectCircularPattern(MouseDevice& device, DetectionResult& result) {
    if (device.patternHistory.size() < MIN_PATTERN_HISTORY) {
        return false;
    }

    // Calculate center of mass
    double centerX = 0.0, centerY = 0.0;
    for (const auto& pt : device.patternHistory) {
        centerX += pt.x;
        centerY += pt.y;
    }
    centerX /= device.patternHistory.size();
    centerY /= device.patternHistory.size();

    // Calculate angles and radii from center
    std::vector<double> radii;
    double cumulativeAngle = 0.0;
    double prevAngle = 0.0;
    bool firstPoint = true;

    for (const auto& pt : device.patternHistory) {
        double dx = pt.x - centerX;
        double dy = pt.y - centerY;
        double radius = sqrt(dx * dx + dy * dy);
        radii.push_back(radius);

        double angle = atan2(dy, dx) * 180.0 / M_PI;
        if (!firstPoint) {
            double angleDiff = angle - prevAngle;
            // Normalize angle difference to [-180, 180]
            while (angleDiff > 180.0) angleDiff -= 360.0;
            while (angleDiff < -180.0) angleDiff += 360.0;
            cumulativeAngle += fabs(angleDiff);
        }
        prevAngle = angle;
        firstPoint = false;
    }

    // Check radius consistency
    double meanRadius = 0.0;
    for (double r : radii) meanRadius += r;
    meanRadius /= radii.size();

    if (meanRadius < 5.0) return false; // Too small to be meaningful

    double radiusVariance = 0.0;
    for (double r : radii) {
        radiusVariance += (r - meanRadius) * (r - meanRadius);
    }
    radiusVariance /= radii.size();
    double radiusCV = sqrt(radiusVariance) / meanRadius;

    // Check if pattern is circular (full loop or arc detection)
    if (cumulativeAngle > ANGLE_THRESHOLD && radiusCV < RADIUS_CV_THRESHOLD) {
        result.type = DetectionType::CIRCULAR_PATTERN;
        result.value = cumulativeAngle;
        result.reason = "Jiggler Detected: Circular pattern detected";
        return true;
    }
    return false;
}

// 5. Oscillation Pattern Detection
bool DetectOscillationPattern(MouseDevice& device, DetectionResult& result) {
    if (device.directionXHistory.size() < MIN_OSCILLATION_HISTORY) {
        return false;
    }
    
    // Count direction reversals
    int reversalsX = 0, reversalsY = 0;
    for (size_t i = 1; i < device.directionXHistory.size(); i++) {
        if (device.directionXHistory[i] != device.directionXHistory[i-1] && 
            device.directionXHistory[i] != 0 && device.directionXHistory[i-1] != 0) {
            reversalsX++;
        }
    }
    for (size_t i = 1; i < device.directionYHistory.size(); i++) {
        if (device.directionYHistory[i] != device.directionYHistory[i-1] && 
            device.directionYHistory[i] != 0 && device.directionYHistory[i-1] != 0) {
            reversalsY++;
        }
    }
    
    // Check if there are regular reversals (oscillation pattern)
    if (reversalsX >= MIN_REVERSALS || reversalsY >= MIN_REVERSALS) {
        result.type = DetectionType::OSCILLATION_PATTERN;
        result.value = std::max(reversalsX, reversalsY);
        result.reason = "Jiggler Detected: Oscillating pattern detected";
        return true;
    }
    return false;
}

// 6. Constant Delta Detection (constant pixel steps, e.g., always ±1px, ±2px, ±5px)
bool DetectConstantDelta(MouseDevice& device, DetectionResult& result) {
    if (device.patternHistory.size() < MIN_SPEED_SAMPLES + 1) {
        return false;
    }

    std::vector<POINT> pts(device.patternHistory.begin(), device.patternHistory.end());

    // Compute absolute dx and dy deltas
    std::vector<double> absDx, absDy;
    for (size_t i = 1; i < pts.size(); i++) {
        double dx = fabs((double)(pts[i].x - pts[i-1].x));
        double dy = fabs((double)(pts[i].y - pts[i-1].y));
        if (dx > 0.0) absDx.push_back(dx);
        if (dy > 0.0) absDy.push_back(dy);
    }

    auto calcCV = [](const std::vector<double>& vals) -> double {
        if (vals.size() < MIN_CV_SAMPLES) return 1.0;
        double sum = 0.0;
        for (double v : vals) sum += v;
        double mean = sum / vals.size();
        if (mean < MIN_PIXEL_MEAN_THRESHOLD) return 1.0; // ignore sub-pixel
        double var = 0.0;
        for (double v : vals) var += (v - mean) * (v - mean);
        var /= vals.size();
        return sqrt(var) / mean;
    };

    double cvX = calcCV(absDx);
    double cvY = calcCV(absDy);

    bool xConstant = cvX < CONSTANT_DELTA_CV_THRESHOLD && absDx.size() >= (size_t)MIN_SPEED_SAMPLES;
    bool yConstant = cvY < CONSTANT_DELTA_CV_THRESHOLD && absDy.size() >= (size_t)MIN_SPEED_SAMPLES;

    if (xConstant || yConstant) {
        result.type = DetectionType::CONSTANT_DELTA;
        result.value = std::min(cvX, cvY);
        std::ostringstream oss;
        oss << "Jiggler Detected: Constant pixel-step movement";
        if (xConstant && !absDx.empty()) {
            double meanX = 0.0; for (double v : absDx) meanX += v; meanX /= absDx.size();
            oss << " (X: ±" << std::fixed << std::setprecision(1) << meanX << "px, CV=" << std::setprecision(2) << cvX << ")";
        }
        if (yConstant && !absDy.empty()) {
            double meanY = 0.0; for (double v : absDy) meanY += v; meanY /= absDy.size();
            oss << " (Y: ±" << std::fixed << std::setprecision(1) << meanY << "px, CV=" << std::setprecision(2) << cvY << ")";
        }
        result.reason = oss.str();
        return true;
    }
    return false;
}

// 7. Alternating Pattern Detection (+N/-N sign alternation, any period 1-8)
bool DetectAlternatingPattern(MouseDevice& device, DetectionResult& result) {
    if (device.directionXHistory.size() < MIN_OSCILLATION_HISTORY) {
        return false;
    }

    // Build sign vectors (only non-zero directions)
    std::vector<double> signsX, signsY;
    for (int d : device.directionXHistory) if (d != 0) signsX.push_back((double)d);
    for (int d : device.directionYHistory) if (d != 0) signsY.push_back((double)d);

    auto checkAutocorr = [](const std::vector<double>& signs, int maxPeriod, double threshold) -> int {
        if (signs.size() < MIN_ALTERNATING_SAMPLES) return -1;
        size_t n = signs.size();
        double mean = 0.0;
        for (double s : signs) mean += s;
        mean /= n;
        double var = 0.0;
        for (double s : signs) var += (s - mean) * (s - mean);
        var /= n;
        if (var < MIN_SIGN_VARIANCE) return -1;

        for (int lag = 1; lag <= maxPeriod && lag < (int)n / 2; lag++) {
            double corr = 0.0;
            int cnt = 0;
            for (size_t i = 0; i + lag < n; i++) {
                corr += (signs[i] - mean) * (signs[i + lag] - mean);
                cnt++;
            }
            if (cnt > 0) {
                corr /= (cnt * var);
                if (corr >= threshold) return lag;
            }
        }
        return -1;
    };

    int periodX = checkAutocorr(signsX, MAX_ALTERNATING_PERIOD, ALTERNATING_AUTOCORR_THRESHOLD);
    int periodY = checkAutocorr(signsY, MAX_ALTERNATING_PERIOD, ALTERNATING_AUTOCORR_THRESHOLD);

    if (periodX > 0 || periodY > 0) {
        result.type = DetectionType::ALTERNATING_PATTERN;
        result.value = (periodX > 0) ? periodX : periodY;
        std::ostringstream oss;
        oss << "Jiggler Detected: Alternating direction pattern";
        if (periodX > 0) oss << " (X period=" << periodX << ")";
        if (periodY > 0) oss << " (Y period=" << periodY << ")";
        result.reason = oss.str();
        return true;
    }
    return false;
}

// Calculate behavioral suspicion score for a device (0-100)
int CalculateBehaviorSuspicionScore(MouseDevice& device, DetectionResult& result) {
    int score = 0;

    // Skip if not enough movement data (minimum 3 for basic analysis)
    if (device.movements.size() < 3) {
        return 0;
    }

    // 1. Check for small repetitive movements (+40 points)
    // Only flag when deltas are small AND the cursor stays in a small area
    // (i.e., net displacement is low relative to total path length).
    // Continuous directional movement produces small per-report deltas at
    // high polling rates but accumulates large net displacement, so it must
    // not be flagged.
    int smallMoveCount = 0;
    double totalPathLength = 0.0;
    for (size_t j = 1; j < device.movements.size(); ++j) {
        LONG dx = device.movements[j].x - device.movements[j - 1].x;
        LONG dy = device.movements[j].y - device.movements[j - 1].y;
        totalPathLength += sqrt((double)(dx * dx + dy * dy));
        if (abs(dx) <= 3 && abs(dy) <= 3) {
            smallMoveCount++;
        }
    }
    double smallMoveRatio = (double)smallMoveCount / device.movements.size();

    // Compute net displacement from first to last recorded position
    double netDisplacement = 0.0;
    if (device.movements.size() >= 2) {
        double ndx = (double)(device.movements.back().x - device.movements.front().x);
        double ndy = (double)(device.movements.back().y - device.movements.front().y);
        netDisplacement = sqrt(ndx * ndx + ndy * ndy);
    }

    // straightnessRatio near 1 means steady directional travel (normal use);
    // near 0 means the cursor is oscillating in place (jiggler-like).
    double straightnessRatio = (totalPathLength > 0.0) ? (netDisplacement / totalPathLength) : 0.0;

    if (smallMoveRatio > 0.8 && straightnessRatio < 0.3) {
        score += 25;
        result.type = DetectionType::SMALL_MOVEMENT;
        result.reason = "Small repetitive movements detected";
    } else if (smallMoveRatio > 0.5 && straightnessRatio < 0.3) {
        score += 10;
    }

    // 2. Check for constant speed (+30 points)
    if (device.speedHistory.size() >= MIN_SPEED_SAMPLES) {
        double sum = 0.0;
        for (double speed : device.speedHistory) sum += speed;
        double mean = sum / device.speedHistory.size();

        if (mean > 0.1) {
            double variance = 0.0;
            for (double speed : device.speedHistory) {
                variance += (speed - mean) * (speed - mean);
            }
            variance /= device.speedHistory.size();
            double cv = sqrt(variance) / mean;

            if (cv < SPEED_CV_THRESHOLD) {
                score += 30;
                if (result.type == DetectionType::NONE) {
                    result.type = DetectionType::CONSTANT_SPEED;
                    result.reason = "Constant speed pattern detected";
                }
            } else if (cv < SPEED_CV_THRESHOLD * 2) {
                score += 15;
            }
        }
    }

    // 3. Check for oscillation pattern (+20 points)
    if (device.directionXHistory.size() >= MIN_OSCILLATION_HISTORY) {
        int reversalsX = 0, reversalsY = 0;
        for (size_t i = 1; i < device.directionXHistory.size(); i++) {
            if (device.directionXHistory[i] != device.directionXHistory[i-1] &&
                device.directionXHistory[i] != 0 && device.directionXHistory[i-1] != 0) {
                reversalsX++;
            }
        }
        for (size_t i = 1; i < device.directionYHistory.size(); i++) {
            if (device.directionYHistory[i] != device.directionYHistory[i-1] &&
                device.directionYHistory[i] != 0 && device.directionYHistory[i-1] != 0) {
                reversalsY++;
            }
        }

        if (reversalsX >= MIN_REVERSALS || reversalsY >= MIN_REVERSALS) {
            score += 20;
            if (result.type == DetectionType::NONE) {
                result.type = DetectionType::OSCILLATION_PATTERN;
                result.reason = "Oscillating pattern detected";
            }
        }
    }

    // 4. Check for arc-based circular pattern / wheel jiggler (+25 points)
    {
        DetectionResult arcResult;
        if (DetectArcPattern(device, arcResult)) {
            score += 25;
            if (result.type == DetectionType::NONE) {
                result.type = arcResult.type;
                result.reason = arcResult.reason;
            }
        }
    }

    // 5. Check for continuous movement without pause (+10 points)
    if (device.continuousDuration >= CONTINUOUS_THRESHOLD_SECONDS) {
        score += 10;
        if (result.type == DetectionType::NONE) {
            result.type = DetectionType::CONTINUOUS_MOVEMENT;
            int minutes = (int)(device.continuousDuration / 60.0);
            std::ostringstream oss;
            oss << "Continuous movement for " << minutes << " minute(s)";
            result.reason = oss.str();
        }
    }

    // 6. Check for geometric pattern (circle, square, etc.) (+30 points)
    {
        DetectionResult geoResult;
        if (DetectGeometricPattern(device, geoResult)) {
            score += 30;
            if (result.type == DetectionType::NONE) {
                result.type = geoResult.type;
                result.reason = geoResult.reason;
            }
        }
    }

    // 7. Check for constant pixel-step delta (+35 points)
    {
        DetectionResult deltaResult;
        if (DetectConstantDelta(device, deltaResult)) {
            score += 35;
            if (result.type == DetectionType::NONE) {
                result.type = deltaResult.type;
                result.reason = deltaResult.reason;
            }
        }
    }

    // 8. Check for alternating direction pattern (+30 points)
    {
        DetectionResult altResult;
        if (DetectAlternatingPattern(device, altResult)) {
            score += 30;
            if (result.type == DetectionType::NONE) {
                result.type = altResult.type;
                result.reason = altResult.reason;
            }
        }
    }

    return std::min(score, 100);
}

// Primary mouse zigzag (linear/sawtooth) qualification:
// Requires sustained evidence of constant-speed diagonal oscillation within a bounded
// coordinate region. All four primary signals must be present simultaneously across
// multiple consecutive detection cycles before triggering.
//
// Two detection paths exist:
//  Path A (original): High straightness + direction consistency + low speed CV.
//  Path B (unit-step): When >=90% of speed samples are unit steps (<=sqrt(2)),
//          the straightness and direction consistency checks are bypassed because
//          periodic reversals break those metrics even though the underlying pattern
//          is clearly mechanical. Only bounded range + low speed CV are required.
bool DetectPrimaryMouseZigzagPattern(MouseDevice& device, DetectionResult& result) {
    // Require minimum data for reliable analysis.
    if (device.speedHistory.size() < PRIMARY_MOUSE_ZIGZAG_MIN_SPEED_SAMPLES ||
        device.patternHistory.size() < PRIMARY_MOUSE_ZIGZAG_MIN_SPEED_SAMPLES) {
        device.primaryZigzagConsecutiveHighScores = std::max(0, device.primaryZigzagConsecutiveHighScores - 1);
        return false;
    }

    // --- Unit-step ratio calculation ---
    // External wheel jigglers produce exclusively ±1px deltas, resulting in speeds of
    // exactly 1.0 (single axis) or sqrt(2) ≈ 1.414 (both axes). A very high unit-step
    // ratio is a strong discriminative signal that normal mice never produce.
    int unitStepCount = 0;
    for (double s : device.speedHistory) {
        if (s <= PRIMARY_MOUSE_ZIGZAG_UNIT_STEP_SPEED_MAX) {
            unitStepCount++;
        }
    }
    double unitStepRatio = (double)unitStepCount / (double)device.speedHistory.size();
    bool hasUnitStepSignal = (unitStepRatio >= PRIMARY_MOUSE_ZIGZAG_UNIT_STEP_RATIO);

    // --- Check 1: Bounded oscillation ---
    // Both X and Y must have measurable but non-excessive range in the recent position window.
    // When unit-step is confirmed, use a relaxed minimum for the minor axis since wheel
    // jigglers may produce very little movement on one axis within a single window.
    std::vector<POINT> pts(device.patternHistory.begin(), device.patternHistory.end());
    long minX = pts[0].x, maxX = pts[0].x, minY = pts[0].y, maxY = pts[0].y;
    for (const auto& p : pts) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    }
    double rangeX = (double)(maxX - minX);
    double rangeY = (double)(maxY - minY);
    double bboxMin = hasUnitStepSignal ? PRIMARY_MOUSE_ZIGZAG_UNIT_STEP_MIN_BBOX : PRIMARY_MOUSE_ZIGZAG_MIN_BBOX_RANGE;
    if (rangeX < bboxMin || rangeX > PRIMARY_MOUSE_ZIGZAG_MAX_BBOX_RANGE ||
        rangeY < bboxMin || rangeY > PRIMARY_MOUSE_ZIGZAG_MAX_BBOX_RANGE) {
        device.primaryZigzagConsecutiveHighScores = std::max(0, device.primaryZigzagConsecutiveHighScores - 1);
        return false;
    }

    // --- Compute path metrics (used by both detection paths and output) ---
    double sumCos = 0.0, sumSin = 0.0;
    int angleCount = 0;
    double pathLength = 0.0;
    for (size_t i = 1; i < pts.size(); i++) {
        double dx = (double)(pts[i].x - pts[i-1].x);
        double dy = (double)(pts[i].y - pts[i-1].y);
        double dist = sqrt(dx * dx + dy * dy);
        pathLength += dist;
        if (dist > 0.0) {
            sumCos += dx / dist;
            sumSin += dy / dist;
            angleCount++;
        }
    }
    if (angleCount == 0) {
        device.primaryZigzagConsecutiveHighScores = std::max(0, device.primaryZigzagConsecutiveHighScores - 1);
        return false;
    }
    double directionConsistency = sqrt(sumCos * sumCos + sumSin * sumSin) / (double)angleCount;

    double netDx = (double)(pts.back().x - pts.front().x);
    double netDy = (double)(pts.back().y - pts.front().y);
    double netDisp = sqrt(netDx * netDx + netDy * netDy);
    double straightness = (pathLength > 0.0) ? (netDisp / pathLength) : 0.0;

    // --- Check 2 & 3: Direction consistency and straightness ---
    // When the unit-step signal is present, these checks are bypassed because periodic
    // reversals in the zigzag path reduce both metrics even though the motion is mechanical.
    if (!hasUnitStepSignal) {
        if (directionConsistency < PRIMARY_MOUSE_ZIGZAG_DIRECTION_CONSISTENCY) {
            device.primaryZigzagConsecutiveHighScores = std::max(0, device.primaryZigzagConsecutiveHighScores - 1);
            return false;
        }
        if (straightness < PRIMARY_MOUSE_ZIGZAG_STRAIGHTNESS_THRESHOLD) {
            device.primaryZigzagConsecutiveHighScores = std::max(0, device.primaryZigzagConsecutiveHighScores - 1);
            return false;
        }
    }

    // --- Check 4: Constant speed (low coefficient of variation) ---
    double speedSum = 0.0;
    size_t speedCount = device.speedHistory.size();
    for (double s : device.speedHistory) speedSum += s;
    double speedMean = speedSum / (double)speedCount;
    if (speedMean < 0.1) {
        device.primaryZigzagConsecutiveHighScores = std::max(0, device.primaryZigzagConsecutiveHighScores - 1);
        return false;
    }
    double speedVar = 0.0;
    for (double s : device.speedHistory) speedVar += (s - speedMean) * (s - speedMean);
    speedVar /= (double)speedCount;
    double speedCV = sqrt(speedVar) / speedMean;
    if (speedCV > PRIMARY_MOUSE_ZIGZAG_SPEED_CV_THRESHOLD) {
        device.primaryZigzagConsecutiveHighScores = std::max(0, device.primaryZigzagConsecutiveHighScores - 1);
        return false;
    }

    // --- Optional: direction reversals in history window ---
    // For fast jigglers, reversals may be visible in the 50-entry direction history.
    // For slow jigglers, reversals occur less often than the window size so this may be 0.
    int reversalsX = 0, reversalsY = 0;
    for (size_t i = 1; i < device.directionXHistory.size(); i++) {
        if (device.directionXHistory[i] != device.directionXHistory[i-1] &&
            device.directionXHistory[i] != 0 && device.directionXHistory[i-1] != 0)
            reversalsX++;
    }
    for (size_t i = 1; i < device.directionYHistory.size(); i++) {
        if (device.directionYHistory[i] != device.directionYHistory[i-1] &&
            device.directionYHistory[i] != 0 && device.directionYHistory[i-1] != 0)
            reversalsY++;
    }
    bool hasReversalEvidence = (reversalsX >= PRIMARY_MOUSE_ZIGZAG_MIN_REVERSALS ||
                                 reversalsY >= PRIMARY_MOUSE_ZIGZAG_MIN_REVERSALS);

    // All qualifying signals passed: increment the consecutive confirmation counter.
    // Use fewer confirmation cycles when unit-step evidence is present since the
    // ±1px-only speed signature is already a very strong discriminative signal.
    int requiredCycles = hasUnitStepSignal
        ? PRIMARY_MOUSE_ZIGZAG_UNIT_STEP_CONFIRMATION
        : PRIMARY_MOUSE_ZIGZAG_CONFIRMATION_CYCLES;
    device.primaryZigzagConsecutiveHighScores++;
    if (device.primaryZigzagConsecutiveHighScores < requiredCycles) {
        return false;
    }

    result.type = DetectionType::ZIGZAG_PATTERN;
    result.value = hasUnitStepSignal ? unitStepRatio : directionConsistency;
    std::ostringstream oss;
    oss << "Linear zigzag/sawtooth pattern on primary mouse (";
    if (hasUnitStepSignal) {
        oss << "unit-step ratio: " << std::fixed << std::setprecision(2) << unitStepRatio
            << ", speed CV: " << std::setprecision(2) << speedCV;
    } else {
        oss << "direction consistency: " << std::fixed << std::setprecision(2) << directionConsistency
            << ", speed CV: " << std::setprecision(2) << speedCV
            << ", straightness: " << std::setprecision(2) << straightness;
    }
    if (hasReversalEvidence) {
        oss << ", reversals: " << std::max(reversalsX, reversalsY);
    }
    oss << ")";
    result.reason = oss.str();
    return true;
}

// Primary mouse repetitive micro-delta detection.
// Detects jigglers producing nearly straight-line movement by looking at X and
// Y axes separately.  A wheel-based jiggler moves in long monotonic runs on
// each axis (hundreds of identical-direction ±1px steps = a straight line),
// then reverses course on both axes simultaneously, then another straight line.
//
// This approach is placement-independent: regardless of the angle of the jiggler
// on the mouse, the dominant-direction ratio on each active axis stays very high
// within any 100-point window that falls inside a straight segment.
//
// Signals checked:
//  1. Unit-step ratio: >=90% of speeds are <= sqrt(2) (±1px deltas only).
//  2. Major-axis range: at least a few pixels of net travel on the dominant axis.
//  3. Straight-line dominance: on each axis with enough non-zero deltas, the
//     vast majority point in the same direction (>=80%).
//  4. Sustained confirmation across multiple detection cycles.
bool DetectPrimaryMouseRepetitiveDelta(MouseDevice& device, DetectionResult& result) {
    // Require minimum data for reliable analysis.
    if (device.patternHistory.size() < PRIMARY_MOUSE_ZIGZAG_MIN_SPEED_SAMPLES ||
        device.speedHistory.size() < PRIMARY_MOUSE_ZIGZAG_MIN_SPEED_SAMPLES) {
        device.primaryRepetitiveConsecutiveScores = std::max(0, device.primaryRepetitiveConsecutiveScores - 1);
        return false;
    }

    // --- Check 1: Unit-step signal ---
    // Wheel jigglers exclusively produce ±1px deltas (speed 1.0 or sqrt(2)).
    int unitStepCount = 0;
    for (double s : device.speedHistory) {
        if (s <= PRIMARY_MOUSE_ZIGZAG_UNIT_STEP_SPEED_MAX) {
            unitStepCount++;
        }
    }
    double unitStepRatio = (double)unitStepCount / (double)device.speedHistory.size();
    if (unitStepRatio < PRIMARY_MOUSE_ZIGZAG_UNIT_STEP_RATIO) {
        device.primaryRepetitiveConsecutiveScores = std::max(0, device.primaryRepetitiveConsecutiveScores - 1);
        return false;
    }

    // --- Compute deltas from patternHistory ---
    size_t n = device.patternHistory.size();
    std::vector<std::pair<long, long>> deltas;
    deltas.reserve(n - 1);
    for (size_t i = 1; i < n; i++) {
        long dx = device.patternHistory[i].x - device.patternHistory[i - 1].x;
        long dy = device.patternHistory[i].y - device.patternHistory[i - 1].y;
        deltas.push_back({dx, dy});
    }

    if (deltas.size() < PRIMARY_MOUSE_REPETITIVE_MIN_DELTAS) {
        device.primaryRepetitiveConsecutiveScores = std::max(0, device.primaryRepetitiveConsecutiveScores - 1);
        return false;
    }

    // --- Check 2: Major axis has meaningful movement ---
    // Only require the dominant axis to have some range (not both axes).
    long minX = device.patternHistory[0].x, maxX = minX;
    long minY = device.patternHistory[0].y, maxY = minY;
    for (size_t i = 1; i < n; i++) {
        if (device.patternHistory[i].x < minX) minX = device.patternHistory[i].x;
        if (device.patternHistory[i].x > maxX) maxX = device.patternHistory[i].x;
        if (device.patternHistory[i].y < minY) minY = device.patternHistory[i].y;
        if (device.patternHistory[i].y > maxY) maxY = device.patternHistory[i].y;
    }
    double majorRange = std::max((double)(maxX - minX), (double)(maxY - minY));
    if (majorRange < PRIMARY_MOUSE_ZIGZAG_UNIT_STEP_MIN_BBOX) {
        device.primaryRepetitiveConsecutiveScores = std::max(0, device.primaryRepetitiveConsecutiveScores - 1);
        return false;
    }

    // --- Check 3: Straight-line dominance on each axis ---
    // Look at X and Y separately.  On each axis, count how many non-zero
    // deltas go positive vs negative.  A jiggler produces a nearly straight
    // line: the vast majority of deltas on each active axis point in the same
    // direction.  Course changes are brief (10-20 moves out of 700+ segment
    // length) and rarely land inside a 100-point window.
    int xPos = 0, xNeg = 0, yPos = 0, yNeg = 0;
    for (const auto& d : deltas) {
        if (d.first > 0) xPos++;
        else if (d.first < 0) xNeg++;
        if (d.second > 0) yPos++;
        else if (d.second < 0) yNeg++;
    }
    int xTotal = xPos + xNeg;
    int yTotal = yPos + yNeg;

    // Both axes must have enough non-zero samples, or at least one must.
    // If neither axis has enough samples there is not enough data to judge.
    if (xTotal < (int)PRIMARY_MOUSE_STRAIGHT_LINE_MIN_AXIS_SAMPLES &&
        yTotal < (int)PRIMARY_MOUSE_STRAIGHT_LINE_MIN_AXIS_SAMPLES) {
        device.primaryRepetitiveConsecutiveScores = std::max(0, device.primaryRepetitiveConsecutiveScores - 1);
        return false;
    }

    // Compute dominance on each axis that has enough samples.
    // An axis with too few non-zero samples is not checked (default 1.0)
    // but at least one axis is guaranteed to be checked by the gate above.
    double xDominance = (xTotal >= (int)PRIMARY_MOUSE_STRAIGHT_LINE_MIN_AXIS_SAMPLES)
        ? (double)std::max(xPos, xNeg) / (double)xTotal : 1.0;
    double yDominance = (yTotal >= (int)PRIMARY_MOUSE_STRAIGHT_LINE_MIN_AXIS_SAMPLES)
        ? (double)std::max(yPos, yNeg) / (double)yTotal : 1.0;

    // Both active axes must show high dominance (straight-line movement).
    if (xDominance < PRIMARY_MOUSE_STRAIGHT_LINE_DOMINANCE ||
        yDominance < PRIMARY_MOUSE_STRAIGHT_LINE_DOMINANCE) {
        device.primaryRepetitiveConsecutiveScores = std::max(0, device.primaryRepetitiveConsecutiveScores - 1);
        return false;
    }

    // --- Confirmation cycles ---
    device.primaryRepetitiveConsecutiveScores++;
    if (device.primaryRepetitiveConsecutiveScores < PRIMARY_MOUSE_ZIGZAG_UNIT_STEP_CONFIRMATION) {
        return false;
    }

    // Detected!
    result.type = DetectionType::REPETITIVE_DELTA;
    double minDominance = std::min(xDominance, yDominance);
    result.value = minDominance;
    std::ostringstream oss;
    oss << "Straight-line micro-movement on primary mouse ("
        << "X dominance: " << std::fixed << std::setprecision(2) << xDominance
        << ", Y dominance: " << std::setprecision(2) << yDominance
        << ", unit-step ratio: " << std::setprecision(2) << unitStepRatio
        << ")";
    result.reason = oss.str();
    return true;
}

// Primary mouse wheel-jiggler qualification:
// require a dominant repeated arc radius sustained across many cycles.
bool DetectPrimaryMouseWheelPattern(MouseDevice& device, DetectionResult& result) {
    DetectionResult arcResult;
    const bool hasArcEvidence = DetectArcPattern(device, arcResult);
    if (!hasArcEvidence || device.arcHistory.size() < PRIMARY_MOUSE_MIN_ARC_HISTORY) {
        device.primaryConsecutiveHighScores = std::max(0, device.primaryConsecutiveHighScores - 1);
        return false;
    }

    size_t bestMatchCount = 0;
    double bestRadius = 0.0;
    double bestTotalArcAngle = 0.0;

    for (size_t i = 0; i < device.arcHistory.size(); i++) {
        size_t matchCount = 1;
        double totalAngle = device.arcHistory[i].arcAngle;
        const double refRadius = device.arcHistory[i].radius;
        if (refRadius <= 0.0) continue;

        for (size_t j = 0; j < device.arcHistory.size(); j++) {
            if (i == j) continue;
            double radiusDiff = fabs(device.arcHistory[j].radius - refRadius) / refRadius;
            if (radiusDiff <= PRIMARY_MOUSE_RADIUS_MATCH_TOLERANCE) {
                matchCount++;
                totalAngle += device.arcHistory[j].arcAngle;
            }
        }

        if (matchCount > bestMatchCount) {
            bestMatchCount = matchCount;
            bestRadius = refRadius;
            bestTotalArcAngle = totalAngle;
        }
    }

    const bool qualified =
        bestMatchCount >= PRIMARY_MOUSE_MIN_SIMILAR_RADIUS_HITS &&
        bestTotalArcAngle >= PRIMARY_MOUSE_MIN_TOTAL_ARC_ANGLE;

    if (qualified) {
        device.primaryConsecutiveHighScores++;
    } else {
        device.primaryConsecutiveHighScores = std::max(0, device.primaryConsecutiveHighScores - 1);
    }

    if (device.primaryConsecutiveHighScores < PRIMARY_MOUSE_CONFIRMATION_CYCLES) {
        return false;
    }

    result.type = DetectionType::CIRCULAR_ARC_PATTERN;
    result.value = bestRadius;
    std::ostringstream oss;
    oss << "External wheel-style radius pattern on primary mouse (radius "
        << std::fixed << std::setprecision(1) << bestRadius
        << "px, repeated arcs " << bestMatchCount
        << ", total angle ~" << std::setprecision(0) << bestTotalArcAngle << " deg)";
    result.reason = oss.str();
    return true;
}
