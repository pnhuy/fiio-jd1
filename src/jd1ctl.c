#include "cli.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define JD1_VID 0x31B2
#define JD1_PID 0x2581
#define REPORT_ID 0x4B
#define REPORT_LEN 11

struct response_state {
    bool ready;
    uint8_t bytes[64];
    CFIndex length;
    CFIndex report_id;
};

static void input_report(void *context, IOReturn result, void *sender,
                         IOHIDReportType type, uint32_t report_id,
                         uint8_t *report, CFIndex report_length) {
    (void)sender;
    (void)type;
    struct response_state *state = context;
    if (result != kIOReturnSuccess || report_id != REPORT_ID
        || report_length <= 0) return;
    if (report_length > (CFIndex)sizeof(state->bytes))
        report_length = sizeof(state->bytes);
    memcpy(state->bytes, report, (size_t)report_length);
    state->length = report_length;
    state->report_id = report_id;
    state->ready = true;
}

static CFMutableDictionaryRef matching_dictionary(void) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    int vendor = JD1_VID;
    int product = JD1_PID;
    CFNumberRef vendor_num = CFNumberCreate(kCFAllocatorDefault,
                                             kCFNumberIntType, &vendor);
    CFNumberRef product_num = CFNumberCreate(kCFAllocatorDefault,
                                              kCFNumberIntType, &product);
    CFDictionarySetValue(dict, CFSTR(kIOHIDVendorIDKey), vendor_num);
    CFDictionarySetValue(dict, CFSTR(kIOHIDProductIDKey), product_num);
    CFRelease(vendor_num);
    CFRelease(product_num);
    return dict;
}

static IOHIDDeviceRef find_device(IOHIDManagerRef manager) {
    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    if (!devices || CFSetGetCount(devices) == 0) {
        if (devices) CFRelease(devices);
        return NULL;
    }
    CFIndex count = CFSetGetCount(devices);
    IOHIDDeviceRef *items = calloc((size_t)count, sizeof(*items));
    if (!items) {
        CFRelease(devices);
        return NULL;
    }
    CFSetGetValues(devices, (const void **)items);
    IOHIDDeviceRef found = NULL;
    for (CFIndex i = 0; i < count; i++) {
        CFTypeRef max_output = IOHIDDeviceGetProperty(items[i],
                                                       CFSTR(kIOHIDMaxOutputReportSizeKey));
        if (max_output) {
            found = items[i];
            CFRetain(found);
            break;
        }
    }
    free(items);
    CFRelease(devices);
    return found;
}

static void print_packet(const char *prefix, CFIndex report_id,
                         const uint8_t *bytes, CFIndex length) {
    printf("%s report-id=%02lX data=", prefix, (long)report_id);
    for (CFIndex i = 0; i < length; i++) printf("%02X%s", bytes[i], i + 1 == length ? "" : " ");
    putchar('\n');
}

static bool transact(IOHIDDeviceRef device, uint32_t address, uint8_t command,
                     uint32_t write_value, struct response_state *state) {
    uint8_t packet[REPORT_LEN] = {0};
    packet[0] = REPORT_ID;
    packet[1] = (uint8_t)(address & 0xff);
    packet[2] = (uint8_t)((address >> 8) & 0xff);
    packet[3] = (uint8_t)((address >> 16) & 0xff);
    packet[4] = (uint8_t)((address >> 24) & 0xff);
    packet[5] = command;
    if (command == 0x57) { /* 'W' */
        packet[7] = (uint8_t)(write_value & 0xff);
        packet[8] = (uint8_t)((write_value >> 8) & 0xff);
        packet[9] = (uint8_t)((write_value >> 16) & 0xff);
        packet[10] = (uint8_t)((write_value >> 24) & 0xff);
    }
    state->ready = false;
    print_packet("TX", REPORT_ID, packet, REPORT_LEN);
    IOReturn rc = IOHIDDeviceSetReport(device, kIOHIDReportTypeOutput,
                                        REPORT_ID, packet, REPORT_LEN);
    if (rc != kIOReturnSuccess) {
        fprintf(stderr, "IOHIDDeviceSetReport failed: 0x%08X\n", rc);
        return false;
    }
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.5, true);
    if (!state->ready) {
        fprintf(stderr, "Timed out waiting for report 0x4B response.\n");
        return false;
    }
    print_packet("RX", state->report_id, state->bytes, state->length);
    return true;
}

static bool response_value(const struct response_state *state, uint32_t *value) {
    if (state->length < 11 || state->bytes[0] != REPORT_ID) return false;
    *value = (uint32_t)state->bytes[7]
           | ((uint32_t)state->bytes[8] << 8)
           | ((uint32_t)state->bytes[9] << 16)
           | ((uint32_t)state->bytes[10] << 24);
    return true;
}

