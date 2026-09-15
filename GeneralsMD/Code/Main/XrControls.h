// GeneralsX @feature Codex 14/09/2026 Bind both hands once; swap logical roles live.
#pragma once
#include "XrHandedness.h"
#include "XrTracking.h"
struct XrControls {
	XrActionSet set=XR_NULL_HANDLE;
	XrAction aim[2]={},trigger[2]={},grip[2]={},gripPose[2]={};
	XrAction lower[2]={},upper[2]={},stick[2]={},stickClick[2]={},menu=XR_NULL_HANDLE;
	XrSpace aimSpace[2]={},gripSpace[2]={};
};
static bool initControls(XrControls &c,XrInstance instance,XrSession session) {
	XrActionSetCreateInfo setInfo={XR_TYPE_ACTION_SET_CREATE_INFO};
	// GeneralsX @feature Codex 14/09/2026 Runtime-visible product identity.
	strcpy(setInfo.actionSetName,"generals_controls");strcpy(setInfo.localizedActionSetName,"Generals: Zero Hour XR");
	XR_CHECK(xrCreateActionSet(instance,&setInfo,&c.set),"xrCreateActionSet");
	std::vector<XrActionSuggestedBinding> bindings;
	auto action=[&](XrAction &handle,const std::string &name,const std::string &path,XrActionType type) {
		XrActionCreateInfo info={XR_TYPE_ACTION_CREATE_INFO};info.actionType=type;
		strcpy(info.actionName,name.c_str());strcpy(info.localizedActionName,name.c_str());
		if(XR_FAILED(xrCreateAction(c.set,&info,&handle)))return false;
		XrPath binding=XR_NULL_PATH;
		if(XR_FAILED(xrStringToPath(instance,path.c_str(),&binding)))return false;
		bindings.push_back({handle,binding});return true;
	};
	for(int i=0;i<2;++i) {
		const std::string name=i==0 ? "left_":"right_",path=i==0 ? "/user/hand/left/input/":"/user/hand/right/input/";
		if(!action(c.aim[i],name+"aim",path+"aim/pose",XR_ACTION_TYPE_POSE_INPUT) ||
			!action(c.gripPose[i],name+"pose",path+"grip/pose",XR_ACTION_TYPE_POSE_INPUT) ||
			!action(c.trigger[i],name+"trigger",path+"trigger/value",XR_ACTION_TYPE_FLOAT_INPUT) ||
			!action(c.grip[i],name+"grip",path+"squeeze/value",XR_ACTION_TYPE_FLOAT_INPUT) ||
			!action(c.stick[i],name+"stick",path+"thumbstick",XR_ACTION_TYPE_VECTOR2F_INPUT) ||
			!action(c.stickClick[i],name+"stick_click",path+"thumbstick/click",XR_ACTION_TYPE_BOOLEAN_INPUT) ||
			!action(c.lower[i],name+"lower",path+(i==0 ? "x/click":"a/click"),XR_ACTION_TYPE_BOOLEAN_INPUT) ||
			!action(c.upper[i],name+"upper",path+(i==0 ? "y/click":"b/click"),XR_ACTION_TYPE_BOOLEAN_INPUT))return false;
	}
	if(!action(c.menu,"system_back","/user/hand/left/input/menu/click",XR_ACTION_TYPE_BOOLEAN_INPUT))return false;
	XrPath profile=XR_NULL_PATH;
	XR_CHECK(xrStringToPath(instance,"/interaction_profiles/oculus/touch_controller",&profile),"Touch profile");
	XrInteractionProfileSuggestedBinding suggestion={XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
	suggestion.interactionProfile=profile;suggestion.countSuggestedBindings=(uint32_t)bindings.size();suggestion.suggestedBindings=bindings.data();
	XR_CHECK(xrSuggestInteractionProfileBindings(instance,&suggestion),"Touch bindings");
	XrActionSpaceCreateInfo space={XR_TYPE_ACTION_SPACE_CREATE_INFO};space.poseInActionSpace.orientation.w=1;
	for(int i=0;i<2;++i) {
		space.action=c.aim[i];XR_CHECK(xrCreateActionSpace(session,&space,&c.aimSpace[i]),"controller aim space");
		space.action=c.gripPose[i];XR_CHECK(xrCreateActionSpace(session,&space,&c.gripSpace[i]),"controller grip space");
	}
	XrSessionActionSetsAttachInfo attach={XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attach.countActionSets=1;attach.actionSets=&c.set;
	XR_CHECK(xrAttachSessionActionSets(session,&attach),"attach controls");
	XR_LOG("P10.1 Touch controls: physical left/right bound; saved handedness selects roles");return true;
}
static XrControllerState pollControls(const XrControls &c,XrSession session,XrSpace localSpace,XrTime time,bool focused,bool leftHanded) {
	if(!focused || c.set==XR_NULL_HANDLE)return {};
	const XrActiveActionSet active={c.set,XR_NULL_PATH};
	XrActionsSyncInfo sync={XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;
	if(xrSyncActions(session,&sync)!=XR_SUCCESS)return {};
	auto button=[&](XrAction action,bool &edge) {
		XrActionStateGetInfo get={XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;
		XrActionStateBoolean state={XR_TYPE_ACTION_STATE_BOOLEAN};
		const bool held=XR_SUCCEEDED(xrGetActionStateBoolean(session,&get,&state)) && state.isActive && state.currentState;
		edge=held && state.changedSinceLastSync;return held;
	};
	auto trigger=[&](XrAction action) {
		XrActionStateGetInfo get={XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;
		XrActionStateFloat state={XR_TYPE_ACTION_STATE_FLOAT};
		return XR_SUCCEEDED(xrGetActionStateFloat(session,&get,&state)) && state.isActive && state.currentState>.65f;
	};
	auto stick=[&](XrAction action) {
		XrActionStateGetInfo get={XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;
		XrActionStateVector2f state={XR_TYPE_ACTION_STATE_VECTOR2F};
		return XR_SUCCEEDED(xrGetActionStateVector2f(session,&get,&state)) && state.isActive ? state.currentState:XrVector2f{0,0};
	};
	auto locate=[&](XrAction action,XrSpace space,XrPosef &pose) {
		XrActionStateGetInfo get={XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;
		XrActionStatePose state={XR_TYPE_ACTION_STATE_POSE};
		if(XR_FAILED(xrGetActionStatePose(session,&get,&state)) || !state.isActive)return false;
		XrSpaceLocation location={XR_TYPE_SPACE_LOCATION};
		// Estimated/inertial poses may be valid after optical tracking is lost.
		// Never let those estimates drag a window or commit a world command.
		if(XR_FAILED(xrLocateSpace(space,localSpace,time,&location)) || !xrTrackedSpace(location.locationFlags))return false;
		pose=location.pose;return true;
	};
	XrPhysicalHand hands[2];
	for(int i=0;i<2;++i) {
		auto &h=hands[i];h.trigger=trigger(c.trigger[i]);h.grip=trigger(c.grip[i]);h.stick=stick(c.stick[i]);
		h.lower=button(c.lower[i],h.lowerEdge);h.upper=button(c.upper[i],h.upperEdge);h.stickClick=button(c.stickClick[i],h.stickEdge);
		h.aimValid=locate(c.aim[i],c.aimSpace[i],h.aim);h.poseValid=locate(c.gripPose[i],c.gripSpace[i],h.pose);
	}
	bool ignored=false;return xrMapHands(hands,leftHanded,button(c.menu,ignored));
}
