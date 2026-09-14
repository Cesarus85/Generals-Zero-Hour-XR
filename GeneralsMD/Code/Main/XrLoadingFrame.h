// GeneralsX @bugfix Codex 14/09/2026 A synchronous loader can borrow the
// presentation thread, never the simulation. Balance the interrupted outer
// frame once, then each loading frame, even when drawing fails.
#pragma once
struct XrLoadingFrame {
	bool consumed=false,failed=false,busy=false,skipArmed=false;
	bool skip(bool focused,bool pressed) {
		if(!focused) {skipArmed=false;return false;}
		const bool fire=skipArmed && pressed;skipArmed=!pressed;return fire;
	}
	template<class Close,class Ready,class Begin,class Draw,class End>
	bool pump(Close closeOuter,Ready ready,Begin begin,Draw draw,End end) {
		if(failed)return false;
		if(busy)return true;
		busy=true;struct Guard {bool &busy;~Guard(){busy=false;}} guard{busy};
		if(!consumed) {consumed=true;if(!closeOuter()) {failed=true;return false;}}
		if(!ready())return true;
		if(!begin()) {failed=true;return false;}
		const bool drawn=draw();
		const bool ended=end(); // A successful begin always has an end attempt.
		failed=!drawn || !ended;return !failed;
	}
};
