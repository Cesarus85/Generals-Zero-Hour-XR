// GeneralsX @test Muse 16/09/2026 Match-result latch: win, loss, observer,
// quick end, campaign, dismiss, reset and card pose. Pure production header,
// no engine stubs needed.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "XrEndgame.h"
static int checks = 0;
static void check(bool ok) {
	++checks;
	if (!ok) { fprintf(stderr, "endgame check %d failed\n", checks); exit(1); }
}
static XrEndgameInput skirmish(unsigned frame) {
	XrEndgameInput in;
	in.interactive = true;
	in.frame = frame;
	in.vcValid = true;
	return in;
}
static XrEndgameInput campaign(unsigned frame, bool ending, bool victorious) {
	XrEndgameInput in;
	in.interactive = true;
	in.frame = frame;
	in.ending = ending;
	in.endActionValid = true;
	in.victorious = victorious;
	return in;
}
int main() {
	// Fresh state shows nothing and dismiss is a harmless no-op.
	XrEndgameState s;
	check(!xrEndgameVisible(s));
	xrEndgameDismiss(s);
	check(!xrEndgameVisible(s) && s.latch == XrEndgameResult::None);
	// A quiet mid-match never latches.
	for (unsigned f = 10; f < 20; ++f) xrEndgamePoll(s, skirmish(f));
	check(!xrEndgameVisible(s));
	// Skirmish win latches and survives the direct transition to statistics
	// (falling edge) with no timeout over many score-screen frames.
	XrEndgameInput win = skirmish(9000);
	win.localVictory = true;
	xrEndgamePoll(s, win);
	check(s.latch == XrEndgameResult::Victory && s.latchFrame == 9000);
	check(xrEndgameVisible(s));
	xrEndgamePoll(s, win); // re-poll keeps the original latch frame
	check(s.latchFrame == 9000 && xrEndgameVisible(s));
	XrEndgameInput score;
	score.frame = 9010; // non-interactive statistics screen
	for (unsigned f = 0; f < 50; ++f) { score.frame = 9010 + f; xrEndgamePoll(s, score); }
	check(s.latch == XrEndgameResult::Victory && xrEndgameVisible(s));
	// Dismiss hides until a new match; score-screen polls cannot reshow it.
	xrEndgameDismiss(s);
	check(!xrEndgameVisible(s) && s.latch == XrEndgameResult::Victory);
	xrEndgamePoll(s, score);
	check(!xrEndgameVisible(s) && s.latch == XrEndgameResult::Victory);
	// A mid-match dismiss also sticks while the same defeat input persists.
	XrEndgameState m;
	for (unsigned f = 10; f < 60; ++f) xrEndgamePoll(m, skirmish(f));
	XrEndgameInput mdef = skirmish(60);
	mdef.localDefeat = true;
	xrEndgamePoll(m, mdef);
	check(xrEndgameVisible(m));
	xrEndgameDismiss(m);
	xrEndgamePoll(m, mdef);
	xrEndgamePoll(m, mdef);
	check(!xrEndgameVisible(m) && m.latch == XrEndgameResult::Defeat);
	// A new match (rising edge) clears latch and dismissal, and can latch anew.
	XrEndgameInput fresh = skirmish(12);
	xrEndgamePoll(s, fresh);
	check(s.latch == XrEndgameResult::None && !s.dismissed);
	xrEndgamePoll(s, win);
	check(xrEndgameVisible(s));
	// Elimination latches DEFEAT immediately while the match continues, and
	// the first result wins even when victorious allies are marked later.
	XrEndgameState d;
	for (unsigned f = 10; f < 100; ++f) xrEndgamePoll(d, skirmish(f));
	XrEndgameInput elim = skirmish(100);
	elim.localDefeat = true;
	xrEndgamePoll(d, elim);
	check(d.latch == XrEndgameResult::Defeat && d.latchFrame == 100);
	XrEndgameInput allyWin = skirmish(4000);
	allyWin.localDefeat = true;
	allyWin.localVictory = true; // defeated allies count as victorious
	xrEndgamePoll(d, allyWin);
	check(d.latch == XrEndgameResult::Defeat && d.latchFrame == 100);
	// Pure alliance defeat at match end latches DEFEAT as well.
	XrEndgameState e;
	for (unsigned f = 10; f < 50; ++f) xrEndgamePoll(e, skirmish(f));
	XrEndgameInput lost = skirmish(7000);
	lost.alliedDefeat = true;
	xrEndgamePoll(e, lost);
	check(e.latch == XrEndgameResult::Defeat && xrEndgameVisible(e));
	// Observers get the neutral card only once the match is over, never a
	// personal victory or defeat.
	XrEndgameState o;
	for (unsigned f = 10; f < 50; ++f) {
		XrEndgameInput mid = skirmish(f);
		mid.observer = true;
		mid.localVictory = true; // ignored for observers
		xrEndgamePoll(o, mid);
	}
	check(o.latch == XrEndgameResult::None);
	XrEndgameInput over = skirmish(8000);
	over.observer = true;
	over.alliedDefeat = true; // single alliance remains
	over.localDefeat = true; // ignored for observers
	xrEndgamePoll(o, over);
	check(o.latch == XrEndgameResult::MatchOver && xrEndgameVisible(o));
	// Quick end: a single interactive frame latches and persists to score.
	XrEndgameState q;
	for (unsigned f = 10; f < 60; ++f) xrEndgamePoll(q, campaign(f, false, false));
	xrEndgamePoll(q, campaign(60, true, true));
	check(q.latch == XrEndgameResult::Victory);
	XrEndgameInput qscore;
	xrEndgamePoll(q, qscore);
	check(q.latch == XrEndgameResult::Victory && xrEndgameVisible(q));
	// A quick end action can also run in a multiplayer-mode Skirmish before
	// VictoryConditions marks a winning alliance.
	XrEndgameState quickSkirmish;
	xrEndgamePoll(quickSkirmish, skirmish(59));
	XrEndgameInput quickMp = skirmish(60);
	quickMp.ending = true;
	quickMp.endActionValid = true;
	quickMp.victorious = true;
	xrEndgamePoll(quickSkirmish, quickMp);
	check(quickSkirmish.latch == XrEndgameResult::Victory);
	// Campaign defeat needs the visible end timer; the flag alone is nothing.
	XrEndgameState c;
	for (unsigned f = 10; f < 60; ++f) xrEndgamePoll(c, campaign(f, false, true));
	check(c.latch == XrEndgameResult::None);
	xrEndgamePoll(c, campaign(60, true, false));
	check(c.latch == XrEndgameResult::Defeat && xrEndgameVisible(c));
	// Quitting mid-mission with a stale victory flag must never show a card:
	// no post-hoc recovery on the falling edge.
	XrEndgameState stale;
	for (unsigned f = 10; f < 60; ++f) xrEndgamePoll(stale, campaign(f, false, true));
	XrEndgameInput quit;
	xrEndgamePoll(stale, quit);
	check(stale.latch == XrEndgameResult::None && !xrEndgameVisible(stale));
	// Loading a save (frame counter reset while interactive) clears the latch.
	XrEndgameState l;
	for (unsigned f = 10; f < 60; ++f) xrEndgamePoll(l, skirmish(f));
	XrEndgameInput lw = skirmish(9000);
	lw.localVictory = true;
	xrEndgamePoll(l, lw);
	check(l.latch == XrEndgameResult::Victory);
	xrEndgamePoll(l, skirmish(5000)); // save from before the end
	check(l.latch == XrEndgameResult::None);
	// Shell polls without result sources stay quiet.
	XrEndgameState shell;
	for (unsigned f = 0; f < 10; ++f) xrEndgamePoll(shell, XrEndgameInput{});
	check(shell.latch == XrEndgameResult::None && !shell.wasInteractive);
	// Card pose: centered ahead of the head, gravity upright, tunable.
	XrEndgameCardPose p = xrEndgameCardPose(0, 1.6f, 0, 0, -1);
	check(fabsf(p.x) < 1e-6f && fabsf(p.y - 1.55f) < 1e-6f && fabsf(p.z + 1.2f) < 1e-6f);
	check(fabsf(p.yaw) < 1e-6f && fabsf(p.width - 1.0f) < 1e-6f);
	XrEndgameCardPose side = xrEndgameCardPose(1, 1.6f, 2, 1, 0);
	check(fabsf(side.x - 2.2f) < 1e-5f && fabsf(side.z - 2.0f) < 1e-6f);
	check(fabsf(side.yaw + 1.57079633f) < 1e-5f);
	XrEndgameCardPose custom = xrEndgameCardPose(0, 0, 0, 0, -1, 2.0f, 0.3f, 0.8f);
	check(fabsf(custom.z + 2.0f) < 1e-6f && fabsf(custom.y + 0.3f) < 1e-6f);
	check(fabsf(custom.width - 0.8f) < 1e-6f);
	printf("PASS %d match-result latch, dismiss, reset and card-pose checks\n", checks);
	return 0;
}
