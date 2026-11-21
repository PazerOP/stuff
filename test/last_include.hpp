#pragma once

// This file should be included at the end of all C++ files to poison dangerous functions
// and encourage use of safer alternatives

#ifdef __unix__

// Poison raw process functions - use mh::process instead
#pragma GCC poison fork execvp waitpid system

#endif // __unix__
