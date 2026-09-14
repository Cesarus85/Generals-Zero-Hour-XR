// GeneralsX @feature Codex 14/09/2026 Opt-in scene permission, modal preview,
// explicit confirmation. No gameplay input survives entry, exit or tracking loss.
#pragma once
static int scenePermission(XrHello &x,bool request) {
	if(!x.panelEnv || !x.activityRef)return 0;
	auto *env=x.panelEnv;auto cls=env->GetObjectClass(x.activityRef);
	if(!cls){env->ExceptionClear();return 0;}
	const auto method=env->GetMethodID(cls,"scenePermission","(Z)I");
	const int result=method ? env->CallIntMethod(x.activityRef,method,jboolean(request)):0;
	env->DeleteLocalRef(cls);
	if(env->ExceptionCheck()){env->ExceptionClear();return 0;}return result;
}
static void requestSceneData(XrHello &x,bool capture) {
 auto &s=x.scene;
 if(!s.available){s.message="Raumflächen nicht unterstützt";return;}
 if(s.querying || s.capturing || s.permissionPending)return;
 const int permission=scenePermission(x,true);
 if(permission==1){if(capture)s.scan(x.session);else s.refresh(x.session);}
 else if(permission==2){s.permissionPending=true;s.wantsCapture=capture;s.message="Raumfreigabe im System bestätigen";}
 else s.message="Keine Raumfreigabe; manuelles Anordnen bleibt verfügbar";
}
// GeneralsX @feature Codex 14/09/2026 The same preview works in shell and match.
static void beginScenePreview(XrHello &x,bool manual) {
 auto &s=x.scene;
 if(x.loadingPresentation || (x.interactiveGame && (!x.splitVisible || !XrGameBoot_CanAdjustWorld()))) {
  s.message="Während Video oder Laden nicht verfügbar";return;
 }
 if(!manual && (s.querying || s.capturing || s.permissionPending))return;
 if(!manual && (s.filter==0 ? s.tableCount():s.floorCount())==0) {
  s.message=s.filter==0 ? "Kein Tisch gefunden; Raum erfassen oder Höhe manuell setzen":"Kein Boden gefunden; Raum erfassen oder Höhe manuell setzen";return;
 }
 XrGameBoot_CancelTarget();x.grab.cancel();x.buildRotation={};
 x.commands.input.click.cancel();x.menu.click.cancel();
 x.menu.open=false;x.arranging=false;x.controlsArmed=false;x.inputArmed=false;
 s.cancel();s.placing=true;s.manual=manual;s.heightKnown=false;s.workingBoard=x.surfaces[1];
 if(x.roomPoseLost)s.workingBoard.pose.orientation=x.layoutAnchor.orientation;
}
static void sceneFreeWorkspace(XrHello &x) {
 // Explicit action only. Normal starts keep the user's freely arranged layout.
 if(x.roomPoseLost) {
  XrLayout defaults;
  for(int i=0;i<3;++i){x.surfaces[i]=defaults.relative[i];x.surfaces[i].pose=xrPoseMul(x.layoutAnchor,x.surfaces[i].pose);}
  x.roomPoseLost=false;x.layoutDirty=true;saveLayout(x);
 }
 x.scene.cancel();x.scene.placed=false;x.scene.step=XrScene::Step::Done;
 x.scene.message="Freier Tisch aktiv; reale Fläche ist optional";
}
static bool xrSceneMenuAction(XrHello &x,int action) {
 if(x.menu.page!=5)return false;
 auto &s=x.scene;
 if(action==17){s.cancel();x.menu.open=false;x.controlsArmed=false;return true;}
 if(action==16){s.cancel();if(s.step==XrScene::Step::Choice)x.menu.page=0;s.step=XrScene::Step::Choice;return true;}
 if(action<0 || action>5)return true;
 if(s.step==XrScene::Step::Choice) {
  if(action==0)sceneFreeWorkspace(x);
  if(action==1){s.step=XrScene::Step::Surfaces;requestSceneData(x,false);}
  if(action==2)beginScenePreview(x,true);
 } else if(s.step==XrScene::Step::Surfaces) {
  if(action==0 || action==1){s.filter=action;beginScenePreview(x,false);}
  if(action==2)requestSceneData(x,false);
  if(action==3)requestSceneData(x,true);
  if(action==4)beginScenePreview(x,true);
  if(action==5)s.fit=!s.fit;
 } else {
  if(action==0){x.menu.open=false;x.controlsArmed=false;}
  if(action==1)s.step=XrScene::Step::Choice;
  if(action==2)sceneFreeWorkspace(x);
 }
 return true;
}
static void xrSceneMenuText(const XrHello &x,std::string &labels,char *state,size_t size) {
 const auto &s=x.scene;
 std::string buttons[18];buttons[16]="Zurück";buttons[17]="Schließen";
 if(s.step==XrScene::Step::Choice) {
  snprintf(state,size,"%s\n%s",xrTr("1 / 3 · Platzierungsart wählen"),xrTr(x.roomPoseLost ?
   "Raumbezug verloren. Bitte neu platzieren oder freien Tisch wählen.":
   "Ohne Raumscan spielbar. Reale Flächen sind optional; Einrichtung auch vor einer Partie möglich."));
  buttons[0]="Freien Tisch verwenden";buttons[1]="Reale Fläche auswählen";buttons[2]="Höhe manuell festlegen";
  buttons[16]="Zurück zu Fenstern";
 } else if(s.step==XrScene::Step::Surfaces) {
  snprintf(state,size,"%s\n%s\n%s",xrTr("2 / 3 · Fläche auswählen"),xrTr(s.message.c_str()),
   xrTr("Tische und Boden werden in der Vorschau umrandet. Danach Größe und Ausrichtung einstellen."));
  buttons[0]=std::string(xrTr("Tische"))+" · "+std::to_string(s.tableCount());
  buttons[1]=std::string(xrTr("Boden"))+" · "+std::to_string(s.floorCount());
  buttons[2]="Raumdaten neu laden";buttons[3]="Raum neu erfassen";buttons[4]="Höhe manuell festlegen";
  buttons[5]=s.fit ? "Anpassen: AN":"Anpassen: AUS";
 } else {
  snprintf(state,size,"%s\n%s\n%s",xrTr("3 / 3 · Bereit"),xrTr(s.message.c_str()),
   xrTr("Position gilt für diese Sitzung. Nach Neustart keine automatische Bindung an reale Möbel."));
  buttons[0]="Fertig";buttons[1]="Neu platzieren";buttons[2]="Freien Tisch verwenden";
 }
 labels.clear();
 for(int i=0;i<18;++i){if(i)labels+='\n';labels+=buttons[i];}
}
static bool updateScenePlacement(XrHello &x,const XrControllerState &c,XrTime time) {
 auto &s=x.scene;
 if(s.permissionPending) {
  const int permission=scenePermission(x,false);
  if(permission!=2) {
   s.permissionPending=false;
   if(permission==1){if(s.wantsCapture)s.scan(x.session);else s.refresh(x.session);}
   else s.message="Keine Raumfreigabe; manuelles Anordnen bleibt verfügbar";
  }
 }
 if(s.reloadAfterCapture && x.state==XR_SESSION_STATE_FOCUSED) {
  s.reloadAfterCapture=false;s.refresh(x.session);
 }
 if(!s.placing)return false;
 updateControls(x,XrControllerState{},time);
 x.hoverVisible=false;x.pointerVisible=false;x.worldCursorVisible=false;x.buildRotation={};
 x.controlsArmed=false;x.inputArmed=false;
 if(x.state!=XR_SESSION_STATE_FOCUSED || !c.aimValid || x.loadingPresentation ||
  (x.interactiveGame && (!x.splitVisible || !XrGameBoot_CanAdjustWorld() || XrGameBoot_ExpandedUI())) || x.recoveryVisible) {
  s.cancel();x.menu.open=true;x.menu.page=5;s.message="Platzierung unterbrochen; Vorschau neu starten";
  x.rayVisible=false;return true;
 }
 if(c.back || c.grip[0] || c.grip[1]) {
  s.cancel();x.menu.open=true;x.menu.page=5;s.message="Platzierung abgebrochen";return true;
 }
 const float dt=s.previousAimTime ? std::clamp(float(time-s.previousAimTime)*1e-9f,0.0f,.05f):0;
 s.previousAimTime=time;
 const bool neutral=!c.select && !c.buttonsHeld && !c.secondary &&
  fabsf(c.pan.x)<.25f && fabsf(c.pan.y)<.25f && fabsf(c.zoom.x)<.25f && fabsf(c.zoom.y)<.25f;
 if(!s.armed && neutral)s.armed=true;
 if(s.manual && !s.heightKnown) {
  s.message="Controller auf gewünschte Flächenhöhe halten; Trigger merkt die Höhe. Grip: zurück.";
  if(s.armed && c.select && !s.held) {
   s.manualFace.pose={xrAxisAngle({1,0,0},-1.5707963268f),c.aim.position};
   xrSceneRectangle(s.manualFace,-5,-5,10,10);s.heightKnown=true;s.armed=false;
  }
 } else {
  // Preview changes are temporary: cancellation never edits the actual board.
  if(s.armed) {
   if(fabsf(c.zoom.y)>.25f)s.workingBoard.width=std::clamp(s.workingBoard.width*expf(c.zoom.y*dt),.45f,4.0f);
   if(fabsf(c.zoom.x)>.25f)s.workingBoard.pose.orientation=xrMul(xrAxisAngle({0,1,0},-c.zoom.x*dt),s.workingBoard.pose.orientation);
  }
  s.aim(x.session,x.localSpace,time,c.aim,s.workingBoard,surfaceAspect(1));
  if(s.preview && xrLength(xrSub(s.candidate.pose.position,x.layoutAnchor.position))>4.5f) {
   s.preview=false;s.message="Fläche zu weit entfernt";
  }
 }
 x.rayVisible=true;x.rayStart=c.aim.position;
 x.rayEnd=s.distance<6 ? s.hit:xrAdd(c.aim.position,xrRotate(c.aim.orientation,{0,0,-3}));
 x.rayHit=s.preview;x.pointerPressed=false;
 if(s.armed && c.select && !s.held && s.preview) {
  if(x.roomPoseLost) {
   XrLayout defaults;
   for(int i=0;i<3;++i){x.surfaces[i]=defaults.relative[i];x.surfaces[i].pose=xrPoseMul(x.layoutAnchor,x.surfaces[i].pose);}
  }
  xrSceneMoveWorkspace(x.surfaces,s.candidate);x.layout.snap[1]=true;
  x.roomPoseLost=false;x.layoutDirty=true;saveLayout(x);s.cancel();
  s.placed=true;s.step=XrScene::Step::Done;x.menu.open=true;x.menu.page=5;
  s.message="Auf Fläche platziert; frei weiter anpassbar";
  XR_LOG("P19.1 placement confirmed manual=%d width=%.3f height=%.3f",int(s.manual),x.surfaces[1].width,x.surfaces[1].pose.position.y);
  return true;
 }
 s.held=c.select;
 const std::string instructions=s.manual && !s.heightKnown ? "" :
  xrTr("Zeigestick: hoch/runter Größe, links/rechts drehen. Trigger: bestätigen. Grip: zurück.");
 const auto key=std::string(xrTr(s.message.c_str()))+"\n"+instructions;
 if(key!=x.sceneKey && paintPanel(x,x.sceneTexture,xrTr("3 / 3 · Vorschau"),key,"",-1,2))x.sceneKey=key;
 return true;
}
