// GeneralsX @feature Codex 13/09/2026 Smooth, local Canvas artwork for
// native OpenXR panels. No WebView, UI thread, permissions or game assets.
package com.generalsx.zerohour;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.text.Layout;
import android.text.StaticLayout;
import android.text.TextPaint;
import android.text.TextUtils;

public final class XrPanelPainter {
    private XrPanelPainter() {}

    // GeneralsX @feature Ultron 15/09/2026 P21 shared palette.
    private static final int BG = Color.rgb(14, 24, 32);
    private static final int EDGE = Color.rgb(42, 59, 71);
    private static final int INSET = Color.rgb(10, 18, 24);
    private static final int BTN = Color.rgb(30, 46, 58);
    private static final int BTN2 = Color.rgb(24, 36, 46);
    private static final int IMM = Color.rgb(43, 59, 71);
    private static final int SUB = Color.rgb(180, 200, 209);
    private static final int MUT = Color.rgb(123, 143, 154);
    private static final int AMBER = Color.rgb(255, 166, 31);
    private static final int CYAN = Color.rgb(50, 205, 192);
    private static final int ARMED_BG = Color.rgb(42, 36, 21);
    private static final int DANGER_BG = Color.rgb(58, 42, 30);
    private static final int DISABLED_BG = Color.rgb(23, 33, 42);

    // Roles/states mirror GeneralsMD/Code/Main/XrPanelLayout.h exactly.
    private static final int ROLE_BUTTON = 0, ROLE_TARGETED = 1, ROLE_IMMEDIATE = 2, ROLE_CLOSE = 3,
        ROLE_SECTION = 4, ROLE_CONTEXT = 5, ROLE_GROUPNUM = 6, ROLE_TAB = 7, ROLE_INFO = 8,
        ROLE_BODY = 9, ROLE_CARD = 10, ROLE_OP = 11, ROLE_DANGER = 12;
    private static final int ST_HOVER = 1, ST_SELECTED = 2, ST_ARMED = 4, ST_PENDING = 8,
        ST_DISABLED = 16, ST_ON = 32, ST_ACTIVE = 64;

