// GeneralsX @test Codex 14/09/2026 Production tactical functions with spies.
#include "XrTactics.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
static int checks=0;
static void check(bool ok){++checks;if(!ok){fprintf(stderr,"tactical bridge check %d failed\n",checks);exit(1);}}
constexpr int ALLIES=1,KINDOF_STRUCTURE=1,KINDOF_SELECTABLE=2;
using FormationID=int;constexpr int NO_FORMATION_ID=0;
using Coord3D=XrVector3f;using PickType=int;
constexpr int GUARDMODE_NORMAL=0,GUARDMODE_GUARD_WITHOUT_PURSUIT=1;
struct Object {
 bool local=true,dead=false,contained=false,offMap=false,structure=false,ai=true;int formation=0,id=123,team=ALLIES;
 bool isLocallyControlled(){return local;}bool isEffectivelyDead(){return dead;}bool isContained(){return contained;}
 bool isOffMap(){return offMap;}bool isKindOf(int){return structure;}void *getAIUpdateInterface(){return ai ? this:nullptr;}
 int getFormationID(){return formation;}int getID(){return id;}int getTeam(){return team;}
};
struct Drawable {Object *object=nullptr;bool shroud=false,selected=false;Object *getObject(){return object;}
 bool getFullyObscuredByShroud(){return shroud;}bool isSelected(){return selected;}};
struct KindOfMaskType {void set(int){}};
struct UI {
 std::vector<Drawable*> selection;bool queue=false,force=false,move=false,attack=false;
 int getSelectCount(){return int(selection.size());}auto *getAllSelectedDrawables(){return &selection;}
 bool isInWaypointMode(){return queue;}bool isInForceAttackMode(){return force;}
 bool isInForceMoveToMode(){return move;}bool isInAttackMoveToMode(){return attack;}
 void setWaypointMode(bool v){queue=v;}void setForceAttackMode(bool v){force=v;}void setForceMoveMode(bool v){move=v;}
 void clearAttackMoveToMode(){attack=false;}void toggleAttackMoveToMode(){attack=!attack;}
 void selectMatchingAcrossMap(){}void selectAllUnitsByTypeAcrossMap(KindOfMaskType,KindOfMaskType){}
} ui;
static UI *TheInGameUI=&ui;
struct Player {int getRelationship(int team){return team;}} player;
struct Players {Player *getLocalPlayer(){return &player;}} players;
static Players *ThePlayerList=&players;
struct GameMessage {
 enum Type {MSG_INVALID,MSG_DO_GUARD_OBJECT,MSG_DO_GUARD_POSITION,MSG_META_CREATE_FORMATION,MSG_META_STOP,
 MSG_META_SCATTER,MSG_META_SELECT_NEXT_IDLE_WORKER,MSG_META_SELECT_NEXT_UNIT,MSG_META_SELECT_NEXT_WORKER,
 MSG_META_SELECT_HERO,MSG_META_SELECT_ALL_AIRCRAFT};
 Type type=MSG_INVALID;int object=-1,mode=-1;Coord3D location={};
 void appendObjectIDArgument(int v){object=v;}void appendIntegerArgument(int v){mode=v;}void appendLocationArgument(Coord3D v){location=v;}
};
struct Stream {std::vector<GameMessage> messages;GameMessage *appendMessage(GameMessage::Type type){messages.push_back({});messages.back().type=type;return &messages.back();}} stream;
static Stream *TheMessageStream=&stream;
static int voices=0;static void pickAndPlayUnitVoiceResponse(const std::vector<Drawable*> *,GameMessage::Type){++voices;}
struct ViewLocation {int value=0;};
struct View {Drawable *picked=nullptr;int camera=42,recalls=0;
 Drawable *pickDrawable(const int *,bool,PickType){return picked;}
 void getLocation(ViewLocation *v){v->value=camera;}void userSetLocation(const ViewLocation *v){camera=v->value;++recalls;}
} view;
static View *TheTacticalView=&view;static int s_activePixel=0;
namespace CommandTranslator {constexpr int DO_COMMAND=1;}
static int getPickTypesForContext(bool){return 1;}
struct Client {int calls=0;bool queue=false,force=false,move=false,attack=false;Drawable *target=nullptr;
 void evaluateContextCommand(Drawable *d,const Coord3D *,int){++calls;target=d;queue=ui.queue;force=ui.force;move=ui.move;attack=ui.attack;}
} client;
static Client *TheGameClient=&client;
static bool allowed=true,controllable=true;
static bool XrGameBoot_CanAdjustWorld(){return allowed;}
namespace TouchInput {static int cancels=0;static void backOutOfArmedState(){++cancels;}
 static void cancelOrDeselect(){++cancels;}static bool hasControllableSelection(){return controllable && !ui.selection.empty();}}
