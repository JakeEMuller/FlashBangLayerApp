#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <hidapi.h>

constexpr unsigned short LEFT_VID = 0xfeed; // replace
constexpr unsigned short LEFT_PID = 0x8020; // replace
constexpr unsigned short RIGHT_VID = 0xfeed; // replace
constexpr unsigned short RIGHT_PID = 0x8021; // replace

constexpr unsigned short USAGE_PAGE = 0xFF60; // QMK default raw HID usage page
constexpr unsigned short LEFT_USAGE = 0x61;   // adjust if needed
constexpr unsigned short RIGHT_USAGE = 0x61;

constexpr uint16_t RAW_REPORT_SIZE = 32;  // must match QMK RAW_REPORT_SIZE

// Utility: find device path that matches criteria
std::string find_device_path(unsigned short vid, unsigned short pid, unsigned short usage_page, unsigned short usage) {
    struct hid_device_info* devs = hid_enumerate(vid, pid);
    struct hid_device_info* cur = devs;
    std::string path;
    while (cur) {
        if (cur->usage_page == usage_page && cur->usage == usage) {
            if (cur->path) { path = cur->path; break; }
        }
        cur = cur->next;
    }
    hid_free_enumeration(devs);
    return path;
}

int main() {
    if (hid_init() != 0) {
        std::cerr << "hid_init failed\n";
        return 1;
    }

    std::string left_path, right_path;
    while (true) {
        left_path = find_device_path(LEFT_VID, LEFT_PID, USAGE_PAGE, LEFT_USAGE);
        right_path = find_device_path(RIGHT_VID, RIGHT_PID, USAGE_PAGE, RIGHT_USAGE);

        if (!left_path.empty() && !right_path.empty()) break;
#ifdef _DEBUG
        std::cerr << "Waiting for devices (left/right) to appear...\n";
#endif
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    hid_device* left_dev = hid_open_path(left_path.c_str());
    hid_device* right_dev = hid_open_path(right_path.c_str());
    if (!left_dev || !right_dev) {
#ifdef _DEBUG
        std::cerr << "Failed to open devices\n";
#endif
        return 2;
    }

    // Set non-blocking or blocking as you prefer. We'll use blocking read here:
    hid_set_nonblocking(left_dev, 0);

    uint8_t buffer[RAW_REPORT_SIZE + 1] = { 0 };
    while (true) {

        // hid_write requires the first byte to be the report ID. So read from an offset 1 so it can be passed directly into the read.
        // The report ID should always be zero here, so no need to set it.
        int res = hid_read(left_dev, (buffer + 1), RAW_REPORT_SIZE); // blocking
        if (res > 0) {

            if (buffer[1] == 0x1 || buffer[1] == 0x2) 
            {
                buffer[0] = 0;
                int w = hid_write(right_dev, buffer, RAW_REPORT_SIZE + 1);
#ifdef _DEBUG
                std::cout << "Sent press, wrote " << w << " bytes\n";
#endif
            }
        }
        else {
            // error or timeout
            if (res < 0) {
#ifdef _DEBUG
                std::cerr << "hid_read error\n";
#endif // _DEBUG

            }
            // small sleep to avoid tight loop if non-blocking
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    hid_close(left_dev);
    hid_close(right_dev);
    hid_exit();
    return 0;
}
