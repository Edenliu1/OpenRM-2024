/*
 * AzureBot Armor Detector Demo
 * 
 * Complete armor detection pipeline demonstration using OpenRM-2024 library.
 * Works on both macOS (development) and Linux (deployment/Jetson).
 * 
 * Features:
 * - Real-time camera capture (OpenCV VideoCapture)
 * - Color-based preprocessing (red/blue armor detection)
 * - Lightbar detection using contour analysis
 * - Armor matching from lightbar pairs
 * - Performance monitoring (FPS, processing time)
 * 
 * Usage:
 *   ./armor_detector_demo           # Use default camera, detect BLUE armor
 *   ./armor_detector_demo 0 red     # Use camera 0, detect RED armor
 *   ./armor_detector_demo 1 blue    # Use camera 1, detect BLUE armor
 * 
 * Controls:
 *   'q' or ESC: Quit
 *   'r': Switch to RED armor detection
 *   'b': Switch to BLUE armor detection
 *   'd': Toggle debug view (show preprocessing steps)
 */

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <chrono>

// OpenRM headers (avoid openrm.h which includes TensorRT)
#include <pointer/pointer.h>          // Armor detection algorithms
#include <structure/enums.hpp>        // ArmorColor, ArmorID, etc.
#include <structure/stamp.hpp>        // LightbarPair, etc.
#include <utils/timer.h>              // Timing utilities
#include <utils/print.h>              // Debug printing

using namespace std;
using namespace cv;

// =============================================================================
// Configuration Parameters
// =============================================================================
struct DetectionConfig {
    // Color detection
    rm::ArmorColor target_color = rm::ARMOR_COLOR_BLUE;
    
    // Preprocessing
    double binary_threshold = 0.5;      // Higher = more strict (0-1)
    int morph_size = 3;                 // Morphological kernel size
    
    // Lightbar detection parameters
    double min_lightbar_area = 50.0;    // Increased to filter noise
    double max_lightbar_area = 3000.0;  // Reduced max
    double min_lightbar_ratio = 2.0;    // Stricter ratio (taller)
    double max_lightbar_ratio = 10.0;   // Reduced max
    double max_lightbar_angle = 45.0;   // Max tilt angle
    
    // Armor matching parameters
    double max_angle_diff = 8.0;        // Stricter angle matching
    double max_length_ratio = 1.5;      // Lightbars should be similar length
    double min_armor_ratio = 1.5;       // Armor is wider than tall
    double max_armor_ratio = 4.5;       // Not too wide
    double max_center_offset = 0.5;     // Centers should be aligned
    
    // Display
    bool show_debug = false;
};

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * @brief Draw detected lightbars on the image
 */
void drawLightbars(Mat& img, const vector<rm::Lightbar>& lightbars) {
    for (const auto& lb : lightbars) {
        // Draw rotated rectangle
        Point2f vertices[4];
        lb.rect.points(vertices);
        for (int i = 0; i < 4; i++) {
            line(img, vertices[i], vertices[(i + 1) % 4], Scalar(0, 255, 0), 2);
        }
        
        // Draw center point
        circle(img, lb.rect.center, 3, Scalar(0, 255, 255), -1);
    }
}

/**
 * @brief Draw detected armor pairs on the image
 */
