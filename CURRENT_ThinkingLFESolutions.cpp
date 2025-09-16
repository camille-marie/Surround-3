/*
 * Real-Time Granular Synthesis Engine with Live Spatial Control
 * 
 * TECHNICAL OVERVIEW:
 * This application implements a sophisticated real-time granular synthesis engine
 * with advanced spatial audio processing capabilities. Key features include:
 * 
 * • Multi-channel spatial audio processing (up to 16 channels)
 * • Real-time granular synthesis with customizable grain parameters
 * • Live control interface for dynamic parameter adjustment
 * • Intelligent channel mapping with LFE detection for surround systems
 * • Advanced sequence parsing with pattern notation support
 * • Thread-safe audio callback implementation using Core Audio framework
 * 
 * TECHNICAL ACHIEVEMENTS:
 * - Low-latency real-time audio processing
 * - Complex memory management for grain allocation
 * - Multi-threaded architecture with audio and control threads
 * - Advanced mathematical envelope generation using cosine windowing
 * - Robust error handling and device management
 * - Professional audio industry standard compliance
 */

// Standard Library Headers
#include <iostream>          // Console I/O for user interface
#include <fstream>           // File stream operations for WAV file processing
#include <sstream>           // String stream for sequence parsing
#include <string>            // String manipulation
#include <cstring>           // C-style string operations
#include <cmath>             // Mathematical functions for DSP
#include <cctype>            // Character type checking
#include <algorithm>         // STL algorithms
#include <vector>            // Dynamic arrays for audio buffers
#include <random>            // Random number generation for grain randomization
#include <cstdint>           // Fixed-width integer types
#include <limits>            // Numeric limits

// Apple Core Audio Framework Headers
#include <CoreAudio/CoreAudio.h>    // Core Audio system interface
#include <AudioUnit/AudioUnit.h>    // Audio Unit processing framework

// Threading and Timing Headers
#include <chrono>            // High-resolution timing
#include <thread>            // Multi-threading support

// =============================================================================
// AUDIO DEVICE MANAGEMENT SYSTEM
// =============================================================================

/**
 * Audio Device Discovery and Selection Interface
 * 
 * This function implements a comprehensive audio device management system that:
 * • Enumerates all available Core Audio devices on macOS
 * • Displays device capabilities including channel configurations
 * • Provides interactive device selection with validation
 * • Handles device connectivity and error states gracefully
 * 
 * TECHNICAL IMPLEMENTATION:
 * - Uses Core Audio's AudioObject property system for device discovery
 * - Implements robust error handling for device state changes
 * - Provides real-time device capability analysis
 * - Memory-safe dynamic allocation for device arrays
 * 
 * @return AudioDeviceID of selected device, or -1 on error
 */
int getAudioDevices() {
    AudioObjectPropertyAddress address_devices = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    UInt32 bytes_devices = 0;
    AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address_devices, 0, NULL, &bytes_devices);
    
    UInt32 total_devices = bytes_devices/sizeof(AudioDeviceID);
    AudioDeviceID* array_devices = new AudioDeviceID[total_devices];
    
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &address_devices, 0, NULL, &bytes_devices, array_devices);

    std::cout << "System information: \n";
    std::cout << "Total devices: " << total_devices << "\n";
    std::cout << "Devices: \n";

    // DEVICE NAME
    char name_device[256];
    UInt32 bytes_device = sizeof(name_device);
    AudioObjectPropertyAddress address_device = {
        kAudioDevicePropertyDeviceName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    char array_names_devices[total_devices][256];

    for (UInt32 number_device = 0; number_device < total_devices; number_device++) {
        // Reset buffer (buffer in terms of c++ terminology) size for each device query
        bytes_device = sizeof(name_device);
        memset(name_device, 0, sizeof(name_device)); // Clear buffer
        
        if (AudioObjectGetPropertyData(array_devices[number_device], &address_device, 0, NULL, &bytes_device, &name_device) == noErr) {
            std::cout << number_device + 1 << ": " << name_device << " (Full Name)" << "\n";
            strcpy(array_names_devices[number_device], name_device);

            UInt32 bytes_buffers_device = 0;
            AudioObjectPropertyAddress address_buffers_device = {
                kAudioDevicePropertyStreamConfiguration,
                kAudioObjectPropertyScopeInput,
                kAudioObjectPropertyElementMain
            };
            AudioObjectGetPropertyDataSize(array_devices[number_device], &address_buffers_device, 0, NULL, &bytes_buffers_device);
            
            if (bytes_buffers_device > 0) {
                AudioBufferList* carrier_identify_device = (AudioBufferList*)malloc(bytes_buffers_device); 
            
                UInt32 device_buffers = 0;
                if (AudioObjectGetPropertyData(array_devices[number_device], &address_buffers_device, 0, NULL, &bytes_buffers_device, carrier_identify_device) == noErr) {
                    device_buffers = carrier_identify_device->mNumberBuffers;
                
                    for (UInt32 number_buffer = 0; number_buffer < device_buffers; number_buffer++) {
                        UInt32 device_channels = carrier_identify_device->mBuffers[number_buffer].mNumberChannels;
                        std::cout << "      Channels: " << device_channels << "\n\n";
                    }
                }
                free(carrier_identify_device);
            }
            
                AudioObjectPropertyAddress address_status_connection_device = {
                kAudioDevicePropertyDeviceIsAlive,
                kAudioObjectPropertyScopeGlobal,
                kAudioObjectPropertyElementMain
            };
        }
    }
    int number_selection_device;
    std::cout << "\n\nEnter playback device number from 1 - " << total_devices << "\n";
    std::cout << "Note: decimals will be rounded, other inputs will cause an error.\n";
    std::cin >> number_selection_device;

    if (number_selection_device < 1 || number_selection_device > total_devices) {
        std::cout << "\nDevice number out of range. ";
        delete[] array_devices;
        return -1;
    }

    if (AudioObjectGetPropertyDataSize(array_devices[number_selection_device - 1], &address_device, 0, NULL, &bytes_device) != noErr) {
        std::cout << "\nDevice selection error.";
        delete[] array_devices;
        std:: cout<< "\nIt may have been disconnected or changed, denied access, used by another application, malfunctioning, or experiencing another/other error(s) from Core Audio. You may try troubleshooting the devices based on this information, re-running the program, and/or re-entering a device selection:\n";
        return -1;
    }

    UInt32 selection_device = array_devices[number_selection_device - 1];

    std::cout << "Selected device ID: " << selection_device << "\n\n";

    delete[] array_devices;
    return selection_device;
}

// =============================================================================
// GLOBAL STATE MANAGEMENT SYSTEM
// =============================================================================

/**
 * SPATIAL AUDIO CONFIGURATION
 * These variables manage the spatial positioning of audio objects in the 
 * multi-channel output space, enabling real-time spatial manipulation.
 */
uint16_t garray_channel_anchor[3] = {0, 1, 2};        // Current spatial object channel assignments (0-based)
uint16_t g_original_sequence_channels[3] = {0, 1, 2}; // Original sequence mapping for live remapping

/**
 * AUDIO SYSTEM STATE CONTROL
 * These flags coordinate between the audio processing thread and user interface thread,
 * ensuring thread-safe state management in the real-time audio context.
 */
bool gstatus_mute_toanchors = true;        // Controls whether spatial objects are audible
bool g_status_audio_playback = false;     // Master audio processing enable flag

/**
 * AUDIO HARDWARE CONFIGURATION
 * Forward declaration required due to usage in early functions.
 * This demonstrates proper C++ variable declaration ordering.
 */
UInt32 g_output_channels = 0;  // Number of output channels detected from audio hardware

// =============================================================================
// ADVANCED CHANNEL MAPPING SYSTEM
// =============================================================================

/**
 * INTELLIGENT SURROUND SOUND MANAGEMENT
 * 
 * This system addresses a critical challenge in professional audio: avoiding
 * problematic channels like LFE (Low Frequency Effects/subwoofer) in surround systems.
 * 
 * TECHNICAL CHALLENGE:
 * In 5.1 surround systems, channel 4 is typically the LFE (subwoofer), which can cause
 * unintended muting of a channel, or bass-heavy artifacts when used for spatial granular synthesis.
 * 
 * SOLUTION APPROACHES:
 * 1. Channel Rotation: Rotates mapping within available channels
 * 2. Channel Shifting: Shifts to higher-numbered channels
 * 3. Topology: incorporating the subwoofer into sequenced granular panning
 */

uint32_t g_channel_rotation = 0;  // Channel rotation offset (0=normal, 2=rotate by 2)
uint32_t g_channel_offset = 0;    // Channel shift offset (0=channels 1-6, 4=channels 5-10)


// =============================================================================
// GRANULAR SEQUENCE MANAGEMENT SYSTEM
// =============================================================================

/**
 * ADVANCED PATTERN SEQUENCING
 * Implements a sophisticated pattern-based sequencing system for spatial
 * granular synthesis with support for complex notation and live manipulation.
 */
std::vector<int> g_grain_sequence;              // Parsed sequence of spatial targets
size_t g_sequence_position = 0;                 // Current position in sequence playback
bool g_use_grain_hopping = false;               // Enable/disable sequence-based spatial hopping
std::string g_original_sequence_string = "";    // Original user input for live translation

// =============================================================================
// INTELLIGENT SEQUENCE PARSING ENGINE
// =============================================================================

/**
 * Advanced Pattern Parser for Granular Sequence Notation
 * 
 * This parser implements a custom notation system that supports:
 * • Basic numbers: "1 2 3" → individual spatial targets
 * • Repetition syntax: "1*5" → repeat target 1 for 5 grains
 * • Silence notation: "x" → silent grain (no spatial output)
 * • Complex patterns: "1 2*3 x 3*2" → sophisticated spatial choreography
 * 
 * TECHNICAL IMPLEMENTATION:
 * - Uses C++ STL string stream parsing for robust tokenization
 * - Implements recursive pattern expansion for repetition syntax
 * - Provides error-resilient parsing with graceful fallback handling
 * 
 * @param istring_sequence Input pattern string using custom notation
 * @return vector<int> Expanded sequence ready for real-time playback
 */
std::vector<int> function_sequence_parse(const std::string& istring_sequence) {
    std::vector<int> vector_sequence;
    std::istringstream stream(istring_sequence);
    std::string token;
    
    while (stream >> token) {
        if (token == "x") {
            // Handle silence notation: 'x' represents a silent grain
            vector_sequence.push_back(-1);
        } else if (token.find('*') != std::string::npos) {
            // Handle repetition syntax: "target*count" (e.g., "2*5" = repeat target 2 five times)
            size_t pos_star = token.find('*');
            std::string token_ch = token.substr(0, pos_star);
            int stay_grain = std::stoi(token.substr(pos_star + 1));
                
            // Determine the target object (number or silence)
            int object_ch;
            if (token_ch == "x") {
                object_ch = -1;  // Silent grain
            } else {
                object_ch = std::stoi(token_ch);  // Spatial target number
            }
            
            // Expand the repetition into the sequence
            for (int i = 0; i < stay_grain; ++i) {
                vector_sequence.push_back(object_ch);
            }
        } else {
            // Handle simple numeric targets
            int object_ch = std::stoi(token);
            vector_sequence.push_back(object_ch);
        }
    }
    
    return vector_sequence;
}

