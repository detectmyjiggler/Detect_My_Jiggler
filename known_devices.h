#pragma once
// =============================================================================
// KNOWN DEVICE VID/PID LOOKUP TABLE
// =============================================================================
// Gaming mice and common peripherals that don't always report manufacturer/product strings.

#include <string>

// Known device VID/PID lookup table for devices that don't report manufacturer/product strings
struct KnownDevice {
    const char* vid;
    const char* pid;
    const char* manufacturer;
    const char* product;
};

// Table of known gaming mice and their VID/PID combinations
// This serves as a fallback when HID API doesn't return manufacturer/product strings
extern const KnownDevice knownDevices[];

// Lookup device manufacturer and product from VID/PID
void LookupKnownDevice(const std::string& vid, const std::string& pid,
                        std::string& manufacturer, std::string& product);
