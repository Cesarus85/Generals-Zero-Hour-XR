// GeneralsX @bugfix Codex 14/09/2026 Preserve world poses across LOCAL rebases.
// OpenXR supplies the NEW origin in the OLD coordinates. Never use head height.
#pragma once
#include "XrPlacement.h"
#include <vector>
struct XrReferenceChanges {
 std::vector<XrEventDataReferenceSpaceChangePending> pending;
 // Initial placement already uses current-frame poses; it has no OLD world
 // to preserve. Keep only future changes, avoiding a false startup recovery.
 void adoptCurrentOrigin(XrTime time) {
  pending.erase(std::remove_if(pending.begin(),pending.end(),
   [time](const auto &event){return event.changeTime<=time;}),pending.end());
 }
 void enqueue(const XrEventDataReferenceSpaceChangePending &event) {
  pending.push_back(event);
  std::stable_sort(pending.begin(),pending.end(),[](const auto &a,const auto &b){return a.changeTime<b.changeTime;});
 }
 // 0 unchanged, 1 compensated, 2 unknown origin: require explicit replacement.
 int apply(XrTime time,XrSurface surfaces[3],XrPosef &anchor,XrSurface &menu) {
  int result=0;
  while(!pending.empty() && pending.front().changeTime<=time) {
   const auto event=pending.front();pending.erase(pending.begin());
   if(!event.poseValid){result=2;continue;}
   const auto delta=xrPoseInverse(event.poseInPreviousSpace);
   for(int i=0;i<3;++i)surfaces[i].pose=xrPoseMul(delta,surfaces[i].pose);
   anchor=xrPoseMul(delta,anchor);menu.pose=xrPoseMul(delta,menu.pose);
   if(result!=2)result=1;
  }
  return result;
 }
};
