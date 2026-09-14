/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// GeneralsX @bugfix Android port 11/07/2026 targetSdk 35 (Android 15)
// enforces edge-to-edge by default -- without explicit inset handling,
// top-row content (buttons, status text) in these landscape Settings/Log/
// Account screens gets drawn under the status bar and, on phones with a
// landscape-edge camera cutout, under the cutout itself, making it
// invisible and unclickable (reported on a real device: the "Share" button
// in the log viewer and "Sign Out" in the GeneralsOnline account screen
// were both unreachable). Pads the given root view by the system bar +
// cutout insets on all sides so nothing important is ever drawn under them.

package com.generalsx.zerohour;

import android.view.View;

import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

final class InsetUtil {
    private InsetUtil() {}

    static void applySafeInsets(View root) {
        // Capture whatever padding the caller already set (e.g. a uniform
        // content margin) BEFORE this runs, and add insets on top of it
        // rather than overwriting it -- setPadding() replaces, it doesn't
        // accumulate.
        final int baseLeft = root.getPaddingLeft();
        final int baseTop = root.getPaddingTop();
        final int baseRight = root.getPaddingRight();
        final int baseBottom = root.getPaddingBottom();

        ViewCompat.setOnApplyWindowInsetsListener(root, (v, insets) -> {
            Insets bars = insets.getInsets(
                WindowInsetsCompat.Type.systemBars() | WindowInsetsCompat.Type.displayCutout());
            v.setPadding(baseLeft + bars.left, baseTop + bars.top,
                baseRight + bars.right, baseBottom + bars.bottom);
            return insets;
        });
    }
}