    public static int[] paint(String title, String detail, String labels, int hover, int kind) {
        int width = kind == 0 ? 192 : 768;
        int height = kind == 0 ? 128 : kind == 6 ? 768 : kind == 5 ? 1280 : (kind == 1 || kind == 3 || kind == 4 || kind == 7) ? 1024 : 384;
        // GeneralsX @feature Muse 16/09/2026 Match-result card: kind 2 with a
        // positive hover selects the accent (1 victory gold, 2 defeat red,
        // 3 neutral). Existing kind-2 callers pass -1 and stay pixel-identical.
        boolean result = kind == 2 && hover > 0;
        int accent = !result ? CYAN : hover == 1 ? AMBER : hover == 2 ? Color.rgb(255, 92, 92) : CYAN;
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        fill.setColor(Color.rgb(14, 24, 32));
        canvas.drawRoundRect(2, 2, width - 2, height - 2, 28, 28, fill);
        fill.setColor(accent);
        canvas.drawRoundRect(24, kind == 1 ? 12 : 22, kind == 0 ? 168 : 120, kind == 1 ? 16 : 28, 3, 3, fill);
        TextPaint text = new TextPaint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
        text.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        text.setColor(Color.WHITE);
        text.setTextSize(kind == 0 ? 46 : 36);
        if (kind == 0) {
            text.setTextAlign(Paint.Align.CENTER);
            text.setTextSize(title.length() > 2 ? 28 : 46);
            canvas.drawText(title, width / 2f, 89, text);
        } else if (kind == 6) {
            // GeneralsX @feature Codex 14/09/2026 Full native descriptions:
            // fixed legible type, line-aligned pages, no ellipsis or lost tail.
            text.setTextSize(30);
            StaticLayout heading = StaticLayout.Builder.obtain(title, 0, title.length(), text, 704)
                .setIncludePad(false).build();
            canvas.save(); canvas.translate(32, 48); heading.draw(canvas); canvas.restore();
            int top = Math.max(106, 60 + heading.getHeight());
            text.setTextSize(26); text.setTypeface(Typeface.create("sans-serif", Typeface.NORMAL));
            text.setColor(Color.rgb(210, 224, 232));
            StaticLayout body = StaticLayout.Builder.obtain(detail, 0, detail.length(), text, 704)
                .setIncludePad(false).setLineSpacing(3, 1).build();
            java.util.ArrayList<Integer> starts = new java.util.ArrayList<>();
            starts.add(0); int pageStart = 0;
            for (int line = 0; line < body.getLineCount(); ++line) {
                if (body.getLineBottom(line) - body.getLineTop(pageStart) > 704 - top && line > pageStart) {
                    starts.add(line); pageStart = line;
                }
            }
            int page = Math.floorMod(Math.max(0, hover), starts.size());
            int first = starts.get(page);
            int end = page + 1 < starts.size() ? body.getLineTop(starts.get(page + 1)) : body.getHeight();
            canvas.save(); canvas.clipRect(32, top, 736, top + end - body.getLineTop(first));
            canvas.translate(32, top - body.getLineTop(first)); body.draw(canvas); canvas.restore();
            if (starts.size() > 1) {
                text.setTextSize(22); text.setColor(Color.rgb(50, 205, 192));
                canvas.drawText((page + 1) + " / " + starts.size() + "  ·  10 s", 32, 743, text);
            }
        } else {
            // GeneralsX @refactor Ultron 15/09/2026 P21 kinds 1/3/4/5 moved to
            // paint2 (shared native control table). Only 0/2/6/7 remain here.
            if (result) {
                text.setTextSize(64);
                text.setColor(hover == 1 ? AMBER : hover == 2 ? Color.rgb(255, 130, 130) : Color.WHITE);
                text.setTextAlign(Paint.Align.CENTER);
                canvas.drawText(TextUtils.ellipsize(title, text, 704, TextUtils.TruncateAt.END).toString(), width / 2f, 100, text);
                text.setTextAlign(Paint.Align.LEFT);
            } else {
                canvas.drawText(TextUtils.ellipsize(title, text, 704, TextUtils.TruncateAt.END).toString(), 32, 82, text);
            }
            text.setTypeface(Typeface.create("sans-serif", Typeface.NORMAL));
            text.setColor(Color.rgb(180, 200, 209));
            text.setTextSize(kind == 7 ? 24 : 28);
            StaticLayout description = StaticLayout.Builder.obtain(detail, 0, detail.length(), text, 704)
                .setAlignment(Layout.Alignment.ALIGN_NORMAL).setIncludePad(false)
                .setMaxLines(kind == 7 ? 6 : 8).setEllipsize(TextUtils.TruncateAt.END).build();
            canvas.save(); canvas.translate(32, 116); description.draw(canvas); canvas.restore();
            if (kind == 7) {
                // GeneralsX @feature Codex 14/09/2026 One setup step per page.
                // Empty actions are absent, instructions live above the buttons.
                String[] buttons = labels.split("\n", -1);
                for (int i = 0; i < Math.min(6, buttons.length); ++i)
                    if (!buttons[i].isEmpty())
                        button(canvas, fill, text, buttons[i], 32, 300+i*82, 704, 68, hover == i, 27);
                if (buttons.length > 17) {
                    button(canvas, fill, text, buttons[16], 32, 900, 344, 72, hover == 16, 25);
                    button(canvas, fill, text, buttons[17], 392, 900, 344, 72, hover == 17, 25);
                }
            }
        }
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        bitmap.recycle();
        return pixels;
    }

    private static void button(Canvas canvas, Paint fill, TextPaint text, String label,
            float x, float y, float w, float h, boolean hover, float size) {
        boolean disabled = label.startsWith("! ");
        if (disabled) label = label.substring(2);
        fill.setColor(disabled ? Color.rgb(23, 33, 42) : hover ? Color.rgb(29, 100, 107) : Color.rgb(30, 46, 58));
        canvas.drawRoundRect(x, y, x + w, y + h, 10, 10, fill);
        text.setColor(disabled ? Color.rgb(123, 143, 154) : Color.WHITE); text.setTextSize(size);
        canvas.drawText(TextUtils.ellipsize(label, text, w - 28, TextUtils.TruncateAt.END).toString(), x + 14, y + h / 2 + size * .35f, text);
    }

