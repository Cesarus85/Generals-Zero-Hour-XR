// GeneralsX @feature Ultron 15/09/2026 P21 single layout source for the UI
// and Commands panels: one native control table drives Canvas pixels (via
// JNI paint2), UV hit testing and state rendering. Java renders primitives
// from this table only; it owns no geometry, so pixels and hit regions
// cannot drift apart. Rectangles are Canvas pixels (768 wide; 1024 tall,
// 1280 with the commands tactics foldout). Pure C++17, host-testable.
#pragma once
#include <cmath>
#include <string>
#include <vector>
struct XrPanelControl {
	int id;    // action/hit id consumed by applyMenuAction/applyCommandAction; -1 decoration
	int x,y,w,h;
	int role;  // XrPanelRole
	int state; // XrPanelState bits
	int label; // index into the labels array; -1 = no label
};
enum XrPanelRole {
	kXrRoleButton=0,   // neutral button
	kXrRoleTargeted=1, // order that arms table targeting (chevron)
	kXrRoleImmediate=2,// acts instantly, filled emphasis
	kXrRoleClose=3,    // close/X
	kXrRoleSection=4,  // section header, not hittable
	kXrRoleContext=5,  // status/context card, not hittable
	kXrRoleGroupNum=6, // group number key with count badge
	kXrRoleTab=7,      // page tab
	kXrRoleInfo=8,     // plain information line, not hittable
	kXrRoleBody=9,     // long-text body region (help), not hittable
	kXrRoleCard=10,    // large target card (edit-target picker)
	kXrRoleOp=11,      // group operation (save/add/center)
	kXrRoleDanger=12   // destructive-flavored immediate action (STOPP)
};
enum XrPanelState {
	kXrStateHover=1, kXrStateSelected=2, kXrStateArmed=4, kXrStatePending=8,
	kXrStateDisabled=16, kXrStateOn=32, kXrStateActive=64
};
constexpr int kXrPanelWidth=768,kXrPanelHeight=1024,kXrPanelHeightTall=1280;
inline bool xrPanelRoleHittable(int role) {
	return role==kXrRoleButton||role==kXrRoleTargeted||role==kXrRoleImmediate||role==kXrRoleClose||
		role==kXrRoleGroupNum||role==kXrRoleTab||role==kXrRoleCard||role==kXrRoleOp||role==kXrRoleDanger;
}
struct XrPanelBuilder {
	XrPanelControl *out;int max,count=0;
	void add(int id,int x,int y,int w,int h,int role) {
		if(count>=max)return;
		out[count++]={id,x,y,w,h,role,0,-1};
	}
};
// GeneralsX @feature Ultron 15/09/2026 Shared help/guide geometry, used by
// the commands help and the workspace controller guide.
inline int xrHelpLayout(XrPanelControl *out,int max) {
	XrPanelBuilder b{out,max};
	b.add(33,664,24,72,52,kXrRoleClose);
	b.add(-2,32,100,704,800,kXrRoleBody);
	b.add(34,32,920,344,64,kXrRoleButton);
	b.add(36,392,920,344,64,kXrRoleButton);
	return b.count;
}
// GeneralsX @feature Ultron 15/09/2026 Commands console geometry. The
// compact region is pixel-stable; tactics only appends below (kind 5).
inline int xrCommandLayout(bool help,bool tactics,XrPanelControl *out,int max) {
	if(help)return xrHelpLayout(out,max);
	XrPanelBuilder b{out,max};
	b.add(33,664,24,72,52,kXrRoleClose);
	b.add(-1,32,92,704,84,kXrRoleContext);
	b.add(-10,32,192,704,20,kXrRoleSection); // orders
	{const int ids[]={1,2,3,4,8,0};
	 for(int i=0;i<6;++i)b.add(ids[i],32+(i%2)*360,220+(i/2)*76,344,68,i<5?kXrRoleTargeted:kXrRoleButton);}
	b.add(-11,32,456,704,20,kXrRoleSection); // instant & selection
	{const int ids[]={5,6,15,7,13,14,11,12,9};
	 const int roles[]={kXrRoleDanger,kXrRoleImmediate,kXrRoleImmediate,kXrRoleButton,kXrRoleButton,kXrRoleButton,kXrRoleButton,kXrRoleButton,kXrRoleButton};
	 for(int i=0;i<9;++i)b.add(ids[i],32+(i%3)*240,484+(i/3)*68,224,60,roles[i]);}
	b.add(10,32,688,464,60,kXrRoleButton);
	b.add(35,512,688,224,60,kXrRoleButton);
	b.add(-12,32,764,704,20,kXrRoleSection); // groups
	for(int i=0;i<10;++i)b.add(20+i,32+i*71,790,64,52,kXrRoleGroupNum);
	b.add(30,32,850,224,52,kXrRoleOp);
	b.add(31,272,850,224,52,kXrRoleOp);
	b.add(32,512,850,224,52,kXrRoleOp);
	b.add(37,32,922,344,56,kXrRoleButton);
	b.add(34,392,922,344,56,kXrRoleButton);
	if(tactics) {
		b.add(-13,32,1016,704,20,kXrRoleSection); // tactics
		b.add(40,32,1044,344,60,kXrRoleTargeted);
		b.add(41,392,1044,344,60,kXrRoleTargeted);
		b.add(42,32,1112,344,60,kXrRoleTargeted);
		b.add(43,392,1112,344,60,kXrRoleButton);
		b.add(-14,32,1188,704,20,kXrRoleSection); // bookmarks
		for(int i=0;i<4;++i)b.add(44+i,32+i*179,1214,168,56,kXrRoleGroupNum);
	}
	return b.count;
}
// GeneralsX @feature Ultron 15/09/2026 Workspace menu geometry per page.
// Tabs and the guide entry keep stable anchors across all pages.
inline int xrMenuLayout(int page,XrPanelControl *out,int max) {
	if(page==4)return xrHelpLayout(out,max);
	XrPanelBuilder b{out,max};
	// GeneralsX @feature Codex 15/09/2026 Explicit surface-detachment prompt.
	if(page==6) {
		b.add(-1,32,130,704,84,kXrRoleContext);
		b.add(18,32,320,704,76,kXrRoleImmediate);
		b.add(19,32,416,704,76,kXrRoleButton);
		return b.count;
	}
	b.add(24,664,20,72,44,kXrRoleButton); // controller guide
	for(int i=0;i<4;++i)b.add(20+i,32+i*179,78,168,40,kXrRoleTab);
	b.add(-1,32,130,704,62,kXrRoleContext);
	if(page==0) {
		b.add(-10,32,208,704,20,kXrRoleSection); // target
		b.add(0,32,234,344,78,kXrRoleCard);
		b.add(1,392,234,344,78,kXrRoleCard);
		b.add(-11,32,330,704,20,kXrRoleSection); // size & distance
		for(int i=0;i<4;++i)b.add(2+i,32+i*179,356,168,62,kXrRoleButton);
		b.add(-12,32,436,704,20,kXrRoleSection); // position
		for(int i=0;i<4;++i)b.add(6+i,32+i*179,462,168,62,kXrRoleButton);
		b.add(10,32,540,344,62,kXrRoleButton);
		b.add(11,392,540,344,62,kXrRoleButton);
		b.add(-13,32,620,704,20,kXrRoleSection); // map
		b.add(12,32,646,344,62,kXrRoleButton);
		b.add(13,392,646,344,62,kXrRoleButton);
		b.add(-14,32,726,704,20,kXrRoleSection); // actions
		b.add(14,32,752,464,66,kXrRoleImmediate);
		b.add(15,512,752,224,66,kXrRoleButton);
		b.add(16,32,834,344,66,kXrRoleButton);
		b.add(17,392,834,344,66,kXrRoleButton);
		b.add(18,32,916,704,66,kXrRoleImmediate); // whole workspace, not edit target
	} else if(page==1) {
		b.add(-10,32,208,704,20,kXrRoleSection); // orders
		{const int ids[]={5,6,7,8};
		 for(int i=0;i<4;++i)b.add(ids[i],32+(i%2)*360,234+(i/2)*74,344,66,kXrRoleTargeted);}
		b.add(9,32,382,344,66,kXrRoleTargeted);
		b.add(0,392,382,344,66,kXrRoleButton);
		b.add(-11,32,472,704,20,kXrRoleSection); // selection
		{const int ids[]={1,2,3,4,16,12};
		 for(int i=0;i<6;++i)b.add(ids[i],32+(i%3)*240,498+(i/3)*68,224,62,kXrRoleButton);}
		b.add(-12,32,652,704,20,kXrRoleSection); // instant
		b.add(10,32,678,224,62,kXrRoleDanger);
		b.add(11,272,678,224,62,kXrRoleImmediate);
		b.add(13,512,678,224,62,kXrRoleImmediate);
		b.add(-20,32,762,704,24,kXrRoleInfo);
		b.add(-13,32,868,704,20,kXrRoleSection); // navigation
		b.add(14,32,894,224,64,kXrRoleButton);
		b.add(15,272,894,224,64,kXrRoleButton);
		b.add(17,512,894,224,64,kXrRoleButton);
	} else if(page==2) {
		b.add(-10,32,208,704,20,kXrRoleSection); // group
		{const int ids[]={0,1,2,3,4,5};
		 for(int i=0;i<6;++i)b.add(ids[i],32+(i%2)*360,234+(i/2)*74,344,66,kXrRoleButton);}
		b.add(-11,32,472,704,20,kXrRoleSection); // selection
		{const int ids[]={6,7,8,9,10,11};
		 for(int i=0;i<6;++i)b.add(ids[i],32+(i%3)*240,498+(i/3)*68,224,62,kXrRoleButton);}
		b.add(12,32,634,224,62,kXrRoleButton);
		b.add(13,272,634,224,62,kXrRoleDanger);
		b.add(16,512,634,224,62,kXrRoleImmediate);
		b.add(-13,32,868,704,20,kXrRoleSection); // navigation
		b.add(14,32,894,224,64,kXrRoleButton);
		b.add(15,272,894,224,64,kXrRoleButton);
		b.add(17,512,894,224,64,kXrRoleButton);
	} else {
		b.add(-10,32,208,704,20,kXrRoleSection); // presentation
		b.add(-20,32,234,704,26,kXrRoleInfo);   // game: always tabletop
		b.add(-21,32,262,704,26,kXrRoleInfo);   // videos: upright screen
		b.add(4,32,300,224,64,kXrRoleButton);
		b.add(5,272,300,224,64,kXrRoleButton);
		b.add(6,512,300,224,64,kXrRoleButton);
		b.add(-11,32,392,704,20,kXrRoleSection); // graphics
		b.add(10,32,418,344,64,kXrRoleButton);
		b.add(12,392,418,344,64,kXrRoleButton);
		b.add(14,32,490,344,64,kXrRoleButton);
		b.add(15,392,490,344,64,kXrRoleButton);
		b.add(13,32,562,344,64,kXrRoleButton);
		b.add(-12,32,654,704,20,kXrRoleSection); // controls & language
		b.add(8,32,680,344,64,kXrRoleButton);
		b.add(11,392,680,344,64,kXrRoleButton);
		b.add(9,32,752,344,64,kXrRoleButton);
		b.add(7,392,752,344,64,kXrRoleButton);
	}
	return b.count;
}
// GeneralsX @feature Ultron 15/09/2026 Single hit test over the shared
// table: every visible hittable control has exactly one region, gaps dead.
inline int xrPanelHit(const XrPanelControl *table,int count,float u,float v,int height) {
	if(!std::isfinite(u)||!std::isfinite(v))return -1;
	const float x=u*kXrPanelWidth,y=(1-v)*height;
	for(int i=0;i<count;++i) {
		const auto &c=table[i];
		if(!xrPanelRoleHittable(c.role))continue;
		if(x>=c.x && x<c.x+c.w && y>=c.y && y<c.y+c.h)return c.id;
	}
	return -1;
}
inline XrPanelControl *xrFindControl(XrPanelControl *table,int count,int id) {
	for(int i=0;i<count;++i)if(table[i].id==id)return table+i;
	return nullptr;
}
// GeneralsX @feature Ultron 15/09/2026 JNI paint2 packing: 8 ints per
// control; the state key repaints only when visible content/state changes.
inline void xrPackControls(std::vector<int> &packed,const XrPanelControl *table,int count) {
	for(int i=0;i<count;++i) {
		const auto &c=table[i];
		const int entry[]={c.id,c.x,c.y,c.w,c.h,c.role,c.state,c.label};
		packed.insert(packed.end(),entry,entry+8);
	}
}
inline std::string xrControlsKey(const XrPanelControl *table,int count) {
	std::string key;
	for(int i=0;i<count;++i) {
		key+=std::to_string(table[i].id)+":"+std::to_string(table[i].state)+":"+std::to_string(table[i].label)+";";
	}
	return key;
}
