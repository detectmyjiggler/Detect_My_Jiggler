#pragma once
// =============================================================================
// DETECTION CONSTANTS AND THRESHOLDS
// =============================================================================

#include <cstddef>

// Detection constants
const double CONTINUOUS_THRESHOLD_SECONDS = 30.0;
const double PAUSE_THRESHOLD_SECONDS = 2.0;
const double MIN_DETECTION_SECONDS = 5.0;  // Wait at least 5 seconds of data before detecting any mode
const size_t MIN_SPEED_SAMPLES = 12;
const double SPEED_CV_THRESHOLD = 0.15; // 15% coefficient of variation
const size_t MIN_PATTERN_HISTORY = 50;
const double ANGLE_THRESHOLD = 340.0; // degrees (close to 360)
const double RADIUS_CV_THRESHOLD = 0.25; // 25% variance in radius
const size_t MIN_OSCILLATION_HISTORY = 30;
const int MIN_REVERSALS = 10;
const size_t MAX_SPEED_HISTORY_SIZE = 100;
const size_t MAX_PATTERN_HISTORY_SIZE = 100;
const size_t MAX_DIRECTION_HISTORY_SIZE = 50;
const int LARGE_MOVEMENT_THRESHOLD = 3; // Movements larger than 3 pixels
const size_t MIN_REPETITIVE_SAMPLES = 10; // Minimum samples to check for repetitive pattern
const double REPETITIVE_TOLERANCE = 0.3; // 30% tolerance for considering movements similar
const size_t MAX_REPETITIVE_CHECK_SAMPLES = 50; // Limit samples to check for performance
const double REPETITIVE_SIMILARITY_THRESHOLD = 0.4; // 40% of pairs must be similar

// Arc-based circular detection (partial arc / wheel jiggler)
const size_t MIN_ARC_POINTS = 10;             // Minimum points in a sliding window for circle fitting
const size_t ARC_SLIDING_WINDOW = 15;         // Sliding window size for circle fitting
const double ARC_RADIUS_CV_THRESHOLD = 0.15;  // 15% radius variance within a window (tighter than full-circle)
const double MIN_ARC_RADIUS = 3.0;            // Minimum radius to consider (filter noise)
const double MAX_ARC_RADIUS = 500.0;          // Maximum radius (filter near-straight lines)
const double ARC_RADIUS_MATCH_TOLERANCE = 0.20; // 20% tolerance when comparing radii across windows
const size_t MIN_CONSISTENT_ARCS = 3;         // Minimum number of arc windows with matching radius
const double MIN_ARC_ANGLE = 20.0;            // Minimum angular span (degrees) per arc window
const size_t MAX_ARC_HISTORY_SIZE = 20;        // Maximum stored arc segments

// Geometric/delta/alternating detection constants
const size_t MIN_GEOMETRIC_HISTORY = 60;       // Min samples for geometric pattern
const double GEOMETRIC_AUTOCORR_THRESHOLD = 0.75; // Autocorrelation threshold for geometric
const int MIN_GEOMETRIC_PERIOD = 4;             // Min period length (samples)
const int MAX_GEOMETRIC_PERIOD = 60;            // Max period length (samples)
const double CONSTANT_DELTA_CV_THRESHOLD = 0.10; // Max CV for constant delta detection
const double ALTERNATING_AUTOCORR_THRESHOLD = 0.80; // Autocorrelation threshold for alternating
const int MAX_ALTERNATING_PERIOD = 8;           // Max period to check for alternating
const double MIN_MOVEMENT_THRESHOLD = 0.5;      // Minimum movement to consider (pixels)
const double MIN_ANGLE_VARIANCE = 1e-10;        // Minimum variance for angle analysis
const double MIN_PIXEL_MEAN_THRESHOLD = 0.5;    // Minimum pixel mean for delta analysis
const size_t MIN_CV_SAMPLES = 5;                // Minimum samples for CV calculation
const size_t MIN_ALTERNATING_SAMPLES = 10;      // Minimum samples for alternating pattern
const double MIN_SIGN_VARIANCE = 1e-10;         // Minimum variance for sign analysis

