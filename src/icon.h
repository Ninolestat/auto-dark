#ifndef AUTODARK_ICON_H
#define AUTODARK_ICON_H

#include <windows.h>

// Creates a crisp anti-aliased HICON (size x size) with a minimal sun/moon split emblem.
// isLightActive = TRUE highlights sun; FALSE highlights moon.
HICON CreateAutoDarkIcon(int size, BOOL isLightActive);

#endif // AUTODARK_ICON_H
