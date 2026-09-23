// GeneralsX @feature Codex 13/09/2026 Smooth, local Canvas artwork for
// native OpenXR panels. No WebView, UI thread, permissions or game assets.
// GeneralsX @tweak Ultron 22/09/2026 Military command-console theme: cut
// corners, gunmetal/olive gradients, brass trim, hazard stripes and rivets
// replace the generic rounded dark-blue panels. Pure paint change: control
// geometry, hit regions and text fitting contracts are unchanged.
package com.generalsx.zerohour;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.text.Layout;
import android.text.StaticLayout;
import android.text.TextPaint;
import android.text.TextUtils;

public final class XrPanelPainter {
    private XrPanelPainter() {}

    // GeneralsX @tweak Ultron 22/09/2026 Zero Hour command-console palette:
    // gunmetal/olive plates, brass trim, stencil gold headers. Semantic state
    // colors stay distinct: gold = armed/selected, red = danger, muted =
    // disabled, bright gold = hover.
    private static final int BG_TOP = Color.rgb(44, 48, 36);
    private static final int BG_BOT = Color.rgb(20, 23, 15);
    private static final int FRAME = Color.rgb(6, 8, 5);
    private static final int TRIM = Color.rgb(201, 162, 39);
    private static final int TRIM_DIM = Color.rgb(105, 88, 38);
    private static final int EDGE = Color.rgb(74, 80, 58);
    private static final int INSET = Color.rgb(14, 16, 10);
    private static final int BTN_TOP = Color.rgb(70, 78, 56);
    private static final int BTN_BOT = Color.rgb(43, 49, 36);
    private static final int BTN2_TOP = Color.rgb(62, 72, 60);
    private static final int BTN2_BOT = Color.rgb(36, 43, 32);
    private static final int IMM_TOP = Color.rgb(84, 92, 66);
    private static final int IMM_BOT = Color.rgb(52, 58, 42);
    private static final int SUB = Color.rgb(185, 188, 164);
    private static final int MUT = Color.rgb(126, 132, 108);
    private static final int PAPER = Color.rgb(232, 230, 220);
    private static final int AMBER = Color.rgb(232, 178, 60);
    private static final int GOLD_HI = Color.rgb(255, 215, 90);
    private static final int HAZARD = Color.rgb(242, 194, 0);
    private static final int HAZARD_DARK = Color.rgb(18, 18, 14);
    private static final int ARMED_TOP = Color.rgb(92, 66, 20);
    private static final int ARMED_BOT = Color.rgb(58, 40, 12);
    private static final int DANGER_TOP = Color.rgb(96, 40, 28);
    private static final int DANGER_BOT = Color.rgb(56, 22, 15);
    private static final int DANGER_FG = Color.rgb(255, 177, 153);
    private static final int DISABLED_TOP = Color.rgb(34, 38, 28);
    private static final int DISABLED_BOT = Color.rgb(24, 27, 19);

    // Roles/states mirror GeneralsMD/Code/Main/XrPanelLayout.h exactly.
    private static final int ROLE_BUTTON = 0, ROLE_TARGETED = 1, ROLE_IMMEDIATE = 2, ROLE_CLOSE = 3,
        ROLE_SECTION = 4, ROLE_CONTEXT = 5, ROLE_GROUPNUM = 6, ROLE_TAB = 7, ROLE_INFO = 8,
        ROLE_BODY = 9, ROLE_CARD = 10, ROLE_OP = 11, ROLE_DANGER = 12;
    private static final int ST_HOVER = 1, ST_SELECTED = 2, ST_ARMED = 4, ST_PENDING = 8,
        ST_DISABLED = 16, ST_ON = 32, ST_ACTIVE = 64;

