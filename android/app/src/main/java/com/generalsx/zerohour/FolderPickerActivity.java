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

// GeneralsX @build Android port 07/07/2026
//
// A minimal folder browser over plain java.io.File — deliberately NOT the
// system Storage Access Framework picker (ACTION_OPEN_DOCUMENT_TREE): SAF
// hands back a content:// tree the native engine's plain fopen()/chdir()
// calls cannot use directly, which would force copying the ~2-3 GB game
// data into app storage before every launch. Requires the "All files
// access" permission (MANAGE_EXTERNAL_STORAGE, granted from SetupActivity)
// so the resulting path is a real filesystem path the engine can chdir()
// into wherever the user actually put their files — Downloads, an SD card,
// wherever a normal file manager or USB cable already reaches.

package com.generalsx.zerohour;

import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.core.content.ContextCompat;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

public class FolderPickerActivity extends Activity {

    static final String EXTRA_SELECTED_PATH = "selected_path";

    private File currentDir;
    private TextView pathLabel;
    private TextView hintLabel;
    private ListView listView;

    @Override
    protected void attachBaseContext(android.content.Context newBase) {
        super.attachBaseContext(LocaleHelper.wrap(newBase));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setTitle(R.string.folderpicker_title);

        File start = Environment.getExternalStorageDirectory();
        currentDir = (start != null && start.isDirectory()) ? start : new File("/storage/emulated/0");

        // GeneralsX @feature Android port launcher-ui-2026 08/09/2026 Same
        // shell as the rest of the launcher: an app bar carrying the current
        // path, the folder list on the page ground, and the two decisions as
        // M3 actions at the bottom where a thumb reaches them.
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(UiKit.color(this, R.color.gzh_background));

        UiKit.appBar(root, getString(R.string.setup_window_title),
            getString(R.string.folderpicker_title), 0, null, null);

        int gutter = UiKit.dim(this, R.dimen.gzh_gutter);

        pathLabel = new TextView(this);
        pathLabel.setPadding(gutter, 0, gutter, UiKit.dp(this, 2));
        pathLabel.setTextIsSelectable(true);
        pathLabel.setTextSize(android.util.TypedValue.COMPLEX_UNIT_PX,
            UiKit.dim(this, R.dimen.gzh_text_body));
        pathLabel.setTextColor(UiKit.color(this, R.color.gzh_on_surface));
        root.addView(pathLabel);

        hintLabel = new TextView(this);
        hintLabel.setPadding(gutter, 0, gutter, UiKit.dim(this, R.dimen.gzh_item_gap_tight));
        hintLabel.setTextSize(android.util.TypedValue.COMPLEX_UNIT_PX,
            UiKit.dim(this, R.dimen.gzh_text_caption));
        hintLabel.setTextColor(UiKit.color(this, R.color.gzh_on_surface_variant));
        root.addView(hintLabel);

        listView = new ListView(this);
        listView.setDivider(new android.graphics.drawable.ColorDrawable(
            UiKit.color(this, R.color.gzh_outline_variant)));
        listView.setDividerHeight(Math.max(1, UiKit.dp(this, 1)));
        listView.setPadding(UiKit.dim(this, R.dimen.gzh_item_gap_tight), 0,
            UiKit.dim(this, R.dimen.gzh_item_gap_tight), 0);
        listView.setClipToPadding(false);
        listView.setSelector(new android.graphics.drawable.ColorDrawable(
            UiKit.color(this, R.color.gzh_ripple_light)));
        LinearLayout.LayoutParams listParams = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f);
        root.addView(listView, listParams);