// Primary mouse detection thresholds (wheel-based external jiggler only).
// Open-source safe: no device-specific exceptions, only sustained geometric evidence.
// The primary mouse may move randomly at first (normal human use) and a jiggler
// may be activated at any time.  Rolling data windows (100 entries) naturally
// flush old movement data, and per-detector confirmation cycles (15-25 cycles)
// ensure qualification only starts once a sustained jiggler-like pattern (e.g.
// straight-line movement) is observed.  The movement gate below must stay within
// the movements-deque cap (200) so that detection is always reachable.
const size_t PRIMARY_MOUSE_MIN_MOVEMENTS = 60;   // Must be <= movements-deque cap (200); detectors have own per-window minimums
const double PRIMARY_MOUSE_MIN_DETECTION_SECONDS = 10.0; // Initial observation window; confirmation cycles add 3-5s more
const int PRIMARY_MOUSE_CONFIRMATION_CYCLES = 25;  // ~5s at 200ms/cycle
const size_t PRIMARY_MOUSE_MIN_ARC_HISTORY = 8;  // Require multiple arc windows
const size_t PRIMARY_MOUSE_MIN_SIMILAR_RADIUS_HITS = 5; // "Couple of times" radius repeats
const double PRIMARY_MOUSE_RADIUS_MATCH_TOLERANCE = 0.15; // Tight radius cluster
const double PRIMARY_MOUSE_MIN_TOTAL_ARC_ANGLE = 540.0; // At least ~1.5 turns of evidence

// Primary mouse zigzag (linear/sawtooth) jiggler detection.
// Detects wheel-based jigglers that produce a diagonal back-and-forth path with
// constant speed, consistent direction, and a bounded coordinate range.
const double PRIMARY_MOUSE_ZIGZAG_MIN_BBOX_RANGE = 10.0;       // Min bounding box range per axis (px)
const double PRIMARY_MOUSE_ZIGZAG_MAX_BBOX_RANGE = 1000.0;     // Max bounding box range per axis (px)
const int    PRIMARY_MOUSE_ZIGZAG_MIN_REVERSALS = 2;            // Min direction reversals in history window
const double PRIMARY_MOUSE_ZIGZAG_REVERSAL_RATE_THRESHOLD = 0.03; // Min reversal fraction of direction history
const double PRIMARY_MOUSE_ZIGZAG_STRAIGHTNESS_THRESHOLD = 0.85;  // Min path straightness ratio
const double PRIMARY_MOUSE_ZIGZAG_SPEED_CV_THRESHOLD = 0.25;   // Max speed coefficient of variation
const double PRIMARY_MOUSE_ZIGZAG_DIRECTION_CONSISTENCY = 0.85; // Min circular mean resultant length (R)
const int    PRIMARY_MOUSE_ZIGZAG_CONFIRMATION_CYCLES = 25;     // Consecutive qualifying cycles required
const size_t PRIMARY_MOUSE_ZIGZAG_MIN_SPEED_SAMPLES = 30;      // Min speed samples before qualifying

// Unit-step linearity detection for external wheel-based jigglers.
// Wheel jigglers produce exclusively ±1px deltas at a very consistent rate,
// resulting in speeds of exactly 1.0 (single axis) or sqrt(2) (both axes).
// When the unit-step ratio is high, the straightness and direction consistency
// checks are bypassed because periodic reversals break those metrics even though
// the underlying pattern is clearly mechanical.
const double PRIMARY_MOUSE_ZIGZAG_UNIT_STEP_SPEED_MAX = 1.45;  // sqrt(2) + tolerance for unit step
const double PRIMARY_MOUSE_ZIGZAG_UNIT_STEP_RATIO = 0.90;      // Min fraction of speeds that are unit steps
const double PRIMARY_MOUSE_ZIGZAG_UNIT_STEP_MIN_BBOX = 3.0;    // Relaxed min bbox per axis (minor axis)
const int    PRIMARY_MOUSE_ZIGZAG_UNIT_STEP_CONFIRMATION = 15;  // ~3s at 200ms/cycle (fewer with strong signal)

// Primary mouse repetitive micro-delta detection.
// Detects jigglers that produce nearly straight-line movement by looking at
// X and Y axes separately. A jiggler moves in long monotonic runs on each
// axis (straight line), then reverses course on both axes simultaneously.
// The dominant-direction ratio on each axis is placement-independent.
const double PRIMARY_MOUSE_STRAIGHT_LINE_DOMINANCE = 0.80;     // Min fraction of deltas in dominant direction per axis
const size_t PRIMARY_MOUSE_STRAIGHT_LINE_MIN_AXIS_SAMPLES = 5; // Min non-zero samples on an axis to check dominance
const size_t PRIMARY_MOUSE_REPETITIVE_MIN_DELTAS = 20;         // Min deltas in window to qualify
