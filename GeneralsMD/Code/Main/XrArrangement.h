// GeneralsX @bugfix Codex 14/09/2026 Shared exit for every workspace route.
#pragma once
static void finishArrangement(XrHello &x) {
	x.arranging=false;x.menu.open=false;x.grab.cancel();x.controlsArmed=false;
	x.menu.click.cancel();x.commands.input.click.cancel();saveLayout(x);
}
