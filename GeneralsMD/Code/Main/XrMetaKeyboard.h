// GeneralsX @feature Codex 23/09/2026 Native Meta Quest virtual keyboard.
//
// The Android InputMethodManager cannot present a visible IME over an
// immersive OpenXR activity. Quest's supported native path is the
// XR_META_virtual_keyboard extension plus the runtime-supplied
// XR_FB_render_model. This adapter deliberately renders that Meta-owned model
// and sends controller rays back to the runtime; no application keyboard UI
// or key layout is implemented here.
#pragma once

#if !defined(_snprintf)
#define GX_XR_RESTORE_SNPRINTF_ALIAS 1
#define _snprintf snprintf
#endif
#include "GameNetwork/GeneralsOnline/json.hpp"
#if defined(GX_XR_RESTORE_SNPRINTF_ALIAS)
#undef _snprintf
#undef GX_XR_RESTORE_SNPRINTF_ALIAS
#endif

#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <utility>

struct XrMetaKeyboard {
	struct Vertex { float x,y,z,u,v; };
	struct MorphTarget { std::vector<std::array<float,3>> position;std::vector<std::array<float,2>> uv; };
	struct Primitive {
		GLuint vao=0,vbo=0,ibo=0,texture=0;
		GLsizei count=0;
		float color[4]={1,1,1,1};
		std::vector<Vertex> baseVertices,vertices;
		std::vector<MorphTarget> targets;
	};
	struct Draw {
		int primitive=-1,node=-1;
		// A mesh can occur at many nodes (including different key labels).
		// Morph weights belong to the node, not to the shared mesh.
		GLuint vao=0,vbo=0;
		std::vector<Vertex> vertices;
	};
	struct NodeState {
		std::array<float,3> translation{0,0,0},baseTranslation{0,0,0};
		std::array<float,4> rotation{0,0,0,1},baseRotation{0,0,0,1};
		std::array<float,3> scale{1,1,1},baseScale{1,1,1};
		std::array<float,16> local{},global{},baseLocal{};
		std::vector<float> weights;
		std::vector<int> children,primitives;
		int parent=-1;bool matrix=false;
	};
	struct RuntimeTexture { GLuint texture=0;uint32_t width=0,height=0; };
	struct AnimationSampler { std::vector<float> input,output;uint32_t valuesPerFrame=0;std::string interpolation; };
	struct AnimationChannel { int sampler=-1,node=-1,additiveWeightIndex=-1;std::string path; };
	struct Animation { std::vector<AnimationSampler> samplers;std::vector<AnimationChannel> channels;float start=0,end=0; };

	bool extensionEnabled=false,supported=false,visible=false,modelLoaded=false;
	uintptr_t token=0;
	std::wstring text;
	XrInstance instance=XR_NULL_HANDLE;XrSession session=XR_NULL_HANDLE;
	XrSpace baseSpace=XR_NULL_HANDLE,keyboardSpace=XR_NULL_HANDLE;
	XrVirtualKeyboardMETA keyboard=XR_NULL_HANDLE;
	XrPosef pose={{0,0,0,1},{0,0,0}};float scale=1;
	GLuint program=0,whiteTexture=0;GLint uMvp=-1,uTexture=-1,uColor=-1,aPosition=-1,aUv=-1;
	std::vector<Primitive> primitives;std::vector<Draw> draws;std::vector<NodeState> nodes;
	std::map<uint64_t,RuntimeTexture> runtimeTextures;
	std::vector<Animation> animations;
	bool loggedLocation=false,loggedTextures=false,loggedAnimations=false,ignorePressUntilRelease=false,animationApplied=false;
	float animationStart=0,animationEnd=0;

	PFN_xrCreateVirtualKeyboardMETA createKeyboard=nullptr;
	PFN_xrDestroyVirtualKeyboardMETA destroyKeyboard=nullptr;
	PFN_xrCreateVirtualKeyboardSpaceMETA createSpace=nullptr;
	PFN_xrSuggestVirtualKeyboardLocationMETA suggestLocation=nullptr;
	PFN_xrGetVirtualKeyboardScaleMETA getScale=nullptr;
	PFN_xrSetVirtualKeyboardModelVisibilityMETA setVisibility=nullptr;
	PFN_xrGetVirtualKeyboardModelAnimationStatesMETA getAnimationStates=nullptr;
	PFN_xrGetVirtualKeyboardDirtyTexturesMETA getDirtyTextures=nullptr;
	PFN_xrGetVirtualKeyboardTextureDataMETA getTextureData=nullptr;
	PFN_xrSendVirtualKeyboardInputMETA sendInput=nullptr;
	PFN_xrChangeVirtualKeyboardTextContextMETA changeTextContext=nullptr;
	PFN_xrEnumerateRenderModelPathsFB enumerateModels=nullptr;
	PFN_xrGetRenderModelPropertiesFB getModelProperties=nullptr;
	PFN_xrLoadRenderModelFB loadModel=nullptr;

	static std::string toUtf8(const std::wstring &value) {
		std::string out;
		for(uint32_t cp:value) {
			if(cp<=0x7f)out.push_back(char(cp));
			else if(cp<=0x7ff){out.push_back(char(0xc0|(cp>>6)));out.push_back(char(0x80|(cp&63)));}
			else if(cp<=0xffff){out.push_back(char(0xe0|(cp>>12)));out.push_back(char(0x80|((cp>>6)&63)));out.push_back(char(0x80|(cp&63)));}
			else if(cp<=0x10ffff){out.push_back(char(0xf0|(cp>>18)));out.push_back(char(0x80|((cp>>12)&63)));out.push_back(char(0x80|((cp>>6)&63)));out.push_back(char(0x80|(cp&63)));}
		}
		return out;
	}
	static std::wstring fromUtf8(const char *source) {
		std::wstring out;if(!source)return out;
		const auto *p=reinterpret_cast<const unsigned char *>(source);
		while(*p) {
			uint32_t cp=0;int trail=0;
			if(*p<0x80)cp=*p++;
			else if((*p&0xe0)==0xc0){cp=*p++&0x1f;trail=1;}
			else if((*p&0xf0)==0xe0){cp=*p++&0x0f;trail=2;}
			else if((*p&0xf8)==0xf0){cp=*p++&7;trail=3;}
			else {++p;continue;}
			bool ok=true;for(int i=0;i<trail;++i){if((p[i]&0xc0)!=0x80){ok=false;break;}cp=(cp<<6)|(p[i]&63);}p+=ok ? trail:0;
			if(ok && cp<=0x10ffff)out.push_back(wchar_t(cp));
		}
		return out;
	}

