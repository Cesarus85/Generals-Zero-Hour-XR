// GeneralsX @feature Codex 14/09/2026 Optional native OpenXR scene adapter.
// Own queried handles until refresh/shutdown; no querying in the render hot path.
#pragma once
#include "XrSceneSurface.h"
struct XrScene {
	struct Entry {XrSpace space=XR_NULL_HANDLE;bool floor=false,volume=false,locateRequested=false;
		XrRect3DfFB box={};std::vector<XrVector2f> boundary;};
	std::vector<Entry> entries;
	PFN_xrQuerySpacesFB query=nullptr;
	PFN_xrRetrieveSpaceQueryResultsFB retrieve=nullptr;
	PFN_xrGetSpaceComponentStatusFB status=nullptr;
	PFN_xrSetSpaceComponentStatusFB enable=nullptr;
	PFN_xrGetSpaceSemanticLabelsFB labels=nullptr;
	PFN_xrGetSpaceBoundingBox2DFB box2=nullptr;
	PFN_xrGetSpaceBoundingBox3DFB box3=nullptr;
	PFN_xrGetSpaceBoundary2DFB boundary=nullptr;
	PFN_xrRequestSceneCaptureFB capture=nullptr;
	XrAsyncRequestIdFB queryId=0,captureId=0;
	bool querying=false,capturing=false,available=false,placing=false,preview=false,fit=true;
	// GeneralsX @feature Codex 14/09/2026 Optional ordered setup, never auto-scan.
	enum class Step {Choice,Surfaces,Done};
	Step step=Step::Choice;
	bool manual=false,heightKnown=false,detected=false,placed=false,reloadAfterCapture=false;
	XrSceneFace manualFace;
	std::vector<XrSceneFace> visibleFaces;
	XrTime previousAimTime=0;
	int tableCount() const {return int(std::count_if(entries.begin(),entries.end(),[](const Entry &e){return !e.floor;}));}
	int floorCount() const {return int(entries.size())-tableCount();}
	bool wantsCapture=false,permissionPending=false,armed=false,held=false;
	int filter=0; // 0 tables, 1 floor
	XrSurface candidate,workingBoard;XrVector3f hit={};float distance=0;
	std::string message="Raumdaten noch nicht geladen";
	template<class T> static void function(XrInstance instance,const char *name,T &out) {
		if(XR_FAILED(xrGetInstanceProcAddr(instance,name,reinterpret_cast<PFN_xrVoidFunction *>(&out))))out=nullptr;
	}
	void init(XrInstance instance,bool supported,bool canCapture) {
		if(!supported){message="Raumflächen nicht unterstützt";return;}
		function(instance,"xrQuerySpacesFB",query);function(instance,"xrRetrieveSpaceQueryResultsFB",retrieve);
		function(instance,"xrGetSpaceComponentStatusFB",status);function(instance,"xrSetSpaceComponentStatusFB",enable);
		function(instance,"xrGetSpaceSemanticLabelsFB",labels);function(instance,"xrGetSpaceBoundingBox2DFB",box2);
		function(instance,"xrGetSpaceBoundingBox3DFB",box3);function(instance,"xrGetSpaceBoundary2DFB",boundary);
		if(canCapture)function(instance,"xrRequestSceneCaptureFB",capture);
		available=query && retrieve && status && enable && labels && box2 && box3 && boundary;
		if(!available)message="Raumflächen nicht unterstützt";
	}
	void clear() {for(const auto &e:entries)if(e.space)xrDestroySpace(e.space);entries.clear();preview=false;visibleFaces.clear();}
	void cancel() {placing=false;preview=false;detected=false;armed=false;held=false;permissionPending=false;previousAimTime=0;distance=6;visibleFaces.clear();}
	bool enabled(XrSpace space,XrSpaceComponentTypeFB component) {
		XrSpaceComponentStatusFB s={XR_TYPE_SPACE_COMPONENT_STATUS_FB};
		return XR_SUCCEEDED(status(space,component,&s)) && s.enabled;
	}
	void refresh(XrSession session) {
		if(!available || querying || capturing)return;
		clear();
		XrSpaceStorageLocationFilterInfoFB storage={XR_TYPE_SPACE_STORAGE_LOCATION_FILTER_INFO_FB,nullptr,XR_SPACE_STORAGE_LOCATION_LOCAL_FB};
		XrSpaceComponentFilterInfoFB filterInfo={XR_TYPE_SPACE_COMPONENT_FILTER_INFO_FB,&storage,XR_SPACE_COMPONENT_TYPE_SEMANTIC_LABELS_FB};
		XrSpaceQueryInfoFB info={XR_TYPE_SPACE_QUERY_INFO_FB};
		info.queryAction=XR_SPACE_QUERY_ACTION_LOAD_FB;info.maxResultCount=256;info.timeout=5000000000;
		info.filter=reinterpret_cast<XrSpaceFilterInfoBaseHeaderFB *>(&filterInfo);
		querying=XR_SUCCEEDED(query(session,reinterpret_cast<XrSpaceQueryInfoBaseHeaderFB *>(&info),&queryId));
		message=querying ? "Raumdaten werden geladen":"Raumabfrage fehlgeschlagen";
	}
	void scan(XrSession session) {
		if(!capture || querying || capturing){message="Raumaufnahme nicht verfügbar";return;}
		XrSceneCaptureRequestInfoFB info={XR_TYPE_SCENE_CAPTURE_REQUEST_INFO_FB};
		capturing=XR_SUCCEEDED(capture(session,&info,&captureId));
		message=capturing ? "Raumaufnahme im System abschließen":"Raumaufnahme fehlgeschlagen";
	}
	void results(XrSession session) {
		XrSpaceQueryResultsFB r={XR_TYPE_SPACE_QUERY_RESULTS_FB};
		if(XR_FAILED(retrieve(session,queryId,&r)) || !r.resultCountOutput)return;
		std::vector<XrSpaceQueryResultFB> found(r.resultCountOutput);
		r.resultCapacityInput=uint32_t(found.size());r.results=found.data();
		if(XR_FAILED(retrieve(session,queryId,&r)))return;
		for(uint32_t i=0;i<std::min(r.resultCountOutput,r.resultCapacityInput);++i) {
			const auto space=found[i].space;
			if(std::any_of(entries.begin(),entries.end(),[&](const Entry &e){return e.space==space;}))continue;
			Entry e;e.space=space;
			char text[1024]={};XrSemanticLabelsFB l={XR_TYPE_SEMANTIC_LABELS_FB,nullptr,1023,0,text};
			if(XR_FAILED(labels(session,space,&l))){xrDestroySpace(space);continue;}
			const std::string label(text);
			auto has=[&](const char *token){return (","+label+",").find(std::string(",")+token+",")!=std::string::npos;};
			e.floor=has("FLOOR");
			if(!e.floor && !has("TABLE") && !has("DESK")){xrDestroySpace(space);continue;}
			// Prefer the actual tabletop plane, independent of clutter volume.
			if(box2 && boundary && enabled(space,XR_SPACE_COMPONENT_TYPE_BOUNDED_2D_FB)) {
				XrBoundary2DFB b={XR_TYPE_BOUNDARY_2D_FB};
				if(XR_SUCCEEDED(boundary(session,space,&b)) && b.vertexCountOutput>=3 && b.vertexCountOutput<=4096) {
					e.boundary.resize(b.vertexCountOutput);b.vertexCapacityInput=uint32_t(e.boundary.size());b.vertices=e.boundary.data();
					if(XR_FAILED(boundary(session,space,&b)))e.boundary.clear();
					else e.boundary.resize(std::min(b.vertexCountOutput,b.vertexCapacityInput));
				}
				if(e.boundary.empty()) {
					XrRect2Df r2={};
					if(XR_SUCCEEDED(box2(session,space,&r2)) && r2.extent.width>0 && r2.extent.height>0) {
						XrSceneFace f;xrSceneRectangle(f,r2.offset.x,r2.offset.y,r2.extent.width,r2.extent.height);e.boundary=f.boundary;
					}
				}
			}
			if(e.boundary.size()<3) {
				e.volume=!e.floor && box3 && enabled(space,XR_SPACE_COMPONENT_TYPE_BOUNDED_3D_FB) &&
					XR_SUCCEEDED(box3(session,space,&e.box));
				if(!e.volume){xrDestroySpace(space);continue;}
			}
			entries.push_back(e);
		}
	}
	void event(XrSession session,const XrEventDataBuffer &ev) {
		if(ev.type==XR_TYPE_EVENT_DATA_SPACE_QUERY_RESULTS_AVAILABLE_FB) {
			const auto &e=reinterpret_cast<const XrEventDataSpaceQueryResultsAvailableFB &>(ev);
			if(querying && e.requestId==queryId)results(session);
		} else if(ev.type==XR_TYPE_EVENT_DATA_SPACE_QUERY_COMPLETE_FB) {
			const auto &e=reinterpret_cast<const XrEventDataSpaceQueryCompleteFB &>(ev);
			if(querying && e.requestId==queryId) {
				querying=false;
				if(XR_FAILED(e.result)){clear();message="Raumabfrage fehlgeschlagen";}
				else message=entries.empty() ? "Keine Tische oder Böden gefunden":"Raumdaten bereit";
			}
		} else if(ev.type==XR_TYPE_EVENT_DATA_SCENE_CAPTURE_COMPLETE_FB) {
			const auto &e=reinterpret_cast<const XrEventDataSceneCaptureCompleteFB &>(ev);
			if(capturing && e.requestId==captureId) {
				capturing=false;reloadAfterCapture=XR_SUCCEEDED(e.result);
				message=reloadAfterCapture ? "Raumaufnahme fertig; Daten neu laden":"Raumaufnahme abgebrochen";
			}
		}
	}
	void aim(XrSession,XrSpace local,XrTime time,const XrPosef &ray,const XrSurface &board,float aspect) {
		preview=false;detected=false;distance=6;visibleFaces.clear();bool tooSmall=false,located=false;
		if(manual && heightKnown)visibleFaces.push_back(manualFace);
		for(auto &e:entries) {
			if(manual)break;
			if(e.floor!=(filter==1))continue;
			XrSpaceComponentStatusFB s={XR_TYPE_SPACE_COMPONENT_STATUS_FB};
			if(XR_FAILED(status(e.space,XR_SPACE_COMPONENT_TYPE_LOCATABLE_FB,&s)))continue;
			if(!s.enabled) {
				if(!s.changePending && !e.locateRequested) {
					e.locateRequested=true;
					XrSpaceComponentStatusSetInfoFB info={XR_TYPE_SPACE_COMPONENT_STATUS_SET_INFO_FB,nullptr,XR_SPACE_COMPONENT_TYPE_LOCATABLE_FB,XR_TRUE,0};
					XrAsyncRequestIdFB id=0;enable(e.space,&info,&id);
				}
				continue;
			}
			XrSpaceLocation loc={XR_TYPE_SPACE_LOCATION};
			const XrSpaceLocationFlags flags=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|
				XR_SPACE_LOCATION_POSITION_TRACKED_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
			if(XR_FAILED(xrLocateSpace(e.space,local,time,&loc)) || (loc.locationFlags&flags)!=flags)continue;
			located=true;XrSceneFace face;face.floor=e.floor;
			if(e.volume){if(!xrSceneTop(loc.pose,e.box,face))continue;}
			else {
				face.pose=loc.pose;face.boundary=e.boundary;
				if(xrRotate(face.pose.orientation,{0,0,1}).y<0) {
					face.pose.orientation=xrMul(face.pose.orientation,xrAxisAngle({1,0,0},3.1415926536f));
					for(auto &v:face.boundary)v.y=-v.y;
				}
			}
			if(xrRotate(face.pose.orientation,{0,0,1}).y>=.985f)visibleFaces.push_back(face);
		}
		for(const auto &face:visibleFaces) {
			XrVector3f point;float t=0;
			if(!xrSceneHit(face,ray,point,t) || t>=distance)continue;
			auto next=xrSceneSupportedBoard(face,board,point,board.width,aspect);
			if(!xrSceneFits(face,next,aspect) && fit) {
				float lo=.45f,hi=board.width;
				for(int i=0;i<12;++i){float mid=(lo+hi)*.5f;
					if(xrSceneFits(face,xrSceneSupportedBoard(face,board,point,mid,aspect),aspect))lo=mid;else hi=mid;}
				next=xrSceneSupportedBoard(face,board,point,lo,aspect);
			}
			distance=t;hit=point;detected=true;
			preview=xrSceneFits(face,next,aspect);candidate=next;tooSmall=!preview;
		}
		message=preview ? "Vorschau: Trigger bestätigen · Grip abbrechen":
			tooSmall ? "Zu wenig Platz: mittig zielen oder Anpassen aktivieren":
			(manual || located) ? "Auf die gewählte Fläche zeigen · Grip abbrechen":"Flächen noch nicht lokalisiert · Grip abbrechen";
	}
};
