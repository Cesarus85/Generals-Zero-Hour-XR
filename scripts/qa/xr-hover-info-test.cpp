// GeneralsX @test Codex 14/09/2026 Compile original tooltip evaluation,
// including discounts, prerequisites, queue limits, upgrades and science points.
#include <string>
#include <vector>
#include <set>
#include <cstdio>
#include <cstdlib>
using Int=int;using UnsignedInt=unsigned;using Bool=bool;using ScienceType=int;
constexpr bool TRUE=true,FALSE=false;constexpr int SCIENCE_INVALID=-1,KINDOF_STRUCTURE=1,MAX_BUILD_QUEUE_BUTTONS=9;
enum {GUI_COMMAND_UNIT_BUILD,GUI_COMMAND_PLAYER_UPGRADE,GUI_COMMAND_OBJECT_UPGRADE,GUI_COMMAND_PURCHASE_SCIENCE,GUI_COMMAND_TOGGLE_OVERCHARGE,GUI_COMMAND_SPECIAL_POWER};
using CanMakeType=int;enum {CANMAKE_OK,CANMAKE_NO_MONEY,CANMAKE_QUEUE_FULL,CANMAKE_PARKING_PLACES_FULL,CANMAKE_MAXED_OUT_FOR_PLAYER};
struct AsciiString {
	std::string text;AsciiString(const char *s=""):text(s){}
	const char *str()const{return text.c_str();}bool isNotEmpty()const{return !text.empty();}
};
struct UnicodeString {
	std::wstring text;static const UnicodeString TheEmptyString;
	UnicodeString(const wchar_t *s=L""):text(s){}void clear(){text.clear();}
	bool isEmpty()const{return text.empty();}const wchar_t *str()const{return text.c_str();}
	void concat(const UnicodeString &s){text+=s.text;}void concat(const wchar_t *s){text+=s;}
	void format(const UnicodeString &f,unsigned value){text=f.text+L":"+std::to_wstring(value);}
	void format(const wchar_t *f,const wchar_t *value){const auto next=std::wstring(f)+L":"+value;text=next;}
	bool operator!=(const UnicodeString &o)const{return text!=o.text;}
};
const UnicodeString UnicodeString::TheEmptyString;
struct GameText {
	UnicodeString fetch(const AsciiString &s,bool *exists=nullptr){if(exists)*exists=true;UnicodeString u;u.text.assign(s.text.begin(),s.text.end());return u;}
} gameText;
GameText *TheGameText=&gameText;
struct Player;
struct ProductionPrerequisite {UnicodeString getRequiresList(Player *)const{return L"Barracks";}};
struct ThingTemplate {
	unsigned cost=1000;bool structure=false;
	unsigned calcCostToBuild(Player *)const;
	int getPrereqCount()const{return 1;}
	const ProductionPrerequisite *getNthPrereq(int)const{static ProductionPrerequisite p;return &p;}
	bool isKindOf(int)const{return structure;}
};
struct UpgradeTemplate {unsigned calcCostToBuild(Player *)const;};
struct Player {
	float discount=.75f;bool upgraded=false,inProduction=false;std::set<int> sciences;
	bool hasScience(int s)const{return sciences.count(s)!=0;}
	bool hasUpgradeInProduction(const UpgradeTemplate *)const{return inProduction;}
	bool hasUpgradeComplete(const UpgradeTemplate *)const{return upgraded;}
} player;
unsigned ThingTemplate::calcCostToBuild(Player *p)const{return unsigned(cost*p->discount);}
unsigned UpgradeTemplate::calcCostToBuild(Player *p)const{return unsigned(400*p->discount);}
struct PlayerList {Player *local=&player;Player *getLocalPlayer(){return local;}} playerList;
PlayerList *ThePlayerList=&playerList;
struct OverchargeBehaviorInterface {bool isOverchargeActive(){return true;}};
struct BehaviorModule {OverchargeBehaviorInterface *getOverchargeBehaviorInterface(){return nullptr;}};
struct ProductionUpdateInterface {int count=0;int getProductionCount(){return count;}} production;
struct Object {
	bool upgrade=false,affected=true;ProductionUpdateInterface *getProductionUpdateInterface(){return &production;}
	bool hasUpgrade(const UpgradeTemplate *){return upgrade;}bool affectedByUpgrade(const UpgradeTemplate *){return affected;}
	BehaviorModule **getBehaviorModules(){static BehaviorModule *modules[]={nullptr};return modules;}
} object;
struct Drawable {Object *getObject(){return &object;}} drawable;
struct InGameUI {Drawable *selection=&drawable;Drawable *getFirstSelectedDrawable(){return selection;}} gameUI;
InGameUI *TheInGameUI=&gameUI;
struct BuildAssistant {CanMakeType result=CANMAKE_OK;CanMakeType canMakeUnit(Object *,const ThingTemplate *){return result;}} build;
BuildAssistant *TheBuildAssistant=&build;
struct UpgradeCenter {bool affordable=true;bool canAffordUpgrade(Player *,const UpgradeTemplate *,Bool){return affordable;}} upgrades;
UpgradeCenter *TheUpgradeCenter=&upgrades;
struct ScienceStore {
	void getNameAndDescription(int s,UnicodeString &name,UnicodeString &description){name.text=L"Science"+std::to_wstring(s);description=L"Native science description";}
	unsigned getSciencePurchaseCost(int s){return unsigned(s+1);}
} science;
ScienceStore *TheScienceStore=&science;
struct CommandButton {
	int type=GUI_COMMAND_UNIT_BUILD;const ThingTemplate *thing=nullptr;const UpgradeTemplate *upgrade=nullptr;
	std::vector<int> sciences;
	const ThingTemplate *getThingTemplate()const{return thing;}
	const UpgradeTemplate *getUpgradeTemplate()const{return upgrade;}
	int getCommandType()const{return type;}
	const std::vector<int> &getScienceVec()const{return sciences;}
	AsciiString getTextLabel()const{return "Unit name";}
	AsciiString getDescriptionLabel()const{return "Native description";}
	AsciiString getConflictingLabel()const{return "Conflicting upgrade";}
	AsciiString getPurchasedLabel()const{return "Already purchased";}
};
struct ControlBar {unsigned describeCommand(const CommandButton *,UnicodeString &,UnicodeString &,UnicodeString &)const;};
#include "xr-hover-info.inc"
static int checks=0;
static void check(bool b){++checks;if(!b){fprintf(stderr,"hover info check %d failed\n",checks);exit(1);}}
int main(){
	ControlBar bar;ThingTemplate thing;UpgradeTemplate upgrade;CommandButton cmd;cmd.thing=&thing;
	UnicodeString name,cost,description;
	check(bar.describeCommand(&cmd,name,cost,description)==750);
	check(name.text==L"Unit name" && cost.text==L"TOOLTIP:Cost:750");
	check(description.text.find(L"Barracks")!=std::wstring::npos);
	for(auto reason:{CANMAKE_NO_MONEY,CANMAKE_QUEUE_FULL,CANMAKE_PARKING_PLACES_FULL,CANMAKE_MAXED_OUT_FOR_PLAYER}) {
		build.result=reason;bar.describeCommand(&cmd,name,cost,description);
		check(description.text.find(L"TOOLTIP:Tooltip")!=std::wstring::npos && cost.text==L"TOOLTIP:Cost:750");
	}
	build.result=CANMAKE_OK;player.discount=.5f;check(bar.describeCommand(&cmd,name,cost,description)==500);
	thing.cost=0;check(bar.describeCommand(&cmd,name,cost,description)==0 && cost.isEmpty());
	cmd.thing=nullptr;cmd.upgrade=&upgrade;cmd.type=GUI_COMMAND_PLAYER_UPGRADE;
	check(bar.describeCommand(&cmd,name,cost,description)==200 && cost.text==L"TOOLTIP:Cost:200");
	player.upgraded=true;check(bar.describeCommand(&cmd,name,cost,description)==0 && cost.isEmpty() && description.text==L"Already purchased");
	player.upgraded=false;cmd.type=GUI_COMMAND_OBJECT_UPGRADE;object.affected=false;
	bar.describeCommand(&cmd,name,cost,description);check(description.text==L"Conflicting upgrade" && cost.isEmpty());
	object.affected=true;cmd.sciences={1};bar.describeCommand(&cmd,name,cost,description);
	check(description.text.find(L"CONTROLBAR:GeneralsPromotion")!=std::wstring::npos);
	production.count=MAX_BUILD_QUEUE_BUTTONS;bar.describeCommand(&cmd,name,cost,description);
	check(description.text.find(L"QueueFull")!=std::wstring::npos);
	production.count=0;upgrades.affordable=false;bar.describeCommand(&cmd,name,cost,description);
	check(description.text.find(L"NotEnoughMoney")!=std::wstring::npos);
	cmd.upgrade=nullptr;cmd.type=GUI_COMMAND_PURCHASE_SCIENCE;cmd.sciences={0,1,2};
	check(bar.describeCommand(&cmd,name,cost,description)==1 && cost.text==L"TOOLTIP:ScienceCost:1" && name.text==L"Science0");
	player.sciences.insert(0);check(bar.describeCommand(&cmd,name,cost,description)==2 && name.text==L"Science1");
	cmd.type=GUI_COMMAND_SPECIAL_POWER;check(bar.describeCommand(&cmd,name,cost,description)==0 && cost.isEmpty());
	playerList.local=nullptr;check(bar.describeCommand(&cmd,name,cost,description)==0 && name.isEmpty() && description.isEmpty());
	playerList.local=&player;check(bar.describeCommand(nullptr,name,cost,description)==0 && name.isEmpty());
	check(player.sciences==std::set<int>{0} && !player.upgraded && !player.inProduction);
	printf("PASS %d production native tooltip cost/status/read-only checks\n",checks);
}