	template<typename T> bool proc(const char *name,T &slot) {
		return XR_SUCCEEDED(xrGetInstanceProcAddr(instance,name,reinterpret_cast<PFN_xrVoidFunction *>(&slot))) && slot;
	}

	bool init(XrInstance inst,XrSession sess,XrSystemId systemId,XrSpace localSpace,bool enabled) {
		extensionEnabled=enabled;instance=inst;session=sess;baseSpace=localSpace;
		if(!enabled)return false;
		bool functions=
			proc("xrCreateVirtualKeyboardMETA",createKeyboard) && proc("xrDestroyVirtualKeyboardMETA",destroyKeyboard) &&
			proc("xrCreateVirtualKeyboardSpaceMETA",createSpace) && proc("xrSuggestVirtualKeyboardLocationMETA",suggestLocation) &&
			proc("xrGetVirtualKeyboardScaleMETA",getScale) && proc("xrSetVirtualKeyboardModelVisibilityMETA",setVisibility) &&
			proc("xrGetVirtualKeyboardModelAnimationStatesMETA",getAnimationStates) &&
			proc("xrGetVirtualKeyboardDirtyTexturesMETA",getDirtyTextures) && proc("xrGetVirtualKeyboardTextureDataMETA",getTextureData) &&
			proc("xrSendVirtualKeyboardInputMETA",sendInput) && proc("xrChangeVirtualKeyboardTextContextMETA",changeTextContext) &&
			proc("xrEnumerateRenderModelPathsFB",enumerateModels) && proc("xrGetRenderModelPropertiesFB",getModelProperties) &&
			proc("xrLoadRenderModelFB",loadModel);
		if(!functions){XR_LOGE("Meta virtual keyboard entry points unavailable");return false;}
		XrSystemVirtualKeyboardPropertiesMETA keyboardProps={XR_TYPE_SYSTEM_VIRTUAL_KEYBOARD_PROPERTIES_META};
		XrSystemRenderModelPropertiesFB modelProps={XR_TYPE_SYSTEM_RENDER_MODEL_PROPERTIES_FB};
		keyboardProps.next=&modelProps;
		XrSystemProperties properties={XR_TYPE_SYSTEM_PROPERTIES,&keyboardProps};
		if(XR_FAILED(xrGetSystemProperties(instance,systemId,&properties)) || !keyboardProps.supportsVirtualKeyboard || !modelProps.supportsRenderModelLoading) {
			XR_LOGE("Meta virtual keyboard unsupported (keyboard=%d model=%d)",int(keyboardProps.supportsVirtualKeyboard),int(modelProps.supportsRenderModelLoading));return false;
		}
		XrVirtualKeyboardCreateInfoMETA create={XR_TYPE_VIRTUAL_KEYBOARD_CREATE_INFO_META};
		if(XR_FAILED(createKeyboard(session,&create,&keyboard))){XR_LOGE("xrCreateVirtualKeyboardMETA failed");return false;}
		XrVirtualKeyboardSpaceCreateInfoMETA spaceInfo={XR_TYPE_VIRTUAL_KEYBOARD_SPACE_CREATE_INFO_META};
		spaceInfo.locationType=XR_VIRTUAL_KEYBOARD_LOCATION_TYPE_CUSTOM_META;spaceInfo.space=baseSpace;spaceInfo.poseInSpace.orientation.w=1;
		if(XR_FAILED(createSpace(session,keyboard,&spaceInfo,&keyboardSpace))){XR_LOGE("xrCreateVirtualKeyboardSpaceMETA failed");destroyKeyboard(keyboard);keyboard=XR_NULL_HANDLE;return false;}
		supported=true;XR_LOG("Meta native virtual keyboard ready");return true;
	}

	void updateContext() {
		if(!supported || !changeTextContext)return;
		const std::string utf8=toUtf8(text);
		XrVirtualKeyboardTextContextChangeInfoMETA info={XR_TYPE_VIRTUAL_KEYBOARD_TEXT_CONTEXT_CHANGE_INFO_META};info.textContext=utf8.c_str();
		if(XR_FAILED(changeTextContext(keyboard,&info)))XR_LOGE("xrChangeVirtualKeyboardTextContextMETA failed");
	}

	bool show(uintptr_t fieldToken,const XrPosef &head) {
		if(!supported || !fieldToken)return false;
		if(!ensureModel())return false;
		token=fieldToken;text=XrGameBoot_TextFieldValue(token);updateContext();
		// App-owned placement is still the native Meta keyboard; it only avoids
		// the runtime's deliberately room-sized FAR preset. Put a compact board
		// below the current gaze, stable in LOCAL space and facing the player.
		float fx=0,fz=-1;yawForwardFromQuat(head.orientation,&fx,&fz);
		const float yaw=atan2f(-fx,-fz);
		XrVirtualKeyboardLocationInfoMETA location={XR_TYPE_VIRTUAL_KEYBOARD_LOCATION_INFO_META};
		location.locationType=XR_VIRTUAL_KEYBOARD_LOCATION_TYPE_CUSTOM_META;location.space=baseSpace;
		location.poseInSpace.orientation=xrAxisAngle({0,1,0},yaw);
		location.poseInSpace.position={head.position.x+fx*.72f,head.position.y-.30f,head.position.z+fz*.72f};
		location.scale=.62f;
		if(XR_FAILED(suggestLocation(keyboard,&location))){XR_LOGE("xrSuggestVirtualKeyboardLocationMETA failed");return false;}
		XrVirtualKeyboardModelVisibilitySetInfoMETA showInfo={XR_TYPE_VIRTUAL_KEYBOARD_MODEL_VISIBILITY_SET_INFO_META};showInfo.visible=XR_TRUE;
		if(XR_FAILED(setVisibility(keyboard,&showInfo))){XR_LOGE("xrSetVirtualKeyboardModelVisibilityMETA(show) failed");return false;}
		visible=true;ignorePressUntilRelease=true;loggedLocation=false;
		XR_LOG("Meta native keyboard requested for original field");return true;
	}

	void hide(bool done=false) {
		if(!supported || !token)return;
		if(done)XrGameBoot_ReplaceTextField(token,text,true);
		XrVirtualKeyboardModelVisibilitySetInfoMETA hideInfo={XR_TYPE_VIRTUAL_KEYBOARD_MODEL_VISIBILITY_SET_INFO_META};hideInfo.visible=XR_FALSE;
		setVisibility(keyboard,&hideInfo);visible=false;token=0;text.clear();
	}

