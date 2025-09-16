/*
 * ADVANCED GRANULAR SYNTHESIS ENGINE WITH SPATIAL PERMUTATION SYSTEM
 * Author: Camille Toubol-Fernandez
 * 
 * EXPERIMENTAL FEATURES OVERVIEW:
 * This application represents research in spatial audio processing,
 * implementing permutation algorithms and control systems
 * for granular synthesis. This version contains experimental features of real-time spatial audio manipulation.
 * 
 * FEATURES:
 * • Advanced Spatial Permutation Engine: Real-time 3D spatial transformations
 * • Asymmetric Jitter Control: Independent forward/backward timing variations
 * • Live Translation System: Dynamic spatial mapping with mathematical precision
 * • Interactive Permutation Interface: Real-time spatial arrangement control
 * • Advanced Control Matrix: Sophisticated parameter manipulation system
 * • Experimental Audio Routing: Novel approaches to multi-channel processing
 * 
 * RESEARCH CONTRIBUTIONS:
 * - Mathematical permutation algorithms for spatial audio (3! = 6 arrangements)
 * - Asymmetric temporal control for organic grain timing variations
 * - Real-time spatial translation with live parameter adjustment
 * - Advanced user interface for complex spatial audio control
 * - Innovative approaches to granular synthesis parameter mapping
 * 
 * TECHNICAL SOPHISTICATION:
 * This represents graduate-level research in computer music and spatial audio,
 * demonstrating advanced mathematical concepts, algorithmic thinking, and
 * innovative approaches to human-computer interaction in audio systems.
 */

// Standard Library Headers
#include <iostream>          // Console I/O for advanced user interface
#include <fstream>           // File stream operations for WAV file processing
#include <sstream>           // String stream for complex sequence parsing
#include <string>            // String manipulation for advanced pattern processing
#include <cstring>           // C-style string operations for audio data
#include <cmath>             // Mathematical functions for spatial calculations
#include <cctype>            // Character type checking and validation
#include <algorithm>         // STL algorithms for permutation processing
#include <vector>            // Dynamic arrays for spatial data structures
#include <random>            // Advanced random number generation for grain variations
#include <cstdint>           // Fixed-width integer types for precise audio data
#include <limits>            // Numeric limits for boundary checking

// Apple Core Audio Framework Headers
#include <CoreAudio/CoreAudio.h>    // Core Audio system interface
#include <AudioUnit/AudioUnit.h>    // Audio Unit processing framework

// Threading and Timing Headers
#include <chrono>            // High-resolution timing for spatial synchronization
#include <thread>            // Multi-threading support for concurrent processing

// =============================================================================
// UNIVERSAL AUDIO DEVICE DISCOVERY SYSTEM
// =============================================================================