static void XrGameBoot_TacticalGroup(int,int){}
static XrTactics s_tactics;static XrTriggerGesture s_triggerGesture;static bool s_triggerPreview=false;
static const char *s_groupNotice="";static ViewLocation s_bookmarks[4];static bool s_bookmarkKnown[4]={};static unsigned s_bookmarkFrame=0;
#include "xr-tactical-bridge.inc"
int main(){
 Object a,b;Drawable da{&a},db{&b};ui.selection={&da,&db};Coord3D ground={3,4,5};
 for(int flags=0;flags<16;++flags)for(auto mode:{XrOrderMode::Move,XrOrderMode::AttackMove,XrOrderMode::ForceAttack,XrOrderMode::ForceMove,XrOrderMode::Context})for(bool queue:{false,true}) {
  ui.queue=flags&1;ui.force=flags&2;ui.move=flags&4;ui.attack=flags&8;view.picked=&db;
  xrIssueMovement(mode,ground,queue);
  check(client.move==(mode==XrOrderMode::ForceMove));check(client.force==(mode==XrOrderMode::ForceAttack));
  check(client.attack==(mode==XrOrderMode::AttackMove));check(client.queue==(queue && (mode==XrOrderMode::Context || mode==XrOrderMode::Move)));
  check((client.target==nullptr)==(mode==XrOrderMode::Move || mode==XrOrderMode::AttackMove || mode==XrOrderMode::ForceMove));
  check(ui.queue==bool(flags&1) && ui.force==bool(flags&2) && ui.move==bool(flags&4) && ui.attack==bool(flags&8));
 }
 check(xrIssueGuard(nullptr,ground,true,false));check(stream.messages.back().type==GameMessage::MSG_DO_GUARD_POSITION);
 check(stream.messages.back().mode==GUARDMODE_NORMAL && stream.messages.back().location.x==3);
 check(xrIssueGuard(&db,ground,false,false));check(stream.messages.back().type==GameMessage::MSG_DO_GUARD_OBJECT && stream.messages.back().object==123);
 check(xrIssueGuard(&db,ground,true,true));check(stream.messages.back().mode==GUARDMODE_GUARD_WITHOUT_PURSUIT);
 for(int invalid=0;invalid<7;++invalid){
  b=Object{};db.shroud=invalid==0;db.selected=invalid==1;b.dead=invalid==2;b.contained=invalid==3;b.offMap=invalid==4;b.team=invalid==5 ? 2:ALLIES;
  if(invalid==6)ThePlayerList=nullptr;
  const auto count=stream.messages.size();check(!xrIssueGuard(&db,ground,true,false));check(stream.messages.size()==count);ThePlayerList=&players;
 }
 db.shroud=db.selected=false;b=Object{};check(!xrIssueGuard(nullptr,ground,false,true));
 check(XrGameBoot_TacticalReason(40).empty());check(!XrGameBoot_FormationActive());
 XrGameBoot_TacticalAction(40);check(stream.messages.back().type==GameMessage::MSG_META_CREATE_FORMATION);
 a.formation=b.formation=7;check(XrGameBoot_FormationActive());check(XrGameBoot_TacticalReason(40).empty());
 b.formation=8;check(!XrGameBoot_FormationActive());check(!XrGameBoot_TacticalReason(40).empty());
 a.formation=b.formation=0;ui.selection={&da};check(!XrGameBoot_TacticalReason(40).empty());a.formation=7;check(XrGameBoot_TacticalReason(40).empty());
 a.structure=true;check(!XrGameBoot_TacticalReason(41).empty());a.structure=false;a.local=false;check(!XrGameBoot_TacticalReason(42).empty());a.local=true;
 for(int action:{41,42}){s_tactics.queue=true;XrGameBoot_TacticalAction(action);check(!s_tactics.queue);check(s_tactics.mode==(action==41 ? XrOrderMode::ForceMove:XrOrderMode::GuardHold));}
 XrGameBoot_TacticalAction(9);check(s_tactics.queue && s_tactics.mode==XrOrderMode::Move);
 XrGameBoot_TacticalAction(8);check(!s_tactics.queue && s_tactics.mode==XrOrderMode::Guard);
 ui.selection.clear();const auto count=stream.messages.size();XrGameBoot_TacticalAction(40);check(stream.messages.size()==count);
 for(int slot=0;slot<4;++slot){
  check(!XrGameBoot_BookmarkKnown(slot));const int calls=view.recalls;XrGameBoot_Bookmark(slot,false);check(view.recalls==calls);
  view.camera=100+slot;XrGameBoot_Bookmark(slot,true);check(XrGameBoot_BookmarkKnown(slot));view.camera=0;
  XrGameBoot_Bookmark(slot,false);check(view.camera==100+slot && view.recalls==calls+1);
 }
 allowed=false;const int recalls=view.recalls;XrGameBoot_Bookmark(0,false);check(view.recalls==recalls);check(!XrGameBoot_TacticalReason(40).empty());allowed=true;
 check(!XrGameBoot_BookmarkKnown(-1) && !XrGameBoot_BookmarkKnown(4));
 xrUpdateBookmarkSession(true,100);check(XrGameBoot_BookmarkKnown(0));xrUpdateBookmarkSession(true,99);check(!XrGameBoot_BookmarkKnown(0));
 XrGameBoot_Bookmark(0,true);xrUpdateBookmarkSession(false,100);check(!XrGameBoot_BookmarkKnown(0));
 s_tactics.setMode(XrOrderMode::ForceMove);s_tactics.queue=true;s_triggerGesture.active=true;s_triggerPreview=true;
 XrGameBoot_CancelTarget();check(s_tactics.mode==XrOrderMode::Context && !s_tactics.queue && !s_triggerGesture.active && !s_triggerPreview);
 printf("PASS %d production tactical/native-message checks\n",checks);
}