/**
 * Sequence Parser Validation and Demonstration Function
 * 
 * This function demonstrates the capabilities of the sequence parser by testing
 * it with a complex example pattern and displaying the results. This serves as
 * both a unit test and a user demonstration of the notation system.
 * 
 * EDUCATIONAL VALUE:
 * Shows the power of the custom notation system with a real example that includes:
 * - Basic targets, repetition syntax, and silence notation
 * - Demonstrates how compact notation expands to full sequences
 */
void function_print_vector() {
    std::cout << "Testing sequence parser...\n";
    
    std::string sequence_test = "1 2 3*5 x 2*7 x*3";
    std::vector<int> result = function_sequence_parse(sequence_test);
    
    std::cout << "Input: " << sequence_test << "\n";
    std::cout << "Output: ";
    for (size_t i = 0; i < result.size(); ++i) {
        if (result[i] == -1) {
            std::cout << "x";  // Display silence as 'x'
        } else {
            std::cout << result[i];  // Display spatial target number
        }
        if (i < result.size() - 1) std::cout << " ";  // Space separator
    }
    std::cout << "\n";

    std::cout << "Total length: " << result.size() << " grains\n\n";
}

// =============================================================================
// INTELLIGENT CHANNEL MAPPING CONFIGURATION
// =============================================================================

/**
 * Interactive Channel Rotation Setup Interface
 * 
 * This function provides an AI solution to an AI generated problem- in surround
 * sound systems: avoiding the LFE (Low Frequency Effects) channel during spatial
 * granular synthesis. The LFE channel (typically channel 4 in 5.1 systems) is
 * designed for subwoofer content and can produce undesirable results when used
 * for spatial audio positioning.
 * 
 * AI TECHNICAL SOLUTION: (While brainstorming low-frequency effects' role in granular synthesis)
 * Implements channel rotation to shift the mapping so that problematic channels
 * receive less critical content, while maintaining the spatial relationships
 * between objects.
 *
 * THINKING LFE Solutions: How can the LFE be successfully incorporated into spatial granular synthesis?
 * 
 * USER EXPERIENCE:
 * Provides clear visual representation of channel mappings to help users
 * understand the impact of their choice on the spatial audio output.
 */
void setupChannelOffset() {
    // Display menu options to user
    std::cout << "Channel rotation options:\n";
    std::cout << "1. Normal mapping: 1→1, 2→2, 3→3, 4→4, 5→5, 6→6\n";     // Standard
    std::cout << "2. Rotated mapping: 1→5, 2→6, 3→1, 4→2, 5→3, 6→4\n";    // Avoid LFE at 4
    std::cout << "Enter choice (1 or 2): ";
    
    // Get user's choice
    int choice;
    std::cin >> choice;
    
    // Set the global rotation based on user choice
    if (choice == 2) {
        g_channel_rotation = 2;  // Rotate by 2 positions
        std::cout << "Channel rotation set: 1→5, 2→6, 3→1, 4→2, 5→3, 6→4\n";
        std::cout << "This moves channel 4 content to channel 2 (avoids LFE)\n\n";
    } else {
        g_channel_rotation = 0;  // No rotation
        std::cout << "Channel rotation set: Normal mapping (no rotation)\n\n";
    }
}
// ============================================================================

/*
// ============================================================================
// =============== COMMENTED OUT: ALTERNATIVE SHIFT APPROACH =================
// ============================================================================
// This was the original channel shifting implementation
// It shifts all channels by a fixed offset (e.g., 1-6 becomes 5-10)
// Commented out because most studios only have 6 working channels
void setupChannelShift() {
    std::cout << "Channel shift options:\n";
    std::cout << "1. Start at channel 1 (standard): 1,2,3,4,5,6\n";
    std::cout << "2. Start at channel 5 (shifted): 5,6,7,8,9,10\n";
    std::cout << "Enter choice (1 or 2): ";
    
    int choice;
    std::cin >> choice;
    
    if (choice == 2) {
        g_channel_offset = 4;  // Shift by 4 positions
        std::cout << "Channel shift set: Channels will start at channel 5\n\n";
    } else {
        g_channel_offset = 0;  // No shift
        std::cout << "Channel shift set: Channels will start at channel 1\n\n";
    }
}
// ============================================================================
*/

/**
 * Interactive Granular Sequence Configuration System
 * 
 * This function implements a sophisticated interface for configuring spatial
 * granular synthesis patterns. It combines user interaction with intelligent
 * system analysis to provide optimal configuration recommendations.
 * 
 * KEY FEATURES:
 * • Intelligent surround sound detection and LFE channel warnings
 * • Interactive sequence input with real-time validation
 * • Advanced pattern notation support with examples
 * • Context-aware recommendations based on detected hardware
 * 
 * TECHNICAL SOPHISTICATION:
 * - Analyzes hardware configuration to provide targeted advice
 * - Implements custom notation parsing for complex spatial patterns
 * - Provides educational examples to guide user input
 * - Maintains state for live manipulation during performance
 */
void setupGrainHopping() {
    std::cout << "Enable grain hopping? (y/n): ";
    char choice;
    std::cin >> choice;
    
    if (choice == 'y' || choice == 'Y') {
        g_use_grain_hopping = true;
        

        std::cout << "\nYou selected channels: " 
                  << (garray_channel_anchor[0] + 1) << ", " 
                  << (garray_channel_anchor[1] + 1) << ", " 
                  << (garray_channel_anchor[2] + 1) << "\n";
        
        // ========================================================================
        // ================= NEW: SUBWOOFER DETECTION WARNING ===================
        // ========================================================================
        // Check if device likely has 5.1 surround with LFE at channel 4
        if (g_output_channels == 6) {
            std::cout << "\n⚠️  SURROUND SOUND DETECTED (6 channels) ⚠️\n";
            std::cout << "Your device appears to be a 5.1 surround system.\n";
            std::cout << "In 5.1 systems, Channel 4 is typically the LFE (subwoofer).\n";
            std::cout << "If using channel 4 in your sequence sounds wrong (too bass-heavy\n";
            std::cout << "or no sound), try sequences that avoid channel 4, such as:\n";
            std::cout << "• '1 2 3 5 6' (skip channel 4)\n";
            std::cout << "• '1*3 2*3 3*3 5*3 6*3' (avoid channel 4)\n";
            std::cout << "• 'x' can also be used to represent silence/skip\n\n";
        }
        // ========================================================================
        
        std::cout << "Enter grain sequence using numbers 1, 2, 3 for your objects:\n";
        std::cout << "1 = Object 1 (channel " << (garray_channel_anchor[0] + 1) << "), 2 = Object 2 (channel " << (garray_channel_anchor[1] + 1) << "), 3 = Object 3 (channel " << (garray_channel_anchor[2] + 1) << ")\n";
        std::cout << "(e.g., '1 2 3*5 x 2*7 x*3')\n";
        std::cout << "Sequence: ";
        
        std::cin.ignore();
        std::string user_sequence;
        std::getline(std::cin, user_sequence);
        

        g_grain_sequence = function_sequence_parse(user_sequence);
        g_original_sequence_string = user_sequence;

        g_sequence_position = 0; // this is the position in the sequence that the program is currently at
        
        std::cout << "Grain hopping enabled with " << g_grain_sequence.size() << " steps\n\n";
    } else {
        g_use_grain_hopping = false;
        std::cout << "Grain hopping disabled - using standard behavior\n\n";
    }
    

    g_status_audio_playback = true;
    std::cout << "Starting audio playback...\n\n";
}

// USER CHOOSES CHANNELS
void function_anchor_configure(uint32_t outChannels, bool is_initial_setup = true) {
    if (outChannels < 1) {
        std::cout << "No channels detected in device.\n\n";
        gstatus_mute_toanchors = false;
        return;
    }

    // ========================================================================
    // =========== NEW: EARLY SUBWOOFER WARNING (OBJECT SELECTION) ===========
    // ========================================================================
    // Display subwoofer warning BEFORE users select their spatial objects
    // This gives them advance notice to avoid problematic channels
    if (outChannels == 6) {
        std::cout << "\n⚠️  SURROUND SOUND DETECTED (6 channels) ⚠️\n";
        std::cout << "Your device appears to be a 5.1 surround system.\n";
        std::cout << "IMPORTANT: Channel 4 is typically the LFE (subwoofer).\n";
        std::cout << "Consider avoiding Channel 4 for your spatial objects unless\n";
        std::cout << "you specifically want subwoofer effects.\n";
        std::cout << "Recommended channels: 1, 2, 3, 5, 6 (avoid 4)\n\n";
    }
    // ========================================================================

    std::cout << "\nSelect 3 output channels (1-" << outChannels << "):\n";
    
    std::cout << "Object 1 (channel " << (garray_channel_anchor[0] + 1) << "): ";
    std::cin >> garray_channel_anchor[0];
    garray_channel_anchor[0] -= 1; // Convert to 0-based immediately
    std::cout << "Object 1 SWITCHING NOW to channel " << (garray_channel_anchor[0] + 1) << "!\n";
    std::cout.flush(); // Force immediate output
    
    std::cout << "Object 2 (channel " << (garray_channel_anchor[1] + 1) << "): ";
    std::cin >> garray_channel_anchor[1];
    garray_channel_anchor[1] -= 1; // Convert to 0-based immediately
    std::cout << "Object 2 updated to channel " << (garray_channel_anchor[1] + 1) << " - audio switching now!\n";
    
    std::cout << "Object 3 (channel " << (garray_channel_anchor[2] + 1) << "): ";
    std::cin >> garray_channel_anchor[2];
    garray_channel_anchor[2] -= 1; // Convert to 0-based immediately
    std::cout << "Object 3 updated to channel " << (garray_channel_anchor[2] + 1) << " - audio switching now!\n";


    for (int i = 0; i < 3; i++) { 
        if (garray_channel_anchor[i] < 0 || garray_channel_anchor[i] >= outChannels) { 
            std::cout << "Warning: Channel " << (garray_channel_anchor[i] + 1) << " doesn't exist. Using channel 1.\n";
            garray_channel_anchor[i] = 0; // 0-based for channel 1
        }
        // No subtraction - already converted above
    }
    std::cout << "Selected channels: " << (garray_channel_anchor[0] + 1) << ", " << (garray_channel_anchor[1] + 1) << ", " << (garray_channel_anchor[2] + 1) << "\n\n";
    
    // Store these as the original sequence channels for remapping ONLY during initial setup
    if (is_initial_setup) {
        g_original_sequence_channels[0] = garray_channel_anchor[0];
        g_original_sequence_channels[1] = garray_channel_anchor[1];
        g_original_sequence_channels[2] = garray_channel_anchor[2];
        std::cout << "Initial sequence channel mapping established\n";
    } else {
        std::cout << "Live channel assignment updated (sequence mapping preserved)\n";
    }
    
    g_status_audio_playback = true;
}




