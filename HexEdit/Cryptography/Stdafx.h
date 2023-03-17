
// MSVC quirk: a pre-compiled header cannot be included via relative path
// directly, but you can put a file with the same name in the folder, include
// the real PCH, and that'll work without issue.
#include "../Stdafx.h"
