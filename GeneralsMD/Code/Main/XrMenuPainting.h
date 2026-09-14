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
	// GeneralsX @bugfix Codex 14/09/2026 The entry button becomes a direct exit.
	static std::string uiButtonTitle;
	const std::string uiTitle=x.arranging ? xrTr("Fertig"):"UI";
	if(!x.uiButtonTexture || uiButtonTitle!=uiTitle)
		if(paintPanel(x,x.uiButtonTexture,uiTitle.c_str(),"","",-1,0))uiButtonTitle=uiTitle;
	static XrLanguage buttonLanguage=XrLanguage::German;
	if(!x.commandButtonTexture || buttonLanguage!=g_xrLanguage)
		if(paintPanel(x,x.commandButtonTexture,xrTr("BEFEHLE"),"","",-1,0))buttonLanguage=g_xrLanguage;
	if(commandsAvailable(x) && x.layout.commandsVisible) {
		const char *operations[]={"Gruppe auswählen; zum Anlegen: Speichern → Zahl","Speichern: Zahl wählen (ersetzt die Gruppe)","Zur Auswahl: Zahl wählen (Gruppe bleibt gleich)","Zentrieren: Zahl wählen"};
		const auto hint=x.commands.bookmarkSave ? std::string(xrTr("Ansicht merken: jetzt A–D wählen")):XrGameBoot_TacticalHint();
		const std::string status=XrGameBoot_TacticalStatus()+"\n"+
			(x.commands.groupOperation || hint.empty() ? xrTr(operations[x.commands.groupOperation]):hint);
		std::string labels=std::string("Auswahl / Befehle\nBewegen\nAngriffsmarsch\nZwangsangriff\nPosition bewachen\nSTOPP\nAuseinanderlaufen\nFreier Bauarbeiter\nWegpunkte an / aus\nGleicher Typ: Karte\nAlle Einheiten\nHeld auswählen\nAlle Flugzeuge\nNächste Einheit\nNächster Bauarbeiter\nAbbrechen / Abwahl\n")+
			(x.layout.leftHanded ? "Links: Trigger / Rahmen · Rechts: Stick = Karte\nRechter Grip: Auswahl +/- · X: Befehle ein/aus":"Rechts: Trigger / Rahmen · Links: Stick = Karte\nLinker Grip: Auswahl +/- · A: Befehle ein/aus");
		labels=xrLines(labels)+"\n"+xrTr("GRUPPEN · Zahl = auswählen");
		const char *ops[]={"Speichern","Zur Auswahl","Zentrieren"};
		for(int i=0;i<3;++i)labels+="\n"+std::string(x.commands.groupOperation==i+1 ? "> ":"")+xrTr(ops[i]);
		for(int i=0;i<10;++i)labels+="\n"+std::to_string(i+1)+" · "+std::to_string(XrGameBoot_GroupSize(i));
		labels+="\n"+std::string(xrTr("Communicator"))+"\n"+xrTr("Hilfe");
		labels+="\n"+std::string(xrTr(x.commands.tactics ? "Taktik −":"Taktik +"));
		const char *tactics[]={XrGameBoot_FormationActive() ? "Formation lösen":"Formation bilden","Zwangsbewegung","Ohne Verfolgung"};
		for(int i=0;i<3;++i)labels+="\n"+std::string(XrGameBoot_TacticalReason(40+i).empty() ? "":"! ")+xrTr(tactics[i]);
		labels+="\n"+std::string(x.commands.bookmarkSave ? "> ":"")+xrTr("Ansicht merken");
		for(int i=0;i<4;++i)labels+="\n"+std::string(1,char('A'+i))+(XrGameBoot_BookmarkKnown(i) ? " ●":" ○");
		labels+="\n"+std::string(xrTr("KARTENPLÄTZE · merken → A–D"));
		std::string detail=status;int kind=3;
		if(x.commands.tactics)kind=5;
		if(x.commands.help){kind=4;detail=xrCommandHelp(x.commands.helpPage);labels=xrLines("Zurück zu Befehlen\nWeiter");}
		const auto title=std::string(xrTr("Befehle · P11.1"))+(x.commands.help ? " · "+std::to_string(x.commands.helpPage+1)+"/4":"");
		const auto key=title+detail+labels+std::to_string(x.commands.input.hover);
		if(key!=x.commandsKey && paintPanel(x,x.commandsTexture,title,detail,labels,x.commands.input.hover,kind))x.commandsKey=key;
	}
	if(x.menu.open) {
		char state[512];const int slot=x.menu.target;
		snprintf(state,sizeof(state),xrTr("Bearbeitung: %s · %.2f m · Karte %.1fx · P19.1"),xrTr(slot==1 ? "Tisch":slot==2 ? "Baufenster":"Bildschirm"),x.surfaces[slot].width,x.worldZoom);
		std::string labels=std::string(x.splitVisible ? "Tisch wählen":"Bildschirm wählen")+"\nBaufenster wählen\nKleiner\nGrößer\nNäher\nWeiter weg\nHöher\nTiefer\nFlacher\nSteiler\nLinks drehen\nRechts drehen\nMehr Karte\nWeniger Karte\nGreifen / Anordnen\nPosition zurücksetzen\nSpielplatz einrichten\nSchließen";
		if(x.menu.page==1) labels="Kontextbefehl\nEinheit wählen\nAuswahl +/-\nBereich: zwei Ecken\nBereich hinzufügen\nBewegen\nAngriffsmarsch\nZwangsangriff\nPosition bewachen\nWegpunkte an / aus\nSTOPP\nAuseinanderlaufen\nFreier Bauarbeiter\nAbbrechen / Abwahl\nGruppen verwalten\nZurück zum Spiel\nAlle Einheiten\nSchließen";
		if(x.menu.page==2) labels="Gruppe vorher\nGruppe weiter\nAuswahl speichern\nGruppe auswählen\nGruppe zur Auswahl\nZur Gruppe schauen\nNächste Einheit\nNächster Bauarbeiter\nHeld auswählen\nAlle Flugzeuge\nGleicher Typ: Karte\nAlle Einheiten\nFreier Bauarbeiter\nSTOPP\nEinheitenbefehle\nFenster einstellen\nAbbrechen / Abwahl\nSchließen";
		if(x.menu.page==1 || x.menu.page==2) snprintf(state,sizeof(state),"%s",XrGameBoot_TacticalStatus().c_str());
		if(x.menu.page==3) {
			labels=std::string("Spiel: immer Tabletop\nVideos: Bildschirm\n\n\nLebenspunkte an/aus\nEinheitenringe an/aus\nBrettkörper an/aus\nSchließen\n")+
				(x.layout.leftHanded ? "Linkshändig: AN":"Linkshändig: AUS")+"\nFoto-Anordnung\n"+
				(x.layout.highQuality ? "Auflösung: Hoch":"Auflösung: Ausgewogen")+std::string("\n")+
				(x.layout.language==XrLanguage::German ? "Sprache: Deutsch":"Sprache: English")+std::string("\n")+
				(x.performance.volumeShadows ? "Schatten A: Original":"Schatten B: Leicht")+"\n"+
				(x.performance.enabled ? "Messung: AN":"Messung: AUS")+std::string("\n")+
				(x.performance.multiviewStereo ? "Stereo: Multiview":x.performance.atlasStereo ? "Stereo: Kompakt":"Stereo: Referenz")+std::string("\n")+
				(x.performance.elideWorldCopy ? "Zusatzwelt: Auto":"Zusatzwelt: Immer");
			snprintf(state,sizeof(state),"%s\n%s",XrGameBoot_PresentationStatus(x.stereoVisible,x.stereoWorld).c_str(),
				x.menu.hover==11 ? XrGameBoot_LanguageStatus().c_str():x.performance.status().c_str());
		}
		if(x.menu.page==5)xrSceneMenuText(x,labels,state,sizeof(state));
		// Pad the fixed 18 action slots before the four localized tab labels.
		while(std::count(labels.begin(),labels.end(),'\n')<17)labels+='\n';
		labels=xrLines(labels+"\nFenster\nEinheiten\nGruppen\nAnsicht");
		// GeneralsX @feature Codex 14/09/2026 Persistent target check mark,
		// independent of ray hover. Canvas recognizes it for an orange outline.
		if(x.menu.page==0) {
			const auto selected=xrEditTarget(x)==2 ? labels.find('\n')+1:0;
			labels.insert(selected,"✓ ");
		}
		if(x.menu.page==4) {
			const auto title=std::string(xrTr("Controller-Anleitung"))+" · "+std::to_string(x.menu.helpPage+1)+"/4";
			const auto detail=xrControllerHelp(x.menu.helpPage,x.layout.leftHanded);
			labels=xrLines("Zurück zu Fenstern\nWeiter");
			const auto key=title+detail+labels+std::to_string(x.menu.hover);
			if(key!=x.settingsKey && paintPanel(x,x.settingsTexture,title,detail,labels,x.menu.hover,4))x.settingsKey=key;
		} else {
		const std::string key=std::string(state)+std::to_string(x.menu.hover)+labels;
		if(key!=x.settingsKey && paintPanel(x,x.settingsTexture,x.menu.page==5 ? xrTr("Spielplatz einrichten · P19.1"):"Generals: Zero Hour XR",state,labels,x.menu.hover,x.menu.page==5 ? 7:1)) x.settingsKey=key;
		}
	}
	std::string info;
	if(commandsAvailable(x) && x.layout.commandsVisible) {
		const auto explanation=commandExplanation(x);
		if(!explanation.empty())info=std::string(xrTr("Befehle · P11.1"))+"\n"+explanation;
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
