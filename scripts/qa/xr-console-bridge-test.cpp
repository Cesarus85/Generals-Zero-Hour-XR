// GeneralsX @test Codex 14/09/2026 Actual group and language bridge with spies.
#include "XrTactics.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <set>
#include <string>
static int checks=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"console bridge check %d failed\n",checks);exit(1);}}
constexpr bool FALSE=false;
struct Drawable {XrVector3f point={};const XrVector3f *getPosition(){return &point;}};
struct Object {Drawable draw;Drawable *getDrawable(){return &draw;}};
struct Squad {std::vector<Object*> objects;const std::vector<Object*> &getLiveObjects(){return objects;}};
struct Player {Squad squads[10];Squad *getHotkeySquad(int g){check(g>=0 && g<10);return &squads[g];}} player;
struct Players {Player *getLocalPlayer(){return &player;}} players;
static Players *ThePlayerList=&players;
struct UI {int selected=0;bool placing=false,queue=false;int getSelectCount(){return selected;}const void *getPendingPlaceType(){return placing ? this:nullptr;}void setWaypointMode(bool value){queue=value;}} ui;
static UI *TheInGameUI=&ui;
struct View {int calls=0;void userLookAt(const XrVector3f *){++calls;}} view;
static View *TheTacticalView=&view;
struct GameMessage {enum Type {MSG_META_VIEW_COMMAND_CENTER=50,MSG_META_SELECT_TEAM0=100,MSG_META_CREATE_TEAM0=200,MSG_META_ADD_TEAM0=300};};
struct Stream {std::vector<int> messages;void appendMessage(GameMessage::Type type){messages.push_back(type);}} stream;
static Stream *TheMessageStream=&stream;
static bool allowed=true;
static bool XrGameBoot_CanAdjustWorld(){return allowed;}
static bool expanded=false;
static bool XrGameBoot_ExpandedUI(){return expanded;}
namespace TouchInput {static int cancels=0;static void backOutOfArmedState(){++cancels;}}
static int diplomacyCalls=0;
static void ToggleDiplomacy(bool){++diplomacyCalls;}
static XrTactics s_tactics;
static const char *s_groupNotice="",*s_languageNotice="";
static std::string s_textLanguagePath;
struct FS {std::set<std::string> files;bool doesFileExist(const char *name){return files.count(name)>0;}} fs;
static FS *TheFileSystem=&fs;
int XrGameBoot_GroupSize(int);
#include "xr-console-bridge.inc"
int main(int argc,char **argv) {
 check(argc==2);s_textLanguagePath=argv[1];Object object;
 // The bridge emits only the original local view message, without selection
 // writes or canceling a pending order. Blocked contexts emit nothing.
 ui.selected=3;check(XrGameBoot_ViewBase());
 check(stream.messages.size()==1 && stream.messages.back()==GameMessage::MSG_META_VIEW_COMMAND_CENTER);
 check(ui.selected==3 && TouchInput::cancels==0);
 allowed=false;check(!XrGameBoot_ViewBase());allowed=true;
 expanded=true;check(!XrGameBoot_ViewBase());expanded=false;
 ui.placing=true;check(!XrGameBoot_ViewBase());ui.placing=false;
 TheMessageStream=nullptr;check(!XrGameBoot_ViewBase());TheMessageStream=&stream;
 TheInGameUI=nullptr;check(!XrGameBoot_ViewBase());TheInGameUI=&ui;
 check(stream.messages.size()==1);stream.messages.clear();
 for(int group=0;group<10;++group) {
  player.squads[group].objects={};ui.selected=3;const auto before=stream.messages.size();
  for(int op:{0,3}){XrGameBoot_TacticalGroup(group,op);check(stream.messages.size()==before);check(std::strstr(s_groupNotice,"leer"));}
  XrGameBoot_TacticalGroup(group,2);check(stream.messages.size()==before+1 && stream.messages.back()==200+group);
  const auto afterEmptyExtend=stream.messages.size();
  ui.selected=0;XrGameBoot_TacticalGroup(group,1);check(stream.messages.size()==afterEmptyExtend);
  ui.selected=3;XrGameBoot_TacticalGroup(group,1);check(stream.messages.back()==200+group);
  player.squads[group].objects={&object};check(XrGameBoot_GroupSize(group)==1);
  XrGameBoot_TacticalGroup(group,0);check(stream.messages.back()==100+group);
  const auto extend=stream.messages.size();XrGameBoot_TacticalGroup(group,2);
  check(stream.messages.size()==extend+2 && stream.messages[extend]==300+group && stream.messages.back()==200+group);
  const int views=view.calls;XrGameBoot_TacticalGroup(group,3);check(view.calls==views+1);
 }
 const auto messages=stream.messages.size();const auto views=view.calls;
 allowed=false;for(int op=0;op<4;++op)XrGameBoot_TacticalGroup(0,op);
 XrGameBoot_Communicator();check(stream.messages.size()==messages && view.calls==views && diplomacyCalls==0);
 allowed=true;XrGameBoot_Communicator();check(diplomacyCalls==1 && s_tactics.mode==XrOrderMode::Context);
 XrGameBoot_TacticalGroup(-1,0);XrGameBoot_TacticalGroup(10,0);XrGameBoot_TacticalGroup(0,4);check(stream.messages.size()==messages);
 check(XrGameBoot_GroupSize(-1)==0 && XrGameBoot_GroupSize(10)==0);
 // Missing pack does not overwrite an existing language choice or force text.
 XrGameBoot_SetLanguage(0);check(g_xrLanguage==XrLanguage::German);check(std::strstr(XrGameBoot_LanguageStatus().c_str(),"fehlen"));
 check(fopen(argv[1],"r")==nullptr);
 fs.files.insert("data/english/generals.csf");XrGameBoot_SetLanguage(1);
 check(g_xrLanguage==XrLanguage::English);check(std::strstr(XrGameBoot_LanguageStatus().c_str(),"restart"));
 FILE *marker=fopen(argv[1],"r");check(marker!=nullptr);char token[64]={};check(fscanf(marker,"%63s",token)==1);fclose(marker);check(!strcmp(token,"english"));
 XrGameBoot_SetLanguage(0);marker=fopen(argv[1],"r");check(marker!=nullptr);check(fscanf(marker,"%63s",token)==1);fclose(marker);check(!strcmp(token,"english"));
 fs.files.insert("data/german/generals.str");XrGameBoot_SetLanguage(0);
 marker=fopen(argv[1],"r");check(marker!=nullptr);check(fscanf(marker,"%63s",token)==1);fclose(marker);check(!strcmp(token,"german"));
 XrGameBoot_SetLanguage(7);check(g_xrLanguage==XrLanguage::German);
 s_textLanguagePath="";XrGameBoot_SetLanguage(1);check(std::strstr(XrGameBoot_LanguageStatus().c_str(),"could not save"));
 std::set<std::string> keys;
 for(const auto &entry:kXrTranslations) {
  check(keys.insert(entry.de).second);check(*entry.de && *entry.en);
  g_xrLanguage=XrLanguage::German;check(!strcmp(xrTr(entry.de),entry.de));
  g_xrLanguage=XrLanguage::English;check(!strcmp(xrTr(entry.de),entry.en));
 }
 check(xrLines("Speichern\nHilfe")=="Save selection\nHelp");
 for(int p=0;p<3;++p){g_xrLanguage=XrLanguage::German;std::string de=xrCommandHelp(p);g_xrLanguage=XrLanguage::English;check(de!=xrCommandHelp(p));check(strlen(xrCommandHelp(p))>100);}
 remove(argv[1]);printf("PASS %d production console/group/language checks\n",checks);
}
