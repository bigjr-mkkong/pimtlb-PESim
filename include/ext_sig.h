#ifndef EXT_SIG

#include <cstddef>

enum sig_cmd {
    PAUSE,
    RESUME
};

struct ext_sig_t{
    sig_cmd cmd;
    size_t time; 

    bool operator<(const ext_sig_t& other) const {
        return time > other.time;
    }

};

#define EXT_SIG
#endif