	bool handleEvent(const XrEventDataBuffer &event) {
		if(!supported)return false;
		switch(event.type) {
			case XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_COMMIT_TEXT_META: {
				const auto *e=reinterpret_cast<const XrEventDataVirtualKeyboardCommitTextMETA *>(&event);
				if(e->keyboard!=keyboard || !token)return true;text+=fromUtf8(e->text);
				XrGameBoot_ReplaceTextField(token,text,false);updateContext();return true;
			}
			case XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_BACKSPACE_META:
				if(token && !text.empty()){text.pop_back();XrGameBoot_ReplaceTextField(token,text,false);updateContext();}return true;
			case XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_ENTER_META:
				if(token)hide(true);return true;
			case XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_SHOWN_META:
				visible=true;XR_LOG("Meta native keyboard shown");return true;
			case XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_HIDDEN_META:
				visible=false;token=0;text.clear();XR_LOG("Meta native keyboard hidden");return true;
			default:return false;
		}
	}

	void update(XrTime time,const XrControllerState &controls) {
		if(!visible || !supported)return;
		XrSpaceLocation located={XR_TYPE_SPACE_LOCATION};
		if(XR_SUCCEEDED(xrLocateSpace(keyboardSpace,baseSpace,time,&located)) &&
			(located.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT) && (located.locationFlags&XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))pose=located.pose;
		getScale(keyboard,&scale);
		if(!loggedLocation){XR_LOG("Meta keyboard pose=(%.2f,%.2f,%.2f) scale=%.3f",pose.position.x,pose.position.y,pose.position.z,scale);loggedLocation=true;}
		bool anyPressed=false;for(int hand=0;hand<2;++hand)anyPressed=anyPressed||controls.physicalSelect[hand];
		if(ignorePressUntilRelease && !anyPressed)ignorePressUntilRelease=false;
		for(int hand=0;hand<2;++hand)if(controls.physicalAimValid[hand]) {
			const bool pressed=controls.physicalSelect[hand] && !ignorePressUntilRelease;
			XrPosef root=controls.physicalGripValid[hand] ? controls.physicalGrip[hand]:controls.physicalAim[hand];
			auto submit=[&](XrVirtualKeyboardInputSourceMETA source,const XrPosef &inputPose) {
				XrVirtualKeyboardInputInfoMETA input={XR_TYPE_VIRTUAL_KEYBOARD_INPUT_INFO_META};
				input.inputSource=source;input.inputSpace=baseSpace;input.inputPoseInSpace=inputPose;
				if(pressed)input.inputState=XR_VIRTUAL_KEYBOARD_INPUT_STATE_PRESSED_BIT_META;
				return XR_SUCCEEDED(sendInput(keyboard,&input,&root));
			};
			// Match Meta's reference sample: submit ray and direct controller
			// input every frame and let the runtime choose the useful modality.
			submit(hand==0 ? XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_RAY_LEFT_META:XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_RAY_RIGHT_META,controls.physicalAim[hand]);
			submit(hand==0 ? XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_DIRECT_LEFT_META:XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_DIRECT_RIGHT_META,controls.physicalAim[hand]);
		}
		refreshTextures();
		refreshAnimations();
	}

	XrVector3f rayEndpoint(const XrPosef &aim) const {
		const XrVector3f direction=xrRotate(aim.orientation,{0,0,-1});
		const XrVector3f normal=xrRotate(pose.orientation,{0,0,1});
		const XrVector3f delta=xrSub(pose.position,aim.position);
		const float denominator=direction.x*normal.x+direction.y*normal.y+direction.z*normal.z;
		float distance=1.2f;
		if(fabsf(denominator)>1e-5f){const float hit=(delta.x*normal.x+delta.y*normal.y+delta.z*normal.z)/denominator;if(hit>0)distance=std::clamp(hit,.1f,3.0f);}
		return {aim.position.x+direction.x*distance,aim.position.y+direction.y*distance,aim.position.z+direction.z*distance};
	}

	static void poseMatrix(float *out,const XrPosef &p) {
		const float x=p.orientation.x,y=p.orientation.y,z=p.orientation.z,w=p.orientation.w;
		out[0]=1-2*(y*y+z*z);out[1]=2*(x*y+z*w);out[2]=2*(x*z-y*w);out[3]=0;
		out[4]=2*(x*y-z*w);out[5]=1-2*(x*x+z*z);out[6]=2*(y*z+x*w);out[7]=0;
		out[8]=2*(x*z+y*w);out[9]=2*(y*z-x*w);out[10]=1-2*(x*x+y*y);out[11]=0;
		out[12]=p.position.x;out[13]=p.position.y;out[14]=p.position.z;out[15]=1;
	}

	void render(const float *projection,const float *view) {
		if(!visible || !modelLoaded || !program)return;
		float keyboardModel[16],scaled[16],scaleMatrix[16];poseMatrix(keyboardModel,pose);matScale(scaleMatrix,scale,scale,scale);matMultiply(scaled,keyboardModel,scaleMatrix);
		xr_glUseProgram(program);xr_glActiveTexture(GL_TEXTURE4);xr_glUniform1i(uTexture,4);
		xr_glEnable(GL_DEPTH_TEST);xr_glDepthFunc(GL_LEQUAL);xr_glDepthMask(GL_TRUE);xr_glDisable(GL_CULL_FACE);
		xr_glEnable(GL_BLEND);xr_glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
		for(const auto &draw:draws) {
			if(draw.primitive<0 || draw.primitive>=int(primitives.size()))continue;const auto &primitive=primitives[draw.primitive];
			if(draw.node<0 || draw.node>=int(nodes.size()))continue;
			float model[16],viewModel[16],mvp[16];matMultiply(model,scaled,nodes[draw.node].global.data());matMultiply(viewModel,view,model);matMultiply(mvp,projection,viewModel);
			xr_glUniformMatrix4fv(uMvp,1,GL_FALSE,mvp);xr_glUniform4f(uColor,primitive.color[0],primitive.color[1],primitive.color[2],primitive.color[3]);
			xr_glBindTexture(GL_TEXTURE_2D,primitive.texture ? primitive.texture:whiteTexture);xr_glBindVertexArray(draw.vao ? draw.vao:primitive.vao);
			xr_glDrawElements(GL_TRIANGLES,primitive.count,GL_UNSIGNED_INT,nullptr);
		}
		xr_glBindVertexArray(0);xr_glBindTexture(GL_TEXTURE_2D,0);xr_glDisable(GL_BLEND);xr_glUseProgram(0);
	}