        LinearLayout actions = new LinearLayout(this);
        actions.setOrientation(LinearLayout.VERTICAL);
        actions.setPadding(gutter, UiKit.dim(this, R.dimen.gzh_item_gap_tight),
            gutter, UiKit.dim(this, R.dimen.gzh_item_gap_tight));
        root.addView(actions, new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        LinearLayout buttonRow = UiKit.buttonRow(actions);
        UiKit.share(UiKit.button(buttonRow, UiKit.BTN_OUTLINE, 0,
            getString(R.string.common_cancel),
            () -> { setResult(RESULT_CANCELED); finish(); }), true);
        UiKit.share(UiKit.button(buttonRow, UiKit.BTN_PRIMARY, R.drawable.ic_gzh_check,
            getString(R.string.folderpicker_button_use), this::finishWithSelection), false);

        setContentView(root);
        InsetUtil.applySafeInsets(root);

        listView.setOnItemClickListener((AdapterView<?> parent, View view, int position, long id) -> {
            String name = (String) parent.getItemAtPosition(position);
            if (getString(R.string.folderpicker_up_entry).equals(name)) {
                File parentDir = currentDir.getParentFile();
                if (parentDir != null && parentDir.canRead()) {
                    currentDir = parentDir;
                    refresh();
                }
                return;
            }
            File next = new File(currentDir, name);
            if (next.isDirectory()) {
                currentDir = next;
                refresh();
            }
        });

        refresh();
    }

    private void refresh() {
        pathLabel.setText(currentDir.getAbsolutePath());
        hintLabel.setText(SetupActivity.isValidGameFolder(currentDir)
            ? getString(R.string.folderpicker_hint_valid)
            : getString(R.string.folderpicker_hint_invalid));

        List<String> entries = new ArrayList<>();
        if (currentDir.getParentFile() != null) {
            entries.add(getString(R.string.folderpicker_up_entry));
        }
        File[] children = currentDir.listFiles();
        if (children != null) {
            List<File> dirs = new ArrayList<>();
            for (File f : children) {
                if (f.isDirectory() && !f.isHidden()) {
                    dirs.add(f);
                }
            }
            Collections.sort(dirs, Comparator.comparing(File::getName, String.CASE_INSENSITIVE_ORDER));
            for (File d : dirs) {
                entries.add(d.getName());
            }
        } else {
            Toast.makeText(this, R.string.folderpicker_toast_cant_read, Toast.LENGTH_LONG).show();
        }

        // Framework list rows on a near-black ground default to a light-theme
        // text colour on some OEM builds; style each row explicitly instead,
        // and give it the same folder glyph the rest of the launcher uses.
        ArrayAdapter<String> adapter = new ArrayAdapter<String>(
                this, android.R.layout.simple_list_item_1, entries) {
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                TextView row = (TextView) super.getView(position, convertView, parent);
                row.setTextColor(UiKit.color(FolderPickerActivity.this, R.color.gzh_on_surface));
                row.setTextSize(android.util.TypedValue.COMPLEX_UNIT_PX,
                    UiKit.dim(FolderPickerActivity.this, R.dimen.gzh_text_body));
                int padH = UiKit.dim(FolderPickerActivity.this, R.dimen.gzh_item_gap);
                int padV = UiKit.dp(FolderPickerActivity.this, 14);
                row.setPadding(padH, padV, padH, padV);
                row.setCompoundDrawablePadding(padH);
                android.graphics.drawable.Drawable icon = ContextCompat.getDrawable(
                    FolderPickerActivity.this, R.drawable.ic_gzh_folder);
                if (icon != null) {
                    int s = UiKit.dim(FolderPickerActivity.this, R.dimen.gzh_icon);
                    icon.setBounds(0, 0, s, s);
                    icon.setTint(UiKit.color(FolderPickerActivity.this, R.color.gzh_primary));
                    row.setCompoundDrawablesRelative(icon, null, null, null);
                }
                return row;
            }
        };
        listView.setAdapter(adapter);
    }

    private void finishWithSelection() {
        android.content.Intent result = new android.content.Intent();
        result.putExtra(EXTRA_SELECTED_PATH, currentDir.getAbsolutePath());
        setResult(RESULT_OK, result);
        finish();
    }

}