    // Cut-corner silhouette shared by panels, plates and chips.
    private static Path chamfer(float x, float y, float w, float h, float cut) {
        Path p = new Path();
        p.moveTo(x + cut, y);
        p.lineTo(x + w - cut, y);
        p.lineTo(x + w, y + cut);
        p.lineTo(x + w, y + h - cut);
        p.lineTo(x + w - cut, y + h);
        p.lineTo(x + cut, y + h);
        p.lineTo(x, y + h - cut);
        p.lineTo(x, y + cut);
        p.close();
        return p;
    }

    // Vertical two-tone metal fill inside a cut-corner path.
    private static void plate(Canvas canvas, float x, float y, float w, float h, float cut,
            int top, int bottom) {
        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        p.setShader(new LinearGradient(x, y, x, y + h, top, bottom, Shader.TileMode.CLAMP));
        canvas.drawPath(chamfer(x, y, w, h, cut), p);
    }

    private static void outline(Canvas canvas, float x, float y, float w, float h, float cut,
            int color, float width) {
        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(width);
        p.setColor(color);
        canvas.drawPath(chamfer(x, y, w, h, cut), p);
    }

    // Top-left light / bottom-right shade bevel that sells the metal plate.
    private static void bevel(Canvas canvas, float x, float y, float w, float h, float cut) {
        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        p.setStrokeWidth(1.5f);
        p.setColor(Color.argb(70, 255, 255, 230));
        canvas.drawLine(x + cut, y + 1, x + w - cut, y + 1, p);
        canvas.drawLine(x + 1, y + cut, x + 1, y + h - cut, p);
        p.setColor(Color.argb(90, 0, 0, 0));
        canvas.drawLine(x + cut, y + h - 1, x + w - cut, y + h - 1, p);
        canvas.drawLine(x + w - 1, y + cut, x + w - 1, y + h - cut, p);
    }

    // Corner bolts of the command-console frame.
    private static void rivet(Canvas canvas, float cx, float cy) {
        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        p.setColor(Color.rgb(8, 10, 6));
        canvas.drawCircle(cx, cy, 7, p);
        p.setColor(Color.rgb(88, 94, 70));
        canvas.drawCircle(cx, cy, 4.5f, p);
        p.setColor(Color.argb(120, 255, 255, 235));
        canvas.drawCircle(cx - 1.3f, cy - 1.5f, 1.6f, p);
    }

    // 45-degree hazard band, clipped to the given rect.
    private static void hazard(Canvas canvas, float x, float y, float w, float h) {
        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        p.setColor(HAZARD_DARK);
        canvas.drawRect(x, y, x + w, y + h, p);
        p.setColor(HAZARD);
        canvas.save();
        canvas.clipRect(x, y, x + w, y + h);
        final float step = h * 2;
        for (float sx = x - h; sx < x + w; sx += step) {
            Path stripe = new Path();
            stripe.moveTo(sx, y + h);
            stripe.lineTo(sx + h, y);
            stripe.lineTo(sx + h + h, y);
            stripe.lineTo(sx + h, y + h);
            stripe.close();
            canvas.drawPath(stripe, p);
        }
        canvas.restore();
    }

    // Shared command-console chrome: gradient hull, brass double frame,
    // rivets and a faint scanline raster for the HUD feel. Controls and text
    // are drawn on top by the caller.
    private static void consoleChrome(Canvas canvas, int width, int height, float cut) {
        canvas.drawColor(FRAME);
        plate(canvas, 4, 4, width - 8, height - 8, cut, BG_TOP, BG_BOT);
        outline(canvas, 4, 4, width - 8, height - 8, cut, TRIM_DIM, 5);
        outline(canvas, 9, 9, width - 18, height - 18, Math.max(4, cut - 5), TRIM, 2);
        Paint scan = new Paint();
        scan.setColor(Color.argb(9, 0, 0, 0));
        for (int sy = 14; sy < height - 12; sy += 4)
            canvas.drawRect(10, sy, width - 10, sy + 1.4f, scan);
        rivet(canvas, cut * .75f + 6, cut * .75f + 6);
        rivet(canvas, width - cut * .75f - 6, cut * .75f + 6);
        rivet(canvas, cut * .75f + 6, height - cut * .75f - 6);
        rivet(canvas, width - cut * .75f - 6, height - cut * .75f - 6);
    }

