// GeneralsX @test Codex 14/09/2026 Compile production panel text generation.
// GeneralsX @test Ultron 15/09/2026 P21 the settings/commands panels now use
// the structured paint2 contract: every hittable control must carry a valid
// label, every label index must resolve, and states (armed/pending/selected/
// hover/toggles) must appear in the packed table for both languages.
// GeneralsX @test Muse 16/09/2026 Match-result card payloads: localized
// title, shared hint and per-result accent in both languages.
#include "XrLayout.h"
#include "XrCommands.h"
#include "XrEndgame.h"
#include "XrLayers.h"
#include "XrPerformance.h"
#include "XrWorld.h"
#include <vector>
#include <map>
#include <cstdio>
#include <cstdlib>
using GLuint=unsigned;
struct XrHello {
 XrLayout layout;XrCommandState commands;XrMenuState menu;XrSurface surfaces[3];
 XrPerformance performance;
	bool stereoVisible=false,stereoWorld=false;
	bool recoveryVisible=false;GLuint recoveryTexture=0;
	bool resultVisible=false;GLuint resultTexture=0;std::string resultKey;
	XrObserverState observer;GLuint observerHintTexture=0;std::string observerHintKey;
	bool rayVisible=false,rayHit=false;
 GLuint uiButtonTexture=0,groundButtonTexture=0,commandButtonTexture=0,commandsTexture=0,settingsTexture=0,hoverTexture=0;
 std::string commandsKey,settingsKey,hoverCandidate,hoverKey;
 bool splitVisible=true,arranging=false,pointerVisible=false,pointerPressed=false,hoverVisible=false;
 int pointerPiece=0;float pointerU=0,pointerV=0,worldZoom=1;XrTime hoverSince=0;
 int arrangeSlot=1;
};
static bool commandsAvailable(const XrHello &x){return !x.menu.open;}
static std::string XrGameBoot_PresentationStatus(bool stereo,bool){return stereo ? "Stereo 1920x2011 · P12.1":"Flat 1280x720 · Video";}
static std::string XrGameBoot_TacticalHint(){return {};}
static std::string XrGameBoot_TacticalStatus(){return "3 / 1";}
static bool available=true;
static std::string XrGameBoot_TacticalReason(int){return available ? "":xrTr("Zuerst eigene bewegliche Einheiten auswählen");}
static bool XrGameBoot_FormationActive(){return false;}
static bool XrGameBoot_BookmarkKnown(int slot){return slot==0;}
static std::string XrGameBoot_LanguageStatus(){return xrTr("XR sofort; Spieltexte nach Neustart (Sprachdateien nötig)");}
static int XrGameBoot_GroupSize(int group){return group;}
static int tacticMode=0,tacticGroup=0;static bool tacticQueue=false;
static void XrGameBoot_TacticalState(int &mode,int &group,bool &queue){mode=tacticMode;group=tacticGroup;queue=tacticQueue;}
static std::string XrGameBoot_WorldHoverInfo(){return {};}
static bool building=false;
static bool XrGameBoot_CanRotatePlacement(){return building;}
static bool XrGameBoot_CanObserveGround(){return true;}
static float XrGameBoot_PlacementDegrees(){return 90;}
static std::string nativeHover;
static std::string XrGameBoot_HoverInfo(float,float){return nativeHover;}
static XrEndgameResult stubResult=XrEndgameResult::None;
static XrEndgameResult XrGameBoot_MatchResult(){return stubResult;}
static XrGameRect surfaceRect(int){return {0,0,1,1};}
static int XrGameBoot_GameWidth(){return 1280;}
static int XrGameBoot_GameHeight(){return 720;}
static int checks=0;
static void check(bool v){++checks;if(!v){fprintf(stderr,"panel text check %d failed\n",checks);exit(1);}}
static FILE *output=nullptr;
static int hoverPage=-1;static std::string hoverDetail;
static std::string smallTitle;
static std::string recoveryTitle;
static int resultAccent=-1;static std::string resultTitle,resultDetail;
static std::string observerTitle,observerDetail;
static std::string groundButtonTitle;
static std::vector<std::string> lines(const std::string &s){
 std::vector<std::string> v;size_t start=0;
 do{size_t end=s.find('\n',start);v.push_back(s.substr(start,end-start));if(end==std::string::npos)break;start=end+1;}while(start<=s.size());return v;
}
struct Capture {int kind=0;std::string title,detail;std::vector<std::string> labels;std::vector<int> ctrl;};
static std::map<int,Capture> captures;
static void validate2(const Capture &c) {
 check(c.ctrl.size()%8==0 && !c.ctrl.empty());
 const int height=c.kind==5 ? kXrPanelHeightTall:kXrPanelHeight;
 std::vector<int> hitIds;
 for(size_t i=0;i+7<c.ctrl.size();i+=8) {
  const int x=c.ctrl[i+1],y=c.ctrl[i+2],w=c.ctrl[i+3],h=c.ctrl[i+4];
  const int role=c.ctrl[i+5],li=c.ctrl[i+7];
  check(x>=0 && y>=0 && w>0 && h>0 && x+w<=kXrPanelWidth && y+h<=height);
  check(li<0 || li<int(c.labels.size()));
  if(xrPanelRoleHittable(role)) {
   check(li>=0); // every visible hittable control has a label
   hitIds.push_back(c.ctrl[i]);
  } else if(role==kXrRoleSection || role==kXrRoleInfo) {
   check(li>=0); // sections and info lines are never blank
  }
 }
 // Duplicate hit ids would make ray routing ambiguous.
 for(size_t i=0;i<hitIds.size();++i)for(size_t j=i+1;j<hitIds.size();++j)
  check(hitIds[i]!=hitIds[j]);
}
static bool hasLabel(const Capture &c,const std::string &what){for(const auto &s:c.labels)if(s==what)return true;return false;}
static bool hasLabelPrefix(const Capture &c,const std::string &what){for(const auto &s:c.labels)if(s.find(what)==0)return true;return false;}
static int stateOf(const Capture &c,int id){for(size_t i=0;i+7<c.ctrl.size();i+=8)if(c.ctrl[i]==id)return c.ctrl[i+6];return -1;}
static bool paintPanel(XrHello &,GLuint &texture,const std::string &title,const std::string &detail,const std::string &,int hover,int kind){
 if(kind==6){hoverPage=hover;hoverDetail=detail;}
 texture=1;
 if(kind==0)smallTitle=title;
 if(kind==2 && title==xrTr("Darstellung wird wiederhergestellt")){recoveryTitle=title;check(detail==xrTr("Die Spielwelt wird neu gezeichnet. Bitte die Trigger loslassen."));}
 if(kind==2 && hover>0){resultAccent=hover;resultTitle=title;resultDetail=detail;}
 if(kind==2 && title==xrTr("Bodenansicht")){observerTitle=title;observerDetail=detail;}
 if(kind==0 && title==xrTr("BODENANSICHT"))groundButtonTitle=title;
 // Legacy kinds 0/2/6/7 only; the redesigned panels must use paint2.
 check(kind==0 || kind==2 || kind==6 || kind==7);
 if(output){for(const auto &s:{std::to_string(kind),title,detail,std::string()}){fwrite(s.data(),1,s.size(),output);fputc(0,output);}}
 return true;
}
static bool paintPanel2(XrHello &,GLuint &texture,const std::string &title,const std::string &detail,
	const std::string &labels,const std::vector<int> &packed,int kind){
 texture=1;Capture c;c.kind=kind;c.title=title;c.detail=detail;c.labels=lines(labels);c.ctrl=packed;
 validate2(c);captures[kind]=c;
 if(output){for(const auto &s:{std::to_string(kind),title,detail,labels}){fwrite(s.data(),1,s.size(),output);fputc(0,output);}}
 return true;
}
static void xrSceneMenuText(const XrHello &,std::string &,char *,size_t){}
#include "xr-panel-text-bridge.inc"
int main(int argc,char **argv){
 check(argc==2);output=fopen(argv[1],"wb");check(output!=nullptr);
 for(auto lang:{XrLanguage::German,XrLanguage::English}){
  XrHello x;g_xrLanguage=lang;x.layout.language=lang;
  for(int i=0;i<3;++i)x.surfaces[i]=x.layout.relative[i];
  updateMenuTextures(x,100);
  check(groundButtonTitle==xrTr("BODENANSICHT") && x.groundButtonTexture!=0);
  x.recoveryVisible=true;updateMenuTextures(x,100);check(recoveryTitle==xrTr("Darstellung wird wiederhergestellt"));x.recoveryVisible=false;
  // Match-result card: localized title, shared hint and per-result accent.
  x.resultVisible=true;
  stubResult=XrEndgameResult::Victory;updateMenuTextures(x,100);
  check(resultTitle==xrTr("Sieg!") && resultAccent==1);
  stubResult=XrEndgameResult::Defeat;updateMenuTextures(x,100);
  check(resultTitle==xrTr("Niederlage") && resultAccent==2);
  stubResult=XrEndgameResult::MatchOver;updateMenuTextures(x,100);
  check(resultTitle==xrTr("Partie beendet") && resultAccent==3);
  check(resultDetail==xrTr("Die Partie ist entschieden.\nBeliebige Taste zum Schließen."));
  x.resultVisible=false;stubResult=XrEndgameResult::None;
  // P25 armed, invalid-target and active return cards stay bilingual.
  check(x.observer.arm(true));updateMenuTextures(x,100);
  check(observerTitle==xrTr("Bodenansicht") && observerDetail==xrTr("Sichtbaren freien Boden mit Trigger wählen. B/Y bricht ab."));
  x.rayVisible=true;x.rayHit=false;updateMenuTextures(x,100);
  check(observerDetail==xrTr("Hier kein sicherer, sichtbarer Boden. Anderen Ort wählen; B/Y bricht ab."));
  x.observer.neutral(true);check(x.observer.choose({500,500,20},{0,1.6f,0},{0,0,-1}));
  updateMenuTextures(x,100);check(observerDetail==xrTr("Links: gehen · Rechts: drehen · B: Tisch"));
  x.layout.leftHanded=true;updateMenuTextures(x,100);check(observerDetail==xrTr("Links: gehen · Rechts: drehen · Y: Tisch"));
  x.observer.cancel();x.layout.leftHanded=false;x.rayVisible=false;
  x.arranging=true;updateMenuTextures(x,100);check(smallTitle==(lang==XrLanguage::German ? "Fertig":"Done"));
  x.arranging=false;updateMenuTextures(x,100);check(smallTitle=="UI");
  // Commands compact: title, sections, group badges and context detail.
  x.layout.commandsVisible=true;updateMenuTextures(x,100);
  {const auto &c=captures[3];check(c.title==xrTr("Befehle"));
   check(hasLabel(c,xrTr("Auftrag · danach Ziel wählen")));check(hasLabel(c,xrTr("Sofort & Auswahl")));
   check(hasLabel(c,xrTr("Gruppen · Aktion wählen → Zahl")));
   check(hasLabel(c,xrTr("Gruppe ersetzen")));check(hasLabel(c,xrTr("Neu / Erweitern")));
   check(hasLabel(c,xrTr("Bewegen")));check(hasLabel(c,xrTr("STOPP")));check(hasLabel(c,xrTr("Communicator")));
   check(hasLabel(c,"1|0") && hasLabel(c,"10|9"));check(c.detail.find('\n')!=std::string::npos);}
  // Pending group operation, waypoint toggle and armed order are visible bits.
  x.commands.groupOperation=1;updateMenuTextures(x,100);
  check(stateOf(captures[3],30)&kXrStatePending);
  tacticQueue=true;tacticMode=6;updateMenuTextures(x,100);
  check(stateOf(captures[3],8)&kXrStateOn);check(stateOf(captures[3],2)&kXrStateArmed);
  x.commands.groupOperation=0;tacticQueue=false;tacticMode=0;
  // Tactics foldout: disabled reasons, bookmark slots, foldout-only labels.
  x.commands.tactics=true;updateMenuTextures(x,100);
  {const auto &c=captures[5];check(hasLabel(c,xrTr("Taktik · erweitert")));check(hasLabel(c,"A|●") && hasLabel(c,"D|○"));}
  available=false;updateMenuTextures(x,100);
  check(stateOf(captures[5],40)&kXrStateDisabled);check(stateOf(captures[5],41)&kXrStateDisabled);
  available=true;x.commands.bookmarkSave=true;updateMenuTextures(x,100);
  check(stateOf(captures[5],43)&kXrStateOn);x.commands.bookmarkSave=false;
  // The hover explanation still feeds the hover card for tactics hits.
  x.commands.input.hover=40;updateMenuTextures(x,100);updateMenuTextures(x,400000000);
  check(x.hoverVisible);x.commands.input.hover=-1;
  x.commands.tactics=false;
  // Commands help replaces the content in place.
  x.commands.help=true;for(int page=0;page<4;++page){x.commands.helpPage=page;updateMenuTextures(x,100);
   const auto &c=captures[4];check(c.detail.find('{')==std::string::npos);check(c.detail.size()>100);
   check(hasLabel(c,xrTr("Zurück zu Befehlen")));check(c.title.find("/4")!=std::string::npos);}
  x.commands.helpPage=0;updateMenuTextures(x,100);
  check(captures[4].detail.find(xrTr("Neu / Erweitern"))!=std::string::npos);
  x.commands.help=false;
  // Workspace window pages 0..3: active tab marker on every page.
  x.menu.open=true;
  for(int page=0;page<4;++page){x.menu.page=page;updateMenuTextures(x,100);
   const auto &c=captures[1];check(stateOf(c,20+page)&kXrStateActive);}
  {const auto &c=captures[1];
   const std::string expected=std::string(xrTr("Sprache"))+"|"+xrTr(lang==XrLanguage::German ? "Deutsch":"English");
   check(hasLabel(c,expected));check(hasLabel(c,xrTr("Bodenansicht · Ort wählen")));
   check(hasLabel(c,xrTr("Spiel: Tisch; Bodenansicht optional")));}
  for(int tier=0;tier<3;++tier){x.layout.resolutionTier=tier;updateMenuTextures(x,100);
   const char *name=tier==0 ? "Ausgewogen":tier==1 ? "Hoch":"Ultra+";
   check(hasLabel(captures[1],std::string(xrTr("Auflösung"))+"|"+xrTr(name)));}
  x.performance.volumeShadows=true;x.performance.enabled=true;updateMenuTextures(x,100);
  {const auto &c=captures[1];check(hasLabelPrefix(c,std::string(xrTr("Schatten"))+"|"));check(stateOf(c,13)&kXrStateOn);}
  x.performance.report="B · CPU 15.1 ms · GPU 21.8 ms";updateMenuTextures(x,100);
  x.menu.page=0;x.menu.target=2;updateMenuTextures(x,100);
  check(hasLabel(captures[1],xrTr("Alles vor mir ausrichten")));
  x.menu.page=6;updateMenuTextures(x,100);
  check(captures[1].title==xrTr("Spielfläche verlassen?"));
  check(hasLabel(captures[1],xrTr("Verlassen & vor mir ausrichten")));
  check(hasLabel(captures[1],xrTr("Abbrechen")));
  x.menu.page=0;updateMenuTextures(x,100);
  {const auto &c=captures[1];check(stateOf(c,1)&kXrStateSelected);check(hasLabelPrefix(c,std::string(xrTr("Baufenster"))+"|"));}
  x.menu.target=1;updateMenuTextures(x,100);check(stateOf(captures[1],0)&kXrStateSelected);
  // Controller guide pages resolve all placeholders in both handedness modes.
  x.menu.page=4;for(bool left:{false,true})for(int page=0;page<kXrControllerHelpPages;++page) {
   x.layout.leftHanded=left;x.menu.helpPage=page;updateMenuTextures(x,100);
   const auto &c=captures[4];check(c.detail.find('{')==std::string::npos && c.detail.size()>300);
   check(hasLabel(c,xrTr("Zurück zu Fenstern")));
  }
  x.layout.leftHanded=false;x.menu.page=0;
  // Hover is a visible bit on the hovered control only.
  x.menu.hover=3;updateMenuTextures(x,100);
  check(stateOf(captures[1],3)&kXrStateHover);check(!(stateOf(captures[1],4)&kXrStateHover));
  x.menu.hover=-1;
  x.menu.open=false;building=true;x.pointerVisible=true;x.pointerPiece=1;
  updateMenuTextures(x,100);check(x.hoverVisible && x.hoverKey.find("90°")!=std::string::npos);
  building=false;x.pointerVisible=true;x.pointerPiece=2;
  nativeHover="Paladin\nCost: $750\nRequires: Strategy Center";
  updateMenuTextures(x,1000000000);check(!x.hoverVisible);
  nativeHover="Paladin\nCost: $700\nRequires: Strategy Center";
  updateMenuTextures(x,1300000001);check(x.hoverVisible && hoverDetail.find("$700")!=std::string::npos);
  updateMenuTextures(x,11300000001LL);check(hoverPage==1);
  nativeHover="Ranger\nCost: $225";updateMenuTextures(x,12000000000LL);check(!x.hoverVisible);
  updateMenuTextures(x,12400000000LL);check(x.hoverVisible && hoverDetail.find("$225")!=std::string::npos);
  nativeHover.clear();x.pointerVisible=false;
 }
 check(fclose(output)==0);printf("PASS %d production bilingual panel text checks\n",checks);
}
