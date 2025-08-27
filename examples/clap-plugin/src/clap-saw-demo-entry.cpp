#include "clap-saw-demo.h"
#include "clap-saw-demo-gui.h"
#include <CLAPExport.h>
#include <functional>

// One line export replaces entire pluginentry.cpp
MADRONALIB_EXPORT_CLAP_PLUGIN_WITH_GUI(ClapSawDemo, ClapSawDemoGUI, "Clap Saw Demo", "Madrona Labs")

// VST3 module initialization functions
extern "C" {
    bool InitModule() { return true; }
    bool DeinitModule() { return true; }
}

// VST3 module handle (required on macOS)
void* moduleHandle = nullptr;

// Additional VST3 symbols required by clap-wrapper
namespace Steinberg {
    // Forward declarations for classes we need to implement
    class ModuleInitializer {
    public:
        ModuleInitializer(std::function<void()>&& func, unsigned int order) {
            // Empty implementation for clap-wrapper compatibility
        }
    };
    
    class ModuleTerminator {
    public:
        ModuleTerminator(std::function<void()>&& func, unsigned int order) {
            // Empty implementation for clap-wrapper compatibility  
        }
    };
}
