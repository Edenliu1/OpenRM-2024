# AzureBot Demo - OpenRM-2024

Complete armor detection demonstration using the OpenRM-2024 library.

## 🎯 Features

- **Cross-Platform**: Works on both macOS (development) and Linux (Jetson deployment)
- **Real-Time Detection**: Uses OpenCV VideoCapture for camera input
- **Complete Pipeline**: Color preprocessing → Lightbar detection → Armor matching
- **Interactive Controls**: Switch colors, toggle debug view in real-time
- **Performance Monitoring**: FPS and detection time display

## 📋 Prerequisites

Make sure OpenRM-2024 is already built and installed:

```bash
cd /path/to/OpenRM-2024
sudo ./run.sh
```

This will install the OpenRM libraries to `/usr/local/lib/` and headers to `/usr/local/include/openrm/`.

## 🔨 Building the Demo

### Quick Build (Recommended)

```bash
cd azurebot_demo
./build.sh
```

### Manual Build

```bash
cd azurebot_demo
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

## 🚀 Running the Demos

### 1. Camera Test (Simple)

First, verify your camera is working:

```bash
./build/camera_test          # Use default camera (ID 0)
./build/camera_test 1        # Use camera ID 1
```

**Expected Output**: Live video feed with FPS counter

### 2. Armor Detector (Full Pipeline)

Run the complete armor detection:

```bash
# Basic usage (default camera, blue armor)
./build/armor_detector_demo

# Specify camera and color
./build/armor_detector_demo 0 blue   # Camera 0, blue armor
./build/armor_detector_demo 1 red    # Camera 1, red armor
```

**Interactive Controls**:
- `Q` or `ESC`: Quit program
- `R`: Switch to RED armor detection
- `B`: Switch to BLUE armor detection  
- `D`: Toggle debug view (shows preprocessing steps)

## 📊 Understanding the Output

### Main View
- **Green Boxes**: Detected lightbars (armor light strips)
- **Red Boxes**: Matched armor pairs
- **Yellow Cross**: Armor center point
- **Stats Panel**: FPS, detection time, armor count

### Debug View (Press `D`)
Shows three-channel visualization:
- **Red Channel**: Binary image (thresholded)
- **Green Channel**: Grayscale (color-filtered)
- **Blue Channel**: Original image
- Overlaid with all detection results

## 🎨 Detection Algorithm

```
Input Image
    ↓
Color Filtering (Red/Blue separation)
    ↓
Binary Thresholding
    ↓
Contour Detection
    ↓
Lightbar Filtering (size, ratio, angle)
    ↓
Lightbar Pairing (geometry matching)
    ↓
Armor Validation
    ↓
Output with Bounding Boxes
```

## 🔧 Tuning Parameters

Edit `armor_detector_demo.cpp` to adjust detection sensitivity:

```cpp
struct DetectionConfig {
    // Lightbar detection
    double min_lightbar_area = 20.0;    // Minimum lightbar size
    double max_lightbar_area = 5000.0;  // Maximum lightbar size
    double min_lightbar_ratio = 1.5;    // Min height/width ratio
    double max_lightbar_ratio = 15.0;   // Max height/width ratio
    
    // Armor matching
    double max_angle_diff = 15.0;       // Max angle difference (degrees)
    double min_armor_ratio = 1.2;       // Min armor width/height
    double max_armor_ratio = 5.0;       // Max armor width/height
};
```

After editing, rebuild:
```bash
./build.sh
```

## 🐛 Troubleshooting

### Camera Not Found
```
ERROR: Cannot open camera 0
```
**Solution**: Try different camera IDs (0, 1, 2, etc.) or check camera permissions

### Low FPS
**Possible causes**:
- High resolution (try lower resolution in code)
- Debug view enabled (press `D` to disable)
- Slow hardware

### No Armors Detected
**Solutions**:
- Press `R` or `B` to switch detection color
- Adjust lighting conditions
- Point camera at red/blue objects
- Tune detection parameters (see above)

### Build Errors

**If you see "Cannot find OpenRM"**:
```bash
# Make sure OpenRM is installed
cd /path/to/OpenRM-2024
sudo ./run.sh

# Verify installation
ls /usr/local/lib/libopenrm_*
ls /usr/local/include/openrm/
```

**If you see "Cannot find Ceres" or "Cannot find Eigen"**:
```bash
# macOS
brew install ceres-solver eigen

# Ubuntu/Jetson
sudo apt-get install libceres-dev libeigen3-dev
```

## 📚 Code Structure

```
azurebot_demo/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── build.sh                    # Quick build script
├── camera_test.cpp             # Simple camera verification
└── armor_detector_demo.cpp     # Full detection pipeline
```

## 🎓 Learning Resources

### OpenRM Modules Used

| Module | Purpose | Headers |
|--------|---------|---------|
| **pointer** | Color processing, lightbar detection | `<pointer/pointer.h>` |
| **structure** | Data structures (enums, stamps) | `<structure/enums.hpp>` |
| **utils** | Timing, debug printing | `<utils/timer.h>` |

### Key Functions

```cpp
// Color filtering
rm::getGrayScale(src, gray, color, method);

// Binary thresholding
rm::getBinary(gray, binary, threshold, method);

// Lightbar detection
rm::getLightbarsFromContours(contours, lightbars);

// Armor matching
rm::getArmorPairsFromLightbars(lightbars, armor_pairs);
```

## 🎯 Next Steps

1. **For macOS Students**: 
   - Use this demo for algorithm development
   - Test with webcam or video files
   - Debug and tune parameters

2. **For Jetson Deployment**:
   - Transfer code to Jetson
   - Switch to industrial camera (GigE/USB3)
   - Enable CUDA acceleration (if using TensorRT models)
   - Integrate with serial communication for turret control

3. **Advanced Features to Add**:
   - PnP solver for 3D position estimation
   - Kalman filter for motion prediction
   - Multiple armor tracking
   - Target selection logic

## 📞 Support

- **Issues**: Report in the main OpenRM-2024 repository
- **Wiki**: https://github.com/HHgzs/TJURM-2024/wiki
- **Video Tutorial**: https://www.bilibili.com/video/BV1jApUe1EPT/

## 📝 License

Same as OpenRM-2024 main project.

---

**Happy Detecting! 🎉**