    // Stencil header text: heavy face, tracked out, gold. Resets tracking so
    // the shared TextPaint stays body-clean.
    private static void stencil(TextPaint text, float size) {
        text.setTypeface(Typeface.create("sans-serif-black", Typeface.NORMAL));
        text.setTextSize(size);
        text.setLetterSpacing(0.09f);
    }

    private static void body(TextPaint text) {
        text.setTypeface(Typeface.create("sans-serif", Typeface.NORMAL));
        text.setLetterSpacing(0);
    }

    public static int[] paint(String title, String detail, String labels, int hover, int kind) {
        int width = kind == 0 ? 192 : 768;
        int height = kind == 0 ? 128 : kind == 6 ? 768 : kind == 5 ? 1280 : (kind == 1 || kind == 3 || kind == 4 || kind == 7) ? 1024 : 384;
        // GeneralsX @feature Muse 16/09/2026 Match-result card: kind 2 with a
        // positive hover selects the accent (1 victory gold, 2 defeat red,
        // 3 neutral). Existing kind-2 callers pass -1 and stay pixel-identical.
        boolean result = kind == 2 && hover > 0;
        int accent = !result ? TRIM : hover == 1 ? AMBER : hover == 2 ? Color.rgb(255, 92, 92) : TRIM;
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        TextPaint text = new TextPaint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
        if (kind == 0) {
            // Compact room-space shortcut: one chamfered metal plate, brass
            // frame, centered stencil label. Hit surface unchanged.
            consoleChrome(canvas, width, height, 22);
            stencil(text, 30);
            text.setColor(GOLD_HI);
            text.setTextAlign(Paint.Align.CENTER);
            text.setTextSize(title.length() > 2 ? 24 : 40);
            // Keep localized labels such as BODENANSICHT fully visible on the
            // compact, room-space button without changing the hit surface.
            float measured = text.measureText(title.toUpperCase(java.util.Locale.ROOT));
            if (measured > width - 20) text.setTextSize(text.getTextSize() * (width - 20) / measured);
            canvas.drawText(title.toUpperCase(java.util.Locale.ROOT), width / 2f, 86, text);
            text.setTextAlign(Paint.Align.LEFT);
        } else {
            consoleChrome(canvas, width, height, 30);
            // Header band: hazard ticks flank a brass rule under the title.
            Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
            stencil(text, 36);
            text.setColor(GOLD_HI);
            if (kind == 6) {
                // GeneralsX @feature Codex 14/09/2026 Full native descriptions:
                // fixed legible type, line-aligned pages, no ellipsis or lost tail.
                text.setTextSize(30);
                StaticLayout heading = StaticLayout.Builder.obtain(title, 0, title.length(), text, 560)
                    .setIncludePad(false).build();
                canvas.save(); canvas.translate(36, 48); heading.draw(canvas); canvas.restore();
                hazard(canvas, 616, 52, 116, 16);
                fill.setColor(TRIM);
                canvas.drawRect(36, 96, 732, 98.5f, fill);
                int top = Math.max(118, 72 + heading.getHeight());
                body(text);
                text.setTextSize(26);
                text.setColor(Color.rgb(216, 222, 200));
                StaticLayout bodyLayout = StaticLayout.Builder.obtain(detail, 0, detail.length(), text, 696)
                    .setIncludePad(false).setLineSpacing(3, 1).build();
                java.util.ArrayList<Integer> starts = new java.util.ArrayList<>();
                starts.add(0); int pageStart = 0;
                for (int line = 0; line < bodyLayout.getLineCount(); ++line) {
                    if (bodyLayout.getLineBottom(line) - bodyLayout.getLineTop(pageStart) > 704 - top && line > pageStart) {
                        starts.add(line); pageStart = line;
                    }
                }
                int page = Math.floorMod(Math.max(0, hover), starts.size());
                int first = starts.get(page);
                int end = page + 1 < starts.size() ? bodyLayout.getLineTop(starts.get(page + 1)) : bodyLayout.getHeight();
                canvas.save(); canvas.clipRect(36, top, 732, top + end - bodyLayout.getLineTop(first));
                canvas.translate(36, top - bodyLayout.getLineTop(first)); bodyLayout.draw(canvas); canvas.restore();
                if (starts.size() > 1) {
                    stencil(text, 22);
                    text.setColor(AMBER);
                    canvas.drawText((page + 1) + " / " + starts.size() + "  ·  10 s", 36, 743, text);
                }
            } else {
                // GeneralsX @refactor Ultron 15/09/2026 P21 kinds 1/3/4/5 moved to
                // paint2 (shared native control table). Only 0/2/6/7 remain here.
                if (result) {
                    stencil(text, 64);
                    text.setColor(hover == 1 ? AMBER : hover == 2 ? Color.rgb(255, 130, 130) : GOLD_HI);
                    text.setTextAlign(Paint.Align.CENTER);
                    canvas.drawText(TextUtils.ellipsize(title.toUpperCase(java.util.Locale.ROOT), text, 660, TextUtils.TruncateAt.END).toString(), width / 2f, 100, text);
                    text.setTextAlign(Paint.Align.LEFT);
                } else {
                    canvas.drawText(TextUtils.ellipsize(title.toUpperCase(java.util.Locale.ROOT), text, 560, TextUtils.TruncateAt.END).toString(), 36, 82, text);
                }
                hazard(canvas, 616, kind == 7 ? 40 : 58, 116, 16);
                fill.setColor(TRIM);
                canvas.drawRect(36, 96, 732, 98.5f, fill);
                body(text);
                text.setColor(SUB);
                text.setTextSize(kind == 7 ? 24 : 28);
                StaticLayout description = StaticLayout.Builder.obtain(detail, 0, detail.length(), text, 696)
                    .setAlignment(Layout.Alignment.ALIGN_NORMAL).setIncludePad(false)
                    .setMaxLines(kind == 7 ? 6 : 8).setEllipsize(TextUtils.TruncateAt.END).build();
                canvas.save(); canvas.translate(36, 116); description.draw(canvas); canvas.restore();
                if (kind == 7) {
                    // GeneralsX @feature Codex 14/09/2026 One setup step per page.
                    // Empty actions are absent, instructions live above the buttons.
                    String[] buttons = labels.split("\n", -1);
                    for (int i = 0; i < Math.min(6, buttons.length); ++i)
                        if (!buttons[i].isEmpty())
                            button(canvas, text, buttons[i], 32, 300+i*82, 704, 68, hover == i, 27);
                    if (buttons.length > 17) {
                        button(canvas, text, buttons[16], 32, 900, 344, 72, hover == 16, 25);
                        button(canvas, text, buttons[17], 392, 900, 344, 72, hover == 17, 25);
                    }
                }
            }
        }
        int[] pixels = new int[width * height];
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height);
        bitmap.recycle();
        return pixels;
    }

    private static void button(Canvas canvas, TextPaint text, String label,
            float x, float y, float w, float h, boolean hover, float size) {
        boolean disabled = label.startsWith("! ");
        if (disabled) label = label.substring(2);
        plate(canvas, x, y, w, h, 10,
            disabled ? DISABLED_TOP : hover ? IMM_TOP : BTN_TOP,
            disabled ? DISABLED_BOT : hover ? IMM_BOT : BTN_BOT);
        bevel(canvas, x, y, w, h, 10);
        outline(canvas, x + .75f, y + .75f, w - 1.5f, h - 1.5f, 9, disabled ? EDGE : TRIM_DIM, 1.5f);
        if (hover) outline(canvas, x + 1.5f, y + 1.5f, w - 3, h - 3, 9, GOLD_HI, 3);
        body(text);
        text.setColor(disabled ? MUT : PAPER); text.setTextSize(size);
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
        consoleChrome(canvas, width, height, 30);
        Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        // Header: stencil gold title, hazard ticks, brass rule.
        TextPaint text = new TextPaint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
        stencil(text, 34);
        text.setColor(GOLD_HI);
        String heading = title.toUpperCase(java.util.Locale.ROOT);
        canvas.drawText(TextUtils.ellipsize(heading, text, 520, TextUtils.TruncateAt.END).toString(), 36, 60, text);
        hazard(canvas, 616, 42, 116, 16);
        fill.setColor(TRIM);
        canvas.drawRect(36, 74, 732, 76.5f, fill);
        body(text);
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
                stencil(text, 17);
                text.setColor(TRIM);
                String caps = main.toUpperCase(java.util.Locale.ROOT);
                canvas.drawText(caps, x, y + h - 5, text);
                float tw = text.measureText(caps);
                body(text);
                // Brass chevron + double rule carry the stencil header across.
                fill.setColor(TRIM);
                Path tick = new Path();
                tick.moveTo(x + tw + 16, y + h / 2 - 5);
                tick.lineTo(x + tw + 26, y + h / 2 + .75f);
                tick.lineTo(x + tw + 16, y + h / 2 + 6.5f);
                tick.close();
                canvas.drawPath(tick, fill);
                canvas.drawRect(x + tw + 34, y + h / 2, x + w, y + h / 2 + 1.5f, fill);
                fill.setColor(TRIM_DIM);
                canvas.drawRect(x + tw + 34, y + h / 2 + 4, x + w, y + h / 2 + 5, fill);
                return;
            }
            case ROLE_CONTEXT: {
                plate(canvas, x, y, w, h, 12, Color.rgb(24, 27, 17), INSET);
                outline(canvas, x + .75f, y + .75f, w - 1.5f, h - 1.5f, 11, TRIM_DIM, 1.5f);
                fill.setColor(TRIM);
                canvas.drawRect(x + 6, y + 8, x + 9.5f, y + h - 8, fill);
                String[] lines = detail == null ? new String[0] : detail.split("\n", -1);
                text.setTextSize(23); text.setColor(PAPER);
                fit(text, lines.length > 0 ? lines[0] : "", w - 44, 23, 16);
                canvas.drawText(lines.length > 0 ? lines[0] : "", x + 24, y + 33, text);
                if (lines.length > 1 && !lines[1].isEmpty()) {
                    text.setColor(AMBER); text.setTextSize(20);
                    String fitted = fit(text, lines[1], w - 44, 20, 14);
                    canvas.drawText(fitted, x + 24, y + 62, text);
                }
                return;
            }
            case ROLE_INFO: {
                text.setColor(MUT); text.setTextSize(20);
                canvas.drawText(fit(text, main, w, 20, 14), x, y + h * .72f, text);
                return;
            }
            case ROLE_BODY: {
                body(text);
                text.setColor(SUB); text.setTextSize(26);
                String bodyText = detail == null ? "" : detail;
                StaticLayout layout = StaticLayout.Builder.obtain(bodyText, 0, bodyText.length(), text, w)
                    .setIncludePad(false).setLineSpacing(3, 1).build();
                while (layout.getHeight() > h && text.getTextSize() > 17) {
                    text.setTextSize(text.getTextSize() - 1);
                    layout = StaticLayout.Builder.obtain(bodyText, 0, bodyText.length(), text, w)
                        .setIncludePad(false).setLineSpacing(3, 1).build();
                }
                canvas.save(); canvas.clipRect(x, y, x + w, y + h); canvas.translate(x, y);
                layout.draw(canvas); canvas.restore();
                return;
            }
        }
        int top = BTN_TOP, bot = BTN_BOT, fg = PAPER;
        switch (role) {
            case ROLE_TARGETED: top = BTN2_TOP; bot = BTN2_BOT; break;
            case ROLE_IMMEDIATE: top = IMM_TOP; bot = IMM_BOT; break;
            case ROLE_DANGER: top = DANGER_TOP; bot = DANGER_BOT; fg = DANGER_FG; break;
            case ROLE_TAB: top = Color.rgb(34, 39, 27); bot = Color.rgb(24, 28, 19); fg = SUB; break;
            case ROLE_CLOSE: fg = SUB; break;
            default: break;
        }
        if (armed || pending) { top = ARMED_TOP; bot = ARMED_BOT; }
        if (disabled) { top = DISABLED_TOP; bot = DISABLED_BOT; fg = MUT; }
        final float cut = role == ROLE_GROUPNUM ? 11 : role == ROLE_CARD ? 13 : 10;
        // Buttons always start from the same medium face and zero tracking;
        // stencil headers above mutate the shared paint.
        text.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        text.setLetterSpacing(0);
        plate(canvas, x, y, w, h, cut, top, bot);
        bevel(canvas, x, y, w, h, cut);
        outline(canvas, x + .75f, y + .75f, w - 1.5f, h - 1.5f, cut - 1,
            role == ROLE_DANGER ? Color.rgb(150, 58, 42) : disabled ? EDGE : TRIM_DIM, 1.5f);
        if (role == ROLE_DANGER || selected || armed || pending) {
            outline(canvas, x + 2.5f, y + 2.5f, w - 5, h - 5, cut - 2,
                role == ROLE_DANGER ? DANGER_FG : AMBER, (armed || selected) ? 3.5f : 2.5f);
        }
        if (role == ROLE_TAB && active) {
            fg = PAPER;
            fill.setColor(HAZARD);
            canvas.drawRect(x + 16, y + h - 6, x + w - 16, y + h - 2, fill);
            fill.setColor(TRIM_DIM);
            canvas.drawRect(x + 16, y + h - 2, x + w - 16, y + h - 1, fill);
        }
        if (role == ROLE_GROUPNUM) {
            stencil(text, 22);
            text.setColor(disabled ? MUT : PAPER);
            String num = fit(text, main, w - 12, 22, 14);
            float nw = text.measureText(num);
            canvas.drawText(num, x + (w - nw) / 2, y + (chip == null ? h / 2 + 8 : 26), text);
            body(text);
            if (chip != null) {
                text.setTextSize(14); text.setColor(on ? AMBER : MUT);
                float cw = text.measureText(chip);
                canvas.drawText(chip, x + (w - cw) / 2, y + h - 9, text);
            }
        } else if (role == ROLE_CARD) {
            if (selected) {
                plate(canvas, x + 10, y + h / 2 - 13, 26, 26, 5, ARMED_TOP, ARMED_BOT);
                outline(canvas, x + 10, y + h / 2 - 13, 26, 26, 5, AMBER, 1.5f);
                text.setColor(AMBER); text.setTextSize(22);
                canvas.drawText("✓", x + 15, y + h / 2 + 8, text);
            }
            text.setColor(fg); text.setTextSize(24);
            String fitted = fit(text, main, w - (selected ? 62 : 28) - (chip != null ? 90 : 0), 24, 15);
            canvas.drawText(fitted, x + (selected ? 48 : 14), y + h / 2 + 8, text);
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
            outline(canvas, x + 1.5f, y + 1.5f, w - 3, h - 3, cut - 1, GOLD_HI, 3);
            fill.setColor(Color.argb(14, 255, 215, 90));
            canvas.drawPath(chamfer(x + 2, y + 2, w - 4, h - 4, Math.max(2, cut - 2)), fill);
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