static bool read_register(IOHIDDeviceRef device, uint32_t address,
                          struct response_state *state, uint32_t *value) {
    uint8_t command = address <= 0x100 ? 0x52 : 0x08;
    return transact(device, address, command, 0, state)
        && response_value(state, value);
}

static bool apply_digital_gain(IOHIDDeviceRef device, double requested_db,
                               bool only_if_needed) {
    IOReturn rc = IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
    if (rc != kIOReturnSuccess) {
        fprintf(stderr, "Could not open JD1 HID interface: 0x%08X\n", rc);
        return false;
    }

    uint8_t input_buffer[64] = {0};
    struct response_state state = {0};
    IOHIDDeviceRegisterInputReportCallback(device, input_buffer,
                                            sizeof(input_buffer), input_report, &state);
    IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(),
                                    kCFRunLoopDefaultMode);

    uint32_t old_value = 0;
    bool ok = read_register(device, 0x66, &state, &old_value);
    if (ok) {
        int8_t left = (int8_t)(old_value & 0xff);
        int8_t right = (int8_t)((old_value >> 8) & 0xff);
        printf("Current digital DAC gain: L=%+.1f dB R=%+.1f dB (raw 0x%08X)\n",
               left / 2.0, right / 2.0, old_value);
    }

    if (ok) {
        int8_t encoded = (int8_t)(requested_db * 2.0);
        uint32_t new_value = (old_value & 0xffff0000U)
                           | (uint8_t)encoded
                           | ((uint32_t)(uint8_t)encoded << 8);
        if (only_if_needed && new_value == old_value) {
            printf("JD1 gain is already %+.1f dB; no write needed.\n", requested_db);
        } else {
            printf("Backup/restore value for register 0x66: 0x%08X\n", old_value);
            ok = transact(device, 0x66, 0x57, new_value, &state);
            uint32_t ack = 0;
            ok = ok && response_value(&state, &ack) && ack == 0x03;
            if (!ok) {
                fprintf(stderr, "Gain write was not acknowledged; stopping.\n");
            } else {
                uint32_t verify = 0;
                ok = read_register(device, 0x66, &state, &verify) && verify == new_value;
                if (ok)
                    printf("Verified digital DAC gain at %+.1f dB on both channels.\n",
                           requested_db);
                else
                    fprintf(stderr, "Read-back did not match the requested gain.\n");
            }
        }
    }

    IOHIDDeviceUnscheduleFromRunLoop(device, CFRunLoopGetCurrent(),
                                      kCFRunLoopDefaultMode);
    IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
    fflush(stdout);
    fflush(stderr);
    return ok;
}

struct watch_context {
    double requested_db;
};

struct delayed_apply {
    IOHIDDeviceRef device;
    double requested_db;
};

static bool apply_gain_on_attach(IOHIDDeviceRef device, double requested_db) {
    IOReturn rc = IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
    if (rc != kIOReturnSuccess) {
        fprintf(stderr, "Watcher could not open JD1 HID interface: 0x%08X\n", rc);
        return false;
    }
    int8_t encoded = (int8_t)(requested_db * 2.0);
    uint32_t value = (uint8_t)encoded | ((uint32_t)(uint8_t)encoded << 8);
    uint8_t packet[REPORT_LEN] = {0};
    packet[0] = REPORT_ID;
    packet[1] = 0x66;
    packet[5] = 0x57; /* 'W' */
    packet[7] = (uint8_t)(value & 0xff);
    packet[8] = (uint8_t)((value >> 8) & 0xff);
    rc = IOHIDDeviceSetReport(device, kIOHIDReportTypeOutput,
                               REPORT_ID, packet, REPORT_LEN);
    IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
    if (rc != kIOReturnSuccess) {
        fprintf(stderr, "Watcher gain write failed: 0x%08X\n", rc);
        return false;
    }
    printf("Applied JD1 digital DAC gain %+.1f dB on attachment.\n", requested_db);
    fflush(stdout);
    return true;
}

static void apply_timer_fired(CFRunLoopTimerRef timer, void *info) {
    (void)timer;
    struct delayed_apply *apply = info;
    apply_gain_on_attach(apply->device, apply->requested_db);
    CFRelease(apply->device);
    free(apply);
}

