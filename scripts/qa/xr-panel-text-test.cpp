// GeneralsX @test Codex 14/09/2026 Compile production panel text generation.
#include "XrLayout.h"
#include "XrCommands.h"
#include "XrLayers.h"
#include "XrPerformance.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
using GLuint=unsigned;
struct XrHello {
 XrLayout layout;XrCommandState commands;XrMenuState menu;XrSurface surfaces[3];
 XrPerformance performance;
	bool stereoVisible=false,stereoWorld=false;
	bool recoveryVisible=false;GLuint recoveryTexture=0;
 GLuint uiButtonTexture=0,commandButtonTexture=0,commandsTexture=0,settingsTexture=0,hoverTexture=0;
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
static std::string XrGameBoot_WorldHoverInfo(){return {};}
static bool building=false;
static bool XrGameBoot_CanRotatePlacement(){return building;}
static float XrGameBoot_PlacementDegrees(){return 90;}
static std::string nativeHover;
static std::string XrGameBoot_HoverInfo(float,float){return nativeHover;}
static XrGameRect surfaceRect(int){return {0,0,1,1};}
static int XrGameBoot_GameWidth(){return 1280;}
static int XrGameBoot_GameHeight(){return 720;}
static int checks=0;
static void check(bool v){++checks;if(!v){fprintf(stderr,"panel text check %d failed\n",checks);exit(1);}}
static FILE *output=nullptr;
static int hoverPage=-1;static std::string hoverDetail;
static std::string smallTitle;
static std::string recoveryTitle;
static std::vector<std::string> lines(const std::string &s){
 std::vector<std::string> v;size_t start=0;
 do{size_t end=s.find('\n',start);v.push_back(s.substr(start,end-start));if(end==std::string::npos)break;start=end+1;}while(start<=s.size());return v;
}
static bool paintPanel(XrHello &,GLuint &texture,const std::string &title,const std::string &detail,const std::string &labels,int hover,int kind){
 if(kind==6){hoverPage=hover;hoverDetail=detail;}
 texture=1;auto text=lines(labels);
 if(kind==0)smallTitle=title;
 if(kind==2 && title==xrTr("Darstellung wird wiederhergestellt")){recoveryTitle=title;check(detail==xrTr("Die Spielwelt wird neu gezeichnet. Bitte die Trigger loslassen."));}
 if(kind==1){check(text.size()==22);check(text[18]==xrTr("Fenster") && text[21]==xrTr("Ansicht"));}
 if(kind==3 || kind==5){check(text.size()==44);check(text[19].find(xrTr("Speichern"))!=std::string::npos);check(text[22]=="1 · 0" && text[31]=="10 · 9");check(text[32]=="Communicator" && text[33]==xrTr("Hilfe"));check(text[39]=="A ●" && text[42]=="D ○");}
 if(kind==4){check(text.size()==2);check(text[0]==xrTr("Zurück zu Befehlen") || text[0]==xrTr("Zurück zu Fenstern"));check(detail.find('{')==std::string::npos);}
 if(kind==1 && detail.find("P19")!=std::string::npos){
  check(labels.find("✓ ")!=std::string::npos);
  check(title=="Generals: Zero Hour XR");
 }
 // Android fixture consumes exact strings emitted by production code.
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
  x.recoveryVisible=true;updateMenuTextures(x,100);check(recoveryTitle==xrTr("Darstellung wird wiederhergestellt"));x.recoveryVisible=false;
  x.arranging=true;updateMenuTextures(x,100);check(smallTitle==(lang==XrLanguage::German ? "Fertig":"Done"));
  x.arranging=false;updateMenuTextures(x,100);check(smallTitle=="UI");
  for(int op=1;op<=3;++op){x.commands.groupOperation=op;updateMenuTextures(x,100);}
  x.commands.groupOperation=0;x.commands.tactics=true;updateMenuTextures(x,100);
  available=false;updateMenuTextures(x,100);
  x.commands.input.hover=40;updateMenuTextures(x,100);updateMenuTextures(x,400000000);
  check(x.hoverVisible);available=true;x.commands.bookmarkSave=true;updateMenuTextures(x,100);
  x.commands.help=true;for(int page=0;page<4;++page){x.commands.helpPage=page;updateMenuTextures(x,100);}
  x.menu.open=true;for(int page=0;page<4;++page){x.menu.page=page;updateMenuTextures(x,100);}
  x.performance.volumeShadows=false;x.performance.enabled=true;updateMenuTextures(x,100);
  x.performance.report="B · CPU 15.1 ms · GPU 21.8 ms";updateMenuTextures(x,100);
  x.menu.page=0;x.menu.target=2;updateMenuTextures(x,100);
  x.menu.page=4;for(bool left:{false,true})for(int page=0;page<kXrControllerHelpPages;++page) {
   x.layout.leftHanded=left;x.menu.helpPage=page;updateMenuTextures(x,100);
   const auto help=xrControllerHelp(page,left);
   check(help.find('{')==std::string::npos && help.size()>300);
  }
  x.menu.open=false;building=true;x.pointerVisible=true;x.pointerPiece=1;
  updateMenuTextures(x,100);check(x.hoverVisible && x.hoverKey.find("90°")!=std::string::npos);
  building=false;x.pointerVisible=true;x.pointerPiece=2;x.commands.input.hover=-1;
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