	void shutdown() {
		if(supported && keyboard!=XR_NULL_HANDLE){XrVirtualKeyboardModelVisibilitySetInfoMETA info={XR_TYPE_VIRTUAL_KEYBOARD_MODEL_VISIBILITY_SET_INFO_META};info.visible=XR_FALSE;setVisibility(keyboard,&info);}
		for(auto &d:draws){if(d.vbo)xr_glDeleteBuffers(1,&d.vbo);if(d.vao)xr_glDeleteVertexArrays(1,&d.vao);}draws.clear();nodes.clear();
		for(auto &p:primitives){if(p.ibo)xr_glDeleteBuffers(1,&p.ibo);if(p.vbo)xr_glDeleteBuffers(1,&p.vbo);if(p.vao)xr_glDeleteVertexArrays(1,&p.vao);}primitives.clear();
		for(auto &entry:runtimeTextures)if(entry.second.texture)xr_glDeleteTextures(1,&entry.second.texture);runtimeTextures.clear();
		if(whiteTexture)xr_glDeleteTextures(1,&whiteTexture);whiteTexture=0;if(program)xr_glDeleteProgram(program);program=0;
		if(keyboardSpace!=XR_NULL_HANDLE)xrDestroySpace(keyboardSpace);keyboardSpace=XR_NULL_HANDLE;
		if(keyboard!=XR_NULL_HANDLE && destroyKeyboard)destroyKeyboard(keyboard);keyboard=XR_NULL_HANDLE;
		supported=visible=modelLoaded=false;token=0;
	}

	private:
	static int components(const std::string &type) {return type=="SCALAR"?1:type=="VEC2"?2:type=="VEC3"?3:type=="VEC4"?4:0;}
	static int componentSize(int type) {return type==5120||type==5121?1:type==5122||type==5123?2:type==5125||type==5126?4:0;}
	static float scalar(const uint8_t *p,int type) {
		switch(type){case 5120:return *reinterpret_cast<const int8_t *>(p);case 5121:return *p;case 5122:{int16_t v;memcpy(&v,p,2);return v;}case 5123:{uint16_t v;memcpy(&v,p,2);return v;}case 5125:{uint32_t v;memcpy(&v,p,4);return float(v);}case 5126:{float v;memcpy(&v,p,4);return v;}default:return 0;}
	}
	static float attributeScalar(const uint8_t *p,int type,bool normalized) {
		const float value=scalar(p,type);if(!normalized)return value;
		switch(type){case 5120:return std::max(value/127.0f,-1.0f);case 5121:return value/255.0f;case 5122:return std::max(value/32767.0f,-1.0f);case 5123:return value/65535.0f;default:return value;}
	}
	static uint32_t indexValue(const uint8_t *p,int type) {return uint32_t(scalar(p,type));}
	static void identity(float *m){memset(m,0,16*sizeof(float));m[0]=m[5]=m[10]=m[15]=1;}
	static void nodeMatrix(const nlohmann::json &node,float *m) {
		if(node.contains("matrix")){for(int i=0;i<16;++i)m[i]=node["matrix"][i].get<float>();return;}
		XrPosef p={{0,0,0,1},{0,0,0}};float sx=1,sy=1,sz=1;
		if(node.contains("translation")){p.position.x=node["translation"][0];p.position.y=node["translation"][1];p.position.z=node["translation"][2];}
		if(node.contains("rotation")){p.orientation.x=node["rotation"][0];p.orientation.y=node["rotation"][1];p.orientation.z=node["rotation"][2];p.orientation.w=node["rotation"][3];}
		if(node.contains("scale")){sx=node["scale"][0];sy=node["scale"][1];sz=node["scale"][2];}
		float pose[16],scale[16];poseMatrix(pose,p);matScale(scale,sx,sy,sz);matMultiply(m,pose,scale);
	}

