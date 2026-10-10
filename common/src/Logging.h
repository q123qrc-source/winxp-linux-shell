// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QLoggingCategory>

#if defined(MEADOW_COMMON_LIBRARY)
#  define MEADOW_COMMON_EXPORT Q_DECL_EXPORT
#else
#  define MEADOW_COMMON_EXPORT Q_DECL_IMPORT
#endif

// Never log passwords or raw PAM conversation text in any category.
MEADOW_COMMON_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcTheme)
MEADOW_COMMON_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcSound)