    // GeneralsX @feature Ultron 15/09/2026 P21 primitive renderer for the
    // shared native control table: 8 ints per control (id, x, y, w, h, role,
    // state, labelIndex). Java owns no geometry; pixels and UV hit regions
    // are generated from the same rects in XrPanelLayout.h.
    public static int[] paint2(String title, String detail, String labels, int[] ctrl, int kind) {
        int width = 768;
        int height = kind == 5 ? 1280 : 1024;
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        fill.setColor(BG);
        canvas.drawRoundRect(2, 2, width - 2, height - 2, 16, 16, fill);
        fill.setColor(EDGE); fill.setStyle(Paint.Style.STROKE); fill.setStrokeWidth(2);
        canvas.drawRoundRect(3, 3, width - 3, height - 3, 15, 15, fill);
        fill.setStyle(Paint.Style.FILL);
        TextPaint text = new TextPaint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
        text.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        text.setColor(AMBER); text.setTextSize(34);
        canvas.drawText(title, 32, 62, text);
        String[] parts = labels.split("\n", -1);
        if (ctrl != null) {
            for (int i = 0; i + 7 < ctrl.length; i += 8) {
                int role = ctrl[i + 5], state = ctrl[i + 6], li = ctrl[i + 7];
                String label = li >= 0 && li < parts.length ? parts[li] : "";
                control(canvas, fill, text, ctrl[i + 1], ctrl[i + 2], ctrl[i + 3], ctrl[i + 4], role, state, label, detail);
            }
        }
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        bitmap.recycle();
        return pixels;
    }