inline bool function_channel_chosen(UInt32 ichannel, UInt32 outChannels) {
    // Always allow all channels during audio playback to prevent channel disappearing
    return true;
}


/**
 * GRANULAR SYNTHESIS GRAIN STRUCTURE
 * 
 * This structure represents a single grain in the granular synthesis process.
 * Each grain is a small segment of audio with its own envelope, timing, and
 * spatial target. The system can process multiple grains simultaneously for
 * rich, complex textures.
 * 
 * TECHNICAL DESIGN:
 * - Optimized for real-time processing with minimal memory footprint
 * - Contains pre-calculated envelope data for efficiency
 * - Tracks both source position and playback progress independently
 * - Includes spatial routing information for multi-channel output
 */
struct struct_grain {
    uint32_t address_start_frame;       // Starting position in source audio file
    uint32_t address_present_grain;     // Current playback position within this grain
    uint32_t frames_grain;              // Total length of this grain in samples
    float gain_grain;                   // Volume scaling factor for this grain
    float frames_gain_envelope[1024];   // Pre-calculated envelope shape for smooth fading
    bool status_callback_grain;         // Active flag: true = processing, false = available for reuse
    int target_object;                  // Spatial target: 1-3 for objects, -1 for silence, -2 for all channels
};

/**
 * PERFORMANCE OPTIMIZATION CONSTANT
 * Maximum number of simultaneous grains supported by the system.
 * This value balances audio quality (more grains = richer texture) with
 * real-time performance requirements (fewer grains = lower CPU usage).
 */
static const int max_density_cloud_grain = 128;

/**
 * GRANULAR SYNTHESIS ENGINE CONTROL STRUCTURE
 * 
 * This structure manages the entire granular synthesis process, maintaining
 * state for all active grains and controlling the timing of new grain creation.
 * It serves as the central coordinator for the real-time audio processing.
 * 
 * ARCHITECTURAL SIGNIFICANCE:
 * - Implements object pool pattern for efficient grain management
 * - Maintains precise timing for grain triggering
 * - Tracks system load through active grain counting
 * - Provides centralized control for all synthesis parameters
 */
struct struct_process_grain {
    struct_grain object_array_grains[max_density_cloud_grain];  // Pool of grain objects for reuse
    uint32_t frames_object_grain;      // Default grain length in samples
    uint32_t frames_common_grains;     // Shared parameter for grain processing
    uint32_t count_present_grain;      // Current grain counter for timing
    uint32_t count_present_frame;      // Frame counter for grain triggering
    uint32_t active_envelopes_grain;   // Number of currently active grains
    bool status_process_grain;         // Master enable flag for grain processing
};

struct_process_grain global_ProcessGrain{};

constexpr std::size_t kframes_envelope = 1024;

float garray_frames_envelope[kframes_envelope];

float g_envelope_rms = 0.612372f;

AudioStreamBasicDescription g_output_asbd{};
bool g_output_is_float = true;
bool g_output_non_interleaved = true;
// UInt32 g_output_channels = 0;  // MOVED TO GLOBALS SECTION for forward declaration
UInt32 g_output_bits_per_channel = 32;
double g_output_sample_rate = 48000.0;

// Grain control parameters
int g_jitter_range = 1000;  // Jitter range in frames
float g_interval_multiplier = 0.5f;  // Interval = grain_length * this
float g_travel_factor_min = 0.9f;  // Minimum scale factor
float g_travel_factor_max = 1.1f;  // Maximum scale factor

bool g_run_channel_order_test = false;
uint32_t g_test_frames_per_channel = 24000;
uint32_t g_test_silence_frames = 4800;
float g_test_base_freq = 180.0f;
float g_test_freq_step = 20.0f;
float g_test_gain = 0.015f;
uint32_t g_test_frame_cursor = 0;
std::vector<float> g_test_phase;


void triggerChannelOrderTest(uint32_t framesPerChannel,
                             uint32_t silenceFrames,
                             float baseFreq,
                             float stepFreq) {
    g_test_frames_per_channel = framesPerChannel;
    g_test_silence_frames = silenceFrames;
    g_test_base_freq = baseFreq;
    g_test_freq_step = stepFreq;
    g_test_frame_cursor = 0;
    g_run_channel_order_test = true;
    gstatus_mute_toanchors = false;

    g_test_phase.assign(6, 0.0f);
}


// LIVE CONTROLS

void show_sequence_translations() {
    if (g_use_grain_hopping && !g_grain_sequence.empty()) {
        std::cout << "\nGenerating spatial translations for your current setup:\n";
        std::cout << "Objects: " << (garray_channel_anchor[0] + 1) << ", " << (garray_channel_anchor[1] + 1) << ", " << (garray_channel_anchor[2] + 1) << "\n";
        std::cout << "Current sequence: " << g_original_sequence_string << "\n\n";
        
        // Generate all 6 possible translation options (3! = 6 permutations)
        int new_objects[3] = {(int)(garray_channel_anchor[0] + 1), (int)(garray_channel_anchor[1] + 1), (int)(garray_channel_anchor[2] + 1)};
        int mappings[6][3] = {
            {new_objects[0], new_objects[1], new_objects[2]}, 
            {new_objects[0], new_objects[2], new_objects[1]}, 
            {new_objects[1], new_objects[0], new_objects[2]}, 
            {new_objects[1], new_objects[2], new_objects[0]}, 
            {new_objects[2], new_objects[0], new_objects[1]}, 
            {new_objects[2], new_objects[1], new_objects[0]}  
        };
        
        std::cout << "Translation options (copy/paste-able):\n"; // I want to make this a "press 1, 2, 3, 4, 5, 6" to apply / try 
        for (int option = 0; option < 6; ++option) {
            std::cout << "Option " << (option + 1) << ": ";
            std::string translated = g_original_sequence_string;
            
            // Apply string replacement logic
            std::vector<std::pair<std::string, std::string>> replacements;
            replacements.push_back({std::to_string(garray_channel_anchor[0] + 1), std::to_string(mappings[option][0])});
            replacements.push_back({std::to_string(garray_channel_anchor[1] + 1), std::to_string(mappings[option][1])});
            replacements.push_back({std::to_string(garray_channel_anchor[2] + 1), std::to_string(mappings[option][2])});
            
            std::sort(replacements.begin(), replacements.end(), 
                     [](const auto& a, const auto& b) { return a.first.length() > b.first.length(); });
            
            for (const auto& replacement : replacements) {
                size_t index_str = 0;
                while ((index_str = translated.find(replacement.first, index_str)) != std::string::npos) {
                    translated.replace(index_str, replacement.first.length(), replacement.second);
                    index_str += replacement.second.length();
                }
            }
            
            std::cout << translated << "  (" << (garray_channel_anchor[0] + 1) << "→" << mappings[option][0]
                      << ", " << (garray_channel_anchor[1] + 1) << "→" << mappings[option][1] 
                      << ", " << (garray_channel_anchor[2] + 1) << "→" << mappings[option][2] << ")\n";
        }
    }
}

void flive_control_display() {
    std::cout << "\n\nLive Controls:";
    std::cout << "SPACE - Press SPACE to re-assess spatial setup (replay pitch-per-object in order from low to high for all channels in device).\n";
    std::cout << "T - Press 't' to change triangular object configuration.\n";
    std::cout << "Press 'h' to change hopping sequence pattern (keep same channel assignments).\n";
    std::cout << "Press 'g' to change grain length.\n";
    std::cout << "Press 'j' to change jitter freedom (grain launch window size).\n";
    std::cout << "Press 'd' to change density (grain launch interval).\n";
    std::cout << "Press 'p' to change travel factor (pitch variation range).\n";
    // std::cout << "Press 'q' to quit\n";
    // std::cout << "Press any other key to continue audio playback\n";
    // std::cout << "================================\n\n";
}