	bool ensureProgram() {
		if(program)return true;
		static const char *vs="#version 300 es\nuniform mat4 uMVP;in vec3 aPosition;in vec2 aUV;out highp vec2 vUV;void main(){gl_Position=uMVP*vec4(aPosition,1.0);vUV=aUV;}";
		static const char *fs="#version 300 es\nprecision mediump float;uniform sampler2D uTexture;uniform vec4 uColor;in highp vec2 vUV;out vec4 oColor;void main(){vec4 c=texture(uTexture,vUV);oColor=vec4(pow(c.rgb*uColor.rgb,vec3(2.2))*c.a,c.a);}";
		GLuint vert=compileShader(GL_VERTEX_SHADER,vs),frag=compileShader(GL_FRAGMENT_SHADER,fs);if(!vert||!frag)return false;
		program=xr_glCreateProgram();xr_glAttachShader(program,vert);xr_glAttachShader(program,frag);xr_glLinkProgram(program);xr_glDeleteShader(vert);xr_glDeleteShader(frag);
		GLint linked=0;xr_glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked){XR_LOGE("Meta keyboard shader link failed");return false;}
		uMvp=xr_glGetUniformLocation(program,"uMVP");uTexture=xr_glGetUniformLocation(program,"uTexture");uColor=xr_glGetUniformLocation(program,"uColor");
		aPosition=xr_glGetAttribLocation(program,"aPosition");aUv=xr_glGetAttribLocation(program,"aUV");
		uint32_t pixel=0xffffffff;xr_glGenTextures(1,&whiteTexture);xr_glBindTexture(GL_TEXTURE_2D,whiteTexture);xr_glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,&pixel);
		xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);return true;
	}

	GLuint textureFromUri(const std::string &uri) {
		static const std::string prefix="metaVirtualKeyboard://texture/";if(uri.rfind(prefix,0)!=0)return whiteTexture;
		char *end=nullptr;const uint64_t id=strtoull(uri.c_str()+prefix.size(),&end,10);if(!end || *end!='?')return whiteTexture;
		uint32_t width=0,height=0;if(sscanf(end,"?w=%u&h=%u&fmt=RGBA32",&width,&height)!=2 || !width || !height)return whiteTexture;
		auto existing=runtimeTextures.find(id);if(existing!=runtimeTextures.end())return existing->second.texture;
		RuntimeTexture out;out.width=width;out.height=height;std::vector<uint8_t> blank(size_t(width)*height*4);
		xr_glGenTextures(1,&out.texture);xr_glBindTexture(GL_TEXTURE_2D,out.texture);xr_glPixelStorei(GL_UNPACK_ALIGNMENT,1);
		xr_glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,blank.data());
		xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
		runtimeTextures[id]=out;return out.texture;
	}

	bool ensureModel() {
		if(modelLoaded)return true;if(!supported || !ensureProgram())return false;
		uint32_t count=0;if(XR_FAILED(enumerateModels(session,0,&count,nullptr)) || !count){XR_LOGE("No Meta render models available");return false;}
		std::vector<XrRenderModelPathInfoFB> paths(count,{XR_TYPE_RENDER_MODEL_PATH_INFO_FB});if(XR_FAILED(enumerateModels(session,count,&count,paths.data())))return false;
		XrRenderModelKeyFB key=XR_NULL_RENDER_MODEL_KEY_FB;
		for(const auto &entry:paths){char path[XR_MAX_PATH_LENGTH]={};uint32_t length=0;if(XR_FAILED(xrPathToString(instance,entry.path,sizeof(path),&length,path)))continue;if(strcmp(path,"/model_meta/keyboard/virtual"))continue;
			XrRenderModelCapabilitiesRequestFB capabilities={XR_TYPE_RENDER_MODEL_CAPABILITIES_REQUEST_FB};capabilities.flags=XR_RENDER_MODEL_SUPPORTS_GLTF_2_0_SUBSET_2_BIT_FB;
			XrRenderModelPropertiesFB properties={XR_TYPE_RENDER_MODEL_PROPERTIES_FB,&capabilities};if(XR_SUCCEEDED(getModelProperties(session,entry.path,&properties)))key=properties.modelKey;break;}
		if(key==XR_NULL_RENDER_MODEL_KEY_FB){XR_LOGE("Meta virtual keyboard render model unavailable");return false;}
		XrRenderModelLoadInfoFB load={XR_TYPE_RENDER_MODEL_LOAD_INFO_FB};load.modelKey=key;XrRenderModelBufferFB buffer={XR_TYPE_RENDER_MODEL_BUFFER_FB};
		if(XR_FAILED(loadModel(session,&load,&buffer)) || !buffer.bufferCountOutput)return false;std::vector<uint8_t> bytes(buffer.bufferCountOutput);buffer.bufferCapacityInput=uint32_t(bytes.size());buffer.buffer=bytes.data();
		if(XR_FAILED(loadModel(session,&load,&buffer)) || !parseGlb(bytes)){XR_LOGE("Meta virtual keyboard GLB load failed");return false;}
		modelLoaded=true;XR_LOG("Meta virtual keyboard model loaded: %zu draw(s), %zu runtime texture(s)",draws.size(),runtimeTextures.size());refreshTextures();return true;
	}

	bool accessorData(const nlohmann::json &root,const uint8_t *binary,size_t binarySize,int index,const uint8_t *&data,size_t &stride,size_t &count,int &type,int &width) {
		if(index<0 || !root.contains("accessors") || index>=int(root["accessors"].size()))return false;const auto &accessor=root["accessors"][index];
		const int viewIndex=accessor.value("bufferView",-1);if(viewIndex<0 || viewIndex>=int(root["bufferViews"].size()))return false;const auto &view=root["bufferViews"][viewIndex];
		type=accessor.value("componentType",0);width=components(accessor.value("type",std::string()));count=accessor.value("count",size_t(0));const int bytes=componentSize(type);if(!bytes||!width||!count)return false;
		const size_t offset=view.value("byteOffset",size_t(0))+accessor.value("byteOffset",size_t(0));stride=view.value("byteStride",size_t(bytes*width));if(offset+stride*(count-1)+bytes*width>binarySize)return false;data=binary+offset;return true;
	}

	bool parseGlb(const std::vector<uint8_t> &bytes) {
		if(bytes.size()<20 || memcmp(bytes.data(),"glTF",4)!=0)return false;uint32_t version=0;memcpy(&version,bytes.data()+4,4);if(version!=2)return false;
		const uint8_t *jsonData=nullptr,*binary=nullptr;size_t jsonSize=0,binarySize=0,offset=12;
		while(offset+8<=bytes.size()){uint32_t length=0,type=0;memcpy(&length,bytes.data()+offset,4);memcpy(&type,bytes.data()+offset+4,4);offset+=8;if(offset+length>bytes.size())return false;if(type==0x4e4f534a){jsonData=bytes.data()+offset;jsonSize=length;}else if(type==0x004e4942){binary=bytes.data()+offset;binarySize=length;}offset+=length;}
		if(!jsonData || !binary)return false;auto root=nlohmann::json::parse(jsonData,jsonData+jsonSize,nullptr,false);if(root.is_discarded())return false;
		std::vector<GLuint> images;if(root.contains("images"))for(const auto &image:root["images"])images.push_back(textureFromUri(image.value("uri",std::string())));
		std::vector<GLuint> textures;if(root.contains("textures"))for(const auto &texture:root["textures"]){const int source=texture.value("source",-1);textures.push_back(source>=0&&source<int(images.size())?images[source]:whiteTexture);}
		struct Material {GLuint texture=0;float color[4]={1,1,1,1};};std::vector<Material> materials;
		if(root.contains("materials"))for(const auto &entry:root["materials"]){Material material;material.texture=whiteTexture;if(entry.contains("pbrMetallicRoughness")){const auto &pbr=entry["pbrMetallicRoughness"];
			if(pbr.contains("baseColorTexture")){const int t=pbr["baseColorTexture"].value("index",-1);if(t>=0&&t<int(textures.size()))material.texture=textures[t];}
			if(pbr.contains("baseColorFactor"))for(int i=0;i<4&&i<int(pbr["baseColorFactor"].size());++i)material.color[i]=pbr["baseColorFactor"][i];}materials.push_back(material);}
		std::vector<std::vector<int>> meshPrimitives;
		if(root.contains("meshes"))for(const auto &mesh:root["meshes"]){std::vector<int> list;for(const auto &source:mesh["primitives"]){if(source.value("mode",4)!=4 || !source.contains("attributes") || !source["attributes"].contains("POSITION"))continue;
			const uint8_t *positions=nullptr,*uvs=nullptr,*indices=nullptr;size_t posStride=0,uvStride=0,indexStride=0,posCount=0,uvCount=0,indexCount=0;int posType=0,uvType=0,indexType=0,posWidth=0,uvWidth=0,indexWidth=0;
			if(!accessorData(root,binary,binarySize,source["attributes"]["POSITION"],positions,posStride,posCount,posType,posWidth) || posWidth<3)continue;
			const int uvAccessor=source["attributes"].value("TEXCOORD_0",-1);if(uvAccessor>=0)accessorData(root,binary,binarySize,uvAccessor,uvs,uvStride,uvCount,uvType,uvWidth);
			const bool uvNormalized=uvAccessor>=0&&root["accessors"][uvAccessor].value("normalized",false);
			std::vector<Vertex> vertices(posCount);for(size_t i=0;i<posCount;++i){const auto *p=positions+i*posStride;vertices[i]={scalar(p,posType),scalar(p+componentSize(posType),posType),scalar(p+2*componentSize(posType),posType),0,0};if(uvs&&i<uvCount&&uvWidth>=2){const auto *uv=uvs+i*uvStride;vertices[i].u=attributeScalar(uv,uvType,uvNormalized);vertices[i].v=attributeScalar(uv+componentSize(uvType),uvType,uvNormalized);}}
			std::vector<uint32_t> indexValues;const int accessor=source.value("indices",-1);if(accessor>=0&&accessorData(root,binary,binarySize,accessor,indices,indexStride,indexCount,indexType,indexWidth)){indexValues.resize(indexCount);for(size_t i=0;i<indexCount;++i)indexValues[i]=indexValue(indices+i*indexStride,indexType);}else{indexValues.resize(posCount);for(size_t i=0;i<posCount;++i)indexValues[i]=uint32_t(i);}
			Primitive primitive;primitive.baseVertices=vertices;primitive.vertices=vertices;
			if(source.contains("targets"))for(const auto &targetSource:source["targets"]){MorphTarget target;target.position.resize(posCount,{0,0,0});target.uv.resize(posCount,{0,0});
				auto readTarget=[&](const char *name,int wanted){if(!targetSource.contains(name))return;const uint8_t *values=nullptr;size_t stride=0,count=0;int type=0,width=0;const int targetAccessor=targetSource[name];if(!accessorData(root,binary,binarySize,targetAccessor,values,stride,count,type,width)||width<wanted)return;const bool normalized=root["accessors"][targetAccessor].value("normalized",false);for(size_t i=0;i<std::min(count,posCount);++i)for(int c=0;c<wanted;++c){const float value=attributeScalar(values+i*stride+c*componentSize(type),type,normalized);if(wanted==3)target.position[i][c]=value;else target.uv[i][c]=value;}};
				readTarget("POSITION",3);readTarget("TEXCOORD_0",2);primitive.targets.push_back(std::move(target));}
			const int materialIndex=source.value("material",-1);if(materialIndex>=0&&materialIndex<int(materials.size())){primitive.texture=materials[materialIndex].texture;memcpy(primitive.color,materials[materialIndex].color,sizeof(primitive.color));}else primitive.texture=whiteTexture;primitive.count=GLsizei(indexValues.size());
			xr_glGenVertexArrays(1,&primitive.vao);xr_glBindVertexArray(primitive.vao);xr_glGenBuffers(1,&primitive.vbo);xr_glBindBuffer(GL_ARRAY_BUFFER,primitive.vbo);xr_glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STATIC_DRAW);
			xr_glEnableVertexAttribArray(GLuint(aPosition));xr_glVertexAttribPointer(GLuint(aPosition),3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);xr_glEnableVertexAttribArray(GLuint(aUv));xr_glVertexAttribPointer(GLuint(aUv),2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void *>(3*sizeof(float)));
			xr_glGenBuffers(1,&primitive.ibo);xr_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,primitive.ibo);xr_glBufferData(GL_ELEMENT_ARRAY_BUFFER,indexValues.size()*sizeof(uint32_t),indexValues.data(),GL_STATIC_DRAW);
			list.push_back(int(primitives.size()));primitives.push_back(std::move(primitive));}meshPrimitives.push_back(std::move(list));}
		xr_glBindVertexArray(0);xr_glBindBuffer(GL_ARRAY_BUFFER,0);xr_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
		if(!root.contains("nodes"))return false;
		const auto &sourceNodes=root["nodes"];nodes.resize(sourceNodes.size());
		for(size_t i=0;i<nodes.size();++i){auto &node=nodes[i];const auto &source=sourceNodes[i];
			node.matrix=source.contains("matrix");nodeMatrix(source,node.local.data());node.baseLocal=node.local;
			if(source.contains("translation"))for(int c=0;c<3;++c)node.translation[c]=source["translation"][c];node.baseTranslation=node.translation;
			if(source.contains("rotation"))for(int c=0;c<4;++c)node.rotation[c]=source["rotation"][c];node.baseRotation=node.rotation;
			if(source.contains("scale"))for(int c=0;c<3;++c)node.scale[c]=source["scale"][c];node.baseScale=node.scale;
			if(source.contains("children"))for(int child:source["children"])if(child>=0&&child<int(nodes.size())){node.children.push_back(child);nodes[child].parent=int(i);}
			const int mesh=source.value("mesh",-1);
			if(mesh>=0&&mesh<int(meshPrimitives.size())){
				node.primitives=meshPrimitives[mesh];size_t targetCount=0;for(int p:node.primitives)targetCount=std::max(targetCount,primitives[p].targets.size());node.weights.resize(targetCount,0);
				const auto &sourceMesh=root["meshes"][mesh];const auto *weights=source.contains("weights")?&source["weights"]:sourceMesh.contains("weights")?&sourceMesh["weights"]:nullptr;
				if(weights)for(size_t w=0;w<std::min(weights->size(),node.weights.size());++w)node.weights[w]=(*weights)[w];
			}
		}
		std::vector<bool> visited(nodes.size(),false);
		std::function<void(int)> visit=[&](int index){if(index<0||index>=int(nodes.size())||visited[index])return;visited[index]=true;
			const auto &node=nodes[index];if(sourceNodes[index].value("name",std::string())!="collision")for(int primitiveIndex:node.primitives){
				const auto &primitive=primitives[primitiveIndex];Draw draw;draw.primitive=primitiveIndex;draw.node=index;
				if(!primitive.targets.empty()){
					draw.vertices=primitive.baseVertices;xr_glGenVertexArrays(1,&draw.vao);xr_glBindVertexArray(draw.vao);xr_glGenBuffers(1,&draw.vbo);xr_glBindBuffer(GL_ARRAY_BUFFER,draw.vbo);
					xr_glBufferData(GL_ARRAY_BUFFER,draw.vertices.size()*sizeof(Vertex),draw.vertices.data(),GL_DYNAMIC_DRAW);
					xr_glEnableVertexAttribArray(GLuint(aPosition));xr_glVertexAttribPointer(GLuint(aPosition),3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
					xr_glEnableVertexAttribArray(GLuint(aUv));xr_glVertexAttribPointer(GLuint(aUv),2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void *>(3*sizeof(float)));
					xr_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,primitive.ibo);
				}draws.push_back(std::move(draw));
			}for(int child:node.children)visit(child);
		};
		bool usedScene=false;if(root.contains("scenes")&&!root["scenes"].empty()){const int scene=std::clamp(root.value("scene",0),0,int(root["scenes"].size())-1);if(root["scenes"][scene].contains("nodes")){for(int node:root["scenes"][scene]["nodes"])visit(node);usedScene=true;}}
		if(!usedScene)for(size_t i=0;i<nodes.size();++i)if(nodes[i].parent<0)visit(int(i));
		xr_glBindVertexArray(0);xr_glBindBuffer(GL_ARRAY_BUFFER,0);xr_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
		recalculateTransforms();
		animationStart=1e30f;animationEnd=-1e30f;
		if(root.contains("animations"))for(const auto &sourceAnimation:root["animations"]){Animation animation;animation.start=1e30f;animation.end=-1e30f;
			for(const auto &sourceSampler:sourceAnimation["samplers"]){AnimationSampler sampler;sampler.interpolation=sourceSampler.value("interpolation",std::string("LINEAR"));const uint8_t *input=nullptr,*output=nullptr;size_t inputStride=0,outputStride=0,inputCount=0,outputCount=0;int inputType=0,outputType=0,inputWidth=0,outputWidth=0;
				if(accessorData(root,binary,binarySize,sourceSampler["input"],input,inputStride,inputCount,inputType,inputWidth)){sampler.input.resize(inputCount);for(size_t i=0;i<inputCount;++i)sampler.input[i]=scalar(input+i*inputStride,inputType);if(!sampler.input.empty()){animation.start=std::min(animation.start,sampler.input.front());animation.end=std::max(animation.end,sampler.input.back());}}
				if(accessorData(root,binary,binarySize,sourceSampler["output"],output,outputStride,outputCount,outputType,outputWidth)){sampler.output.resize(outputCount*outputWidth);for(size_t i=0;i<outputCount;++i)for(int c=0;c<outputWidth;++c)sampler.output[i*outputWidth+c]=scalar(output+i*outputStride+c*componentSize(outputType),outputType);sampler.valuesPerFrame=inputCount?uint32_t(sampler.output.size()/inputCount):0;}animation.samplers.push_back(std::move(sampler));}
			for(const auto &sourceChannel:sourceAnimation["channels"]){AnimationChannel channel;channel.sampler=sourceChannel.value("sampler",-1);if(sourceChannel.contains("target")){channel.node=sourceChannel["target"].value("node",-1);channel.path=sourceChannel["target"].value("path",std::string());}if(sourceChannel.contains("extras"))channel.additiveWeightIndex=sourceChannel["extras"].value("additiveWeightIndex",-1);animation.channels.push_back(std::move(channel));}
			if(animation.start>animation.end){animation.start=0;animation.end=1;}animationStart=std::min(animationStart,animation.start);animationEnd=std::max(animationEnd,animation.end);animations.push_back(std::move(animation));}
		XR_LOG("Meta keyboard GLB: nodes=%zu primitives=%zu animations=%zu",nodes.size(),primitives.size(),animations.size());return !draws.empty();
	}

	void refreshTextures() {
		if(!visible && !modelLoaded)return;uint32_t count=0;if(XR_FAILED(getDirtyTextures(keyboard,0,&count,nullptr)) || !count)return;std::vector<uint64_t> ids(count);if(XR_FAILED(getDirtyTextures(keyboard,count,&count,ids.data())))return;
		if(!loggedTextures){XR_LOG("Meta keyboard dirty textures=%u registered=%zu",count,runtimeTextures.size());loggedTextures=true;}
		for(uint64_t id:ids){auto found=runtimeTextures.find(id);if(found==runtimeTextures.end()){XR_LOGE("Meta keyboard dirty texture id %llu is not registered",(unsigned long long)id);continue;}XrVirtualKeyboardTextureDataMETA data={XR_TYPE_VIRTUAL_KEYBOARD_TEXTURE_DATA_META};if(XR_FAILED(getTextureData(keyboard,id,&data)) || !data.bufferCountOutput)continue;std::vector<uint8_t> pixels(data.bufferCountOutput);data.bufferCapacityInput=uint32_t(pixels.size());data.buffer=pixels.data();if(XR_FAILED(getTextureData(keyboard,id,&data)))continue;
			if(data.textureWidth!=found->second.width||data.textureHeight!=found->second.height){XR_LOGE("Meta keyboard texture %llu size mismatch %ux%u != %ux%u",(unsigned long long)id,data.textureWidth,data.textureHeight,found->second.width,found->second.height);continue;}xr_glActiveTexture(GL_TEXTURE4);xr_glBindTexture(GL_TEXTURE_2D,found->second.texture);xr_glPixelStorei(GL_UNPACK_ALIGNMENT,1);xr_glTexSubImage2D(GL_TEXTURE_2D,0,0,0,data.textureWidth,data.textureHeight,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());}
	}

	void recalculateTransforms() {
		std::function<void(int,const float *)> visit=[&](int index,const float *parent){auto &node=nodes[index];
			if(!node.matrix){XrPosef p={{node.rotation[0],node.rotation[1],node.rotation[2],node.rotation[3]},{node.translation[0],node.translation[1],node.translation[2]}};
				float rotation[16],scaling[16];poseMatrix(rotation,p);matScale(scaling,node.scale[0],node.scale[1],node.scale[2]);matMultiply(node.local.data(),rotation,scaling);}
			matMultiply(node.global.data(),parent,node.local.data());for(int child:node.children)visit(child,node.global.data());
		};
		float unit[16];identity(unit);for(size_t i=0;i<nodes.size();++i)if(nodes[i].parent<0)visit(int(i),unit);
	}

	static std::vector<float> sampleAnimation(const AnimationSampler &sampler,float time,bool quaternion) {
		if(sampler.input.empty()||!sampler.valuesPerFrame)return {};
		const bool cubic=sampler.interpolation=="CUBICSPLINE";
		const size_t width=cubic?sampler.valuesPerFrame/3:sampler.valuesPerFrame;if(!width)return {};
		const auto upper=std::upper_bound(sampler.input.begin(),sampler.input.end(),time);
		const size_t zero=upper==sampler.input.begin()?0:size_t(upper-sampler.input.begin()-1);
		const size_t one=std::min(zero+1,sampler.input.size()-1);
		const float duration=std::max(1e-6f,sampler.input[one]-sampler.input[zero]);
		float t=zero==one?0:std::clamp((time-sampler.input[zero])/duration,0.0f,1.0f);
		if(sampler.interpolation=="STEP")t=0;
		const size_t offset=cubic?width:0,i0=zero*sampler.valuesPerFrame+offset,i1=one*sampler.valuesPerFrame+offset;
		if(i0+width>sampler.output.size()||i1+width>sampler.output.size())return {};
		std::vector<float> out(width);
		float sign=1,dot=0;if(quaternion&&width==4){for(size_t c=0;c<4;++c)dot+=sampler.output[i0+c]*sampler.output[i1+c];if(dot<0){sign=-1;dot=-dot;}}
		float a=1-t,b=t;
		if(quaternion&&!cubic&&dot<.9995f){const float angle=acosf(std::clamp(dot,-1.0f,1.0f)),s=sinf(angle);a=sinf((1-t)*angle)/s;b=sinf(t*angle)/s;}
		for(size_t c=0;c<width;++c){if(cubic&&zero!=one){const float t2=t*t,t3=t2*t;
				out[c]=(2*t3-3*t2+1)*sampler.output[i0+c]+(t3-2*t2+t)*duration*sampler.output[i0+width+c]+(-2*t3+3*t2)*sampler.output[i1+c]+(t3-t2)*duration*sampler.output[i1-width+c];
			}else out[c]=a*sampler.output[i0+c]+b*sign*sampler.output[i1+c];}
		if(quaternion&&width==4){float length=0;for(float value:out)length+=value*value;if(length>1e-12f){length=sqrtf(length);for(auto &value:out)value/=length;}}
		return out;
	}

	void refreshAnimations() {
		if(!getAnimationStates || animations.empty())return;XrVirtualKeyboardModelAnimationStatesMETA states={XR_TYPE_VIRTUAL_KEYBOARD_MODEL_ANIMATION_STATES_META};
		if(XR_FAILED(getAnimationStates(keyboard,&states)))return;std::vector<XrVirtualKeyboardAnimationStateMETA> values(states.stateCountOutput,{XR_TYPE_VIRTUAL_KEYBOARD_ANIMATION_STATE_META});
		if(!values.empty()){states.stateCapacityInput=uint32_t(values.size());states.states=values.data();if(XR_FAILED(getAnimationStates(keyboard,&states)))return;values.resize(states.stateCountOutput);}
		// The runtime owns the state sequence; apply its channels in order and
		// retain unmodified nodes, as in Meta's reference renderer. In particular
		// each additive weight channel changes just one component after the base
		// animation has supplied the complete weight vector.
		std::vector<bool> dirty(nodes.size(),false);size_t transforms=0,weights=0;
		for(const auto &state:values){if(state.animationIndex<0||state.animationIndex>=int(animations.size()))continue;const auto &animation=animations[state.animationIndex];
			const float time=(animationEnd-animationStart)*std::clamp(state.fraction,0.0f,1.0f);
			for(const auto &channel:animation.channels){if(channel.node<0||channel.node>=int(nodes.size())||channel.sampler<0||channel.sampler>=int(animation.samplers.size()))continue;
				auto sampled=sampleAnimation(animation.samplers[channel.sampler],time,channel.path=="rotation");if(sampled.empty())continue;auto &node=nodes[channel.node];
				if(channel.path=="weights"){
					if(sampled.size()!=node.weights.size())continue;
					if(channel.additiveWeightIndex>=0){const size_t w=size_t(channel.additiveWeightIndex);if(w<node.weights.size())node.weights[w]+=sampled[w];}
					else node.weights=std::move(sampled);dirty[channel.node]=true;++weights;
				}else if(channel.path=="translation"&&sampled.size()==3){std::copy(sampled.begin(),sampled.end(),node.translation.begin());node.matrix=false;++transforms;
				}else if(channel.path=="rotation"&&sampled.size()==4){std::copy(sampled.begin(),sampled.end(),node.rotation.begin());node.matrix=false;++transforms;
				}else if(channel.path=="scale"&&sampled.size()==3){std::copy(sampled.begin(),sampled.end(),node.scale.begin());node.matrix=false;++transforms;}
			}
		}
		if(transforms)recalculateTransforms();
		for(auto &draw:draws)if(draw.vbo&&dirty[draw.node]){const auto &primitive=primitives[draw.primitive];draw.vertices=primitive.baseVertices;const auto &node=nodes[draw.node];
			// Meta's keyboard subset uses 16 independent scalar morphs for
			// each four-vertex quad: eight XY coordinates, then eight UV
			// coordinates. Match VirtualKeyboardModelRenderer::UpdateSurfaceGeo
			// instead of applying a weight to every vertex of the shared mesh.
			for(size_t w=0;w<std::min(node.weights.size(),primitive.targets.size());++w){const float weight=node.weights[w];if(weight==0)continue;const auto &target=primitive.targets[w];
				if(w<8){const size_t vertex=w/2,component=w%2;if(vertex<draw.vertices.size()&&vertex<target.position.size()){
					if(component==0)draw.vertices[vertex].x+=target.position[vertex][0]*weight;else draw.vertices[vertex].y+=target.position[vertex][1]*weight;}
				}else{const size_t scalarIndex=w-8,vertex=scalarIndex/2,component=scalarIndex%2;if(vertex<draw.vertices.size()&&vertex<target.uv.size()){
					if(component==0)draw.vertices[vertex].u+=target.uv[vertex][0]*weight;else draw.vertices[vertex].v+=target.uv[vertex][1]*weight;}}
			}
			xr_glBindBuffer(GL_ARRAY_BUFFER,draw.vbo);xr_glBufferData(GL_ARRAY_BUFFER,draw.vertices.size()*sizeof(Vertex),draw.vertices.data(),GL_DYNAMIC_DRAW);
		}xr_glBindBuffer(GL_ARRAY_BUFFER,0);
		if(!loggedAnimations&&!values.empty()){XR_LOG("Meta keyboard animated state: states=%zu transform-channels=%zu weight-channels=%zu",values.size(),transforms,weights);loggedAnimations=true;}
	}
};
