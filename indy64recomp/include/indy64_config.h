#ifndef __INDY64_CONFIG_H__
#define __INDY64_CONFIG_H__

#include <string>

namespace indy64 {
    // The id names the folder for config, saves and the imported ROM (AppData); it keeps the old project name so
    // existing saves stay where they are.
    inline const std::u8string program_id = u8"Indy64Recompiled";
    inline const std::string program_name = "Babylon Infernalis";

    void init_config();
};

#endif