static void device_added(void *context, IOReturn result, void *sender,
                         IOHIDDeviceRef device) {
    (void)sender;
    if (result != kIOReturnSuccess) return;
    struct watch_context *watch = context;
    printf("FIIO JD1 connected; applying persistent host profile.\n");
    fflush(stdout);
    struct delayed_apply *apply = calloc(1, sizeof(*apply));
    if (!apply) return;
    apply->device = device;
    apply->requested_db = watch->requested_db;
    CFRetain(device);
    CFRunLoopTimerContext timer_context = {0, apply, NULL, NULL, NULL};
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(
        kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + 0.2, 0, 0, 0,
        apply_timer_fired, &timer_context);
    if (!timer) {
        CFRelease(device);
        free(apply);
        return;
    }
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
    CFRelease(timer);
}

int main(int argc, char **argv) {
    struct cli_options options;
    int parsed = parse_cli(argc, argv, &options);
    if (parsed != 0) return parsed == 1 ? 0 : 2;

    bool is_probe = options.action == ACTION_PROBE;
    bool is_read = options.action == ACTION_READ;
    bool is_gain = options.action == ACTION_GAIN;
    bool is_save = options.action == ACTION_SAVE;
    bool is_watch = options.action == ACTION_WATCH;
    uint32_t address = options.address;
    uint8_t command = is_save ? 0x53 : is_read
        ? (address <= 0x100 ? 0x52 : 0x08) : 0x43;
    double requested_db = options.gain_db;

    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault,
                                                   kIOHIDOptionsTypeNone);
    CFMutableDictionaryRef match = matching_dictionary();
    IOHIDManagerSetDeviceMatching(manager, match);
    CFRelease(match);

    if (is_watch) {
        struct watch_context watch = {.requested_db = requested_db};
        IOHIDManagerRegisterDeviceMatchingCallback(manager, device_added, &watch);
        IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(),
                                        kCFRunLoopDefaultMode);
        IOReturn watch_rc = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
        if (watch_rc != kIOReturnSuccess) {
            fprintf(stderr, "Could not start JD1 watcher: 0x%08X\n", watch_rc);
            CFRelease(manager);
            return 1;
        }
        printf("Watching for FIIO JD1 %04X:%04X; target gain %+.1f dB.\n",
               JD1_VID, JD1_PID, requested_db);
        fflush(stdout);
        CFRunLoopRun();
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        CFRelease(manager);
        return 0;
    }

    IOReturn rc = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
    if (rc != kIOReturnSuccess) {
        fprintf(stderr, "Could not open IOHIDManager: 0x%08X\n", rc);
        CFRelease(manager);
        return 1;
    }

    IOHIDDeviceRef device = find_device(manager);
    if (!device) {
        fprintf(stderr, "FIIO JD1 HID interface %04X:%04X not found.\n", JD1_VID, JD1_PID);
        CFRelease(manager);
        return 1;
    }
    rc = IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
    if (rc != kIOReturnSuccess) {
        fprintf(stderr, "Could not open JD1 HID interface: 0x%08X\n", rc);
        CFRelease(device);
        CFRelease(manager);
        return 1;
    }

    uint8_t input_buffer[64] = {0};
    struct response_state state = {0};
    IOHIDDeviceRegisterInputReportCallback(device, input_buffer,
                                            sizeof(input_buffer), input_report, &state);
    IOHIDDeviceScheduleWithRunLoop(device, CFRunLoopGetCurrent(),
                                    kCFRunLoopDefaultMode);

    bool ok = false;
    if (is_probe || is_read) {
        ok = transact(device, address, command, 0, &state);
    } else if (is_save) {
        ok = transact(device, 0, command, 0, &state);
        uint32_t ack = 0;
        ok = ok && response_value(&state, &ack)
             && (ack == 0x03 || ack == 0x4f);
        if (ok)
            printf("Saved the current JD1 configuration to device flash.\n");
        else
            fprintf(stderr, "Persistent Save was not acknowledged.\n");
    } else if (is_gain) {
        uint32_t old_value = 0;
        ok = read_register(device, 0x66, &state, &old_value);
        if (ok) {
            int8_t left = (int8_t)(old_value & 0xff);
            int8_t right = (int8_t)((old_value >> 8) & 0xff);
            printf("Current digital DAC gain: L=%+.1f dB R=%+.1f dB (raw 0x%08X)\n",
                   left / 2.0, right / 2.0, old_value);
        }
    } else {
        IOHIDDeviceUnscheduleFromRunLoop(device, CFRunLoopGetCurrent(),
                                          kCFRunLoopDefaultMode);
        IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
        ok = apply_digital_gain(device, requested_db, false);
        CFRelease(device);
        CFRelease(manager);
        return ok ? 0 : 1;
    }

    IOHIDDeviceUnscheduleFromRunLoop(device, CFRunLoopGetCurrent(),
                                      kCFRunLoopDefaultMode);
    IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
    CFRelease(device);
    CFRelease(manager);
    return ok ? 0 : 1;
}