void drawArmorPairs(Mat& img, const vector<rm::LightbarPair>& pairs, 
                    const DetectionConfig& config) {
    for (size_t i = 0; i < pairs.size(); i++) {
        const auto& pair = pairs[i];
        
        // Calculate center from both lightbars
        Point2f center = rm::getLightbarPairCenter(pair.first, pair.second);
        
        // Draw bounding box around both lightbars
        vector<Point2f> all_points;
        Point2f vertices1[4], vertices2[4];
        pair.first.rect.points(vertices1);
        pair.second.rect.points(vertices2);
        for (int j = 0; j < 4; j++) {
            all_points.push_back(vertices1[j]);
            all_points.push_back(vertices2[j]);
        }
        Rect bbox = boundingRect(all_points);
        rectangle(img, bbox, Scalar(0, 0, 255), 3);
        
        // Draw center cross
        int cross_size = 10;
        line(img, Point(center.x - cross_size, center.y), 
             Point(center.x + cross_size, center.y), Scalar(0, 255, 255), 2);
        line(img, Point(center.x, center.y - cross_size), 
             Point(center.x, center.y + cross_size), Scalar(0, 255, 255), 2);
        
        // Draw info text
        string color_str = (config.target_color == rm::ARMOR_COLOR_BLUE) ? "BLUE" : "RED";
        string info = format("Armor #%d [%s]", (int)i, color_str.c_str());
        putText(img, info, 
                Point(bbox.x, bbox.y - 10),
                FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 255, 0), 2);
    }
}

/**
 * @brief Display performance statistics
 */
void drawStats(Mat& img, double fps, double detect_time_ms, int armor_count) {
    int y = 30;
    int line_height = 35;
    
    // Background for stats
    rectangle(img, Point(5, 5), Point(350, 150), Scalar(0, 0, 0), -1);
    rectangle(img, Point(5, 5), Point(350, 150), Scalar(255, 255, 255), 2);
    
    // Stats text
    putText(img, format("FPS: %.1f", fps),
            Point(15, y), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 0), 2);
    y += line_height;
    
    putText(img, format("Detection: %.1f ms", detect_time_ms),
            Point(15, y), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 0), 2);
    y += line_height;
    
    putText(img, format("Armors Found: %d", armor_count),
            Point(15, y), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 255, 255), 2);
    y += line_height;
}

/**
 * @brief Display controls help
 */
void drawHelp(Mat& img) {
    int y = img.rows - 120;
    putText(img, "Controls: [Q]uit  [R]ed  [B]lue  [D]ebug",
            Point(10, y), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 255), 2);
    y += 30;
    putText(img, "[+/-] Threshold  [UP/DOWN] Min Area",
            Point(10, y), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(200, 200, 200), 2);
}

// =============================================================================
// Main Detection Function
// =============================================================================

/**
 * @brief Perform armor detection on a single frame
 */
