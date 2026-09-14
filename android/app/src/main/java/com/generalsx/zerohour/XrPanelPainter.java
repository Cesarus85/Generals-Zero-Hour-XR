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

    public static int[] paint(String title, String detail, String labels, int hover, int kind) {
        int width = kind == 0 ? 192 : 768;
        int height = kind == 0 ? 128 : kind == 6 ? 768 : kind == 5 ? 1280 : (kind == 1 || kind == 3 || kind == 4 || kind == 7) ? 1024 : 384;
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
        fill.setColor(Color.rgb(14, 24, 32));
        canvas.drawRoundRect(2, 2, width - 2, height - 2, 28, 28, fill);
        fill.setColor(Color.rgb(50, 205, 192));
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
        } else if (kind == 4) {
            // GeneralsX @feature Codex 14/09/2026 In-place bilingual help.
            text.setTextSize(30); canvas.drawText(title, 32, 62, text);
            button(canvas, fill, text, "X", 664, 36, 72, 56, hover == 33, 26);
            text.setTextSize(26); text.setTypeface(Typeface.create("sans-serif", Typeface.NORMAL));
            StaticLayout help = StaticLayout.Builder.obtain(detail, 0, detail.length(), text, 704)
                .setIncludePad(false).setLineSpacing(3, 1).build();
            while (help.getHeight() > 820 && text.getTextSize() > 18) {
                text.setTextSize(text.getTextSize() - 1);
                help = StaticLayout.Builder.obtain(detail, 0, detail.length(), text, 704)
                    .setIncludePad(false).setLineSpacing(3, 1).build();
            }
            canvas.save(); canvas.clipRect(32, 100, 736, 920); canvas.translate(32, 100);
            help.draw(canvas); canvas.restore();
            String[] actions = labels.split("\n", -1);
            if (actions.length >= 2) {
                button(canvas, fill, text, actions[0], 32, 930, 344, 64, hover == 34, 24);
                button(canvas, fill, text, actions[1], 392, 930, 344, 64, hover == 36, 24);
            }
        } else if (kind == 3 || kind == 5) {
            // Geometry is mirrored by XrCommands.h; no scrolling/submenus.
            text.setTextSize(32); canvas.drawText(title, 32, 62, text);
            button(canvas, fill, text, "X", 664, 36, 72, 56, hover == 33, 26);
            text.setColor(Color.rgb(180, 200, 209)); text.setTextSize(19);
            StaticLayout status = StaticLayout.Builder.obtain(detail, 0, detail.length(), text, 704)
                .setIncludePad(false).setMaxLines(2).setEllipsize(TextUtils.TruncateAt.END).build();
            canvas.save(); canvas.translate(32, 88); status.draw(canvas); canvas.restore();
            String[] commands = labels.split("\n", -1);
            for (int i = 0; i < Math.min(16, commands.length); i++)
                button(canvas, fill, text, commands[i], i % 2 == 0 ? 32 : 392, 144 + i / 2 * 64, 344, 56, hover == i, 24);
            text.setColor(Color.rgb(180, 200, 209)); text.setTextSize(22);
            canvas.drawText(commands.length > 18 ? commands[18] : "", 32, 691, text);
            for (int i = 0; i < 10; i++)
                button(canvas, fill, text, commands.length > 22+i ? commands[22+i] : Integer.toString(i+1), 32 + i % 5 * 142, 704 + i / 5 * 64, 134, 56, hover == 20 + i, 26);
            for (int i = 0; i < 3; i++) button(canvas, fill, text, commands.length > 19+i ? commands[19+i] : "", 32 + i * 238, 850, 228, 60, hover == 30 + i, 23);
            if (commands.length > 33) {
                button(canvas, fill, text, commands[32], 32, 930, 344, 64, hover == 35, 24);
                button(canvas, fill, text, commands[33], 572, 930, 164, 64, hover == 34, 24);
                if (commands.length > 34) button(canvas, fill, text, commands[34], 392, 930, 164, 64, hover == 37, 24);
            }
            if (kind == 5 && commands.length > 43) {
                for (int i = 0; i < 4; i++)
                    button(canvas, fill, text, commands[35+i], i % 2 == 0 ? 32 : 392, 1024 + i / 2 * 64, 344, 56, hover == 40+i, 24);
                for (int i = 0; i < 4; i++)
                    button(canvas, fill, text, commands[39+i], 32+i*178, 1160, 168, 56, hover == 44+i, 26);
                text.setColor(Color.rgb(180, 200, 209)); text.setTextSize(22);
                canvas.drawText(commands[43], 32, 1250, text);
            }
        } else {
            canvas.drawText(TextUtils.ellipsize(title, text, 704, TextUtils.TruncateAt.END).toString(), 32, kind == 1 ? 49 : 82, text);
            text.setTypeface(Typeface.create("sans-serif", Typeface.NORMAL));
            text.setColor(Color.rgb(180, 200, 209));
            text.setTextSize(kind == 1 ? 19 : kind == 7 ? 24 : 28);
            StaticLayout description = StaticLayout.Builder.obtain(detail, 0, detail.length(), text, 704)
                .setAlignment(Layout.Alignment.ALIGN_NORMAL).setIncludePad(false)
                .setMaxLines(kind == 1 ? 2 : kind == 7 ? 6 : 8).setEllipsize(TextUtils.TruncateAt.END).build();
            canvas.save(); canvas.translate(32, kind == 1 ? 57 : 116); description.draw(canvas); canvas.restore();
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
            if (kind == 1) {
                // GeneralsX @feature Codex 14/09/2026 Direct controller guide.
                button(canvas, fill, text, "?", 664, 18, 72, 36, hover == 24, 26);
                String[] buttons = labels.split("\n", -1);
                for (int i = 0; i < 4; i++) {
                    int x = 32 + i * 178;
                    fill.setColor(hover == 20 + i ? Color.rgb(29, 100, 107) : Color.rgb(30, 65, 73));
                    canvas.drawRoundRect(x, 108, x + 168, 146, 8, 8, fill);
                    text.setColor(Color.WHITE); text.setTextSize(21);
                    canvas.drawText(buttons.length > 18+i ? buttons[18+i] : "", x + 12, 134, text);
                }
                for (int i = 0; i < Math.min(18, buttons.length); ++i) {
                    if (buttons[i].isEmpty()) continue;
                    float x = (i % 2 == 0) ? 32 : 392;
                    float y = 160 + (i / 2) * 90;
                    fill.setColor(i == hover ? Color.rgb(29, 100, 107) : Color.rgb(30, 46, 58));
                    canvas.drawRoundRect(x, y, x + 344, y + 72, 14, 14, fill);
                    // Selection is persistent, hover remains cyan. The check
                    // mark also identifies the target without color vision.
                    if (buttons[i].startsWith("✓ ")) {
                        fill.setColor(Color.rgb(255, 166, 31));
                        fill.setStyle(Paint.Style.STROKE); fill.setStrokeWidth(4);
                        canvas.drawRoundRect(x + 2, y + 2, x + 342, y + 70, 12, 12, fill);
                        fill.setStyle(Paint.Style.FILL);
                    }
                    text.setColor(Color.WHITE); text.setTextSize(25);
                    String label = TextUtils.ellipsize(buttons[i], text, 312, TextUtils.TruncateAt.END).toString();
                    canvas.drawText(label, x + 16, y + 45, text);
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
}