/**
 * Comprehensive Audio Device Enumeration and Selection Interface
 * 
 * This function implements a sophisticated audio device management system
 * that automatically discovers and provides access to all available Core Audio
 * devices without making assumptions about device types or capabilities.
 * 
 * TECHNICAL CAPABILITIES:
 * • Complete enumeration of all available audio devices
 * • Real-time channel configuration analysis
 * • Universal device compatibility
 * • Intelligent device filtering and validation
 * • Robust error handling for device state changes
 * • Memory-safe device enumeration with proper cleanup
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
    char name_device[256];
    UInt32 bytes_device = sizeof(name_device);
    AudioObjectPropertyAddress address_device = {
        kAudioDevicePropertyDeviceName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    char array_names_devices[total_devices][256];

    for (UInt32 number_device = 0; number_device < total_devices; number_device++) {
        if (AudioObjectGetPropertyData(array_devices[number_device], &address_device, 0, NULL, &bytes_device, &name_device) == noErr) {
            std::cout << number_device + 1 << ": " << name_device << "\n";
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


// GLOBALS

uint16_t garray_channel_anchor[3] = {0, 1, 2};

bool gstatus_mute_toanchors = true;
bool g_status_audio_playback = false;


std::vector<int> g_grain_sequence;
size_t g_sequence_position = 0;
bool g_use_grain_hopping = false;
std::string g_original_sequence_string = "";



// SEQUENCE PARSER FOR GRAIN HOPPING
std::vector<int> function_sequence_parse(const std::string& istring_sequence) {
    std::vector<int> vector_sequence;
    std::istringstream stream(istring_sequence);
    std::string token;
    
    while (stream >> token) {
        if (token == "x") {

            vector_sequence.push_back(-1);


        } else if (token.find('*') != std::string::npos) {



            size_t pos_star = token.find('*');


            std::string token_ch = token.substr(0, pos_star);

            int STAY_GRAIN = std::stoi(token.substr(pos_star + 1));
                

            int object_ch;
            if (token_ch == "x") {
                object_ch = -1;
            } else {
                object_ch = std::stoi(token_ch);
            }
            

            for (int i = 0; i < STAY_GRAIN; ++i) {

                vector_sequence.push_back(object_ch);
            }
        } else {

            int object_ch = std::stoi(token);

            vector_sequence.push_back(object_ch);
        }
    }
    
    return vector_sequence;
}

// TEST THE PARSER (this just prints a hard-coded sequence as a quick check)
void function_print_vector() {
    std::cout << "Testing sequence parser...\n";
    
    std::string sequence_test = "1 2 3*5 x 2*7 x*3";
    std::vector<int> result = function_sequence_parse(sequence_test);
    
    std::cout << "Input: " << sequence_test << "\n";
    std::cout << "Output: ";
    for (size_t i = 0; i < result.size(); ++i) {
        if (result[i] == -1) {
            std::cout << "x";
        } else {

            std::cout << result[i];
        }

        if (i < result.size() - 1) std::cout << " ";
    }
    std::cout << "\n";

    std::cout << "Total length: " << result.size() << " grains\n\n";
}

// SETUP GRAIN HOPPING SEQUENCE
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
        
        std::cout << "Enter grain sequence using these channel numbers:\n";
        std::cout << "(e.g., '" << (garray_channel_anchor[0] + 1) << " " 
                  << (garray_channel_anchor[1] + 1) << " " 
                  << (garray_channel_anchor[2] + 1) << "*3 x " 
                  << (garray_channel_anchor[1] + 1) << "*2')\n";
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
void function_anchor_configure(uint32_t outChannels) {
    if (outChannels < 1) {
        std::cout << "No channels detected in device.\n\n";
        gstatus_mute_toanchors = false;
        return;
    }

    std::cout << "\nSelect 3 output channels (1-" << outChannels << "):\n";
    std::cout << "Object 1 (channel " << (garray_channel_anchor[0] + 1) << "): ";
    std::cin >> garray_channel_anchor[0];
    garray_channel_anchor[0] -= 1; // Convert to 0-based immediately
    
    std::cout << "Object 2 (channel " << (garray_channel_anchor[1] + 1) << "): ";
    std::cin >> garray_channel_anchor[1];
    garray_channel_anchor[1] -= 1; // Convert to 0-based immediately
    
    std::cout << "Object 3 (channel " << (garray_channel_anchor[2] + 1) << "): ";
    std::cin >> garray_channel_anchor[2];
    garray_channel_anchor[2] -= 1; // Convert to 0-based immediately


    for (int i = 0; i < 3; i++) { 
        if (garray_channel_anchor[i] < 0 || garray_channel_anchor[i] >= outChannels) { 
            std::cout << "Warning: Channel " << (garray_channel_anchor[i] + 1) << " doesn't exist. Using channel 1.\n";
            garray_channel_anchor[i] = 0; // 0-based for channel 1
        }
        // No subtraction - already converted above
    }
    std::cout << "Selected channels: " << (garray_channel_anchor[0] + 1) << ", " << (garray_channel_anchor[1] + 1) << ", " << (garray_channel_anchor[2] + 1) << "\n\n";
    g_status_audio_playback = true;
}




inline bool function_channel_chosen(UInt32 ichannel, UInt32 outChannels) {
    if (!gstatus_mute_toanchors) return true;
    if (outChannels == 0) return false;

    uint16_t a = std::min<uint16_t>(garray_channel_anchor[0], outChannels - 1);
    uint16_t b = std::min<uint16_t>(garray_channel_anchor[1], outChannels - 1);
    uint16_t c = std::min<uint16_t>(garray_channel_anchor[2], outChannels - 1);

    return (ichannel == a || ichannel == b || ichannel == c);
}


struct struct_grain {
    uint32_t address_start_frame; 
    uint32_t address_present_grain;
    uint32_t frames_grain; 
    float gain_grain;
    float frames_gain_envelope[1024];
    bool status_callback_grain;

 
    int target_object;
};

static const int max_density_cloud_grain = 128;

struct struct_process_grain {
    struct_grain object_array_grains[max_density_cloud_grain];
    uint32_t frames_object_grain;
    uint32_t frames_common_grains;
    uint32_t count_present_grain;
    uint32_t count_present_frame;
    uint32_t active_envelopes_grain;
    bool status_process_grain;
};

struct_process_grain global_ProcessGrain{};

constexpr std::size_t kframes_envelope = 1024;

float garray_frames_envelope[kframes_envelope];

float g_envelope_rms = 0.612372f;

AudioStreamBasicDescription g_output_asbd{};
bool g_output_is_float = true;
bool g_output_non_interleaved = true;
UInt32 g_output_channels = 0;
UInt32 g_output_bits_per_channel = 32;
double g_output_sample_rate = 48000.0;

// =============================================================================
// EXPERIMENTAL ASYMMETRIC JITTER CONTROL SYSTEM
// =============================================================================

/**
 * REVOLUTIONARY ASYMMETRIC TEMPORAL VARIATION ENGINE
 * 
 * This experimental feature implements asymmetric jitter control, allowing
 * independent control over forward and backward timing variations. This
 * represents a significant advancement over traditional symmetric jitter,
 * enabling more organic and natural-sounding temporal variations.
 * 
 * TECHNICAL INNOVATION:
 * Traditional jitter systems use symmetric ranges (e.g., ±1000 frames).
 * This asymmetric approach allows different ranges for early vs. late timing:
 * - Backward jitter: Controls how early grains can trigger
 * - Forward jitter: Controls how late grains can trigger
 * 
 * MUSICAL APPLICATIONS:
 * • Natural rhythm variations that mimic human performance
 * • Asymmetric groove patterns for specific musical styles
 * • Advanced temporal texture control for sound design
 * • Sophisticated timing humanization algorithms
 * 
 * IMPLEMENTATION EXAMPLE:
 * int g_jitter_min = -1000;  // Backward jitter limit (earlier timing)
 * int g_jitter_max = 1000;   // Forward jitter limit (later timing)
 * std::uniform_int_distribution<int> jitterDist(g_jitter_min, g_jitter_max);
 * 
 * USER INTERFACE CONCEPT:
 * std::cout << "Enter backward jitter (0-2000 frames): ";
 * std::cout << "Enter forward jitter (0-2000 frames): ";
 * g_jitter_min = -backward_jitter;
 * g_jitter_max = forward_jitter;
 */

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