int detectArmors(const Mat& src, Mat& dst, DetectionConfig& config,
                  double& detect_time_ms) {
    auto t_start = chrono::high_resolution_clock::now();
    
    dst = src.clone();
    
    // Step 1: Convert to grayscale based on target color
    Mat gray;
    rm::getGrayScale(src, gray, config.target_color, rm::GRAY_SCALE_METHOD_RGB);
    
    // Step 2: Convert to binary image
    Mat binary;
    rm::getBinary(gray, binary, config.binary_threshold, rm::BINARY_METHOD_MAX_MIN_RATIO);
    
    // Step 2.5: Morphological operations to reduce noise
    Mat kernel = getStructuringElement(MORPH_RECT, 
                                      Size(config.morph_size, config.morph_size));
    morphologyEx(binary, binary, MORPH_CLOSE, kernel);  // Fill small holes
    morphologyEx(binary, binary, MORPH_OPEN, kernel);   // Remove small noise
    
    // Step 3: Find contours
    vector<vector<Point>> contours;
    findContours(binary, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    
    // Step 4: Filter contours to find valid lightbars
    vector<rm::Lightbar> lightbars;
    rm::getLightbarsFromContours(
        contours, lightbars,
        5.0,      // min_rect_side
        5000.0,   // max_rect_side  
        10.0,     // min_value_area
        0.5,      // min_ratio_area
        90.0      // max_angle
    );
    
    // Set angles for each lightbar (OpenCV 4.5+)
    for (auto& lb : lightbars) {
        rm::setLigntbarAngleRectCV45(lb);
    }
    
    // Filter lightbars by size, ratio, and angle
    vector<rm::Lightbar> valid_lightbars;
    for (const auto& lb : lightbars) {
        double area = lb.rect.size.area();
        double ratio = rm::getRatioRectSide(lb.rect);
        double angle = abs(lb.angle);
        
        // Filter by geometry
        if (area >= config.min_lightbar_area && 
            area <= config.max_lightbar_area &&
            ratio >= config.min_lightbar_ratio && 
            ratio <= config.max_lightbar_ratio &&
            angle <= config.max_lightbar_angle) {
            valid_lightbars.push_back(lb);
        }
    }
    
    // Step 5: Match lightbars into armor pairs with strict validation
    vector<rm::LightbarPair> valid_pairs;
    for (size_t i = 0; i < valid_lightbars.size(); i++) {
        for (size_t j = i + 1; j < valid_lightbars.size(); j++) {
            const auto& lb1 = valid_lightbars[i];
            const auto& lb2 = valid_lightbars[j];
            
            // Calculate matching metrics
            double angle_diff = rm::getAngleDiffLightbarPair(lb1, lb2);
            double length_ratio = rm::getRatioLengthLightbarPair(lb1, lb2);
            double armor_ratio = rm::getRatioArmorSide(lb1, lb2);
            double center_offset = rm::getCenterOffsetLightbarPair(lb1, lb2);
            
            // Calculate distance between lightbars
            Point2f center1 = lb1.rect.center;
            Point2f center2 = lb2.rect.center;
            double distance = norm(center1 - center2);
            double avg_length = rm::getValueLengthLightbarPair(lb1, lb2);
            double distance_ratio = distance / avg_length;
            
            // Strict armor validation
            bool valid_angles = angle_diff <= config.max_angle_diff;
            bool similar_lengths = length_ratio <= config.max_length_ratio;
            bool valid_ratio = (armor_ratio >= config.min_armor_ratio && 
                               armor_ratio <= config.max_armor_ratio);
            bool aligned_centers = (center_offset / avg_length) <= config.max_center_offset;
            bool reasonable_distance = (distance_ratio >= 1.5 && distance_ratio <= 5.0);
            
            if (valid_angles && similar_lengths && valid_ratio && 
                aligned_centers && reasonable_distance) {
                valid_pairs.push_back(rm::LightbarPair(lb1, lb2));
            }
        }
    }
    
    // Calculate detection time
    auto t_end = chrono::high_resolution_clock::now();
    detect_time_ms = chrono::duration<double, milli>(t_end - t_start).count();
    
    // Draw results
    if (config.show_debug) {
        // Show preprocessing steps side-by-side
        Mat binary_color, gray_color;
        cvtColor(binary, binary_color, COLOR_GRAY2BGR);
        cvtColor(gray, gray_color, COLOR_GRAY2BGR);
        
        // Create horizontal concatenation: original | gray | binary
        Mat debug_view;
        hconcat(std::vector<Mat>{src.clone(), gray_color, binary_color}, debug_view);
        
        // Draw detections only on the first (original) part
        Mat roi = debug_view(Rect(0, 0, src.cols, src.rows));
        drawLightbars(roi, valid_lightbars);
        drawArmorPairs(roi, valid_pairs, config);
        
        // Add labels
        putText(debug_view, "Original", Point(10, 30), 
                FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);
        putText(debug_view, "Grayscale", Point(src.cols + 10, 30), 
                FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);
        putText(debug_view, "Binary", Point(src.cols * 2 + 10, 30), 
                FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);
        
        dst = debug_view;
    } else {
        // Normal view with detections
        drawLightbars(dst, valid_lightbars);
        drawArmorPairs(dst, valid_pairs, config);
    }
    
    return valid_pairs.size();
}

// =============================================================================
// Main Function
// =============================================================================

int main(int argc, char** argv) {
    // Parse command line arguments
    int camera_id = 0;
    DetectionConfig config;
    
    if (argc > 1) {
        camera_id = atoi(argv[1]);
    }
    if (argc > 2) {
        string color_str = argv[2];
        if (color_str == "red" || color_str == "RED") {
            config.target_color = rm::ARMOR_COLOR_RED;
        } else {
            config.target_color = rm::ARMOR_COLOR_BLUE;
        }
    }
    
    // Print startup info
    cout << "\n========================================" << endl;
    cout << "  AzureBot Armor Detector Demo" << endl;
    cout << "  OpenRM-2024 Library" << endl;
    cout << "========================================" << endl;
    cout << "Camera ID: " << camera_id << endl;
    cout << "Target Color: " 
         << (config.target_color == rm::ARMOR_COLOR_BLUE ? "BLUE" : "RED") 
         << endl;
    cout << "========================================\n" << endl;
    
    // Open camera
    VideoCapture cap(camera_id);
    if (!cap.isOpened()) {
        cerr << "ERROR: Cannot open camera " << camera_id << endl;
        return -1;
    }
    
    // Try to set resolution (may not work on all cameras)
    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);
    
    int width = (int)cap.get(CAP_PROP_FRAME_WIDTH);
    int height = (int)cap.get(CAP_PROP_FRAME_HEIGHT);
    cout << "Camera Resolution: " << width << "x" << height << endl;
    cout << "\nStarting detection... Press 'q' to quit\n" << endl;
    
    // Main loop
    Mat frame, display;
    auto last_fps_time = chrono::high_resolution_clock::now();
    int frame_count = 0;
    double fps = 0.0;
    double detect_time_ms = 0.0;
    
    while (true) {
        // Capture frame
        cap >> frame;
        if (frame.empty()) {
            cerr << "ERROR: Empty frame" << endl;
            break;
        }
        
        // Perform detection and get armor count
        int armor_count = detectArmors(frame, display, config, detect_time_ms);
        
        // Calculate FPS
        frame_count++;
        auto current_time = chrono::high_resolution_clock::now();
        auto elapsed_ms = chrono::duration<double, milli>(
            current_time - last_fps_time).count();
        
        if (elapsed_ms >= 1000.0) {
            fps = frame_count * 1000.0 / elapsed_ms;
            frame_count = 0;
            last_fps_time = current_time;
        }
        
        // Draw overlays
        drawStats(display, fps, detect_time_ms, armor_count);
        drawHelp(display);
        
        // Show result
        imshow("AzureBot Armor Detector", display);
        
        // Handle keyboard input
        char key = waitKey(1);
        if (key == 'q' || key == 'Q' || key == 27) {  // Quit
            break;
        } else if (key == 'r' || key == 'R') {  // Red
            config.target_color = rm::ARMOR_COLOR_RED;
            cout << "Switched to RED armor detection" << endl;
        } else if (key == 'b' || key == 'B') {  // Blue
            config.target_color = rm::ARMOR_COLOR_BLUE;
            cout << "Switched to BLUE armor detection" << endl;
        } else if (key == 'd' || key == 'D') {  // Debug
            config.show_debug = !config.show_debug;
            cout << "Debug view: " << (config.show_debug ? "ON" : "OFF") << endl;
        } else if (key == '+' || key == '=') {  // Increase threshold
            config.binary_threshold = min(0.9, config.binary_threshold + 0.05);
            cout << "Binary threshold: " << config.binary_threshold << endl;
        } else if (key == '-' || key == '_') {  // Decrease threshold
            config.binary_threshold = max(0.1, config.binary_threshold - 0.05);
            cout << "Binary threshold: " << config.binary_threshold << endl;
        } else if (key == 82 || key == 'w') {  // Up arrow or W - increase min area
            config.min_lightbar_area += 10.0;
            cout << "Min lightbar area: " << config.min_lightbar_area << endl;
        } else if (key == 84 || key == 's') {  // Down arrow or S - decrease min area
            config.min_lightbar_area = max(10.0, config.min_lightbar_area - 10.0);
            cout << "Min lightbar area: " << config.min_lightbar_area << endl;
        }
    }
    
    cap.release();
    destroyAllWindows();
    
    cout << "\nDemo completed successfully!" << endl;
    return 0;
}

