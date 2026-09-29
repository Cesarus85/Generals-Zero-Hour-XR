// GeneralsX @feature Claude 29/09/2026 PLAN-027 LAN voice chat bridge. The engine
// only reports the other human players' LAN addresses; capture, voice
// activation, UDP transport and playback live in LanVoiceChat.java, outside
// lockstep. This header also owns the non-interactive indicator plate.
#pragma once
#include <string>
#include <vector>

// Status bits reported by LanVoiceChat.status().
constexpr int kXrVoiceActive=1,kXrVoiceLocalSpeaking=2,kXrVoicePeerSpeaking=4,kXrVoiceMicMissing=8;

static int micPermission(XrHello &x,bool request) {
	if(!x.panelEnv || !x.activityRef)return 0;
	auto *env=x.panelEnv;auto cls=env->GetObjectClass(x.activityRef);
	if(!cls){env->ExceptionClear();return 0;}
	const auto method=env->GetMethodID(cls,"micPermission","(Z)I");
	const int result=method ? env->CallIntMethod(x.activityRef,method,jboolean(request)):0;
	env->DeleteLocalRef(cls);
	if(env->ExceptionCheck()){env->ExceptionClear();return 0;}return result;
}

// About twice per second: publish peers/mode to Java and read the status bits.
static void updateVoiceChat(XrHello &x) {
	if(!x.gameBooted || !x.panelEnv || !x.voiceClass)return;
	if((x.voiceFrame++%36)!=0)return;
	auto *env=x.panelEnv;
	std::vector<std::string> peers;
	const bool inLan=XrGameBoot_LanVoicePeers(peers);
	const int mode=inLan ? x.layout.voiceChat:0;
	bool micAllowed=false;
	if(x.layout.voiceChat==1) {
		// Ask once right after the player switches voice chat on; afterwards only query.
		micAllowed=micPermission(x,x.voiceMicRequest)==1;
		x.voiceMicRequest=false;
	}
	const auto configure=env->GetStaticMethodID(x.voiceClass,"configure","(I[Ljava/lang/String;Z)V");
	const auto status=env->GetStaticMethodID(x.voiceClass,"status","()I");
	if(!configure || !status || env->ExceptionCheck()){env->ExceptionClear();return;}
	auto stringClass=env->FindClass("java/lang/String");
	if(!stringClass || env->ExceptionCheck()){env->ExceptionClear();return;}
	auto array=env->NewObjectArray(jsize(peers.size()),stringClass,nullptr);
	for(size_t i=0;array && i<peers.size();++i) {
		auto value=env->NewStringUTF(peers[i].c_str());
		env->SetObjectArrayElement(array,jsize(i),value);env->DeleteLocalRef(value);
	}
	env->CallStaticVoidMethod(x.voiceClass,configure,jint(mode),array,jboolean(micAllowed));
	if(array)env->DeleteLocalRef(array);
	env->DeleteLocalRef(stringClass);
	if(env->ExceptionCheck()){env->ExceptionDescribe();env->ExceptionClear();return;}
	const int previous=x.voiceStatus;
	x.voiceStatus=env->CallStaticIntMethod(x.voiceClass,status);
	if(env->ExceptionCheck()){env->ExceptionClear();x.voiceStatus=0;}
	if(previous!=x.voiceStatus)XR_LOG("voice status=%d peers=%zu mode=%d mic=%d",x.voiceStatus,peers.size(),mode,int(micAllowed));
}

// Below the UI/Commands/Ground View column; informational, not a button.
static XrSurface voiceIndicatorSurface(const XrHello &x) {
	auto s=uiButtonSurface(x);
	s.pose.position=xrAdd(s.pose.position,{0,-.48f,0});
	return s;
}
static bool voiceIndicatorVisible(const XrHello &x) {
	return (x.voiceStatus&kXrVoiceActive)!=0 && !x.menu.open && !x.arranging && !x.scene.placing &&
		!x.recoveryVisible && x.observer.mode==XrObserverMode::Off;
}