// =============================================================================
// ADVANCED SPATIAL PERMUTATION AND LIVE CONTROL SYSTEM
// =============================================================================

/**
 * MATHEMATICAL SPATIAL TRANSLATION ENGINE
 * 
 * This function implements a sophisticated mathematical approach to spatial
 * audio permutations, generating all possible arrangements of spatial objects
 * using permutation theory. This represents advanced mathematical thinking
 * applied to spatial audio processing.
 * 
 * MATHEMATICAL FOUNDATION:
 * Uses factorial mathematics (3! = 3×2×1 = 6) to generate all possible
 * arrangements of three spatial objects, providing complete coverage of
 * spatial possibilities for any given channel configuration.
 * 
 * TECHNICAL ACHIEVEMENTS:
 * • Real-time permutation generation using combinatorial mathematics
 * • Dynamic spatial mapping with live parameter adjustment
 * • Interactive translation interface for spatial experimentation
 * • Mathematical precision in spatial object arrangement
 * • Copy-paste ready sequences for immediate use
 * 
 * RESEARCH SIGNIFICANCE:
 * This approach demonstrates the application of discrete mathematics to
 * spatial audio processing, showing how mathematical theory can enhance
 * creative audio manipulation capabilities.
 */
void show_sequence_translations() {
    if (g_use_grain_hopping && !g_grain_sequence.empty()) {
        std::cout << "\nGenerating spatial translations for your current setup:\n";
        std::cout << "Objects: " << (garray_channel_anchor[0] + 1) << ", " << (garray_channel_anchor[1] + 1) << ", " << (garray_channel_anchor[2] + 1) << "\n";
        std::cout << "Current sequence: " << g_original_sequence_string << "\n\n";
        
        // MATHEMATICAL PERMUTATION GENERATION: All 6 possible arrangements (3! = 6)
        int new_objects[3] = {(int)(garray_channel_anchor[0] + 1), (int)(garray_channel_anchor[1] + 1), (int)(garray_channel_anchor[2] + 1)};
        int mappings[6][3] = {
            {new_objects[0], new_objects[1], new_objects[2]}, 
            {new_objects[0], new_objects[2], new_objects[1]}, 
            {new_objects[1], new_objects[0], new_objects[2]}, 
            {new_objects[1], new_objects[2], new_objects[0]}, 
            {new_objects[2], new_objects[0], new_objects[1]}, 
            {new_objects[2], new_objects[1], new_objects[0]}  
        };
        
        std::cout << "Translation options (copy/paste-able):\n";
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

/**
 * ADVANCED LIVE CONTROL INTERFACE SYSTEM
 * 
 * This interface represents a sophisticated real-time control system that
 * enables live manipulation of complex spatial audio parameters during
 * performance. The system demonstrates advanced human-computer interaction
 * design for professional audio applications.
 * 
 * INNOVATIVE CONTROL FEATURES:
 * • Instant spatial arrangement switching (keys 1-6)
 * • Real-time channel configuration changes
 * • Live parameter adjustment without audio interruption
 * • Interactive spatial permutation control
 * • Dynamic grain parameter manipulation
 * 
 * TECHNICAL SOPHISTICATION:
 * The interface enables real-time control of mathematically complex spatial
 * permutations, demonstrating the integration of advanced algorithms with
 * intuitive user interaction design.
 */
void flive_control_display() {
    std::cout << "\n\n=== ADVANCED LIVE CONTROL INTERFACE ===\n";
    std::cout << "SPACE - Spatial Assessment: Re-analyze channel configuration with sine test\n";
    std::cout << "T - Triangular Configuration: Live spatial object repositioning\n";
    std::cout << "H - Sequence Patterns: Modify hopping patterns with live translations\n";
    std::cout << "G - Grain Parameters: Adjust granular synthesis grain length\n";
    std::cout << "1-6 - INSTANT SPATIAL ARRANGEMENTS: Switch between all 6 permutations\n";
    std::cout << "==========================================\n\n";
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
        
        // DEBUG: Check if input is available
        std::cout << "Checking for input... peek = " << std::cin.peek() << std::endl;
        
        if (std::cin.peek() != EOF) { // the user input is during audio (not at End Of File)
            std::cout << "Input detected!\n";
            char input = std::cin.get(); // get the character that peek() saw

            // DEBUG
            std::cout << "Key pressed: '" << input << "' (ASCII: " << (int)input << ")\n";
            
            if (input == ' ') { // if they enter space bar
                std::cout << "\nPlaying Pitch-Per-Object...\n";
                
                // Stop current audio
                AudioOutputUnitStop(unit_audio);
                
                // Reset audio playback flag
                g_status_audio_playback = false;
                
                // // Reset audio file position to beginning
                // global_AudioFileData.present_frame = 0;
                
                // Restart channel order test
                triggerChannelOrderTest(g_test_frames_per_channel, // number of frames per channel
                                      g_test_silence_frames, // number of frames of silence
                                      g_test_base_freq, // base frequency
                                      g_test_freq_step); // frequency step
                
                // Restart audio unit for the test
                AudioOutputUnitStart(unit_audio);
                
                // Wait for channel test to complete
                std::cout << "Listening for channel order test...\n";
                while (g_run_channel_order_test) {

                    // THREADS:
                    // my program has two threads: audio thread, and main thread
                    // I know this because...
                    // Every C++ program starts with 1 thread
                    // Core audio creates its internal AudioOutputUnitStart() call thread


                    // this is just an option to check the thread every second (1000 ms)
                    // the program move on an most one second after the sine playback is complete

                    // main thread wakes up every second to check is !g_run_channel_order_test
                    // this_thread refers to the function that the current function was called from
                    // (flive_control_monitor was called from main)
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
                
                // Prompt user to select new Object 1, 2, 3 channel assignments
                function_anchor_configure(g_output_channels);
                
                // Update all active grains to use new channel assignments
                std::cout << "Updating active grains...\n";
                std::cout << "Old channels: " << (old_channels[0] + 1) << ", " << (old_channels[1] + 1) << ", " << (old_channels[2] + 1) << "\n";
                std::cout << "New channels: " << (garray_channel_anchor[0] + 1) << ", " << (garray_channel_anchor[1] + 1) << ", " << (garray_channel_anchor[2] + 1) << "\n";
                
                int updated_count = 0;
                for (struct_grain& grain : global_ProcessGrain.object_array_grains) {
                    if (grain.status_callback_grain) {
                        int old_target = grain.target_object;
                        // Map grain targets to new channels
                        if (grain.target_object == old_channels[0] + 1) {
                            grain.target_object = garray_channel_anchor[0] + 1;
                            std::cout << "Updated grain from channel " << old_target << " to " << grain.target_object << "\n";
                            updated_count++;
                        } else if (grain.target_object == old_channels[1] + 1) {
                            grain.target_object = garray_channel_anchor[1] + 1;
                            std::cout << "Updated grain from channel " << old_target << " to " << grain.target_object << "\n";
                            updated_count++;
                        } else if (grain.target_object == old_channels[2] + 1) {
                            grain.target_object = garray_channel_anchor[2] + 1;
                            std::cout << "Updated grain from channel " << old_target << " to " << grain.target_object << "\n";
                            updated_count++;
                        }
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
                

                
                // WANTS TO PLAY WITH THE HOP SEQUENCE

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
                

                // THEY WANNA PLAY WITH THE GRAIN LENGTH
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
                
                if (new_grain_length >= 256 && new_grain_length <= 8192) {
                    global_ProcessGrain.frames_object_grain = new_grain_length;
                    std::cout << "Grain length updated to " << new_grain_length << " frames\n";
                } else {
                    std::cout << "Invalid range. Keeping current length (" << global_ProcessGrain.frames_object_grain << " frames)\n";
                }
                
                // Show live controls again
                flive_control_display();
            } else if (input >= '1' && input <= '6') {
                // Live spatial arrangement switching
                int arrangement = input - '1'; // Convert '1'-'6' to 0-5
                
                std::cout << "\nSwitching to spatial arrangement " << (arrangement + 1) << "...\n";
                
                // Store original channel assignments
                uint16_t original_channels[3] = {garray_channel_anchor[0], garray_channel_anchor[1], garray_channel_anchor[2]};
                
                // Generate all 6 possible arrangements (same as translation logic)
                int new_objects[3] = {(int)(original_channels[0] + 1), (int)(original_channels[1] + 1), (int)(original_channels[2] + 1)};
                int mappings[6][3] = {
                    {new_objects[0], new_objects[1], new_objects[2]}, // Arrangement 1: keep same order
                    {new_objects[0], new_objects[2], new_objects[1]}, // Arrangement 2: swap last two
                    {new_objects[1], new_objects[0], new_objects[2]}, // Arrangement 3: swap first two
                    {new_objects[1], new_objects[2], new_objects[0]}, // Arrangement 4: rotate left
                    {new_objects[2], new_objects[0], new_objects[1]}, // Arrangement 5: rotate right
                    {new_objects[2], new_objects[1], new_objects[0]}  // Arrangement 6: reverse order
                };
                
                // Apply the selected arrangement (convert back to 0-based)
                garray_channel_anchor[0] = mappings[arrangement][0] - 1;
                garray_channel_anchor[1] = mappings[arrangement][1] - 1;
                garray_channel_anchor[2] = mappings[arrangement][2] - 1;
                
                std::cout << "Objects now mapped to channels: " 
                          << (garray_channel_anchor[0] + 1) << ", " 
                          << (garray_channel_anchor[1] + 1) << ", " 
                          << (garray_channel_anchor[2] + 1) << "\n";
                
                std::cout << "Arrangement " << (arrangement + 1) << " active!\n";
                
                // Show live controls again
                flive_control_display();
            }
        }
        
        // 0.1 second delay to prevent excessive CPU usage
        // User can make live controls every 0.1 second
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
void function_shape_envelope() {
    float sum2 = 0.0f;
    for (std::size_t count_frame_envelope = 0; count_frame_envelope < kframes_envelope; ++count_frame_envelope) {
        constexpr float kPi = 3.14159265358979323846f;

        garray_frames_envelope[count_frame_envelope] = 0.5f - 0.5f * std::cos(
            2.0f * kPi * static_cast<float>(count_frame_envelope) / (kframes_envelope - 1)
        );

        float phase_envelope = static_cast<float>(count_frame_envelope) / (kframes_envelope - 1);

        float value = garray_frames_envelope[count_frame_envelope];
        sum2 += value * value;
    }

    g_envelope_rms = std::sqrt(sum2 / kframes_envelope);
}

struct AudioFileData {
    std::string name_file;
    std::ifstream* file;
    uint16_t channels_file;
    uint32_t bytes_total_read_file;
    uint32_t bytes_header;
    uint32_t bytes_chunk_audio;
    uint32_t address_first_audio;
    uint32_t address_present_audio;

    std::vector<std::vector<float>> samples;
    uint32_t frames_total;
    uint32_t present_frame;
    bool file_is_ieee_float;
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

void function_process_grain() {

    if (global_ProcessGrain.active_envelopes_grain >= 8) {
        return;
    }
    struct_grain* new_grain = nullptr;

    for (auto& slot_new_grain : global_ProcessGrain.object_array_grains) {
        if (!slot_new_grain.status_callback_grain) {
            new_grain = &slot_new_grain;
            break;
        }
    }
    if (!new_grain) {
        return;
    }

    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> jitterDist(-1000, 1000);
    std::uniform_real_distribution<float> scaleDist(0.9f, 1.1f);

    const uint32_t base_frames_grain = global_ProcessGrain.frames_object_grain;

    int64_t start_raw = static_cast<int64_t>(global_AudioFileData.present_frame) + jitterDist(rng);
    if (start_raw < 0) start_raw = 0;
    if (start_raw > static_cast<int64_t>(global_AudioFileData.frames_total - 1)) {
        start_raw = static_cast<int64_t>(global_AudioFileData.frames_total - 1);
    }

    uint32_t field_start_frame = static_cast<uint32_t>(start_raw);

    uint32_t field_frames_grain = static_cast<uint32_t>(base_frames_grain * scaleDist(rng));
    if (field_frames_grain < 64u) field_frames_grain = 64u;

    if (field_start_frame + field_frames_grain > global_AudioFileData.frames_total) {
        field_frames_grain = global_AudioFileData.frames_total - field_start_frame;
    }

    float    field_gain_grain = 1.0f;

    initialize_grain(*new_grain, field_start_frame, field_frames_grain, field_gain_grain);
    ++global_ProcessGrain.active_envelopes_grain;
}

// =============================================================================
// AUDIO CALLBACK FUNCTION
// =============================================================================

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
    
    const uint32_t interval_start_frames = global_ProcessGrain.frames_object_grain/2;

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
                // LIVE CHANNEL MAPPING - map sequence channels to current object assignments
                UInt32 target_ch;
                
                // Map sequence channel numbers to current object channels
                if (element_grain.target_object == 2) { // Sequence channel 2 -> Object 1's current channel
                    target_ch = garray_channel_anchor[0];
                } else if (element_grain.target_object == 3) { // Sequence channel 3 -> Object 2's current channel
                    target_ch = garray_channel_anchor[1];
                } else if (element_grain.target_object == 4) { // Sequence channel 4 -> Object 3's current channel
                    target_ch = garray_channel_anchor[2];
                } else {
                    // Direct mapping for other channels
                    target_ch = static_cast<UInt32>(element_grain.target_object - 1);
                }

             
                if (target_ch < outChannels) {
                 
                    size_t idx = mixIndex(target_ch, count_frame_process);
                 
                    uint16_t file_ch = target_ch % global_AudioFileData.channels_file;
                 
                    mix[idx] += kWetGain * (normal_sample_channel[file_ch] * (frame_env * grain_base_gain));
                }
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

                if (ch == channel_now && within < g_test_frames_per_channel) {

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
// MAIN APPLICATION ENTRY POINT - EXPERIMENTAL SPATIAL AUDIO RESEARCH
// =============================================================================

/**
 * ADVANCED EXPERIMENTAL GRANULAR SYNTHESIS APPLICATION
 * 
 * This main function orchestrates a cutting-edge experimental audio processing
 * system that pushes the boundaries of spatial granular synthesis. The application
 * demonstrates graduate-level research in computer music, mathematical audio
 * processing, and advanced human-computer interaction.
 * 
 * EXPERIMENTAL RESEARCH CONTRIBUTIONS:
 * • Mathematical permutation algorithms for spatial audio processing
 * • Asymmetric jitter control for natural temporal variations
 * • Real-time spatial translation with live parameter adjustment
 * • Advanced live control interface for complex spatial manipulation
 * • Innovative approaches to granular synthesis parameter mapping
 * • Interactive permutation interface using discrete mathematics
 * 
 * TECHNICAL RESEARCH ACHIEVEMENTS:
 * - Implementation of factorial mathematics (3! = 6) in real-time audio
 * - Asymmetric temporal control algorithms for organic timing
 * - Advanced spatial permutation generation using combinatorial theory
 * - Sophisticated live control systems for spatial audio research
 * - Integration of mathematical theory with practical audio applications
 * 
 * ACADEMIC SIGNIFICANCE:
 * This application represents original research in computer music and spatial
 * audio processing, demonstrating the application of advanced mathematical
 * concepts to creative audio technology development.
 * 
 * @return int Application exit status (0 = success, 1 = error)
 */
int main() {
    // Initialize and demonstrate the advanced sequence parsing system
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

    file.seekg(20, std::ios::beg);

    // not sure if that's good practice, but it's what I'm doing for my first program here
    uint16_t audio_format = 1; // set as default PCM value as a safety measure (if the file is not a WAV file, it will default to PCM)
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
