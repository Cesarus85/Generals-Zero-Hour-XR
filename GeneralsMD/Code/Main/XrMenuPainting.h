// GeneralsX @feature Codex 13/09/2026 Android Canvas to native panel textures.
#pragma once
static bool paintPanel(XrHello &x,GLuint &texture,const std::string &title,const std::string &detail,
	const std::string &labels,int hover,int kind) {
	if(!x.panelEnv || !x.panelPainter) return false;
	auto *env=x.panelEnv;
	const auto method=env->GetStaticMethodID(x.panelPainter,"paint","(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;II)[I");
	if(!method || env->ExceptionCheck()) {env->ExceptionClear();return false;}
	jstring a=env->NewStringUTF(title.c_str()),b=env->NewStringUTF(detail.c_str()),c=env->NewStringUTF(labels.c_str());
	auto pixels=(jintArray)env->CallStaticObjectMethod(x.panelPainter,method,a,b,c,hover,kind);
	env->DeleteLocalRef(a);env->DeleteLocalRef(b);env->DeleteLocalRef(c);
	if(env->ExceptionCheck()) {env->ExceptionDescribe();env->ExceptionClear();return false;}
	if(!pixels) return false;
	const int w=kind==0 ? 192:768,h=kind==0 ? 128:kind==6 ? 768:kind==5 ? 1280:(kind==1 || kind==3 || kind==4 || kind==7) ? 1024:384;
	if(env->GetArrayLength(pixels)!=w*h) {env->DeleteLocalRef(pixels);return false;}
	std::vector<jint> argb(w*h);env->GetIntArrayRegion(pixels,0,w*h,argb.data());env->DeleteLocalRef(pixels);
	std::vector<unsigned char> rgba(w*h*4);
	for(int y=0;y<h;++y) for(int px=0;px<w;++px) {
		const unsigned color=argb[y*w+px];const int i=((h-y-1)*w+px)*4;
		rgba[i]=color>>16;rgba[i+1]=color>>8;rgba[i+2]=color;rgba[i+3]=color>>24;
	}
	if(!texture) xr_glGenTextures(1,&texture);
	xr_glActiveTexture(GL_TEXTURE2);xr_glBindTexture(GL_TEXTURE_2D,texture);
	xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	xr_glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);xr_glPixelStorei(GL_UNPACK_ALIGNMENT,4);
	xr_glPixelStorei(GL_UNPACK_ROW_LENGTH,0);xr_glPixelStorei(GL_UNPACK_SKIP_ROWS,0);xr_glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
	xr_glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());return true;
}
// GeneralsX @feature Ultron 15/09/2026 P21 structured panel painting: the
// native control table (XrPanelLayout.h) is packed 8 ints per control and
// rendered as primitives by Java. One layout source for pixels and rays.
static bool paintPanel2(XrHello &x,GLuint &texture,const std::string &title,const std::string &detail,
	const std::string &labels,const std::vector<int> &packed,int kind) {
	if(!x.panelEnv || !x.panelPainter) return false;
	auto *env=x.panelEnv;
	const auto method=env->GetStaticMethodID(x.panelPainter,"paint2","(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[II)[I");
	if(!method || env->ExceptionCheck()) {env->ExceptionClear();return false;}
	jstring a=env->NewStringUTF(title.c_str()),b=env->NewStringUTF(detail.c_str()),c=env->NewStringUTF(labels.c_str());
	jintArray ctrl=env->NewIntArray(jsize(packed.size()));
	if(ctrl) env->SetIntArrayRegion(ctrl,0,jsize(packed.size()),packed.data());
	auto pixels=(jintArray)env->CallStaticObjectMethod(x.panelPainter,method,a,b,c,ctrl,kind);
	env->DeleteLocalRef(a);env->DeleteLocalRef(b);env->DeleteLocalRef(c);
	if(ctrl) env->DeleteLocalRef(ctrl);
	if(env->ExceptionCheck()) {env->ExceptionDescribe();env->ExceptionClear();return false;}
	if(!pixels) return false;
	const int w=kXrPanelWidth,h=kind==5 ? kXrPanelHeightTall:kXrPanelHeight;
	if(env->GetArrayLength(pixels)!=w*h) {env->DeleteLocalRef(pixels);return false;}
	std::vector<jint> argb(w*h);env->GetIntArrayRegion(pixels,0,w*h,argb.data());env->DeleteLocalRef(pixels);
	std::vector<unsigned char> rgba(w*h*4);
	for(int y=0;y<h;++y) for(int px=0;px<w;++px) {
		const unsigned color=argb[y*w+px];const int i=((h-y-1)*w+px)*4;
		rgba[i]=color>>16;rgba[i+1]=color>>8;rgba[i+2]=color;rgba[i+3]=color>>24;
	}
	if(!texture) xr_glGenTextures(1,&texture);
	xr_glActiveTexture(GL_TEXTURE2);xr_glBindTexture(GL_TEXTURE_2D,texture);
	xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);xr_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	xr_glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);xr_glPixelStorei(GL_UNPACK_ALIGNMENT,4);
	xr_glPixelStorei(GL_UNPACK_ROW_LENGTH,0);xr_glPixelStorei(GL_UNPACK_SKIP_ROWS,0);xr_glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
	xr_glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());return true;
}
static std::string commandExplanation(const XrHello &x) {
	const int hit=x.commands.input.hover;
	if(x.commands.help)return {};
	if(hit>=40 && hit<=42) {
		const auto reason=XrGameBoot_TacticalReason(hit);if(!reason.empty())return reason;
		const char *hints[]={"Formation: aktuelle Anordnung zusammenhalten. Erneut klicken löst sie auf.",
			"Zwangsbewegung: danach Boden anklicken. Ersetzt den Auftrag; keine Wegpunktfolge.",
			"Ohne Verfolgung: danach Boden anklicken. Bewacht dort ohne Gegner zu verfolgen."};
		return xrTr(hints[hit-40]);
	}
	if(hit==43 || (hit>=44 && hit<=47))return xrTr(x.commands.bookmarkSave ?
		"Ansicht merken: A–D wählen. Überschreibt diesen Kartenplatz, nur für diese Partie.":
		"A–D ruft die Kamera auf. Erst Ansicht merken → A–D zum Speichern. Tisch bleibt stehen.");
	if(hit==4)return xrTr("Bewachen: danach Boden oder anderes verbündetes Objekt anklicken. Bewegliche Ziele werden begleitet.");
	if(hit>=20 && hit<30)return xrTr("Zahl ruft die Gruppe auf. Zum Anlegen: Einheiten markieren → Speichern → Zahl.");
	if(hit==37)return xrTr("Taktik aufklappen: Formation, Zwangsbewegung, Bewachen ohne Verfolgung und Kartenplätze.");
	return {};
}
static void updateMenuTextures(XrHello &x,XrTime time) {
	static XrLanguage recoveryLanguage=XrLanguage::German;
	if(x.recoveryVisible && (!x.recoveryTexture || recoveryLanguage!=g_xrLanguage))
		if(paintPanel(x,x.recoveryTexture,xrTr("Darstellung wird wiederhergestellt"),
			xrTr("Die Spielwelt wird neu gezeichnet. Bitte die Trigger loslassen."),"",-1,2))recoveryLanguage=g_xrLanguage;
	// GeneralsX @feature Codex 17/09/2026 Reuse the existing readable XR card.
	if(x.observer.mode!=XrObserverMode::Off) {
		const std::string key=std::to_string(static_cast<int>(g_xrLanguage))+
			(x.layout.leftHanded ? "Y":"B")+std::to_string(int(x.observer.mode))+
			(x.observer.mode==XrObserverMode::Armed && x.rayVisible && !x.rayHit ? "invalid":"valid");
		if(key!=x.observerHintKey && paintPanel(x,x.observerHintTexture,xrTr("Bodenansicht"),
			x.observer.mode==XrObserverMode::Active ?
				xrTr(x.layout.leftHanded ? "Links: gehen · Rechts: drehen · Y: Tisch":
					"Links: gehen · Rechts: drehen · B: Tisch"):
				xrTr(x.rayVisible && !x.rayHit ? "Hier kein sicherer, sichtbarer Boden. Anderen Ort wählen; B/Y bricht ab.":
					"Sichtbaren freien Boden mit Trigger wählen. B/Y bricht ab."),"",-1,2))x.observerHintKey=key;
	}
	// GeneralsX @feature Muse 16/09/2026 Match-result card: accent-colored
	// kind-2 card from the read-only latch; repainted on result/language
	// change, kept across the transition to statistics.
	if(x.resultVisible) {
		const auto endResult=XrGameBoot_MatchResult();
		const char *endTitle=endResult==XrEndgameResult::Victory ? "Sieg!" :
			endResult==XrEndgameResult::Defeat ? "Niederlage" : "Partie beendet";
		const int accent=endResult==XrEndgameResult::Victory ? 1 :
			endResult==XrEndgameResult::Defeat ? 2 : 3;
		const std::string key=std::to_string(static_cast<int>(g_xrLanguage))+endTitle;
		if(key!=x.resultKey && paintPanel(x,x.resultTexture,xrTr(endTitle),
			xrTr("Die Partie ist entschieden.\nBeliebige Taste zum Schließen."),"",accent,2))x.resultKey=key;
	}
	// GeneralsX @bugfix Codex 14/09/2026 The entry button becomes a direct exit.
	static std::string uiButtonTitle;
	const std::string uiTitle=x.arranging ? xrTr("Fertig"):"UI";
	if(!x.uiButtonTexture || uiButtonTitle!=uiTitle)
		if(paintPanel(x,x.uiButtonTexture,uiTitle.c_str(),"","",-1,0))uiButtonTitle=uiTitle;
	static XrLanguage groundButtonLanguage=XrLanguage::German;
	if(!x.groundButtonTexture || groundButtonLanguage!=g_xrLanguage)
		if(paintPanel(x,x.groundButtonTexture,xrTr("BODENANSICHT"),"","",-1,0))
			groundButtonLanguage=g_xrLanguage;
	static XrLanguage buttonLanguage=XrLanguage::German;
	if(!x.commandButtonTexture || buttonLanguage!=g_xrLanguage)
		if(paintPanel(x,x.commandButtonTexture,xrTr("BEFEHLE"),"","",-1,0))buttonLanguage=g_xrLanguage;
	if(commandsAvailable(x) && x.layout.commandsVisible) {
		// GeneralsX @refactor Ultron 15/09/2026 P21 content for the shared
		// control table: labels and states per control id; geometry lives in
		// XrPanelLayout.h as the single source shared with xrCommandHit.
		const bool help=x.commands.help,tactics=x.commands.tactics && !help;
		XrPanelControl table[64];
		const int count=xrCommandLayout(help,x.commands.tactics,table,64);
		std::vector<std::string> labels;
		auto label=[&](int id,const std::string &text) {
			if(auto *c=xrFindControl(table,count,id)) {c->label=int(labels.size());labels.push_back(text);}
		};
		auto mark=[&](int id,int bits) {
			if(auto *c=xrFindControl(table,count,id)) c->state|=bits;
		};
		int mode=0,group=0;bool queue=false;
		XrGameBoot_TacticalState(mode,group,queue);
		std::string title,detail;
		label(33,"✕");
		if(help) {
			title=std::string(xrTr("Befehle"))+" · "+std::to_string(x.commands.helpPage+1)+"/4";
			detail=xrCommandHelp(x.commands.helpPage);
			label(34,xrTr("Zurück zu Befehlen"));label(36,xrTr("Weiter"));
		} else {
			title=xrTr("Befehle");
				const char *operations[]={"Zahl wählt Gruppe; Aktion → Zahl führt sie aus","Gruppe ersetzen: jetzt Zahl wählen","Neu / Erweitern: jetzt Zahl wählen","Zentrieren: Zahl wählen"};
			const auto hint=x.commands.bookmarkSave ? std::string(xrTr("Ansicht merken: jetzt A–D wählen")):XrGameBoot_TacticalHint();
			detail=XrGameBoot_TacticalStatus()+"\n"+
				(x.commands.groupOperation || hint.empty() ? xrTr(operations[x.commands.groupOperation]):hint);
			label(-10,xrTr("Auftrag · danach Ziel wählen"));
			label(1,xrTr("Bewegen"));label(2,xrTr("Angriffsmarsch"));label(3,xrTr("Zwangsangriff"));label(4,xrTr("Position bewachen"));
			label(8,std::string(xrTr("Wegpunkte"))+"|"+xrTr(queue ? "AN":"AUS"));
			label(0,xrTr("Auswahl / Befehle"));
			label(-11,xrTr("Sofort & Auswahl"));
			label(5,xrTr("STOPP"));label(6,xrTr("Auseinanderlaufen"));label(15,xrTr("Abbrechen / Abwahl"));
			label(7,xrTr("Freier Bauarbeiter"));label(13,xrTr("Nächste Einheit"));label(14,xrTr("Nächster Bauarbeiter"));
			label(11,xrTr("Held auswählen"));label(12,xrTr("Alle Flugzeuge"));label(9,xrTr("Gleicher Typ: Karte"));
			label(10,xrTr("Alle Einheiten"));label(35,xrTr("Communicator"));
				label(-12,xrTr("Gruppen · Aktion wählen → Zahl"));
			for(int i=0;i<10;++i)label(20+i,std::to_string(i+1)+"|"+std::to_string(XrGameBoot_GroupSize(i)));
				label(30,xrTr("Gruppe ersetzen"));label(31,xrTr("Neu / Erweitern"));label(32,xrTr("Zentrieren"));
			label(37,xrTr(x.commands.tactics ? "Taktik −":"Taktik +"));label(34,xrTr("Hilfe"));
			if(x.commands.tactics) {
				label(-13,xrTr("Taktik · erweitert"));
				label(40,xrTr(XrGameBoot_FormationActive() ? "Formation lösen":"Formation bilden"));
				label(41,xrTr("Zwangsbewegung"));label(42,xrTr("Ohne Verfolgung"));
				label(43,std::string(xrTr("Ansicht merken"))+"|"+xrTr(x.commands.bookmarkSave ? "AN":"AUS"));
				label(-14,xrTr("KARTENPLÄTZE · merken → A–D"));
				for(int i=0;i<4;++i)label(44+i,std::string(1,char('A'+i))+"|"+(XrGameBoot_BookmarkKnown(i) ? "●":"○"));
			}
			// Persistent states: armed order, toggles, pending operations,
			// disabled tactics; the disabled reason stays in the hover card.
			for(int id=1;id<=4;++id)if(xrCommandAction(id)==mode)mark(id,kXrStateArmed);
			if(mode==9)mark(41,kXrStateArmed);
			if(mode==10)mark(42,kXrStateArmed);
			if(queue)mark(8,kXrStateOn);
			for(int id=40;id<=42;++id)if(!XrGameBoot_TacticalReason(id).empty())mark(id,kXrStateDisabled);
			if(XrGameBoot_FormationActive())mark(40,kXrStateOn);
			if(x.commands.bookmarkSave)mark(43,kXrStateOn);
			for(int i=0;i<10;++i)if(group==i)mark(20+i,kXrStateOn);
			if(x.commands.groupOperation)mark(29+x.commands.groupOperation,kXrStatePending);
		}
		if(x.commands.input.hover>=0)mark(x.commands.input.hover,kXrStateHover);
		std::vector<int> packed;xrPackControls(packed,table,count);
		std::string key=title+detail+xrControlsKey(table,count);
		for(const auto &s:labels)key+="\x1f"+s;
		std::string joined;
		for(size_t i=0;i<labels.size();++i){if(i)joined+='\n';joined+=labels[i];}
		if(key!=x.commandsKey && paintPanel2(x,x.commandsTexture,title,detail,joined,packed,help ? 4:(tactics ? 5:3)))x.commandsKey=key;
	}
	if(x.menu.open) {
		// GeneralsX @refactor Ultron 15/09/2026 P21 the workspace window uses
		// the same shared-table contract as the commands console; the
		// play-space wizard (page 5) keeps its sequential kind-7 layout.
		if(x.menu.page==5) {
			std::string labels;char state[512];
			xrSceneMenuText(x,labels,state,sizeof(state));
			while(std::count(labels.begin(),labels.end(),'\n')<17)labels+='\n';
			labels=xrLines(labels);
			const std::string key=std::string(state)+std::to_string(x.menu.hover)+labels;
			if(key!=x.settingsKey && paintPanel(x,x.settingsTexture,xrTr("Spielplatz einrichten"),state,labels,x.menu.hover,7)) x.settingsKey=key;
		} else {
		XrPanelControl table[80];
		const int count=xrMenuLayout(x.menu.page,table,80);
		std::vector<std::string> labels;
		auto label=[&](int id,const std::string &text) {
			if(auto *c=xrFindControl(table,count,id)) {c->label=int(labels.size());labels.push_back(text);}
		};
		auto mark=[&](int id,int bits) {
			if(auto *c=xrFindControl(table,count,id)) c->state|=bits;
		};
		const char *tabs[]={"Fenster","Einheiten","Gruppen","Ansicht"};
		for(int i=0;i<4;++i)label(20+i,xrTr(tabs[i]));
		mark(20+x.menu.page,kXrStateActive);
		label(24,"?");
		std::string title,detail;
		if(x.menu.page==6) {
			title=xrTr("Spielfläche verlassen?");
			detail=std::string(xrTr("Die Tisch-/Bodenplatzierung wird verlassen."))+"\n"+
				xrTr("Brett und Fenster kommen gemeinsam vor dich.");
			label(18,xrTr("Verlassen & vor mir ausrichten"));label(19,xrTr("Abbrechen"));
		} else if(x.menu.page==4) {
			title=std::string(xrTr("Controller-Anleitung"))+" · "+std::to_string(x.menu.helpPage+1)+"/"+
				std::to_string(kXrControllerHelpPages);
			detail=xrControllerHelp(x.menu.helpPage,x.layout.leftHanded);
			label(33,"✕");label(34,xrTr("Zurück zu Fenstern"));label(36,xrTr("Weiter"));
		} else {
			title=xrTr(tabs[x.menu.page<0 || x.menu.page>3 ? 0:x.menu.page]);
			if(x.menu.page==0) {
				char line[256];const int slot=x.menu.target;
				snprintf(line,sizeof(line),xrTr("Bearbeitung: %s · %.2f m · Karte %.1fx"),xrTr(slot==1 ? "Tisch":slot==2 ? "Baufenster":"Bildschirm"),x.surfaces[slot].width,x.worldZoom);
				detail=line;
				char chip[32];
				const int boardSlot=x.splitVisible ? 1:0;
				snprintf(chip,sizeof(chip),"%.2f m",x.surfaces[boardSlot].width);
				label(0,std::string(xrTr(x.splitVisible ? "Tisch":"Bildschirm"))+"|"+chip);
				snprintf(chip,sizeof(chip),"%.2f m",x.surfaces[2].width);
				label(1,std::string(xrTr("Baufenster"))+"|"+chip);
				mark(xrEditTarget(x)==2 ? 1:0,kXrStateSelected);
				label(-10,xrTr("Ziel"));
				label(-11,xrTr("Größe & Abstand"));
				label(2,xrTr("Kleiner"));label(3,xrTr("Größer"));label(4,xrTr("Näher"));label(5,xrTr("Weiter weg"));
				label(-12,xrTr("Lage"));
				label(6,xrTr("Höher"));label(7,xrTr("Tiefer"));label(8,xrTr("Flacher"));label(9,xrTr("Steiler"));
				label(10,xrTr("Links drehen"));label(11,xrTr("Rechts drehen"));
				label(-13,xrTr("Karte"));
				snprintf(chip,sizeof(chip),"%.1f×",x.worldZoom);
				label(12,std::string(xrTr("Mehr Karte"))+"|"+chip);label(13,xrTr("Weniger Karte"));
				label(-14,xrTr("Aktionen"));
				label(14,xrTr("Greifen / Anordnen"));label(15,xrTr("Position zurücksetzen"));
				label(16,xrTr("Spielplatz einrichten"));label(17,xrTr("Schließen"));
				label(18,xrTr("Alles vor mir ausrichten"));
			} else if(x.menu.page==1) {
				detail=XrGameBoot_TacticalStatus();
				label(-10,xrTr("Auftrag · danach Ziel wählen"));
				label(5,xrTr("Bewegen"));label(6,xrTr("Angriffsmarsch"));label(7,xrTr("Zwangsangriff"));label(8,xrTr("Position bewachen"));
				label(9,xrTr("Wegpunkte an / aus"));label(0,xrTr("Kontextbefehl"));
				label(-11,xrTr("Auswahl"));
				label(1,xrTr("Einheit wählen"));label(2,xrTr("Auswahl +/-"));label(3,xrTr("Bereich: zwei Ecken"));
				label(4,xrTr("Bereich hinzufügen"));label(16,xrTr("Alle Einheiten"));label(12,xrTr("Freier Bauarbeiter"));
				label(-12,xrTr("Sofort"));
				label(10,xrTr("STOPP"));label(11,xrTr("Auseinanderlaufen"));label(13,xrTr("Abbrechen / Abwahl"));
				char hint[128];snprintf(hint,sizeof(hint),xrTr("Direkter im Spiel: Befehle-Konsole (%s)"),x.layout.leftHanded ? "X":"A");
				label(-20,hint);
				label(-13,xrTr("Navigation"));
				label(14,xrTr("Gruppen verwalten"));label(15,xrTr("Zurück zum Spiel"));label(17,xrTr("Schließen"));
			} else if(x.menu.page==2) {
				detail=XrGameBoot_TacticalStatus();
				label(-10,xrTr("Gruppen"));
				label(0,xrTr("Gruppe vorher"));label(1,xrTr("Gruppe weiter"));label(2,xrTr("Auswahl speichern"));
				label(3,xrTr("Gruppe auswählen"));label(4,xrTr("Gruppe zur Auswahl"));label(5,xrTr("Zur Gruppe schauen"));
				label(-11,xrTr("Auswahl"));
				label(6,xrTr("Nächste Einheit"));label(7,xrTr("Nächster Bauarbeiter"));label(8,xrTr("Held auswählen"));
				label(9,xrTr("Alle Flugzeuge"));label(10,xrTr("Gleicher Typ: Karte"));label(11,xrTr("Alle Einheiten"));
				label(12,xrTr("Freier Bauarbeiter"));label(13,xrTr("STOPP"));label(16,xrTr("Abbrechen / Abwahl"));
				label(-13,xrTr("Navigation"));
				label(14,xrTr("Einheitenbefehle"));label(15,xrTr("Fenster einstellen"));label(17,xrTr("Schließen"));
			} else {
				detail=XrGameBoot_PresentationStatus(x.stereoVisible,x.stereoWorld)+"\n"+
					(x.menu.hover==11 ? XrGameBoot_LanguageStatus():x.menu.hover==16 ?
						xrTr("Nur Offline-Gefecht: Bodenansicht wählen, dann sichtbaren freien Boden anklicken. B/Y kehrt zurück."):x.performance.status());
				label(-10,xrTr("Darstellung"));
				label(-20,xrTr("Spiel: Tisch; Bodenansicht optional"));label(-21,xrTr("Videos: Bildschirm"));
				auto toggle=[&](int id,const char *name,bool on) {
					label(id,std::string(xrTr(name))+"|"+xrTr(on ? "AN":"AUS"));if(on)mark(id,kXrStateOn);};
				toggle(4,"Lebenspunkte",x.layout.healthBars);
				toggle(5,"Einheitenringe",x.layout.unitRings);
				toggle(6,"Brettkörper",x.layout.boardFrame);
				label(-11,xrTr("Grafik"));
				label(10,std::string(xrTr("Auflösung"))+"|"+xrTr(x.layout.resolutionTier==2 ? "Ultra+":x.layout.resolutionTier==1 ? "Hoch":"Ausgewogen"));
				if(x.layout.resolutionTier)mark(10,kXrStateOn);
				label(12,std::string(xrTr("Schatten"))+"|"+xrTr(x.performance.volumeShadows ? "Original":"Leicht"));
				if(x.performance.volumeShadows)mark(12,kXrStateOn);
				label(14,std::string(xrTr("Stereo"))+"|"+xrTr(x.performance.multiviewStereo ? "Multiview":x.performance.atlasStereo ? "Kompakt":"Referenz"));
				label(15,std::string(xrTr("Zusatzwelt"))+"|"+xrTr(x.performance.elideWorldCopy ? "Auto":"Immer"));
				toggle(13,"Messung",x.performance.enabled);
				label(-12,xrTr("Steuerung & Sprache"));
				toggle(8,"Linkshändig",x.layout.leftHanded);
				label(11,std::string(xrTr("Sprache"))+"|"+xrTr(x.layout.language==XrLanguage::German ? "Deutsch":"English"));
				label(9,xrTr("Foto-Anordnung"));label(7,xrTr("Schließen"));
				label(16,xrTr("Bodenansicht · Ort wählen"));
				if(!x.stereoVisible || !XrGameBoot_CanObserveGround())mark(16,kXrStateDisabled);
			}
		}
		if(x.menu.hover>=0)mark(x.menu.hover,kXrStateHover);
		std::vector<int> packed;xrPackControls(packed,table,count);
		std::string key=title+detail+xrControlsKey(table,count);
		for(const auto &s:labels)key+="\x1f"+s;
		std::string joined;
		for(size_t i=0;i<labels.size();++i){if(i)joined+='\n';joined+=labels[i];}
		if(key!=x.settingsKey && paintPanel2(x,x.settingsTexture,title,detail,joined,packed,x.menu.page==4 ? 4:1))x.settingsKey=key;
		}
	}
	std::string info;
	if(commandsAvailable(x) && x.layout.commandsVisible) {
		const auto explanation=commandExplanation(x);
		if(!explanation.empty())info=std::string(xrTr("Befehle"))+"\n"+explanation;
	}
	if(!x.menu.open && !x.arranging && x.pointerVisible && !x.pointerPressed && x.pointerPiece==1)
		info=XrGameBoot_WorldHoverInfo();
	if(!x.menu.open && !x.arranging && x.pointerVisible && !x.pointerPressed && x.pointerPiece>=2) {
		const auto r=surfaceRect(x.pointerPiece);
		// Read-only native evaluation is throttled, not run at eye-frame rate.
		static XrTime polled=0;static float lastX=-100,lastY=-100;static std::string cached;
		const float px=(r.x+x.pointerU*r.w)*(XrGameBoot_GameWidth()-1),py=(1-r.y-x.pointerV*r.h)*(XrGameBoot_GameHeight()-1);
		if(time<polled || time-polled>=200000000 || fabsf(px-lastX)>3 || fabsf(py-lastY)>3) {
			cached=XrGameBoot_HoverInfo(px,py);polled=time;lastX=px;lastY=py;
		}
		info=cached;
	}
	// Building instructions are immediate, not a delayed tooltip whose timer
	// restarts on every degree. Quantize the readout to avoid per-frame uploads.
	const bool buildHint=!x.menu.open && !x.arranging && x.pointerVisible &&
		x.pointerPiece==1 && XrGameBoot_CanRotatePlacement();
	if(buildHint) {
		char hint[512];const int angle=(int(std::lround(XrGameBoot_PlacementDegrees()/5))*5)%360;
		snprintf(hint,sizeof(hint),xrTr(x.layout.leftHanded ?
			"Winkel %d° · Rechter Grip + linker Stick ↔ drehen. Linker Trigger: bauen; linker Grip: abbrechen.":
			"Winkel %d° · Linker Grip + rechter Stick ↔ drehen. Rechter Trigger: bauen; rechter Grip: abbrechen."),angle);
		info=std::string(xrTr("Gebäude drehen"))+"\n"+hint;
	}
	// Dynamic money/queue state must not restart the initial hover delay.
	const auto identity=std::to_string(x.pointerPiece)+info.substr(0,info.find('\n'));
	if(identity!=x.hoverCandidate) {x.hoverCandidate=identity;x.hoverSince=time;}
	x.hoverVisible=!info.empty() && (buildHint || time-x.hoverSince>=300000000);
	const int page=int(std::max<XrTime>(0,time-x.hoverSince)/10000000000LL);
	const auto hoverKey=info+"|"+std::to_string(page);
	if(x.hoverVisible && x.hoverKey!=hoverKey) {
		const auto split=info.find('\n');
		if(paintPanel(x,x.hoverTexture,info.substr(0,split),split==std::string::npos ? "":info.substr(split+1),"",page,6)) x.hoverKey=hoverKey;
	}
}