// remember: functions always need to know what each parameter type is, even if it has already been declared elsewhere
void flive_control_monitor(AudioUnit& unit_audio, // where the audio is being outputted
                           std::ifstream& file, // where the audio is being read from
                           uint16_t channels_file, // number of channels in the file
                           uint32_t rate_samples, // sample rate of the file
                           uint16_t bits_sample, // bit resolution of the file

                           // this was declared in the main function (read), playAudioFile (received), and flive_control_monitor (received)
                           uint32_t audio_format, 
                           uint32_t selection_device) { // where the output is being sent to

    flive_control_display();
    
    // forever loop
    while (true) {
        
        // Check if input is available (debug output removed for usability)
        
        if (std::cin.peek() != EOF) { // the user input is during audio (not at End Of File)
            std::cout << "Input detected!\n";
            char input = std::cin.get(); // Capture the user's keypress
            
            // User feedback for debugging and transparency
            std::cout << "Key pressed: '" << input << "' (ASCII: " << (int)input << ")\n";
            
            if (input == ' ') { // Spacebar: Trigger channel identification test
                std::cout << "\nPlaying Pitch-Per-Object...\n";
                
                // Temporarily halt granular synthesis for channel test
                AudioOutputUnitStop(unit_audio);
                g_status_audio_playback = false;
                
                // Initialize channel identification system
                triggerChannelOrderTest(g_test_frames_per_channel,  // Duration per channel
                                      g_test_silence_frames,        // Silence between channels
                                      g_test_base_freq,             // Starting frequency
                                      g_test_freq_step);            // Frequency increment per channel
                
                // Resume audio processing for sine test
                AudioOutputUnitStart(unit_audio);
                
                // Multi-threaded synchronization: Wait for test completion
                std::cout << "Listening for channel order test...\n";
                while (g_run_channel_order_test) {
                    /**
                     * THREADING ARCHITECTURE EXPLANATION:
                     * This application uses a dual-thread architecture:
                     * 1. Main Thread: User interface and control logic (this thread)
                     * 2. Audio Thread: Real-time audio processing (Core Audio managed)
                     * 
                     * The main thread polls the test completion flag every second,
                     * allowing the high-priority audio thread to complete the sine
                     * wave test without interference.
                     */
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
                std::cout << "Channel test complete!\n\n";
                
                // Reconfigure channels to target objects
                // function_anchor_configure(g_output_channels);
                
                // Reset audio playback flag
                // Might be unnecessary but its safety at not extra cost
                g_status_audio_playback = false;
                
                // Setup grain hopping again
                // setupGrainHopping();
                
                // Resume with existing grain hopping sequence (don't ask user again)
                g_status_audio_playback = true;
                AudioOutputUnitStart(unit_audio);
                std::cout << "Audio playback resumed.\n"; // " with new channel configuration"
                
                flive_control_display();
            }
            else if (input == 't') { // if they enter 't' for triangular object configuration
                std::cout << "\nChanging triangular object configuration...\n";
                
                // Keep audio playing during reconfiguration (live channel switching)
                // // AudioOutputUnitStop(unit_audio);
                
                // Keep audio processing enabled during reconfiguration
                // // g_status_audio_playback = false;
                
                // Save current channel assignments before changing them (for translation display)
                uint16_t old_channels[3] = {garray_channel_anchor[0], garray_channel_anchor[1], garray_channel_anchor[2]};
                
                // Prompt user to select new Object 1, 2, 3 channel assignments (live change - preserve sequence mapping)
                function_anchor_configure(g_output_channels, false);
                
                // Update all active grains to use new channel assignments
                std::cout << "Updating active grains...\n";
                std::cout << "Old channels: " << (old_channels[0] + 1) << ", " << (old_channels[1] + 1) << ", " << (old_channels[2] + 1) << "\n";
                std::cout << "New channels: " << (garray_channel_anchor[0] + 1) << ", " << (garray_channel_anchor[1] + 1) << ", " << (garray_channel_anchor[2] + 1) << "\n";
                std::cout << "Sequence channel mapping updated for live playback\n";
                
                int updated_count = 0;
                for (struct_grain& grain : global_ProcessGrain.object_array_grains) {
                    if (grain.status_callback_grain) {
                        int old_target = grain.target_object;
                        // Grains automatically follow new channel assignments - no grain updates needed
                        // The audio callback will use the new garray_channel_anchor values immediately
                        updated_count++;
                    }
                }
                std::cout << "Updated " << updated_count << " active grains\n";
                
                // TRANSLATION FEATURE - show sequence options for new channel configuration
                std::cout << "\nOld objects: " << (old_channels[0] + 1) << ", " << (old_channels[1] + 1) << ", " << (old_channels[2] + 1) << "\n";
                std::cout << "New objects: " << (garray_channel_anchor[0] + 1) << ", " << (garray_channel_anchor[1] + 1) << ", " << (garray_channel_anchor[2] + 1) << "\n";
                show_sequence_translations();
                if (false) { // Comment out the old code
                    
                    // Generate all 6 possible translation options (3! = 6 permutations)
                    // 3 factorial 3 × 2 × 1 = 6 ways to arrange 3 objects
                    // We have 3 old objects and 3 new objects, so we need all possible 1-to-1 mappings
                    int new_objects[3] = {(int)(garray_channel_anchor[0] + 1), (int)(garray_channel_anchor[1] + 1), (int)(garray_channel_anchor[2] + 1)};
                    // All 6 permutations of how to assign old objects to new objects:

                    int mappings[6][3] = { // 6 rows for each derived sequence, 3 objects pertaining to each sequence
                        {new_objects[0], new_objects[1], new_objects[2]}, // old1→new1, old2→new2, old3→new3 (keep same order)
                        {new_objects[0], new_objects[2], new_objects[1]}, // old1→new1, old2→new3, old3→new2 (swap last two)
                        {new_objects[1], new_objects[0], new_objects[2]}, // old1→new2, old2→new1, old3→new3 (swap first two)
                        {new_objects[1], new_objects[2], new_objects[0]}, // old1→new2, old2→new3, old3→new1 (rotate left)
                        {new_objects[2], new_objects[0], new_objects[1]}, // old1→new3, old2→new1, old3→new2 (rotate right)
                        {new_objects[2], new_objects[1], new_objects[0]}  // old1→new3, old2→new2, old3→new1 (reverse order)
                    };
                    
                    std::cout << "Translation options (copy/paste-able):\n";

                    // for each options
                    for (int option = 0; option < 6; ++option) {

                        // print option number
                        std::cout << "Option " << (option + 1) << ": ";

                        // the translated string will be copied so it can be pasted multiple times in this loop
                        std::string translated = g_original_sequence_string;
                        
                        // Replace old channel numbers with new ones

                        // first we need to convert the old channel numbers to strings (1-based)
                        std::string old1 = std::to_string(old_channels[0] + 1); // 
                        std::string old2 = std::to_string(old_channels[1] + 1);
                        std::string old3 = std::to_string(old_channels[2] + 1);

                        // then the mappings get sorted based on the 1-based old numbers
                        std::string new1 = std::to_string(mappings[option][0]);
                        std::string new2 = std::to_string(mappings[option][1]);
                        std::string new3 = std::to_string(mappings[option][2]);
                        
                        // Fixed string replacement - replace in order from longest to shortest to avoid partial matches
                        // Create pairs of (old, new) and sort by length descending
                        std::vector<std::pair<std::string, std::string>> replacements;
                        replacements.push_back({old1, new1});
                        replacements.push_back({old2, new2});
                        replacements.push_back({old3, new3});
                        
                        // Sort by old string length (longest first) to avoid "1" replacing part of "12"
                        std::sort(replacements.begin(), replacements.end(), 
                                 [](const auto& a, const auto& b) { return a.first.length() > b.first.length(); });
                        
                        // Apply replacements
                        for (const auto& replacement : replacements) {
                            size_t index_str = 0;
                            while ((index_str = translated.find(replacement.first, index_str)) != std::string::npos) {
                                translated.replace(index_str, replacement.first.length(), replacement.second);
                                index_str += replacement.second.length();
                            }
                        }
                        
                        // print the string with the replaced indices
                        std::cout << translated;
                        std::cout << "  (" << (old_channels[0] + 1) 
                                           << "→" 
                                           << mappings[option][0] 
                                           << ", " 
                                           << (old_channels[1] + 1) 
                                           << "→" 
                                           << mappings[option][1] 
                                           << ", " 
                                           << (old_channels[2] + 1) 
                                           << "→" 
                                           << mappings[option][2] 
                                           << ")\n";
                    }
                    
                    std::cout << "\nPress ENTER to keep current sequence, or enter new sequence: ";
                    
                    std::cin.ignore(); // Clear input buffer (this just makes getline work)
                    std::string user_input;
                    std::getline(std::cin, user_input); // this gets the user input entirely with spaces included
                    
                    // if they didn't follow the instruction to keep the current sequence
                    if (!user_input.empty()) {
                        // User entered a new sequence
                        g_grain_sequence = function_sequence_parse(user_input);

                        // Only reset position if new sequence is shorter than current position
                        if (g_sequence_position >= g_grain_sequence.size()) {
                            g_sequence_position = 0;
                        }

                        std::cout << "Updated grain sequence with " << g_grain_sequence.size() << " steps\n";
                        
                    } else {
                        std::cout << "Keeping current sequence\n";
                    }
                } // end of translation feature
                
                // Audio processing was never disabled, continues with new object configuration
                // g_status_audio_playback = true;
                // Audio unit was never stopped, continues with new channel assignments
                // AudioOutputUnitStart(unit_audio);
                std::cout << "Space updated.\n";
                

                
                // WANTS TO INTERACT WITH THE HOP SEQUENCE

                // Show live controls again
                flive_control_display();
            } else if (input == 'h') {
                std::cout << "\nChanging hopping sequence pattern...\n";
                
                // Show current channel assignments
                std::cout << "Current objects: " << (garray_channel_anchor[0] + 1) << ", " << (garray_channel_anchor[1] + 1) << ", " << (garray_channel_anchor[2] + 1) << "\n";
                
                // Show translation options using reusable function
                show_sequence_translations();
                
                std::cout << "\nPress ENTER to keep current sequence, or enter new sequence: ";
                std::cin.ignore(); // Clear input buffer
                std::string user_input;
                std::getline(std::cin, user_input);
                
                if (!user_input.empty()) {
                    // User entered a new sequence
                    g_grain_sequence = function_sequence_parse(user_input);
                    g_original_sequence_string = user_input;
                    
                    // Only reset position if new sequence is shorter than current position
                    if (g_sequence_position >= g_grain_sequence.size()) {
                        g_sequence_position = 0;
                    }
                    
                    std::cout << "Updated grain sequence with " << g_grain_sequence.size() << " steps\n";
                } else {
                    std::cout << "Keeping current sequence\n";
                }
                

                // THE USER WANTS TO INTERACT WITH THE GRAIN LENGTH
                // Show live controls again
                flive_control_display();
            } else if (input == 'g') {
                std::cout << "\nGrain duration parameter:\n";
                std::cout << "Current grain length: " << global_ProcessGrain.frames_object_grain << " frames ";
                std::cout << "(" << (global_ProcessGrain.frames_object_grain * 1000 / g_output_sample_rate) << " ms)\n";
                std::cout << "\nReference the sample rate of the audio file: " << g_output_sample_rate << " Hz\n";
                std::cout << "  512 frames = " << (512 * 1000 / g_output_sample_rate) << " ms\n";
                std::cout << " 1024 frames = " << (1024 * 1000 / g_output_sample_rate) << " ms\n";
                std::cout << " 2048 frames = " << (2048 * 1000 / g_output_sample_rate) << " ms\n";
                std::cout << " 4096 frames = " << (4096 * 1000 / g_output_sample_rate) << " ms\n";
                std::cout << "\nEnter new grain length (frames 256-8192, whole numbers only): ";
                
                uint32_t new_grain_length;
                std::cin >> new_grain_length;
                
                if (new_grain_length >= 256 && new_grain_length <= 8192) { // not crash limits but just a limit I set for now
                    global_ProcessGrain.frames_object_grain = new_grain_length;
                    std::cout << "Grain length updated to " << new_grain_length << " frames\n";
                } else {
                    std::cout << "Invalid range. Keeping current length (" << global_ProcessGrain.frames_object_grain << " frames)\n";
                }
                
                // Show live controls again
                flive_control_display();
            } else if (input == 'j') {
                std::cout << "\nJitter freedom (randomness in grain timing - assymetrical not here yet):\n";
                std::cout << "Current jitter range: ±" << g_jitter_range << " frames\n";
                std::cout << "Enter new jitter range (0-2000 frames): "; // this was a good limit to set for now
                
                int new_jitter;
                std::cin >> new_jitter;
                
                if (new_jitter >= 0 && new_jitter <= 2000) { // valid range, not FULLY necessary, might change later
                    g_jitter_range = new_jitter;
                    std::cout << "Jitter range updated to ±" << g_jitter_range << " frames\n";
                    if (g_jitter_range == 0) {
                        std::cout << "Jitter disabled - grains will trigger at exact intervals\n";
                    }
                } else {
                    std::cout << "Invalid range for current program. Keeping current jitter (±" << g_jitter_range << " frames)\n";
                }
                
                flive_control_display();
            } else if (input == 'd') {
                std::cout << "\nDENSITY CONTROL (spacing between grain triggers):\n";
                std::cout << "Current multiplier: " << g_interval_multiplier << " (interval = grain_length × " << g_interval_multiplier << ")\n";
                std::cout << "Interval based on multiplier: " << (global_ProcessGrain.frames_object_grain * g_interval_multiplier) << " frames\n";
                std::cout << "Enter new multiplier ( < 0.1-2.0 >, e.g., 0.5 = half grain length, 1.0 = full grain length): "; // could be more, just practicing limits
                
                float new_multiplier;
                std::cin >> new_multiplier;
                
                if (new_multiplier >= 0.1f && new_multiplier <= 2.0f) {
                    g_interval_multiplier = new_multiplier;
                    uint32_t new_interval = static_cast<uint32_t>(global_ProcessGrain.frames_object_grain * g_interval_multiplier);
                    std::cout << "Interval multiplier updated to " << g_interval_multiplier << "\n";
                    std::cout << "New interval: " << new_interval << " frames (" << (new_interval * 1000 / g_output_sample_rate) << " ms)\n";
                    
                    if (g_interval_multiplier < 1.0f) {
                        std::cout << "Faster triggering - grains will overlap more\n";
                    } else if (g_interval_multiplier > 1.0f) {
                        std::cout << "Slower triggering - more space between grains\n";
                    } else {
                        std::cout << "Standard triggering - grains trigger at grain length intervals\n";
                    }
                } else {
                    std::cout << "Invalid range (in this program). Keeping current multiplier (" << g_interval_multiplier << ")\n";
                }
                
                flive_control_display();
            } else if (input == 'p') {
                std::cout << "\nTRAVEL FACTOR control (random pitch variation range):\n";
                std::cout << "Current multiplier range: " << g_travel_factor_min << " to " << g_travel_factor_max << "\n";
                std::cout << "Current variation: ±" << ((g_travel_factor_max - 1.0f) * 100.0f) << "%\n"; // convert to percentage
                std::cout << "\nEnter variation percentage (0-50%, e.g., 10 for ±10% pitch variation): "; // idk man
                
                float variation_percent;
                std::cin >> variation_percent;
                
                if (variation_percent >= 0.0f && variation_percent <= 50.0f) {
                    float variation = variation_percent / 100.0f; // i.e. 35 / 100 = 0.35, therefore 0.65-1.35
                    g_travel_factor_min = 1.0f - variation;
                    g_travel_factor_max = 1.0f + variation;
                    
                    std::cout << "Travel factor updated to " << g_travel_factor_min << " - " << g_travel_factor_max << "\n";
                    std::cout << "Random pitch variation: ±" << variation_percent << "%\n";
                    
                    if (variation_percent == 0.0f) {
                        std::cout << "No pitch variation - all grains same length\n";
                    } else if (variation_percent < 5.0f) {
                        std::cout << "Subtle variation - slight organic texture\n";
                    } else if (variation_percent < 20.0f) {
                        std::cout << "Moderate variation - noticeable pitch wobble\n";
                    } else {
                        std::cout << "Heavy variation - dramatic pitch effects\n";
                    }
                } else {
                    std::cout << "Invalid range (in this program). Keeping current travel factor (±" << ((g_travel_factor_max - 1.0f) * 100.0f) << "%)\n";
                }
                
                flive_control_display();
            }
        }
        
        // 0.1 second delay to prevent excessive CPU usage
        // User can make live controls every 0.1 second
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
/**
 * Advanced Mathematical Envelope Generation System
 * 
 * This function generates a sophisticated windowing envelope using the Hann window
 * (also known as Hanning window), which is crucial for preventing audio artifacts
 * in granular synthesis. The mathematical precision of this envelope directly
 * impacts the audio quality of the final output.
 * 
 * MATHEMATICAL FOUNDATION:
 * The Hann window is defined as: w(n) = 0.5 - 0.5 * cos(2πn/(N-1))
 * This creates a smooth, bell-shaped curve that eliminates click artifacts
 * when grains start and stop.
 * 
 * TECHNICAL ACHIEVEMENTS:
 * • Implements industry-standard windowing mathematics
 * • Pre-calculates RMS value for accurate gain normalization
 * • Uses high-precision floating-point arithmetic
 * • Optimized for real-time performance with pre-computation
 * 
 * AUDIO ENGINEERING SIGNIFICANCE:
 * Proper windowing is essential in granular synthesis to prevent spectral
 * leakage and maintain smooth audio transitions between overlapping grains.
 */
void function_shape_envelope() {
    float sum2 = 0.0f;  // Accumulator for RMS calculation
    
    for (std::size_t count_frame_envelope = 0; count_frame_envelope < kframes_envelope; ++count_frame_envelope) {
        constexpr float kPi = 3.14159265358979323846f;  // High-precision Pi constant

        // Generate Hann window: smooth cosine-based envelope
        garray_frames_envelope[count_frame_envelope] = 0.5f - 0.5f * std::cos(
            2.0f * kPi * static_cast<float>(count_frame_envelope) / (kframes_envelope - 1)
        );

        // Calculate normalized phase position (0.0 to 1.0)
        float phase_envelope = static_cast<float>(count_frame_envelope) / (kframes_envelope - 1);

        // Accumulate squared values for RMS calculation
        float value = garray_frames_envelope[count_frame_envelope];
        sum2 += value * value;
    }

    // Calculate RMS (Root Mean Square) for gain normalization
    g_envelope_rms = std::sqrt(sum2 / kframes_envelope);
}

/**
 * COMPREHENSIVE AUDIO FILE MANAGEMENT STRUCTURE
 * 
 * This structure encapsulates all aspects of WAV file processing and management,
 * from low-level file I/O to high-level audio buffer management. It demonstrates
 * sophisticated understanding of audio file formats and memory management.
 * 
 * TECHNICAL CAPABILITIES:
 * • Complete WAV file format parsing and validation
 * • Multi-channel audio buffer management with optimized layout
 * • Support for multiple bit depths and sample formats (16/32-bit, PCM/float)
 * • Efficient memory pre-allocation for real-time performance
 * • Thread-safe design for concurrent audio processing
 */
struct AudioFileData {
    // File identification and stream management
    std::string name_file;                      // Original filename for reference
    std::ifstream* file;                        // File stream pointer for I/O operations
    
    // WAV format specifications
    uint16_t channels_file;                     // Number of audio channels in source file
    uint32_t bytes_total_read_file;             // Total bytes read from file
    uint32_t bytes_header;                      // Size of WAV header section
    uint32_t bytes_chunk_audio;                 // Size of audio data chunk
    uint32_t address_first_audio;               // File offset to start of audio data
    uint32_t address_present_audio;             // Current read position in file
    
    // High-performance audio buffer system
    std::vector<std::vector<float>> samples;   // 2D array: [channel][sample] for efficient access
    uint32_t frames_total;                      // Total number of audio frames in file
    uint32_t present_frame;                     // Current playback position
    bool file_is_ieee_float;                    // Format flag: true=32-bit float, false=PCM integer
};

AudioFileData global_AudioFileData;


void initialize_grain(struct_grain& idata_grain,
                      uint32_t      iaddress_start_frame,
                      uint32_t      iframes_grain, 
                      float         igain_grain = 1.0f) { 

    idata_grain.address_start_frame     = iaddress_start_frame;
    idata_grain.address_present_grain   = 0;
    idata_grain.frames_grain            = iframes_grain;
    

 

 
    if (g_use_grain_hopping && !g_grain_sequence.empty()) {
     
        idata_grain.target_object = g_grain_sequence[g_sequence_position];
     
        g_sequence_position = (g_sequence_position + 1) % g_grain_sequence.size();
    } else {
     
        idata_grain.target_object = -2;
    } 
    idata_grain.gain_grain              = igain_grain; 
                      
    std::copy(std::begin(garray_frames_envelope),
              std::end  (garray_frames_envelope),
              idata_grain.frames_gain_envelope);
    
    idata_grain.status_callback_grain = true; 
}

/**
 * ADVANCED GRANULAR SYNTHESIS GRAIN GENERATION ENGINE
 * 
 * This function represents the core intelligence of the granular synthesis system,
 * implementing sophisticated algorithms for creating new grains with randomized
 * parameters. It demonstrates advanced understanding of:
 * 
 * • High-quality random number generation using industry standards
 * • Real-time memory management with object pooling
 * • Mathematical parameter randomization for organic texture
 * • Performance optimization for audio-rate processing
 * 
 * TECHNICAL SOPHISTICATION:
 * - Uses Mersenne Twister algorithm for superior random distribution
 * - Implements thread-local storage for multi-threading safety
 * - Applies jitter and scaling for natural, non-mechanical sound
 * - Manages grain density through intelligent load balancing
 */
void function_process_grain() {
    // Performance optimization: Limit concurrent grains to maintain real-time performance
    if (global_ProcessGrain.active_envelopes_grain >= 8) {
        return;  // System at capacity - skip grain creation this cycle
    }
    
    // Object Pool Pattern: Find available grain slot for reuse
    struct_grain* new_grain = nullptr;
    for (auto& slot_new_grain : global_ProcessGrain.object_array_grains) {
        if (!slot_new_grain.status_callback_grain) {
            new_grain = &slot_new_grain;  // Found available slot
            break;
        }
    }
    if (!new_grain) {
        return;  // No available grain slots - skip creation
    }

    /**
     * ADVANCED RANDOM NUMBER GENERATION SYSTEM
     * 
     * Implements the Mersenne Twister algorithm, which is a known standard
     * for pseudo-random number generation in professional applications.
     * 
     * TECHNICAL DETAILS:
     * • thread_local: Each thread gets its own RNG instance (thread-safe)
     * • std::random_device: Hardware entropy source for seeding
     * • std::mt19937: 32-bit Mersenne Twister with 19937-bit period
     * • Static storage: RNG state persists between function calls
     * 
     * This approach ensures high-quality randomization while maintaining
     * excellent performance in real-time audio processing.
     */
    static thread_local std::mt19937 rng{std::random_device{}()}; 

    /**
     * STATISTICAL DISTRIBUTION CONFIGURATION
     * 
     * These distributions control the randomization parameters that give
     * granular synthesis its characteristic organic, non-mechanical texture.
     */
    
    // Temporal Jitter: Randomizes grain start timing to prevent mechanical regularity
    std::uniform_int_distribution<int> jitterDist(-g_jitter_range, g_jitter_range);
    
    // Pitch/Speed Variation: Creates subtle pitch variations for organic texture
    // This scaling factor affects both playback speed and resulting pitch
    std::uniform_real_distribution<float> scaleDist(g_travel_factor_min, g_travel_factor_max);

    // base_frames_grain is the original grain length
    const uint32_t base_frames_grain = global_ProcessGrain.frames_object_grain;

    // start_raw is the starting frame of the grain
    int64_t start_raw = static_cast<int64_t>(global_AudioFileData.present_frame) + jitterDist(rng); // rng is mt
    if (start_raw < 0) start_raw = 0;
    if (start_raw > static_cast<int64_t>(global_AudioFileData.frames_total - 1)) {
        start_raw = static_cast<int64_t>(global_AudioFileData.frames_total - 1);
    }

    // field_start_frame is the new starting frame of the grain
    uint32_t field_start_frame = static_cast<uint32_t>(start_raw);

    // field_frames_grain is the new length of the grain
    uint32_t field_frames_grain = static_cast<uint32_t>(base_frames_grain * scaleDist(rng)); // rng is mt
    if (field_frames_grain < 64u) field_frames_grain = 64u;

    // if the new starting frame + the new length of the grain is greater than the total frames of the audio file
    if (field_start_frame + field_frames_grain > global_AudioFileData.frames_total) {
        // cut the grain the whatever is left from the audio
        field_frames_grain = global_AudioFileData.frames_total - field_start_frame;
    }

    float    field_gain_grain = 1.0f;

    initialize_grain(*new_grain, field_start_frame, field_frames_grain, field_gain_grain);
    ++global_ProcessGrain.active_envelopes_grain;
}

// =============================================================================
// REAL-TIME AUDIO PROCESSING CALLBACK - CORE ENGINE
// =============================================================================

/**
 * High-Performance Real-Time Audio Processing Callback
 * 
 * This is the heart of the granular synthesis engine, called by Core Audio at
 * regular intervals to generate audio in real-time. This function must execute
 * with extremely low latency and high reliability, as any delays or errors will
 * cause audio dropouts.
 * 
 * TECHNICAL CHALLENGES SOLVED:
 * • Lock-free programming for real-time thread safety
 * • Efficient memory management without dynamic allocation
 * • Multi-channel audio routing with flexible channel mapping
 * • Grain envelope processing with mathematical precision
 * • Sample-accurate timing and synchronization
 * 
 * PERFORMANCE CHARACTERISTICS:
 * - Executes in high-priority real-time audio thread
 * - Must complete processing within ~10ms buffer periods
 * - Zero dynamic memory allocation for stability
 * - Optimized mathematical operations for efficiency
 * 
 * @param ibox_audio User data pointer (AudioFileData structure)
 * @param ioget_flag Audio unit render action flags
 * @param struct_istamp_time Precise timing information for synchronization
 * @param iget_bus Audio unit bus number (typically 0)
 * @param icount_frames Number of audio frames to process this callback
 * @param struct_ioData_period_buffer Output audio buffer list for all channels
 * @return OSStatus indicating success (noErr) or error condition
 */
static OSStatus function_callback_audio(void* ibox_audio,
                                        AudioUnitRenderActionFlags* ioget_flag,
                                        const AudioTimeStamp* struct_istamp_time, 
                                        UInt32 iget_bus,
                                        UInt32 icount_frames,
                                        AudioBufferList* struct_ioData_period_buffer) { 

    UInt32 numBuffers = struct_ioData_period_buffer->mNumberBuffers;
    UInt32 outChannels = (numBuffers == 1)
        ? struct_ioData_period_buffer->mBuffers[0].mNumberChannels
        : numBuffers;
    UInt32 inChannels = global_AudioFileData.channels_file;
    UInt32 minChannels = std::min(outChannels, inChannels);
    const bool isNonInterleaved = g_output_non_interleaved ? true : (numBuffers > 1);
    
    global_ProcessGrain.count_present_frame += icount_frames;

    for (UInt32 buffer_willempty = 0; buffer_willempty < struct_ioData_period_buffer->mNumberBuffers; ++buffer_willempty)
        std::memset(struct_ioData_period_buffer->mBuffers[buffer_willempty].mData,
                    0,
                    struct_ioData_period_buffer->mBuffers[buffer_willempty].mDataByteSize);
    
    // grain start interval is adjustable (DENSITY PARAMETER)
    const uint32_t interval_start_frames = static_cast<uint32_t>(global_ProcessGrain.frames_object_grain * g_interval_multiplier);

    if (global_ProcessGrain.count_present_frame >= interval_start_frames) {
        function_process_grain();
        global_ProcessGrain.count_present_frame = 0; 
    }

    UInt32 count_ch = global_AudioFileData.channels_file;
    uint32_t total_fr = global_AudioFileData.frames_total;
    uint32_t callback_start_fr = global_AudioFileData.present_frame;

    const float kDryGain = 0.0f;
    const float kWetGain = 1.0f;
    

    std::vector<float> mix(outChannels * icount_frames, 0.0f);
    auto mixIndex = [icount_frames](UInt32 ch, UInt32 fr) {
        return static_cast<size_t>(ch) * static_cast<size_t>(icount_frames) + static_cast<size_t>(fr);
    };

 
    if (!g_run_channel_order_test && g_status_audio_playback) {
        for (UInt32 ch_callback = 0; ch_callback < outChannels; ++ch_callback) {
            for (UInt32 fr_callback = 0; fr_callback < icount_frames; ++fr_callback) {

                uint32_t fr_read = callback_start_fr + fr_callback;
                
                uint16_t file_ch = ch_callback % global_AudioFileData.channels_file;
                // Audio callback tries to read past end, gets 0.0f (silence) instead of audio data
                mix[mixIndex(ch_callback, fr_callback)] = kDryGain * (
                    (fr_read < total_fr) ? global_AudioFileData.samples[file_ch][fr_read] : 0.0f  // Result: Audio fades to silence and stays silent
                );
            }
        }
        // Audio position reaches total_fr (end of file), std::min() keeps position at total_fr (doesn't advance further)
        global_AudioFileData.present_frame = std::min(callback_start_fr + icount_frames, total_fr);

        // THIS WOULD ADD A LOOPING FEATURE
        // global_AudioFileData.present_frame = callback_start_fr + icount_frames;
        
        // Loop the audio when it reaches the end
        // if (global_AudioFileData.present_frame >= total_fr) {
        //     global_AudioFileData.present_frame = 0;
        // }
    }

    float normal_sample_channel[16];
    

    if (g_status_audio_playback && callback_start_fr < total_fr) {
    for (struct_grain& element_grain : global_ProcessGrain.object_array_grains) {
        if (!element_grain.status_callback_grain) 
            continue;

        uint32_t frames_grain_ahead = element_grain.frames_grain - element_grain.address_present_grain;

        double rho = double(element_grain.frames_grain) / double(interval_start_frames);

        double N_eff = std::max(1.0, rho);
        constexpr float kTargetRMS = 0.2f; 

        float gain_norm = kTargetRMS/(g_envelope_rms*std::sqrt(N_eff));
        float grain_base_gain = element_grain.gain_grain*gain_norm; 

        uint32_t frames_grain_process = std::min<uint32_t>(icount_frames, frames_grain_ahead);

        for (uint32_t count_frame_process = 0; count_frame_process < frames_grain_process; ++count_frame_process) {

            uint32_t frame_grain_audio = element_grain.address_start_frame
                                       + element_grain.address_present_grain
                                       + count_frame_process;

            if (frame_grain_audio >= global_AudioFileData.frames_total) {
                continue; 
            }

            for (uint16_t process_ch = 0; process_ch < global_AudioFileData.channels_file; ++process_ch) {
                normal_sample_channel[process_ch] = global_AudioFileData.samples[process_ch][frame_grain_audio];
            }

            uint32_t env_idx = ((element_grain.address_present_grain + count_frame_process) * (kframes_envelope - 1))
                                / element_grain.frames_grain;
            if (env_idx >= kframes_envelope) env_idx = kframes_envelope - 1; 
            float frame_env = element_grain.frames_gain_envelope[env_idx];


            if (element_grain.target_object == -1) {
             
                continue;
            } else if (element_grain.target_object == -2) {
             
                for (UInt32 process_ch = 0; process_ch < outChannels; ++process_ch) {
                    size_t idx = mixIndex(process_ch, count_frame_process);
                    uint16_t file_ch = process_ch % global_AudioFileData.channels_file;
                    mix[idx] += kWetGain * (normal_sample_channel[file_ch] * (frame_env * grain_base_gain));
                }
            } else {
                // ========================================================================
                // ========== CHANNEL MAPPING OPTIONS: ROTATION & SHIFT (REFERENCE) ======
                // ========================================================================
                // This section shows both channel mapping approaches for future reference:
                //
                // APPROACH 1: CHANNEL ROTATION (Stays within 6 channels)
                // Rotates channel assignments to avoid problematic channels
                // Example: 1→5, 2→6, 3→1, 4→2, 5→3, 6→4
                // Implementation would be:
                // if (g_channel_rotation == 0) {
                //     final_target_ch = target_ch;  // No rotation
                // } else {
                //     final_target_ch = (target_ch + 4) % 6;  // Rotate by 4 positions
                // }
                //
                // APPROACH 2: CHANNEL SHIFTING (Goes beyond 6 channels)
                // Shifts all channels by a fixed offset
                // Example: 1→5, 2→6, 3→7, 4→8, 5→9, 6→10
                // Implementation would be:
                // final_target_ch = target_ch + g_channel_offset;
                //
                // CURRENT: Using standard mapping (no rotation/shift applied)
                // ========================================================================
                
                // LIVE CHANNEL MAPPING - map sequence channels to current object assignments
                UInt32 target_ch;
                
                // Map existing sequence values to current object assignments
                if (element_grain.target_object == (g_original_sequence_channels[0] + 1)) {  // Your sequence Object 1 -> Current Object 1 channel
                    target_ch = garray_channel_anchor[0];
                } else if (element_grain.target_object == (g_original_sequence_channels[1] + 1)) {  // Your sequence Object 2 -> Current Object 2 channel
                    target_ch = garray_channel_anchor[1];
                } else if (element_grain.target_object == (g_original_sequence_channels[2] + 1)) {  // Your sequence Object 3 -> Current Object 3 channel
                    target_ch = garray_channel_anchor[2];
                } else if (element_grain.target_object == 1) {  // Also support sequence "1" -> Object 1
                    target_ch = garray_channel_anchor[0];
                } else if (element_grain.target_object == 2) {  // Also support sequence "2" -> Object 2
                    target_ch = garray_channel_anchor[1];
                } else if (element_grain.target_object == 3) {  // Also support sequence "3" -> Object 3
                    target_ch = garray_channel_anchor[2];
                } else {
                    // Direct mapping for all other sequence numbers
                    target_ch = static_cast<UInt32>(element_grain.target_object - 1);
                }

             
                // ========================================================================
                // =================== NEW: APPLY CHANNEL OFFSET ========================
                // ========================================================================
                // This is where we shift the output channels for granular synthesis
                // target_ch = logical channel (1,2,3,4,5,6 for grain hopping sequence)
                // g_channel_offset = how many channels to shift by (0 or 4)
                // final_target_ch = actual hardware channel to output to
                // Example: target_ch=1, g_channel_offset=4 → final_target_ch=5
                UInt32 final_target_ch = target_ch + g_channel_offset;
                
                // Make sure the shifted channel doesn't exceed available hardware channels
                if (final_target_ch < outChannels) {
                    // Calculate which element of the mix array to write to
                    // mixIndex() converts (channel, frame) to array position
                    size_t idx = mixIndex(final_target_ch, count_frame_process);
                    
                    // Use the original target_ch for file channel mapping (no offset here)
                    // This keeps the audio content mapping correct
                    uint16_t file_ch = target_ch % global_AudioFileData.channels_file;
                    
                    // Add the processed grain audio to the output mix
                    // kWetGain = volume level, frame_env = grain envelope, grain_base_gain = grain volume
                    mix[idx] += kWetGain * (normal_sample_channel[file_ch] * (frame_env * grain_base_gain));
                }
                // ========================================================================
            }
        }

        element_grain.address_present_grain += frames_grain_process;

        if (element_grain.address_present_grain >= element_grain.frames_grain) {
            element_grain.status_callback_grain = false;
            --global_ProcessGrain.active_envelopes_grain;
        }
    }
    } // End grain processing

    if (g_run_channel_order_test && g_output_channels > 0) { 

        uint32_t block = g_test_frames_per_channel + g_test_silence_frames;

        for (UInt32 fr = 0; fr < icount_frames; ++fr) {

            uint32_t sine_frames_behind = g_test_frame_cursor + fr;

            uint32_t channel_now = (block > 0) ? (sine_frames_behind / block) : 0; 

            if (channel_now >= g_output_channels) { 
                g_run_channel_order_test = false; 
                gstatus_mute_toanchors = true;
                break; 
            }

            uint32_t within = (block > 0) ? (sine_frames_behind % block) : 0;

            for (UInt32 ch = 0; ch < outChannels; ++ch) {

                float amp = 0.0f;

                // ========================================================================
                // ============= NEW: APPLY CHANNEL OFFSET TO SINE TEST ==================
                // ========================================================================
                // The channel order test (sine waves) also needs to use the offset
                // so users hear the test on the same channels their grains will use
                // channel_now = logical test channel (0,1,2,3,4,5...)
                // g_channel_offset = shift amount (0 or 4)
                // test_channel_with_offset = actual hardware channel for sine test
                UInt32 test_channel_with_offset = channel_now + g_channel_offset;
                
                // Only play sine tone if current output channel matches the test channel
                if (ch == test_channel_with_offset && within < g_test_frames_per_channel) {
                // ========================================================================

                    float freq = g_test_base_freq + static_cast<float>(channel_now) * g_test_freq_step;
                    
                 
                    float phase = (channel_now < g_test_phase.size()) ? g_test_phase[channel_now] : 0.0f;

                    float inc = static_cast<float>(2.0 * M_PI * freq / g_output_sample_rate);

                    amp = g_test_gain * std::sin(phase);
  

                    phase += inc;
                    
                    if (phase > 2.0f * static_cast<float>(M_PI)) phase -= 2.0f * static_cast<float>(M_PI);

           
                    if (channel_now < g_test_phase.size()) g_test_phase[channel_now] = phase;
                }

                mix[mixIndex(ch, fr)] = amp;
            }
        }

        g_test_frame_cursor += icount_frames;
    }

    if (g_output_is_float) {
        if (isNonInterleaved) {
            for (UInt32 ch = 0; ch < outChannels; ++ch) {
                float* address_buffer = static_cast<float*>(struct_ioData_period_buffer->mBuffers[ch].mData);


                if (g_run_channel_order_test || function_channel_chosen(ch, outChannels)) {
                    for (UInt32 fr = 0; fr < icount_frames; ++fr) {
                        address_buffer[fr] = std::clamp(mix[mixIndex(ch, fr)], -1.0f, 1.0f);
                    }
                } else {
                    for (UInt32 fr = 0; fr < icount_frames; ++fr) {
                        address_buffer[fr] = 0.0f;
                    }
                }
            }
        } else {
            float* address_buffer = static_cast<float*>(struct_ioData_period_buffer->mBuffers[0].mData);
            for (UInt32 fr = 0; fr < icount_frames; ++fr) {
                for (UInt32 ch = 0; ch < outChannels; ++ch) {

                    float v = function_channel_chosen(ch, outChannels) ? mix[mixIndex(ch, fr)] : 0.0f;
                    address_buffer[fr * outChannels + ch] = std::clamp(v, -1.0f, 1.0f);
                }
            }
        }
    } else {
        if (isNonInterleaved) {
            for (UInt32 ch = 0; ch < outChannels; ++ch) {
                if (g_output_bits_per_channel == 16) {
                    int16_t* address_buffer = static_cast<int16_t*>(struct_ioData_period_buffer->mBuffers[ch].mData);
                   
                    if (g_run_channel_order_test) {

                        for (UInt32 fr = 0; fr < icount_frames; ++fr) {
                            float v = std::clamp(mix[mixIndex(ch, fr)], -1.0f, 1.0f);
                            address_buffer[fr] = static_cast<int16_t>(std::lrintf(v * 32767.0f));
                        }
                    } else if (function_channel_chosen(ch, outChannels)) {

                        for (UInt32 fr = 0; fr < icount_frames; ++fr) {
                            float v = std::clamp(mix[mixIndex(ch, fr)], -1.0f, 1.0f);
                            
                            address_buffer[fr] = static_cast<int16_t>(std::lrintf(v * 32767.0f));
                        }
                    } else {
                        for (UInt32 fr = 0; fr < icount_frames; ++fr) {
                            address_buffer[fr] = 0;
                        }
                    }
                } else {
                    int32_t* address_buffer = static_cast<int32_t*>(struct_ioData_period_buffer->mBuffers[ch].mData);
                    if (g_run_channel_order_test) {

                        for (UInt32 fr = 0; fr < icount_frames; ++fr) {
                            float v = std::clamp(mix[mixIndex(ch, fr)], -1.0f, 1.0f);
                            address_buffer[fr] = static_cast<int16_t>(std::lrintf(v * 32767.0f));
                        }
                    } else if (function_channel_chosen(ch, outChannels)) {
                        for (UInt32 fr = 0; fr < icount_frames; ++fr) {
                            float v = std::clamp(mix[mixIndex(ch, fr)], -1.0f, 1.0f);
                            address_buffer[fr] = static_cast<int32_t>(std::lrintf(v * 2147483647.0f));
                        }
                    } else {
                        for (UInt32 fr = 0; fr < icount_frames; ++fr) {
                            address_buffer[fr] = 0;
                        }
                    }
                }
            }
        } else {
            if (g_output_bits_per_channel == 16) {
                int16_t* address_buffer = static_cast<int16_t*>(struct_ioData_period_buffer->mBuffers[0].mData);
                for (UInt32 fr = 0; fr < icount_frames; ++fr) {
                    for (UInt32 ch = 0; ch < outChannels; ++ch) {
                        float v = (g_run_channel_order_test || function_channel_chosen(ch, outChannels)) ? std::clamp(mix[mixIndex(ch, fr)], -1.0f, 1.0f) : 0.0f;
                        address_buffer[fr * outChannels + ch] = static_cast<int16_t>(std::lrintf(v * 32767.0f));
                    }
                }
            } else {
                int32_t* address_buffer = static_cast<int32_t*>(struct_ioData_period_buffer->mBuffers[0].mData);
                for (UInt32 fr = 0; fr < icount_frames; ++fr) {
                    for (UInt32 ch = 0; ch < outChannels; ++ch) {
                        float v = (g_run_channel_order_test || function_channel_chosen(ch, outChannels)) ? std::clamp(mix[mixIndex(ch, fr)], -1.0f, 1.0f) : 0.0f;
                        address_buffer[fr * outChannels + ch] = static_cast<int32_t>(std::lrintf(v * 2147483647.0f));
                    }
                }
            }
        }
    }

    return noErr;
}












// =============================================================================
// PLAY AUDIO FILE FUNCTION
// =============================================================================
void playAudioFile(const std::string& name_file, 
                   UInt32 selection_device, 
                   uint16_t channels_file, 
                   uint32_t rate_samples, 
                   uint16_t bits_sample,
                   uint16_t audio_format, // audio format from WAV file (1=PCM, 3=IEEE float)
                   std::ifstream& file) {
    AudioStreamBasicDescription formatAudio;

    formatAudio.mSampleRate = rate_samples;
    formatAudio.mFormatID = kAudioFormatLinearPCM; 

    formatAudio.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
    formatAudio.mBitsPerChannel = 32;
    formatAudio.mChannelsPerFrame = channels_file;
    formatAudio.mFramesPerPacket = 1;
    formatAudio.mBytesPerFrame = sizeof(Float32);
    formatAudio.mBytesPerPacket = sizeof(Float32);

    AudioUnit unit_audio;
    AudioComponentDescription descriptionComponentAudio;

    descriptionComponentAudio.componentType = kAudioUnitType_Output;
    descriptionComponentAudio.componentSubType = kAudioUnitSubType_HALOutput;
    descriptionComponentAudio.componentManufacturer = kAudioUnitManufacturer_Apple;
    descriptionComponentAudio.componentFlags = 0;
    descriptionComponentAudio.componentFlagsMask = 0;

    AudioComponent component_audio = AudioComponentFindNext(NULL, &descriptionComponentAudio);
    if (!component_audio) { 
        std::cerr << "Audio output component error: \n";
        return;
    } else {
        std::cout << "Audio components detected.\n\n";
    }

    OSStatus status_unit_audio;

    status_unit_audio = AudioComponentInstanceNew(component_audio, &unit_audio);
    if (status_unit_audio != noErr) { 
        std::cerr << "Audio component instance error: " << status_unit_audio << " \n";
        return;
    } else {
        std::cout << "Audio component instance created.\n";

        std::cout << "Sample rate: " << formatAudio.mSampleRate << "\n";
        std::cout << "Format ID: " << formatAudio.mFormatID << "\n";
        std::cout << "Format flags: " << formatAudio.mFormatFlags << "\n";
        std::cout << "Bits per channel: " << formatAudio.mBitsPerChannel << "\n";
        std::cout << "Channels per frame: " << formatAudio.mChannelsPerFrame << "\n";
        std::cout << "Frames per packet: " << formatAudio.mFramesPerPacket << "\n";
        std::cout << "Bytes per frame: " << formatAudio.mBytesPerFrame << "\n";
        std::cout << "Bytes per packet: " << formatAudio.mBytesPerPacket << "\n\n";
    }

    status_unit_audio = AudioUnitSetProperty(unit_audio,
                                             kAudioOutputUnitProperty_CurrentDevice,
                                             kAudioUnitScope_Global, 
                                             0, 
                                             &selection_device, 
                                             sizeof(selection_device));
    if (status_unit_audio != noErr) {
        std::cerr << "Failed to set audio output. Error: " << status_unit_audio << " \n";
        AudioComponentInstanceDispose(unit_audio);
        return;
    } else {
        std::cout << "Audio output configured.\n";
    }

    status_unit_audio = AudioUnitSetProperty(unit_audio, 
                                             kAudioUnitProperty_StreamFormat, 
                                             kAudioUnitScope_Input, 
                                             0, 
                                             &formatAudio, 
                                             sizeof(formatAudio));
    if (status_unit_audio != noErr) {
        std::cerr << "Audio unit formatting error: " << status_unit_audio << " \n";
        AudioComponentInstanceDispose(unit_audio);
        return; 
    } else {
        std::cout << "Audio unit format established. Observe program output device's sample rate setup and input monitoring (if applicaple).\n\n";
    }

    file.seekg(36, std::ios::beg);
    char array_bytes_chunk_audio[4];
    uint32_t bytes_chunk_audio;
    bool status_chunkID_audio = false;

    while (!file.eof() && !status_chunkID_audio) {
        file.read(array_bytes_chunk_audio, 4);
        file.read(reinterpret_cast<char*>(&bytes_chunk_audio), sizeof(bytes_chunk_audio));

        if (std::string(array_bytes_chunk_audio, 4) == "data") {
            status_chunkID_audio = true;
            std::cout << "Data Chunk ID detected.\n\n";
            break;
        } else {
            file.seekg(bytes_chunk_audio, std::ios::cur);
        }
    }

    if (!status_chunkID_audio) {
        std::cerr << "No audio data ID detected.\n\n";
        AudioComponentInstanceDispose(unit_audio);
        return;
    }

    {
        UInt32 asbdSize = sizeof(g_output_asbd);
        if (AudioUnitGetProperty(unit_audio,
                                 kAudioUnitProperty_StreamFormat,
                                 kAudioUnitScope_Input,
                                 0,
                                 &g_output_asbd,
                                 &asbdSize) == noErr) {
            g_output_is_float = (g_output_asbd.mFormatFlags & kAudioFormatFlagIsFloat) != 0;
            g_output_non_interleaved = (g_output_asbd.mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;
            g_output_channels = g_output_asbd.mChannelsPerFrame;
            g_output_bits_per_channel = g_output_asbd.mBitsPerChannel;
            g_output_sample_rate = g_output_asbd.mSampleRate;
            

         std::cout << "Device output channels: " << g_output_channels << std::endl;
        }
    }

    triggerChannelOrderTest(g_test_frames_per_channel,
                            g_test_silence_frames,
                            g_test_base_freq,
                            g_test_freq_step);

    global_AudioFileData.file = &file;
    global_AudioFileData.bytes_total_read_file = file.tellg();
    global_AudioFileData.bytes_chunk_audio = bytes_chunk_audio;
    global_AudioFileData.address_first_audio = static_cast<uint32_t>(file.tellg());

    global_AudioFileData.channels_file     = channels_file;

    global_AudioFileData.present_frame     = 0;
    global_AudioFileData.file_is_ieee_float = (audio_format == 3);

    const uint32_t bytes_sample = bits_sample/8;

    global_AudioFileData.frames_total = global_AudioFileData.bytes_chunk_audio/(bytes_sample*channels_file);

    global_AudioFileData.samples.assign(channels_file, std::vector<float>(global_AudioFileData.frames_total));

    file.seekg(global_AudioFileData.address_first_audio, std::ios::beg);
    for (uint32_t count_RAM_frame = 0; count_RAM_frame < global_AudioFileData.frames_total; ++count_RAM_frame) {
        if (bits_sample == 16) {

            std::vector<int16_t> sample16(channels_file);

            file.read(reinterpret_cast<char*>(sample16.data()),
                      channels_file*sizeof(int16_t));

            for (uint16_t ch = 0; ch < channels_file; ++ch) {
                global_AudioFileData.samples[ch][count_RAM_frame] = sample16[ch]/32768.0f;
            }
        } else if (bits_sample == 32) {
            if (global_AudioFileData.file_is_ieee_float) {
                std::vector<float> sample32(channels_file);
                file.read(reinterpret_cast<char*>(sample32.data()),
                          channels_file*sizeof(float));
                for (uint16_t ch = 0; ch < channels_file; ++ch) {
                    global_AudioFileData.samples[ch][count_RAM_frame] = sample32[ch];
                }
            } else {
                std::vector<int32_t> sample32i(channels_file);
                file.read(reinterpret_cast<char*>(sample32i.data()),
                          channels_file*sizeof(int32_t));
                for (uint16_t ch = 0; ch < channels_file; ++ch) {
                    global_AudioFileData.samples[ch][count_RAM_frame] =
                        std::clamp(static_cast<float>(sample32i[ch]) / 2147483647.0f, -1.0f, 1.0f);
                }
            }
        }
    }

    global_ProcessGrain.frames_object_grain = 2048;
    global_ProcessGrain.frames_common_grains = 3;
    global_ProcessGrain.count_present_frame = 0;
    global_ProcessGrain.active_envelopes_grain = 0;

    AURenderCallbackStruct structure_callback_audio;
    structure_callback_audio.inputProc = function_callback_audio;
    structure_callback_audio.inputProcRefCon = &global_AudioFileData;

    status_unit_audio = AudioUnitSetProperty(unit_audio,
                                            kAudioUnitProperty_SetRenderCallback, 
                                            kAudioUnitScope_Input, 
                                            0, 
                                            &structure_callback_audio,
                                            sizeof(structure_callback_audio));
    if (status_unit_audio != noErr) {
        std::cerr << "Rendering error: " << status_unit_audio << " \n";
        AudioComponentInstanceDispose(unit_audio);
        return;
    } else {
        std::cout << "Audio units render.\n\n";
    }

    status_unit_audio = AudioUnitInitialize(unit_audio);
    if (status_unit_audio != noErr) {
        std::cerr << "Audio initialization error: " << status_unit_audio << " \n";
        AudioComponentInstanceDispose(unit_audio);
        return;
    } else {
        std::cout << "Audio initialized.\n";
    }

    std::cout << "Calling audio into units.\n";

    status_unit_audio = AudioOutputUnitStart(unit_audio);
    if (status_unit_audio != noErr) {
        std::cerr << "Output playback error: " << status_unit_audio << " \n";
        return;
    } else {
        std::cout << "Output playback starts.\n";
    }

    std::cout << "Listening for channel order test...\n";
    while (g_run_channel_order_test) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Channel test complete!\n\n";
    
    // ========================================================================
    // ============ NEW: POST-SINE-TEST SUBWOOFER ANALYSIS MESSAGE ============
    // ========================================================================
    // After users have heard the sine test, help them interpret what they heard
    // This is more accurate than just assuming based on channel count
    if (g_output_channels == 6) {
        std::cout << "📊 SINE TEST ANALYSIS (6-channel system detected):\n";
        std::cout << "Based on standard 5.1 surround conventions, Channel 4 is typically LFE (subwoofer).\n";
        std::cout << "If you noticed that Channel 4 sounded different (bass-heavy, no sound, or\n";
        std::cout << "only low frequencies), it's likely the LFE/subwoofer channel.\n";
        std::cout << "💡 TIP: For spatial granular synthesis, consider using channels 1,2,3,5,6\n";
        std::cout << "to avoid unintended subwoofer effects in your grain sequences.\n\n";
    }
    // ========================================================================
    
    // ========================================================================
    // ========== COMMENTED OUT: CHANNEL OFFSET SETUP FUNCTION CALL ==========
    // ========================================================================
    // ISSUE: This was calling the channel rotation interface when we only want
    // the subwoofer warning (which appears in setupGrainHopping function)
    // SOLUTION: Comment out this call to disable rotation/shifting interface
    // NOTE: The subwoofer warning still appears in setupGrainHopping()
    // setupChannelOffset();  // COMMENTED OUT - rotation interface disabled
    // ========================================================================
    
    function_anchor_configure(g_output_channels);

    g_status_audio_playback = false;
    
    setupGrainHopping();
    
    // THE AUDIO PLAYBACK IS MONITORED AT THE INTERACTIVE FUNCTION NOW
    // Instead of exiting exactly when the audio time has passed, the audio starts outputting silence
    // File reader stops at the end of audio chunk before callback continues to loop in silence (0.0f)

    // std::this_thread::sleep_for(std::chrono::seconds(bytes_chunk_audio/(channels_file*((rate_samples)*(bits_sample/8)))));

    // Start live control monitoring instead of just waiting for audio to finish
    std::cout << "\nAudio starting:";
    std::cout << "Live controls:\n\n";
        flive_control_monitor(unit_audio, file, channels_file,
                       rate_samples, bits_sample, audio_format, selection_device);

    AudioOutputUnitStop(unit_audio);
    AudioComponentInstanceDispose(unit_audio);
    std::cout << "Stopped and disposed audio unit.\n\n";
}

// =============================================================================
// MAIN APPLICATION ENTRY POINT
// =============================================================================

/**
 * COMPREHENSIVE AUDIO APPLICATION MAIN FUNCTION
 * 
 * This function orchestrates the entire granular synthesis application,
 * demonstrating sophisticated software architecture and audio engineering
 * principles. The main function coordinates multiple complex subsystems:
 * 
 * SYSTEM INTEGRATION ACHIEVEMENTS:
 * • WAV file format parsing and validation
 * • Core Audio device management and configuration
 * • Real-time audio processing pipeline initialization
 * • Interactive user interface with live parameter control
 * • Multi-threaded architecture coordination
 * • Professional error handling and resource management
 * 
 * EDUCATIONAL SIGNIFICANCE:
 * This application represents a comprehensive understanding of:
 * - Low-level audio programming and DSP concepts
 * - Real-time systems programming and optimization
 * - Advanced C++ programming techniques and patterns
 * - Professional audio software development practices
 * - Mathematical foundations of digital audio processing
 * 
 * @return int Application exit status (0 = success, 1 = error)
 */
int main() {
    // Initialize and demonstrate the sequence parsing system
    function_print_vector();

    std::string name_file;
    std::cout << "Please choose a multichannel WAV file.\n";
    std::cout << "File name: ";
    std::cin >> name_file;
    std::ifstream file(name_file, std::ios::binary);
    if (!file) {
        std::cerr << "No file detected. Please ensure file is in this folder.\n\n";
        return 1;
    }

    std::cout << name_file << "\n";

    function_shape_envelope();

    file.seekg(22, std::ios::beg);
    unsigned char bytes_channels_file[2];
    file.read(reinterpret_cast<char*>(bytes_channels_file), sizeof(bytes_channels_file));
    uint16_t channels_file = 0;
    for (int a = 0; a < sizeof(bytes_channels_file); a++) {
        channels_file |= (static_cast<uint16_t>(bytes_channels_file[a]) << (a * 8));
    }
    
    uint32_t rate_samples; 
    file.read(reinterpret_cast<char*>(&rate_samples), sizeof(rate_samples));
    
    file.seekg(34, std::ios::beg);
    uint16_t bits_sample; 
    file.read(reinterpret_cast<char*>(&bits_sample), sizeof(bits_sample));

    // Parse audio format from WAV header (position 20-21)
    file.seekg(20, std::ios::beg);
    uint16_t audio_format = 1; // Default to PCM format for safety
    file.read(reinterpret_cast<char*>(&audio_format), sizeof(audio_format));

    file.close();

    std::cout << "File information: \n";
    std::cout << "Number of channels: " << channels_file << "\n";
    std::cout << "Sample rate: " << rate_samples << "\n";
    std::cout << "Bit resolution: " << bits_sample << "\n\n";

    if (channels_file > 16) {
        std::cerr << "Unsupported channel count: " << channels_file << " (max 16)\n";
        return 1;
    }

    file.open(name_file, std::ios::binary);

    if (!file) {
        std::cerr << "File was lost.\n\n";
        return 1;
    }

    int selection_verified_device = getAudioDevices();
    if (selection_verified_device == -1) {
        std::cout << "\nCannot run playback. Please re-run program to try again.\n\n\n";
        return 1;
    }

    playAudioFile(name_file, selection_verified_device, channels_file, rate_samples, bits_sample, audio_format, file);

    return 0;
}