    private static void control(Canvas canvas, Paint fill, TextPaint text,
            int x, int y, int w, int h, int role, int state, String label, String detail) {
        boolean hover = (state & ST_HOVER) != 0, selected = (state & ST_SELECTED) != 0,
            armed = (state & ST_ARMED) != 0, pending = (state & ST_PENDING) != 0,
            disabled = (state & ST_DISABLED) != 0, on = (state & ST_ON) != 0,
            active = (state & ST_ACTIVE) != 0;
        String main = label, chip = null;
        int bar = label.indexOf('|');
        if (bar >= 0) { main = label.substring(0, bar); chip = label.substring(bar + 1); }
        switch (role) {
            case ROLE_SECTION: {
                text.setColor(MUT); text.setTextSize(17);
                String caps = main.toUpperCase(java.util.Locale.ROOT);
                canvas.drawText(caps, x, y + h - 5, text);
                float tw = text.measureText(caps);
                fill.setColor(EDGE);
                canvas.drawRect(x + tw + 14, y + h / 2, x + w, y + h / 2 + 1.5f, fill);
                return;
            }
            case ROLE_CONTEXT: {
                fill.setColor(INSET);
                canvas.drawRoundRect(x, y, x + w, y + h, 12, 12, fill);
                fill.setColor(EDGE); fill.setStyle(Paint.Style.STROKE); fill.setStrokeWidth(1.5f);
                canvas.drawRoundRect(x + 1, y + 1, x + w - 1, y + h - 1, 11, 11, fill);
                fill.setStyle(Paint.Style.FILL);
                String[] lines = detail == null ? new String[0] : detail.split("\n", -1);
                text.setTextSize(23); text.setColor(Color.WHITE);
                fit(text, lines.length > 0 ? lines[0] : "", w - 36, 23, 16);
                canvas.drawText(lines.length > 0 ? lines[0] : "", x + 18, y + 33, text);
                if (lines.length > 1 && !lines[1].isEmpty()) {
                    text.setColor(AMBER); text.setTextSize(20);
                    String fitted = fit(text, lines[1], w - 36, 20, 14);
                    canvas.drawText(fitted, x + 18, y + 62, text);
                }
                return;
            }
            case ROLE_INFO: {
                text.setColor(MUT); text.setTextSize(20);
                canvas.drawText(fit(text, main, w, 20, 14), x, y + h * .72f, text);
                return;
            }
            case ROLE_BODY: {
                text.setColor(SUB); text.setTextSize(26);
                text.setTypeface(Typeface.create("sans-serif", Typeface.NORMAL));
                String body = detail == null ? "" : detail;
                StaticLayout layout = StaticLayout.Builder.obtain(body, 0, body.length(), text, w)
                    .setIncludePad(false).setLineSpacing(3, 1).build();
                while (layout.getHeight() > h && text.getTextSize() > 17) {
                    text.setTextSize(text.getTextSize() - 1);
                    layout = StaticLayout.Builder.obtain(body, 0, body.length(), text, w)
                        .setIncludePad(false).setLineSpacing(3, 1).build();
                }
                canvas.save(); canvas.clipRect(x, y, x + w, y + h); canvas.translate(x, y);
                layout.draw(canvas); canvas.restore();
                text.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
                return;
            }
        }
        int bg = BTN, fg = Color.WHITE;
        switch (role) {
            case ROLE_TARGETED: bg = BTN2; break;
            case ROLE_IMMEDIATE: bg = IMM; break;
            case ROLE_DANGER: bg = DANGER_BG; fg = AMBER; break;
            case ROLE_TAB: bg = Color.rgb(26, 41, 51); fg = SUB; break;
            case ROLE_GROUPNUM: bg = BTN; break;
            case ROLE_CARD: bg = BTN; break;
            case ROLE_OP: bg = BTN; break;
            case ROLE_CLOSE: bg = BTN; fg = SUB; break;
            default: break;
        }
        if (armed || pending) bg = ARMED_BG;
        if (disabled) { bg = DISABLED_BG; fg = MUT; }
        fill.setColor(bg);
        canvas.drawRoundRect(x, y, x + w, y + h, 10, 10, fill);
        if (role == ROLE_TARGETED && !disabled) {
            fill.setColor(EDGE); fill.setStyle(Paint.Style.STROKE); fill.setStrokeWidth(1.5f);
            canvas.drawRoundRect(x + 1, y + 1, x + w - 1, y + h - 1, 9, 9, fill);
            fill.setStyle(Paint.Style.FILL);
        }
        if (role == ROLE_DANGER || selected || armed || pending) {
            fill.setColor(AMBER); fill.setStyle(Paint.Style.STROKE);
            fill.setStrokeWidth((armed || selected) ? 4 : 2.5f);
            canvas.drawRoundRect(x + 2, y + 2, x + w - 2, y + h - 2, 9, 9, fill);
            fill.setStyle(Paint.Style.FILL);
        }
        if (role == ROLE_TAB && active) {
            fg = Color.WHITE;
            fill.setColor(AMBER);
            canvas.drawRect(x + 14, y + h - 4, x + w - 14, y + h - 1, fill);
        }
        if (role == ROLE_GROUPNUM) {
            text.setColor(disabled ? MUT : Color.WHITE); text.setTextSize(22);
            String num = fit(text, main, w - 12, 22, 14);
            float nw = text.measureText(num);
            canvas.drawText(num, x + (w - nw) / 2, y + (chip == null ? h / 2 + 8 : 24), text);
            if (chip != null) {
                text.setTextSize(14); text.setColor(on ? AMBER : MUT);
                float cw = text.measureText(chip);
                canvas.drawText(chip, x + (w - cw) / 2, y + h - 9, text);
            }
        } else if (role == ROLE_CARD) {
            if (selected) { text.setColor(AMBER); text.setTextSize(24); canvas.drawText("✓", x + 14, y + h / 2 + 9, text); }
            text.setColor(fg); text.setTextSize(24);
            String fitted = fit(text, main, w - (selected ? 56 : 28) - (chip != null ? 90 : 0), 24, 15);
            canvas.drawText(fitted, x + (selected ? 46 : 14), y + h / 2 + 8, text);
            if (chip != null) { text.setColor(AMBER); text.setTextSize(19); canvas.drawText(chip, x + w - text.measureText(chip) - 14, y + h / 2 + 7, text); }
        } else {
            boolean chevron = role == ROLE_TARGETED;
            text.setColor(disabled ? MUT : fg); text.setTextSize(role == ROLE_CLOSE ? 26 : 23);
            float reserve = (chevron ? 34 : 0) + (chip != null ? 86 : 0) + 26;
            String fitted = fit(text, main, w - reserve, role == ROLE_CLOSE ? 26 : 23, 14);
            float ty = y + h / 2 + text.getTextSize() * .35f;
            if (role == ROLE_CLOSE) {
                float tw = text.measureText(fitted);
                canvas.drawText(fitted, x + (w - tw) / 2, ty, text);
            } else {
                canvas.drawText(fitted, x + 13, ty, text);
            }
            if (chevron) {
                text.setColor(disabled ? MUT : AMBER); text.setTextSize(24);
                canvas.drawText(armed ? "●" : "›", x + w - 30, y + h / 2 + 9, text);
            }
            if (chip != null) {
                text.setColor(on || pending ? AMBER : MUT); text.setTextSize(17);
                canvas.drawText(chip, x + w - text.measureText(chip) - 13, ty, text);
            }
        }
        if (hover) {
            fill.setColor(CYAN); fill.setStyle(Paint.Style.STROKE); fill.setStrokeWidth(3);
            canvas.drawRoundRect(x + 1.5f, y + 1.5f, x + w - 1.5f, y + h - 1.5f, 10, 10, fill);
            fill.setStyle(Paint.Style.FILL);
        }
    }

    // Shrink-to-fit instead of ellipsis: a command's only label may never be
    // silently truncated (PLAN-024 §8). Returns the label at a fitting size.
    private static String fit(TextPaint text, String label, float maxWidth, float startSize, float minSize) {
        float size = startSize;
        text.setTextSize(size);
        while (size > minSize && text.measureText(label) > maxWidth) {
            size -= 1;
            text.setTextSize(size);
        }
        return label;
    }
}
